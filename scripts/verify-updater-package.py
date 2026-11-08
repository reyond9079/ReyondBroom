"""Independently inspect repacked payloads and corresponding source bytes."""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,re,tarfile,zipfile
p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('head');a=p.parse_args()
repo=Path(__file__).resolve().parents[1];d=json.loads((a.folder/'package-proof.json').read_text());platform=d['platform'];version='0.5.0-alpha.131'
assert d['release_revision']==a.head
def sha(f):
 with f.open('rb') as s:return hashlib.file_digest(s,'sha256').hexdigest()
files=[]
for f in sorted(a.folder.glob('*.sha256')):
 fields=f.read_text().split();assert len(fields)==2 and re.fullmatch('[a-f0-9]{64}',fields[0])
 assert Path(fields[1]).name==fields[1];payload=a.folder/fields[1];assert sha(payload)==fields[0]
 files.append({'name':payload.name,'size':payload.stat().st_size,'sha256':fields[0]})
assert len(files)==2
prefix='ReyondBroom-'+version+'-'+platform;runtime=a.folder/(prefix+('.zip' if platform=='Windows-x64' else '.tar.gz'))
root='ReyondBroom-Windows-x64/' if platform=='Windows-x64' else 'ReyondBroom-Linux-x64/'
hashes={};contents={}
def read(name,stream):
 assert name.startswith(root);relative=name[len(root):];n=PurePosixPath(relative)
 assert relative and not n.is_absolute() and '..' not in n.parts and relative not in hashes
 digest=hashlib.sha256();data=[]
 while block:=stream.read(1048576):
  digest.update(block)
  if relative in ['build-info.txt','distribution-manifest.json','ftebroom/update-ftebroom.py','ftebroom/update-ftebroom.ps1']:data.append(block)
 hashes[relative]=digest.hexdigest()
 if data:contents[relative]=b''.join(data)
if platform=='Windows-x64':
 with zipfile.ZipFile(runtime) as z:
  for f in z.infolist():
   if not f.is_dir():
    with z.open(f) as s:read(f.filename,s)
else:
 with tarfile.open(runtime,mode='r|gz') as z:
  for f in z:
   if f.isdir():continue
   assert f.isfile()
   with z.extractfile(f) as s:read(f.name,s)
manifest=json.loads(contents['distribution-manifest.json']);assert set(hashes)==set(manifest)|{'distribution-manifest.json'}
for n,digest in manifest.items():assert hashes[n]==digest,n
for n,digest in d['retained_file_sha256'].items():assert hashes[n]==digest,n
for name in ['update-ftebroom.py','update-ftebroom.ps1']:assert contents['ftebroom/'+name]==(repo/'runtime'/name).read_bytes()
info=contents['build-info.txt'].decode().replace('\r\n','\n');assert info.startswith('ReyondBroom '+version+'\n') and 'Release revision: '+a.head+'\n' in info
module='fteplug_ftebroom_x64.dll' if platform=='Windows-x64' else 'fteplug_ftebroom_amd64.so';assert 'Module SHA256: '+hashes[module]+'\n' in info
with zipfile.ZipFile(a.folder/(prefix+'-Sources.zip')) as z:
 for name in ['update-ftebroom.py','update-ftebroom.ps1']:assert z.read('sources/ftebroom/runtime/'+name)==contents['ftebroom/'+name]
result={'passed':True,'release_revision':a.head,'platform':platform,'retained_files':len(d['retained_file_sha256']),'module_sha256':hashes[module],'csaddon_sha256':hashes['ftebroom/csaddon.dat'],'payloads':files}
(a.folder/'verification.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
