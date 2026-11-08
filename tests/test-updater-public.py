"""Install an explicitly verified public release in a disposable real runtime.

The release is resolved before starting and pinned for the updater, so publishing
another release during the test cannot change its expected version or bytes.
The real downloaded archive is independently checked while the updater waits
for a live parent; no transport, installer or hashing code is substituted.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import http.client
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.error
import urllib.request
import zipfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--target-version', help='Optional published version to verify; defaults to the newest complete release for this platform.')
p.add_argument('--current-version', default='0.5.0-alpha.129')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
windows = os.name == 'nt'
platform = 'Windows-x64' if windows else 'Linux-SDL2-x64' if (a.runtime/'linux-sdl2-build').exists() else 'Linux-x64'
module = 'fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'
api = 'https://api.github.com/repos/reyond9079/ReyondBroom/releases'
http_errors = []


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def version(tag):
    match = re.fullmatch(r'v?(\d+)\.(\d+)\.(\d+)(?:-alpha\.(\d+))?', tag)
    return tuple(int(x) if x is not None else 999999 for x in match.groups()) if match else None


def public_read(urls, limit):
    """Unauthenticated, bounded public reads; never inherit GH_TOKEN from CI."""
    last = None
    for route, url in enumerate(urls):
        for attempt in range(3):
            try:
                fresh = url + ('&' if '?' in url else '?') + '_rb_public_qa=' + str(time.time_ns())
                headers = {'User-Agent': 'ReyondBroom-Public-Update-QA', 'Cache-Control': 'no-cache',
                           'Accept': 'application/octet-stream' if '/releases/assets/' in url else 'application/vnd.github+json',
                           'X-GitHub-Api-Version': '2022-11-28'}
                with urllib.request.urlopen(urllib.request.Request(fresh, headers=headers), timeout=60) as response:
                    result = response.read(limit+1)
                assert len(result) <= limit, 'Public response exceeds its limit'
                return result
            except urllib.error.HTTPError as error:
                last = error
                try:
                    body = error.read(4096).decode('utf-8', errors='replace')
                except (OSError, http.client.HTTPException) as read_error:
                    body = 'Cannot read HTTP error body: '+str(read_error)
                failure = {'url': fresh, 'status': error.code, 'attempt': attempt+1,
                           'retry_after': error.headers.get('Retry-After'), 'body': body}
                http_errors.append(failure)
                (a.output/'public-http-errors.json').write_text(json.dumps(http_errors, indent=2))
                print('Public HTTP error: '+json.dumps(failure), file=sys.stderr, flush=True)
                error.close()
                if error.code not in (408, 429, 500, 502, 503, 504):
                    if error.code in (403, 404) and route+1 < len(urls):
                        break
                    raise
            except (urllib.error.URLError, TimeoutError, ConnectionError, http.client.IncompleteRead) as error:
                last = error
            if attempt < 2:
                time.sleep(attempt+1)
    raise last


assert version(a.current_version), 'Invalid baseline version'
if a.target_version:
    assert version(a.target_version), 'Invalid requested public version'
    releases = [json.loads(public_read([api+'/tags/v'+a.target_version.removeprefix('v')], 1024*1024))]
else:
    releases = json.loads(public_read([api+'?per_page=20'], 8*1024*1024))
candidates = []
for release in releases:
    tag = release.get('tag_name', '')
    if release.get('draft') or not version(tag) or version(tag) <= version(a.current_version):
        continue
    if a.target_version and version(tag) != version(a.target_version):
        continue
    name = 'ReyondBroom-'+tag.removeprefix('v')+'-'+platform+('.zip' if windows else '.tar.gz')
    assets = {asset['name']: asset for asset in release.get('assets', [])}
    if name in assets and name+'.sha256' in assets:
        candidates.append((release, assets[name], assets[name+'.sha256']))
assert candidates, 'No complete newer public release for '+platform
release, asset, checksum = max(candidates, key=lambda item: version(item[0]['tag_name']))
target = release['tag_name'].removeprefix('v')
name = asset['name']
assert Path(name).name == name and '/' not in name and '\\' not in name
assert 0 < asset['size'] <= 512*1024*1024
checksum_urls = []
if checksum.get('url'):
    assert re.fullmatch(re.escape(api)+r'/assets/[1-9][0-9]*', checksum['url'])
    checksum_urls.append(checksum['url'])
assert checksum['browser_download_url'].startswith('https://github.com/reyond9079/ReyondBroom/releases/download/')
checksum_urls.append(checksum['browser_download_url'])
fields = public_read(checksum_urls, 4096).decode('ascii').split()
assert len(fields) == 2 and re.fullmatch('[a-fA-F0-9]{64}', fields[0]) and fields[1] == name
expected_archive_sha = fields[0].lower()
if asset.get('digest'):
    assert asset['digest'] == 'sha256:'+expected_archive_sha, 'Public asset digest disagrees with its checksum'
(a.output/'public-target.json').write_text(json.dumps({'tag': release['tag_name'], 'release_id': release['id'],
    'platform': platform, 'asset': asset, 'checksum': checksum, 'archive_sha256': expected_archive_sha}, indent=2))

root = a.output/'installation with spaces'
shutil.copytree(a.runtime, root)
data = root/'ftebroom'
(data/'maps/updater-user.map').write_text('user map survives the real download')
(data/'ftebroom-preferences.cfg').write_text('user bindings survive the real download')
protected = ['build-info.txt', module, 'ftebroom/csaddon.dat']
protected += ['bin/fteqw64.exe' if (root/'bin/fteqw64.exe').exists() else 'fteqw64.exe'] if windows else ['fteqw-gl64']
before = {name: sha(root/name) for name in protected}
parent = subprocess.Popen([sys.executable, '-c', 'import time;time.sleep(600)'])
session = 'public-qa-'+str(time.time_ns())
if windows:
    command = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(a.runtime/'ftebroom/update-ftebroom.ps1'),
               '-Root', str(root), '-CurrentVersion', a.current_version, '-TargetVersion', target, '-ParentId', str(parent.pid), '-Session', session]
else:
    command = [sys.executable, str(a.runtime/'ftebroom/update-ftebroom.py'), '--root', str(root),
               '--current-version', a.current_version, '--target-version', target, '--parent-id', str(parent.pid), '--session', session]
log = (a.output/'public-update.log').open('wb')
worker = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
try:
    deadline = time.monotonic()+240
    while time.monotonic() < deadline:
        assert worker.poll() is None, 'Updater exited before the real parent wait; see public-update.log'
        try:
            status = json.loads((root/'updates/status.json').read_text(encoding='utf-8-sig'))
            if status.get('session') == session and status.get('stage') == 'ready':
                assert status['current_version'] == a.current_version and status['available_version'] == target
                break
        except (OSError, UnicodeError, json.JSONDecodeError):
            pass
        time.sleep(.1)
    else:
        raise AssertionError('Public download did not reach the parent-exit boundary')
    assert parent.poll() is None
    assert {name: sha(root/name) for name in protected} == before, 'Live binaries changed before parent exit'
    time.sleep(.2)
    assert worker.poll() is None and {name: sha(root/name) for name in protected} == before
    archive = root/'updates'/asset['name']
    assert archive.is_file() and archive.stat().st_size == asset['size']
    assert sha(archive) == expected_archive_sha, 'Downloaded archive differs from the verified public checksum'

    hashes = {}
    contents = {}
    prefix = 'ReyondBroom-Windows-x64/' if windows else 'ReyondBroom-Linux-x64/'
    def inspect(name, stream):
        assert name.startswith(prefix), 'Unexpected public archive root'
        relative = name[len(prefix):]
        path = PurePosixPath(relative)
        assert relative and not path.is_absolute() and '..' not in path.parts and '\\' not in relative and relative not in hashes
        digest = hashlib.sha256()
        keep = relative in ('build-info.txt', 'distribution-manifest.json')
        blocks = []
        for block in iter(lambda: stream.read(1024*1024), b''):
            digest.update(block)
            if keep:
                blocks.append(block)
        hashes[relative] = digest.hexdigest()
        if keep:
            contents[relative] = b''.join(blocks)
    if windows:
        with zipfile.ZipFile(archive) as package:
            for entry in package.infolist():
                if not entry.is_dir():
                    with package.open(entry) as stream:
                        inspect(entry.filename, stream)
    else:
        with tarfile.open(archive, mode='r|gz') as package:
            for entry in package:
                if entry.isdir():
                    continue
                assert entry.isfile(), 'Public package contains a link or special entry'
                with package.extractfile(entry) as stream:
                    inspect(entry.name, stream)
    manifest = json.loads(contents['distribution-manifest.json'])
    assert set(hashes) == set(manifest) | {'distribution-manifest.json'}
    assert all(hashes[name] == digest for name, digest in manifest.items()), 'Public package manifest differs from its actual files'
    expected_info = contents['build-info.txt'].decode('utf-8').replace('\r\n', '\n')
    assert expected_info.splitlines()[0] == 'ReyondBroom '+target
    assert 'Module SHA256: '+hashes[module]+'\n' in expected_info
    core = [module, 'ftebroom/csaddon.dat', 'build-info.txt']
    core += ['bin/fteqw64.exe' if 'bin/fteqw64.exe' in hashes else 'fteqw64.exe'] if windows else ['fteqw-gl64']
    assert all(name in hashes for name in core)

    parent.terminate()
    parent.wait(timeout=15)
    assert worker.wait(timeout=120) == 0, 'See '+str(a.output/'public-update.log')
    assert (root/'build-info.txt').read_text().replace('\r\n', '\n') == expected_info
    assert all(sha(root/name) == hashes[name] for name in core), 'Installed core bytes differ from the verified downloaded archive'
    assert (data/'maps/updater-user.map').read_text() == 'user map survives the real download'
    assert (data/'ftebroom-preferences.cfg').read_text() == 'user bindings survive the real download'
    assert list((root/'updates').glob('backup-*')) and 'installed' in (root/'updates/status.txt').read_text(encoding='utf-8-sig')
    assert any((backup/module).is_file() and sha(backup/module) == before[module]
               for backup in (root/'updates').glob('backup-*')), 'Rollback module does not match the original installation'
    assert (root/'linux-sdl2-build').exists() == (a.runtime/'linux-sdl2-build').exists()
    proof = {'passed': True, 'target_version': target, 'release_id': release['id'], 'platform': platform,
        'archive_name': name, 'archive_sha256': expected_archive_sha, 'archive_bytes': asset['size'],
        'before_sha256': before, 'installed_core_sha256': {name: hashes[name] for name in core},
        'module_sha256': hashes[module], 'rollback_module_sha256': before[module], 'waited_for_parent_exit': True, 'maps_preferences_preserved': True,
        'backend_preserved': True, 'archive_manifest_files_verified': len(manifest),
        'method': 'Unauthenticated public GitHub metadata and checksum; pinned public target; actual downloaded archive SHA/size/full manifest verification; real parent wait and byte-exact core installation.'}
    (a.output/'public-result.json').write_text(json.dumps(proof, indent=2))
    print(json.dumps(proof, indent=2))
finally:
    if worker.poll() is None:
        worker.kill();worker.wait()
    if parent.poll() is None:
        parent.kill();parent.wait()
    log.close()
