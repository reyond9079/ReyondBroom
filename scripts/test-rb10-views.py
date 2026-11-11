#!/usr/bin/env python3
"""RB-10 View Options, independent grid plane and Tools: actual OS input.

Pass a disposable packaged runtime. Run at 1920x1080 and 1024x768 in both themes.
The read-only live state locates controls and observes cvars; it never performs
an action. No console command is used to change tested settings or UI state.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from PIL import ImageGrab
from editor_os_input import NativeInput, read_ui_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path)
p.add_argument('--width',type=int,default=1920);p.add_argument('--height',type=int,default=1080)
p.add_argument('--theme',choices=['dark','light'],default='dark')
a=p.parse_args();root=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
data=root/'ftebroom';state=data/'compilation-ui-live.json';prefs=data/'ftebroom-preferences.cfg'
checks=[];metrics={};process=None;stream=None
prefs.unlink(missing_ok=True)
assert (data/'qa-clip.cfg').is_file(),'Run tests/make-linux-build-fixture.py on this disposable runtime first'

def live():return read_ui_json(state)
def wait(fn,timeout=45):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  assert process.poll() is None,'Editor exited; inspect editor log'
  try:
   value=fn()
   if value:return value
  except (FileNotFoundError,PermissionError,KeyError,json.JSONDecodeError):pass
  time.sleep(.08)
 raise AssertionError('View/Tools QA timed out')
def widgets():return live()['globalWidgets']
def settings():return widgets()['view-settings']
def check(value,name):
 assert value,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
def numeric(name):return float(settings()[name])
def saved(name):
 text=prefs.read_text();match=re.search(r'^seta '+re.escape(name)+r' (".*")$',text,re.M)
 assert match,'Not persisted: '+name
 return json.loads(match[1])
def expect_value(name,value):
 wait(lambda:numeric(name)==value)
 wait(lambda:float(saved(name))==value)
def start(label):
 global process,stream,ui
 state.unlink(missing_ok=True)
 (data/'rb10-views-qa.cfg').write_text('exec qa-clip.cfg\nset ftebroom_input_qa 1\nin 12 ftebroom_test_desktop reyon8qa '+a.theme+'\n')
 engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64')
 renderer=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
 stream=(out/(label+'.log')).open('wb')
 process=subprocess.Popen([str(engine),*renderer,'+set','ftebroom_input_qa','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width',str(a.width),'-height',str(a.height),'-basedir',str(root),'+set','ftebroom_skip_setup','1','+exec','rb10-views-qa.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
 wait(lambda:widgets().get('view-settings') and widgets().get('tools-layout'))
 ui=NativeInput(process,state,live,wait,out,focus_required=True);ui.position()
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 ui.move(600,300);ui.settle()
def stop():
 process.kill();process.wait(timeout=15);stream.close()
def shot(name):
 ui.move(a.width-4,a.height-4);ui.settle();ImageGrab.grab().save(out/(name+'.png'));(out/(name+'.json')).write_text(json.dumps(live(),indent=2))
def click(key,region=None):
 ui.settle()
 for _ in range(45):
  w=widgets();r=w.get(key)
  if region:
   area=w[region];top=area['y']+10;bottom=area['y']+area['height']-10
   cy=r['y']+r['height']*.5 if r else bottom+1
   if r and r.get('visible',True) and top<cy<bottom:break
   ui.move(area['x']+area['width']*.72,area['y']+area['height']*.5)
   ui.scroll(3 if r and cy<=top else -3)
  elif r:break
  else:wait(lambda:widgets().get(key))
 else:raise AssertionError('Control not reachable by scrolling: '+key)
 r=widgets()[key];ui.click(r['x']+r['width']*.5,r['y']+r['height']*.5);ui.settle()
def open_options():
 views=live()['views'];indices=[i for i,v in enumerate(views) if v['type']==0]
 check(bool(indices),'3D viewport is available')
 for i in indices:
  r=widgets()['view-options-button-'+str(i)];v=views[i]
  check(v['x']+v['width']-r['x']-r['width']<20 and r['y']-v['y']<20,'View Options is at top-right of viewport '+str(i))
 click('view-options-button-'+str(indices[0]));wait(lambda:widgets().get('view-options-display-region'))
def tools_fits():
 t=widgets()['tools-layout']
 return t if t['horizontal'] and t['scrollX']<1 and t['scrollY']<1 and abs(t['height']-t['contentHeight'])<3 else None

try:
 start('editor')
 check(numeric('ftebroom_grid_plane')==0,'fresh defaults disable the independent horizontal grid plane')
 collapsed=wait(tools_fits)['height'];metrics['tools_collapsed_height']=collapsed
 check(widgets()['tools-layout']['y']<min(v['y'] for v in live()['views']),'new workspace docks horizontal Tools above the scene')
 click('tools-extra-toggle');expanded=wait(tools_fits)['height']
 check(expanded>=collapsed,'extra tools wrap without clipping or reducing the required height')
 click('tools-shape-toggle');wait(lambda:widgets().get('tools/shape-choice'))
 shape=wait(lambda:(t if (t:=tools_fits()) and t['height']>collapsed else None))
 metrics['tools_expanded_height']=shape['height'];shot('tools-expanded')
 check(shape['height']>collapsed,'expanded shape controls automatically grow the horizontal Tools dock')
 click('tools-shape-toggle');wait(lambda:'tools/shape-choice' not in widgets());click('tools-extra-toggle')
 wait(lambda:(t:=tools_fits()) and abs(t['height']-collapsed)<3)
 check(True,'collapsing Tools reclaims its original height');shot('tools-collapsed')
 open_options();display='view-options-display-region'
 bools=['ftebroom_show_entity_names','ftebroom_show_group_bounds','ftebroom_show_brush_entity_bounds','ftebroom_show_entity_bounds','ftebroom_show_points','ftebroom_show_models','ftebroom_show_brushes','ftebroom_show_patches','ftebroom_shade_faces','r_fog_enabled','ftebroom_show_edges','ftebroom_bounds_visible']
 if a.width>=1600:
  area=widgets()[display]
  base=[k for k in widgets() if k.startswith(('view-tag-','view-render-mode-','view-entity-links-'))]+['view-option-'+name for name in bools]+['view-options-restore']
  check(all((r:=widgets()[key]).get('visible',True) and area['y']<=r['y'] and r['y']+r['height']<=area['y']+area['height'] for key in base),'all base TrenchBroom View Options fit without scrolling at desktop size')
 shot('view-options-defaults')
 for name in bools:
  before=numeric(name);click('view-option-'+name,display);expect_value(name,1-int(bool(before)))
  check(True,'visible checkbox changes and persists '+name)
 tags=['trigger','clip','caulk','hint','detail','liquid','translucent','areaportal','clusterportal']
 for i,tag in enumerate(tags):
  before=int(numeric('ftebroom_hidden_tags'));click('view-tag-'+tag,display);expect_value('ftebroom_hidden_tags',before^(1<<i))
 check(True,'all nine brush-tag visibility controls toggle their independent saved bits')
 for mode in [1,2,0]:click('view-render-mode-'+str(mode),display);expect_value('ftebroom_render_faces',mode)
 check(True,'all three material/face rendering modes are selectable and saved')
 for mode in [3,2,1,0]:click('view-entity-links-'+str(mode),display);expect_value('ftebroom_entity_links',mode)
 check(True,'all four entity-link modes are selectable and saved')
 names=[k for k in widgets() if k.startswith('view-definition-')]
 key=next((k for k in names if k=='view-definition-info_player_start'),names[0]);classname=key.removeprefix('view-definition-')
 click(key,'view-options-classes-region');token='|'+classname+'|'
 wait(lambda:token in settings()['ftebroom_hidden_classes'] and token in saved('ftebroom_hidden_classes'))
 check(True,'entity-definition visibility changes and persists an individual class')
 classes=[key.removeprefix('view-definition-') for key in names]
 click('view-options-hideall')
 wait(lambda:all('|'+name+'|' in settings()['ftebroom_hidden_classes'] and '|'+name+'|' in saved('ftebroom_hidden_classes') for name in classes))
 metrics['hidden_classes_bytes']=len(saved('ftebroom_hidden_classes'))
 check(metrics['hidden_classes_bytes']>256,'Hide all persists the complete entity-class list beyond 256 bytes')
 click('view-options-restore',display)
 for name in bools:expect_value(name,0 if name=='r_fog_enabled' else 1)
 expect_value('ftebroom_render_faces',0);expect_value('ftebroom_entity_links',1)
 check(True,'Restore Defaults restores renderer visibility controls and link/material modes')
 click('view-options-additional',display);click('view-option-ftebroom_grid_plane',display);expect_value('ftebroom_grid_plane',1)
 check(True,'horizontal grid plane enables through its own saved View Options checkbox');shot('view-options-grid-plane')
 area=widgets()[display];ui.move(area['x']+area['width']*.7,area['y']+area['height']*.5);ui.scroll(-20);shot('view-options-advanced')
 click('view-options-close');wait(lambda:'view-options-close' not in widgets());wanted=settings();stop();start('restart')
 check(numeric('ftebroom_grid_plane')==1 and settings()['ftebroom_hidden_classes']==wanted['ftebroom_hidden_classes'],'restart keeps grid-plane and entity-definition visibility choices')
 check(all('|'+name+'|' in settings()['ftebroom_hidden_classes'] for name in classes),'restart keeps every class hidden after Hide all without truncation')
 for name in bools+['ftebroom_render_faces','ftebroom_entity_links','ftebroom_hidden_tags']:
  check(settings()[name]==wanted[name],'restart keeps '+name)
 open_options();click('view-options-showall');wait(lambda:settings()['ftebroom_hidden_classes']=='' and saved('ftebroom_hidden_classes')=='')
 check(True,'Show all restores all entity-definition visibility and saves it')
 click('view-options-additional',display);click('view-option-ftebroom_grid_plane',display);expect_value('ftebroom_grid_plane',0)
 check(True,'horizontal grid plane can be independently disabled again');shot('view-options-final');stop()
except BaseException:
 try:ImageGrab.grab().save(out/'failure.png')
 except Exception:pass
 for path in [state,data/'native-input-live.json',data/'qconsole.log',root/'qconsole.log']:
  try:shutil.copy2(path,out/('failure-'+path.name))
  except OSError:pass
 raise
finally:
 if process is not None and process.poll() is None:process.kill();process.wait(timeout=15)
 if stream:stream.close()
 (out/'metrics.json').write_text(json.dumps(metrics,indent=2))
 module=root/('fteplug_ftebroom_x64.dll' if os.name=='nt' else 'fteplug_ftebroom_amd64.so')
 if module.exists():(out/'provenance.json').write_text(json.dumps({'build_info':(root/'build-info.txt').read_text(),'module_sha256':hashlib.sha256(module.read_bytes()).hexdigest(),'theme':a.theme,'window':[a.width,a.height],'renderer_requested':os.environ.get('RB_QA_RENDERER','default'),'method':'Actual OS mouse clicks/wheel; read-only UI/cvar geometry snapshots; persisted preferences; real process restart'},indent=2))
