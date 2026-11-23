#!/usr/bin/env python3
"""Publish the pinned Mesen R2 archives after reconstruction and download checks.

Existing tags/assets are never replaced. No emulator binary is executed.
New releases remain drafts until every uploaded asset has been downloaded and
verified. Authentication uses only the Actions-provided GH_TOKEN.
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
PAYLOAD = PurePosixPath('release-payloads/mesen-ko/20261123_01')
REPOSITORY = 'bambabo-cmd/retro-korean-patches'
TAG = 'mesen-ko-2.2.1-r2-test-20261123-01'
TITLE = 'MesenCE KO 2.2.1 R2 TEST (Windows x64)'
PREFIX = 'MesenCE_KO_2.2.1_R2_TEST_20261123_01_'
CHUNK_SIZE = 524288
BLOCK = 1024 * 1024
PINNED = {
    PREFIX + 'Windows_x64.zip': (74292933, '1ab513269303a2bb5b174ce044669a0d1ce4b3c45333311d0b5f58f3759c67d0', 'windows_x64'),
    PREFIX + 'source.zip': (42096245, 'd75dde78c0b799322d8d763abab0e7deaf26611b20385cff055965acf1e34af1', 'source'),
    PREFIX + 'SHA256SUMS.txt': (233, '211ab88ade331ba9a3bcf599e254dbfafdaf537c1d942aa372e485358d9df92d', None),
}

def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)

def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key: ' + key)
        result[key] = value
    return result

def read_json(text: str) -> dict:
    value = json.loads(text, object_pairs_hook=unique_object)
    require(isinstance(value, dict), 'Expected a JSON object')
    return value

def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(BLOCK), b''):
            digest.update(data)
    return digest.hexdigest()

def relative(value: object) -> PurePosixPath:
    require(isinstance(value, str) and bool(value) and '\\' not in value and '\x00' not in value,
            'Invalid relative path')
    path = PurePosixPath(value)
    require(not path.is_absolute() and '..' not in path.parts and path.as_posix() == value,
            'Path must be canonical and relative')
    return path

def payload_file(value: object) -> Path:
    path = relative(value)
    require(path.is_relative_to(PAYLOAD), 'Path leaves the release payload')
    current = ROOT
    for part in path.parts:
        current /= part
        require(not current.is_symlink(), 'Payload symlinks are not allowed')
    require(current.is_file(), 'Missing payload file: ' + str(path))
    return current

def checked(path: Path, size: int, digest: str) -> None:
    require(not path.is_symlink() and path.is_file(), 'Expected a regular file: ' + path.name)
    require(path.stat().st_size == size and sha(path) == digest, 'Size or SHA-256 differs: ' + path.name)

def verify_zip(path: Path, windows: bool) -> dict:
    with zipfile.ZipFile(path) as archive:
        items = archive.infolist()
        names = [item.filename for item in items]
        require(len(names) == len(set(names)) and 0 < len(names) <= 50000, 'Invalid ZIP member list')
        require(sum(item.file_size for item in items) <= 1024 * 1024 * 1024, 'ZIP exceeds size bound')
        for item in items:
            relative(item.filename.rstrip('/') if item.is_dir() else item.filename)
            require(not stat.S_ISLNK(item.external_attr >> 16) and not item.flag_bits & 1,
                    'ZIP has a symlink or encrypted member')
        require(archive.testzip() is None, 'ZIP CRC verification failed')
        if windows:
            required = {'MesenCE_KO/Mesen.exe', 'MesenCE_KO/Mesen.dll',
                        'MesenCE_KO/README_KO_R2.txt', 'MesenCE_KO/QA_KO_R2.txt',
                        'MesenCE_KO/SOURCE_NOTICE_KO.txt'}
            require(required.issubset(names), 'Windows ZIP is missing required files')
            data = archive.read('MesenCE_KO/Mesen.exe')
            require(len(data) >= 64 and data[:2] == b'MZ', 'Mesen.exe has no DOS header')
            pe = struct.unpack_from('<I', data, 0x3c)[0]
            require(pe + 26 <= len(data) and data[pe:pe + 4] == b'PE\0\0', 'Invalid PE header')
            require(struct.unpack_from('<H', data, pe + 4)[0] == 0x8664
                    and struct.unpack_from('<H', data, pe + 24)[0] == 0x20b,
                    'Mesen.exe must be Windows x64 PE32+')
            require(not struct.unpack_from('<H', data, pe + 22)[0] & 0x2000, 'Mesen.exe is a DLL')
            return {'members': len(names), 'crc_verified': True, 'exe_pe_x64_verified': True}
        required = {'MesenCE_KO_source/Mesen.sln', 'MesenCE_KO_source/LICENSE',
                    'MesenCE_KO_source/Core/Shared/Emulator.cpp',
                    'MesenCE_KO_source/UI/ViewModels/MainWindowViewModel.cs',
                    'MesenCE_KO_source/BUILD_KO_R2.txt', 'MesenCE_KO_source/README_KO_R2.txt'}
        require(required.issubset(names), 'Source ZIP is missing required source/build/license files')
        return {'members': len(names), 'crc_verified': True, 'required_sources_verified': True}

def assemble(manifest: Path, output: Path) -> tuple[dict, list[dict], dict]:
    data = read_json(manifest.read_text(encoding='utf-8'))
    require(type(data.get('schema_version')) is int and data['schema_version'] == 1, 'Expected schema 1')
    require(data.get('tag') == TAG and data.get('title') == TITLE, 'Unexpected release tag/title')
    notes = payload_file(data.get('notes_path'))
    require(notes.stat().st_size <= 256 * 1024 and bool(notes.read_text(encoding='utf-8').strip()),
            'Release notes must contain bounded UTF-8 text')
    values = data.get('assets')
    require(isinstance(values, list) and len(values) == 2, 'Expected two archive assets')
    by_name = {}
    for item in values:
        require(isinstance(item, dict), 'Asset must be an object')
        name = item.get('asset_name')
        require(name in PINNED and PINNED[name][2] is not None and name not in by_name,
                'Unexpected or duplicate archive filename')
        size, digest, _ = PINNED[name]
        require(type(item.get('size')) is int and item['size'] == size and item.get('sha256') == digest
                and item.get('chunk_size') == CHUNK_SIZE, 'Archive differs from pinned original')
        chunks = item.get('chunks')
        require(isinstance(chunks, list) and len(chunks) == (size + CHUNK_SIZE - 1) // CHUNK_SIZE,
                'Invalid ordered chunk list')
        by_name[name] = item
    checksum = data.get('checksum')
    checksum_name = PREFIX + 'SHA256SUMS.txt'
    require(isinstance(checksum, dict) and checksum.get('name') == checksum_name, 'Unexpected checksum name')
    require(checksum.get('path') == str(PAYLOAD / checksum_name)
            and checksum.get('size') == PINNED[checksum_name][0]
            and checksum.get('sha256') == PINNED[checksum_name][1], 'Unexpected checksum metadata')
    require(not output.is_symlink(), 'Output directory must not be a symlink')
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='assemble-mesen-', dir=output) as temporary:
        work = Path(temporary)
        for name, item in by_name.items():
            size, digest, folder = PINNED[name]
            with (work / name).open('xb') as destination:
                for index, chunk in enumerate(item['chunks']):
                    require(isinstance(chunk, dict), 'Chunk must be an object')
                    exact = str(PAYLOAD / folder / f'part{index:04d}.bin')
                    expected_size = min(CHUNK_SIZE, size - index * CHUNK_SIZE)
                    require(chunk.get('path') == exact and chunk.get('size') == expected_size,
                            'Chunk path/order/size differs')
                    path = payload_file(exact)
                    checked(path, expected_size, chunk.get('sha256'))
                    content = path.read_bytes()
                    git_sha = hashlib.sha1(b'blob ' + str(len(content)).encode('ascii') + b'\0' + content).hexdigest()
                    require(chunk.get('git_sha') == git_sha, 'Chunk Git blob identity differs')
                    destination.write(content)
            checked(work / name, size, digest)
        checksum_source = payload_file(checksum['path'])
        checked(checksum_source, *PINNED[checksum_name][:2])
        shutil.copyfile(checksum_source, work / checksum_name)
        actual_sums = {}
        for line in checksum_source.read_text(encoding='ascii').splitlines():
            match = re.fullmatch(r'([0-9a-f]{64})  ([A-Za-z0-9_.-]+)', line)
            require(match is not None and match.group(2) not in actual_sums, 'Invalid checksum file')
            actual_sums[match.group(2)] = match.group(1)
        require(actual_sums == {name: PINNED[name][1] for name in by_name}, 'Checksum list differs')
        evidence = {'windows': verify_zip(work / (PREFIX + 'Windows_x64.zip'), True),
                    'source': verify_zip(work / (PREFIX + 'source.zip'), False),
                    'checksums_verified': True, 'manifest_sha256': sha(manifest)}
        for name, (size, digest, _) in PINNED.items():
            target = output / name
            if target.exists() or target.is_symlink():
                checked(target, size, digest)
            else:
                (work / name).replace(target)
    assets = [{'name': name, 'size': size, 'sha256': digest} for name, (size, digest, _) in PINNED.items()]
    print('Verified original hashes, ordered Git blobs, ZIP CRC, x64 EXE, sources and checksum list.', flush=True)
    return data, assets, evidence

def gh(*arguments: str, allow_missing: bool = False) -> str | None:
    env = dict(os.environ)
    env.pop('GH_DEBUG', None)
    env['GH_PROMPT_DISABLED'] = '1'
    result = subprocess.run(['gh', *arguments], capture_output=True, text=True, env=env, timeout=240)
    if result.returncode:
        if allow_missing and 'HTTP 404' in result.stderr:
            return None
        raise RuntimeError('GitHub CLI operation failed: ' + ' '.join(arguments[:2])
                           + ' (exit ' + str(result.returncode) + ')')
    return result.stdout

def api(route: str, allow_missing: bool = False) -> dict | None:
    value = gh('api', route, allow_missing=allow_missing)
    return None if value is None else read_json(value)

def find_release(repository: str) -> dict | None:
    # The tag-specific REST endpoint can omit drafts. Authenticated release
    # listing includes drafts, allowing interrupted uploads to resume safely.
    for page in range(1, 101):
        values = json.loads(gh('api', f'repos/{repository}/releases?per_page=100&page={page}'),
                            object_pairs_hook=unique_object)
        require(isinstance(values, list), 'Invalid release listing')
        matches = [item for item in values if item.get('tag_name') == TAG]
        require(len(matches) <= 1, 'Duplicate release tag')
        if matches:
            return matches[0]
        if len(values) < 100:
            return None
    raise ValueError('Release listing exceeded the bounded search')

def verify_tag_before_create(repository: str, commit: str) -> None:
    ref = api(f'repos/{repository}/git/ref/tags/{TAG}', allow_missing=True)
    if ref is None:
        return
    obj = ref['object']
    for _ in range(8):
        if obj.get('type') == 'commit':
            require(obj.get('sha') == commit, 'Existing tag targets another commit; it will not be replaced')
            return
        require(obj.get('type') == 'tag' and re.fullmatch(r'[0-9a-f]{40}', obj.get('sha', '')) is not None,
                'Unexpected existing tag object')
        obj = api(f'repos/{repository}/git/tags/{obj["sha"]}')['object']
    raise ValueError('Existing tag chain is too long')

def verify_remote_asset(repository: str, release: dict, expected: dict, directory: Path) -> bool:
    matches = [item for item in release.get('assets', []) if item.get('name') == expected['name']]
    if not matches:
        return False
    require(len(matches) == 1 and matches[0].get('state') == 'uploaded'
            and matches[0].get('size') == expected['size'], 'Existing asset differs; no overwrite allowed')
    gh('release', 'download', TAG, '--repo', repository, '--pattern', expected['name'], '--dir', str(directory))
    checked(directory / expected['name'], expected['size'], expected['sha256'])
    return True

def publish(data: dict, assets: list[dict], output: Path, repository: str, commit: str) -> dict:
    require(repository == REPOSITORY, 'Unexpected publication repository')
    require(re.fullmatch(r'[0-9a-f]{40}', commit) is not None, 'Expected a full commit SHA')
    release = find_release(repository)
    created = release is None
    if created:
        verify_tag_before_create(repository, commit)
        gh('release', 'create', TAG, '--repo', repository, '--target', commit, '--draft', '--prerelease',
           '--title', data['title'], '--notes-file', str(payload_file(data['notes_path'])))
        release = find_release(repository)
    require(isinstance(release, dict) and type(release.get('id')) is int,
            'Expected release was not found after creation')
    route = f'repos/{repository}/releases/{release["id"]}'
    require(release.get('tag_name') == TAG and release.get('prerelease') is True,
            'Release is not the expected R2 TEST prerelease')
    expected_names = {item['name'] for item in assets}
    require({item.get('name') for item in release.get('assets', [])}.issubset(expected_names),
            'Release has unexpected assets; it will not be changed')
    with tempfile.TemporaryDirectory(prefix='download-verify-mesen-', dir=output) as temporary:
        directory = Path(temporary)
        missing = [item for item in assets if not verify_remote_asset(repository, release, item, directory)]
        if missing:
            require(release.get('draft') is True, 'Published release is incomplete; it will not be modified')
            gh('release', 'upload', TAG, '--repo', repository, *[str(output / item['name']) for item in missing])
            release = api(route)
            for item in missing:
                require(verify_remote_asset(repository, release, item, directory), 'Asset missing after upload')
    if release.get('draft') is True:
        gh('release', 'edit', TAG, '--repo', repository, '--draft=false', '--prerelease', '--latest=false')
        release = api(route)
    require(release.get('draft') is False and release.get('prerelease') is True,
            'Release was not published as the expected prerelease')
    urls = {item['name']: item['browser_download_url'] for item in release['assets']}
    require(set(urls) == expected_names, 'Final release asset list differs')
    print('Published and download-verified: ' + release['html_url'], flush=True)
    return {'status': 'PASS_PUBLISHED_ASSETS_DOWNLOADED_AND_HASH_VERIFIED',
            'release_url': release['html_url'], 'tag': TAG, 'repository': repository,
            'workflow_commit': commit, 'created_new_release': created,
            'assets': [dict(item, url=urls[item['name']]) for item in assets],
            'windows_executable_executed': False,
            'scope': 'Publication and byte identity; no new Windows GPU/gameplay validation.'}

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--repository', default=os.environ.get('GITHUB_REPOSITORY'))
    parser.add_argument('--commit', default=os.environ.get('GITHUB_SHA'))
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--verify-only', action='store_true')
    mode.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    if args.publish and (not args.repository or not args.commit):
        parser.error('--publish requires --repository and --commit')
    output = args.output.absolute()
    data, assets, evidence = assemble(payload_file(args.manifest), output)
    result = {'status': 'PASS_LOCAL_ASSEMBLY', 'tag': TAG, 'assets': assets, 'windows_executable_executed': False}
    if args.publish:
        result = publish(data, assets, output, args.repository, args.commit)
    result['assembly_verification'] = evidence
    (output / 'release_verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n',
                                                     encoding='utf-8')

if __name__ == '__main__':
    main()
