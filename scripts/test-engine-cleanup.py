#!/usr/bin/env python3
"""Exercise customer copy/cleanup and engine launch through real OS input.

The Quake executables are process/argv probes, not VKQuake or Ironwail. A
synthetic BSP header and generated palette require no copyrighted game data.
All editor state and external game files live in a disposable runtime copy.
"""
import argparse, hashlib, json, os, shutil, subprocess, time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput, read_ui_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path)
p.add_argument('--probe',type=Path,help='Built compilation-profile-probe executable')
a=p.parse_args();original=a.runtime.resolve();out=a.output.resolve()
assert original!=out and original not in out.parents,'Use a separate QA output directory'
out.mkdir(parents=True,exist_ok=True)
windows=os.name=='nt'
module_name='fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'
engine_name='bin/fteqw64.exe' if windows and (original/'bin/fteqw64.exe').exists() else 'fteqw64.exe' if windows else 'fteqw-gl64'
probe=(a.probe.resolve() if a.probe else original/('tools/compilation-profile-probe.exe' if windows else 'tools/compilation-profile-probe'))
assert probe.is_file(),'Pass --probe with the built compilation-profile-probe executable'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inventory(root):return {p.relative_to(root).as_posix():sha(p) for p in root.rglob('*') if p.is_file()}
original_hashes=inventory(original)
root=out/'editor-runtime'
assert not root.exists(),'Use fresh QA output: '+str(root)
shutil.copytree(original,root,ignore=shutil.ignore_patterns('updates','downloads','__pycache__'))
data=root/'ftebroom';state=data/'compilation-ui-live.json'
game=out/'External Game';mod=game/'CustomMod';mapdir=out/'External Source Maps';enginedir=out/'Engine Bin'
mapdir.mkdir();enginedir.mkdir();(game/'id1/gfx').mkdir(parents=True)
(mod/'maps').mkdir(parents=True);(mod/'textures').mkdir()
(game/'id1/gfx/palette.lmp').write_bytes(bytes(range(256))*3)
executable=enginedir/('engine-argv-probe.exe' if windows else 'engine-argv-probe')
shutil.copy2(probe,executable)
mapfile=mapdir/'customer-real-map.map'
scene=(data/'maps/start-quake1.map').read_text()
scene=scene.replace('"classname" "worldspawn"','"classname" "worldspawn"\n"_tb_mod" "CustomMod"',1)
mapfile.write_text(scene)
bsp=mapdir/'customer-real-map.bsp';lit=mapdir/'customer-real-map.lit'
target=mod/'maps/customer-real-map.bsp';targetlit=mod/'maps/customer-real-map.lit'
valid=bytes([29])+bytes(123);lighting=b'customer lighting'
bsp.write_bytes(valid);lit.write_bytes(lighting)
markers=[out/'vkquake-argv-probe.json',out/'ironwail-argv-probe.json'];engines=[]
for i,name in enumerate(['VKQuake argv probe','Ironwail argv probe']):
 engines.append({'id':str(i),'name':name,'path':executable.as_posix(),
  'parameters':'record "'+markers[i].as_posix()+'" -basedir "${GAME_DIR_PATH}" -game "${MODS[-1]}" +map "${MAP_BASE_NAME}"',
  'workdir':'${ENGINE_DIR_PATH}','mapFile':'${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp'})
profile={'name':'Customer copy-delete-launch','workdir':'${MAP_DIR_PATH}','trenchBroom':True,'tasks':[
 {'type':'copy','source':'${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp','target':'${GAME_DIR_PATH}/${MODS[-1]}/maps'},
 {'type':'copy','source':'${WORK_DIR_PATH}/${MAP_BASE_NAME}.lit','target':'${GAME_DIR_PATH}/${MODS[-1]}/maps'},
 {'type':'delete','target':'${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp'},
 {'type':'delete','target':'${WORK_DIR_PATH}/${MAP_BASE_NAME}.lit'},
 {'type':'launchEngine','engineProfileId':'0','treatLaunchFailureAsError':True}]}
workspace={'profiles':[profile],'engines':engines,'variables':{},'gameRoot':game.as_posix(),'baseMod':'id1','enabledMods':['CustomMod'],'modsConfigured':True}
(data/'compilation-profiles.json').write_text(json.dumps({'version':1,'games':{'quake1':workspace}}))
for parent in [root,data]:
 (parent/'ftebroom-game-profile.txt').write_text('2 0 "" "" "" "" 1 5 "" 1 1\n')
 (parent/'ftebroom-project.txt').write_text(mod.as_posix())
 (parent/'ftebroom-quake1.txt').write_text('"" '+json.dumps(mod.as_posix())+' "" "" 0 0 "" 0\n')
for name in ['compilation-ui-live.json','native-input-live.json','welcome-ui-live.json']:(data/name).unlink(missing_ok=True)
# These commands seed the fixture and enable read-only telemetry. Compilation
# and both launch actions below are performed through native OS mouse input.
(data/'engine-cleanup-qa.cfg').write_text('exec ftebroom.cfg\nset ftebroom_input_qa 1\nin 10 ftebroom_test_desktop reyon8qa dark\nin 11 ftebroom_test_filepath open '+json.dumps(mapfile.as_posix())+'\nin 12 ftebroom_test_run 50\n')
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
stream=(out/'editor.log').open('wb');process=None;checks=[]
def live():return read_ui_json(state)
def wait(fn,timeout=60):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  assert process.poll() is None,'Editor exited; inspect '+str(out/'editor.log')
  try:
   if value:=fn():return value
  except (OSError,ValueError,KeyError):pass
  time.sleep(.07)
 raise AssertionError('Engine cleanup UI timeout: '+str(live())[:1800])
def check(ok,name):
 assert ok,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
def click(key):
 ui.settle();r=wait(lambda:live()['widgets'].get(key));origin=live()['widgets']['window']
 with (out/'os-clicks.jsonl').open('a') as log:log.write(json.dumps({'key':key,'rect':r,'origin':origin})+'\n')
 ui.click(origin['x']+r['x']+r['width']*.45,origin['y']+r['y']+r['height']*.5);ui.settle()
def shot(name):
 ui.move(1268,780);ui.settle();ImageGrab.grab().save(out/name)
def validate_record(path,prefix):
 record=json.loads(path.read_text())
 check(Path(record['cwd'])==enginedir,prefix+'_uses_engine_executable_directory')
 check(record['argv'][-6:]==['-basedir',game.as_posix(),'-game','CustomMod','+map','customer-real-map'],prefix+'_uses_real_map_and_active_mod')
try:
 process=subprocess.Popen([str(root/engine_name),*render,'+set','ftebroom_skip_setup','1','+set','ftebroom_input_qa','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width','1280','-height','800','-basedir',str(root),'+exec','engine-cleanup-qa.cfg'],cwd=root,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'},stdout=stream,stderr=subprocess.STDOUT)
 wait(lambda:Path(live().get('document',''))==mapfile and live().get('compileOpen') and 'compile' in live().get('widgets',{}))
 ui=NativeInput(process,state,live,wait,out,focus_required=True);ui.position()
 if not windows:ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 ui.move(1100,740);ui.settle()
 check(Path(live()['document'])==mapfile,'external_map_path_keeps_user_filename')
 click('compile');wait(lambda:not live()['running'] and 'Launching engine profile' in live()['log'])
 check(not bsp.exists() and not lit.exists() and target.read_bytes()==valid and targetlit.read_bytes()==lighting,'customer_profile_copy_cleanup_deploys_bsp_lit')
 wait(lambda:markers[0].is_file())
 check(not live()['error'],'copy_cleanup_compile_launch_starts_process_probe')
 validate_record(markers[0],'compile_launch')
 # A console process can write its receipt before its console closes and
 # changes foreground ownership. Observe termination before the next OS click.
 wait(lambda:'Engine exited with code 0' in live()['log'])
 shot('copy-cleanup-launch.png')
 previous_exits=live()['log'].count('Engine exited with code 0')
 click('launch');click('launch-engine/1');click('engine-launch');wait(lambda:markers[1].is_file())
 check(not live()['error'],'standalone_launch_starts_process_probe')
 validate_record(markers[1],'standalone_launch')
 wait(lambda:live()['log'].count('Engine exited with code 0')>previous_exits)
 check(targetlit.read_bytes()==lighting and not bsp.exists(),'deployed_lighting_survives_launch_without_build_output')
 click('launch-close');markers[0].unlink();bsp.write_bytes(b'bad newer output')
 click('launch');click('launch-engine/0');click('engine-launch');wait(lambda:'truncated BSP header' in live()['error'])
 check(not markers[0].exists() and target.read_bytes()==valid,'existing_invalid_build_does_not_launch_stale_deployed_map')
 shot('invalid-source-rejected.png');bsp.unlink();target.unlink();click('engine-launch')
 wait(lambda:'deployed map also missing' in live()['error'])
 check(not markers[0].exists(),'missing_build_and_deployed_map_prevent_launch')
 check(inventory(original)==original_hashes,'source_runtime_remains_byte_identical')
except Exception:
 (out/'original-failure-state.json').write_text(json.dumps(live(),indent=2))
 if windows and process is not None and process.poll() is None and 'ui' in globals():
  try:
   ui.position();ui.windows_receipt('diagnostic-before-F24')
   ui.user.keybd_event(135,ui.user.MapVirtualKeyW(135,0),0,0)
   ui.settle()
   ui.user.keybd_event(135,ui.user.MapVirtualKeyW(135,0),2,0)
   ui.settle();ui.windows_receipt('diagnostic-after-F24')
   (out/'after-F24-state.json').write_text(json.dumps(live(),indent=2))
  except Exception as error:(out/'F24-error.txt').write_text(type(error).__name__+': '+str(error))
  try:
   ui.console('echo RB134_INPUT_DIAGNOSTIC; in_rawinput; in_rawinput_keyboard; in_dinput; in_windowed_mouse')
   (out/'after-console-state.json').write_text(json.dumps(live(),indent=2))
  except Exception as error:(out/'console-diagnostic-error.txt').write_text(type(error).__name__+': '+str(error))
 raise
finally:
 if process is not None and process.poll() is None:process.kill();process.wait(timeout=15)
 stream.close()
 if state.exists():
  try:(out/'last-state.json').write_text(json.dumps(live(),indent=2))
  except (OSError,ValueError):pass
 (out/'provenance.json').write_text(json.dumps({'build_info':(root/'build-info.txt').read_text(),'module_sha256':sha(root/module_name),'qc_sha256':sha(data/'csaddon.dat'),'engine_sha256':sha(root/engine_name),'probe_sha256':sha(executable),'source_runtime':str(original),'checks':len(checks),'method':'Native OS clicks in the actual packaged editor; process/argv probes record arguments and working directory. Synthetic Quake BSP header; no actual VKQuake/Ironwail rendering claim.'},indent=2))
