#!/usr/bin/env python3
"""Cold viewport Create and tall native plant Add to Map via real OS input."""
import argparse,json,os,re,struct,subprocess,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json

p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True);data=root/'ftebroom';state=data/'placement-ui-live.json';state.unlink(missing_ok=True)
# No plant QA/open command here: first ever viewport Create must open its own
# editor before any hidden unsaved-document confirmation can be drawn.
(data/'plant-target-ui.cfg').write_text('exec qa-clip.cfg\nset ftebroom_placement_qa 1\nin 11 echo RB_PLANT_FIXTURE_READY\n')
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
 # qa-clip schedules its camera/view setup after map load. Wait for that
 # fixture to finish and fresh UI frames before the first cold context click.
 wait(lambda:'RB_PLANT_FIXTURE_READY' in current_run_log());ui.settle()
 view=live()['views'][0];ui.click(view['x']+view['width']*.46,view['y']+view['height']*.52,button=3)
 hover('context/create');hover('context/model');click('context/new-plant');wait(lambda:live()['plantOpen'] and live()['plantReady'])
 check(live()['plant']['active'] and live()['plant']['id']==live()['context']['id'],'cold viewport Create opens Plant Editor and retains captured scene click')
 ui.console('ftebroom_plant qa dark');wait(lambda:'plant/open' in live()['widgets']);click('plant/open');wait(lambda:'plant/file-path' in live()['widgets']);r=live()['widgets']['plant/file-path'];ui.click(r['x']+r['width']*.35,r['y']+r['height']*.5);ui.key('a',('ctrl',));ui.text(str(project));ui.key('enter');click('plant/file-choose');wait(lambda:live()['plantReady'] and live()['plantPath']==str(project))
 check(not live()['plant']['active'],'ordinary tall source open has no pending context insertion')
 plant_state=data/'plant-ui-live.json'
 def game_preview():
  state=read_ui_json(plant_state)
  preview=state.get('gamePreview',{})
  return preview if preview.get('active') and preview.get('current') and not preview.get('error') else None
 wait(game_preview)
 check(True,'newly opened Plant Editor defaults to the active profile model')
 original_camera=read_ui_json(plant_state)['camera']
 click('plant/lod');click('plant/lod/0')
 wait(lambda:not read_ui_json(plant_state)['gamePreview']['active'])
 click('plant/lod');click('plant/game-preview')
 game=wait(game_preview)
 check(read_ui_json(plant_state)['camera']==original_camera,'switching to Game Preview preserves the camera')
 click('plant/add-map');wait(lambda:bool(live()['plantMessage']) and not live()['plantError']);placed=wait(lambda:map_entities('tall'))
 check(len(placed)==1 and placed[0]['classname']=='misc_model','direct Add to Map creates one Quake III misc_model')
 entity=placed[0];scale=float(entity.get('modelscale','1'));normalization=float(entity.get('_reyondbroom_normalization','1'))
 check(scale>1 and abs(scale-normalization)<.0001,'tall MD3 stores its world-size normalization on the entity')
 check(entity['_reyondbroom_source'].endswith('.plant.json'),'direct Add to Map retains immutable editable source')
 model=data/entity['model'];encoded=model.read_bytes();check(encoded[:4]==b'IDP3','FTE/Quake III target is an actual native MD3')
 metadata=json.loads((data/(entity['model']+'.reyond.json')).read_text());_,_,zmin=struct.unpack_from('<3f',encoded,108);_,_,zmax=struct.unpack_from('<3f',encoded,120)
 check(all(game[field]==metadata['target'][field] for field in ('vertices','triangles')),
       'Game Preview reports the same geometry counts as the placed MD3')
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
 if process.poll() is None:process.kill();process.wait(timeout=15)
 log.close()
