#!/usr/bin/env python3
"""RB-10 browser acceptance through real OS clicks, typing and wheel events.

Use a disposable runtime. Fixture preparation happens before launching; read-only
UI telemetry publishes rectangles/state and never executes an interaction.
"""
import argparse,json,os,statistics,struct,subprocess,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json
p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);p.add_argument('--width',type=int,default=1600);p.add_argument('--height',type=int,default=1000);p.add_argument('--perf-only',action='store_true');a=p.parse_args()
root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True);data=root/'ftebroom';folder=data/'browser132';models=folder/'models';models.mkdir(parents=True,exist_ok=True);(folder/'subfolder'/'deeper').mkdir(parents=True,exist_ok=True);(folder/'empty').mkdir(exist_ok=True)
(folder/'existing.cfg').write_text('// fixture\n');(folder/'subfolder'/'inside.cfg').write_text('// navigation fixture\n')
for name in ['created-background.cfg','created-item.cfg']:(folder/name).unlink(missing_ok=True)
# 2048 tiny triangles, one texture; 240 model files reproduce a model-heavy folder.
count=2048;verts=count*3;triangles=bytearray();uv=bytearray();xyz=bytearray()
for i in range(count):
 triangles.extend(struct.pack('<3I',i*3,i*3+1,i*3+2));x=i%64;y=i//64
 for j,(dx,dy) in enumerate([(0,0),(1,0),(0,1)]):uv.extend(struct.pack('<2f',dx,dy));xyz.extend(struct.pack('<3h2B',(x+dx)*64,(y+dy)*64,(i%7)*8,0,0))
shader=b'browser132/models/skin';surface=bytearray(108);surface[:4]=b'IDP3';surface[4:8]=b'body';offsets=(108,108+len(triangles),108+len(triangles)+68,108+len(triangles)+68+len(uv));length=offsets[3]+len(xyz)
for at,value in [(72,1),(76,1),(80,verts),(84,count),(88,offsets[0]),(92,offsets[1]),(96,offsets[2]),(100,offsets[3]),(104,length)]:struct.pack_into('<I',surface,at,value)
header=bytearray(108);header[:4]=b'IDP3'
for at,value in [(4,15),(84,1),(100,108),(104,108+length)]:struct.pack_into('<I',header,at,value)
model=header+surface+triangles+shader.ljust(64,b'\0')+b'\0'*4+uv+xyz
for i in range(240):(models/f'model-{i:03}.md3').write_bytes(model)
tga=bytearray(18);tga[2]=2;tga[12]=tga[14]=2;tga[16]=32;tga[17]=40;(models/'skin.tga').write_bytes(tga+bytes([50,100,190,255])*4)
path=data/'browser132-live.json';path.unlink(missing_ok=True)
(data/'browser132-interaction.cfg').write_text('exec ftebroom.cfg\nset ftebroom_skip_setup 1\nin 7 mapedit maps/start.map\nin 10 set ca_brush_view 0\nin 12 ftebroom_test_desktop browser132\n')
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64');render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
stream=(out/'editor.log').open('wb');process=subprocess.Popen([str(engine),*render,'+set','ftebroom_skip_setup','1','+set','ftebroom_input_qa','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width',str(a.width),'-height',str(a.height),'-basedir',str(root),'+exec','browser132-interaction.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
checks=[]
def live():return read_ui_json(path)
def wait(fn,timeout=90):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  if process.poll() is not None:raise RuntimeError('Editor exited; see '+str(out/'editor.log'))
  try:
   value=fn()
   if value:return value
  except (FileNotFoundError,json.JSONDecodeError,KeyError,PermissionError):pass
  time.sleep(.07)
 raise AssertionError('Timed out; state='+path.read_text()[:1800])
ui=NativeInput(process,path,live,wait,out,focus_required=True)
def check(value,name):
 assert value,name
 checks.append(name);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',name,flush=True)
def rect(key):return wait(lambda:live()['widgets'].get(key))
def click(key,button=1):
 ui.settle();r=rect(key);assert r.get('visible',True),key+' clipped';ui.click(r['x']+r['width']*.5,r['y']+r['height']*.5,button=button)
def edit(key,text):
 click(key);ui.key('a',('ctrl',));ui.key('backspace');ui.text(str(text));ui.key('enter')
def navigate(target):
 click('nav/mod-folder/edit');wait(lambda:'nav/mod-folder/input' in live()['widgets']);edit('nav/mod-folder/input',target);wait(lambda:Path(live()['folder'])==Path(target))
def create(name,item=None):
 if item:click('files/item/'+item,3)
 else:
  r=rect('files/grid');ui.click(r['x']+r['width']*.72,r['y']+r['height']*.86,button=3)
 click('files/context-create');edit('files/create-name',name);click('files/create-confirm');wait(lambda:(folder/name).is_file())
def wheel(grid,key):
 r=rect(grid);old=live()[key];ui.move(r['x']+r['width']*.65,r['y']+r['height']*.52)
 ui.scroll(-1);ui.settle();check(live()[key]==old,grid+' plain wheel preserves thumbnail size')
 ui.mod('ctrl',True)
 try:ui.scroll(1);wait(lambda:live()[key]>old)
 finally:ui.mod('ctrl',False)
 check(True,grid+' Ctrl+wheel increases thumbnail size')
def sample_frames(seconds=3):
 rows=[];deadline=time.monotonic()+seconds;last=-1
 while time.monotonic()<deadline:
  s=live()
  if s['frame']!=last:rows.append(s['frameMs']);last=s['frame']
  time.sleep(.08)
 return rows
try:
 wait(lambda:live()['folder'].endswith('/browser132') and 'files/grid' in live()['widgets']);ui.position()
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 if not a.perf_only:
  create('created-background.cfg');check((folder/'created-background.cfg').read_text()=='','right-click empty file grid creates a file')
  create('created-item.cfg','existing.cfg');check((folder/'created-item.cfg').is_file(),'right-click an existing item creates a file')
  click('files/item/subfolder');wait(lambda:live()['folder'].endswith('/subfolder'));click('nav/mod-folder/segment/'+folder.as_posix());wait(lambda:Path(live()['folder'])==folder);check(True,'ancestor breadcrumb navigates to its directory')
  navigate(folder/'subfolder'/'deeper');check(Path(live()['folder'])==folder/'subfolder'/'deeper','trailing blank opens editable path and Enter navigates')
  toolbar_keys=('files/up','files/refresh','files/open-in-manager','files/view-list','files/view-tiles','nav/mod-folder/bar')
  toolbar=[rect(key) for key in toolbar_keys]
  check(all(item['visible'] for item in toolbar) and max(item['y'] for item in toolbar)<min(item['y']+item['height'] for item in toolbar),'file commands and breadcrumbs share one row')
  panel=rect('files/panel');hide=rect('files/hide');contents=rect('files/grid')
  check(contents['y']+contents['height']<=hide['y'] and hide['y']+hide['height']<=panel['y']+panel['height'],'Hide files is in the browser footer')
  click('files/view-list');wait(lambda:not live()['filesGrid'] and 'thumbnail/mod-thumbnail' not in live()['widgets'])
  click('files/hide');wait(lambda:'files/panel' not in live()['widgets'] and 'files/show' in live()['widgets'])
  click('files/show');wait(lambda:'files/hide' in live()['widgets'] and 'files/show' not in live()['widgets'])
  reopened=rect('files/panel')
  check(abs(reopened['x']-panel['x'])<2 and abs(reopened['y']-panel['y'])<2 and Path(live()['folder'])==folder/'subfolder'/'deeper' and not live()['filesGrid'],'Show files reopens the same docked browser and preserves its folder and view')
  click('files/view-tiles');wait(lambda:live()['filesGrid'] and 'thumbnail/mod-thumbnail' in live()['widgets'])
  check(True,'list and tile buttons switch views and show zoom only for tiles')
 navigate(folder);wheel('files/grid','thumbnail');ImageGrab.grab().save(out/'mod-file-manager.png')
 navigate(folder/'empty');baseline=sample_frames();navigate(models);wait(lambda:live()['readyModels']>=4 and not live()['previewBusy'],120);ui.settle();warm_start=live();warm=sample_frames();warm_end=live();ImageGrab.grab().save(out/'model-heavy-folder.png')
 check(warm_end['modelQuads']>0,'model-heavy folder shows cached image previews');
 r=rect('files/grid');old_size=live()['thumbnail'];old_scroll=r['scrollY'];assert r['scrollMax']>0
 ui.move(r['x']+r['width']*.65,r['y']+r['height']*.52);ui.scroll(-3)
 wait(lambda:live()['widgets']['files/grid']['scrollY']>old_scroll)
 check(live()['thumbnail']==old_size,'plain wheel scrolls an overflowing file grid without resizing thumbnails');check(warm_start['modelReads']==warm_end['modelReads'] and warm_start['modelBuilds']==warm_end['modelBuilds'],'warm model folder performs no resource reads or preview rebuilds')
 metrics={'baseline_frame_ms':baseline,'warm_frame_ms':warm,'baseline_median_ms':statistics.median(baseline),'warm_median_ms':statistics.median(warm),'entries':live()['entries'],'cachedModels':live()['cachedModels'],'modelQuads':live()['modelQuads'],'source_triangles_per_model':count,'warm_triangles_per_model':2};(out/'performance.json').write_text(json.dumps(metrics,indent=2));check(metrics['warm_median_ms']<=metrics['baseline_median_ms']*1.35+3,'warm model folder does not significantly reduce frame rate')
 if not a.perf_only:
  click('tab/Entity');wait(lambda:'entities/grid' in live()['widgets'] and live()['entityClasses']);check('browser132_point' in live()['entityClasses'] and 'browser132_brush' not in live()['entityClasses'],'entity browser includes point entities and excludes brush entities');wheel('entities/grid','entityThumbnail');ImageGrab.grab().save(out/'entity-browser.png')
  click('tab/Face');wait(lambda:'materials/grid' in live()['widgets']);wheel('materials/grid','materialThumbnail');ImageGrab.grab().save(out/'material-browser.png')
 (out/'final-state.json').write_text(json.dumps(live(),indent=2));print('PASS browser acceptance',len(checks),flush=True)
except Exception:
 ImageGrab.grab().save(out/'failure.png')
 try:(out/'failure-state.json').write_text(json.dumps(live(),indent=2))
 except Exception:pass
 raise
finally:
 if process.poll() is None:process.kill();process.wait(timeout=15)
 stream.close()
