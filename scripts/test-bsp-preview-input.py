#!/usr/bin/env python3
"""OS RMB capture/release and native relative motion must preserve BSP preview."""
import argparse,json,os,subprocess,sys,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[1];data=root/'ftebroom'
source=data/'maps/ftebroom-build/preview-input.map';source.parent.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(repo/'tests/make-build-fixture.py'),str(source)],check=True)
compiler=root/'tools/q3map2'/('q3map2.exe' if os.name=='nt' else 'q3map2')
with (out/'compile.log').open('wb') as log:
 subprocess.run([str(compiler),'-game','quake3','-fs_basepath',str(root),'-fs_game','ftebroom','-meta',str(source)],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
assert source.with_suffix('.bsp').stat().st_size>1024
state=data/'placement-ui-live.json';focus=data/'native-input-live.json'
for path in (state,focus,data/'qconsole.log',root/'qconsole.log'):path.unlink(missing_ok=True)
(data/'preview-input.cfg').write_text('set ftebroom_skip_setup 1\nexec ftebroom.cfg\nset ftebroom_input_qa 1\nset in_windowed_mouse 1\nset ftebroom_placement_qa 1\nin 7 mapedit maps/ftebroom-build/preview-input.map\nin 10 ftebroom_camera_restore 0 -160 96 0 90 0 90\nin 11 ftebroom_bsp maps/ftebroom-build/preview-input.bsp unlit\nin 13 echo PREVIEW_INPUT_READY\n')
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64');stream=(out/'editor.log').open('wb')
process=subprocess.Popen([str(engine),*render,'-basedir',str(root),'-nohome','-noupdates','-nosound','-condebug','-window','-width','1280','-height','800','+exec','preview-input.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT)
checks=[]
def logs():
 for path in (data/'qconsole.log',root/'qconsole.log'):
  if path.exists():return path.read_text(errors='replace')
 return ''
def live():return read_ui_json(state)
def wait(predicate,seconds=50):
 until=time.monotonic()+seconds
 while time.monotonic()<until:
  assert process.poll() is None,'Editor exited'
  try:
   result=predicate()
   if result:return result
  except (FileNotFoundError,KeyError,json.JSONDecodeError,PermissionError):pass
  time.sleep(.08)
 raise AssertionError('Timed out waiting for preview input state')
def check(condition,name):
 assert condition,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
ui=NativeInput(process,focus,live,wait,out,focus_required=True)
try:
 wait(lambda:'PREVIEW_INPUT_READY' in logs() and live()['bspPreview']);ui.position()
 # XTest's synthetic XI2 device cannot emit the physical relative events used
 # by the engine. Schedule the existing guarded native event bridge, then use
 # real OS RMB edges. This exercises the production event -> camera -> QC path
 # without mistaking an absolute cursor warp for camera movement.
 ui.console('in 6 ftebroom_test_filepath event mouse2 3 40 -8')
 v=live()['views'][0];x,y=v['x']+v['width']*.55,v['y']+v['height']*.55
 ImageGrab.grab().save(out/'before-rmb.png');ui.move(x,y)
 if os.name=='nt':ui.user.mouse_event(8,0,0,0,0)
 else:ui.xtest.fake_input(ui.d,ui.X.ButtonPress,3);ui.d.sync()
 try:
  wait(lambda:live()['contextGesture']['looking']);ui.settle()
  check(live()['bspPreview'],'RMB press and its context query retain BSP preview')
  gesture=wait(lambda:(g if (g:=live()['contextGesture'])['dx']**2+g['dy']**2>=16 else None))
  check(not gesture['candidate'] and live()['bspPreview'],'native relative motion reaches the camera while BSP preview remains active')
 finally:
  if os.name=='nt':ui.user.mouse_event(16,0,0,0,0)
  else:ui.xtest.fake_input(ui.d,ui.X.ButtonRelease,3);ui.d.sync()
 wait(lambda:not live()['contextGesture']['looking'])
 check(live()['bspPreview'],'RMB release retains BSP preview')
 ImageGrab.grab().save(out/'after-rmb.png')
 ui.key('esc');wait(lambda:not live()['bspPreview'])
 check(True,'Escape still returns explicitly to the editable map')
 (out/'combined.log').write_text(logs());(out/'provenance.json').write_text(json.dumps({'build_info':(root/'build-info.txt').read_text(),'method':'Actual OS RMB press/release and Escape; guarded native relative-event bridge; real q3map2 BSP'},indent=2))
finally:
 if process.poll() is None:process.kill();process.wait(timeout=15)
 stream.close()
