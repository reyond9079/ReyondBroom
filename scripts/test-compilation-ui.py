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
def profiles():return json.loads(store.read_text())['games'][live()['game']]['profiles']
def expect(value,name):
 assert value,name
 checks.append(name);(out/'ui-results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
if os.name=='nt':
 from ctypes import wintypes
 user=ctypes.windll.user32
 from pywinauto import Desktop
 user.SetProcessDPIAware()
 hwnd=None;client_scale=(1,1)
 user.GetForegroundWindow.restype=wintypes.HWND
 user.ClientToScreen.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.POINT)]
 user.GetClientRect.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.RECT)]
 @ctypes.WINFUNCTYPE(ctypes.c_bool,wintypes.HWND,wintypes.LPARAM)
 def enum(w,l):
  global hwnd
  pid=wintypes.DWORD();user.GetWindowThreadProcessId(w,ctypes.byref(pid))
  title=ctypes.create_unicode_buffer(512);user.GetWindowTextW(w,title,512)
  if pid.value==process.pid and user.IsWindowVisible(w) and title.value.startswith('ReyondBroom:'):hwnd=w;return False
  return True
 def close_window():
  position();user.PostMessageW(hwnd,0x0010,0,0);time.sleep(.2)
 def position():
  global client_scale
  user.EnumWindows(enum,0)
  assert hwnd
  point=wintypes.POINT();user.ClientToScreen(hwnd,ctypes.byref(point));rect=wintypes.RECT();user.GetClientRect(hwnd,ctypes.byref(rect));display=live()['display'];client_scale=(rect.right/display[0],rect.bottom/display[1]);
  (out/'window-position.json').write_text(json.dumps({'origin':[point.x,point.y],'client':[rect.right,rect.bottom],'display':display,'scale':client_scale,'target':hwnd,'foreground':user.GetForegroundWindow()}))
  return point.x,point.y
 def window_diagnostics():
  windows=[]
  @ctypes.WINFUNCTYPE(ctypes.c_bool,wintypes.HWND,wintypes.LPARAM)
  def collect(w,l):
   pid=wintypes.DWORD();thread=user.GetWindowThreadProcessId(w,ctypes.byref(pid))
   if pid.value==process.pid:
    title=ctypes.create_unicode_buffer(512);kind=ctypes.create_unicode_buffer(256)
    user.GetWindowTextW(w,title,512);user.GetClassNameW(w,kind,256)
    windows.append({'handle':w,'thread':thread,'title':title.value,'class':kind.value,'visible':bool(user.IsWindowVisible(w))})
   return True
  user.EnumWindows(collect,0)
  (out/'native-windows.json').write_text(json.dumps(windows,indent=2))
 # Hosted desktops can accept mouse input while dropping globally injected
 # keyboard events. Send native window messages, with scan codes and shared
 # keyboard state, through FTE's real WM_KEYDOWN/UP handler instead.
 def strokes(text):
  window_diagnostics()
  Desktop(backend='win32').window(handle=hwnd).send_keystrokes(text)
 def chord(code,modifiers=()):
  strokes(''.join({17:'^',16:'+',18:'%'}[m] for m in modifiers)+chr(code).lower())
 codes={'ctrl':'^','shift':'+','alt':'%','enter':'{ENTER}','esc':'{ESC}','tab':'{TAB}','backspace':'{BACKSPACE}','grave':'`'}
 def key(name,mods=()):
  strokes(''.join(codes[m] for m in mods)+codes[name])
  time.sleep(.15)
 def type_text(text):
  strokes(''.join('{'+c+'}' if c in '+^%~(){}' else c for c in text))
 def mouse(x,y,right=False):
  ox,oy=position();Desktop(backend='win32').window(handle=hwnd).set_focus();user.SetCursorPos(int(ox+x*client_scale[0]),int(oy+y*client_scale[1]));time.sleep(.08)
  user.mouse_event(8 if right else 2,0,0,0,0);time.sleep(.08);user.mouse_event(16 if right else 4,0,0,0,0);time.sleep(.25)
else:
 from Xlib import X,XK,display
 from Xlib.ext import xtest
 d=display.Display()
 def close_window():
  from Xlib.protocol import event
  for window in d.screen().root.query_tree().children:
   if (window.get_wm_name() or '').startswith('ReyondBroom:'):
    message=event.ClientMessage(window=window,client_type=d.intern_atom('WM_PROTOCOLS'),data=(32,[d.intern_atom('WM_DELETE_WINDOW'),X.CurrentTime,0,0,0]));window.send_event(message,event_mask=0);d.sync();time.sleep(.2);return
  raise RuntimeError('Editor window not found')
 def position():
  for window in d.screen().root.query_tree().children:
   if (window.get_wm_name() or '').startswith('ReyondBroom:'):
    point=d.screen().root.translate_coords(window,0,0);return point.x,point.y
  raise RuntimeError('Editor window not found')
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
  ox,oy=position();xtest.fake_input(d,X.MotionNotify,x=int(ox+x),y=int(oy+y));d.sync();time.sleep(.08);button=3 if right else 1
  xtest.fake_input(d,X.ButtonPress,button);d.sync();time.sleep(.06);xtest.fake_input(d,X.ButtonRelease,button);d.sync();time.sleep(.25)
def click(name,right=False):
 state=wait_for(lambda:live() if name in live()['widgets'] or name in live().get('globalWidgets',{}) else None)
 if name in state.get('globalWidgets',{}):
  r=state['globalWidgets'][name];mouse(r['x']+r['width']*.5,r['y']+r['height']*.5,right)
 else:
  r=state['widgets'][name];w=state['widgets']['window'];mouse(w['x']+r['x']+r['width']*.5,w['y']+r['y']+(10 if right and name.startswith('card/') else r['height']*.5),right)

def edit(name,text):
 click(name)
 # Ctrl+A is a normal text-edit shortcut on both backends.
 if os.name=='nt':chord(65,[17])
 else:
  ctrl=d.keysym_to_keycode(XK.string_to_keysym('Control_L'));code=d.keysym_to_keycode(ord('a'));xtest.fake_input(d,X.KeyPress,ctrl);xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code);xtest.fake_input(d,X.KeyRelease,ctrl);d.sync()
 key('backspace');type_text(text);time.sleep(.2)
def console(command):key('grave');type_text(command);key('enter');key('grave')
def paste(text):
 if os.name=='nt':
  kernel=ctypes.windll.kernel32
  kernel.GlobalAlloc.argtypes=[wintypes.UINT,ctypes.c_size_t];kernel.GlobalAlloc.restype=wintypes.HANDLE
  kernel.GlobalLock.argtypes=[wintypes.HANDLE];kernel.GlobalLock.restype=ctypes.c_void_p
  kernel.GlobalUnlock.argtypes=[wintypes.HANDLE]
  user.OpenClipboard.argtypes=[wintypes.HWND]
  user.SetClipboardData.argtypes=[wintypes.UINT,wintypes.HANDLE];user.SetClipboardData.restype=wintypes.HANDLE
  payload=(text+'\0').encode('utf-16-le');handle=kernel.GlobalAlloc(2,len(payload));assert handle
  pointer=kernel.GlobalLock(handle);assert pointer;ctypes.memmove(pointer,payload,len(payload));kernel.GlobalUnlock(handle)
  wait_for(lambda:user.OpenClipboard(hwnd))
  try:user.EmptyClipboard();assert user.SetClipboardData(13,handle)
  finally:user.CloseClipboard()
  chord(86,[17])
 else:
  subprocess.run([os.environ.get('RB_QA_XCLIP','xclip'),'-selection','clipboard'],input=text.encode(),check=True)
  ctrl=d.keysym_to_keycode(XK.string_to_keysym('Control_L'));v=d.keysym_to_keycode(ord('v'))
  for code,kind in [(ctrl,X.KeyPress),(v,X.KeyPress),(v,X.KeyRelease),(ctrl,X.KeyRelease)]:xtest.fake_input(d,kind,code)
  d.sync()
 time.sleep(.3)
try:
 wait_for(lambda:live()['widgets'].get('profile-add'))
 click('profile-add');expect(profiles()[0]['name']=='unnamed','create profile using mini toolbar')
 edit('Profile name','');expect(profiles()[0]['name']=='','empty names persist')
 edit('Profile name','UI parity');key('tab');wait_for(lambda:live()['widgets']['Working directory'].get('active'));expect(True,'Tab moves from Name to Working Directory')
 edit('Working directory','${MAP_D');key('tab');click('completion/MAP_DIR_PATH');expect(profiles()[0]['workdir']=='${MAP_DIR_PATH}','Tab variable completion')
 sandbox=data/'maps'/('ui-parity-'+str(process.pid));sandbox.mkdir(parents=True,exist_ok=True);edit('Working directory',str(sandbox).replace('\\','/'))
 click('task-add');click('add-task/0');expect(profiles()[0]['tasks'][0]['type']=='export','add Export Map from six-item menu')
 click('task-add');click('add-task/1');expect(profiles()[0]['tasks'][1]['type']=='tool','insert tool after selected task')
 probe=root/('tools/compilation-profile-probe.exe' if os.name=='nt' else 'tools/compilation-profile-probe')
 edit('task/1/Tool path',str(probe).replace('\\','/'))
 click('task/1/Parameters');paste('x'*8192);wait_for(lambda:len(profiles()[0]['tasks'][1]['parameters'])==8192);expect(True,'pasting long parameters does not truncate the value')
 edit('task/1/Parameters','write ui-real.txt marker')
 click('test');time.sleep(1);expect(not (sandbox/'ui-real.txt').exists() and not (sandbox/'qa_clip-compile.map').exists(),'Test launches no process and writes no export')
 click('task/1/Parameters');key('enter');wait_for(lambda:(sandbox/'ui-real.txt').exists());expect((sandbox/'ui-real.txt').read_text()=='marker','Compile starts actual configured process');expect((sandbox/'qa_clip-compile.map').is_file(),'Compile exports current map')
 click('task/1/enabled');expect(not profiles()[0]['tasks'][1]['enabled'],'disabled state persists')
 click('task-up');expect(profiles()[0]['tasks'][0]['type']=='tool','move selected task up')
 click('task-down');expect(profiles()[0]['tasks'][1]['type']=='tool','move selected task down')
 click('card/1',right=True);click('task-context/duplicate');expect(len(profiles()[0]['tasks'])==3,'duplicate selected task')
 click('task-remove');expect(len(profiles()[0]['tasks'])==2,'remove selected task')
 click('profile/0',right=True);click('profile-context/duplicate');expect(len(profiles())==2 and profiles()[0]['name']==profiles()[1]['name'],'duplicate profile keeps its name')
 click('profile-remove');expect(len(profiles())==1,'remove profile from mini toolbar')
 # A close confirmation must not block the background process loop.
 click('task/1/enabled');edit('task/1/Parameters','sleep 30000');click('compile');wait_for(lambda:live().get('running'))
 click('task/1/Parameters');key('esc');click('close-confirm-no');expect(live()['compileOpen'] and live()['running'],'Escape from a field / No keeps compilation running')
 original_document=live()['document'];click('menu/File');click('Menu/File/New');click('close-confirm-no');expect(live()['running'] and live()['document']==original_document,'Changing document asks before stopping active compilation')
 close_window();click('close-confirm-no');expect(live()['compileOpen'] and live()['running'],'OS window Close / No keeps editor and compilation alive')
 click('stop');wait_for(lambda:not live()['running']);expect(True,'Stop terminates actual sleeping process')
 click('compile');wait_for(lambda:live()['running']);click('close');click('close-confirm-yes');wait_for(lambda:not live()['running'] and not live()['compileOpen']);expect(True,'Close Yes stops process and closes dialog')
 click('menu/Run');click('Menu/Run/Compile...');wait_for(lambda:live()['compileOpen'])
 click('launch');click('launch-configure');click('engine-add');edit('Engine name','Probe engine');edit('Engine path',str(probe).replace('\\','/'));click('engine-close')
 wait_for(lambda:len(json.loads(store.read_text())['games'][live()['game']]['engines'])==1)
 click('launch-engine/0');edit('Launch parameters','write ui-detached.txt launched');marker=probe.parent/'ui-detached.txt';marker.unlink(missing_ok=True);click('engine-launch');wait_for(lambda:marker.is_file());expect(marker.read_text()=='launched','Launch dialog preserves edited arguments and engine working directory');click('launch-close')
 before=store.read_bytes();click('import');edit('import-path',str(repo/'tests/fixtures/CompilationProfiles-original.cfg').replace('\\','/'));click('import-review');click('import-cancel');expect(store.read_bytes()==before,'review and Cancel do not change profiles')
 click('import');click('import-review');click('import-commit');expect(len(profiles())==6 and sum(len(x['tasks']) for x in profiles()[1:])==70,'UI imports original five profiles and seventy tasks')
 expect(any('.bspw' in t.get('parameters','') for p in profiles()[1:] for t in p['tasks']),'UI import preserves bspw')
 from PIL import ImageGrab
 ImageGrab.grab().save(out/'ui-completed.png')
 click('menu/File');click('Menu/File/Exit');process.wait(timeout=10)

finally:
 try:
  (out/'last-state.json').write_text(json.dumps(live(),indent=2))
  shutil.copy2(store,out/'profiles-final.json')
  from PIL import ImageGrab
  ImageGrab.grab().save(out/'last-desktop.png')
 except Exception as error:(out/'capture-error.txt').write_text(str(error))
 if process.poll() is None:process.terminate();process.wait(timeout=10)
 stream.close()
