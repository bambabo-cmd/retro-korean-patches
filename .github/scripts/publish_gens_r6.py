#!/usr/bin/env python3
"""Assemble the pinned R6 payload and publish/download-verify immutable assets.

Assembly uses only Python's standard library. GitHub release operations use the
workflow's short-lived token through gh. Existing tags and assets are never
replaced. PE headers are inspected; no Windows executable or DLL is executed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PAYLOAD = PurePosixPath('release-payloads/gens-ko/20261123_01')
TAG = 'gens-ko-2.14-r6-20261123-01'
VERSION = 'R6_20261123_01'
PART_SIZE = 262144
BLOCK = 1024 * 1024
MAX_FILE_SIZE = 200 * 1024 * 1024
SOURCE_PREFIX = 'Gens_KO_R6_source/'
FILENAMES = {
    'windows': 'Gens_KO_R6_20261123_01_Windows.zip',
    'source': 'Gens_KO_R6_20261123_01_Source.zip',
    'validation': 'Gens_KO_R6_20261123_01_Validation_KO.txt',
    'checksums': 'Gens_KO_R6_20261123_01_SHA256SUMS.txt',
}
BINARY_MEMBERS = ('Gens_KO/Gens_KO.exe', 'Gens_KO/librashader.dll')


def fail(message: str) -> None:
    raise ValueError(message)


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            fail('Duplicate JSON key: ' + key)
        result[key] = value
    return result


def read_json(text: str) -> dict:
    value = json.loads(text, object_pairs_hook=unique_object)
    if not isinstance(value, dict):
        fail('Expected a JSON object')
    return value


def stream_sha(stream) -> str:
    digest = hashlib.sha256()
    for data in iter(lambda: stream.read(BLOCK), b''):
        digest.update(data)
    return digest.hexdigest()


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return stream_sha(stream)


def spec(value: object, label: str) -> dict:
    if not isinstance(value, dict):
        fail(label + ' must be an object')
    size, digest = value.get('size'), value.get('sha256')
    if type(size) is not int or not 0 < size <= MAX_FILE_SIZE:
        fail(label + ' has an invalid size')
    if not isinstance(digest, str) or not re.fullmatch(r'[0-9a-f]{64}', digest):
        fail(label + ' has an invalid SHA-256')
    return value


def clean_relative(value: object, label: str) -> PurePosixPath:
    if not isinstance(value, str) or not value or '\\' in value or '\x00' in value:
        fail(label + ' has an invalid path')
    path = PurePosixPath(value)
    if path.is_absolute() or '..' in path.parts or path.as_posix() != value:
        fail(label + ' must be a canonical relative path')
    return path


def payload_file(value: object) -> Path:
    relative = clean_relative(value, 'Payload')
    if not relative.is_relative_to(PAYLOAD):
        fail('Payload path must stay in ' + str(PAYLOAD))
    current = ROOT
    for part in relative.parts:
        current = current / part
        if current.is_symlink():
            fail('Payload symlink is not permitted: ' + str(value))
    if not current.is_file():
        fail('Payload file is missing: ' + str(value))
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


def zip_members(archive: zipfile.ZipFile) -> set[str]:
    items = archive.infolist()
    names = [item.filename for item in items]
    if len(names) != len(set(names)) or not 1 <= len(names) <= 50000:
        fail('ZIP contains duplicate members or an invalid member count')
    if sum(item.file_size for item in items) > 512 * 1024 * 1024:
        fail('ZIP exceeds the bounded uncompressed size')
    for item in items:
        clean_relative(item.filename, 'ZIP member')
        if item.is_dir() or stat.S_ISLNK(item.external_attr >> 16) or item.flag_bits & 1:
            fail('ZIP contains a directory, symlink, or encrypted member')
    return set(names)


def verify_windows(path: Path, binaries: list[dict]) -> dict:
    with zipfile.ZipFile(path) as archive:
        names = zip_members(archive)
        for expected in binaries:
            member = expected['member']
            info = archive.getinfo(member)
            if info.file_size != expected['size']:
                fail(member + ' has a different size')
            data = archive.read(member)
            if hashlib.sha256(data).hexdigest() != expected['sha256']:
                fail(member + ' has a different SHA-256')
            if len(data) < 64 or data[:2] != b'MZ':
                fail(member + ' has no DOS/PE header')
            pe = struct.unpack_from('<I', data, 0x3c)[0]
            if pe + 26 > len(data) or data[pe:pe + 4] != b'PE\0\0':
                fail(member + ' has an invalid PE header')
            if struct.unpack_from('<H', data, pe + 4)[0] != 0x14c:
                fail(member + ' is not i386')
            if struct.unpack_from('<H', data, pe + 24)[0] != 0x10b:
                fail(member + ' is not PE32')
            is_dll = bool(struct.unpack_from('<H', data, pe + 22)[0] & 0x2000)
            if is_dll != member.endswith('.dll'):
                fail(member + ' has the wrong executable/DLL characteristic')
        required = {'Gens_KO/shaders/crt-mattias.glsl', 'Gens_KO/shaders/crt-mattias.glslp',
                    'Gens_KO/README_KO.txt', 'Gens_KO/VALIDATION.json',
                    'Gens_KO/verification/r6_shaders/slang_capi.json'}
        if not required.issubset(names):
            fail('Windows ZIP is missing required R6 shader/runtime evidence files')
        validation = read_json(archive.read('Gens_KO/VALIDATION.json').decode('utf-8'))
        if validation.get('version') != VERSION or validation.get('status') != 'PASS':
            fail('Windows ZIP contains an unexpected or failed validation record')
        for binary in binaries:
            recorded = validation['binaries'][PurePosixPath(binary['member']).name]
            if recorded.get('sha256') != binary['sha256'] or recorded.get('bytes') != binary['size']:
                fail('Release manifest and embedded binary validation disagree')
        return {'members': len(names), 'binaries_pe32_and_sha256_verified': True,
                'windows_gui_executed': validation.get('windows_gui_executed')}


def verify_source(path: Path) -> dict:
    with zipfile.ZipFile(path) as archive:
        names = zip_members(archive)
        manifest_name = SOURCE_PREFIX + 'SOURCE_MANIFEST.json'
        manifest = read_json(archive.read(manifest_name).decode('utf-8'))
        if manifest.get('version') != VERSION or manifest.get('algorithm') != 'sha256':
            fail('Source ZIP has an unexpected source manifest version or algorithm')
        files = manifest.get('files')
        if not isinstance(files, dict) or not files:
            fail('Source ZIP manifest has no files')
        expected_names = {manifest_name}
        for name, expected in files.items():
            clean_relative(name, 'Source manifest')
            if not isinstance(expected, dict):
                fail('Invalid source manifest entry: ' + name)
            size, digest = expected.get('bytes'), expected.get('sha256')
            if type(size) is not int or not 0 <= size <= MAX_FILE_SIZE:
                fail('Invalid source size: ' + name)
            if not isinstance(digest, str) or not re.fullmatch(r'[0-9a-f]{64}', digest):
                fail('Invalid source SHA-256: ' + name)
            member = SOURCE_PREFIX + name
            if member == manifest_name:
                fail('Source manifest cannot hash itself')
            expected_names.add(member)
            if archive.getinfo(member).file_size != size:
                fail('Source member size differs: ' + name)
            with archive.open(member) as stream:
                if stream_sha(stream) != digest:
                    fail('Source member SHA-256 differs: ' + name)
        if names != expected_names:
            fail('Source ZIP and its complete source manifest have different member sets')
        return {'members': len(names), 'source_manifest_files_verified': len(files)}


def assemble(manifest: Path, output: Path) -> tuple[dict, list[dict], dict]:
    data = read_json(manifest.read_text(encoding='utf-8'))
    if type(data.get('schema_version')) is not int or data['schema_version'] != 1:
        fail('Expected manifest schema_version 1')
    if data.get('tag') != TAG:
        fail('Unexpected release tag')
    title = data.get('title')
    if not isinstance(title, str) or not title.strip() or len(title) > 256:
        fail('Invalid release title')
    notes = payload_file(data.get('notes_path'))
    if notes.stat().st_size > 256 * 1024 or not notes.read_text(encoding='utf-8').strip():
        fail('Release notes must contain bounded UTF-8 text')
    values = data.get('assets')
    if not isinstance(values, list) or len(values) != 4:
        fail('Expected exactly four release assets')
    by_role = {}
    for value in values:
        item = spec(value, 'asset')
        role = item.get('role')
        if role not in FILENAMES or role in by_role or item.get('file_name') != FILENAMES[role]:
            fail('Unexpected or duplicate asset role/filename')
        by_role[role] = item
    binaries = data.get('binaries')
    if not isinstance(binaries, list) or len(binaries) != 2:
        fail('Expected the EXE and Slang DLL specifications')
    for value in binaries:
        spec(value, 'binary')
    if {value.get('member') for value in binaries} != set(BINARY_MEMBERS):
        fail('Unexpected EXE/DLL members')
    if output.is_symlink():
        fail('Output directory must not be a symlink')
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='assemble-r6-', dir=output) as temporary:
        work = Path(temporary)
        for role, expected in by_role.items():
            target = work / expected['file_name']
            if role in ('windows', 'source'):
                parts = expected.get('parts')
                count = (expected['size'] + PART_SIZE - 1) // PART_SIZE
                if 'path' in expected or not isinstance(parts, list) or len(parts) != count:
                    fail(role + ' has an invalid ordered part list')
                with target.open('xb') as destination:
                    for index, value in enumerate(parts):
                        part = spec(value, role + ' part')
                        exact = str(PAYLOAD / role / f'{role}.part{index:03d}')
                        expected_size = min(PART_SIZE, expected['size'] - index * PART_SIZE)
                        if part.get('path') != exact or part['size'] != expected_size:
                            fail(role + ' part order/path/size differs at ' + str(index))
                        path = payload_file(part['path'])
                        checked(path, part, exact)
                        with path.open('rb') as stream:
                            shutil.copyfileobj(stream, destination, BLOCK)
            else:
                if 'parts' in expected:
                    fail(role + ' must be a direct text payload')
                path = payload_file(expected.get('path'))
                checked(path, expected, role)
                shutil.copyfile(path, target)
                target.read_text(encoding='utf-8')
            checked(target, expected, expected['file_name'])
        windows = verify_windows(work / FILENAMES['windows'], binaries)
        source = verify_source(work / FILENAMES['source'])
        sums = (work / FILENAMES['checksums']).read_text(encoding='ascii')
        expected_sums = {by_role[r]['file_name']: by_role[r]['sha256']
                         for r in ('windows', 'source', 'validation')}
        actual_sums = {}
        for line in sums.splitlines():
            match = re.fullmatch(r'([0-9a-f]{64})  ([A-Za-z0-9_.-]+)', line)
            if not match or match.group(2) in actual_sums:
                fail('Invalid or duplicate checksum line')
            actual_sums[match.group(2)] = match.group(1)
        if actual_sums != expected_sums:
            fail('Checksum file does not match the three payload assets')
        for role in FILENAMES:
            expected = by_role[role]
            publish_local(work / expected['file_name'], output / expected['file_name'], expected)
    assets = [{key: by_role[role][key] for key in ('role', 'file_name', 'size', 'sha256')}
              for role in FILENAMES]
    evidence = {'windows': windows, 'source': source, 'checksums_verified': True,
                'manifest_sha256': sha(manifest), 'chunk_bytes': PART_SIZE}
    print('Verified R6 parts, four assets, EXE/DLL PE32 headers, and every source manifest entry.', flush=True)
    return data, assets, evidence


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
    return None if value is None else read_json(value)


def validate_existing_tag(repository: str, commit: str) -> None:
    ref = api(f'repos/{repository}/git/ref/tags/{TAG}', allow_missing=True)
    if ref is None:
        return
    obj = ref['object']
    for _ in range(8):
        if obj.get('type') == 'commit':
            if obj.get('sha') != commit:
                fail('Existing tag points to another commit; no tag or release was changed')
            return
        if obj.get('type') != 'tag':
            fail('Unexpected existing tag object')
        obj = api(f'repos/{repository}/git/tags/{obj["sha"]}')['object']
    fail('Tag chain is too long')


def verify_remote_asset(repository: str, release: dict, expected: dict, directory: Path) -> bool:
    matches = [asset for asset in release.get('assets', []) if asset.get('name') == expected['file_name']]
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
           *[str(output / asset['file_name']) for asset in assets])
        release = api(route)
    if release.get('draft') is not False or release.get('tag_name') != TAG:
        fail('Release is not the expected public download')
    with tempfile.TemporaryDirectory(prefix='download-verify-r6-', dir=output) as temporary:
        work = Path(temporary)
        missing = [asset for asset in assets if not verify_remote_asset(repository, release, asset, work)]
        if missing:
            # No --clobber: verified existing assets remain untouched.
            gh('release', 'upload', TAG, '--repo', repository,
               *[str(output / asset['file_name']) for asset in missing])
            release = api(route)
            for expected in missing:
                if not verify_remote_asset(repository, release, expected, work):
                    fail('Expected asset is missing after upload')
    expected_names = {asset['file_name'] for asset in assets}
    urls = {asset['name']: asset['browser_download_url'] for asset in release['assets']
            if asset['name'] in expected_names}
    result = {'status': 'PASS_PUBLISHED_ASSETS_DOWNLOADED_AND_HASH_VERIFIED',
              'tag': TAG, 'repository': repository, 'workflow_commit': commit,
              'release_url': release['html_url'], 'created_new_release': created,
              'assets': [dict(asset, url=urls[asset['file_name']]) for asset in assets],
              'windows_executable': next(binary for binary in data['binaries']
                                         if binary['member'].endswith('/Gens_KO.exe')),
              'binaries': data['binaries'], 'executable_executed': False,
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
    data, assets, evidence = assemble(payload_file(args.manifest), output)
    result = {'status': 'PASS_LOCAL_ASSEMBLY', 'tag': TAG, 'assets': assets,
              'executable_executed': False}
    if args.publish:
        result = publish(data, assets, output, args.repository, args.commit)
    result['assembly_verification'] = evidence
    (output / 'release_verification.json').write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
