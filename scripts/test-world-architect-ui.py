#!/usr/bin/env python3
"""Exercise the developing World Architect through actual OS input in a private runtime."""
import argparse,hashlib,json,os,re,shlex,shutil,subprocess,sys,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json
p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);p.add_argument('--preset',choices=['hospital','metro','evacuation'],default='hospital');p.add_argument('--regions-only',action='store_true');p.add_argument('--pins-only',action='store_true');p.add_argument('--designer-only',action='store_true');p.add_argument('--paint-only',action='store_true');p.add_argument('--themes-only',action='store_true');p.add_argument('--scenarios-only',action='store_true');p.add_argument('--local-only',action='store_true');p.add_argument('--modules-only',action='store_true');p.add_argument('--quality-only',action='store_true');p.add_argument('--module-fixtures',type=Path);p.add_argument('--art-update-only',action='store_true');p.add_argument('--legacy-only',action='store_true');p.add_argument('--legacy-fixtures',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
assert sum([a.regions_only,a.pins_only,a.designer_only,a.paint_only,a.legacy_only,a.themes_only,a.art_update_only,a.scenarios_only,a.local_only,a.modules_only,a.quality_only])<=1,'Select one focused authoring scenario'
root=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);data=root/'ftebroom';state=data/'world-architect-ui-live.json';state.unlink(missing_ok=True)
for name in ['wa-before','wa-generated','wa-applied','wa-undone','wa-redone']:(data/'maps'/(name+'.map')).unlink(missing_ok=True)
(data/'world-qa.cfg').write_text('exec qa-clip.cfg\nset ftebroom_placement_qa 1\nin 12 ftebroom_world_architect qa\n')
engine=a.engine.resolve() if a.engine else root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64');stream=(out/'editor.log').open('wb');render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
proc=subprocess.Popen([str(engine),*render,'+set','ftebroom_skip_setup','1','+set','ftebroom_input_qa','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width','1920','-height','1080','-basedir',str(root),'+exec','world-qa.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
checks=[]
module=root/('fteplug_ftebroom_x64.dll' if os.name=='nt' else 'fteplug_ftebroom_amd64.so')
(out/'provenance.json').write_text(json.dumps({'moduleSha256':hashlib.sha256(module.read_bytes()).hexdigest(),'engineSha256':hashlib.sha256(engine.read_bytes()).hexdigest(),'qcSha256':hashlib.sha256((data/'csaddon.dat').read_bytes()).hexdigest(),'runtimeBuildInfo':(root/'build-info.txt').read_text(),'method':'Actual OS input; live read-only telemetry; generated MAP/texture assets; real executed import and history transactions'},indent=2)+'\n')
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
def drag(start,end):
 ui.move(*start)
 if os.name=='nt':ui.user.mouse_event(2,0,0,0,0)
 else:ui.xtest.fake_input(ui.d,ui.X.ButtonPress,1);ui.d.sync()
 try:
  ui.settle();ui.move(*end);ui.settle()
 finally:
  if os.name=='nt':ui.user.mouse_event(4,0,0,0,0)
  else:ui.xtest.fake_input(ui.d,ui.X.ButtonRelease,1);ui.d.sync()
 ui.settle()
def console(text):ui.console(text);time.sleep(.2)
def save(name):
 dest=data/'maps'/(name+'.map');dest.unlink(missing_ok=True);console('mod_terrain_save "" '+name);wait(dest.exists);return dest
# Source brush plane signatures survive map pathname and editor metadata changes.
def planes(path):return sorted(line.strip() for line in path.read_text(errors='replace').splitlines() if line.lstrip().startswith('( '))
def choose(path):
 text=path.as_posix();r=wait(lambda:live()['widgets'].get('file-path'));ui.click(r['x']+r['width']*.4,r['y']+r['height']/2);ui.key('a',('ctrl',));ui.text(text)
 wait(lambda:read_ui_json(data/'placement-ui-live.json')['browsePath']==text);ui.key('enter');click('file-choose');wait(lambda:'file-path' not in live()['widgets'])
def entities(path):
 text=path.read_text();level=0;start=0;result=[];header=[]
 for match in re.finditer(r'"(?:\\.|[^"\\])*"|//[^\n]*|[{}]',text):
  token=match.group()
  if token=='{':
   if level==0:start=match.start();header=[]
   level+=1
  elif token=='}':
   level-=1
   if level==0:
    block=text[start:match.end()];fields={};assert len(header)%2==0
    for key,value in zip(header[::2],header[1::2]):
     try:fields[json.loads(key)]=json.loads(value)
     except json.JSONDecodeError:fields[key[1:-1]]=value[1:-1]
    result.append((fields,sorted(line.strip() for line in block.splitlines() if line.lstrip().startswith('( ')),block))
  elif token.startswith('"') and level==1:header.append(token)
 assert level==0
 return result
def regions(path):
 rows=entities(path);result={}
 for fields,geometry,block in rows:
  if '_wa_region' not in fields or fields.get('_wa_project')!=live()['projectId']:continue
  children=sorted(json.dumps(f,sort_keys=True) for f,g,b in rows if f.get('_tb_group')==fields.get('_tb_id'))
  result[fields['_wa_region']]={'fields':fields,'geometry':geometry,'points':children}
 return result
def verify_paint(existing):
 click('tab/Destruction');canvas=live()['widgets']['plan-canvas'];drag((canvas['x']+canvas['width']*.2,canvas['y']+canvas['height']*.2),(canvas['x']+canvas['width']*.35,canvas['y']+canvas['height']*.35));wait(lambda:len(live()['settings']['influences'])==existing+1)
 painted=live()['settings']['influences'][-1];check(painted['kind']=='damage' and all(b>a for a,b in zip(painted['low'],painted['high'])),'dragging the floor plan creates a real bounded influence volume')
 click('undo-settings');wait(lambda:len(live()['settings']['influences'])==existing);check(True,'painted volumes participate in settings Undo')
try:
 wait(lambda:live()['open'] and 'generate' in live()['widgets']);ui.position()
 # Xvfb has no window manager to transfer focus after an external engine exits.
 # Acquire actual OS keyboard focus before the first console shortcut.
 if os.name!='nt':ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 click('tab/Export');wait(lambda:'apply' in live()['widgets'])
 check(True,'the first workspace tab click is received after actual OS activation')
 click('tab/Overview')
 before=save('wa-before');baseline=planes(before)
 if a.quality_only:
  click('generate');wait(lambda:live()['candidates'] and not live()['busy']);click('tab/Quality');click('quality-controls');r=live()['widgets']['candidate-count'];ui.click(r['x']+r['width']*.3,r['y']+r['height']/2,mods=('ctrl',));ui.key('a',('ctrl',));ui.text('3');ui.key('enter');r=live()['widgets']['quality-weight/visibility'];ui.click(r['x']+r['width']*.3,r['y']+r['height']/2,mods=('ctrl',));ui.key('a',('ctrl',));ui.text('5');ui.key('enter');wait(lambda:live()['settings']['candidates']==3 and live()['settings']['qualityWeights']['visibility']==5);check(True,'actual quality controls edit candidate budget and ranking weights')
  click('generate');wait(lambda:live()['candidates'] and not live()['busy']);check(2<=live()['candidates']<=3 and not live()['error'],'edited budget generates a new bounded batch of physically accepted candidates');click('candidate/1');wait(lambda:live()['selected']==1);click('select-best');wait(lambda:live()['selected']==0);check(True,'Select best restores the highest-ranked accepted candidate through actual OS input')
  metrics=live()['world']['scores'];check(all(0<=metrics[name]<=1 for name in ['visibility','cover','verticality','landmarkVisibility','roomAssetCoverage','spawnExposure']) and abs(sum(metrics[name] for name in ['shortSightlines','mediumSightlines','longSightlines'])-1)<1e-8,'Quality exposes finite normalized measurements from the generated physical world');ImageGrab.grab().save(out/'world-quality-controls.png')
  chosen=live()['world']['seed'];click('tab/Overview');project=out/'quality.world.json';click('save-project');choose(project);wait(project.exists);click('open-project');choose(project);wait(lambda:live()['candidates']==0);click('generate');wait(lambda:live()['candidates'] and not live()['busy']);check(live()['settings']['qualityWeights']['visibility']==5 and live()['world']['seed']==chosen and live()['world']['scores']==metrics,'saved ranking weights and selected physical measurements replay exactly after reopen');check(planes(save('wa-quality-preview'))==baseline,'quality edits, candidate selection and project replay preserve the authored map');print('World Architect quality UI:',len(checks),'checks passed',flush=True);sys.exit(0)
 if a.modules_only:
  assert a.module_fixtures,'Pass the actual authored module fixtures';fixtures=a.module_fixtures.resolve()
  def module_generate():
   click('generate');wait(lambda:live()['candidates'] and not live()['busy']);assert not live()['error'],live()['error'];fixture=live()['candidateSeeds'].index(1);click('candidate/'+str(fixture));wait(lambda:live()['world']['seed']==1)
  def module_export(name):
   click('tab/Export');dest=out/name/'world.map';click('export');choose(dest);wait(dest.exists);return dest
  module_generate();original=module_export('original');click('tab/Dressing');click('module-panel');click('module-open');choose(fixtures/('module-pack-'+str(live()['settings']['profileId'])+'.json'));wait(lambda:len(live()['settings']['authoredModules'])==1);check(True,'actual file-dialog import embeds the authored module pack source')
  click('module-choice');click('module/qa-arch');wait(lambda:len(live()['settings']['roomModules'])==1);module_generate();changed=module_export('authored');check(planes(changed)!=planes(original),'assigned authored geometry changes the actual exported room')
  sources=list((changed.parent/'modules').glob('qa-arch-*.map'));check(len(sources)==1 and sources[0].read_bytes()==(fixtures/'original-module.map').read_bytes(),'export preserves byte-exact authored MAP source with versioned provenance')
  click('undo-settings');wait(lambda:not live()['settings']['roomModules']);module_generate();undone=module_export('undone');check(planes(undone)==planes(original),'one settings Undo removes the room assignment and restores procedural geometry')
  click('redo-settings');wait(lambda:len(live()['settings']['roomModules'])==1);module_generate();click('tab/Overview');project=out/'authored.world.json';click('save-project');choose(project);wait(project.exists);click('open-project');choose(project);wait(lambda:live()['candidates']==0);module_generate();replayed=module_export('replayed');check(planes(replayed)==planes(changed),'embedded module pack and room assignment replay exactly after project reopen')
  check(planes(save('wa-authored-preview'))==baseline,'module import, assignment, history and export preserve the active authored scene');click('tab/Overview');ImageGrab.grab().save(out/'world-authored-module.png');print('World Architect authored module UI:',len(checks),'checks passed',flush=True);sys.exit(0)
 if a.local_only:
  def local_generate():
   click('generate');wait(lambda:live()['candidates'] and not live()['busy']);assert not live()['error'],live()['error']
  def local_export(name):
   click('tab/Export');dest=out/name/'world.map';click('export');choose(dest);wait(dest.exists);return dest
  local_generate();fixture=live()['candidateSeeds'].index(1);click('candidate/'+str(fixture));wait(lambda:live()['world']['seed']==1);original=local_export('original');click('tab/Dressing');r=live()['widgets']['room-seed'];ui.click(r['x']+r['width']*.3,r['y']+r['height']/2);ui.key('a',('ctrl',));ui.text('57');ui.key('enter');click('regenerate-room');wait(lambda:live()['candidates']==1 and not live()['busy']);assert not live()['error'],live()['error'];changed=local_export('room-changed')
  check(planes(changed)!=planes(original),'local room furnishing changes actual exported brush geometry')
  seeds=live()['settings']['frozenStageInputs'][4]['roomSeeds'];check(len(seeds)==1 and seeds[0]['seed']==57,'room regeneration stores only the selected room seed in its frozen dressing recipe')
  click('tab/Overview');project=out/'room.world.json';click('save-project');choose(project);wait(project.exists);click('open-project');choose(project);wait(lambda:live()['candidates']==0);local_generate();replayed=local_export('room-replayed');check(planes(replayed)==planes(changed),'room furnishing project reopens to the same actual brush geometry')
  click('freeze');click('freeze/0');click('preset');click('preset/evacuation');local_generate();click('tab/Site & Roads');road=live()['world']['roads'][0];points=road['points'];mid=[(points[0][i]+points[-1][i])/2 for i in range(3)];click('road-curved');wait(lambda:live()['settings']['roadPins'] and live()['settings']['roadPins'][0]['control'] is not None)
  r=live()['widgets']['road-control'];ui.click(r['x']+r['width']*.15,r['y']+r['height']/2);ui.key('a',('ctrl',));ui.text(str(round(mid[0])));ui.key('enter');r=live()['widgets']['road-control'];ui.click(r['x']+r['width']*.55,r['y']+r['height']/2);ui.key('a',('ctrl',));ui.text(str(round(mid[1]+32)));ui.key('enter');local_generate()
  check(len(live()['world']['roads'][0]['points'])>2 and live()['world']['valid'],'Bezier controls generate a physically validated curved road in the actual editor')
  world=live()['world'];canvas=live()['widgets']['plan-canvas'];scale=min((canvas['width']-32)/(world['bounds']['high'][0]-world['bounds']['low'][0]),(canvas['height']-32)/(world['bounds']['high'][1]-world['bounds']['low'][1]));points=world['roads'][0]['points'];start=points[max(1,int((len(points)-1)*.2))];end=points[max(2,int((len(points)-1)*.35))]
  def road_pixel(p):return (canvas['x']+16+(p[0]-world['bounds']['low'][0])*scale,canvas['y']+16+(p[1]-world['bounds']['low'][1])*scale)
  count=len(world['roads']);drag(road_pixel(start),road_pixel(end));wait(lambda:any(p['id']>=9000 for p in live()['settings']['roadPins']));local_generate();check(len(live()['world']['roads'])==count+1 and live()['world']['valid'],'dragging empty plan space builds an additional real road and navigation path');ImageGrab.grab().save(out/'world-curved-road.png')
  click('tab/Destruction');click('mask-preserve-building');wait(lambda:len(live()['settings']['influences'])==1);r=live()['widgets']['volume-max'];old=live()['settings']['influences'][0]['high'][2];ui.click(r['x']+r['width']*.72,r['y']+r['height']/2);ui.key('a',('ctrl',));ui.text(str(round(old+64)));ui.key('enter');wait(lambda:live()['settings']['influences'][0]['high'][2]==round(old+64));check(True,'volume height editing updates the saved XYZ field rather than only the plan rectangle')
  click('undo-settings');wait(lambda:live()['settings']['influences'][0]['high'][2]==old);check(True,'numeric volume edits undo in one settings transaction');check(planes(save('wa-local-preview'))==baseline,'local authoring and export preserve the active authored scene');print('World Architect local authoring UI:',len(checks),'checks passed',flush=True);sys.exit(0)
 if a.scenarios_only:
  from PIL import ImageStat
  records=[]
  for preset in ['checkpoint','factory','courtyard','overpass','mall','evacuation']:
   click('preset');click('preset/'+preset);click('generate');wait(lambda:live()['candidates'] and not live()['busy'] and live()['world']['preset']==preset);assert not live()['error'],live()['error']
   click('tab/Overview');ui.settle();time.sleep(.3);desktop=ImageGrab.grab().convert('RGB');r=live()['widgets']['preview'];ox,oy=ui.position();sx,sy=getattr(ui,'scale',(1,1));crop=desktop.crop((int(ox+r['x']*sx),int(oy+r['y']*sy),int(ox+(r['x']+r['width'])*sx),int(oy+(r['y']+r['height'])*sy)));crop.save(out/('scenario-'+preset+'.png'));check(max(ImageStat.Stat(crop).var)>20,'actual '+preset+' scenario has textured rendered architecture')
   records.append(live()['world']);click('tab/Quality');wait(lambda:'compare-plan/0' in live()['widgets']);count=min(4,live()['candidates']);check(count>=2 and all('compare-plan/'+str(i) in live()['widgets'] for i in range(count)),preset+' shows multiple actual candidate plans for comparison')
   chosen=count-1;click('compare-select/'+str(chosen));wait(lambda:live()['selected']==chosen);check(live()['world']['seed']==live()['candidateSeeds'][chosen],preset+' comparison selects the corresponding validated world')
  ImageGrab.grab().save(out/'world-candidate-comparison.png');check(planes(save('wa-scenario-preview'))==baseline,'scenario previews and candidate comparison preserve the authored scene');(out/'scenario-results.json').write_text(json.dumps(records,indent=2));print('World Architect scenarios UI:',len(checks),'checks passed',flush=True);sys.exit(0)
 if a.themes_only or a.art_update_only:
  from PIL import ImageStat
  records=[];shared=out/'shared-packs'
  for theme in ([] if a.art_update_only else ['brick','concrete','industrial','medical','metro','downtown']):
   click('tab/Dressing');click('art-theme');click('art-theme/'+theme);click('generate');wait(lambda:live()['candidates'] and not live()['busy'] and live()['world']['artTheme']==theme);assert not live()['error'],live()['error']
   click('tab/Overview');ui.settle();time.sleep(.3);desktop=ImageGrab.grab().convert('RGB');r=live()['widgets']['preview'];ox,oy=ui.position();sx,sy=getattr(ui,'scale',(1,1));crop=desktop.crop((int(ox+r['x']*sx),int(oy+r['y']*sy),int(ox+(r['x']+r['width'])*sx),int(oy+(r['y']+r['height'])*sy)));crop.save(out/('theme-'+theme+'.png'));check(max(ImageStat.Stat(crop).var)>20,'actual '+theme+' preview contains rendered art geometry')
   records.append({'theme':theme,'assetKey':live()['world']['assetKey'],'brushes':live()['world']['brushes'],'renderSha256':hashlib.sha256(crop.tobytes()).hexdigest()});click('tab/Export');dest=shared/('world-'+theme+'.map');click('export');choose(dest);wait(dest.exists)
  if not a.art_update_only:check(len({r['assetKey'] for r in records})==6 and len({r['renderSha256'] for r in records})==6,'all six live art grammars have distinct resources and actual rendered output')
  click('tab/Dressing');click('art-theme');click('art-theme/brick');click('generate');wait(lambda:live()['candidates'] and not live()['busy'] and live()['world']['artTheme']=='brick')
  if a.art_update_only:
   click('tab/Export');original=shared/'world-brick.map';click('export');choose(original);wait(original.exists)
  click('tab/Overview');project=out/'original-art.world.json';click('save-project');choose(project);wait(project.exists);art_project=json.loads(project.read_text())
  for theme in art_project['registry']['themes']:
   if theme['id']=='brick':theme['colors'][0][0]+=1
  changed=out/'changed-art.world.json';changed.write_text(json.dumps(art_project));old_files={str(f.relative_to(shared)):hashlib.sha256(f.read_bytes()).hexdigest() for f in shared.rglob('*') if f.is_file() and f.name!='shaderlist.txt'};click('open-project');choose(changed);wait(lambda:live()['candidates']==0);click('generate');wait(lambda:live()['candidates'] and not live()['busy']);click('tab/Export');dest=shared/'world-brick-updated.map';click('export');choose(dest);wait(dest.exists)
  check(all((shared/name).exists() and hashlib.sha256((shared/name).read_bytes()).hexdigest()==digest for name,digest in old_files.items()),'exporting an updated art dependency preserves every previous MAP, WAD, shader and texture')
  check(len(list(shared.glob('world-architect-brick-*.wad')))==2,'old and updated versions of the same theme coexist in one export folder')
  check(planes(save('wa-themed-preview'))==baseline,'art changes and repeated exports preserve the authored scene');(out/'theme-results.json').write_text(json.dumps(records,indent=2));print('World Architect theme UI:',len(checks),'checks passed',flush=True);sys.exit(0)
 if a.legacy_only:
  assert a.legacy_fixtures,'Pass the actual core-generated AutoLevel fixtures';fixtures=a.legacy_fixtures.resolve();expected=json.loads((fixtures/'expected.json').read_text());sources={name:(fixtures/name).read_bytes() for name in ['repository.txt','builder.txt','settings.txt']};seed_before=live()['settings']['worldSeed']
  click('legacy-panel');click('legacy-open');choose(fixtures/'repository.txt');wait(lambda:live().get('legacy',{}).get('sourceFormat')=='AUTOLEVEL_REPO')
  check(live()['legacy']['status']=='partial' and live()['settings']['worldSeed']==seed_before,'legacy repository inspection reports partial conversion and leaves generation settings unchanged')
  report=out/'repository-bundle/report.json';click('legacy-save');choose(report);wait(lambda:live()['legacySaved'] and report.exists())
  check((report.parent/'backup/original.txt').read_bytes()==sources['repository.txt'],'native migration saves a byte-exact original backup, including CRLF')
  check((report.parent/'modules/block-1.map').read_bytes()==expected['moduleMap'].encode(),'native migration extracts the original authored brush module and point entity unchanged')
  parsed=json.loads(report.read_text());check(parsed['convertedModules'][0]['materialDependencies']==['legacy/original'] and parsed['unsupported'],'migration report retains asset dependencies and lists unsupported solver features')
  click('legacy-use-module');wait(lambda:len(live()['settings']['authoredModules'])==1);check(live()['settings']['authoredModules'][0]['map']==expected['moduleMap'],'verified legacy brush geometry can be explicitly imported into the procedural module library')
  click('legacy-open');choose(fixtures/'builder.txt');wait(lambda:live()['legacy']['sourceFormat']=='AUTOLEVEL_BUILDER');check(not live()['legacySaved'] and live()['settings']['worldSeed']==seed_before,'inspecting a builder does not silently copy its seed before backup publication')
  builder_report=out/'builder-bundle/report.json';click('legacy-save');choose(builder_report);wait(lambda:live()['legacySaved'] and builder_report.exists());click('legacy-seed');wait(lambda:live()['settings']['worldSeed']==23);check(True,'explicit seed conversion becomes available after verified backup publication')
  click('undo-settings');wait(lambda:live()['settings']['worldSeed']==seed_before);check(True,'copying a migrated seed participates in settings Undo')
  check(all((fixtures/name).read_bytes()==data for name,data in sources.items()) and planes(save('wa-legacy-preview'))==baseline,'legacy inspection, bundle publication and seed Undo preserve original files and authored map geometry')
  ImageGrab.grab().save(out/'world-legacy-migration.png');(out/'world-state.json').write_text(json.dumps(live(),indent=2));print('World Architect legacy UI:',len(checks),'checks passed',flush=True);sys.exit(0)
 if a.paint_only:
  click('generate');wait(lambda:live()['candidates'] and not live()['busy']);verify_paint(0);check(planes(save('wa-paint-preview'))==baseline,'painting and settings Undo leave the authored map intact');ImageGrab.grab().save(out/'world-painted-fields.png');print('World Architect paint UI:',len(checks),'checks passed',flush=True);sys.exit(0)
 if a.designer_only:
  assert a.preset=='hospital'
  def generate():
   click('generate');wait(lambda:live()['candidates'] and not live()['busy']);assert not live()['error'],live()['error']
  def export_design(name):
   click('tab/Export');dest=out/name/'world.map';click('export');choose(dest);wait(dest.exists);return dest
  generate();original=export_design('original');nodes=len(live()['world']['missionNodes'])
  for tab in ['Overview','Mission Graph','Site & Roads','Buildings','Architecture','Destruction','Dressing','Quality','Export']:click('tab/'+tab)
  check(True,'all nine requested workspace panels receive actual OS input')
  click('tab/Architecture');r=live()['widgets']['architecture-width'];ui.click(r['x']+r['width']*.3,r['y']+r['height']/2,mods=('ctrl',));ui.key('a',('ctrl',));ui.text('1920');ui.key('enter');wait(lambda:live()['settings']['architecture']['width']==1920)
  generate();wider=export_design('wider');check(planes(wider)!=planes(original),'architecture dimensions change actual exported brush planes')
  click('undo-settings');wait(lambda:live()['settings']['architecture']['width']==0);generate();restored=export_design('restored');check(planes(restored)==planes(original),'architecture settings Undo restores the original deterministic brushes')
  click('tab/Mission Graph');world=live()['world'];beat=next(n for n in world['missionNodes'] if n['role']=='Introduction');room=next(r for r in world['rooms'] if r['floor']==0 and r['id']!=beat['room']);canvas=live()['widgets']['plan-canvas'];low=world['bounds']['low'];high=world['bounds']['high'];scale=min((canvas['width']-32)/(high[0]-low[0]),(canvas['height']-32)/(high[1]-low[1]))
  def canvas_point(p):return (canvas['x']+16+(p[0]-low[0])*scale,canvas['y']+16+(p[1]-low[1])*scale)
  target=[(room['low'][i]+room['high'][i])/2 for i in range(3)];drag(canvas_point(beat['position']),canvas_point(target));wait(lambda:any(p['id']==beat['id'] for p in live()['settings']['missionPins']));generate()
  check(next(n for n in live()['world']['missionNodes'] if n['id']==beat['id'])['room']==room['id'],'dragging a graph beat reserves the actual target room in the regenerated geometry')
  click('mission-add');wait(lambda:len(live()['settings']['missionGraph']['nodes'])==nodes+1);click('mission-one-way');wait(lambda:live()['settings']['missionGraph']['edges'][0]['oneWay'])
  generate();check(len(live()['world']['missionNodes'])==nodes+1 and live()['world']['valid'],'mission authoring produces an additional physically reachable room beat and a valid directed traversal');mission_edited=export_design('mission-edited')
  click('tab/Destruction');click('mask-preserve-building');wait(lambda:len(live()['settings']['influences'])==1);generate();preserved=export_design('preserved');check(planes(preserved)!=planes(mission_edited),'preserve fields change real generated architecture and dressing')
  click('tab/Overview');project=out/'designer.world.json';click('save-project');choose(project);wait(project.exists);saved=json.loads(project.read_text());check(saved['missionGraph']['edges'][0]['oneWay'] and saved['influences'][0]['kind']=='preserve','editable project persists authored graph transitions and spatial fields')
  click('open-project');choose(project);wait(lambda:live()['candidates']==0);generate();replayed=export_design('replayed');check(planes(replayed)==planes(preserved),'saved designer project reproduces the exact exported architecture after reopen')
  verify_paint(1)
  check(planes(save('wa-designer-preview'))==baseline,'designer edits, generation and export leave the authored scene intact')
  click('tab/Mission Graph');ImageGrab.grab().save(out/'world-mission-editor.png');(out/'world-state.json').write_text(json.dumps(live(),indent=2));print('World Architect designer UI:',len(checks),'checks passed',flush=True);sys.exit(0)
 if a.pins_only:
  assert a.preset=='evacuation','Pin fixture needs independently generated buildings'
  click('preset');click('preset/evacuation');click('generate');wait(lambda:live()['candidates'] and not live()['busy'])
  def export_pin(name):
   click('tab/Export');dest=out/name/'world.map';click('export');choose(dest);wait(dest.exists);return regions(dest)
  original=export_pin('before-pin');click('tab/Buildings');click('detail-building');click('detail-building/101');click('pin-building');wait(lambda:live()['settings']['pinnedRegions']==[101])
  check(all(live()['settings']['stageLocks'][:2]),'pinning through actual UI freezes the shared Mission and Site')
  click('undo-settings');wait(lambda:not live()['settings']['pinnedRegions']);click('redo-settings');wait(lambda:live()['settings']['pinnedRegions']==[101])
  check(True,'pinning participates in ordinary settings Undo and Redo')
  click('seed');ui.key('a',('ctrl',));ui.text('57');ui.key('enter');click('generate');wait(lambda:live()['candidates'] and not live()['busy']);assert not live()['error'],live()['error']
  changed=export_pin('after-pin')
  check(changed['101']['geometry']==original['101']['geometry'] and changed['101']['points']==original['101']['points'],'a pinned building keeps exact exported brushes and point entities after regeneration')
  check(any(changed[k]['geometry']!=original[k]['geometry'] for k in ['100','102','103']),'other buildings still vary under a new global seed')
  click('tab/Overview');project=out/'pinned.world.json';click('save-project');choose(project);wait(project.exists);click('open-project');choose(project);wait(lambda:live()['candidates']==0);click('generate');wait(lambda:live()['candidates'] and not live()['busy'])
  replay=export_pin('reopened-pin');check(replay==changed,'saved project reload restores the accepted pinned world without an in-memory candidate')
  click('tab/Buildings');click('pin-building');wait(lambda:not live()['settings']['pinnedRegions']);check(not live()['settings']['regionRecipes'],'unpin removes the building recipes through actual UI')
  check(planes(save('wa-pin-preview'))==baseline,'pin, regenerate, export and project replay retain the authored map')
  ImageGrab.grab().save(out/'world-pins.png');(out/'world-state.json').write_text(json.dumps(live(),indent=2));print('World Architect pin UI:',len(checks),'checks passed',flush=True);sys.exit(0)
 if a.regions_only:
  assert a.preset=='evacuation','Regional fixture needs multiple buildings'
  click('preset');click('preset/evacuation');click('generate');wait(lambda:live()['candidates'] and not live()['busy']);seeds=live()['candidateSeeds'];assert 1 in seeds;click('candidate/'+str(seeds.index(1)))
  click('tab/Export');click('apply');wait(lambda:live()['applyStatus']=='ok' and not live()['request']);original=save('wa-regions-original');old=regions(original)
  check({'100','101','102','103'}<=set(old),'actual editor imports four separately owned building groups')
  check(all(len(v['fields'].get('_wa_baseline',''))==64 for v in old.values()),'ownership baselines are computed from the executed editor contents')
  click('tab/Buildings');click('detail-building');click('detail-building/101');r=live()['widgets']['detail-seed'];ui.click(r['x']+r['width']*.4,r['y']+r['height']/2);ui.key('a',('ctrl',));ui.text('57');ui.key('enter')
  click('regenerate-region');wait(lambda:live()['candidates']==1 and not live()['busy']);click('tab/Export');click('update-region');wait(lambda:live()['applyStatus']=='ok' and not live()['request']);updated=save('wa-regions-updated');new=regions(updated)
  check(new['101']['geometry']!=old['101']['geometry'],'selected building details change in the actual map')
  check(len(new['101']['geometry'])>100,'regenerated building retains real brushes inside its owned group')
  check(entities(original)[0][:2]==entities(updated)[0][:2],'regional replacement preserves worldspawn geometry and properties exactly')
  check(all(new[k]==v for k,v in old.items() if k!='101'),'regional Apply preserves every unselected group, point entity and brush')
  check(new['101']['fields']['_tb_id']==old['101']['fields']['_tb_id'],'regional Apply retains the original group identity')
  check(all(line in planes(updated) for line in baseline),'regional Apply retains the original authored scene')
  click('apply');wait(lambda:live()['applyStatus']=='exists' and not live()['request']);check(regions(save('wa-regions-duplicate'))==new,'adding a duplicate project fails without changing the map')
  console('ftebroom_undo');check(regions(save('wa-regions-undo'))==old,'one Undo restores the complete previous regional contents')
  console('ftebroom_redo');check(regions(save('wa-regions-redo'))==new,'one Redo restores only the regenerated region')
  tree=[shlex.split(line) for line in (data/'data/ftebroom-ui-tree.txt').read_text().splitlines()];target=next(row[1] for row in tree if row[0]=='2' and row[5]=='World Architect region 101');console('ftebroom_cancel');console('ftebroom_organize focus '+target);console('ftebroom_move 8 0 0');manual=save('wa-regions-manual');manual_regions=regions(manual);check(manual_regions['101']['geometry']!=new['101']['geometry'],'ordinary editor transform creates real manual region edits')
  click('update-region');wait(lambda:live()['applyStatus']=='modified' and not live()['request']);check(regions(save('wa-regions-rejected'))==manual_regions,'manual region edits are rejected before replacement')
  console('ftebroom_undo');check(regions(save('wa-regions-rejected-undo'))==new,'rejected replacement creates no additional Undo transaction')
  epoch=live()['mapEpoch'];console('mapedit maps/wa-regions-updated.map');wait(lambda:live()['mapEpoch']!=epoch)
  reopened=save('wa-regions-reopened');click('update-region');wait(lambda:not live()['request'])
  check(live()['applyStatus']=='ok','saved and reopened generated regions retain a valid replacement baseline')
  check(entities(reopened)[0][:2]==entities(save('wa-regions-reopened-update'))[0][:2],'reopened regional update leaves worldspawn unchanged')
  tree=[shlex.split(line) for line in (data/'data/ftebroom-ui-tree.txt').read_text().splitlines()];target=next(row[1] for row in tree if row[0]=='2' and row[5]=='World Architect region 101')
  console('ftebroom_cancel');console('ftebroom_organize focus '+target);console('ftebroom_group "Authored container"');parented=save('wa-regions-parented');parent=regions(parented)['101']['fields'].get('_tb_group')
  check(parent and any(f.get('_tb_id')==parent and f.get('_tb_name')=='Authored container' for f,g,b in entities(parented)),'ordinary grouping places the generated building inside an authored container')
  click('update-region');wait(lambda:not live()['request']);parented=save('wa-regions-parented-updated')
  check(live()['applyStatus']=='ok' and regions(parented)['101']['fields'].get('_tb_group')==parent,'regional update retains authored parent membership')
  # Lock a saved generated group and load it through the ordinary map command.
  locked=data/'maps/wa-regions-locked.map';text=updated.read_text()
  for fields,geometry,block in entities(updated):
   if fields.get('_wa_region')=='101':text=text.replace(block,block.replace('{','{\n"_tb_layer_locked" "1"',1),1)
  locked.write_text(text);epoch=live()['mapEpoch'];console('mapedit maps/wa-regions-locked.map');wait(lambda:live()['mapEpoch']!=epoch);locked_before=planes(save('wa-regions-locked-before'));click('update-region');wait(lambda:live()['applyStatus']=='locked' and not live()['request'])
  check(planes(save('wa-regions-lock-rejected'))==locked_before,'locked generated regions survive a replacement request')
  custom=data/'maps/wa-regions-custom-model.map';text=updated.read_text();changed=False
  for fields,geometry,block in entities(updated):
   if fields.get('_tb_group')==new['101']['fields']['_tb_id'] and fields.get('classname')=='light':
    text=text.replace(block,block.replace('{','{\n"model" "models/qa-custom.mdl"',1),1);changed=True;break
  assert changed,'Missing point entity for ownership negative control'
  custom.write_text(text);epoch=live()['mapEpoch'];console('mapedit maps/wa-regions-custom-model.map');wait(lambda:live()['mapEpoch']!=epoch);custom_before=regions(save('wa-regions-custom-before'));click('update-region');wait(lambda:not live()['request'])
  check(live()['applyStatus']=='modified' and regions(save('wa-regions-custom-rejected'))==custom_before,'manual point-entity model properties cannot be overwritten by region replacement')
  tampered=data/'maps/wa-regions-tampered.map';text=updated.read_text()
  for fields,geometry,block in entities(updated):
   if fields.get('_wa_region')=='101':text=text.replace(block,re.sub(r'("_wa_snapshot_0"\s+")[^"]',r'\g<1>X',block,count=1),1)
  tampered.write_text(text);epoch=live()['mapEpoch'];console('mapedit maps/wa-regions-tampered.map');wait(lambda:live()['mapEpoch']!=epoch);tampered_before=planes(save('wa-regions-tampered-before'));click('update-region');wait(lambda:not live()['request'])
  check(live()['applyStatus']=='modified' and planes(save('wa-regions-tampered-rejected'))==tampered_before,'a corrupt ownership snapshot cannot authorize replacement')
  epoch=live()['mapEpoch'];console('mapedit maps/wa-regions-parented-updated.map');wait(lambda:live()['mapEpoch']!=epoch);whole_before=save('wa-whole-before')
  click('freeze');click('freeze/0');click('preset');click('preset/hospital');click('generate');wait(lambda:live()['candidates'] and not live()['busy']);click('tab/Export');click('update-world');wait(lambda:not live()['request']);whole=save('wa-whole-hospital')
  check(live()['applyStatus']=='ok' and set(regions(whole))=={'0','100'},'whole-project update removes obsolete building and street regions')
  check(entities(whole_before)[0][:2]==entities(whole)[0][:2] and any(f.get('_tb_id')==parent and f.get('_tb_name')=='Authored container' for f,g,b in entities(whole)),'whole-project update retains worldspawn and the emptied authored container')
  click('preset');click('preset/evacuation');click('generate');wait(lambda:live()['candidates'] and not live()['busy']);click('tab/Export');click('update-world');wait(lambda:not live()['request']);expanded=save('wa-whole-evacuation')
  check(live()['applyStatus']=='ok' and {'100','101','102','103'}<=set(regions(expanded)),'whole-project update adds newly generated building regions')
  ImageGrab.grab().save(out/'world-regions.png');(out/'world-state.json').write_text(json.dumps(live(),indent=2));print('World Architect regional UI:',len(checks),'checks passed',flush=True);sys.exit(0)
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
 click('tab/Overview');saved_project=out/'saved.world.json';click('save-project');choose(saved_project);wait(saved_project.exists);saved=json.loads(saved_project.read_text())
 check(saved['selectedCandidateSeed']==chosen and len(saved['registry']['presets'])==8,'project saves the selected candidate and complete preset registry snapshot')
 click('open-project');choose(saved_project);wait(lambda:live()['candidates']==0);click('generate');wait(lambda:live()['candidates'] and not live()['busy'])
 check(live()['world']['seed']==chosen,'project reopen and regeneration restore the exact selected candidate')
 click('tab/Overview');time.sleep(1);desktop=ImageGrab.grab().convert('RGB');desktop.save(out/'world-preview.png')
 preview=live()['widgets']['preview'];ox,oy=ui.position();sx,sy=getattr(ui,'scale',(1,1));box=(int(ox+preview['x']*sx),int(oy+preview['y']*sy),int(ox+(preview['x']+preview['width'])*sx),int(oy+(preview['y']+preview['height'])*sy))
 visible=sum(g>r*1.01 and g>b*1.02 and 50<g<210 for r,g,b in desktop.crop(box).getdata())
 check(visible>1000,'3D preview contains actual textured architecture instead of a missing render target')
 click('tab/Site & Roads');time.sleep(.5);ImageGrab.grab().save(out/'world-site-plan.png')
 click('tab/Quality');time.sleep(.5);ImageGrab.grab().save(out/'world-quality.png')
 click('tab/Export');exported=out/'exported/world.map';exported.parent.mkdir(exist_ok=True);click('export');choose(exported);wait(exported.exists)
 check('"worldspawn"' in exported.read_text() and (exported.parent/'world.world.json').exists(),'export writes actual brush MAP and a versioned editable project')
 check(any(exported.parent.glob('world-architect-*.wad')) and any((exported.parent/'textures').glob('*.tga')),'portable export includes real original WAD and texture assets')
 if a.preset=='metro':check(any('surfaceparm water' in shader.read_text() for shader in (exported.parent/'scripts').glob('world-architect-metro-*.shader')) and any(name in exported.read_text() for name in ['wa_water_metro','*wametro','!wametro']),'flooded platform exports its liquid MAP material and shader assets')
 click('apply');wait(lambda:'One map Undo' in live()['message']);check(not live()['request'],'Apply waits for an executed QC map transaction acknowledgment')
 applied=save('wa-applied');check(len(planes(applied))>len(baseline)+100,'Apply inserts editable generated brushes')
 check(all(line in planes(applied) for line in baseline),'Apply retains every original authored brush plane')
 compile_map=data/'data/export-map.map';compile_map.unlink(missing_ok=True);console('ftebroom_export_map');wait(compile_map.exists);shutil.copy2(compile_map,out/'wa-compile.map')
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
  locked_seed=live()['world']['seed'];locked_project=out/'locked.world.json';click('tab/Overview');click('save-project');choose(locked_project);wait(locked_project.exists);click('open-project');choose(locked_project);wait(lambda:live()['candidates']==0);click('generate');wait(lambda:live()['candidates'] and not live()['busy'])
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
