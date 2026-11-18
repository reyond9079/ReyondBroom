#!/usr/bin/env python3
"""Exercise link preferences and FGD/group bounds through the real editor."""
import argparse, json, os, subprocess, time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput, read_ui_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path)
a=p.parse_args();root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
data=root/'ftebroom';state=data/'compilation-ui-live.json'
(data/'ftebroom-preferences.cfg').unlink(missing_ok=True)
fgd=data/'link-color-qa.fgd'
fgd.write_text('@PointClass size(-8 -8 -8, 8 8 8) color(80 160 240) = target_relay : "Link fixture" []\n'
               '@SolidClass color(255 160 0) = func_color_qa : "Brush fixture" []\n')
(data/'link-color-qa.cfg').write_text('exec qa-clip.cfg\nset ftebroom_input_qa 1\nin 12 ftebroom_test_desktop reyon8qa\n')
checks=[];metrics={}
def live():return read_ui_json(state)
def wait(fn,timeout=40):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  assert process.poll() is None,'Editor exited'
  try:
   value=fn()
   if value:return value
  except (OSError,ValueError,KeyError):pass
  time.sleep(.08)
 raise AssertionError('Entity color QA timed out')
def check(value,name):
 assert value,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
def start(label):
 global process,stream,ui
 state.unlink(missing_ok=True)
 engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64')
 render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
 stream=(out/(label+'.log')).open('wb')
 process=subprocess.Popen([str(engine),*render,'-basedir',str(root),'-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width','1600','-height','1000','+set','ftebroom_skip_setup','1','+exec','link-color-qa.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
 wait(lambda:live()['views'] and live()['globalWidgets'].get('menu/View'))
 ui=NativeInput(process,state,live,wait,out,focus_required=True);ui.position()
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 ui.move(600,300);ui.settle()
def stop():process.kill();process.wait(timeout=15);stream.close()
def shot(name):
 ui.settle();img=ImageGrab.grab();img.save(out/(name+'.png'));return img
def click(name):
 r=wait(lambda:live()['globalWidgets'].get(name));ui.click(r['x']+r['width']*.5,r['y']+r['height']*.5)
def rgb(name,values):
 r=wait(lambda:live()['globalWidgets'].get('preference-color-'+name));ui.click(r['x']+25,r['y']+r['height']*.5,mods=('ctrl',))
 for i,v in enumerate(values):ui.key('a',('ctrl',));ui.text(str(v));ui.key('tab' if i<2 else 'enter')
def count(img,kind,predicate):
 ox,oy=ui.position();sx,sy=getattr(ui,'scale',(1,1));total=0
 for v in live()['views']:
  if (kind=='2d')!=(v['type']!=0):continue
  area=img.crop((int(ox+(v['x']+12)*sx),int(oy+(v['y']+40)*sy),int(ox+(v['x']+v['width']-12)*sx),int(oy+(v['y']+v['height']-12)*sy)))
  total+=sum(predicate(c[:3]) for c in area.getdata())
 return total
green=lambda c:c[1]>c[0]+60 and c[1]>c[2]+50
red=lambda c:c[0]>c[1]+60 and c[0]>c[2]+60
cyan=lambda c:c[1]>c[0]+60 and c[2]>c[0]+60 and abs(c[1]-c[2])<30
magenta=lambda c:c[0]>c[1]+60 and c[2]>c[1]+60 and abs(c[0]-c[2])<30
orange=lambda c:c[0]>c[1]+40 and c[1]>c[2]+40
purple=lambda c:c[2]>c[0]+30 and c[0]>c[1]+30
def link_delta(name,predicate):
 ui.console('set ftebroom_entity_links 0');off=shot(name+'-hidden')
 ui.console('set ftebroom_entity_links 3');on=shot(name)
 for kind in ('2d','3d'):
  delta=count(on,kind,predicate)-count(off,kind,predicate);metrics[name+'-'+kind]=delta
  check(delta>25,name+'_'+kind)
def select_source():
 v=next(v for v in live()['views'] if v['type']==3);unit=max(v['width'],v['height'])/640
 ui.click(v['x']+v['width']*.5-160*unit,v['y']+v['height']*.5-64*unit);ui.settle()
try:
 start('editor')
 colors=live()['viewportColors']
 check([float(x) for x in colors['ftebroom_color_entity_links'].split()]==[.2,1,.3],'original_green_default')
 check([float(x) for x in colors['ftebroom_color_selected_entity_links'].split()]==[1,.2,.2],'original_selected_red_default')
 ui.console('ftebroom_definitions "'+fgd.as_posix()+'"')
 ui.console('set ftebroom_grid_visible 0');ui.console('set ftebroom_show_edges 0');ui.console('set ftebroom_clear_sky 0')
 ui.console('set ftebroom_show_entity_names 0');ui.console('set ftebroom_show_entity_names_2d 0')
 ui.console('set ca_brush_viewsize 640');ui.console('ftebroom_camera_restore 0 -512 256 0 90 0 90')
 ui.console('ftebroom_entity_create target_relay 0 "-160 64 256"');ui.console('ftebroom_entity_property set target color_destination')
 ui.console('ftebroom_entity_create target_relay 0 "160 64 256"');ui.console('ftebroom_entity_property set targetname color_destination')
 link_delta('selected-links-red',red)
 ui.console('ftebroom_deselect');link_delta('ordinary-links-green',green)
 select_source();link_delta('clicked-source-links-red',red)
 click('menu/Edit');click('Menu/Edit/Preferences...');click('preferences-colors-tab')
 rgb('ftebroom_color_entity_links',(0,255,255));rgb('ftebroom_color_selected_entity_links',(255,0,255))
 click('preferences-close');link_delta('selected-links-custom-magenta',magenta)
 ui.console('ftebroom_deselect');link_delta('ordinary-links-custom-cyan',cyan)
 ui.console('set ftebroom_view_cache_disable 1');link_delta('uncached-links-custom-cyan',cyan);ui.console('set ftebroom_view_cache_disable 0')
 ui.console('set ftebroom_entity_links 0');ui.console('ftebroom_box 0 64 256 96 96 96')
 ui.console('ftebroom_entity_create func_color_qa 1 "0 0 0"');selected=shot('selected-brush-bounds-red')
 ui.console('ftebroom_deselect');normal=shot('brush-bounds-fgd-orange')
 for kind in ('2d','3d'):
  check(count(normal,kind,orange)-count(selected,kind,orange)>25,'brush_bounds_follow_fgd_'+kind)
  check(count(selected,kind,red)-count(normal,kind,red)>25,'selected_entity_bounds_red_'+kind)
 ui.console('ftebroom_box 0 -128 256 96 96 96');before=shot('ordinary-selection-before-group')
 ui.console('ftebroom_group ColorQA');group=shot('selected-group-bounds-purple')
 for kind in ('2d','3d'):check(count(group,kind,purple)-count(before,kind,purple)>25,'selected_group_bounds_purple_'+kind)
 stop();start('restart');colors=live()['viewportColors']
 check([float(x) for x in colors['ftebroom_color_entity_links'].split()]==[0,1,1],'ordinary_link_color_survives_restart')
 check([float(x) for x in colors['ftebroom_color_selected_entity_links'].split()]==[1,0,1],'selected_link_color_survives_restart')
 stop()
except BaseException:
 try:ui.console('ftebroom_test_entities link-colors-state')
 except Exception:pass
 try:ImageGrab.grab().save(out/'failure.png')
 except Exception:pass
 try:(out/'failure-state.json').write_text(json.dumps(live(),indent=2))
 except Exception:pass
 raise
finally:
 if 'process' in globals() and process.poll() is None:process.kill();process.wait(timeout=15)
 if 'stream' in globals():stream.close()
 (out/'metrics.json').write_text(json.dumps(metrics,indent=2))
