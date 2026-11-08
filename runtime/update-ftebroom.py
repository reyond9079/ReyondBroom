#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Portable Linux updater, retaining the legacy download-and-wait CLI.

--mode check writes a version offer without downloading. --mode prepare reports
real byte progress and stages verified files, then waits for the editor to exit.
Only restart.request containing the matching --session requests a relaunch.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import tarfile
import tempfile
import time
import http.client
import urllib.error
import urllib.request
import urllib.parse
import subprocess

RELEASES = 'https://api.github.com/repos/reyond9079/ReyondBroom/releases?per_page=100'
DOWNLOADS = 'https://github.com/reyond9079/ReyondBroom/releases/download/'
ASSETS = 'https://api.github.com/repos/reyond9079/ReyondBroom/releases/assets/'
TRANSIENT = (408, 429, 500, 502, 503, 504)

def version(tag):
    match = re.fullmatch(r'v?(\d+)\.(\d+)\.(\d+)(?:-alpha\.(\d+))?', tag)
    return tuple(int(x) if x is not None else 999999 for x in match.groups()) if match else None

def select_release(releases, current, sdl2=False):
    for release in sorted((r for r in releases if not r.get('draft') and version(r['tag_name'])),
                          key=lambda r: version(r['tag_name']), reverse=True):
        if version(release['tag_name']) <= version(current):
            continue
        platform = 'Linux-SDL2-x64' if sdl2 else 'Linux-x64'
        name = 'ReyondBroom-' + release['tag_name'].removeprefix('v') + '-' + platform + '.tar.gz'
        assets = {a['name']: a for a in release.get('assets', [])}
        if name in assets and name + '.sha256' in assets:
            return release['tag_name'], assets[name], assets[name + '.sha256']
    return None

def request(url):
    fresh = url + ('&' if '?' in url else '?') + '_rb_update=' + str(time.time_ns())
    headers = {'User-Agent': 'ReyondBroom-Updater', 'Cache-Control': 'no-cache',
               'Accept': 'application/octet-stream' if url.startswith(ASSETS) else 'application/vnd.github+json',
               'X-GitHub-Api-Version': '2022-11-28'}
    return urllib.request.urlopen(urllib.request.Request(fresh, headers=headers), timeout=60)

def asset_urls(asset):
    browser = asset.get('browser_download_url', '')
    if not browser.startswith(DOWNLOADS) or urllib.parse.urlsplit(browser).fragment:
        raise ValueError('Unexpected download URL')
    api = asset.get('url')
    if api and not re.fullmatch(re.escape(ASSETS) + r'[1-9][0-9]*', api):
        raise ValueError('Unexpected asset API URL')
    return [api, browser] if api else [browser]

def fetch(urls, consume, status):
    """Retry transport failures, never integrity failures or installation steps."""
    last = None
    for index, url in enumerate(urls):
        for attempt in range(3):
            try:
                with request(url) as response:
                    return consume(response)
            except urllib.error.HTTPError as error:
                last = error
                if error.code not in TRANSIENT:
                    if error.code in (403, 404) and index + 1 < len(urls):
                        break
                    raise
            except (urllib.error.URLError, TimeoutError, ConnectionError, http.client.IncompleteRead) as error:
                last = error
            if attempt < 2:
                status('Temporary download error. Retrying...')
                time.sleep(attempt + 1)
        if index + 1 < len(urls):
            status('Retrying through the alternative GitHub download route...')
    raise last

def download_release(asset, checksum, updates, tag, status, progress=None):
    # Validate both routes before making requests. Asset names must remain files.
    urls, checksum_urls = asset_urls(asset), asset_urls(checksum)
    name = asset['name']
    if PurePosixPath(name).name != name or '/' in name or '\\' in name:
        raise ValueError('Unexpected asset name')
    def read_checksum(response):
        fields = response.read(4097).decode('ascii').split()
        if len(fields) != 2 or not re.fullmatch('[0-9a-fA-F]{64}', fields[0]) or fields[1] != name:
            raise ValueError('Invalid release checksum')
        return fields[0].lower()
    digest = fetch(checksum_urls, read_checksum, status)
    if asset.get('digest') and asset['digest'] != 'sha256:' + digest:
        raise ValueError('Release checksum disagrees with GitHub asset digest')
    archive = target_under(updates, name)
    partial = target_under(updates, name + '.part')
    def transfer(response):
        received = 0
        if progress:
            progress(0, asset['size'])
        # Each route/retry starts a fresh file, never appending to a failed transfer.
        with partial.open('wb') as output:
            while block := response.read(1024*1024):
                received += len(block)
                if received > 512*1024**2:
                    raise ValueError('Update exceeds 512 MiB')
                output.write(block)
                if progress:
                    progress(received, asset['size'])
                else:
                    status(f'Downloading {tag}: {received//1024**2} / {asset["size"]//1024**2} MiB')
        if received != asset['size']:
            raise ConnectionError('Incomplete update download')
    try:
        fetch(urls, transfer, status)
        if file_hash(partial) != digest:
            raise ValueError('Update checksum mismatch')
        os.replace(partial, archive)
    finally:
        partial.unlink(missing_ok=True)
    return archive

def file_hash(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            digest.update(block)
    return digest.hexdigest()

def target_under(root, name):
    relative = PurePosixPath(name)
    if relative.is_absolute() or not relative.parts or '..' in relative.parts or '\\' in name:
        raise ValueError('Unsafe package path')
    target = root.joinpath(*relative.parts)
    cursor = target
    while cursor != root:
        if cursor.is_symlink():
            raise ValueError('Update path contains a symbolic link')
        cursor = cursor.parent
    return target

def stage_package(archive, stage):
    seen = set()
    total = 0
    with tarfile.open(archive, 'r:gz') as package:
        for entry in package:
            parts = PurePosixPath(entry.name).parts
            if not parts or parts[0] != 'ReyondBroom-Linux-x64':
                raise ValueError('Unexpected package layout')
            if len(parts) == 1 and entry.isdir():
                continue
            relative = '/'.join(parts[1:])
            target = target_under(stage, relative)
            if entry.isdir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            if not entry.isfile() or relative in seen:
                raise ValueError('Links, special files and duplicate entries are not allowed')
            total += entry.size
            if total > 2*1024**3:
                raise ValueError('Expanded update exceeds 2 GiB')
            seen.add(relative)
            target.parent.mkdir(parents=True, exist_ok=True)
            with package.extractfile(entry) as source, target.open('xb') as output:
                shutil.copyfileobj(source, output)
            target.chmod(0o755 if entry.mode & 0o111 else 0o644)
    for name in ('fteqw-gl64', 'fteplug_ftebroom_amd64.so', 'Launch-ReyondBroom.sh', 'distribution-manifest.json'):
        if not (stage/name).is_file():
            raise ValueError('Incomplete update: ' + name)
    manifest = json.loads((stage/'distribution-manifest.json').read_text())
    for name, digest in manifest.items():
        if file_hash(target_under(stage, name)) != digest:
            raise ValueError('Package file checksum mismatch: ' + name)
    if seen != set(manifest) | {'distribution-manifest.json'}:
        raise ValueError('Package manifest does not cover all files')

def preserve(root, name, old):
    path = target_under(root, name)
    leaf = path.name
    if name.startswith(('updates/', 'downloads/', 'ftebroom/autosave/', 'ftebroom/cache/')):
        return True
    if leaf in ('appearance.cfg', 'workspace.ini', 'config.cfg', 'fte.cfg', 'installed.lst'):
        return path.exists()
    if re.match(r'ftebroom-(project|game-profile|quake1|build|preferences|run|plugins|camera|recent)', leaf):
        return path.exists()
    if path.is_file() and name.startswith('ftebroom/') and path.suffix.lower() in ('.cfg','.ini','.map','.wad','.def','.ent','.fgd','.txt'):
        return name not in old or file_hash(path) != old[name]
    return False

def install(root, stage, backup):
    try:
        old = json.loads((root/'distribution-manifest.json').read_text())
    except (OSError, ValueError):
        old = {}
    changed = []
    try:
        for source in sorted(stage.rglob('*')):
            if not source.is_file():
                continue
            name = source.relative_to(stage).as_posix()
            if preserve(root, name, old):
                continue
            target = target_under(root, name)
            saved = target_under(backup, name)
            existed = target.exists()
            if existed:
                saved.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(target, saved)
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_name(target.name + '.ftebroom-update.tmp')
            if temporary.exists() or temporary.is_symlink():
                raise ValueError('An unfinished update file already exists')
            shutil.copy2(source, temporary)
            changed.append((target, saved, existed))
            os.replace(temporary, target)
    except Exception:
        for target, saved, existed in reversed(changed):
            if existed:
                os.replace(saved, target)
            elif target.exists():
                target.unlink()
        raise

def process_identity(pid):
    try:
        return Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()[19]
    except OSError:
        return None

def cleanup_updates(updates, keep_backup):
    """Remove only updater-owned direct children, never following links."""
    updates = updates.resolve()
    keep_backup = keep_backup.resolve()
    for path in updates.iterdir():
        if path.is_symlink() or path.resolve() == keep_backup:
            continue
        if path.is_dir() and re.fullmatch(r'(stage|backup)-[a-zA-Z0-9_-]+', path.name):
            # rmtree does not follow symlinks contained within the directory.
            shutil.rmtree(path)
        elif path.is_file() and re.fullmatch(
                r'(?:ReyondBroom|FTEBroom)-[0-9][a-zA-Z0-9.\-]*-(Windows-x64\.zip|Linux-(?:SDL2-)?x64\.tar\.gz)(\.part|\.sha256)?', path.name):
            path.unlink()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--current-version', required=True)
    parser.add_argument('--parent-id', type=int, default=0)
    parser.add_argument('--local-package', type=Path)
    parser.add_argument('--mode', choices=('auto', 'check', 'prepare'), default='auto')
    parser.add_argument('--session', default='')
    parser.add_argument('--target-version', default='')
    args = parser.parse_args()
    root = args.root.resolve()
    if not (root/'fteqw-gl64').is_file() or not version(args.current_version):
        raise ValueError('Invalid installation or version')
    updates = target_under(root, 'updates')
    updates.mkdir(exist_ok=True)
    if args.session and not re.fullmatch(r'[a-zA-Z0-9_-]{1,80}', args.session):
        raise ValueError('Invalid update session')
    state = dict(session=args.session, current_version=args.current_version,
                 available_version='', stage='checking', received=0, total=0)
    def status(text, **fields):
        state.update(fields)
        state['message'] = text
        temporary = target_under(updates, 'status.tmp')
        temporary.write_text(text, encoding='utf-8')
        os.replace(temporary, target_under(updates, 'status.txt'))
        temporary = target_under(updates, 'status-json.tmp')
        temporary.write_text(json.dumps(state), encoding='utf-8')
        os.replace(temporary, target_under(updates, 'status.json'))
    with target_under(updates, 'update.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        try:
            identity = process_identity(args.parent_id) if args.parent_id else None
            archive = args.local_package
            if not archive:
                status('Checking GitHub releases...')
                releases = fetch([RELEASES], json.load, status)
                if args.target_version:
                    releases = [r for r in releases if r.get('tag_name', '').removeprefix('v') == args.target_version.removeprefix('v')]
                release = select_release(releases, args.current_version, (root/'linux-sdl2-build').is_file())
                if not release:
                    if args.target_version:
                        raise ValueError('The selected release is no longer available')
                    status('ReyondBroom is up to date for Linux.', stage='current')
                    return
                tag, asset, checksum = release
                state['available_version'] = tag.removeprefix('v')
                if args.mode == 'check':
                    status('A new version of ReyondBroom is available.', stage='available', total=asset['size'])
                    return
                status('Downloading update...', stage='downloading')
                def progress(received, total):
                    status(f'Downloading {tag}: {received/1024**2:.1f} / {total/1024**2:.1f} MiB',
                           stage='downloading', received=received, total=total)
                archive = download_release(asset, checksum, updates, tag, status, progress)
            elif args.mode == 'check':
                raise ValueError('Check-only mode cannot install a local package')
            status('Verifying and preparing the update...', stage='verifying')
            stage = Path(tempfile.mkdtemp(prefix='stage-', dir=updates))
            stage_package(archive, stage)
            status('Update verified. Restart the editor to finish updating.' if args.mode == 'prepare' else
                   'Update downloaded. Close the editor to install it.', stage='ready')
            while identity and process_identity(args.parent_id) == identity:
                time.sleep(1)
            backup = Path(tempfile.mkdtemp(prefix='backup-', dir=updates))
            status('Installing update...', stage='installing')
            install(root, stage, backup)
            try:
                cleanup_updates(updates, backup)
            except OSError:
                # A locked cache file must not invalidate a successful installation.
                pass
            status('Update installed. Settings and documents were preserved. Start ReyondBroom normally.', stage='installed')
            restart = target_under(updates, 'restart.request')
            if args.mode == 'prepare' and args.session and restart.is_file() and restart.read_text().strip() == args.session:
                restart.unlink()
                # Only a button press in the matching editor session can relaunch.
                try:
                    subprocess.Popen([str(root/'Launch-ReyondBroom.sh')], cwd=root,
                                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL, start_new_session=True)
                except OSError:
                    # Relaunch failure must never roll back a verified installation.
                    status('Update installed. Start ReyondBroom normally to continue.', stage='installed')
        except Exception as error:
            status('Update failed: ' + str(error), stage='error')
            raise

if __name__ == '__main__':
    main()
