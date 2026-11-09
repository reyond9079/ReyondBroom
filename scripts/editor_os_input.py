"""OS input for native editor QA; never edits editor model state or calls UI callbacks."""
import ctypes
import json
import os
import time


def read_ui_json(path,timeout=2):
 # The engine periodically replaces these read-only QA snapshots. Windows can
 # briefly deny access during replacement; existence alone is not a read ack.
 # Retry only the file read, never a user action or a failed UI assertion.
 deadline=time.monotonic()+timeout
 while True:
  try:return json.loads(path.read_text(encoding='utf-8'))
  except (FileNotFoundError,PermissionError,json.JSONDecodeError):
   if time.monotonic()>=deadline:raise
   time.sleep(.02)


class NativeInput:
 def __init__(self,process,live_path,live,wait,output,focus_required=False):
  self.process,self.path,self.live,self.wait,self.output=process,live_path,live,wait,output
  self.focus_path=live_path.parent/'native-input-live.json'
  self.focus_required=focus_required
  self.hwnd=None
  self.in_console=False
  if os.name=='nt':
   from ctypes import wintypes
   from pywinauto import Desktop,keyboard
   self.types,self.Desktop,self.keyboard=wintypes,Desktop,keyboard
   self.user=ctypes.windll.user32;self.user.SetProcessDPIAware()
   self.user.SendMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM];self.user.SendMessageW.restype=wintypes.LPARAM
   self.user.ClientToScreen.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.POINT)]
   self.user.GetClientRect.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.RECT)]
   self.user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)];self.user.GetWindowThreadProcessId.restype=wintypes.DWORD
   self.user.AttachThreadInput.argtypes=[wintypes.DWORD,wintypes.DWORD,wintypes.BOOL]
   self.user.VkKeyScanW.argtypes=[wintypes.WCHAR];self.user.VkKeyScanW.restype=ctypes.c_short
  else:
   from Xlib import X,XK,display
   from Xlib.ext import xtest
   self.X,self.XK,self.xtest,self.d=X,XK,xtest,display.Display()

 def read_focus(self):
  if not self.focus_path.exists():return None
  try:
   state=read_ui_json(self.focus_path)
   return state if state['pid']==self.process.pid else None
  except (FileNotFoundError,PermissionError,json.JSONDecodeError,KeyError):return None

 def focus_state(self):
  state=self.read_focus()
  return self.wait(self.read_focus) if state is None and self.focus_required else state

 def settle(self):
  if self.in_console:
   state=self.focus_state()
   if state is None:return # Older modules lack opt-in engine input telemetry.
   for _ in range(2):
    sequence=state['sequence']
    state=self.wait(lambda:(s if (s:=self.read_focus()) and s['sequence']!=sequence else None))
   return
  for _ in range(2):
   stamp=self.wait(lambda:self.path.stat().st_mtime_ns)
   self.wait(lambda:self.path.stat().st_mtime_ns!=stamp)

 def position(self):
  if os.name=='nt':
   t=self.types;u=self.user
   @ctypes.WINFUNCTYPE(ctypes.c_bool,t.HWND,t.LPARAM)
   def enum(w,l):
    pid=t.DWORD();u.GetWindowThreadProcessId(w,ctypes.byref(pid));title=ctypes.create_unicode_buffer(512);u.GetWindowTextW(w,title,512)
    if pid.value==self.process.pid and u.IsWindowVisible(w) and title.value.startswith('ReyondBroom:'):self.hwnd=w;return False
    return True
   u.EnumWindows(enum,0);assert self.hwnd,'Editor window missing'
   self.Desktop(backend='win32').window(handle=self.hwnd).set_focus()
   point=t.POINT();u.ClientToScreen(self.hwnd,ctypes.byref(point));rect=t.RECT();u.GetClientRect(self.hwnd,ctypes.byref(rect))
   size=self.live()['display'];self.scale=(rect.right/size[0],rect.bottom/size[1]);return point.x,point.y
  for w in self.d.screen().root.query_tree().children:
   if (w.get_wm_name() or '').startswith('ReyondBroom:'):
    self.window=w;point=self.d.screen().root.translate_coords(w,0,0);return point.x,point.y
  raise AssertionError('Editor window missing')

 def strokes(self,text):
  u=self.user;target=u.GetWindowThreadProcessId(self.hwnd,None);current=ctypes.windll.kernel32.GetCurrentThreadId()
  assert u.AttachThreadInput(current,target,True)
  state=(ctypes.c_ubyte*256)();assert u.GetKeyboardState(state);initial=bytes(state);stack=[]
  paced=bool(text) and text[0] in "^+%"
  try:
   for action in self.keyboard.parse_keys(text,with_spaces=True,with_tabs=True,with_newlines=True):
    vk,scan,flags=action.get_key_info();modifiers=0
    if flags&4:
     mapped=u.VkKeyScanW(chr(scan));assert mapped!=-1;vk=mapped&255;modifiers=(mapped>>8)&255;scan=u.MapVirtualKeyW(vk,0)
    if action.down:
     stack.append(bytes(state));state[vk]|=128
     for mask,key in [(1,16),(2,17),(4,18)]:
      if modifiers&mask:state[key]|=128
     assert u.SetKeyboardState(state);context=bool(state[18]&128)
     u.SendMessageW(self.hwnd,0x104 if context else 0x100,vk,1|(scan<<16)|((flags&1)<<24)|(int(context)<<29))
    if action.up:
     context=bool(state[18]&128);u.SendMessageW(self.hwnd,0x105 if context else 0x101,vk,1|(scan<<16)|((flags&1)<<24)|(int(context)<<29)|(3<<30))
     if stack:state=(ctypes.c_ubyte*256).from_buffer_copy(stack.pop())
     else:state[vk]&=127
     assert u.SetKeyboardState(state)
    # A shortcut spans multiple ImGui key transitions. Keep the target-thread
    # modifier state until each transition has reached fresh UI frames.
    if paced:time.sleep(.03);self.settle()
  finally:u.SetKeyboardState((ctypes.c_ubyte*256).from_buffer_copy(initial));u.AttachThreadInput(current,target,False)
  self.settle()

 def mod(self,name,down):
  if os.name=='nt':
   code={'ctrl':17,'shift':16,'alt':18}[name];scan=self.user.MapVirtualKeyW(code,0)
   # Mouse events below enter the OS queue. Their modifier state must come
   # from that same queue, rather than an isolated WM_KEYDOWN to the window.
   self.user.keybd_event(code,scan,0 if down else 2,0);time.sleep(.05);self.settle()
  else:
   code=self.d.keysym_to_keycode(self.XK.string_to_keysym({'ctrl':'Control_L','shift':'Shift_L','alt':'Alt_L'}[name]));self.xtest.fake_input(self.d,self.X.KeyPress if down else self.X.KeyRelease,code);self.d.sync();time.sleep(.07)

 def move(self,x,y):
  ox,oy=self.position()
  if os.name=='nt':self.user.SetCursorPos(int(ox+x*self.scale[0]),int(oy+y*self.scale[1]));self.settle()
  else:self.xtest.fake_input(self.d,self.X.MotionNotify,x=int(ox+x),y=int(oy+y));self.d.sync();time.sleep(.08)

 def click(self,x,y,button=1,mods=(),hold=None):
  self.move(x,y)
  for mod in mods:self.mod(mod,True)
  try:
   if os.name=='nt':
    self.user.mouse_event(2 if button==1 else 8 if button==3 else 32,0,0,0,0)
    # Observe fresh UI frames with the button down before releasing it. A
    # fixed hold can otherwise fit entirely between slow software-rendered
    # frames, losing the click. This never retries the action.
    try:time.sleep(.10 if hold is None else hold);self.settle()
    finally:self.user.mouse_event(4 if button==1 else 16 if button==3 else 64,0,0,0,0)
   else:
    self.xtest.fake_input(self.d,self.X.ButtonPress,button);self.d.sync()
    try:time.sleep(.08 if hold is None else hold);self.settle()
    finally:self.xtest.fake_input(self.d,self.X.ButtonRelease,button);self.d.sync()
   # Keep modifiers held until the release has reached the editor as well.
   self.settle()
  finally:
   for mod in reversed(mods):self.mod(mod,False)
  time.sleep(.25)
  self.settle()

 def key(self,name,mods=()):
  names={'enter':'Return','esc':'Escape','tab':'Tab','backspace':'BackSpace','grave':'grave'}
  if os.name=='nt':self.strokes(''.join({'ctrl':'^','shift':'+','alt':'%'}[m] for m in mods)+{'enter':'{ENTER}','esc':'{ESC}','tab':'{TAB}','backspace':'{BACKSPACE}','grave':'`','space':'{SPACE}','Home':'{HOME}','End':'{END}','Down':'{DOWN}','Up':'{UP}'}.get(name,name));return
  for mod in mods:self.mod(mod,True)
  code=self.d.keysym_to_keycode(self.XK.string_to_keysym(names.get(name,name)));self.xtest.fake_input(self.d,self.X.KeyPress,code);self.xtest.fake_input(self.d,self.X.KeyRelease,code);self.d.sync()
  for mod in reversed(mods):self.mod(mod,False)
  time.sleep(.15)
  self.settle()

 def text(self,text):
  if any(ord(c)>127 for c in text) or (os.name=='nt' and len(text)>20):
   # Clipboard paste is real OS input and preserves UTF-8 paths that cannot be
   # represented by the active keyboard layout. Long Windows paths/commands
   # also paste through the OS; short ASCII probes still use key events.
   if os.name=='nt':
    from ctypes import wintypes
    u,k=self.user,ctypes.windll.kernel32;k.GlobalAlloc.argtypes=[wintypes.UINT,ctypes.c_size_t];k.GlobalAlloc.restype=wintypes.HGLOBAL;k.GlobalLock.argtypes=[wintypes.HGLOBAL];k.GlobalLock.restype=ctypes.c_void_p;k.GlobalUnlock.argtypes=[wintypes.HGLOBAL];u.SetClipboardData.argtypes=[wintypes.UINT,wintypes.HANDLE];u.SetClipboardData.restype=wintypes.HANDLE
    for attempt in range(20):
     if u.OpenClipboard(None):break
     time.sleep(.03)
    else:raise AssertionError('Cannot acquire OS clipboard for Unicode input')
    try:
     assert u.EmptyClipboard();data=text.encode('utf-16-le')+b'\0\0';handle=k.GlobalAlloc(2,len(data));assert handle;pointer=k.GlobalLock(handle);assert pointer;ctypes.memmove(pointer,data,len(data));k.GlobalUnlock(handle);assert u.SetClipboardData(13,handle)
    finally:u.CloseClipboard()
   else:
    import shutil,subprocess
    executable=os.environ.get('RB_QA_XCLIP') or shutil.which('xclip');assert executable,'Unicode OS-input QA requires xclip'
    subprocess.run([executable,'-selection','clipboard'],input=text.encode('utf-8'),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True,timeout=10)
   if os.name=='nt':
    # Clipboard paste enters the real OS queue, like mouse-wheel modifiers.
    # Temporary SetKeyboardState around SendMessage is not the keyboard state
    # observed when the engine later consumes its queued paste key event.
    self.position();self.mod('ctrl',True)
    try:
     # A tap is queued in order; ImGui preserves its down/up transitions.
     # Holding V over slow render frames triggers InputText paste repeat.
     self.user.keybd_event(86,self.user.MapVirtualKeyW(86,0),0,0)
     self.user.keybd_event(86,self.user.MapVirtualKeyW(86,0),2,0)
     self.settle()
    finally:self.mod('ctrl',False)
    with (self.output/'clipboard-paste.log').open('a',encoding='utf-8') as log:
     import hashlib
     log.write(json.dumps({'length':len(text),'sha256':hashlib.sha256(text.encode('utf-8')).hexdigest(),'method':'OS queued Ctrl+V single tap; modifiers span observed ticks','focus':self.read_focus()})+'\n')
   else:self.key('v',('ctrl',))
   return
  if os.name=='nt':self.strokes(''.join('{'+c+'}' if c in '+^%~(){}' else c for c in text));return
  shift=self.d.keysym_to_keycode(self.XK.string_to_keysym('Shift_L'))
  for char in text:
   code=self.d.keysym_to_keycode(ord(char));upper=self.d.keycode_to_keysym(code,0)!=ord(char)
   if upper:self.xtest.fake_input(self.d,self.X.KeyPress,shift)
   self.xtest.fake_input(self.d,self.X.KeyPress,code);self.xtest.fake_input(self.d,self.X.KeyRelease,code)
   if upper:self.xtest.fake_input(self.d,self.X.KeyRelease,shift)
   self.d.sync();time.sleep(.007)
  time.sleep(.2)
  self.settle()

 def scroll(self,steps):
  if os.name=='nt':self.user.mouse_event(0x800,0,0,steps*120,0);self.settle();return
  for _ in range(abs(steps)):
   button=4 if steps>0 else 5;self.xtest.fake_input(self.d,self.X.ButtonPress,button);self.xtest.fake_input(self.d,self.X.ButtonRelease,button)
  self.d.sync();time.sleep(.25)

 def console(self,command):
  self.in_console=True
  try:
   self.key('esc',('shift',))
   # A fixed sleep is not an acknowledgment of a slow engine input frame.
   # Hold shortcut transitions across observed ticks, and do not type before
   # the engine actually routes input to its console. No action is retried.
   observed=self.focus_state()
   if observed is not None:
    opened=self.wait(lambda:(s if (s:=self.read_focus()) and s['destination']&48 and not s['destination']&64 else None))
    with (self.output/'console-focus.jsonl').open('a',encoding='utf-8') as log:log.write(json.dumps({'stage':'opened','command':command,'state':opened})+'\n')
   time.sleep(.25)
   self.text('/'+command);time.sleep(.25)
   self.key('enter');time.sleep(.25)
   self.key('esc');time.sleep(.25)
   if observed is not None:
    closed=self.wait(lambda:(s if (s:=self.read_focus()) and not s['destination']&48 else None))
    with (self.output/'console-focus.jsonl').open('a',encoding='utf-8') as log:log.write(json.dumps({'stage':'closed','command':command,'state':closed})+'\n')
  finally:
   try:(self.output/'console-last-focus.json').write_text(json.dumps(self.read_focus()),encoding='utf-8')
   except OSError:pass
   self.in_console=False
  self.settle()
