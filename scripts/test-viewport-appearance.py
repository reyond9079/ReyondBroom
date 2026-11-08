#!/usr/bin/env python3
"""Real OS-input regression for viewport colors/layering, shortcuts and profile order.

Uses a disposable runtime. --capture-only records the unchanged baseline without
asserting the new behavior. No external engine is launched by this suite.
"""
import argparse, hashlib, json, os, shutil, subprocess, time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput, read_ui_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path)
p.add_argument('--capture-only',action='store_true');p.add_argument('--theme',choices=['dark','light'],default='dark')
p.add_argument('--width',type=int,default=1920);p.add_argument('--height',type=int,default=1080)
a=p.parse_args();root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
data=root/'ftebroom';state=data/'compilation-ui-live.json';store=data/'compilation-profiles.json'
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
(data/'ftebroom-preferences.cfg').unlink(missing_ok=True)
profiles=[{'name':n+' compile','workdir':'${MAP_DIR_PATH}','trenchBroom':True,'tasks':[]} for n in ['Zulu','alpha','beta']]
engines=[{'id':str(i),'name':n+' engine','path':'unused-engine-'+str(i),'parameters':'argument-'+str(i)} for i,n in enumerate(['Zulu','alpha','beta'])]
workspace={'profiles':profiles,'engines':engines,'variables':{}}
store.write_text(json.dumps({'version':1,'games':{k:workspace for k in ['quake3','when-the-sky-went-dark','quake1','quake2','halflife']}}))
checks=[];metrics={}
def expect(ok,name):
 assert ok,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
def live():return read_ui_json(state)
def wait(fn,timeout=40):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  assert process.poll() is None,'Editor exited'
  try:
   if value:=fn():return value
  except (FileNotFoundError,PermissionError,KeyError,json.JSONDecodeError):pass
  time.sleep(.08)
 raise AssertionError('Appearance QA timed out')
def start(label):
 global process,stream,ui
 state.unlink(missing_ok=True)
 (data/'appearance-qa.cfg').write_text('exec qa-clip.cfg\nset ftebroom_input_qa 1\nin 12 ftebroom_test_desktop reyon8qa '+a.theme+'\n')
 engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64')
 stream=(out/(label+'.log')).open('wb')
 process=subprocess.Popen([str(engine),*render,'+set','ftebroom_input_qa','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width',str(a.width),'-height',str(a.height),'-basedir',str(root),'+set','ftebroom_skip_setup','1','+exec','appearance-qa.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
 wait(lambda:live()['views'] and live()['game'])
 ui=NativeInput(process,state,live,wait,out,focus_required=True);ui.position()
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 ui.move(600,300);ui.settle()
def stop():
 process.kill();process.wait(timeout=15);stream.close()
def click(name,global_=False):
 r=wait(lambda:live()['globalWidgets' if global_ else 'widgets'].get(name));w={'x':0,'y':0} if global_ else live()['widgets']['window']
 ui.click(w['x']+r['x']+r['width']*.5,w['y']+r['y']+r['height']*.5)
def shot(name):
 ui.settle();img=ImageGrab.grab();img.save(out/(name+'.png'));return img
def count(img,kind,predicate):
 ox,oy=ui.position();sx,sy=getattr(ui,'scale',(1,1));total=0
 for v in live()['views']:
  if (kind=='2d')!=(v['type']!=0):continue
  area=img.crop((int(ox+(v['x']+12)*sx),int(oy+(v['y']+42)*sy),int(ox+(v['x']+v['width']-12)*sx),int(oy+(v['y']+v['height']-12)*sy)))
  total+=sum(predicate(c[:3]) for c in area.getdata())
 return total
def saved():return read_ui_json(store)['games'][live()['game']]
def edit_global(key,text):
 click(key,True);ui.key('a',('ctrl',));ui.text(text);ui.key('enter')
def rgb(setting,values):
 r=wait(lambda:live()['globalWidgets'].get('preference-color-'+setting));ui.click(r['x']+25,r['y']+r['height']*.5,mods=('ctrl',))
 for i,value in enumerate(values):
  ui.key('a',('ctrl',));ui.text(str(value));ui.key('tab' if i<2 else 'enter')
def export(name):
 target=data/'maps'/(name+'.map');target.unlink(missing_ok=True);ui.console('mod_terrain_save "" '+name);wait(lambda:target.exists() and target.stat().st_size>50);return target.read_text(errors='replace')
try:
 start('editor')
 ui.console('ftebroom_deselect');ui.console('set ftebroom_grid_alpha 1');ui.console('set ftebroom_color_brush_edges "1 0 1"')
 ui.console('set ftebroom_grid_color "1 1 1"');white=shot('grid-white')
 metrics['white_grid_2d']=count(white,'2d',lambda c:min(c)>235)
 if not a.capture_only:expect(metrics['white_grid_2d']>1000,'white_grid_is_white_in_2d')
 if not a.capture_only:expect(count(white,'3d',lambda c:min(c)>235)>1000,'white_grid_is_white_in_3d')
 magenta=lambda c:c[0]>230 and c[1]<25 and c[2]>230
 metrics['edges_grid_on']=count(white,'2d',magenta)
 ui.console('set ftebroom_grid_visible 0');no_grid=shot('edges-grid-off');metrics['edges_grid_off']=count(no_grid,'2d',magenta)
 if not a.capture_only:expect(metrics['edges_grid_off']>100 and metrics['edges_grid_on']>=metrics['edges_grid_off']*.95,'grid_never_covers_brush_edges')
 ui.console('set ftebroom_grid_visible 1');ui.console('set ftebroom_color_2d_background "0.6 0.6 0.6"');bright=shot('grid-white-bright-background')
 if not a.capture_only:expect(count(bright,'2d',lambda c:min(c)>235)>1000,'white_grid_independent_of_background_color')
 ui.console('set ftebroom_color_2d_background "0.188235294 0.196078431 0.219607843"');ui.console('set ftebroom_grid_color "0 0 0"');black=shot('grid-black')
 metrics['black_grid_2d']=count(black,'2d',lambda c:max(c)<6)
 if not a.capture_only:expect(metrics['black_grid_2d']>1000,'black_grid_is_black_in_2d')
 ui.console('set ftebroom_grid_color "1 0 0"');red=shot('grid-red')
 if not a.capture_only:expect(count(red,'2d',lambda c:c[0]>235 and c[1]<20 and c[2]<20)>1000,'red_grid_preserves_rgb')
 ui.console('set ftebroom_grid_color "1 1 1"');ui.console('set ftebroom_view_cache_disable 1');direct=shot('grid-direct')
 if not a.capture_only:expect(count(direct,'2d',lambda c:min(c)>235)>1000,'uncached_grid_uses_same_color')
 if not a.capture_only:expect(count(direct,'2d',magenta)>=metrics['edges_grid_on']*.95,'cached_and_uncached_edges_keep_strength')
 ui.console('set ftebroom_view_cache_disable 0')
 if not a.capture_only:
  metrics['edge_refresh_samples']=[]
  for i in range(4):
   time.sleep(.6);ui.settle();metrics['edge_refresh_samples'].append(count(ImageGrab.grab(),'2d',magenta))
  expect(min(metrics['edge_refresh_samples'])>=metrics['edges_grid_on']*.95,'edges_stable_across_periodic_cache_refresh')
 ui.console('set ftebroom_clear_sky 0');flat=shot('sky-off-gray')
 metrics['flat_gray_3d']=count(flat,'3d',lambda c:max(abs(x-38) for x in c)<3)
 if not a.capture_only:expect(metrics['flat_gray_3d']>10000,'sky_off_has_dark_gray_background')
 if not a.capture_only:
  # Reorder real persisted profiles using the visible controls.
  click('menu/Run',True);click('Menu/Run/Compile...',True);click('profile/0');click('profile-down')
  expect([x['name'] for x in saved()['profiles']]==['alpha compile','Zulu compile','beta compile'] and live()['selected']==1,'compile_move_keeps_selected_profile')
  click('profile-sort');expect([x['name'] for x in saved()['profiles']]==['alpha compile','beta compile','Zulu compile'] and live()['selected']==2,'compile_sort_keeps_selected_identity')
  shot('compile-order');click('close');click('menu/Run',True);click('Menu/Run/Launch...',True)
  click('launch-engine/0');click('launch-engine-down');click('launch-engine-sort')
  expect([x['id'] for x in saved()['engines']]==['1','2','0'],'launch_engine_order_saved')
  shot('launch-order');click('launch-configure');click('engine/2');click('engine-up');click('engine-manager-close')
  expect([x['id'] for x in saved()['engines']]==['1','0','2'],'engine_manager_order_shared')
  expect(wait(lambda:live()['widgets'].get('launch-engine/1',{}).get('selected')),'engine_manager_reorder_keeps_launch_selection')
  # The launch Close has its own rectangle, unlike the manager Close.
  click('launch-close') if 'launch-close' in live()['widgets'] else None
  click('menu/View',True);click('menu-preferences',True);click('preferences-keyboard-tab',True)
  actions=[('func_detail','Menu/Edit/Create func_detail','j'),('FUNC_GROUP','Menu/Edit/Create func_group','k'),('WORLD TEXTURE ALIGNMENT','Menu/Textures/Reset to world aligned','l')]
  for query,action,key in actions:
   edit_global('keyboard-search',query);click('keyboard-binding/'+action,True)
   wait(lambda:live()['globalWidgets'].get('keyboard-capture',{}).get('action')==action)
   ui.key(key,('ctrl','alt','shift'))
   wait(lambda:live()['globalWidgets'].get('keyboard-capture',{}).get('action')=='')
  shot('keyboard-world-alignment');click('preferences-colors-tab',True);rgb('ftebroom_color_3d_background',[45,45,45]);rgb('ftebroom_grid_color',[170,170,170]);click('preferences-close',True)
  prefs=(data/'ftebroom-preferences.cfg').read_text()
  expect(all('Ctrl+Alt+Shift+'+key in prefs for key in ['j','k','l']),'three_shortcuts_assigned_and_saved')
  ui.console('ftebroom_box 320 0 64 64 64 64');ui.console('set ftebroom_tool 0');ui.console('set ca_brush_view 0')
  v=live()['views'][0];ui.move(v['x']+v['width']*.5,v['y']+v['height']*.5);ui.key('j',('ctrl','alt','shift'))
  expect('"classname" "func_detail"' in export('appearance-detail'),'func_detail_shortcut_creates_real_brush_entity')
  ui.console('ftebroom_undo');ui.key('k',('ctrl','alt','shift'))
  expect('"_tb_type" "_tb_group"' in export('appearance-group'),'func_group_shortcut_creates_editable_group')
  ui.console('ftebroom_undo');ui.console('ftebroom_texture_key rotate 1 0');rotated=export('appearance-rotated')
  ui.key('l',('ctrl','alt','shift'));aligned=export('appearance-world')
  import re
  axes=re.findall(r'\[\s*([-\d.e+]+)\s+([-\d.e+]+)\s+([-\d.e+]+)\s+([-\d.e+]+)\s*\]',aligned,re.I)
  expect(aligned!=rotated and axes and all(sum(abs(float(v))>.001 for v in axis[:3])==1 for axis in axes),'world_alignment_shortcut_restores_axial_uvs')
  wanted=saved();stop();start('restart')
  expect([x['name'] for x in saved()['profiles']]==[x['name'] for x in wanted['profiles']] and [x['id'] for x in saved()['engines']]==[x['id'] for x in wanted['engines']],'restart_keeps_both_profile_orders')
  colors=live()['viewportColors'];expect(all(abs(float(x)-45/255)<.001 for x in colors['ftebroom_color_3d_background'].split()) and all(abs(float(x)-170/255)<.001 for x in colors['ftebroom_grid_color'].split()),'restart_keeps_grid_and_3d_background_colors')
 stop()
except BaseException:
 # Preserve the failed screen and read-only state instead of losing the
 # evidence when a headless host cannot draw or acknowledge input frames.
 try:ImageGrab.grab().save(out/'failure.png')
 except Exception:pass
 for path in [state,data/'native-input-live.json',data/'qconsole.log',root/'qconsole.log']:
  try:shutil.copy2(path,out/('failure-'+path.name))
  except OSError:pass
 raise
finally:
 if 'process' in globals() and process.poll() is None:process.kill();process.wait(timeout=15)
 if 'stream' in globals():stream.close()
 (out/'metrics.json').write_text(json.dumps(metrics,indent=2))
 module=root/('fteplug_ftebroom_x64.dll' if os.name=='nt' else 'fteplug_ftebroom_amd64.so')
 (out/'provenance.json').write_text(json.dumps({'build_info':(root/'build-info.txt').read_text(),'module_sha256':hashlib.sha256(module.read_bytes()).hexdigest(),'capture_only':a.capture_only,'theme':a.theme,'window':[a.width,a.height],'renderer_requested':os.environ.get('RB_QA_RENDERER','default'),'method':'Real OS input; screenshots/pixel checks; process/console focus acknowledgment; no external game-engine launch'},indent=2))
