#!/usr/bin/env python3
"""Compile prerequisite gates and inspect actual BSP brush models/target wiring."""
import argparse,hashlib,json,os,re,shutil,struct,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);p.add_argument('--fixtures-only',action='store_true');p.add_argument('--applied',type=Path);p.add_argument('--fixture-texture',type=Path);a=p.parse_args()
repo=Path(__file__).resolve().parents[1];root=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);suffix='.exe' if os.name=='nt' else ''
sources=[*sorted((repo/'native/world_architect').glob('*.h')),repo/'tests/world-gates.cpp',repo/'plugins/autolevel/al_brush.cpp',repo/'runtime/world-architect/registry.json',repo/'qc/fb_world_architect.qc']
(out/'provenance.json').write_text(json.dumps({'sourceHashes':{str(f.relative_to(repo)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources}},indent=2)+'\n')
def run(command,log,cwd=repo):
 with log.open('wb') as stream:subprocess.run(list(map(str,command)),cwd=cwd,stdout=stream,stderr=subprocess.STDOUT,check=True)
if a.applied:
 assert not a.fixtures_only and a.fixture_texture,'Applied checks need the fixture texture';ui=a.applied.resolve();state=json.loads((ui/'world-state.json').read_text());profile=state['settings']['profileId'];game=out/('applied-'+str(profile))/'baseq3';shutil.copytree(ui/'exported',game,dirs_exist_ok=True);source=game/'gates-applied.map';original=ui/'wa-gates-compile.map';text=original.read_text();assert '"_wa_snapshot_0"' in text and '"_wa_project"' in text,'Expected the executed owned editor build snapshot';shutil.copy2(original,source);texture=game/'textures/ftebroom/qa_blue.tga';texture.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(a.fixture_texture,texture);cases=[(profile,source)];(out/'ui-provenance.json').write_text((ui/'provenance.json').read_text())
else:
 run(['g++','-std=c++17','-O1',repo/'tests/world-gates.cpp','-o',out/('world-gates'+suffix)],out/'build.log')
 run([out/('world-gates'+suffix),repo/'runtime/world-architect/registry.json',out],out/'contracts.log')
 if a.fixtures_only:
  print('PASS authored gates fixture generation/contracts for focused UI checks',flush=True);raise SystemExit(0)
 cases=[(profile,out/('profile-'+str(profile))/'baseq3/gates.map') for profile in [2,0,1]]
proof=[]
for profile,source in cases:
 for stage in (['qbsp','light'] if profile==2 else ['bsp','light']):
  log=out/(str(profile)+'-'+stage+'.log')
  if profile==2:command=[root/'tools/ericw'/(stage+suffix),source if stage=='qbsp' else source.with_suffix('.bsp')]
  else:command=[root/'tools/q3map2'/('q3map2'+suffix),'-game','quake3','-fs_basepath',source.parent.parent]+(['-meta',source] if stage=='bsp' else ['-light','-fast','-samples','1',source.with_suffix('.bsp')])
  run(command,log,source.parent);text=log.read_text(errors='replace').lower();assert 'map leaked' not in text and "couldn't find image" not in text and not source.with_suffix('.pts').exists(),str(log)
  if profile==2 and stage=='light':assert '0 empty lightmaps' in text
 bsp=source.with_suffix('.bsp').read_bytes();assert len(bsp)>4096;header=4 if profile==2 else 8
 offset,length=struct.unpack_from('<ii',bsp,header);entities=[dict(re.findall(r'"([^"\n]*)"\s*"([^"\n]*)"',item)) for item in re.findall(r'\{([^{}]*)\}',bsp[offset:offset+length].decode(errors='replace'))]
 doors=[e for e in entities if e.get('classname')=='func_door'];triggers=[e for e in entities if e.get('classname')=='trigger_multiple'];assert len(doors)==len(triggers)==1
 door,trigger=doors[0],triggers[0];assert door['targetname']==trigger['target'] and door['model']!=trigger['model']
 offset,length=struct.unpack_from('<ii',bsp,header+(14 if profile==2 else 7)*8);stride=64 if profile==2 else 40;assert length%stride==0
 models=[]
 for entity in [door,trigger]:
  assert entity['model'].startswith('*');index=int(entity['model'][1:]);assert 0<index<length//stride;at=offset+index*stride;bounds=struct.unpack_from('<6f',bsp,at);assert all(bounds[i+3]>bounds[i] for i in range(3))
  if profile!=2:assert struct.unpack_from('<i',bsp,at+36)[0]>0,'Compiled gate lost its real brush hull'
  models.append({'class':entity['classname'],'model':entity['model'],'bounds':bounds})
 proof.append({'profile':profile,'compiledSha256':hashlib.sha256(bsp).hexdigest(),'bspBytes':len(bsp),'models':models,'targetname':door['targetname'],'sourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),'method':'Actual editor Apply/production ExportMap' if a.applied else 'Authored gate core fixture'});print('PASS physical gate BSP/light + actual brush models profile',profile,flush=True)
(out/'compiled.json').write_text(json.dumps(proof,indent=2)+'\n')
