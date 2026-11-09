#!/usr/bin/env python3
"""Actual OS Space input in every pane and real per-stage onboarding map loads."""
import argparse,json,os,subprocess,time
from pathlib import Path
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
root,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
data=root/'ftebroom';state=data/'placement-ui-live.json';focus=data/'native-input-live.json'
for path in (state,focus,data/'qconsole.log',root/'qconsole.log'):path.unlink(missing_ok=True)
(data/'editor133.cfg').write_text('exec qa-clip.cfg\nset ftebroom_input_qa 1\nset ftebroom_placement_qa 1\nin 12 echo EDITOR133_READY\n')
render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64');stream=(out/'editor.log').open('wb')
process=subprocess.Popen([str(engine),*render,'-basedir',str(root),'-nohome','-noupdates','-nosound','-condebug','-window','-width','1280','-height','800','+set','ftebroom_skip_setup','1','+exec','editor133.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT)
checks=[];trace=[]
def live():return read_ui_json(state)
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
 for step in [0,1,2,3,4,5,6,3]:
  marker=f'FTEBROOM_TUTORIAL_PASS native_stage_fixture_{step}'
  previous=logs().count(marker)
  ui.console('ftebroom_test_desktop rb10tourstage '+str(step))
  wait(lambda:logs().count(marker)>previous)
  check('FTEBROOM_TUTORIAL_FAIL' not in logs(),f'onboarding stage {step} loads only its target fixture'+(' on replay' if previous else ''))
  time.sleep(.3);ui.settle()
  ImageGrab.grab().save(out/f'onboarding-stage-{step}.png')
 (out/'combined.log').write_text(logs())
 assert not any(x in logs() for x in ['_FAIL ','CSQC_Abort','Host_Error','runaway loop'])
 (out/'provenance.json').write_text(json.dumps({'build_info':(root/'build-info.txt').read_text(),'method':'Actual OS keyboard input; native stage transitions load disposable maps; independent QC brush counts; unedited screenshots'},indent=2))
except Exception:
 ImageGrab.grab().save(out/'failure.png');(out/'combined.log').write_text(logs());(out/'failure-state.json').write_text(json.dumps(live(),indent=2));raise
finally:
 if process.poll() is None:process.kill();process.wait(timeout=15)
 stream.close()
