#!/usr/bin/env python3
"""Compile the real Plant Editor's exported map and inspect its BSP model surfaces."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('artifacts',type=Path);a=p.parse_args()
root=a.runtime.resolve();out=a.artifacts.resolve();source=out/'build-snapshot'
base=out/'q3-compile';game=base/'baseq3';shutil.copytree(source,game,dirs_exist_ok=True)
compiler=root/'tools/q3map2'/('q3map2.exe' if os.name=='nt' else 'q3map2')
command=[str(compiler),'-game','quake3','-fs_basepath',str(base),'-fs_homepath',str(base),'-meta',str(game/'plant.map')]
result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
(out/'q3map2.log').write_bytes(result.stdout)
assert result.returncode==0,result.stdout.decode(errors='replace')[-2500:]
bsp=(game/'plant.bsp').read_bytes();assert bsp[:4]==b'IBSP' and struct.unpack_from('<i',bsp,4)[0]==46
def lump(n):
 offset,size=struct.unpack_from('<ii',bsp,8+n*8);assert 0<=offset<=len(bsp) and 0<=size<=len(bsp)-offset
 return bsp[offset:offset+size]
shaders=lump(1);names=[shaders[n:n+64].split(b'\0')[0].decode() for n in range(0,len(shaders),72)]
plants={i for i,name in enumerate(names) if name.startswith('textures/reyond-plugins/plant-')}
assert plants,'Compiled BSP omitted the generated plant material'
surfaces=lump(13);triangles=0
for at in range(0,len(surfaces),104):
 shader=struct.unpack_from('<i',surfaces,at)[0]
 if shader in plants:triangles+=struct.unpack_from('<i',surfaces,at+24)[0]//3
assert triangles>1000,'Compiled BSP omitted the generated plant geometry'
text=result.stdout.decode(errors='replace')
assert any('entering scripts/'+name+'.shader' in text for name in (game/'scripts/shaderlist.txt').read_text().split() if name.startswith('plant-')),'Compiler did not read the plant cutout shader'
report={'compiler':str(compiler),'bsp_version':46,'plant_triangles':triangles,'plant_shaders':[names[i] for i in plants]}
(out/'q3-compile-results.json').write_text(json.dumps(report,indent=2))
print('PASS real q3map2 BSP compilation:',triangles,'plant triangles; cutout shader loaded',flush=True)
