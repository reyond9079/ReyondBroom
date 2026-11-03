"""Build the standalone MIT-only shader download and its pinned installer manifest."""
from pathlib import Path
import argparse, hashlib, json, zipfile, re

p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[1];root=repo/'assets/reyond-shader-pack'
settings=re.findall(r'\{"((?:ryn_|rsp_)[^"]+)","[^"]+",([0-9.+-]+),', (repo/'native/ftebroom_shader_pack.h').read_text())
defaults='// SPDX-License-Identifier: MIT\n'+''.join('set '+key+' '+str(float(value))+'\n' for key,value in settings)
defaults+='set ryn_declination 23.44\nset ryn_weather_cloud 1\nset ryn_weather_rain 0\n'
(root/'reyond-pack.cfg').write_text(defaults,encoding='utf-8',newline='\n')
files=sorted(f for f in root.rglob('*') if f.is_file())
for f in files:
    name=f.relative_to(root).as_posix()
    if not (name in ('LICENSE.txt','README.md','PROVENANCE.md','reyond-pack.cfg') or name.startswith(('glsl/reyondpack/','textures/reyondpack/','scripts/','sdk/'))):
        raise ValueError(f'Unexpected pack input: {name}')
    if f.suffix in ('.glsl','.qc','.shader') and 'SPDX-License-Identifier: MIT' not in f.read_text():
        raise ValueError(f'Missing original MIT provenance: {name}')
a.output.mkdir(parents=True,exist_ok=True)
archive=a.output/'Reyond-GLSL-Shader-Pack-1.0.0.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for f in files:
        info=zipfile.ZipInfo(f.relative_to(root).as_posix(),(2026,11,3,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
        info.external_attr=0o100644<<16;z.writestr(info,f.read_bytes())
manifest={'version':'1.0.0','url':'https://github.com/reyond9079/Reyond-FTE-Assets/releases/download/shaders-v1.0.0/'+archive.name,
          'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'size':archive.stat().st_size,
          'files':{f.relative_to(root).as_posix():{'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'size':f.stat().st_size} for f in files}}
(repo/'runtime/reyond-shader-pack.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
(a.output/(archive.name+'.sha256')).write_text(manifest['sha256']+'  '+archive.name+'\n')
print(json.dumps({k:v for k,v in manifest.items() if k!='files'}))
