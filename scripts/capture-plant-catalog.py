#!/usr/bin/env python3
"""Exercise the actual Plant Editor through OS input in a disposable runtime."""
import argparse
import hashlib
import platform
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import time
from PIL import Image,ImageGrab
from editor_os_input import NativeInput

p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);p.add_argument('--controls',action='store_true');p.add_argument('--extended',action='store_true');a=p.parse_args()
root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True);data=root/'ftebroom'
for file in out.glob('ui-plant*'):
 if file.is_file():file.unlink()
settings=data/'plants/library.json'
if settings.exists():
 value=json.loads(settings.read_text());value['advanced']=False;settings.write_text(json.dumps(value))
path=data/'plant-ui-live.json';path.unlink(missing_ok=True)
for name in ['plant-placed','plant-undo','plant-redo','plant-reopened']:(data/'maps'/(name+'.map')).unlink(missing_ok=True)
(data/'plant-asset-qa.json').unlink(missing_ok=True)
shutil.rmtree(data/'plant-qa-build',ignore_errors=True)
(data/'plant-interaction.cfg').write_text('exec qa-clip.cfg\n'+'in 12 ftebroom_plant qa dark\n')
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64')
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
stream=(out/'editor.log').open('wb');process=subprocess.Popen([str(engine),*render,'+set','ftebroom_skip_setup','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width','1920','-height','1080','-basedir',str(root),'+exec','plant-interaction.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
checks=[]
def live():
 deadline=time.monotonic()+2
 while True:
  try:return json.loads(path.read_text(encoding='utf-8'))
  except (FileNotFoundError,PermissionError,json.JSONDecodeError):
   if time.monotonic()>=deadline:raise
   time.sleep(.02)
def wait(fn,timeout=25):
 deadline=time.monotonic()+timeout;last=None
 while time.monotonic()<deadline:
  if process.poll() is not None:raise RuntimeError('Editor exited: '+(out/'editor.log').read_text(errors='replace')[-1800:])
  try:
   value=fn()
   if value:return value
  except (FileNotFoundError,PermissionError,json.JSONDecodeError,KeyError) as e:last=e
  time.sleep(.08)
 raise AssertionError('Timed out waiting for plant state; last='+str(last))
def ready():return wait(lambda:live()['ready'],90)
def expect(value,name):
 assert value,name
 checks.append(name);(out/'ui-results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
ui=NativeInput(process,path,live,wait,out)
def click(name,mods=()):
 for _ in range(12):
  r=wait(lambda:live()['widgets'].get(name));x,y=r['x']+r['width']*.5,r['y']+r['height']*.5
  if 'clip' not in r:break
  left,top,right,bottom=r['clip']
  if r.get('visible',True) and top<=y<=bottom:
   x=max(left+3,min(right-3,x));break
  ui.move((left+right)*.5,(top+bottom)*.5);ui.scroll(4 if y<top else -4)
 else:raise AssertionError('Control remains outside its scroll area: '+name)
 ui.click(x,y,mods=mods)
def edit(name,text,scalar=False):
 click(name,('ctrl',) if scalar else ())
 wait(lambda:live()['widgets'][name].get('active'))
 ui.key('a',('ctrl',));ui.key('backspace');ui.text(str(text));ui.key('enter');time.sleep(.2)
def choose(target):
 wait(lambda:'file-path' in live()['widgets']);edit('file-path',str(target).replace('\\','/'));click('file-choose');wait(lambda:'file-path' not in live()['widgets'])
def screenshot(name):
 ui.settle();ImageGrab.grab().save(out/(name+'.png'))
def visible_foliage(path):
 image=Image.open(path).convert('RGB');w,h=image.size
 # The fixture has blue brushes/sky and teal UI. Its oak must contribute a
 # substantial green silhouette inside the scene, not just an entity marker.
 region=image.crop((int(w*.13),int(h*.16),int(w*.70),int(h*.88)))
 return sum(g>r*1.25 and g>b*1.8 and g>25 and r<160 and b<100 for r,g,b in region.getdata())
def console(command):ui.console(command);time.sleep(.25)
def orbit(yaw,pitch):
 for _ in range(8):
  state=live();cy,cp=state['camera'][:2];dx=((cy-yaw+180)%360-180)/.4;dy=(cp-pitch)/.4
  if abs(dx)<1.5 and abs(dy)<1.5:return
  v=state['widgets']['viewport'];dx=max(-v['width']*.35,min(v['width']*.35,dx));dy=max(-v['height']*.35,min(v['height']*.35,dy));x=v['x']+v['width']*.5;y=v['y']+v['height']*.5
  ui.move(x,y)
  if os.name=='nt':ui.user.mouse_event(2,0,0,0,0)
  else:ui.xtest.fake_input(ui.d,ui.X.ButtonPress,1);ui.d.sync()
  for step in range(1,7):ui.move(x+dx*step/6,y+dy*step/6)
  if os.name=='nt':ui.user.mouse_event(4,0,0,0,0)
  else:ui.xtest.fake_input(ui.d,ui.X.ButtonRelease,1);ui.d.sync()
  ui.settle()
 raise AssertionError('Camera did not reach reference angle')
def record(name):
 screenshot(name);state=live();state['capture_session']=process.pid
 if os.name!='nt':
  status=Path('/proc/'+str(process.pid)+'/status').read_text();state['process_memory_kib']={k:int(v) for k,v in re.findall(r'^(VmHWM|VmRSS):\s+(\d+)',status,re.M)}
 else:
  import ctypes
  class Counters(ctypes.Structure):_fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(n,ctypes.c_size_t) for n in ['peakWorkingSet','workingSet','peakPaged','paged','peakNonpaged','nonpaged','pagefile','peakPagefile']]
  c=Counters();c.cb=ctypes.sizeof(c);kernel=ctypes.windll.kernel32;kernel.OpenProcess.restype=ctypes.c_void_p;handle=kernel.OpenProcess(0x410,False,process.pid);ctypes.windll.psapi.GetProcessMemoryInfo(handle,ctypes.byref(c),c.cb);kernel.CloseHandle(handle);state['process_memory_kib']={'VmHWM':c.peakWorkingSet//1024,'VmRSS':c.workingSet//1024}
 (out/(name+'.json')).write_text(json.dumps(state,indent=2));print('CAPTURE',name,state['lods'][0]['triangles'],flush=True)
try:
 ready();ui.position()
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 controls={'oak','birch','willow','pine','spruce','wild-shrub','meadow-grass','bracken','daisy','amanita','saguaro','date-palm'}
 catalog=json.loads((Path(__file__).resolve().parents[1]/'runtime/plants/builtin/catalog.json').read_text())['presets']
 if a.controls:catalog=[d for d in catalog if d['id'] in controls]
 for d in catalog:
  key=d['id'];edit('search',d['name']);click('preset/'+key);ready();orbit(135,18);record(key)
  if a.extended and key in controls:
   for seed in [137,138,424242]:
    edit('Seed',seed);ready()
    for label,yaw,pitch in [('front',0,0),('side',90,0),('top-oblique-85',0,85)]:orbit(yaw,pitch);record(key+'-'+str(seed)+'-'+label)
   if live()['project']['generatorVersion']==2:
    edit('Seed',137);ready();orbit(135,18)
    for lod in [0,1,2]:click('lod');click('lod/'+str(lod));record(key+'-lod'+str(lod))
    click('lod');click('lod/0');click('display');click('display/1');record(key+'-untextured');click('display');click('display/0')
    v=live()['widgets']['viewport'];ui.move(v['x']+v['width']*.5,v['y']+v['height']*.5);ui.scroll(4);record(key+'-closeup');click('frame')
 metadata={'platform':platform.platform(),'machine':platform.machine(),'processor':platform.processor(),'build_label':os.environ.get('RB_QA_BUILD','unspecified'),'module_sha256':hashlib.sha256((root/('fteplug_ftebroom_x64.dll' if os.name=='nt' else 'fteplug_ftebroom_amd64.so')).read_bytes()).hexdigest(),'build_info':(root/'build-info.txt').read_text(errors='replace'),'screen':live()['display'],'renderer_note':'Inspect editor.log for actual renderer; software-renderer frame rates are not hardware guarantees'}
 if os.name!='nt':metadata['cpu']=next((x for x in Path('/proc/cpuinfo').read_text().splitlines() if x.startswith('model name')),'unknown')
 (out/'hardware.json').write_text(json.dumps(metadata,indent=2))
 (out/'capture-manifest.json').write_text(json.dumps({'presets':len(catalog),'originals':[p.name for p in sorted(out.glob('*.png'))],'camera_note':'top-oblique is 85 degrees, the legacy editor orbit limit'},indent=2))
finally:
 if process.poll() is None:process.kill();process.wait(timeout=15)
 stream.close()
