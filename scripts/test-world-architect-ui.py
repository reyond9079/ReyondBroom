#!/usr/bin/env python3
"""Exercise the developing World Architect through actual OS input in a private runtime."""
import argparse,hashlib,json,os,shutil,subprocess,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json
p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);p.add_argument('--preset',choices=['hospital','metro'],default='hospital');a=p.parse_args()
root=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);data=root/'ftebroom';state=data/'world-architect-ui-live.json';state.unlink(missing_ok=True)
for name in ['wa-before','wa-generated','wa-applied','wa-undone','wa-redone']:(data/'maps'/(name+'.map')).unlink(missing_ok=True)
(data/'world-qa.cfg').write_text('exec qa-clip.cfg\nset ftebroom_placement_qa 1\nin 12 ftebroom_world_architect qa\n')
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64');stream=(out/'editor.log').open('wb');render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
proc=subprocess.Popen([str(engine),*render,'+set','ftebroom_skip_setup','1','+set','ftebroom_input_qa','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width','1920','-height','1080','-basedir',str(root),'+exec','world-qa.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
checks=[]
module=root/('fteplug_ftebroom_x64.dll' if os.name=='nt' else 'fteplug_ftebroom_amd64.so')
(out/'provenance.json').write_text(json.dumps({'moduleSha256':hashlib.sha256(module.read_bytes()).hexdigest(),'qcSha256':hashlib.sha256((data/'csaddon.dat').read_bytes()).hexdigest(),'runtimeBuildInfo':(root/'build-info.txt').read_text(),'method':'Actual OS input; live read-only telemetry; generated MAP/texture assets; real executed import and history transactions'},indent=2)+'\n')
def live():return read_ui_json(state)
def wait(fn,timeout=60):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  assert proc.poll() is None,'Editor exited; inspect '+str(out/'editor.log')
  try:
   if result:=fn():return result
  except (OSError,ValueError,KeyError):pass
  time.sleep(.08)
 raise AssertionError('World UI timeout '+str(live())[:1500])
def check(value,name):
 assert value,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
ui=NativeInput(proc,state,live,wait,out,focus_required=True)
def click(key):
 r=wait(lambda:live()['widgets'].get(key));ui.click(r['x']+r['width']/2,r['y']+r['height']/2)
def console(text):ui.console(text);time.sleep(.2)
def save(name):
 dest=data/'maps'/(name+'.map');dest.unlink(missing_ok=True);console('mod_terrain_save "" '+name);wait(dest.exists);return dest
# Source brush plane signatures survive map pathname and editor metadata changes.
def planes(path):return sorted(line.strip() for line in path.read_text(errors='replace').splitlines() if line.lstrip().startswith('( '))
def choose(path):
 text=path.as_posix();r=wait(lambda:live()['widgets'].get('file-path'));ui.click(r['x']+r['width']*.4,r['y']+r['height']/2);ui.key('a',('ctrl',));ui.text(text)
 wait(lambda:read_ui_json(data/'placement-ui-live.json')['browsePath']==text);ui.key('enter');click('file-choose');wait(lambda:'file-path' not in live()['widgets'])
try:
 wait(lambda:live()['open'] and 'generate' in live()['widgets']);ui.position()
 # Xvfb has no window manager to transfer focus after an external engine exits.
 # Acquire actual OS keyboard focus before the first console shortcut.
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 before=save('wa-before');baseline=planes(before)
 if a.preset=='hospital':
  def seed(value):
   click('seed');ui.key('a',('ctrl',));ui.text(str(value));ui.key('enter');wait(lambda:live()['settings']['worldSeed']==value)
  seed(17);seed(29);click('undo-settings');wait(lambda:live()['settings']['worldSeed']==17);check(True,'settings Undo restores the previous independent seed edit')
  click('undo-settings');wait(lambda:live()['settings']['worldSeed']==1);check(True,'a second settings Undo restores the original seed')
  click('redo-settings');wait(lambda:live()['settings']['worldSeed']==17);click('redo-settings');wait(lambda:live()['settings']['worldSeed']==29);check(True,'settings Redo survives the Undo button action and restores both edits')
  click('undo-settings');wait(lambda:live()['settings']['worldSeed']==17);click('undo-settings');wait(lambda:live()['settings']['worldSeed']==1)
 if a.preset!='hospital':click('preset');click('preset/'+a.preset);wait(lambda:live()['settings']['presetId']==a.preset)
 click('generate');wait(lambda:live()['candidates'] and not live()['busy']);check(not live()['error'],'actual UI generates accepted candidates without mutating the scene')
 generated=save('wa-generated');check(planes(generated)==baseline,'candidate generation and preview leave authored brushes unchanged')
 check(live()['world']['valid'] and live()['world']['reachable']==live()['world']['navigationNodes'],'chosen candidate passes the physical hull navigation oracle')
 click('candidate/1');wait(lambda:live()['selected']==1);chosen=live()['world']['seed']
 click('tab/3');saved_project=out/'saved.world.json';click('save-project');choose(saved_project);wait(saved_project.exists);saved=json.loads(saved_project.read_text())
 check(saved['selectedCandidateSeed']==chosen and len(saved['registry']['presets'])==8,'project saves the selected candidate and complete preset registry snapshot')
 click('open-project');choose(saved_project);wait(lambda:live()['candidates']==0);click('generate');wait(lambda:live()['candidates'] and not live()['busy'])
 check(live()['world']['seed']==chosen,'project reopen and regeneration restore the exact selected candidate')
 click('tab/0');time.sleep(1);desktop=ImageGrab.grab().convert('RGB');desktop.save(out/'world-preview.png')
 preview=live()['widgets']['preview'];ox,oy=ui.position();sx,sy=getattr(ui,'scale',(1,1));box=(int(ox+preview['x']*sx),int(oy+preview['y']*sy),int(ox+(preview['x']+preview['width'])*sx),int(oy+(preview['y']+preview['height'])*sy))
 visible=sum(g>r*1.01 and g>b*1.02 and 50<g<210 for r,g,b in desktop.crop(box).getdata())
 check(visible>1000,'3D preview contains actual textured architecture instead of a missing render target')
 click('tab/1');time.sleep(.5);ImageGrab.grab().save(out/'world-site-plan.png')
 click('tab/2');time.sleep(.5);ImageGrab.grab().save(out/'world-quality.png')
 click('tab/4');exported=out/'exported/world.map';exported.parent.mkdir(exist_ok=True);click('export');choose(exported);wait(exported.exists)
 check('"worldspawn"' in exported.read_text() and (exported.parent/'world.world.json').exists(),'export writes actual brush MAP and a versioned editable project')
 check(any(exported.parent.glob('world-architect-*.wad')) and any((exported.parent/'textures').glob('*.tga')),'portable export includes real original WAD and texture assets')
 if a.preset=='metro':check('surfaceparm water' in (exported.parent/'scripts/world-architect-metro.shader').read_text() and any(name in exported.read_text() for name in ['wa_water_metro','*wametro','!wametro']),'flooded platform exports its liquid MAP material and shader assets')
 click('apply');wait(lambda:'One map Undo' in live()['message']);check(not live()['request'],'Apply waits for an executed QC map transaction acknowledgment')
 applied=save('wa-applied');check(len(planes(applied))>len(baseline)+100,'Apply inserts editable generated brushes')
 check(all(line in planes(applied) for line in baseline),'Apply retains every original authored brush plane')
 console('ftebroom_undo');undone=save('wa-undone');check(planes(undone)==baseline,'one Undo restores the original scene')
 console('ftebroom_redo');redone=save('wa-redone');check(planes(redone)==planes(applied),'one Redo restores the complete generated world')
 token='99999999999999999999';console('ftebroom_world_apply '+token+' -1 '+str(live()['settings']['profileId']));wait(lambda:live()['applyAck']==token)
 check(live()['applyStatus']=='stale' and planes(save('wa-stale'))==planes(redone),'execution-time epoch guard rejects a stale request without changing the map')
 console('ftebroom_world_apply ../unsafe 0 0');wait(lambda:live()['applyAck']=='../unsafe');check(live()['applyStatus']=='invalid','import protocol rejects path-shaped request tokens')
 token='7777777777777777777';invalid=data/'data'/('wa-import-'+token+'.map');invalid.parent.mkdir(exist_ok=True);invalid.write_text('not a MAP')
 console('fs_flush');console('ftebroom_world_apply '+token+' '+str(live()['mapEpoch'])+' '+str(live()['settings']['profileId']));wait(lambda:live()['applyAck']==token)
 check(live()['applyStatus']=='parse-failed' and planes(save('wa-invalid'))==planes(redone),'invalid staged MAP is rejected atomically')
 console('ftebroom_undo');check(planes(save('wa-after-rejections-undo'))==baseline,'rejected imports do not create extra map Undo transactions')
 console('ftebroom_redo');check(planes(save('wa-after-rejections-redo'))==planes(applied),'rejected imports preserve the previous complete Redo transaction')
 if a.preset=='hospital':
  site_seed=live()['world']['stageSeeds']['Site'];click('freeze');click('freeze/3');wait(lambda:all(live()['settings']['stageLocks'][:3]));seed(41);click('generate');wait(lambda:live()['candidates'] and not live()['busy'])
  check(not live()['error'] and live()['world']['stageSeeds']['Site']==site_seed,'freezing architecture retains the selected site while later stages use a new seed')
  previous_seed=live()['world']['seed'];seed(42);click('generate');wait(lambda:not live()['busy'] and live()['world']['seed']!=previous_seed)
  check(set(['Mission','Site','Architecture'])<=set(live()['world']['cachedStages']),'regeneration reuses the actual frozen prefix cache')
  locked_seed=live()['world']['seed'];locked_project=out/'locked.world.json';click('tab/3');click('save-project');choose(locked_project);wait(locked_project.exists);click('open-project');choose(locked_project);wait(lambda:live()['candidates']==0);click('generate');wait(lambda:live()['candidates'] and not live()['busy'])
  check(live()['world']['seed']==locked_seed and all(live()['settings']['stageLocks'][:3]),'saved project restores frozen stage recipes and its selected candidate')
  click('freeze');click('freeze/0');wait(lambda:not any(live()['settings']['stageLocks']));check(planes(save('wa-frozen-preview'))==planes(applied),'freeze, regeneration and project replay leave authored map geometry unchanged')
 for path in [before,generated,applied,undone,redone]:shutil.copy2(path,out/path.name)
 (out/'world-state.json').write_text(json.dumps(live(),indent=2));print('World Architect UI:',len(checks),'checks passed',flush=True)
finally:
 try:ImageGrab.grab().save(out/'last-desktop.png');(out/'last-state.json').write_text(json.dumps(live(),indent=2))
 except Exception:pass
 if proc.poll() is None:
  proc.terminate()
  try:proc.wait(timeout=5)
  except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=10)
 stream.close()
