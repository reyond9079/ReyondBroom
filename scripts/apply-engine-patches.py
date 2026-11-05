# SPDX-License-Identifier: GPL-3.0-or-later
"""Apply the ordered engine patch stack safely, including overlapping patches.

Reconstruct only affected files from the pinned commit in a temporary directory.
An existing file must match the base, a known ordered patch stage, or final result. Never
replace an unrecognized local edit. Re-running on a patched tree is a no-op.
"""
import argparse, hashlib, pathlib, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('engine',type=pathlib.Path);p.add_argument('--check-only',action='store_true');a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[1];engine=a.engine.resolve()
patches=sorted((repo/'patches').glob('*.patch'),key=lambda x:x.name)
paths=set()
for patch in patches:
    for line in patch.read_text(encoding='utf-8').splitlines():
        if line.startswith(('--- a/','+++ b/')):
            path=line[6:].split('\t')[0]
            if pathlib.PurePosixPath(path).is_absolute() or '..' in pathlib.PurePosixPath(path).parts:raise RuntimeError('Unsafe patch path')
            paths.add(path)
with tempfile.TemporaryDirectory(prefix='reyond-engine-patches-') as folder:
    temp=pathlib.Path(folder);base={}
    normalize=lambda data:data.replace(b'\r\n',b'\n') if data is not None else None
    fingerprint=lambda data:hashlib.sha256(normalize(data)).digest() if data is not None else None
    recognized={}
    subprocess.run(['git','init','-q',str(temp)],check=True)
    for path in paths:
        read=subprocess.run(['git','-C',str(engine),'show','f937b9d88f71fc4429db5fe56c6a98d922711b2e:'+path],capture_output=True)
        base[path]=read.stdout if read.returncode==0 else None
        recognized[path]={fingerprint(base[path])}
        if base[path] is not None:
            file=temp/path;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(base[path])
    for patch in patches:
        run=subprocess.run(['git','-C',str(temp),'apply','--ignore-whitespace',str(patch)],capture_output=True,text=True)
        if run.returncode:raise RuntimeError(patch.name+': '+run.stderr)
        touched={line[6:].split('\t')[0] for line in patch.read_text(encoding='utf-8').splitlines() if line.startswith(('--- a/','+++ b/'))}
        for path in touched:
            file=temp/path
            recognized[path].add(fingerprint(file.read_bytes() if file.exists() else None))
    changes=[]
    for path in sorted(paths):
        file=engine/path;target=temp/path;current=file.read_bytes() if file.exists() else None;expected=target.read_bytes() if target.exists() else None
        # Git on Windows may have checked text out with CRLF.
        if normalize(current)==normalize(expected):continue
        if fingerprint(current) not in recognized[path]:raise RuntimeError('Unrecognized local engine changes: '+path+'; use a fresh pinned checkout')
        changes.append((file,expected))
    if not a.check_only:
        for file,data in changes:
            if data is None:file.unlink()
            else:file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(data)
    print('PASS',len(patches),'ordered engine patches;',len(paths),'files;',len(changes),'files '+('to update' if a.check_only else 'updated'))
