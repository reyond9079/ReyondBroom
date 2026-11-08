#!/usr/bin/env python3
"""Cold viewport Create and tall native plant Add to Map via real OS input."""
import argparse,json,os,re,struct,subprocess,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json

p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);p.add_argument('--variant',choices=['A','B'],required=True);a=p.parse_args()
root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True);data=root/'ftebroom';state=data/'placement-ui-live.json';state.unlink(missing_ok=True)
# No plant QA/open command here: first ever viewport Create must open its own
# editor before any hidden unsaved-document confirmation can be drawn.
(data/'plant-target-ui.cfg').write_text('exec qa-clip.cfg\nset ftebroom_placement_qa 1\n'+('in 11 echo RB_PLANT_FIXTURE_READY\n' if a.variant=='B' else ''))
tall=json.loads((Path(__file__).resolve().parents[1]/'tests/fixtures/plant-generator1.json').read_text())[0]['project'];tall['parameters']['height']=1200;tall['name']='Tall native target regression';project=out/'tall.plant.json';project.write_text(json.dumps(tall))
console_logs=[data/'qconsole.log',root/'qconsole.log']
for path in console_logs:path.unlink(missing_ok=True)
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64');log=(out/'editor.log').open('wb')
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
process=subprocess.Popen([str(engine),*render,'-basedir',str(root),'-nohome','-noupdates','-nosound','-condebug','-window','-width','1920','-height','1080','+set','ftebroom_skip_setup','1','+set','ftebroom_input_qa','1','+exec','plant-target-ui.cfg'],cwd=root,stdout=log,stderr=subprocess.STDOUT)
checks=[]
def live():return read_ui_json(state)
def current_run_log():
 chunks=[]
 for path in [out/'editor.log',*console_logs]:
  try:chunks.append(path.read_text(errors='replace'))
  except FileNotFoundError:pass
 return '\n'.join(chunks)
def wait(fn,timeout=90):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  assert process.poll() is None,'Editor exited'
  try:
   if value:=fn():return value
  except (FileNotFoundError,KeyError,json.JSONDecodeError):pass
  time.sleep(.08)
 raise AssertionError('Timed out: '+str(live())[:2000])
ui=NativeInput(process,state,live,wait,out,focus_required=True)
def click(key):
 wait(lambda:live()['widgets'].get(key));ui.settle();r=live()['widgets'][key];ui.click(r['x']+r['width']*.45,r['y']+r['height']*.5)
def hover(key):
 r=wait(lambda:live()['widgets'].get(key));ui.move(r['x']+r['width']*.45,r['y']+r['height']*.5);time.sleep(.5)
def check(value,label):
 assert value,label
 checks.append(label);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',label,flush=True)
cold_snapshots=[]
cold_events=[]
def cold_snapshot(phase,target=None):
 record={'phase':phase,'variant':a.variant,'monotonic':time.monotonic(),'target_client':target}
 try:record['focus']=ui.read_focus()
 except Exception as error:record['focus_error']=type(error).__name__
 try:record['placement']=live()
 except Exception as error:record['placement_error']=type(error).__name__
 if os.name=='nt':
  try:
   import ctypes
   t=ui.types;u=ui.user
   u.GetCursorPos.argtypes=[ctypes.POINTER(t.POINT)];u.GetCursorPos.restype=t.BOOL
   u.ScreenToClient.argtypes=[t.HWND,ctypes.POINTER(t.POINT)];u.ScreenToClient.restype=t.BOOL
   u.GetForegroundWindow.argtypes=[];u.GetForegroundWindow.restype=t.HWND
   u.GetAsyncKeyState.argtypes=[ctypes.c_int];u.GetAsyncKeyState.restype=ctypes.c_short
   point=t.POINT();u.GetCursorPos(ctypes.byref(point));screen=[point.x,point.y]
   u.ScreenToClient(ui.hwnd,ctypes.byref(point));rect=t.RECT();u.GetClientRect(ui.hwnd,ctypes.byref(rect));origin=t.POINT();u.ClientToScreen(ui.hwnd,ctypes.byref(origin))
   record['os']={'cursor_screen':screen,'cursor_client':[point.x,point.y],'client_rect':[rect.left,rect.top,rect.right,rect.bottom],'client_origin_screen':[origin.x,origin.y],'target_hwnd':int(ui.hwnd or 0),'foreground_hwnd':int(u.GetForegroundWindow() or 0),'scale':list(getattr(ui,'scale',(1,1))),'buttons_modifiers':{name:bool(u.GetAsyncKeyState(code)&0x8000) for name,code in [('right',2),('left',1),('shift',16),('ctrl',17),('alt',18)]}}
  except Exception as error:record['os_error']=type(error).__name__
 cold_snapshots.append(record)
def save_cold_receipts():
 (out/'cold-click-snapshots.json').write_text(json.dumps(cold_snapshots,indent=2))
 (out/'cold-click-os-events.json').write_text(json.dumps(cold_events,indent=2))
def cold_click(view):
 x=view['x']+view['width']*.46;y=view['y']+view['height']*.52;target=[x,y]
 cold_snapshot('BEFOREmove',target)
 original_mouse_event=None
 if os.name=='nt':
  original_mouse_event=ui.user.mouse_event
  def observed_mouse_event(*args):
   flags=int(args[0]);cold_events.append({'flags':flags,'args':[int(v) for v in args],'monotonic':time.monotonic()})
   if flags&8:cold_snapshot('BEFORErightdown',target)
   if flags&16:cold_snapshot('BEFOREup_whileheld',target)
   result=original_mouse_event(*args)
   if flags&16:cold_snapshot('AFTERup_queued',target)
   return result
  ui.user.mouse_event=observed_mouse_event
 try:ui.click(x,y,button=3)
 finally:
  if original_mouse_event is not None:ui.user.mouse_event=original_mouse_event
  cold_snapshot('AFTERup',target);save_cold_receipts()
  ImageGrab.grab().save(out/'cold-click-after.png')
def map_entities(label):
 path=data/'maps'/('target-ui-'+label+'.map');path.unlink(missing_ok=True);ui.console('mod_terrain_save "" target-ui-'+label);wait(path.is_file)
 text=path.read_text(errors='replace');(out/path.name).write_text(text)
 return [dict(re.findall(r'"([^"\n]+)"\s+"([^"\n]*)"',entity)) for entity in re.findall(r'\{([^{}]*)\}',text) if '"_reyondbroom_authoring"' in entity]
try:
 # qa-clip loads its fixture after seven seconds. The initial blank document
 # already has an epoch; clicking it would correctly cancel the captured
 # binding when the delayed map arrives while the plant is generating.
 wait(lambda:live()['views'] and live()['epoch']>=2 and live()['mapPath'].replace('\\','/').endswith('/maps/qa_clip.map'));ui.position()
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 if a.variant=='B':
  wait(lambda:'RB_PLANT_FIXTURE_READY' in current_run_log());ui.settle()
 cold_click(live()['views'][0])
 hover('context/create');hover('context/model');click('context/new-plant');wait(lambda:live()['plantOpen'] and live()['plantReady'])
 check(live()['plant']['active'] and live()['plant']['id']==live()['context']['id'],'cold viewport Create opens Plant Editor and retains captured scene click')
 ui.console('ftebroom_plant qa dark');wait(lambda:'plant/open' in live()['widgets']);click('plant/open');wait(lambda:'plant/file-path' in live()['widgets']);r=live()['widgets']['plant/file-path'];ui.click(r['x']+r['width']*.35,r['y']+r['height']*.5);ui.key('a',('ctrl',));ui.text(str(project));ui.key('enter');click('plant/file-choose');wait(lambda:live()['plantReady'] and live()['plantPath']==str(project))
 check(not live()['plant']['active'],'ordinary tall source open has no pending context insertion')
 click('plant/add-map');wait(lambda:bool(live()['plantMessage']) and not live()['plantError']);placed=wait(lambda:map_entities('tall'))
 check(len(placed)==1,'direct Add to Map creates exactly one authoring func_model')
 entity=placed[0];scale=float(entity.get('modelscale','1'));normalization=float(entity.get('_reyondbroom_normalization','1'))
 check(scale>1 and abs(scale-normalization)<.0001,'tall MD3 stores its world-size normalization on the entity')
 check(entity['_reyondbroom_source'].endswith('.plant.json'),'direct Add to Map retains immutable editable source')
 model=data/entity['model'];encoded=model.read_bytes();check(encoded[:4]==b'IDP3','FTE/Quake III target is an actual native MD3')
 metadata=json.loads((data/(entity['model']+'.reyond.json')).read_text());_,_,zmin=struct.unpack_from('<3f',encoded,108);_,_,zmax=struct.unpack_from('<3f',encoded,120)
 check(abs((zmax-zmin)*scale-(metadata['high'][2]-metadata['low'][2]))<.2,'saved model retains tall source height after MD3 coordinate normalization')
 ui.console('ftebroom_test_desktop rb10q1optin')
 wait(lambda:'FTEBROOM_DESKTOP_PASS rb10_q1_scene_optin_does_not_mutate_immutable_assets' in current_run_log())
 check(True,'Quake1 scene opt-in preserves exact immutable target assets')
 ImageGrab.grab().save(out/'plant-target-editor.png')
 # One user action remains one undoable transaction, including normalization.
 ui.console('ftebroom_undo');check(not map_entities('undo'),'Undo removes direct Add to Map in one transaction');ui.console('ftebroom_redo');restored=map_entities('redo');check(len(restored)==1 and restored[0].get('modelscale')==entity['modelscale'],'Redo restores model and its world-size scale together')
except Exception:
 ImageGrab.grab().save(out/'failure.png')
 try:(out/'failure-state.json').write_text(json.dumps(live(),indent=2))
 except Exception:pass
 raise
finally:
 save_cold_receipts()
 (out/'diagnostic-check-count.json').write_text(json.dumps({'variant':a.variant,'completed_original_checks':len(checks),'required_original_checks':10},indent=2))
 if process.poll() is None:process.kill();process.wait(timeout=15)
 log.close()
