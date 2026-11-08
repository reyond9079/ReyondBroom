"""Resume failed Windows QA on the exact already-built owned draft archives.

Only successful prior checks are carried. All carried bytes and product archives
are pinned; incomplete appearance cases and every unexecuted suite run afresh.
"""
from pathlib import Path,PurePosixPath
import hashlib,io,json,re,stat,subprocess,tarfile,zipfile

REPO='reyond9079/ReyondBroom';SOURCE='c77680c7f0d78118c6aa69cc9edd63bfcab40e68';VERSION='0.5.0-alpha.133'
PRIOR_RUN=42529610229;PRIOR_HEAD='3666604adb60c8ce79062682f875e10da92f83ee';PRIOR_JOB=128880690079
PRIOR_ARTIFACT=13637890203;PRIOR_DIGEST='dbd42291c1038eea29bef1c1f22f31e8565125385bba7fddd5cbc7e05540a3e0';PRIOR_SIZE=18797729
PINS={
 'ReyondBroom-0.5.0-alpha.133-Windows-x64.zip':('e13e65a4584c88222fc9e478bca3c97d4bc22c6cbeed05bd753d57c4d1fa89a6',78670567),
 'ReyondBroom-0.5.0-alpha.133-Windows-x64.zip.sha256':('17e419903014434120d240e8e3d18bedaf13b0905c64dd04246c0e8460b02491',110),
 'ReyondBroom-0.5.0-alpha.133-Windows-x64-Sources.zip':('1d94f88cb6831ae8f3927b7cf1157d681c5f3fbf16c1aefdcc3a8cdc81cf30e4',118733759),
 'ReyondBroom-0.5.0-alpha.133-Windows-x64-Sources.zip.sha256':('e37decc8632fd0a8ac4e997cb0b77b133935ce08b0a7187bf4bce6c5a03526ba',118),
}
base=Path.cwd();out=base/'out/release';out.mkdir(parents=True,exist_ok=True);qa=base/'qa';qa.mkdir(exist_ok=True)
def sha(data):return hashlib.sha256(data).hexdigest()
def file_sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def api(path):return json.loads(subprocess.check_output(['gh','api',f'repos/{REPO}/'+path]))
def download(path,target):
 command=['gh','api',f'repos/{REPO}/'+path]
 if path.startswith('releases/assets/'):command+=['-H','Accept: application/octet-stream']
 with target.open('wb') as f:subprocess.run(command,stdout=f,check=True)
def safe(name):
 p=PurePosixPath(name);assert not p.is_absolute() and '..' not in p.parts and '\\' not in name and ':' not in name and '\0' not in name;return p
assert subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()==SOURCE
release=api('releases/438503516');assert release['draft'] is True and release['tag_name']=='v'+VERSION and release['target_commitish']==SOURCE
assets={a['name']:a for a in release['assets']}
for name,(expected,size) in PINS.items():
 asset=assets[name];assert asset['state']=='uploaded' and asset['size']==size and asset['digest']=='sha256:'+expected
 target=out/name
 if not (target.exists() and target.stat().st_size==size and file_sha(target)==expected):download('releases/assets/'+str(asset['id']),target)
 assert target.stat().st_size==size and file_sha(target)==expected
 print('Verified unchanged draft archive/sidecar:',name,flush=True)
meta=api(f'actions/artifacts/{PRIOR_ARTIFACT}')
assert meta['id']==PRIOR_ARTIFACT and meta['size_in_bytes']==PRIOR_SIZE and meta['digest']=='sha256:'+PRIOR_DIGEST and not meta['expired']
assert meta['workflow_run']['id']==PRIOR_RUN and meta['workflow_run']['head_sha']==PRIOR_HEAD
job=api(f'actions/jobs/{PRIOR_JOB}');assert job['run_id']==PRIOR_RUN and job['conclusion']=='failure'
steps={s['name']:s for s in job['steps']}
required_steps=['Validate RB-10 native target formats and browser cache on Windows','Validate PK3 import and the inner audio window in the packaged editor','Validate Space in every pane and all onboarding stages before the full UI matrix']
for name in required_steps:assert steps[name]['conclusion']=='success',name
prior=base/'prior-qa.zip'
if not (prior.exists() and prior.stat().st_size==PRIOR_SIZE and file_sha(prior)==PRIOR_DIGEST):download(f'actions/artifacts/{PRIOR_ARTIFACT}/zip',prior)
assert prior.stat().st_size==PRIOR_SIZE and file_sha(prior)==PRIOR_DIGEST
runtime_name='ReyondBroom-'+VERSION+'-Windows-x64.zip';source_name='ReyondBroom-'+VERSION+'-Windows-x64-Sources.zip'
root=out/'ReyondBroom-Windows-x64'
with zipfile.ZipFile(out/runtime_name) as z:
 names=set();total=0
 for e in z.infolist():
  safe(e.filename);assert e.filename not in names and not stat.S_ISLNK(e.external_attr>>16);names.add(e.filename);total+=e.file_size;assert total<8*1024**3
 if not root.exists():z.extractall(out)
manifest=json.loads((root/'distribution-manifest.json').read_text());files={p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
assert files==set(manifest)|{'distribution-manifest.json'}
for name,digest in manifest.items():assert file_sha(root/name)==digest,name
info=(root/'build-info.txt').read_text();assert info.startswith('ReyondBroom '+VERSION+'\n') and 'Platform: Windows-x64\n' in info and 'Editor revision: '+SOURCE+'\n' in info
module=manifest['fteplug_ftebroom_x64.dll'];assert module=='4fdbd42688e99e1d40cb9ac6abbec0bf1c0c52301219a5804ade7ccf5af9228f' and 'Module SHA256: '+module+'\n' in info
expected={}
checkout=subprocess.check_output(['git','-c','core.autocrlf=false','archive',SOURCE])
with tarfile.open(fileobj=io.BytesIO(checkout)) as t:
 for e in t:
  if e.isfile():expected[e.name]=sha(t.extractfile(e).read())
with zipfile.ZipFile(out/source_name) as z:
 actual={};names=set()
 for e in z.infolist():
  safe(e.filename);assert e.filename not in names and not stat.S_ISLNK(e.external_attr>>16);names.add(e.filename)
  if not e.is_dir() and e.filename.startswith('sources/ftebroom/'):actual[e.filename[len('sources/ftebroom/'):]]=sha(z.read(e))
 assert actual==expected and len(actual)==1777,'Corresponding Sources differ from immutable product checkout'
 assert z.read('RELEASE-BUILD.txt').decode('utf-8').replace('\r\n','\n').startswith(info)
# The Sources ZIP is the exact LF Git-object archive; Windows runtime text
# assets come from the checkout with Git's CRLF conversion and attributes.
# Compare each against its actual Git-generated bytes; never normalize binaries.
windows_checkout=subprocess.check_output(['git','-c','core.autocrlf=true','archive',SOURCE])
copied_assets=0
with tarfile.open(fileobj=io.BytesIO(windows_checkout)) as t:
 for e in t:
  destination=None
  if e.name.startswith(('runtime/glsl/','runtime/plants/')):destination='ftebroom/'+e.name[len('runtime/'):]
  elif e.name.startswith('docs/'):destination=e.name
  elif e.name in ('runtime/ftebroom.cfg','runtime/update-ftebroom.py','runtime/update-ftebroom.ps1','runtime/fonts/FTEBroom-CJK.otf'):destination='ftebroom/'+e.name[len('runtime/'):]
  if destination and e.isfile():
   assert manifest.get(destination)==sha(t.extractfile(e).read()),destination
   copied_assets+=1
assert copied_assets==240
carried={};prefixes=['qa/audio-archive/','qa/engine-viewport/','qa/early-editor133-input/','qa/appearance-light-1024/','qa/rb10/units/']
with zipfile.ZipFile(prior) as z:
 inventory=json.loads(z.read('collection.json'));assert len(inventory['files'])==88
 for name,digest in inventory['files'].items():assert sha(z.read(name))==digest,name
 assert sum(len(z.read(n)) for n in inventory['files'])==inventory['bytes']
 for name in [runtime_name,source_name]:assert z.read('release-sidecars/'+name+'.sha256')==(out/(name+'.sha256')).read_bytes()
 audio=z.read('qa/audio-archive/combined.log').decode(errors='replace');assert 'DESKTOP_FAIL' not in audio
 for key in ['pk3_file_manager_double_click','pk3_sound_in_media_and_timeline','sound_is_inner_window','import_queued_while_decoding','long_pk3_and_compressed_audio_imported','import_reveals_selected_media','pk3_import_preserves_archive','inner_window_can_move','inner_window_can_resize']:assert 'DESKTOP_PASS audio4_'+key in audio
 checks=json.loads(z.read('qa/appearance-light-1024/results.json'));assert len(checks)==len(set(checks))==21
 v=json.loads(z.read('qa/appearance-light-1024/provenance.json'));assert v['build_info']==info and v['module_sha256']==module and v['theme']=='light' and v['window']==[1024,768] and v['capture_only'] is False
 metrics=json.loads(z.read('qa/appearance-light-1024/metrics.json'));assert metrics['flat_gray_3d']>10000 and metrics['white_grid_2d']>1000 and metrics['edges_grid_on']>=metrics['edges_grid_off']*.95
 early=json.loads(z.read('qa/early-editor133-input/results.json'));assert len(early)==len(set(early))==17
 assert json.loads(z.read('qa/early-editor133-input/provenance.json'))['build_info']==info
 for name in inventory['files']:
  if any(name.startswith(prefix) for prefix in prefixes):
   relative=safe(name);target=base.joinpath(*relative.parts);target.parent.mkdir(parents=True,exist_ok=True);body=z.read(name);target.write_bytes(body);carried[name]=sha(body)
 negative=qa/'continuation';negative.mkdir(exist_ok=True)
 for old,new in [('qa/appearance-light-1920/failure.png','prior-failure.png'),('qa/appearance-light-1920/failure-compilation-ui-live.json','prior-failure-state.json')]:
  (negative/new).write_bytes(z.read(old))
(qa/'continuation/prior-artifact-metadata.json').write_text(json.dumps(meta,indent=2))
(qa/'continuation/prior-job.json').write_text(json.dumps(job,indent=2))
engine='bin/fteqw64.exe' if 'bin/fteqw64.exe' in manifest else 'fteqw64.exe'
receipt={'schema':1,'kind':'Windows-x64 immutable SDK QA continuation','product_head':SOURCE,'prior_workflow_head':PRIOR_HEAD,'prior_run':PRIOR_RUN,'prior_artifact_id':PRIOR_ARTIFACT,'prior_artifact_sha256':PRIOR_DIGEST,'prior_artifact_size':PRIOR_SIZE,'prior_passed_steps':required_steps,'runtime_archive_sha256':PINS[runtime_name][0],'sources_archive_sha256':PINS[source_name][0],'module_sha256':module,'qc_sha256':manifest['ftebroom/csaddon.dat'],'engine_sha256':manifest[engine],'runtime_manifest_sha256':file_sha(root/'distribution-manifest.json'),'corresponding_source_files':len(expected),'runtime_source_assets':copied_assets,'source_bytes':'Exact LF Git archive; runtime assets match Git Windows checkout with attributes and core.autocrlf=true; binary files unchanged','carried_prefixes':prefixes,'carried_entry_hashes':carried,'remaining_suites':'Failed light1920 and unexecuted dark appearances; all four Create/Save cases; full RB-10 OS-input/PowerShell workflows. Original assertions run unchanged. Product archives are never rebuilt or uploaded by this continuation.'}
(qa/'continuation/provenance.json').write_text(json.dumps(receipt,indent=2))
print('PASS immutable SDK, complete manifest, exact corresponding Sources and',len(carried),'unchanged evidence entries from successful checks',flush=True)
