#!/usr/bin/env python3
"""REYON-8: actual UI color/persistence and compile -> engine process acceptance.

The portable engine executables are explicitly argv/process probes, not claims
of a real VKQuake/Ironwail run. Use --engine to additionally launch an installed
real Quake engine with licensed game data under --game-root (id1 + mod).
"""
import argparse, json, os, shutil, subprocess, sys, time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path)
p.add_argument('--engine',type=Path);p.add_argument('--game-root',type=Path);p.add_argument('--mod',default='lq1')
p.add_argument('--probe',type=Path,help='Freshly built process probe; no test tool is installed into the distribution')
a=p.parse_args();root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[1];data=root/'ftebroom';statepath=data/'compilation-ui-live.json'
fixture=out/'Quake fixture with spaces';subprocess.run([sys.executable,str(repo/'tests/make-quake1-fixture.py'),str(fixture)],cwd=repo,check=True)
for folder in [root,data]:
 for name in ['ftebroom-project.txt','ftebroom-game-profile.txt','ftebroom-quake1.txt']:
  shutil.copy2(fixture/'editor/ftebroom'/name,folder/name)
mapname='rb_engine_test';source=data/'maps'/(mapname+'.map')
text=(fixture/'editor/ftebroom/maps/build-fixture.map').read_text();text=text.replace('"classname" "worldspawn"','"classname" "worldspawn"\n"wad" '+json.dumps((fixture/'id1/test textures.wad').as_posix()),1);source.write_text(text)
work=out/'Build with spaces';work.mkdir(exist_ok=True)
game=(a.game_root.resolve() if a.game_root else fixture);mod=a.mod if a.game_root else 'custom mod'
(game/mod).mkdir(exist_ok=True)
probe=a.probe.resolve() if a.probe else root/('tools/compilation-profile-probe.exe' if os.name=='nt' else 'tools/compilation-profile-probe')
launchdir=out/'Engine executable with spaces';launchdir.mkdir(exist_ok=True)
engines=[];profiles=[]
names=['Real installed engine'] if a.engine else ['VKQuake argv probe','Ironwail argv probe']
for i,name in enumerate(names):
 executable=a.engine.resolve() if a.engine else launchdir/('engine '+str(i)+('.exe' if os.name=='nt' else ''))
 if not a.engine:shutil.copy2(probe,executable)
 args='-basedir "${GAME_DIR_PATH}" -game "${MODS[-1]}" +map "${MAP_BASE_NAME}"'
 if a.engine:args='-window -width 960 -height 640 -nosound -condebug '+args
 else:args='record "'+(out/f'receipt-{i}.json').as_posix()+'" '+args
 e={'id':str(i),'name':name,'path':executable.as_posix(),'parameters':args,'workdir':launchdir.as_posix(),'mapFile':'${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp'}
 engines.append(e)
 profiles.append({'name':'Compile + '+name,'workdir':work.as_posix(),'trenchBroom':True,'tasks':[
  {'type':'export','target':'${WORK_DIR_PATH}/${MAP_BASE_NAME}.map','stripTbProperties':True},
  {'type':'tool','tool':(root/('tools/ericw/qbsp.exe' if os.name=='nt' else 'tools/ericw/qbsp')).as_posix(),'parameters':'-bsp29 "${WORK_DIR_PATH}/${MAP_BASE_NAME}.map"','treatNonZeroResultCodeAsError':True},
  {'type':'launchEngine','engineProfileId':str(i),'treatLaunchFailureAsError':True}]})
workspace={'profiles':profiles,'engines':engines,'variables':{},'gameRoot':game.as_posix(),'baseMod':'id1','enabledMods':[mod],'modsConfigured':True}
store=data/'compilation-profiles.json';store.write_text(json.dumps({'version':1,'games':{'quake1':workspace}}))
prefs=data/'ftebroom-preferences.cfg';prefs.unlink(missing_ok=True)
config=data/'reyon8-ui.cfg'
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64')
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
checks=[]
for receipt in out.glob('receipt-*.json'):receipt.unlink()
def expect(ok,name):
 assert ok,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
def live():return json.loads(statepath.read_text(encoding='utf-8'))
def start(label):
 global process,stream,ui
 statepath.unlink(missing_ok=True)
 config.write_text('set ftebroom_skip_setup 1\nexec ftebroom.cfg\nin 7 mapedit maps/'+mapname+'.map\nin 10 ftebroom_compile_mods '+json.dumps(mod)+'\nin 10 setpos 320 -400 280 25 130 0\nin 12 ftebroom_test_desktop reyon8qa\n')
 stream=(out/(label+'.log')).open('wb')
 process=subprocess.Popen([str(engine),*render,'+set','ftebroom_skip_setup','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width','1920','-height','1080','-basedir',str(root),'+exec',config.name],cwd=root,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'},stdout=stream,stderr=subprocess.STDOUT)
 def wait(fn,timeout=30):
  end=time.monotonic()+timeout
  while time.monotonic()<end:
   assert process.poll() is None,(label,'editor exited')
   try:
    value=fn()
    if value:return value
   except (FileNotFoundError,PermissionError,KeyError,json.JSONDecodeError):pass
   time.sleep(.1)
  raise AssertionError('Timed out: '+label)
 globals()['wait']=wait
 wait(lambda:live()['game']=='quake1' and len(live()['views'])==4)
 ui=NativeInput(process,statepath,live,wait,out);ui.position()
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 ui.move(600,300);ui.settle()
def stop():
 process.terminate();process.wait(timeout=15);stream.close()
def click(name,global_=False):
 r=wait(lambda:live()['globalWidgets' if global_ else 'widgets'].get(name));w={'x':0,'y':0} if global_ else live()['widgets']['window']
 ui.click(w['x']+r['x']+r['width']*.5,w['y']+r['y']+r['height']*.5)
def preferences():
 click('menu/View',True);click('menu-preferences',True);click('preferences-colors-tab',True)
def rgb(setting,values):
 r=wait(lambda:live()['globalWidgets'].get('preference-color-'+setting))
 ui.click(r['x']+25,r['y']+r['height']*.5,mods=('ctrl',))
 for i,value in enumerate(values):
  ui.key('a',('ctrl',));ui.text(str(value));ui.key('tab' if i<2 else 'enter')
def shot(name):ui.settle();ImageGrab.grab().save(out/(name+'.png'))
def channels(name):return [float(x) for x in live()['viewportColors'][name].split()]
def pixel_count(predicate,kind):
 image=ImageGrab.grab();total=0
 for v in live()['views']:
  if (kind=='2d')!=(v['type']!=0):continue
  area=image.crop((int(v['x'])+6,int(v['y'])+35,int(v['x']+v['width'])-6,int(v['y']+v['height'])-6))
  total+=sum(predicate(rgb[:3]) for rgb in area.getdata())
 return total
try:
 start('editor')
 expect(all(abs(x-y)<.001 for x,y in zip(channels('ftebroom_color_2d_background'),[48/255,50/255,56/255])),'fresh_2d_default_303238')
 expect(pixel_count(lambda c:max(abs(c[i]-[48,50,56][i]) for i in range(3))<3,'2d')>100000,'all_2d_views_render_gray')
 shot('2d-default-dark')
 # Open the real preference dialog and edit RGB fields with native OS input.
 preferences();rgb('ftebroom_color_brush_edges',[0,255,255])
 click('preferences-close',True);shot('after-color-picker')
 expect(channels('ftebroom_color_brush_edges')==[0,1,1],'color_picker_changes_shared_preference')
 expect(pixel_count(lambda c:c[0]<30 and c[1]>220 and c[2]>220,'2d')>100,'2d_edges_change_immediately')
 expect(pixel_count(lambda c:c[0]<30 and c[1]>220 and c[2]>220,'3d')>50,'3d_edges_change_immediately')
 shot('brush-edges-cyan')
 preferences();rgb('ftebroom_color_2d_background',[31,41,51]);click('preferences-close',True)
 expect(prefs.exists() and 'ftebroom_color_brush_edges "0 1 1"' in prefs.read_text(),'viewport_colors_saved')
 stop();start('restart')
 expect(channels('ftebroom_color_brush_edges')==[0,1,1] and all(abs(x-y/255)<.001 for x,y in zip(channels('ftebroom_color_2d_background'),[31,41,51])),'restart_keeps_custom_colors')
 expect(channels('ftebroom_color_selected')==[1,0,0] and channels('ftebroom_color_locked')==[.15,.4,1],'selection_locked_colors_preserved')
 click('menu/Run',True);click('Menu/Run/Compile...',True)
 for i,name in enumerate(names):
  # Select each persisted profile, then press its actual Compile button.
  click('profile/'+str(i))
  click('compile')
  wait(lambda:live()['running'] or live()['error'] or 'Completed successfully.' in live()['log']);wait(lambda:not live()['running'],90)
  expect(not live()['error'],name+'_pipeline_no_error: '+live()['error'])
  expect((game/mod/'maps'/(mapname+'.bsp')).is_file(),name+'_map_deployed')
  if not a.engine:
   receipt=out/f'receipt-{i}.json';wait(lambda:receipt.exists());observed=json.loads(receipt.read_text())
   expect(observed['cwd'].replace('\\','/')==launchdir.as_posix() and observed['argv'][3:]==['-basedir',game.as_posix(),'-game',mod,'+map',mapname],name+'_actual_argv_and_working_directory')
  shot('compiled-engine-'+str(i))
 saved=json.loads(store.read_text())['games']['quake1']['engines']
 expect(saved==engines,'engine_path_arguments_workdir_map_survive_restart')
 click('close');click('menu/Run',True);click('Menu/Run/Launch...',True);click('launch-configure');click('engine/0');shot('engine-configuration')
 if a.engine:
  print('REAL_ENGINE_STARTED',a.engine,flush=True)
  time.sleep(8);ImageGrab.grab().save(out/'actual-engine.png')
 stop()
finally:
 if 'process' in globals() and process.poll() is None:process.terminate();process.wait(timeout=15)
 if 'stream' in globals():stream.close()
