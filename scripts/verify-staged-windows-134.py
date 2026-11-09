#!/usr/bin/env python3
"""Authenticate immutable 5d runtime bytes and canonical corrected QA scripts."""
from pathlib import Path
import hashlib,json,subprocess,zipfile
base=Path('baseline');qa=Path('qa/provenance');qa.mkdir(parents=True,exist_ok=True)
app='5d554d77b9a274133235d785a63ce9540d056844';test='019d9ca1c27c11ec89882141fccfbba0cdff0f80'
def sha(data):return hashlib.sha256(data).hexdigest()
archives={'ReyondBroom-0.5.0-alpha.134-Windows-x64.zip':(78724975,'a528a41615ad544efcec756d001da8d91d2430b34353b6029994724bb6d573ff'),'ReyondBroom-0.5.0-alpha.134-Windows-x64-Sources.zip':(126684235,'07dbe087a96319b92d6402e79800bdf0f30ca8db7ac6df3f35a7faefc13c83d5')}
for name,(size,digest) in archives.items():
    path=base/name;assert path.stat().st_size==size and sha(path.read_bytes())==digest,name
with zipfile.ZipFile(base/'ReyondBroom-0.5.0-alpha.134-Windows-x64.zip') as archive:archive.extractall(base)
root=base/'ReyondBroom-Windows-x64';manifest=json.loads((root/'distribution-manifest.json').read_text())
assert {f.relative_to(root).as_posix() for f in root.rglob('*') if f.is_file()}==set(manifest)|{'distribution-manifest.json'}
for name,digest in manifest.items():assert sha((root/name).read_bytes())==digest,name
info=(root/'build-info.txt').read_text();assert 'ReyondBroom 0.5.0-alpha.134' in info and app in info
entries={}
for line in subprocess.check_output(['git','ls-tree','-rz',app]).split(b'\0'):
    if not line:continue
    details,name=line.split(b'\t',1);mode,kind,oid=details.split()
    if kind==b'blob':entries[name.decode()]=oid.decode()
with zipfile.ZipFile(base/'ReyondBroom-0.5.0-alpha.134-Windows-x64-Sources.zip') as archive:
    prefix='sources/ftebroom/'
    actual={x[len(prefix):] for x in archive.namelist() if x.startswith(prefix) and not x.endswith('/')}
    assert actual==set(entries)
    for name,oid in entries.items():
        data=archive.read(prefix+name)
        assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==oid,name
    assert app in archive.read('RELEASE-BUILD.txt').decode()
diff=subprocess.check_output(['git','diff','--name-only',app,test],text=True).splitlines()
assert diff==['scripts/test-mod-profile-lighting.py'],diff
scripts={}
for name in ['scripts/test-mod-profile-lighting.py','scripts/editor_os_input.py','scripts/test-viewport-create.py','scripts/test-viewport-appearance.py','scripts/collect-release-qa.py','tests/make-linux-build-fixture.py']:
    expected=subprocess.check_output(['git','show',test+':'+name]);actual=Path(name).read_bytes();assert actual==expected,name
    scripts[name]=sha(actual)
(qa/'qa-source-diff.patch').write_bytes(subprocess.check_output(['git','diff',app,test,'--','scripts/test-mod-profile-lighting.py']))
(qa/'test-mod-profile-lighting.py').write_bytes(Path('scripts/test-mod-profile-lighting.py').read_bytes())
engine=root/'bin/fteqw64.exe'
proof={'runtime_revision':app,'canonical_qa_revision':test,'workflow_revision':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'archives':archives,'complete_manifest_verified':True,'complete_git_source_files_verified':len(entries),'source_difference_only':diff,'script_sha256':scripts,'engine_sha256':sha(engine.read_bytes()),'module_sha256':sha((root/'fteplug_ftebroom_x64.dll').read_bytes()),'qc_sha256':sha((root/'ftebroom/csaddon.dat').read_bytes()),'build_info':info,'method':'Exact original release ZIP/source ZIP digests, complete runtime manifest, every corresponding Git source blob from actual 5d build, and exact corrected canonical QA bytes. No runtime binary rebuild or modification.'}
(qa/'staged-runtime-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
print('PASS staged5d complete runtime/source verification and corrected019 QA bytes',len(entries))
