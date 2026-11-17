#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Install verified upstream Quake compilers and retain their exact source."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tempfile
import time
import urllib.error
import urllib.request
import zipfile

PIN = Path(__file__).resolve().parents[1]/'tools/ericw/PIN.json'

def checked_download(url, expected, destination):
    if destination.is_file() and hashlib.sha256(destination.read_bytes()).hexdigest() == expected:
        return
    for attempt in range(3):
        try:
            request = urllib.request.Request(url, headers={'User-Agent': 'ReyondBroom-build'})
            with urllib.request.urlopen(request, timeout=30) as response, destination.open('wb') as output:
                shutil.copyfileobj(response, output)
            if hashlib.sha256(destination.read_bytes()).hexdigest() != expected:
                raise ValueError('Compiler download checksum mismatch')
            return
        except (OSError, urllib.error.URLError):
            if attempt == 2:
                raise
            time.sleep(attempt+1)

def install(destination, platform, sources=None, cache=None):
    pin = json.loads(PIN.read_text(encoding='utf-8'))
    destination = Path(destination)
    cache = Path(cache) if cache else Path(tempfile.gettempdir())/'reyondbroom-compiler-downloads'
    cache.mkdir(parents=True, exist_ok=True)
    windows = platform == 'Windows-x64'
    key = 'binary' if windows else 'linux'
    archive = cache/('ericw-'+pin['version']+'-'+('windows' if windows else 'linux')+'.zip')
    checked_download(pin[key+'_url'], pin[key+'_sha256'], archive)
    source = cache/('ericw-tools-'+pin['version']+'-source.zip')
    checked_download(pin['source_url'], pin['source_sha256'], source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.ericw-', dir=destination.parent) as temporary:
        staged = Path(temporary)/'bundle'
        staged.mkdir()
        with zipfile.ZipFile(archive) as bundle:
            for member in bundle.infolist():
                name = PurePosixPath(member.filename)
                if name.is_absolute() or '..' in name.parts or '\\' in member.filename:
                    raise ValueError('Invalid compiler archive member')
                if (member.is_dir() or '.doctrees' in name.parts or name.name == '.buildinfo' or
                        name.name in {'lightpreview','lightpreview.exe'} or name.name.startswith('Qt6') or
                        name.parts[0] in {'platforms','imageformats','styles','tls','translations'}):
                    continue
                target = staged.joinpath(*name.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                with bundle.open(member) as original, target.open('wb') as output:
                    shutil.copyfileobj(original, output)
        for name in ('qbsp', 'vis', 'light'):
            path = staged/(name+'.exe' if windows else name)
            if not path.is_file():
                raise ValueError('The compiler archive is missing '+name)
            if not windows:
                path.chmod(path.stat().st_mode | 0o111)
                path.rename(staged/(name+'-bin'))
                path.write_text('#!/bin/sh\ncompiler_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                                'export LD_LIBRARY_PATH="$compiler_dir'+'$'+'{LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
                                'exec "$compiler_dir/'+name+'-bin" "$@"\n', encoding='utf-8')
                path.chmod(0o755)
        for name in ('PIN.json','BUNDLE.md'):
            shutil.copy2(PIN.with_name(name), staged/name)
        if windows:
            # Use the redistributable already shipped for the portable Q2 tools.
            for name in ('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll'):
                dependency=destination.parent/'ericw-q2'/name
                if dependency.is_file():
                    shutil.copy2(dependency, staged/name)
        (staged/'VERSION.txt').write_text('ericw-tools '+pin['version']+'\n'+pin['commit']+'\n', encoding='ascii')
        backup = Path(temporary)/'previous'
        if destination.is_symlink():
            raise ValueError('Unexpected link in compiler destination')
        had_previous = destination.exists()
        if had_previous:
            destination.rename(backup)
        try:
            staged.rename(destination)
        except OSError:
            if had_previous:
                backup.rename(destination)
            raise
    source_destination = Path(sources) if sources else destination
    source_destination.mkdir(parents=True, exist_ok=True)
    for old in source_destination.glob('ericw-tools-*-source.zip'):
        if old.name != source.name:
            old.unlink()
    (source_destination/'embree-v2.17.7-source.tar.gz').unlink(missing_ok=True)
    shutil.copy2(source, source_destination/source.name)
    print('Verified ericw-tools', pin['version'], platform)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--platform', choices=('Windows-x64','Linux-x64','Linux-SDL2-x64'), required=True)
    parser.add_argument('--sources', type=Path)
    parser.add_argument('--cache', type=Path)
    args = parser.parse_args()
    install(args.destination, args.platform, args.sources, args.cache)

