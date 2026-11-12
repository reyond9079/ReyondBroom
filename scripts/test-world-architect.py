#!/usr/bin/env python3
"""Validate the World Architect stages and compile their real profile-aware MAPs."""
import argparse,hashlib,json,os,struct,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--seeds',type=int,default=3);a=p.parse_args()
assert 1<=a.seeds<=100
repo=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);suffix='.exe' if os.name=='nt' else ''
sources=[*sorted((repo/'native/world_architect').glob('*.h')),*sorted((repo/'plugins/autolevel').glob('al_*.h')),repo/'plugins/autolevel/al_brush.cpp',repo/'runtime/world-architect/registry.json',repo/'tests/world-architect.cpp',repo/'tests/world-architect-contracts.cpp',repo/'tests/world-assets.cpp']
(out/'provenance.json').write_text(json.dumps({'compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0],'sourceHashes':{str(path.relative_to(repo)):hashlib.sha256(path.read_bytes()).hexdigest() for path in sources},'seedsPerProfilePreset':a.seeds},indent=2)+'\n')
def run(command,log,cwd=None):
 with log.open('wb') as stream:subprocess.run([str(arg) for arg in command],cwd=cwd or repo,stdout=stream,stderr=subprocess.STDOUT,check=True)
for name in ['world-architect-contracts','world-assets','world-architect']:
 run(['g++','-std=c++17','-O1',repo/'tests'/(name+'.cpp'),'-o',out/(name+suffix)],out/(name+'-build.log'))
run([out/('world-architect-contracts'+suffix),repo/'runtime/world-architect/registry.json'],out/'contracts.log')
run([out/('world-assets'+suffix),repo/'runtime/world-architect/registry.json'],out/'assets.log')
proof=[]
for profile in [2,0,1]:
 folder=out/('profile-'+str(profile))/('baseq3' if profile<2 else 'maps-and-assets')
 run([out/('world-architect'+suffix),repo/'runtime/world-architect/registry.json',folder,'*',a.seeds,profile,'qa-world'],out/('profile-'+str(profile)+'.log'))
 results=json.loads((folder/'results.json').read_text());assert len(results)==8*a.seeds
 record={'profile':profile,'accepted_batches':len(results),'attempts':sum(r['attempted'] for r in results),'rejections':sum(len(r['rejected']) for r in results),'compiled':[]}
 record['presets']={}
 for preset in sorted({r['preset'] for r in results}):
  rows=[r for r in results if r['preset']==preset]
  def span(values):return {'min':min(values),'mean':sum(values)/len(values),'max':max(values)}
  variations={name:len({r['variation'][name] for r in rows}) for name in ['masses','rooms','roads','mission','architecture']}
  if a.seeds>=100:
   assert variations['rooms']>=a.seeds//2 and variations['architecture']>=a.seeds//2,(preset,'insufficient structural variation',variations)
   if preset not in ['hospital','metro','mall']:assert variations['masses']>=a.seeds//2 and variations['roads']>=a.seeds//2,(preset,'repetitive urban structure',variations)
  record['presets'][preset]={'batches':len(rows),'uniqueSelectedSeeds':len({r['seed'] for r in rows}),'structuralVariation':variations,'brushes':span([r['brushes'] for r in rows]),'mapBytes':span([r['mapBytes'] for r in rows]),'routeUnits':span([r['routeLength'] for r in rows]),'generationMs':span([sum(r['timingMs'].values()) for r in rows]),'roomHeightKinds':span([r['variation']['roomHeightKinds'] for r in rows])}
 record['compileSeconds']={}

 if a.runtime:
  runtime=a.runtime.resolve()
  for preset in ['hospital','checkpoint','evacuation','metro','factory','courtyard','overpass','mall']:
   compile_started=time.monotonic();source=folder/(preset+'-1.map')
   if profile==2:
    for tool in ['qbsp','light']:
     command=[runtime/'tools/ericw'/(tool+suffix),source if tool=='qbsp' else source.with_suffix('.bsp')]
     log=out/(str(profile)+'-'+preset+'-'+tool+'.log');run(command,log,cwd=folder)
     assert not source.with_suffix('.pts').exists(),'Unsealed compiled world'
     if tool=='light':assert '0 empty lightmaps' in log.read_text(errors='replace')
   else:
    for stage in ['bsp','light']:
     command=[runtime/'tools/q3map2'/('q3map2'+suffix),'-game','quake3','-fs_basepath',folder.parent]
     command+=['-meta',source] if stage=='bsp' else ['-light','-fast','-samples','1',source.with_suffix('.bsp')]
     log=out/(str(profile)+'-'+preset+'-'+stage+'.log');run(command,log,cwd=folder)
     text=log.read_text(errors='replace').lower()
     assert "couldn't find image" not in text and 'map leaked' not in text,text[-1000:]
   assert source.with_suffix('.bsp').stat().st_size>4096
   if preset=='metro':
    bsp=source.with_suffix('.bsp').read_bytes()
    if profile==2:
     kind=bsp[:4];stride=44 if kind==b'BSP2' else 32 if kind==b'2PSB' else 28
     assert kind in (b'BSP2',b'2PSB',struct.pack('<i',29)),'Unknown Quake BSP leaf layout'
     offset,length=struct.unpack_from('<ii',bsp,4+10*8);assert length%stride==0
     water=sum(struct.unpack_from('<i',bsp,at)[0]==-3 for at in range(offset,offset+length,stride))
     assert water>0,'Flooded station exported no actual liquid BSP leaves'
     record['waterLeaves']=water
    else:
     assert bsp[:4]==b'IBSP' and struct.unpack_from('<i',bsp,4)[0]==46
     offset,length=struct.unpack_from('<ii',bsp,8+1*8);assert length%72==0
     water={i for i,at in enumerate(range(offset,offset+length,72)) if struct.unpack_from('<i',bsp,at+68)[0]&0x20}
     offset,length=struct.unpack_from('<ii',bsp,8+8*8);assert length%12==0
     assert any(struct.unpack_from('<i',bsp,at+8)[0] in water for at in range(offset,offset+length,12)),'Flooded station exported no water-content brushes'
     record['waterShaders']=len(water)
   record['compiled'].append(preset);record['compileSeconds'][preset]=time.monotonic()-compile_started
 proof.append(record);print('PASS World Architect profile',profile,record,flush=True)
(out/'validation.json').write_text(json.dumps(proof,indent=2)+'\n')
