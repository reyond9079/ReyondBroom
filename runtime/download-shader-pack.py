#!/usr/bin/env python3
"""Verified data-only installer; never extracts executable game logic."""
from pathlib import Path, PurePosixPath
import argparse, hashlib, json, os, stat, tempfile, urllib.request, zipfile

def install(destination,manifest):
    destination=destination.resolve();destination.mkdir(parents=True,exist_ok=True)
    (destination/'error.txt').unlink(missing_ok=True)
    expected=json.loads(manifest.read_text(encoding='utf-8-sig'))
    if not expected['url'].startswith('https://github.com/reyond9079/Reyond-FTE-Assets/releases/download/'):
        raise ValueError('Unexpected shader pack download host.')
    if not 0<expected['size']<=64*1024*1024:raise ValueError('Invalid download size.')
    with tempfile.TemporaryDirectory(prefix='install-',dir=destination) as scratch:
        archive=Path(scratch)/'pack.zip';received=0
        with urllib.request.urlopen(expected['url'],timeout=45) as response,archive.open('wb') as out:
            while True:
                chunk=response.read(65536)
                if not chunk:break
                received+=len(chunk)
                if received>expected['size']:raise ValueError('Download exceeds pinned size.')
                out.write(chunk);(destination/'progress.txt').write_text(f'{received} {expected["size"]} Downloading')
        if received!=expected['size'] or hashlib.sha256(archive.read_bytes()).hexdigest()!=expected['sha256']:
            raise ValueError('Shader archive checksum mismatch.')
        (destination/'ready.txt').unlink(missing_ok=True)
        content=destination/'content';content.mkdir(exist_ok=True)
        with zipfile.ZipFile(archive) as z:
            entries=z.infolist();names=[e.filename for e in entries]
            if len(names)!=len(set(n.casefold() for n in names)) or set(names)!=set(expected['files']):raise ValueError('Unexpected archive entries.')
            if sum(e.file_size for e in entries)>64*1024*1024:raise ValueError('Expanded pack exceeds limit.')
            for entry in entries:
                name=entry.filename;path=PurePosixPath(name);spec=expected['files'][name]
                if path.is_absolute() or any(p in ('.','..') for p in path.parts) or any(c in name for c in '\\:\r\n\0') or stat.S_ISLNK(entry.external_attr>>16):raise ValueError('Unsafe archive path.')
                target=content/name
                if not target.resolve().is_relative_to(content.resolve()):raise ValueError('Archive escapes install directory.')
                if entry.file_size!=spec['size'] or entry.file_size>16*1024*1024:raise ValueError('Invalid expanded size.')
                data=z.read(entry)
                if hashlib.sha256(data).hexdigest()!=spec['sha256']:raise ValueError('Shader file checksum mismatch.')
                target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        (destination/'files.txt').write_text('\n'.join(sorted(expected['files']))+'\n',encoding='utf-8')
        (destination/'ready.tmp').write_text(expected['version']+' '+expected['sha256'])
        os.replace(destination/'ready.tmp',destination/'ready.txt')
        (destination/'progress.txt').write_text(f'{received} {received} Ready')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--destination',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True);a=p.parse_args()
    try:install(a.destination,a.manifest)
    except Exception as e:
        a.destination.mkdir(parents=True,exist_ok=True);(a.destination/'error.txt').write_text(str(e),encoding='utf-8');raise SystemExit(1)
