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
 for attempt in range(3):
  r=wait(lambda:live()['widgets'].get(key));ui.click(r['x']+r['width']*.3,r['y']+r['height']*.5,mods=('ctrl',) if scalar else ())
  try:wait(lambda:live()['widgets'][key].get('active'),3);break
  except AssertionError:
   if attempt==2:raise
 ui.key('a',('ctrl',));ui.key('backspace');ui.text(str(value));ui.key('enter')
def close_plant():
 w=live()['widgets']['plant/window'];ui.click(w['x']+w['width']-16,w['y']+14)
 if 'plant/confirm-discard' in live()['widgets']:click('plant/confirm-discard')
 wait(lambda:not live()['plantOpen'])
def context(kind,view=0):
 v=live()['views'][view];ui.click(v['x']+v['width']*.46,v['y']+v['height']*.52,button=3)
 hover('context/create');hover('context/model')
 if not (out/'create-model-menu.png').exists():ui.settle();ImageGrab.grab().save(out/'create-model-menu.png')
 click('context/new-'+kind)
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
 ui.console('ftebroom_entity_property set angles \"0 25 0\"');ui.console('ftebroom_entity_property set modelscale \"0.85\"');ui.console('ftebroom_entity_property set modelscale_vec \"1 2 3\"');edit('plant/Height',170,True);wait(lambda:live()['plantReady']);click('plant/save');wait(lambda:not live()['plant']['awaiting']);second=entities(snapshot('second'));check(len(second)==1 and second[0]['model']!=old and second[0]['origin']==origin,'repeat Save updates same entity and preserves transform');check((data/old).is_file(),'previous immutable runtime asset survives for history');check(second[0]['angles']=='0 25 0' and abs(float(second[0]['modelscale'])-.85)<.0001 and second[0]['modelscale_vec']=='1 2 3','Save preserves user rotation and scalar/vector scale')
 ui.console('ftebroom_undo');undone=entities(snapshot('undo'));check(len(undone)==1 and undone[0]['model']==old,'Undo restores previous visible asset path');ui.console('ftebroom_redo');check(entities(snapshot('redo'))[0]['model']==second[0]['model'],'Redo restores updated asset path')
 # Cancellation and a refused overwrite leave both source and scene intact.
 original_path=live()['plantPath'];click('plant/save-as');click('plant/file-cancel');check(live()['plantPath']==original_path and len(entities(snapshot('plant-save-as-cancel')))==1,'cancelled plant Save As preserves the existing binding')
 sentinel=project.parent/'creation-qa-other.plant.json';sentinel.write_text('owned test sentinel');click('plant/save-as');edit('plant/file-path',sentinel);click('plant/file-choose');check(sentinel.read_text()=='owned test sentinel' and live()['plantPath']==original_path,'failed plant Save As never overwrites another project');click('plant/file-cancel');check(len(entities(snapshot('plant-refused-overwrite')))==1,'failed plant Save As leaves the single scene entity intact')
 new_source=project.parent/'植被 assets'/'creation-qa-as.plant.json';new_source.unlink(missing_ok=True);click('plant/save-as');edit('plant/file-path',new_source);click('plant/file-choose');wait(lambda:live()['plantPath']==str(new_source) and not live()['plant']['awaiting']);check(len(entities(snapshot('plant-save-as')))==1,'Unicode path Save As updates the same entity')
 ui.console('ftebroom_delete');edit('plant/Height',160,True);wait(lambda:live()['plantReady']);click('plant/save');wait(lambda:not live()['plant']['active']);check(not entities(snapshot('deleted-save')),'Save after deletion never resurrects an entity')
 close_plant();context('model');wait(lambda:live()['newModel']);click('model/new-cancel');wait(lambda:not live()['newModel']);check(not entities(snapshot('new-model-cancel')),'cancelled New Model inserts no entity');context('model');wait(lambda:live()['newModel']);edit('model/new-name','creation-qa-'+str(int(time.time())))
 # Prop template is chosen through the real combo with keyboard navigation.
 click('model/new-template');click('model/template/Prop');click('model/new-create');wait(lambda:live()['modelOpen'] and live()['model']['active']);check(live()['modelElements']>0,'Prop template creates exportable geometry');ui.key('s',('ctrl',));wait(lambda:live()['model']['created'] and not live()['model']['awaiting']);created=entities(snapshot('model'));check(len(created)==1 and created[0]['classname']=='func_model','Model Editor first Save creates func_model');check(created[0]['_reyondbroom_source'].endswith('.bbmodel'),'model entity retains portable editable source link');check((data/created[0]['model']).is_file(),'model runtime MD3 saved automatically')
 ui.key('s',('ctrl',));time.sleep(.4);check(len(entities(snapshot('model-repeat')))==1,'unchanged model Save does not duplicate the entity')
 # A geometry change produces a new asset but retains the same scene entity.
 before_count=live()['modelElements'];click('model/transform-menu');click('model/transform/Add Cube');wait(lambda:live()['modelElements']>before_count);ui.key('s',('ctrl',));wait(lambda:not live()['modelBusy']);updated=entities(snapshot('model-updated'));check(len(updated)==1 and updated[0]['model']!=created[0]['model'],'model geometry Save updates the same linked entity');check((data/created[0]['model']).exists(),'model history keeps the old geometry asset')
 # Save As changes the authoring filename without inserting another entity.
 click('model/file-menu');click('model/file/Save as');edit('model/file-name','creation-qa-as-'+str(int(time.time()))+'.bbmodel');click('model/file-accept');wait(lambda:not live()['modelBusy']);ImageGrab.grab().save(out/'model-editor.png');check(len(entities(snapshot('model-save-as')))==1,'model Save As preserves the single placement binding');saved_path=live()['modelPath']
 click('model/file-menu');click('model/file/Close');context('model');wait(lambda:live()['newModel']);edit('model/new-name','creation-qa-empty-'+str(int(time.time())));click('model/new-template');click('model/template/Empty');click('model/new-create');wait(lambda:live()['modelOpen'] and live()['model']['active']);ui.key('s',('ctrl',));wait(lambda:not live()['modelBusy'] and 'failed' in live()['modelMessage']);check(len(entities(snapshot('empty-model-error')))==1,'failed model export leaves no broken scene entity')
 # Switching the source map invalidates this unsatisfied Create request.
 ui.console('mapedit maps/creation-qa-switch.map');wait(lambda:not live()['model']['active']);click('model/file-menu');click('model/file/Save');check(not entities(snapshot('stale-map-save')),'Save after a map switch cannot insert into the new map')
 # Both commands also work from an orthographic scene, with its own plane.
 click('model/file-menu');click('model/file/Close');ui.console('set ca_brush_view 2');wait(lambda:live()['views'][0]['type']==2);context('plant');wait(lambda:live()['plantReady'] and live()['plant']['active']);check(abs(live()['plant']['normal'][1])>.99,'orthographic click captures the front workplane normal');ortho=project.parent/'creation-qa-ortho.plant.json';ortho.unlink(missing_ok=True);click('plant/save');wait(lambda:'plant/file-path' in live()['widgets']);edit('plant/file-path',ortho);click('plant/file-choose');wait(lambda:live()['plant']['created'] and not live()['plant']['awaiting']);check(len(entities(snapshot('orthographic-plant')))==1,'orthographic plant Save inserts one func_model');ImageGrab.grab().save(out/'plant-editor.png');close_plant()
 context('model');wait(lambda:live()['newModel']);edit('model/new-name','creation-qa-ortho-'+str(int(time.time())));click('model/new-template');click('model/template/Prop');click('model/new-create');wait(lambda:live()['model']['active']);ui.key('s',('ctrl',));wait(lambda:live()['model']['created'] and not live()['modelBusy']);check(len(entities(snapshot('orthographic-model')))==2,'orthographic Model Editor Save inserts its own single func_model')
 click('model/file-menu');click('model/file/Close');ui.console('set ftebroom_view_layout 1');ui.console('set ca_brush_view 0');placed=entities(snapshot('visible-scene'))[0];xyz=[float(x) for x in placed['origin'].strip("'").split()];ui.console('ftebroom_camera_restore '+' '.join(str(x+d) for x,d in zip(xyz,[320,-400,260]))+' 20 130 0 75');time.sleep(1);ui.settle();photo=ImageGrab.grab();photo.save(out/'placed-models-scene.png');v=live()['views'][0];ox,oy=ui.position();sx,sy=getattr(ui,'scale',(1,1));region=photo.crop((int(ox+v['x']*sx),int(oy+v['y']*sy),int(ox+(v['x']+v['width'])*sx),int(oy+(v['y']+v['height'])*sy))).convert('RGB');green=sum(g>r*1.25 and g>b*1.8 and g>25 and r<160 and b<100 for r,g,b in region.getdata());check(green>max(50,250*a.width*a.height/(1920*1080)),'automatically saved plant visibly renders in the map viewport');(out/'scene-pixels.json').write_text(json.dumps({'green_pixels':green,'scene_size':region.size},indent=2));(out/'final-state.json').write_text(json.dumps(live(),indent=2));
 # Pick an actual green crown pixel, well above the old tiny origin marker.
 foliage=[(i%region.width,i//region.width) for i,(r,g,b) in enumerate(region.getdata()) if g>r*1.25 and g>b*1.8 and g>25 and r<160 and b<100];cx=sum(x for x,y in foliage)/len(foliage);cy=sum(y for x,y in foliage)/len(foliage);px,py=min(foliage,key=lambda p:(p[0]-cx)**2+(p[1]-cy)**2);ui.click(v['x']+px/sx,v['y']+py/sy);ui.console('ftebroom_entity_property set reyon9_pick_test crown');picked=entities(snapshot('crown-pick'));check(sum(e.get('reyon9_pick_test')=='crown' and e['model']==placed['model'] for e in picked)==1,'clicking visible foliage selects its authoring model beyond the origin marker')
 # Ordinary source opens never create a new scene object. Snapshot copies are
 # editable through Save As, so history's source files cannot be overwritten.
 source_root=Path(json.loads((data/'plant-ui-live.json').read_text())['assetSource']);scene=entities(snapshot('before-source-open'));model_entity=next(e for e in scene if e['_reyondbroom_source'].endswith('.bbmodel'));plant_entity=next(e for e in scene if e['_reyondbroom_source'].endswith('.plant.json'));model_source=source_root/model_entity['_reyondbroom_source'];plant_source=source_root/plant_entity['_reyondbroom_source'];model_original=model_source.read_bytes();plant_original=plant_source.read_bytes()
 context('model');wait(lambda:live()['newModel']);edit('model/new-name','creation-qa-guard-'+str(int(time.time())));click('model/new-template');click('model/template/Prop');click('model/new-create');wait(lambda:live()['model']['active']);click('model/file-menu');click('model/file/Open');edit('model/file-name',model_source);click('model/file-accept');wait(lambda:live()['guardModel']);click('model/guard-save');wait(lambda:live()['modelOpen'] and live()['modelPath']=='' and not live()['modelBusy']);check(len(entities(snapshot('model-guard-save')))==3,'guard Save completes placement before switching model documents');check(not live()['model']['active'],'ordinary model source open has no pending insertion')
 ui.key('s',('ctrl',));wait(lambda:'model/file-name' in live()['widgets']);edit('model/file-name','creation-qa-source-copy-'+str(int(time.time()))+'.bbmodel');click('model/file-accept');wait(lambda:live()['modelPath'] and not live()['modelBusy']);check(model_source.read_bytes()==model_original and len(entities(snapshot('model-source-copy')))==3,'model source Save As preserves immutable history and does not insert');click('model/file-menu');click('model/file/Close')
 context('plant');wait(lambda:live()['plantReady']);click('plant/open');wait(lambda:'plant/file-path' in live()['widgets']);edit('plant/file-path',plant_source);click('plant/file-choose');wait(lambda:live()['plantReady'] and not live()['plant']['active']);check(live()['plantPath']=='','versioned plant source opens as a copy');copy=project.parent/'creation-qa-source-copy.plant.json';copy.unlink(missing_ok=True);click('plant/save');wait(lambda:'plant/file-path' in live()['widgets']);edit('plant/file-path',copy);click('plant/file-choose');wait(lambda:copy.is_file());check(plant_source.read_bytes()==plant_original and len(entities(snapshot('plant-source-copy')))==3,'ordinary plant source Save preserves history and does not insert')
 close_plant()
 # Exercise more than the default tree through the same Save placement route.
 for species,label in [('wild-shrub','Wild hawthorn'),('meadow-grass','Meadow grass')]:
  context('plant');wait(lambda:live()['plantReady'] and live()['plant']['active']);edit('plant/search',label);click('plant/preset/'+species);wait(lambda:live()['plantReady']);target=project.parent/('creation-qa-'+species+'.plant.json');target.unlink(missing_ok=True);click('plant/save');wait(lambda:'plant/file-path' in live()['widgets']);edit('plant/file-path',target);click('plant/file-choose');wait(lambda:live()['plant']['created'] and not live()['plant']['awaiting']);check(len(entities(snapshot(species)))==(4 if species=='wild-shrub' else 5),species+' first Save creates its own single func_model');close_plant()
 print('PASS viewport creation',len(checks),flush=True)
except Exception:
 ImageGrab.grab().save(out/'failure.png');(out/'failure-state.json').write_text(json.dumps(live(),indent=2));raise
finally:
 if process.poll() is None:process.kill();process.wait(timeout=15)
 stream.close()
