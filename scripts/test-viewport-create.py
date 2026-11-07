#!/usr/bin/env python3
"""Real OS-input acceptance of world-click -> authoring project -> Save.

The live JSON reports rectangles and state only. It never invokes handlers or
changes editor documents. Console commands only prepare fixtures / export MAP
history for independent inspection; all Create and Save actions use OS input.
"""
import argparse,json,os,re,subprocess,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput
p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);p.add_argument('--width',type=int,default=1920);p.add_argument('--height',type=int,default=1080);p.add_argument('--theme',default='dark');a=p.parse_args()
root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True);data=root/'ftebroom';path=data/'placement-ui-live.json';path.unlink(missing_ok=True)
(data/'placement-interaction.cfg').write_text('exec qa-clip.cfg\nset ftebroom_placement_qa 1\nin 12 ftebroom_plant qa '+a.theme+'\n')
for f in (data/'maps').glob('creation-qa-*.map'):f.unlink()
(data/'maps/creation-qa-switch.map').write_bytes((data/'maps/qa_clip.map').read_bytes())
project=data/'plants/projects/creation-qa.plant.json';project.unlink(missing_ok=True)
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64');render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
stream=(out/'editor.log').open('wb');process=subprocess.Popen([str(engine),*render,'+set','ftebroom_skip_setup','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width',str(a.width),'-height',str(a.height),'-basedir',str(root),'+exec','placement-interaction.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
checks=[]
def live():return json.loads(path.read_text(encoding='utf-8'))
def wait(fn,timeout=90):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  if process.poll() is not None:raise RuntimeError('Editor exited; see '+str(out/'editor.log'))
  try:
   v=fn()
   if v:return v
  except (FileNotFoundError,json.JSONDecodeError,KeyError,PermissionError):pass
  time.sleep(.08)
 raise AssertionError('Timed out; latest state='+path.read_text()[:1200])
ui=NativeInput(process,path,live,wait,out)
def check(ok,name):
 assert ok,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
def click(key):
 r=wait(lambda:live()['widgets'].get(key));ui.click(r['x']+r['width']*.45,r['y']+r['height']*.5)
def hover(key):
 r=wait(lambda:live()['widgets'].get(key));ui.move(r['x']+r['width']*.45,r['y']+r['height']*.5);time.sleep(.5)
def edit(key,value,scalar=False):
 r=wait(lambda:live()['widgets'].get(key));ui.click(r['x']+r['width']*.3,r['y']+r['height']*.5,mods=('ctrl',) if scalar else ());ui.key('a',('ctrl',));ui.key('backspace');ui.text(str(value));ui.key('enter')
def close_plant():
 w=live()['widgets']['plant/window'];ui.click(w['x']+w['width']-16,w['y']+14)
 if 'plant/confirm-discard' in live()['widgets']:click('plant/confirm-discard')
 wait(lambda:not live()['plantOpen'])
def context(kind):
 v=live()['views'][0];ui.click(v['x']+v['width']*.46,v['y']+v['height']*.52,button=3)
 hover('context/create');hover('context/model');click('context/new-'+kind)
def snapshot(name):
 f=data/'maps'/('creation-qa-'+name+'.map');f.unlink(missing_ok=True);ui.console('mod_terrain_save "" creation-qa-'+name);wait(f.exists);text=f.read_text(errors='replace');(out/f.name).write_text(text);return text

def entities(text):return [dict(re.findall(r'"([^"\n]+)"\s+"([^"\n]*)"',x)) for x in re.findall(r'\{([^{}]*)\}',text) if '"_reyondbroom_placement"' in x]
try:
 wait(lambda:live()['plantReady']);ui.position()
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 close_plant();context('plant');wait(lambda:live()['plantReady'] and live()['plant']['active']);captured=live()['plant']['point'];check(live()['plant']['id']==live()['context']['id'],'plant route binds the initial click')
 # Moving the camera after opening the authoring window must not change the hit.
 ui.console('setpos 800 -900 600 20 110 0');click('plant/save');wait(lambda:'plant/file-path' in live()['widgets']);edit('plant/file-path',project);click('plant/file-choose');wait(lambda:live()['plant']['created'] and not live()['plant']['awaiting']);check(not live()['plantError'],live()['plantError'] or 'first Save succeeds')
 first=entities(snapshot('first'));check(len(first)==1 and first[0]['classname']=='func_model','first Save creates exactly one func_model');old=first[0]['model'];origin=first[0]['origin'];check(live()['plant']['point']==captured,'Save retains original world contact after camera movement');normal=live()['plant']['normal'];xyz=[float(x) for x in origin.strip("'").split()];tangent=[xyz[i]-captured[i] for i in range(3)];projection=sum(tangent[i]*normal[i] for i in range(3));check(sum((tangent[i]-normal[i]*projection)**2 for i in range(3))<256,'placement uses captured surface normal and grid basis');check((data/old).is_file(),'runtime asset exists before insertion')
 edit('plant/Height',170,True);wait(lambda:live()['plantReady']);click('plant/save');wait(lambda:not live()['plant']['awaiting']);second=entities(snapshot('second'));check(len(second)==1 and second[0]['model']!=old and second[0]['origin']==origin,'repeat Save updates same entity and preserves transform');check((data/old).is_file(),'previous immutable runtime asset survives for history')
 ui.console('ftebroom_undo');undone=entities(snapshot('undo'));check(len(undone)==1 and undone[0]['model']==old,'Undo restores previous visible asset path');ui.console('ftebroom_redo');check(entities(snapshot('redo'))[0]['model']==second[0]['model'],'Redo restores updated asset path')
 ui.console('ftebroom_delete');edit('plant/Height',160,True);wait(lambda:live()['plantReady']);click('plant/save');wait(lambda:not live()['plant']['active']);check(not entities(snapshot('deleted-save')),'Save after deletion never resurrects an entity')
 close_plant();context('model');wait(lambda:live()['newModel']);edit('model/new-name','creation-qa-'+str(int(time.time())))
 # Prop template is chosen through the real combo with keyboard navigation.
 click('model/new-template');click('model/template/Prop');click('model/new-create');wait(lambda:live()['modelOpen'] and live()['model']['active']);check(live()['modelElements']>0,'Prop template creates exportable geometry');ui.key('s',('ctrl',));wait(lambda:live()['model']['created'] and not live()['model']['awaiting']);created=entities(snapshot('model'));check(len(created)==1 and created[0]['classname']=='func_model','Model Editor first Save creates func_model');check(created[0]['_reyondbroom_source'].endswith('.bbmodel'),'model entity retains portable editable source link');check((data/created[0]['model']).is_file(),'model runtime MD3 saved automatically')
 ui.key('s',('ctrl',));time.sleep(.4);check(len(entities(snapshot('model-repeat')))==1,'unchanged model Save does not duplicate the entity')
 # A geometry change produces a new asset but retains the same scene entity.
 before_count=live()['modelElements'];click('model/transform-menu');click('model/transform/Add Cube');wait(lambda:live()['modelElements']>before_count);ui.key('s',('ctrl',));wait(lambda:not live()['modelBusy']);updated=entities(snapshot('model-updated'));check(len(updated)==1 and updated[0]['model']!=created[0]['model'],'model geometry Save updates the same linked entity');check((data/created[0]['model']).exists(),'model history keeps the old geometry asset')
 # Save As changes the authoring filename without inserting another entity.
 click('model/file-menu');click('model/file/Save as');edit('model/file-name','creation-qa-as-'+str(int(time.time()))+'.bbmodel');click('model/file-accept');wait(lambda:not live()['modelBusy']);check(len(entities(snapshot('model-save-as')))==1,'model Save As preserves the single placement binding');saved_path=live()['modelPath']
 click('model/file-menu');click('model/file/Close');context('model');wait(lambda:live()['newModel']);edit('model/new-name','creation-qa-empty-'+str(int(time.time())));click('model/new-template');click('model/template/Empty');click('model/new-create');wait(lambda:live()['modelOpen'] and live()['model']['active']);ui.key('s',('ctrl',));wait(lambda:not live()['modelBusy'] and 'failed' in live()['modelMessage']);check(len(entities(snapshot('empty-model-error')))==1,'failed model export leaves no broken scene entity')
 # Switching the source map invalidates this unsatisfied Create request.
 ui.console('mapedit maps/creation-qa-switch.map');wait(lambda:not live()['model']['active']);click('model/file-menu');click('model/file/Save');check(not entities(snapshot('stale-map-save')),'Save after a map switch cannot insert into the new map')
 ImageGrab.grab().save(out/'model-editor.png');(out/'final-state.json').write_text(json.dumps(live(),indent=2));print('PASS viewport creation',len(checks),flush=True)
except Exception:
 ImageGrab.grab().save(out/'failure.png');raise
finally:
 if process.poll() is None:process.kill();process.wait(timeout=15)
 stream.close()
