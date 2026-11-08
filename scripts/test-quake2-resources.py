#!/usr/bin/env python3
"""Quake II resource/profile acceptance through real OS input in private copies.

The launch config seeds the initial dirty document and enables read-only QA
telemetry. Tested profile changes, guards and resource edits use OS input;
the public project command is sent through the actual engine console.
"""
import argparse, hashlib, json, os, shlex, shutil, struct, subprocess, time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput, read_ui_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path)
a=p.parse_args();original=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
assert original!=out and original not in out.parents,'QA must use separate runtime copies'
windows=os.name=='nt';engine_name='bin/fteqw64.exe' if windows and (original/'bin/fteqw64.exe').exists() else 'fteqw64.exe' if windows else 'fteqw-gl64'
module_name='fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'
checks=[];process=None;stream=None;caseout=None

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
original_hashes={name:sha(original/name) for name in [module_name,'ftebroom/csaddon.dat','build-info.txt']}
resources=out/'game-resources';q3=resources/'Quake III source';q1=resources/'Quake1 custom';hl=resources/'GoldSrc custom'
for path in [q3/'textures',q1,hl]:path.mkdir(parents=True,exist_ok=True)
base=resources/'Quake II/baseq2';mod=base.parent/'Arbitrary Custom Name';other=base.parent/'Another custom mod';rerelease=base.parent/'rerelease/baseq2'
wal=(original/'ftebroom/textures/fb_grey.wal').read_bytes()
pcx=bytearray(128);pcx[:4]=bytes([10,5,1,8]);struct.pack_into('<4H',pcx,4,0,0,15,15);pcx[65]=1;struct.pack_into('<H',pcx,66,16)
pcx=bytes(pcx)+bytes([160]*256)+bytes([12])+b''.join(bytes([i,i,i]) for i in range(256))
def pak(path,entries):
 path.parent.mkdir(parents=True,exist_ok=True);payload=bytearray();directory=bytearray()
 for name,data in entries.items():
  directory.extend(name.encode().ljust(56,b'\0')+struct.pack('<II',12+len(payload),len(data)));payload.extend(data)
 path.write_bytes(b'PACK'+struct.pack('<II',12+len(payload),len(directory))+payload+directory)
for folder,marker in [(base,b'base resources'),(mod,b'custom overrides'),(other,b'other custom'),(rerelease,b'remaster resources')]:
 pak(folder/'pak0.pak',{'textures/fb_grey.wal':wal,'pics/colormap.pcx':pcx,'q2-source.txt':marker,'models/tank/../ctank/skin.pcx':b'official-style alias'})
resource_hashes={str(path):sha(path) for path in resources.rglob('*') if path.is_file()}

def check(ok,name):
 assert ok,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
def live():
 value=read_ui_json(state)
 if compilation.exists():value['globalWidgets']=read_ui_json(compilation).get('globalWidgets',{})
 # Frame-scoped visible controls are authoritative. AnyPopupId deliberately
 # ignores its supplied name and therefore cannot identify an individual modal.
 value['resourcesOpen']='resources/window' in value.get('widgets',{})
 value['saveChangesOpen']='save/cancel' in value.get('widgets',{})
 return value
def wait(fn,timeout=60):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  assert process.poll() is None,'Editor exited; inspect '+str(caseout/'editor.log')
  try:
   if value:=fn():return value
  except (OSError,KeyError,ValueError):pass
  time.sleep(.06)
 raise AssertionError('Q2 UI timeout: '+str(live())[:2400])
def start(label,profile=0,saved=None,legacy='custom',dirty=False):
 global process,stream,ui,root,data,state,compilation,caseout
 caseout=out/label;caseout.mkdir(exist_ok=True);root=caseout/'editor-runtime'
 assert not root.exists(),'Use fresh QA output: '+str(root)
 shutil.copytree(original,root,ignore=shutil.ignore_patterns('updates','downloads','__pycache__'))
 data=root/'ftebroom';state=data/'welcome-ui-live.json';compilation=data/'compilation-ui-live.json'
 for path in [state,compilation,data/'qconsole.log',root/'qconsole.log',data/'native-input-live.json']:path.unlink(missing_ok=True)
 for settings in [root,data]:
  (settings/'ftebroom-game-profile.txt').write_text(f'{profile} 0 "" "" "" "" 1 5 "" 1 1\n')
  (settings/'ftebroom-project.txt').write_text(str(saved if saved is not None else q3))
  (settings/'ftebroom-quake1.txt').write_text('"" '+json.dumps(str(q1))+' "" "" 1 0 '+json.dumps(str(q3))+' 0\n')
  path=settings/'reyondbroom-legacy-games.txt'
  if legacy=='missing':path.unlink(missing_ok=True)
  else:path.write_text(json.dumps(str(mod) if legacy=='custom' else '')+' '+json.dumps(str(hl))+' "" "" "" 0\n')
 cfg='exec ftebroom.cfg\nset ftebroom_input_qa 1\nset ftebroom_welcome_qa 1\nin 10 ftebroom_test_desktop reyon8qa dark\n'
 if dirty:cfg+='in 11 ftebroom_test_generators layout83\n'
 (data/'q2-resources-qa.cfg').write_text(cfg)
 renderer=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
 stream=(caseout/'editor.log').open('wb')
 process=subprocess.Popen([str(root/engine_name),*renderer,'+set','ftebroom_skip_setup','1','+set','ftebroom_input_qa','1','+set','ftebroom_welcome_qa','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width','1280','-height','800','-basedir',str(root),'+exec','q2-resources-qa.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
 wait(lambda:live().get('gameProfile')==profile and compilation.exists() and live()['globalWidgets'].get('menu/View'))
 if dirty:wait(lambda:live()['dirty'])
 ui=NativeInput(process,state,live,wait,caseout,focus_required=True);ui.position()
 if not windows:ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 ui.move(1100,740);ui.settle()
def stop():
 global process,stream
 if process is not None and process.poll() is None:process.kill();process.wait(timeout=15)
 if stream:stream.close()
def click(key,global_=False):
 ui.settle();r=wait(lambda:live()['globalWidgets' if global_ else 'widgets'].get(key))
 with (caseout/'os-clicks.jsonl').open('a') as log:log.write(json.dumps({'key':key,'rect':r,'time':time.monotonic()})+'\n')
 ui.click(r['x']+r['width']*.45,r['y']+r['height']*.5);ui.settle()
def choose(key,index,global_=False):
 ui.settle();r=wait(lambda:live()['globalWidgets' if global_ else 'widgets'].get(key))
 ui.click(r['x']+r['width']*.3,r['y']+r['height']*.5);ui.settle()
 # The fixture selects UI scale 0. Standard ImGui combo rows use the 16px
 # font plus 6px spacing; FrameHeight includes 4px padding above and below.
 # Use OS mouse input: keyboard navigation is not enabled for these dialogs.
 with (caseout/'os-clicks.jsonl').open('a') as log:log.write(json.dumps({'combo':key,'index':index,'rect':r,'time':time.monotonic()})+'\n')
 ui.click(r['x']+r['width']*.3,r['y']+r['height']+16+index*22);ui.settle()
def preferences(profile):
 click('menu/View',True);click('menu-preferences',True);click('preferences-profile-tab',True)
 if profile!=live()['gameProfile']:choose('preferences-profile-combo',profile,True)
def open_resources(profile):preferences(profile);click('preferences-profile-resources',True)
def edit_folder(path):
 click('resources/folder');ui.key('a',('ctrl',));ui.key('backspace');ui.text(str(path));ui.key('enter');ui.settle()
def shot(name):
 ui.move(1268,782);ui.settle();ImageGrab.grab().save(caseout/(name+'.png'));(caseout/(name+'.json')).write_text(json.dumps(live(),indent=2))
def saved_profile():return int((root/'ftebroom-game-profile.txt').read_text().split()[0])
def logtext():
 # Windows GUI builds write console output to qconsole.log instead of stdout.
 chunks=[]
 for path in [caseout/'editor.log',root/'qconsole.log',data/'qconsole.log']:
  try:chunks.append(path.read_text(errors='replace'))
  except FileNotFoundError:pass
 return '\n'.join(chunks)
def preserved_other_profiles(expected_hl=hl):
 q=shlex.split((root/'ftebroom-quake1.txt').read_text());legacy=shlex.split((root/'reyondbroom-legacy-games.txt').read_text())
 return Path(q[1])==q1 and Path(q[6])==q3 and legacy[1]==str(expected_hl)

try:
 start('profile-selection',dirty=True)
 before=live();before_profile=(root/'ftebroom-game-profile.txt').read_bytes()
 open_resources(0);wait(lambda:live()['resourcesOpen'])
 choose('resources/profile',3);wait(lambda:live()['saveChangesOpen'])
 check(live()['gameProfile']==0 and live()['pendingDocument']=='__profile:3:resources','resource_dialog_profile_change_uses_save_guard')
 click('save/cancel');wait(lambda:not live()['saveChangesOpen'] and not live()['pendingDocument'])
 check(live()['gameProfile']==0 and live()['dirty'] and live()['document']==before['document'] and (root/'ftebroom-game-profile.txt').read_bytes()==before_profile,'dirty_profile_change_cancel_preserves_active_profile')
 open_resources(3);wait(lambda:live()['saveChangesOpen'])
 check(live()['gameProfile']==0 and not live()['resourcesOpen'],'preferences_selected_profile_waits_for_unsaved_guard')
 click('save/cancel');wait(lambda:not live()['saveChangesOpen'] and not live()['pendingDocument'])
 check(live()['gameProfile']==0 and live()['dirty'],'preferences_resources_cancel_keeps_original_game')
 open_resources(3);wait(lambda:live()['saveChangesOpen']);click('save/discard')
 wait(lambda:live()['gameProfile']==3 and live()['resourcesOpen'] and not live()['resourcesLoading'])
 check(saved_profile()==3,'selected_q2_resources_applies_profile_without_switch')
 check(Path(live()['q2Mod'])==mod and Path(live()['resourceFolder'])==mod and not live()['resourceError'],'custom_q2_directory_accepts_arbitrary_name');shot('custom-q2-resources')
 edit_folder(resources/'does not exist');click('resources/load');wait(lambda:bool(live()['resourceError']))
 check('baseq3' not in live()['resourceError'].lower() and 'Quake II' in live()['resourceError'],'invalid_q2_directory_reports_q2_error')
 edit_folder(mod);wait(lambda:live()['resourceError']=='')
 check(True,'stale_resource_error_clears_after_path_edit')
 click('resources/load');wait(lambda:not live()['resourcesOpen'] and not live()['resourcesLoading'])
 ui.console('which q2-source.txt')
 expected_archive='inside '+str(mod/'pak0.pak').replace('\\','/').lower()
 wait(lambda:expected_archive in logtext().replace('\\','/').lower())
 check('fallback resources' in logtext(),'q2_original_base_and_custom_paks_mount')
 check(preserved_other_profiles(),'q2_selection_preserves_quake1_quake3_goldsrc_paths');shot('custom-q2-accepted');stop()

 start('missing-legacy',profile=3,saved=mod,legacy='missing')
 wait(lambda:not live()['resourcesLoading'] and live()['resourceError']=='')
 check(Path(live()['q2Mod'])==mod and Path(live()['resourceFolder'])==mod,'missing_legacy_file_restores_saved_q2_path')
 before_hl=shlex.split((root/'reyondbroom-legacy-games.txt').read_text())[1]
 ui.console('ftebroom_project '+json.dumps(str(other)));wait(lambda:Path(live()['q2Mod'])==other and not live()['resourcesLoading'])
 check(Path(live()['resourceFolder'])==other and live()['gameProfile']==3 and not live()['resourceError'],'q2_project_command_updates_active_resource_directory')
 check(preserved_other_profiles(before_hl),'q2_project_command_preserves_other_game_resource_paths');shot('project-command');stop()

 start('explicit-empty-legacy',profile=3,saved=q3,legacy='empty')
 wait(lambda:bool(live()['resourceError']))
 check(live()['q2Mod']=='' and live()['gameProfile']==3 and 'baseq3' not in live()['resourceError'].lower(),'existing_empty_legacy_file_preserves_explicit_empty_q2_path');stop()

 start('rerelease',profile=3,saved=rerelease,legacy='missing')
 wait(lambda:not live()['resourcesLoading'] and live()['resourceError']=='')
 check(Path(live()['q2Mod'])==rerelease and Path(live()['resourceFolder'])==rerelease,'q2_rerelease_baseq2_is_accepted');shot('rerelease');stop()
 check(all(sha(Path(name))==digest for name,digest in resource_hashes.items()),'all_game_pak_resources_remain_byte_identical')
 check(all(sha(original/name)==digest for name,digest in original_hashes.items()),'source_runtime_remains_byte_identical')
except BaseException:
 if caseout:
  try:ImageGrab.grab().save(caseout/'failure.png');(caseout/'failure.json').write_text(json.dumps(live(),indent=2))
  except Exception:pass
 raise
finally:
 stop()
 (out/'provenance.json').write_text(json.dumps({'module_sha256':original_hashes[module_name],'qc_sha256':original_hashes['ftebroom/csaddon.dat'],'build_info':(original/'build-info.txt').read_text(),'source_runtime':str(original),'checks':len(checks),'method':'Actual OS mouse/keyboard selection, Save guard Cancel/Discard and path edits; read-only QA snapshots; generated native PACK/WAL/PCX resources; public project command via real console; isolated runtime copies'},indent=2))
