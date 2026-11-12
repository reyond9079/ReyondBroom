#!/usr/bin/env python3
"""Actual OS Space input in every pane and real per-stage onboarding map loads."""
import argparse,json,os,subprocess,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
data=root/'ftebroom';state=data/'placement-ui-live.json';focus=data/'native-input-live.json';lesson=data/'onboarding-live.json'
for path in (state,focus,lesson,data/'qconsole.log',root/'qconsole.log'):path.unlink(missing_ok=True)
(data/'editor133.cfg').write_text('exec qa-clip.cfg\nset ftebroom_input_qa 1\nset ftebroom_placement_qa 1\nin 12 echo EDITOR133_READY\n')
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64');stream=(out/'editor.log').open('wb')
process=subprocess.Popen([str(engine),*render,'-basedir',str(root),'-nohome','-noupdates','-nosound','-condebug','-window','-width','1280','-height','800','+set','ftebroom_skip_setup','1','+exec','editor133.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT)
checks=[];trace=[]
def live():return read_ui_json(state)
def lesson_live():return read_ui_json(lesson)
def logs():
 for path in (data/'qconsole.log',root/'qconsole.log'):
  if path.exists():return path.read_text(errors='replace')
 return (out/'editor.log').read_text(errors='replace')
def wait(fn,timeout=90):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  assert process.poll() is None,'Editor exited'
  if any(marker in logs() for marker in ['FTEBROOM_LAYOUT_FAIL','FTEBROOM_TUTORIAL_FAIL']):raise AssertionError('Native input/onboarding check failed; see combined.log')
  try:
   if value:=fn():return value
  except (FileNotFoundError,PermissionError,KeyError,json.JSONDecodeError):pass
  time.sleep(.06)
 raise AssertionError('Timed out; see editor.log and failure screenshot')
def check(ok,label):
 assert ok,label
 checks.append(label);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',label,flush=True)
# Engine input telemetry keeps advancing during the onboarding's dedicated UI.
ui=NativeInput(process,focus,live,wait,out,focus_required=True)
def lesson_click(name):ui.click(*wait(lambda:lesson_live()['widgets'].get(name)))
def button(down,number=1):
 if os.name=='nt':ui.user.mouse_event((2 if down else 4) if number==1 else (8 if down else 16),0,0,0,0)
 else:ui.xtest.fake_input(ui.d,ui.X.ButtonPress if down else ui.X.ButtonRelease,number);ui.d.sync()
 ui.settle()
def drag(start,end):
 ui.move(*start);button(True)
 try:
  for i in range(1,9):ui.move(start[0]+(end[0]-start[0])*i/8,start[1]+(end[1]-start[1])*i/8)
 finally:button(False)
 ui.settle()
try:
 wait(lambda:'EDITOR133_READY' in logs() and live()['views']);ui.position()
 ui.console('set ftebroom_view_layout 4')
 wait(lambda:len([v for v in live()['views'] if v['width']>0 and v['height']>0])==4)
 for pane in range(4):
  before=[v['type'] for v in live()['views']];v=live()['views'][pane]
  ui.move(v['x']+v['width']*.6,v['y']+v['height']*.6)
  ui.key('space');wait(lambda:live()['views'][pane]['type']=={0:3,3:2,2:1,1:0}[before[pane]])
  after=[v['type'] for v in live()['views']]
  check(all(after[i]==before[i] for i in range(4) if i!=pane),f'Space cycles only hovered pane {pane}')
  for turn in range(3):
   old=live()['views'][pane]['type'];ui.key('space');wait(lambda:live()['views'][pane]['type']=={0:3,3:2,2:1,1:0}[old])
   row={'pane':pane,'turn':turn,'expected_wrap':before,'types':[v['type'] for v in live()['views']]}
   if os.name!='nt':
    point=ui.d.screen().root.query_pointer();row['cursor']=[point.root_x,point.root_y]
   trace.append(row);(out/'cycle-trace.json').write_text(json.dumps(trace,indent=2))
  check([v['type'] for v in live()['views']]==before,f'Space wraps all four projections in pane {pane}')
 ImageGrab.grab().save(out/'four-viewports.png')
 ui.console('ftebroom_test_layout')
 wait(lambda:'FTEBROOM_LAYOUT_PASS divider_drag' in logs())
 check('FTEBROOM_LAYOUT_FAIL' not in logs(),'native layout/focus/projection checks pass')
 ui.console('set ftebroom_followup133_qa 1')
 for step in [0,1,2,3,4,5,12,3]:
  marker=f'FTEBROOM_TUTORIAL_PASS native_stage_fixture_{step}'
  previous=logs().count(marker)
  ui.console('ftebroom_test_desktop rb10tourstage '+str(step))
  wait(lambda:logs().count(marker)>previous)
  check('FTEBROOM_TUTORIAL_FAIL' not in logs(),f'onboarding stage {step} loads only its target fixture'+(' on replay' if previous else ''))
  time.sleep(.3);ui.settle()
  ImageGrab.grab().save(out/f'onboarding-stage-{step}.png')
 ui.console('ftebroom_test_desktop rb10tourstage 6')
 wait(lambda:lesson_live()['step']==6 and lesson_live()['ready'] and len(lesson_live()['views'])==1)
 ui.move(860,500);ui.key('space')
 check(not lesson_live()['done'] and lesson_live()['views'][0]['type']==0,'lesson requires a layout change before Space')
 lesson_click('layout');lesson_click('layout/4')
 wait(lambda:len(lesson_live()['views'])==4)
 before=[v['type'] for v in lesson_live()['views']];v=lesson_live()['views'][3]
 ui.move(v['x']+v['width']*.6,v['y']+v['height']*.6);ui.key('space')
 wait(lambda:lesson_live()['done'])
 after=[v['type'] for v in lesson_live()['views']]
 check(after[:3]==before[:3] and after[3]==0,'onboarding layout uses real viewports and Space cycles only the hovered one')
 ImageGrab.grab().save(out/'onboarding-four-views.png')
 lesson_click('next')
 wait(lambda:lesson_live()['step']==7 and lesson_live()['ready'] and len(lesson_live()['targets'])==2)
 check(len(lesson_live()['views'])==1 and lesson_live()['views'][0]['type']==0,'Clip starts in one perspective viewport')
 for tool in (0,1,3,7,8,9,5,6):lesson_click(f'tool/{tool}')
 check(not lesson_live()['chosen'] and not lesson_live()['tool'],'all tools except Clip are disabled on the lesson toolbar')
 clips=logs().count('ReyondBroom: clipped 1 brushes.')
 restored=logs().count('FTEBROOM_TUTORIAL_PASS native_stage_fixture_7')
 lesson_click('tool/2');wait(lambda:lesson_live()['demo'] and lesson_live()['tool']==2)
 wait(lambda:lesson_live()['ready'] and not lesson_live()['demo'] and logs().count('FTEBROOM_TUTORIAL_PASS native_stage_fixture_7')>restored)
 check(logs().count('ReyondBroom: clipped 1 brushes.')==clips+1 and not lesson_live()['done'] and lesson_live()['clipPoints']==0,'animated Clip commits a real cut, then restores exactly one selected uncut brush')
 targets=lesson_live()['targets'];ui.click(*targets[0]);wait(lambda:lesson_live()['clipPoints']==1)
 check(sum((a-b)**2 for a,b in zip(*targets))>18**2,'Clip targets cannot overlap an existing point handle')
 ui.key('enter');check(not lesson_live()['done'],'Enter with one point cannot pass the lesson')
 ui.click(*targets[1]);wait(lambda:lesson_live()['clipPoints']==2)
 ImageGrab.grab().save(out/'onboarding-clip-points.png')
 ui.key('enter');wait(lambda:lesson_live()['done'])
 check(logs().count('ReyondBroom: clipped 1 brushes.')==clips+2,'practice finishes only after a real two-point Clip cut')
 lesson_click('next');wait(lambda:lesson_live()['step']==8 and lesson_live()['ready'])
 check(not lesson_live()['done'] and not lesson_live()['tool'],'Vertex selection is a separate required step')
 lesson_click('tool/2');check(not lesson_live()['done'],'Clip cannot pass the Vertex selection step')
 lesson_click('tool/7');wait(lambda:lesson_live()['done'] and lesson_live()['tool']==7)
 ImageGrab.grab().save(out/'onboarding-vertex-select.png')
 lesson_click('next');wait(lambda:lesson_live()['step']==9 and lesson_live()['ready'] and not lesson_live()['demo'])
 check('FTEBROOM_TUTORIAL_EDIT step=9 demo=1 command=ftebroom_pointer' in logs(),'Vertex demonstration commits a real geometry edit')
 check(not lesson_live()['done'] and lesson_live()['tool']==7,'Vertex demo restores fixture and leaves the tool active for practice')
 targets=lesson_live()['targets'];ui.click(*targets[0]);check(not lesson_live()['done'],'selecting a vertex without moving it cannot pass')
 drag(*targets);wait(lambda:lesson_live()['done'])
 check('FTEBROOM_TUTORIAL_EDIT step=9 demo=0 command=ftebroom_pointer' in logs(),'practice requires a committed vertex move')
 ImageGrab.grab().save(out/'onboarding-vertex-practice.png')
 lesson_click('next');wait(lambda:lesson_live()['step']==10 and lesson_live()['ready'] and not lesson_live()['demo'])
 check('FTEBROOM_TUTORIAL_EDIT step=10 demo=1 command=ftebroom_material' in logs(),'Material Browser demo applies a real material')
 check(not lesson_live()['done'] and lesson_live()['selected']==0,'material demo restores an unselected brush')
 lesson_click('tile/textures/ftebroom/qa_green');check(not lesson_live()['done'],'choosing a texture without a selected brush cannot pass')
 ui.click(*lesson_live()['targets'][0]);wait(lambda:lesson_live()['selected']==1)
 lesson_click('tile/textures/ftebroom/qa_red');wait(lambda:lesson_live()['done'])
 ImageGrab.grab().save(out/'onboarding-material-practice.png')
 lesson_click('next');wait(lambda:lesson_live()['step']==11 and lesson_live()['ready'] and not lesson_live()['demo'])
 check(not lesson_live()['done'] and lesson_live()['selected']==0,'face demo restores an unselected practice brush')
 target=lesson_live()['targets'][0];ui.click(*target);check(not lesson_live()['done'],'whole-brush selection cannot pass the face lesson')
 ui.click(*target,mods=('shift',));wait(lambda:lesson_live()['done'])
 check(lesson_live()['faces']==1,'Shift-click selects exactly one face')
 ImageGrab.grab().save(out/'onboarding-face-practice.png')
 lesson_click('next');wait(lambda:lesson_live()['step']==12 and lesson_live()['ready'] and lesson_live()['done'])
 check(len(lesson_live()['views'])==1,'final tools page still has one viewport')
 previous=state.stat().st_mtime_ns
 lesson_click('next');wait(lambda:state.stat().st_mtime_ns!=previous and len(live()['views'])==4)
 check(True,'finishing restores the saved four-view editor workspace')
 (out/'combined.log').write_text(logs())
 assert not any(x in logs() for x in ['_FAIL ','CSQC_Abort','Host_Error','runaway loop'])
 (out/'provenance.json').write_text(json.dumps({'build_info':(root/'build-info.txt').read_text(),'method':'Actual OS keyboard input; native stage transitions load disposable maps; independent QC brush counts; unedited screenshots'},indent=2))
except Exception:
 ImageGrab.grab().save(out/'failure.png');(out/'combined.log').write_text(logs());(out/'failure-state.json').write_text(json.dumps(live(),indent=2));raise
finally:
 if process.poll() is None:process.kill();process.wait(timeout=15)
 stream.close()
