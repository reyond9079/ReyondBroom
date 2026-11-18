#!/usr/bin/env python3
"""Real engine regression for atomic Jam previews and visible cubic plot bounds."""
import argparse,json,os,re,subprocess,sys,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path)
a=p.parse_args();root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[1];data=root/'ftebroom'
compiler=os.environ.get('CXX','g++');unit=out/('jam-world-unit.exe' if os.name=='nt' else 'jam-world-unit')
with (out/'unit-build.log').open('wb') as log:
 subprocess.run([compiler,'-std=c++17','-O1',*(['-static'] if os.name=='nt' else []),str(repo/'tests/mapping-jam-world.cpp'),'-o',str(unit)],check=True,stdout=log,stderr=subprocess.STDOUT)
with (out/'unit.log').open('wb') as log:subprocess.run([str(unit)],check=True,stdout=log,stderr=subprocess.STDOUT)
source=data/'maps/ftebroom-build/jam-scene-qa.map';source.parent.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(repo/'tests/make-build-fixture.py'),str(source)],check=True)
qbsp=root/'tools/q3map2'/('q3map2.exe' if os.name=='nt' else 'q3map2')
with (out/'fixture-build.log').open('wb') as log:
 subprocess.run([str(qbsp),'-game','quake3','-fs_basepath',str(root),'-fs_game','ftebroom','-meta',str(source)],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
state=data/'compilation-ui-live.json'
for path in (state,data/'qconsole.log',root/'qconsole.log'):path.unlink(missing_ok=True)
(data/'jam-world-qa.cfg').write_text('set ftebroom_skip_setup 1\nexec ftebroom.cfg\nset ftebroom_input_qa 1\nin 7 mapedit maps/ftebroom-build/jam-scene-qa.map\nin 12 ftebroom_test_desktop reyon8qa\n')
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64')
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
stream=(out/'editor.log').open('wb')
process=subprocess.Popen([str(engine),*render,'-basedir',str(root),'-nohome','-noupdates','-nosound','-condebug','-window','-width','1600','-height','1000','+exec','jam-world-qa.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
checks=[]
def live():return read_ui_json(state)
def logs():
 return '\n'.join(x.read_text(errors='replace') for x in (out/'editor.log',data/'qconsole.log',root/'qconsole.log') if x.exists())
def wait(fn,timeout=60):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  assert process.poll() is None,'Editor exited'
  try:
   result=fn()
   if result:return result
  except (OSError,ValueError,KeyError):pass
  time.sleep(.08)
 raise AssertionError('Jam world QA timed out')
def check(ok,name):
 assert ok,name
 checks.append(name);print('PASS',name,flush=True)
def capture(name):
 ui.settle();image=ImageGrab.grab();image.save(out/(name+'.png'))
 ox,oy=ui.position();sx,sy=getattr(ui,'scale',(1,1));v=next(v for v in live()['views'] if v['type']==0)
 image=image.crop((int(ox+(v['x']+12)*sx),int(oy+(v['y']+35)*sy),int(ox+(v['x']+v['width']-12)*sx),int(oy+(v['y']+v['height']-12)*sy))).convert('RGB')
 points=[(x,y) for y in range(image.height) for x in range(image.width) if (lambda r,g,b:g>r+65 and b>r+35 and g>b+15)(*image.getpixel((x,y)))]
 assert len(points)>100,'Owned plot border is not visibly rendered'
 return {'pixels':len(points),'top':min(y for x,y in points),'bottom':max(y for x,y in points)}
try:
 wait(lambda:live()['views'] and live()['globalWidgets'].get('menu/View'))
 ui=NativeInput(process,state,live,wait,out,focus_required=True);ui.position()
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 ui.console('ftebroom_test_jam_bounds')
 wait(lambda:'FTEBROOM_TRANSFORM_PASS jam_scene_explicit_empty_snapshot_removes_geometry' in logs())
 text=logs();check('FTEBROOM_TRANSFORM_FAIL' not in text,'all_edit_tools_and_atomic_scene_checks')
 for name in re.findall(r'FTEBROOM_TRANSFORM_PASS (jam_\S+)',text):
  if name not in checks:checks.append(name)
 ui.console('ftebroom_test_jam_foreign 0')
 wait(lambda:'FTEBROOM_TRANSFORM_PASS jam_foreign_rotate_grid' in logs())
 check('FTEBROOM_TRANSFORM_FAIL' not in logs(),'foreign_geometry_remains_unpickable_by_all_tools')
 for command in ['set ftebroom_view_layout 1','set ca_brush_view 0','set ftebroom_jam_world 1',
                 'set ftebroom_grid_visible 0','set ftebroom_show_edges 0','set ftebroom_jam_plot_center "0 0 0"',
                 'ftebroom_camera_restore 2600 -3200 2400 30 130 0 90']:
  ui.console(command)
 ui.console('set ftebroom_jam_plot_half_size "1024 1024 512"');old=capture('legacy-volume')
 ui.console('set ftebroom_jam_plot_half_size "1024 1024 1024"');new=capture('cubic-volume')
 check(new['bottom']-new['top']>(old['bottom']-old['top'])*1.12,'new_ceiling_and_floor_expand_the_visible_volume')
 (out/'volume-pixels.json').write_text(json.dumps({'legacy':old,'cubic':new},indent=2))
 (out/'results.json').write_text(json.dumps(checks,indent=2))
except BaseException:
 try:ImageGrab.grab().save(out/'failure.png')
 except Exception:pass
 raise
finally:
 if process.poll() is None:process.kill();process.wait(timeout=15)
 stream.close()
