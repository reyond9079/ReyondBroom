"""Independently verify a packaged RB-10 QA artifact; extract proof files only."""
from pathlib import Path,PurePosixPath
from collections import Counter
import argparse,hashlib,json,re,stat,zipfile
p=argparse.ArgumentParser();p.add_argument('metadata',type=Path);p.add_argument('archive',type=Path);p.add_argument('output',type=Path);p.add_argument('--head',required=True);p.add_argument('--run',required=True,type=int);p.add_argument('--platform',required=True,choices=('Linux-x64','Linux-SDL2-x64','Windows-x64'));p.add_argument('--version',default='0.5.0-alpha.132');p.add_argument('--workflow-head',help='Explicit external QA revision for a byte-identical Windows continuation');a=p.parse_args()
meta=json.loads(a.metadata.read_text());digest=hashlib.file_digest(a.archive.open('rb'),'sha256').hexdigest()
assert re.fullmatch('[0-9a-f]{40}',a.head) and meta['name']=='qa-'+a.platform
workflow_head=a.workflow_head or a.head
assert re.fullmatch('[0-9a-f]{40}',workflow_head)
assert meta['workflow_run']['head_sha']==workflow_head and meta['workflow_run']['id']==a.run
if workflow_head!=a.head:assert a.platform=='Windows-x64'
assert a.archive.stat().st_size==meta['size_in_bytes'] and meta['digest']=='sha256:'+digest
assert not a.output.exists();a.output.mkdir(parents=True);proof={'platform':a.platform,'version':a.version,'head':a.head,'run':a.run,'artifact_id':meta['id'],'artifact_sha256':digest,'workflow_head':workflow_head,'cases':[]}
with zipfile.ZipFile(a.archive) as z:
 names=set();size=0
 for f in z.infolist():
  path=PurePosixPath(f.filename);assert not path.is_absolute() and '..' not in path.parts and f.filename not in names and not stat.S_ISLNK(f.external_attr>>16)
  names.add(f.filename);size+=f.file_size
 assert len(names)<50000 and size<20*1024**3
 def name(suffix):
  found=[n for n in names if n==suffix or n.endswith('/'+suffix)];assert len(found)==1,(suffix,found);return found[0]
 def data(suffix):return z.read(name(suffix))
 def obj(suffix):return json.loads(data(suffix))
 continuation=None
 if workflow_head!=a.head:
  continuation=obj('proof/continuation.json')
  expected_continuation={'schema':1,'kind':'Windows-x64-QA-continuation','product_head':a.head,'workflow_head':workflow_head,'continuation_run':a.run,'prior_artifact_id':13626282101,'prior_run':42498112722,'prior_head':'6bd6e2cedcade6e7e4d09cfe8f81901c91655a50','prior_artifact_sha256':'c30cfa600a7f9223bca901bd9b084526dc4fe686d86cc57a67053504e8441313','prior_artifact_size':258920809,'runtime_manifest_sha256':'2c5a137f56d21e82913c8afa7d29d0e0ddb1a481120bc9807188d5075c4e858d','module_sha256':'758ccd1bc16f941683692d151334704d0dd3d32180c9458a79d1f90d8aaabe42','qc_sha256':'68af71fe06750c996c257841e9b97622d4bdeb5d2e059e4a144f9da52af3e04a','engine_sha256':'28591ab7f0888e52f2987127ee5734f846612922e11dd3ec83139fbd54d73a42','original_files':653}
  assert a.head==expected_continuation['prior_head']
  for key,value in expected_continuation.items():assert continuation.get(key)==value,(key,continuation.get(key),value)
  carried=continuation['carried_entry_hashes'];assert isinstance(carried,dict) and len(carried)==279
  for path,expected_hash in carried.items():
   assert path.startswith('qa/') and re.fullmatch('[0-9a-f]{64}',expected_hash)
   assert hashlib.sha256(data(path)).hexdigest()==expected_hash,('changed prior proof',path)
  required_new={'qa/rb10/'+x for x in ('updater','runtime','browsers','welcome','quake2','engine-cleanup','compilation-paths','plant-target-ui','views-dark-1920','views-dark-1024','views-light-1920','views-light-1024')}
  assert set(continuation['new_suite_paths'])==required_new
  proof['continuation']={key:continuation[key] for key in expected_continuation}
  proof['carried_proof_files']=len(carried)
 audio=data('audio-archive/combined.log').decode('utf-8',errors='replace')
 assert not any(x in audio for x in ('DESKTOP_FAIL','CSQC_Abort','Assertion failed'))
 audio_checks=['pk3_file_manager_double_click','pk3_sound_in_media_and_timeline','sound_is_inner_window','import_queued_while_decoding','long_pk3_and_compressed_audio_imported','import_reveals_selected_media','pk3_import_preserves_archive','inner_window_can_move','inner_window_can_resize']
 for check in audio_checks:assert 'DESKTOP_PASS audio4_'+check in audio,check
 assert data('audio-archive/audio-archive-window.png').startswith(b'\x89PNG\r\n\x1a\n')
 proof['audio_archive']=len(audio_checks)
 module=None
 def source(suffix):
  global module
  v=obj(suffix);info=v['build_info'].replace('\r\n','\n');m=v['module_sha256']
  assert re.fullmatch('[0-9a-f]{64}',m),(suffix,m)
  assert info.startswith('ReyondBroom '+a.version+'\n') and 'Platform: '+a.platform+'\n' in info and 'Editor revision: '+a.head+'\n' in info and 'Module SHA256: '+m+'\n' in info,(suffix,info)
  assert module is None or module==m,(suffix,module,m);module=m;return v
 for theme in ('dark','light'):
  for width,height in ((1920,1080),(1024,768)):
   base=f'appearance-{theme}-{width}';checks=obj(base+'/results.json');assert len(checks)==len(set(checks))==21
   v=source(base+'/provenance.json');assert v['theme']==theme and v['window']==[width,height]
   met=obj(base+'/metrics.json');assert met['white_grid_2d']>1000 and met['edges_grid_on']>=met['edges_grid_off']*.95 and met['flat_gray_3d']>10000
   base=f'viewport-create-{theme}-{width}';create=obj(base+'/results.json');assert len(create)==len(set(create))==40
   base=f'rb10/views-{theme}-{width}';checks=obj(base+'/results.json');assert len(checks)>=(48 if width==1920 else 47)
   # open_options runs before and after restarting the editor, so these two
   # real assertions deliberately appear twice. Other checks are unique.
   repeated={k:v for k,v in Counter(checks).items() if v>1}
   assert repeated=={'3D viewport is available':2,'View Options is at top-right of viewport 0':2},(base,repeated)
   assert any('without truncation' in x for x in checks) and any('Show all' in x for x in checks)
   v=source(base+'/provenance.json');assert v['theme']==theme and v['window']==[width,height]
   met=obj(base+'/metrics.json');assert met['tools_expanded_height']>met['tools_collapsed_height'] and met['hidden_classes_bytes']>256
   for image in ('view-options-defaults.png','view-options-final.png','tools-expanded.png'):
    blob=data(base+'/'+image);assert blob.startswith(b'\x89PNG\r\n\x1a\n')
   proof['cases'].append({'theme':theme,'window':[width,height],'appearance':21,'create':40,'rb10_views':len(checks),'metrics':met})
 runtime=obj('rb10/runtime/results.json');assert len(runtime)==4
 expected={'tutorial132':7,'clipboard-attributes132':12,'view-options132':29,'native132':12}
 assert {case['suite'] for case in runtime}==set(expected)
 qc=set()
 for case in runtime:
  assert len(case['checks'])>=expected[case['suite']] and len(case['checks'])==len(set(case['checks']))
  assert re.fullmatch('[0-9a-f]{64}',case['qc_sha256']);qc.add(case['qc_sha256'])
 assert len(qc)==1;proof['qc_sha256']=qc.pop()
 for label,minimum in [('browsers',11),('welcome',20),('plant-target-ui',10)]:
  checks=obj('rb10/'+label+'/results.json');assert len(checks)>=minimum,(label,len(checks))
  repeated={k:v for k,v in Counter(checks).items() if v>1}
  assert repeated==({'only the Restart button relaunches the editor':2} if label=='welcome' else {}),(label,repeated)
  proof[label]=len(checks)
  if label=='plant-target-ui':assert 'Quake1 scene opt-in preserves exact immutable target assets' in checks
 source('rb10/welcome/provenance.json')
 updater=obj('rb10/updater/results.json');assert updater['platform']==a.platform
 expected_updates={'check-only','bad-session','prepare-close','foreign-restart','explicit-restart'}
 assert len(updater['cases'])==5 and {case['case'] for case in updater['cases']}==expected_updates and all(case['passed'] is True for case in updater['cases'])
 proof['updater_cases']=len(updater['cases'])
 targets=obj('rb10/units/plant-targets/results.json');mips=targets['coverageMips']
 assert targets['targetCases']==242 and len(mips)==9 and targets['explicitFteQ1'] is True and targets['vanillaCompileGate'] is True
 assert [(m['width'],m['height']) for m in mips]==[(256>>i,256>>i) for i in range(9)]
 rows=targets['targets'];assert len(rows)==242 and len({(r['preset'],r['profile']) for r in rows})==242
 presets={r['preset'] for r in rows};assert len(presets)==121
 for profile,extension,nv,nt in ((2,'.mdl',1024,2048),(3,'.md2',2048,4096)):
  selected=[r for r in rows if r['profile']==profile];assert len(selected)==121 and {r['preset'] for r in selected}==presets
  assert all(r['format']==extension and r['decoded'] is True and 0<r['vertices']<=nv and 0<r['triangles']<=nt for r in selected)
 proof['target_cases']=targets['targetCases'];proof['coverage_mips']=len(mips);proof['module_sha256']=module
 if continuation is not None:assert module==continuation['module_sha256'] and proof['qc_sha256']==continuation['qc_sha256']
 assert 'PASS: safe Q2 PAK browsing/resource reads, strict writes, traversal rejection' in data('rb10/units/q2-pak.log').decode()
 # quake2-roots.cpp is assert-only and intentionally prints nothing on success.
 data('rb10/units/q2-roots.log')
 q2=obj('rb10/quake2/results.json')
 required_q2={
  'resource_dialog_profile_change_uses_save_guard',
  'dirty_profile_change_cancel_preserves_active_profile',
  'selected_q2_resources_applies_profile_without_switch',
  'custom_q2_directory_accepts_arbitrary_name',
  'stale_resource_error_clears_after_path_edit',
  'missing_legacy_file_restores_saved_q2_path',
  'existing_empty_legacy_file_preserves_explicit_empty_q2_path',
  'q2_project_command_updates_active_resource_directory',
  'q2_original_base_and_custom_paks_mount',
  'q2_rerelease_baseq2_is_accepted',
  'preferences_selected_profile_waits_for_unsaved_guard',
  'preferences_resources_cancel_keeps_original_game',
  'invalid_q2_directory_reports_q2_error',
  'q2_selection_preserves_quake1_quake3_goldsrc_paths',
  'q2_project_command_preserves_other_game_resource_paths',
  'all_game_pak_resources_remain_byte_identical',
  'source_runtime_remains_byte_identical',
 }
 assert isinstance(q2,list) and len(q2)>=17 and len(q2)==len(set(q2)) and required_q2<=set(q2),q2
 q2_source=source('rb10/quake2/provenance.json');assert q2_source['qc_sha256']==proof['qc_sha256'] and q2_source['checks']==len(q2)
 proof['quake2']=len(q2)
 workspace_log=data('rb10/units/workspace-selection.log').decode()
 assert 'PASS actual workspace file restart preserves selected profile/custom workdir, duplicate names and per-game selections; legacy/invalid/empty selections remain compatible' in workspace_log
 cleanup=obj('rb10/engine-cleanup/results.json')
 required_cleanup={
  'external_map_path_keeps_user_filename',
  'customer_profile_copy_cleanup_deploys_bsp_lit',
  'copy_cleanup_compile_launch_starts_process_probe',
  'compile_launch_uses_engine_executable_directory',
  'compile_launch_uses_real_map_and_active_mod',
  'standalone_launch_starts_process_probe',
  'standalone_launch_uses_engine_executable_directory',
  'standalone_launch_uses_real_map_and_active_mod',
  'deployed_lighting_survives_launch_without_build_output',
  'existing_invalid_build_does_not_launch_stale_deployed_map',
  'missing_build_and_deployed_map_prevent_launch',
  'source_runtime_remains_byte_identical',
 }
 assert isinstance(cleanup,list) and len(cleanup)>=12 and len(cleanup)==len(set(cleanup)) and required_cleanup<=set(cleanup),cleanup
 cleanup_source=source('rb10/engine-cleanup/provenance.json')
 assert cleanup_source['qc_sha256']==proof['qc_sha256'] and cleanup_source['checks']==len(cleanup)
 assert re.fullmatch('[0-9a-f]{64}',cleanup_source['engine_sha256']) and re.fullmatch('[0-9a-f]{64}',cleanup_source['probe_sha256'])
 for image in ('copy-cleanup-launch.png','invalid-source-rejected.png'):assert data('rb10/engine-cleanup/'+image).startswith(b'\x89PNG\r\n\x1a\n')
 proof['engine_cleanup']=len(cleanup);proof['engine_sha256']=cleanup_source['engine_sha256'];proof['probe_sha256']=cleanup_source['probe_sha256']
 paths=obj('rb10/compilation-paths/results.json')
 path_cases={
  'internal control',
  'external Quake1 profile and per-map WAD mount',
  'external Quake3 map',
  'resource restart',
  'Save As to another directory and name',
  'reload of the same staged alias',
  'console mounted loose source',
  'console mounted PAK source',
  'console mounted PK3 source',
  'explicit absolute working directory',
  'saved map with intentionally empty working directory',
 }
 required_paths={case+': '+suffix for case in path_cases for suffix in (
  'original document path is retained',
  'MAP variables describe the real source',
  'selected profile controls actual process working directory',
 )}|{'unsaved map cannot export or spawn a strict compilation profile','unsaved map cannot launch an engine process'}
 assert isinstance(paths,list) and len(paths)>=35 and len(paths)==len(set(paths)) and required_paths<=set(paths),paths
 cases=obj('rb10/compilation-paths/cases.json');assert len(cases)==11 and {case['case'] for case in cases}==path_cases
 for case in cases:
  fields=case['variables'];document=PurePosixPath(case['document'].replace('\\','/'))
  assert {'MAP_DIR_PATH','MAP_BASE_NAME','MAP_FULL_NAME','WORK_DIR_PATH','APP_DIR_PATH'}<=set(fields)
  assert fields['MAP_FULL_NAME']==document.name and fields['MAP_BASE_NAME']==document.stem and case['actualProcessCwd']
  if case['case']=='saved map with intentionally empty working directory':assert case['workExpression']==fields['WORK_DIR_PATH']==''
 paths_source=source('rb10/compilation-paths/provenance.json')
 assert paths_source['platform']==a.platform and paths_source['qc_sha256']==proof['qc_sha256'] and paths_source['probe_sha256']==proof['probe_sha256']
 proof['compilation_paths']=len(paths);proof['compilation_path_cases']=len(cases)
 if a.platform=='Linux-x64':
  cat=obj('catalog-originals/capture-manifest.json');assert cat['presets']==121 and len(cat['originals'])>=121 and len(cat['originals'])==len(set(cat['originals']))
  source('catalog-originals/hardware.json');assert all(any(n.endswith('/catalog-originals/'+x) or n=='catalog-originals/'+x for n in names) for x in cat['originals'])
  assert all(preset+'.png' in cat['originals'] for preset in presets)
  for preset in presets:assert data('catalog-originals/'+preset+'.png').startswith(b'\x89PNG\r\n\x1a\n')
  proof['catalogue']={'presets':121,'images':len(cat['originals'])}
 # Only extract readable evidence; cloned runtime executables are unnecessary.
 selected=[n for n in names if '/rb10/' in '/'+n or '/appearance-' in '/'+n or '/viewport-create-' in '/'+n or '/catalog-sheets/' in '/'+n or n.endswith('/catalog-originals/oak.png')]
 for n in selected:
  if PurePosixPath(n).suffix not in ('.json','.png','.log','.map'):continue
  destination=a.output/n;destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(z.read(n))
(a.output/'verification.json').write_text(json.dumps(proof,indent=2));print(json.dumps(proof,indent=2))
