#!/usr/bin/env python3
"""Compile actual MAPs produced by authored missions, fields, roads and dimensions."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);p.add_argument('--case',choices=['both','interior','roads'],default='both');a=p.parse_args()
repo=Path(__file__).resolve().parents[1];root=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);suffix='.exe' if os.name=='nt' else ''
sources=[*sorted((repo/'native/world_architect').glob('*.h')),repo/'tests/world-designer.cpp',repo/'plugins/autolevel/al_brush.cpp',repo/'runtime/world-architect/registry.json']
(out/'provenance.json').write_text(json.dumps({'compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0],'sourceHashes':{str(f.relative_to(repo)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources}},indent=2)+'\n')
def run(command,log,cwd=repo):
 with log.open('wb') as stream:subprocess.run(list(map(str,command)),cwd=cwd,stdout=stream,stderr=subprocess.STDOUT,check=True)
run(['g++','-std=c++17','-O1',repo/'tests/world-designer.cpp','-o',out/('world-designer'+suffix)],out/'build.log')
run([out/('world-designer'+suffix),repo/'runtime/world-architect/registry.json',out],out/'generation.log')
results=json.loads((out/'results.json').read_text())
if a.case!='both':results=[case for case in results if case['case']==a.case]
for case in results:
 source=Path(case['path']);profile=case['profile'];tag=str(profile)+'-'+case['case']
 if profile==2:
  for tool in ['qbsp','light']:
   log=out/(tag+'-'+tool+'.log');run([root/'tools/ericw'/(tool+suffix),source if tool=='qbsp' else source.with_suffix('.bsp')],log,source.parent)
   assert not source.with_suffix('.pts').exists(),'Designer MAP leaked'
   if tool=='light':assert '0 empty lightmaps' in log.read_text(errors='replace')
 else:
  for stage in ['bsp','light']:
   command=[root/'tools/q3map2'/('q3map2'+suffix),'-game','quake3','-fs_basepath',source.parent.parent]
   command+=['-meta',source] if stage=='bsp' else ['-light','-fast','-samples','1',source.with_suffix('.bsp')]
   log=out/(tag+'-'+stage+'.log');run(command,log,source.parent);text=log.read_text(errors='replace').lower();assert 'map leaked' not in text and "couldn't find image" not in text,text[-1000:]
 bsp=source.with_suffix('.bsp');assert bsp.stat().st_size>4096;case['compiledSha256']=hashlib.sha256(bsp.read_bytes()).hexdigest();print('PASS compiled designer',tag,flush=True)
(out/'compiled.json').write_text(json.dumps(results,indent=2)+'\n')
