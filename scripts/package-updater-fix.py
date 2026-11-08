"""Update transport scripts while retaining verified alpha130 native binaries.

Runtime and complete corresponding source archives must already match their
public SHA256 files. This script also validates the runtime's full file manifest.
"""
from pathlib import Path,PurePosixPath
import argparse,hashlib,io,json,shutil,stat,subprocess,tarfile,zipfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--platform',choices=['Windows-x64','Linux-x64','Linux-SDL2-x64'],required=True)
p.add_argument('--base-runtime',type=Path,required=True);p.add_argument('--base-sources',type=Path,required=True)
p.add_argument('--version',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
src=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
windows=a.platform=='Windows-x64';root=out/('ReyondBroom-Windows-x64' if windows else 'ReyondBroom-Linux-x64')
def sha(f):
 with f.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def extract_zip(path,target):
 with zipfile.ZipFile(path) as z:
  seen=set()
  for f in z.infolist():
   n=PurePosixPath(f.filename);assert not n.is_absolute() and '..' not in n.parts and f.filename not in seen and not stat.S_ISLNK(f.external_attr>>16)
   seen.add(f.filename)
  z.extractall(target)
if windows:extract_zip(a.base_runtime,out)
else:
 with tarfile.open(a.base_runtime) as z:z.extractall(out,filter='data')
manifest=json.loads((root/'distribution-manifest.json').read_text())
assert {f.relative_to(root).as_posix() for f in root.rglob('*') if f.is_file()}==set(manifest)|{'distribution-manifest.json'}
for name,digest in manifest.items():assert sha(root/name)==digest,name
old_info=(root/'build-info.txt').read_text();assert old_info.startswith('ReyondBroom 0.5.0-alpha.130\n')
retained={name:digest for name,digest in manifest.items() if name not in ['build-info.txt','SOURCE-CODE.txt','ftebroom/update-ftebroom.py','ftebroom/update-ftebroom.ps1']}
for name in ['update-ftebroom.py','update-ftebroom.ps1']:shutil.copy2(src/'runtime'/name,root/'ftebroom'/name)
shutil.copy2(src/'docs/updater-downloads.md',root/'docs/updater-downloads.md')
revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=src,text=True).strip()
info=old_info.replace('ReyondBroom 0.5.0-alpha.130\n','ReyondBroom '+a.version+'\n',1)
info+='Release revision: '+revision+'\nNative module, CSQC, engine and tools retained byte-for-byte from verified alpha130.\n'
for name in ['update-ftebroom.py','update-ftebroom.ps1']:info+=name+' SHA256: '+sha(root/'ftebroom'/name)+'\n'
(root/'build-info.txt').write_text(info)
source_name='ReyondBroom-'+a.version+'-'+a.platform+'-Sources.zip'
(root/'SOURCE-CODE.txt').write_text('Complete corresponding sources: '+source_name+', in the same GitHub release.\nhttps://github.com/reyond9079/ReyondBroom/releases\nPreserve all licenses.\n')
sources=out/'corresponding-sources';extract_zip(a.base_sources,sources)
shutil.rmtree(sources/'sources/ftebroom');(sources/'sources/ftebroom').mkdir()
checkout=subprocess.check_output(['git','archive','HEAD'],cwd=src)
with tarfile.open(fileobj=io.BytesIO(checkout)) as z:z.extractall(sources/'sources/ftebroom',filter='data')
for name in ['update-ftebroom.py','update-ftebroom.ps1']:
 assert (sources/'sources/ftebroom/runtime'/name).read_bytes()==(root/'ftebroom'/name).read_bytes()
(sources/'RELEASE-BUILD.txt').write_text(info+'Build/QA: sources/ftebroom/.github/workflows/updater-transport-checks.yml\n')
with zipfile.ZipFile(out/source_name,'w',zipfile.ZIP_DEFLATED) as z:
 for f in sorted(sources.rglob('*')):
  if f.is_file():z.write(f,f.relative_to(sources))
for name,digest in retained.items():assert sha(root/name)==digest,name
new_manifest={f.relative_to(root).as_posix():sha(f) for f in sorted(root.rglob('*')) if f.is_file() and f.name!='distribution-manifest.json'}
(root/'distribution-manifest.json').write_text(json.dumps(new_manifest,indent=2))
runtime_name='ReyondBroom-'+a.version+'-'+a.platform+('.zip' if windows else '.tar.gz')
if windows:
 with zipfile.ZipFile(out/runtime_name,'w',zipfile.ZIP_DEFLATED) as z:
  for f in sorted(root.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(out))
else:
 with tarfile.open(out/runtime_name,'w:gz') as z:z.add(root,arcname=root.name)
for name in [runtime_name,source_name]:(out/(name+'.sha256')).write_text(sha(out/name)+'  '+name+'\n')
(out/'package-proof.json').write_text(json.dumps({'platform':a.platform,'release_revision':revision,'retained_file_count':len(retained),'retained_file_sha256':retained,'new_manifest':new_manifest,'build_info':info},indent=2))
print(info)
