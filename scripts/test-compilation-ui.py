#!/usr/bin/env python3
"""Click and type in the real ImGui editor via OS input, checking persistence and real task results."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path)
p.add_argument('output',type=Path)
a=p.parse_args()
root=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
data=root/'ftebroom';repo=Path(__file__).resolve().parents[1]
store=data/'compilation-profiles.json'
store.write_text(json.dumps({'version':1,'games':{}}))
(data/'compilation-ui-live.json').unlink(missing_ok=True)
(data/'ui-interaction.cfg').write_text('exec qa-clip.cfg\nin 5 ftebroom_test_run 60\nin 6 ftebroom_test_run 50\n')
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64')
renderer_args=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
stream=(out/'editor.log').open('wb')
process=subprocess.Popen([str(engine),*renderer_args,'-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width','1280','-height','800','-basedir',str(root),'+exec','ui-interaction.cfg'],cwd=root,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'},stdout=stream,stderr=subprocess.STDOUT)
checks=[]
def wait_for(fn,timeout=10):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  if process.poll() is not None:raise RuntimeError('Editor exited: '+(out/'editor.log').read_text(errors='replace')[-3000:])
  try:
   value=fn()
   if value:return value
  except (FileNotFoundError,json.JSONDecodeError,KeyError):pass
  time.sleep(.1)
 raise AssertionError('Timed out waiting for editor state')
def live():return json.loads((data/'compilation-ui-live.json').read_text())
def profiles():return json.loads(store.read_text())['games']['quake3']['profiles']
def expect(value,name):
 assert value,name
 checks.append(name);(out/'ui-results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
if os.name=='nt':
 from ctypes import wintypes
 user=ctypes.windll.user32
 user.SetProcessDPIAware()
 hwnd=None
 @ctypes.WINFUNCTYPE(ctypes.c_bool,wintypes.HWND,wintypes.LPARAM)
 def enum(w,l):
  global hwnd
  pid=wintypes.DWORD();user.GetWindowThreadProcessId(w,ctypes.byref(pid))
  if pid.value==process.pid and user.IsWindowVisible(w):hwnd=w;return False
  return True
 def position():
  user.EnumWindows(enum,0)
  assert hwnd
  point=wintypes.POINT();user.ClientToScreen(hwnd,ctypes.byref(point));return point.x,point.y
 def press(code,up=False):user.keybd_event(code,0,2 if up else 0,0)
 codes={'ctrl':17,'shift':16,'alt':18,'enter':13,'esc':27,'tab':9,'backspace':8,'grave':192}
 def key(name,mods=()):
  for m in mods:press(codes[m])
  press(codes[name]);press(codes[name],True)
  for m in reversed(mods):press(codes[m],True)
  time.sleep(.15)
 def type_text(text):
  for c in text:
   v=user.VkKeyScanW(ord(c));shift=(v>>8)&1
   if shift:press(16)
   press(v&255);press(v&255,True)
   if shift:press(16,True)
   time.sleep(.005)
 def mouse(x,y,right=False):
  ox,oy=position();user.SetForegroundWindow(hwnd);user.SetCursorPos(int(ox+x),int(oy+y));time.sleep(.08)
  user.mouse_event(8 if right else 2,0,0,0,0);user.mouse_event(16 if right else 4,0,0,0,0);time.sleep(.25)
else:
 from Xlib import X,XK,display
 from Xlib.ext import xtest
 d=display.Display()
 def position():return 0,0
 def key(name,mods=()):
  names={'ctrl':'Control_L','shift':'Shift_L','alt':'Alt_L','enter':'Return','esc':'Escape','tab':'Tab','backspace':'BackSpace','grave':'grave'}
  codes=[d.keysym_to_keycode(XK.string_to_keysym(names[m])) for m in mods]
  for c in codes:xtest.fake_input(d,X.KeyPress,c)
  c=d.keysym_to_keycode(XK.string_to_keysym(names[name]));xtest.fake_input(d,X.KeyPress,c);xtest.fake_input(d,X.KeyRelease,c)
  for c in reversed(codes):xtest.fake_input(d,X.KeyRelease,c)
  d.sync();time.sleep(.15)
 def type_text(text):
  shift=d.keysym_to_keycode(XK.string_to_keysym('Shift_L'))
  for char in text:
   symbol=ord(char);code=d.keysym_to_keycode(symbol);upper=d.keycode_to_keysym(code,0)!=symbol
   if upper:xtest.fake_input(d,X.KeyPress,shift)
   xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code)
   if upper:xtest.fake_input(d,X.KeyRelease,shift)
   d.sync();time.sleep(.005)
 def mouse(x,y,right=False):
  xtest.fake_input(d,X.MotionNotify,x=int(x),y=int(y));d.sync();time.sleep(.08);button=3 if right else 1
  xtest.fake_input(d,X.ButtonPress,button);xtest.fake_input(d,X.ButtonRelease,button);d.sync();time.sleep(.25)
def click(name,right=False):
 state=wait_for(lambda:live() if name in live()['widgets'] or name in live().get('globalWidgets',{}) else None)
 if name in state.get('globalWidgets',{}):
  r=state['globalWidgets'][name];mouse(r['x']+r['width']*.5,r['y']+r['height']*.5,right)
 else:
  r=state['widgets'][name];w=state['widgets']['window'];mouse(w['x']+r['x']+r['width']*.5,w['y']+r['y']+(10 if right and name.startswith('card/') else r['height']*.5),right)

def edit(name,text):
 click(name)
 # Ctrl+A is a normal text-edit shortcut on both backends.
 if os.name=='nt':press(17);press(65);press(65,True);press(17,True)
 else:
  ctrl=d.keysym_to_keycode(XK.string_to_keysym('Control_L'));code=d.keysym_to_keycode(ord('a'));xtest.fake_input(d,X.KeyPress,ctrl);xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code);xtest.fake_input(d,X.KeyRelease,ctrl);d.sync()
 key('backspace');type_text(text);time.sleep(.2)
def console(command):key('grave');type_text(command);key('enter');key('grave')
try:
 wait_for(lambda:live()['widgets'].get('profile-add'))
 click('profile-add');expect(profiles()[0]['name']=='unnamed','create profile using mini toolbar')
 edit('Profile name','');expect(profiles()[0]['name']=='','empty names persist')
 edit('Profile name','UI parity')
 edit('Working directory','${MAP_D');key('tab');click('completion/MAP_DIR_PATH');expect(profiles()[0]['workdir']=='${MAP_DIR_PATH}','Tab variable completion')
 sandbox=data/'maps'/('ui-parity-'+str(process.pid));sandbox.mkdir(parents=True,exist_ok=True);edit('Working directory',str(sandbox).replace('\\','/'))
 click('task-add');click('add-task/0');expect(profiles()[0]['tasks'][0]['type']=='export','add Export Map from six-item menu')
 click('task-add');click('add-task/1');expect(profiles()[0]['tasks'][1]['type']=='tool','insert tool after selected task')
 probe=root/('tools/compilation-profile-probe.exe' if os.name=='nt' else 'tools/compilation-profile-probe')
 edit('task/1/Tool path',str(probe).replace('\\','/'));edit('task/1/Parameters','write ui-real.txt marker')
 click('test');time.sleep(1);expect(not (sandbox/'ui-real.txt').exists() and not (sandbox/'qa_clip-compile.map').exists(),'Test launches no process and writes no export')
 click('compile');wait_for(lambda:(sandbox/'ui-real.txt').exists());expect((sandbox/'ui-real.txt').read_text()=='marker','Compile starts actual configured process');expect((sandbox/'qa_clip-compile.map').is_file(),'Compile exports current map')
 click('task/1/enabled');expect(not profiles()[0]['tasks'][1]['enabled'],'disabled state persists')
 click('task-up');expect(profiles()[0]['tasks'][0]['type']=='tool','move selected task up')
 click('task-down');expect(profiles()[0]['tasks'][1]['type']=='tool','move selected task down')
 click('card/1',right=True);click('task-context/duplicate');expect(len(profiles()[0]['tasks'])==3,'duplicate selected task')
 click('task-remove');expect(len(profiles()[0]['tasks'])==2,'remove selected task')
 click('profile/0',right=True);click('profile-context/duplicate');expect(len(profiles())==2 and profiles()[0]['name']==profiles()[1]['name'],'duplicate profile keeps its name')
 click('profile-remove');expect(len(profiles())==1,'remove profile from mini toolbar')
 # A close confirmation must not block the background process loop.
 click('task/1/enabled');edit('task/1/Parameters','sleep 10000');click('compile');wait_for(lambda:live().get('running'))
 click('close');click('close-confirm-no');expect(live()['compileOpen'] and live()['running'],'Close No keeps compilation running')
 click('stop');wait_for(lambda:not live()['running']);expect(True,'Stop terminates actual sleeping process')
 click('compile');wait_for(lambda:live()['running']);click('close');click('close-confirm-yes');wait_for(lambda:not live()['running'] and not live()['compileOpen']);expect(True,'Close Yes stops process and closes dialog')
 click('menu/Run');click('Menu/Run/Compile...');wait_for(lambda:live()['compileOpen'])
 click('launch');click('launch-configure');click('engine-add');edit('Engine name','Probe engine');edit('Engine path',str(probe).replace('\\','/'));click('engine-close')
 wait_for(lambda:len(json.loads(store.read_text())['games']['quake3']['engines'])==1)
 click('launch-engine/0');edit('Launch parameters','write ui-detached.txt launched');marker=probe.parent/'ui-detached.txt';marker.unlink(missing_ok=True);click('engine-launch');wait_for(lambda:marker.is_file());expect(marker.read_text()=='launched','Launch dialog preserves edited arguments and engine working directory');click('launch-close')
 before=store.read_bytes();click('import');edit('import-path',str(repo/'tests/fixtures/CompilationProfiles-original.cfg').replace('\\','/'));click('import-review');click('import-cancel');expect(store.read_bytes()==before,'review and Cancel do not change profiles')
 click('import');click('import-review');click('import-commit');expect(len(profiles())==6 and sum(len(x['tasks']) for x in profiles()[1:])==70,'UI imports original five profiles and seventy tasks')
 expect(any('.bspw' in t.get('parameters','') for p in profiles()[1:] for t in p['tasks']),'UI import preserves bspw')
 from PIL import ImageGrab
 ImageGrab.grab().save(out/'ui-completed.png')
 click('menu/File');click('Menu/File/Exit');process.wait(timeout=10)

finally:
 if process.poll() is None:process.terminate();process.wait(timeout=10)
 stream.close()
