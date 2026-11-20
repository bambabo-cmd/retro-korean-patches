#!/usr/bin/env python3
"""Assemble pinned Gens assets and publish/download-verify an immutable release.

Only Python's standard library is used for assembly. The GitHub CLI performs
authorized release operations using the workflow's short-lived token. No EXE is
executed. Existing assets are never replaced or deleted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PAYLOAD = PurePosixPath('release-payloads/gens-ko/20261121_01')
TAG = 'gens-ko-2.14-r3-test-20261121-01'
WINDOWS_NAME = 'Gens_KO_2.14_r3_TEST_20261121_01_Windows.zip'
SOURCE_NAME = 'Gens_KO_2.14_r3_TEST_20261121_01_source.zip'
CHECKSUM_NAME = 'Gens_KO_2.14_r3_TEST_20261121_01_SHA256.txt'
BLOCK = 1024 * 1024


def fail(message: str) -> None:
    raise ValueError(message)


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(BLOCK), b''):
            h.update(data)
    return h.hexdigest()


def spec(value: object, label: str) -> dict:
    if not isinstance(value, dict):
        fail(label + ' must be an object')
    size, digest = value.get('size'), value.get('sha256')
    if type(size) is not int or not 0 < size <= 200 * 1024 * 1024:
        fail(label + ' has invalid size')
    if not isinstance(digest, str) or not re.fullmatch(r'[0-9a-f]{64}', digest):
        fail(label + ' has invalid SHA-256')
    return value


def payload_file(value: object) -> Path:
    if not isinstance(value, str) or '\\' in value:
        fail('Invalid payload path')
    relative = PurePosixPath(value)
    if relative.is_absolute() or '..' in relative.parts or not relative.is_relative_to(PAYLOAD):
        fail('Payload path must stay in ' + str(PAYLOAD))
    current = ROOT
    for part in relative.parts:
        current = current / part
        if current.is_symlink():
            fail('Payload symlink is not permitted: ' + value)
    if not current.is_file():
        fail('Payload file is missing: ' + value)
    return current


def checked(path: Path, expected: dict, label: str) -> None:
    if path.is_symlink() or not path.is_file():
        fail(label + ' is not a regular file')
    if path.stat().st_size != expected['size'] or sha(path) != expected['sha256']:
        fail(label + ' size or SHA-256 differs')


def publish_local(temp: Path, target: Path, expected: dict) -> None:
    checked(temp, expected, temp.name)
    if target.exists() or target.is_symlink():
        checked(target, expected, target.name)
        temp.unlink()
    else:
        temp.replace(target)


def assemble(manifest: Path, output: Path) -> tuple[dict, list[dict]]:
    data = json.loads(manifest.read_text(encoding='utf-8'))
    if data.get('tag') != TAG:
        fail('Unexpected release tag')
    title = data.get('title')
    if not isinstance(title, str) or not title.strip() or len(title) > 256:
        fail('Invalid release title')
    windows = spec(data.get('windows'), 'windows')
    source = spec(data.get('source'), 'source')
    exe = spec(data.get('exe'), 'exe')
    if windows.get('file_name') != WINDOWS_NAME or source.get('file_name') != SOURCE_NAME:
        fail('Unexpected release filenames')
    if exe.get('member') != 'Gens_KO/Gens_KO.exe':
        fail('Unexpected EXE member')
    if source.get('member') != 'Gens_KO/source/' + SOURCE_NAME:
        fail('Unexpected corresponding source member')
    payload_file(data.get('notes_path'))
    parts = windows.get('parts')
    if not isinstance(parts, list) or not 1 <= len(parts) <= 1024:
        fail('Expected a bounded, ordered chunk list')
    if sum(spec(p, 'part')['size'] for p in parts) != windows['size']:
        fail('Chunk sizes do not sum to Windows ZIP size')
    if output.is_symlink():
        fail('Output directory must not be a symlink')
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='assemble-', dir=output) as work:
        work = Path(work)
        joined = work / WINDOWS_NAME
        with joined.open('xb') as destination:
            for index, part in enumerate(parts):
                expected_path = str(PAYLOAD / f'windows.part{index:03d}')
                if part.get('path') != expected_path:
                    fail('Chunk order/path differs at index ' + str(index))
                path = payload_file(part['path'])
                checked(path, part, part['path'])
                with path.open('rb') as stream:
                    shutil.copyfileobj(stream, destination, BLOCK)
        checked(joined, windows, WINDOWS_NAME)
        with zipfile.ZipFile(joined) as archive:
            names = archive.namelist()
            if len(names) != len(set(names)):
                fail('Windows ZIP contains duplicate member names')
            exe_info = archive.getinfo(exe['member'])
            source_info = archive.getinfo(source['member'])
            if exe_info.file_size != exe['size'] or source_info.file_size != source['size']:
                fail('Embedded EXE or source size differs')
            with archive.open(exe['member']) as stream:
                h = hashlib.sha256()
                first = stream.read(BLOCK)
                if first[:2] != b'MZ':
                    fail('The release does not contain the expected Windows executable')
                h.update(first)
                for block in iter(lambda: stream.read(BLOCK), b''):
                    h.update(block)
                if h.hexdigest() != exe['sha256']:
                    fail('Embedded Windows EXE SHA-256 differs')
            extracted = work / SOURCE_NAME
            with archive.open(source['member']) as stream, extracted.open('xb') as destination:
                shutil.copyfileobj(stream, destination, BLOCK)
        publish_local(joined, output / WINDOWS_NAME, windows)
        publish_local(extracted, output / SOURCE_NAME, source)
        checksum = work / CHECKSUM_NAME
        checksum.write_text(
            windows['sha256'] + '  ' + WINDOWS_NAME + '\n' +
            source['sha256'] + '  ' + SOURCE_NAME + '\n', encoding='ascii')
        checksum_spec = {'file_name': CHECKSUM_NAME, 'size': checksum.stat().st_size,
                         'sha256': sha(checksum)}
        publish_local(checksum, output / CHECKSUM_NAME, checksum_spec)
    assets = [{k: s[k] for k in ('file_name', 'size', 'sha256')}
              for s in (windows, source, checksum_spec)]
    print('Verified Windows ZIP, executable, corresponding source ZIP, and checksums.', flush=True)
    return data, assets


def gh(*arguments: str, allow_missing: bool = False) -> str | None:
    env = dict(os.environ)
    env.pop('GH_DEBUG', None)
    env['GH_PROMPT_DISABLED'] = '1'
    result = subprocess.run(['gh', *arguments], check=False, capture_output=True,
                            text=True, env=env, timeout=180)
    if result.returncode:
        if allow_missing and 'HTTP 404' in result.stderr:
            return None
        raise RuntimeError('GitHub CLI failed: ' + result.stderr.strip()[:2000])
    return result.stdout


def api(route: str, allow_missing: bool = False) -> dict | None:
    value = gh('api', route, allow_missing=allow_missing)
    return None if value is None else json.loads(value)


def validate_existing_tag(repository: str, commit: str) -> None:
    ref = api(f'repos/{repository}/git/ref/tags/{TAG}', allow_missing=True)
    if ref is None:
        return
    obj = ref['object']
    for _ in range(8):
        if obj.get('type') == 'commit':
            if obj.get('sha') != commit:
                fail('An existing tag points to a different commit; no tag or release was changed')
            return
        if obj.get('type') != 'tag':
            fail('Unexpected existing tag object')
        obj = api(f'repos/{repository}/git/tags/{obj["sha"]}')['object']
    fail('Tag chain is too long')


def verify_remote_asset(repository: str, release: dict, expected: dict, directory: Path) -> bool:
    matches = [a for a in release.get('assets', []) if a.get('name') == expected['file_name']]
    if not matches:
        return False
    if len(matches) != 1 or matches[0].get('state') != 'uploaded':
        fail('Existing release asset is duplicated or not fully uploaded')
    if matches[0].get('size') != expected['size']:
        fail('Existing asset has a different size; it will not be replaced')
    gh('release', 'download', TAG, '--repo', repository, '--pattern', expected['file_name'],
       '--dir', str(directory))
    checked(directory / expected['file_name'], expected, 'Downloaded ' + expected['file_name'])
    return True


def publish(data: dict, assets: list[dict], output: Path, repository: str, commit: str) -> dict:
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository):
        fail('Invalid repository')
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        fail('A full commit SHA is required')
    route = f'repos/{repository}/releases/tags/{TAG}'
    release = api(route, allow_missing=True)
    created = release is None
    if created:
        validate_existing_tag(repository, commit)
        gh('release', 'create', TAG, '--repo', repository, '--target', commit,
           '--prerelease', '--title', data['title'], '--notes-file',
           str(payload_file(data['notes_path'])),
           *[str(output / a['file_name']) for a in assets])
        release = api(route)
    if release.get('draft') is not False:
        fail('Release is still a draft; public download is not confirmed')
    with tempfile.TemporaryDirectory(prefix='download-verify-', dir=output) as work:
        work = Path(work)
        missing = [a for a in assets if not verify_remote_asset(repository, release, a, work)]
        if missing:
            # The existing assets have all passed download/hash verification.
            # No --clobber: only absent assets may be added to a partial release.
            gh('release', 'upload', TAG, '--repo', repository,
               *[str(output / a['file_name']) for a in missing])
            release = api(route)
            for expected in missing:
                if not verify_remote_asset(repository, release, expected, work):
                    fail('An expected release asset is missing after upload')
    urls = {a['name']: a['browser_download_url'] for a in release['assets']
            if a['name'] in {a['file_name'] for a in assets}}
    result = {'status': 'PASS_PUBLISHED_ASSETS_DOWNLOADED_AND_HASH_VERIFIED',
              'tag': TAG, 'repository': repository, 'workflow_commit': commit,
              'release_url': release['html_url'], 'created_new_release': created,
              'assets': [dict(a, url=urls[a['file_name']]) for a in assets],
              'windows_executable': data['exe'], 'executable_executed': False,
              'scope': 'Publication and byte identity; not a new emulator/gameplay test.'}
    print('Published and download-verified: ' + release['html_url'], flush=True)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--repository')
    parser.add_argument('--commit')
    parser.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    if args.publish and (not args.repository or not args.commit):
        parser.error('--publish requires --repository and --commit')
    output = args.output.absolute()
    data, assets = assemble(payload_file(args.manifest), output)
    result = {'status': 'PASS_LOCAL_ASSEMBLY', 'assets': assets, 'executable_executed': False}
    if args.publish:
        result = publish(data, assets, output, args.repository, args.commit)
    (output / 'release_verification.json').write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
