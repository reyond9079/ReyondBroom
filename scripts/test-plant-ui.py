#!/usr/bin/env python3
"""Exercise the actual Plant Editor through OS input in a disposable runtime."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from PIL import ImageGrab
from editor_os_input import NativeInput

p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True);data=root/'ftebroom'
for file in out.glob('ui-plant*'):
 if file.is_file():file.unlink()
settings=data/'plants/library.json'
if settings.exists():
 value=json.loads(settings.read_text());value['advanced']=False;settings.write_text(json.dumps(value))
path=data/'plant-ui-live.json';path.unlink(missing_ok=True)
(data/'plant-interaction.cfg').write_text('exec qa-clip.cfg\nin 12 ftebroom_plant qa dark\n')
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64')
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
stream=(out/'editor.log').open('wb');process=subprocess.Popen([str(engine),*render,'-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width','1920','-height','1080','-basedir',str(root),'+exec','plant-interaction.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
checks=[]
def live():return json.loads(path.read_text(encoding='utf-8'))
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
def console(command):ui.console(command);time.sleep(.25)
try:
 ready();ui.position();expect(live()['open'] and len(live()['lods'])==3,'native Plant Editor opens with three real LODs')
 expect(live()['lods'][0]['triangles']>live()['lods'][1]['triangles']>live()['lods'][2]['triangles'],'live LOD statistics show real reduction')
 old_seed=live()['project']['seed'];click('randomize');ready();expect(live()['project']['seed']!=old_seed,'Randomize changes the generated seed')
 click('keep-variation');ready();expect(live()['project']['seed'] in live()['project']['variations'],'saved seed variation persists in editable project')
 source=data/'plants/projects/qa-editable.plant.json';source.unlink(missing_ok=True)
 click('save');choose(source);wait(lambda:source.exists());saved=json.loads(source.read_text());expect(saved==live()['project'],'Save writes the complete editable project')
 previous=live();height=previous['project']['parameters']['height'];edit('Height','180',True);ready();expect(abs(live()['project']['parameters']['height']-180)<.01,'Height control changes actual parameters')
 expect(live()['camera']==previous['camera'] and live()['cameraLow']==previous['cameraLow'] and live()['cameraHigh']==previous['cameraHigh'],'regeneration preserves camera orbit, zoom and target')
 click('undo');ready();expect(live()['project']['parameters']['height']==height,'Plant Undo restores the parameter edit')
 click('redo');ready();expect(live()['project']['parameters']['height']==180,'Plant Redo restores the generated change')
 click('new');click('confirm-discard');ready();expect(live()['project']['name']=='Untitled oak','New respects the unsaved-project confirmation')
 click('open');click('confirm-discard');choose(source);ready();expect(live()['project']==saved,'Open restores parameters, hierarchy, seed and saved variations')
 exported=out/'ui-plant.glb';exported.unlink(missing_ok=True);click('export');choose(exported);wait(lambda:exported.exists());expect(exported.read_bytes()[:4]==b'glTF','Export writes a real embedded GLB')
 obj=out/'ui-plant.obj';obj.unlink(missing_ok=True);click('export');choose(obj);wait(lambda:obj.exists());expect((out/'ui-plant.mtl').is_file() and (out/'ui-plant-albedo.png').is_file(),'OBJ export writes its real MTL and PNG dependencies')
 edit('Plant name','QA woodland');ready();click('save-preset');ready()
 user_id=live()['project']['preset'];user_file=data/'plants/presets'/user_id.removeprefix('user:');expect(user_id.startswith('user:') and user_file.exists() and json.loads(user_file.read_text())==live()['project'],'Save preset creates a versioned editable user preset')
 click('favorite');expect(''==live()['error'],'Favorite persists without model errors')
 # Select representative species through the actual search and preset rows.
 for preset,search in [('oak','English oak'),('pine','Scots pine'),('wild-shrub','Wild hawthorn'),('meadow-grass','Meadow grass'),('amanita','Fly agaric'),('daisy','Oxeye daisy'),('bracken','Common bracken')]:
  edit('search',search);click('preset/'+preset);ready();expect(live()['project']['preset']==preset and live()['lods'][0]['triangles']>0,'native generation '+preset);screenshot(preset+'-dark')
 # Settings use the actual editor theme; geometry and project stay intact.
 console('ftebroom_plant qa light');wait(lambda:live()['theme']==3);screenshot('plant-editor-light');expect(live()['project']['preset']=='bracken','Modern Light changes the UI without replacing the plant')
 console('ftebroom_plant qa dark');wait(lambda:live()['theme']==0)
 edit('search','English oak');click('preset/oak');ready();click('advanced');expect('node/trunk' in live()['widgets'],'Advanced mode exposes the editable structure')
 before=live()['lods'][0]['triangles'];click('node-visible/leaves');ready();expect(live()['lods'][0]['triangles']<before,'hiding Leaves removes actual geometry')
 click('undo');ready();expect(live()['lods'][0]['triangles']==before,'hierarchy visibility participates in Undo')
 # This profile is Quake III or FTE in the disposable fixture.
 click('add-map');wait(lambda:live()['model']);model=Path(live()['model']);expect(model.is_file() and model.read_bytes()[:4]==b'IDP3','Add to Map writes a native MD3 asset')
 manifest=json.loads(Path(str(model)+'.reyond.json').read_text());expect(any(s.endswith('.shader') for s in manifest['dependencies']),'placed asset retains its alpha-cutout shader dependency')
 placed=data/'maps/plant-placed.map';placed.unlink(missing_ok=True)
 console('mod_terrain_save "" maps/plant-placed.map');wait(lambda:placed.exists());map_text=placed.read_text(errors='replace');relative=model.relative_to(data).as_posix()
 expect(relative in map_text and '"_reyondbroom_authoring" "1"' in map_text,'map save retains the generated model reference and authoring marker')
 console('ftebroom_undo');console('mod_terrain_save "" maps/plant-undo.map');undone=data/'maps/plant-undo.map';wait(lambda:undone.exists());expect(relative not in undone.read_text(errors='replace'),'map Undo removes the inserted entity')
 console('ftebroom_redo');console('mod_terrain_save "" maps/plant-redo.map');redone=data/'maps/plant-redo.map';wait(lambda:redone.exists());expect(relative in redone.read_text(errors='replace'),'map Redo restores the inserted entity')
 console('mapedit maps/plant-redo.map');time.sleep(2);console('mod_terrain_save "" maps/plant-reopened.map');reopened=data/'maps/plant-reopened.map';wait(lambda:reopened.exists());expect(relative in reopened.read_text(errors='replace'),'map reopen preserves the installed model')
 for f in [placed,undone,redone,reopened]:shutil.copy2(f,out/f.name)
 screenshot('plant-in-editor');(out/'final-state.json').write_text(json.dumps(live(),indent=2));shutil.copy2(source,out/source.name)
 print('Plant UI:',len(checks),'checks passed',flush=True)
finally:
 try:(out/'last-state.json').write_text(json.dumps(live(),indent=2));ImageGrab.grab().save(out/'last-desktop.png')
 except Exception:pass
 if process.poll() is None:process.terminate();process.wait(timeout=20)
 stream.close()
