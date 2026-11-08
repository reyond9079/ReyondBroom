#!/usr/bin/env python3
"""Run opt-in QC/native RB-10 regressions in disposable packaged runtimes."""
import argparse, hashlib, json, os, re, shutil, subprocess, sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
repo=Path(__file__).resolve().parents[1];source=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
suites={
 'tutorial132':('FTEBROOM_TUTORIAL_PASS',7),
 'clipboard-attributes132':('FTEBROOM_CLIPATTRS_PASS',12),
 'view-options132':('FTEBROOM_VIEWOPTS_PASS',29),
 'native132':('FTEBROOM_',12),
}
results=[]
for name,(prefix,minimum) in suites.items():
 root=out/name;shutil.copytree(source,root);data=root/'ftebroom'
 for stale in (root/'qconsole.log',data/'qconsole.log'):stale.unlink(missing_ok=True)
 if name=='native132':
  cfg='''set ftebroom_skip_setup 1
exec ftebroom.cfg
in 6 ftebroom_test_desktop rb10guards
in 7 ftebroom_test_workspace rb10tools
in 9 ftebroom_test_desktop rb10tourstage 5
in 13 ftebroom_test_desktop rb10tourcheck
in 15 quit force
'''
 else:cfg=(repo/'tests'/(name+'.cfg')).read_text()
 (data/'rb10-runtime.cfg').write_text(cfg)
 engine=root/('bin/fteqw64.exe' if os.name=='nt' else 'fteqw-gl64')
 render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
 log=out/(name+'.log')
 with log.open('wb') as stream:
  process=subprocess.run([str(engine),*render,'+set','ftebroom_skip_setup','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-window','-width','1280','-height','800','-condebug','-basedir',str(root),'+exec','rb10-runtime.cfg'],cwd=root,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'},stdout=stream,stderr=subprocess.STDOUT,timeout=100)
 text=log.read_text(errors='replace')
 console=data/'qconsole.log'
 if console.exists():text+='\n'+console.read_text(errors='replace')
 assert process.returncode==0,(name,process.returncode,text[-4000:])
 assert not any(marker in text for marker in ['_FAIL ','CSQC_Abort','runaway loop error','Host_Error','Program error']), (name,text[-8000:])
 lines=(re.sub(r'^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} ', '', line) for line in text.splitlines())
 passed=sorted(set(line.strip() for line in lines if line.startswith(prefix) and '_PASS ' in line))
 assert len(passed)>=minimum,(name,passed,text[-4000:])
 if name=='native132':
  for required in ['rb10_closed_plant_opens_without_hidden_confirmation','rb10_dirty_open_plant_preserves_confirmation','rb10_tools_default_top','rb10_tools_compact_content_fits','rb10_tools_expansion_grows_height','rb10_tools_collapse_reclaims_height','rb10_tools_custom_side_dock_preserved','rb10_tutorial_every_stage_requests_fresh_map','rb10_tutorial_stage_map_ready','rb10_tutorial_final_stage','rb10_tutorial_final_stage_has_no_selected_practice_brush','rb10_tutorial_wheel_indicator_rotates']:
   assert any(line.endswith(' '+required) for line in passed),(name,required,passed)
 results.append({'suite':name,'checks':passed,'qc_sha256':hashlib.file_digest((data/'csaddon.dat').open('rb'),'sha256').hexdigest()});(out/'results.json').write_text(json.dumps(results,indent=2))
 print('PASS',name,len(passed),flush=True)
