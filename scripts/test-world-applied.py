#!/usr/bin/env python3
"""Compile the editor's actual build snapshot after Apply and inspect its liquid BSP."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('compiler',type=Path)
p.add_argument('ui',type=Path)
p.add_argument('--fixture-texture',type=Path,required=True)
a=p.parse_args();out=a.ui.resolve();compiler=a.compiler.resolve()
state=json.loads((out/'world-state.json').read_text())
assert state['settings']['profileId'] in (0,1),'This oracle requires the Q3/FTE fixture'
original=out/'wa-compile.map';text=original.read_text()
assert '"_wa_snapshot_0"' in text and '"_wa_project"' in text,'Expected executed owned Apply, not the preview export'
base=out/'compiled-applied';game=base/'baseq3'
shutil.copytree(out/'exported',game,dirs_exist_ok=True)
source=game/'maps/world-applied.map';source.parent.mkdir(exist_ok=True);shutil.copy2(original,source)
fixture=game/'textures/ftebroom/qa_blue.tga';fixture.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(a.fixture_texture,fixture)
for stage in ('bsp','light'):
 command=[str(compiler),'-game','quake3','-fs_basepath',str(base),'-fs_homepath',str(base)]
 command+=['-meta',str(source)] if stage=='bsp' else ['-light','-fast','-samples','1',str(source.with_suffix('.bsp'))]
 result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
 (out/('applied-'+stage+'.log')).write_bytes(result.stdout)
 log=result.stdout.decode(errors='replace').lower()
 assert result.returncode==0,log[-2500:]
 assert 'map leaked' not in log and "couldn't find image" not in log,log[-2500:]
bsp=source.with_suffix('.bsp').read_bytes()
assert bsp[:4]==b'IBSP' and struct.unpack_from('<i',bsp,4)[0]==46
def lump(index,stride):
 offset,length=struct.unpack_from('<ii',bsp,8+index*8)
 assert 0<=offset<=len(bsp) and 0<=length<=len(bsp)-offset and length%stride==0
 return range(offset,offset+length,stride)
water={i for i,offset in enumerate(lump(1,72)) if struct.unpack_from('<i',bsp,offset+68)[0]&32}
water_brushes=sum(struct.unpack_from('<i',bsp,offset+8)[0] in water for offset in lump(8,12))
assert water_brushes>0,'Executed Apply lost the metro liquid contents'
assert source.read_bytes()==original.read_bytes(),'Compiler input differed from the actual editor build snapshot'
report={'sourceSha256':hashlib.sha256(original.read_bytes()).hexdigest(),
 'compilerSha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),
 'waterShaders':len(water),'waterBrushes':water_brushes,'bspBytes':len(bsp),
 'method':'Executed editor Apply and production ExportMap snapshot, unchanged compiler input, real q3map2 BSP/light and binary liquid-content inspection'}
(out/'applied-compile.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS actual owned build snapshot compiles and retains',water_brushes,'liquid BSP brushes',flush=True)
