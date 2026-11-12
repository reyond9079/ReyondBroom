#!/usr/bin/env python3
"""Validate authored module ownership, replay and real profile BSP/lighting."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);p.add_argument('--fixtures-only',action='store_true');a=p.parse_args()
repo=Path(__file__).resolve().parents[1];root=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);suffix='.exe' if os.name=='nt' else ''
sources=[*sorted((repo/'native/world_architect').glob('*.h')),repo/'tests/world-modules.cpp',repo/'plugins/autolevel/al_brush.cpp',repo/'runtime/world-architect/registry.json']
(out/'provenance.json').write_text(json.dumps({'sourceHashes':{str(f.relative_to(repo)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources}},indent=2)+'\n')
def run(command,log,cwd=repo):
 with log.open('wb') as stream:subprocess.run(list(map(str,command)),cwd=cwd,stdout=stream,stderr=subprocess.STDOUT,check=True)
run(['g++','-std=c++17','-O1',repo/'tests/world-modules.cpp','-o',out/('world-modules'+suffix)],out/'build.log')
run([out/('world-modules'+suffix),repo/'runtime/world-architect/registry.json',out],out/'contracts.log')
if a.fixtures_only:
 print('PASS authored modules fixture generation/contracts for focused UI checks',flush=True);raise SystemExit(0)
proof=[]
for profile in [2,0,1]:
 source=out/('profile-'+str(profile))/'baseq3/module.map'
 for stage in (['qbsp','light'] if profile==2 else ['bsp','light']):
  log=out/(str(profile)+'-'+stage+'.log')
  if profile==2:command=[root/'tools/ericw'/(stage+suffix),source if stage=='qbsp' else source.with_suffix('.bsp')]
  else:command=[root/'tools/q3map2'/('q3map2'+suffix),'-game','quake3','-fs_basepath',source.parent.parent]+(['-meta',source] if stage=='bsp' else ['-light','-fast','-samples','1',source.with_suffix('.bsp')])
  run(command,log,source.parent);text=log.read_text(errors='replace').lower();assert 'map leaked' not in text and "couldn't find image" not in text and not source.with_suffix('.pts').exists()
  if profile==2 and stage=='light':assert '0 empty lightmaps' in text
 bsp=source.with_suffix('.bsp');assert bsp.stat().st_size>4096;proof.append({'profile':profile,'compiledSha256':hashlib.sha256(bsp.read_bytes()).hexdigest(),'bspBytes':bsp.stat().st_size});print('PASS authored module BSP+light profile',profile,flush=True)
(out/'compiled.json').write_text(json.dumps(proof,indent=2)+'\n')
