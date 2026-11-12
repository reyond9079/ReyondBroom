#!/usr/bin/env python3
"""Check lossless AutoLevel migration and prepare native UI input fixtures."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);a=p.parse_args();repo=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);suffix='.exe' if os.name=='nt' else ''
sources=[repo/'native/world_architect/legacy.h',repo/'native/world_architect/plan.h',repo/'plugins/autolevel/al_storage.cpp',repo/'plugins/autolevel/al_brush.cpp',repo/'tests/world-legacy.cpp']
(out/'provenance.json').write_text(json.dumps({'sourceHashes':{str(f.relative_to(repo)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources}},indent=2)+'\n')
with (out/'build.log').open('wb') as log:subprocess.run(['g++','-std=c++17','-O1',str(repo/'tests/world-legacy.cpp'),'-o',str(out/('world-legacy'+suffix))],stdout=log,stderr=subprocess.STDOUT,check=True,cwd=repo)
with (out/'contracts.log').open('wb') as log:subprocess.run([str(out/('world-legacy'+suffix)),str(out)],stdout=log,stderr=subprocess.STDOUT,check=True,cwd=repo)
print((out/'contracts.log').read_text(),end='')
with (out/'original-storage-build.log').open('wb') as log:subprocess.run(['g++','-std=c++17','-O1',str(repo/'tests/test-autolevel-storage.cpp'),'-o',str(out/('original-storage'+suffix))],stdout=log,stderr=subprocess.STDOUT,check=True,cwd=repo)
with (out/'original-storage.log').open('wb') as log:subprocess.run([str(out/('original-storage'+suffix))],stdout=log,stderr=subprocess.STDOUT,check=True,cwd=repo)
print((out/'original-storage.log').read_text(),end='')
