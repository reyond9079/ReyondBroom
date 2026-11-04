"""Package the public header, independent example, build helper and guide."""
from pathlib import Path
import argparse, hashlib, zipfile
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--version',required=True);a=p.parse_args()
src=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
archive=a.output/f'ReyondBroom-{a.version}-Plugin-SDK.zip'
names=['sdk/reyondbroom_plugin.h','sdk/LICENSE.txt','plugins/rubble/rubble.cpp','plugins/rubble/feasibility.h',
       'plugins/rubble/plugin.ini','plugins/rubble/CMakeLists.txt',
       'scripts/build-plugins.py','docs/plugins.md','docs/features113.md','docs/fixes115.md','docs/fixes116.md','docs/fixes117.md','docs/sprites118.md','docs/modular-levelgen117.md','GPL-3.0-LICENSE.txt']
names += [p.relative_to(src).as_posix() for folder in ('plugins/common','plugins/levelgen','plugins/ivy') for p in (src/folder).rglob('*') if p.is_file()]
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for name in names:z.write(src/name,'ReyondBroom-Plugin-SDK/'+name)
archive.with_suffix('.zip.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n',encoding='ascii')
print(archive)
