#!/usr/bin/env python3
# Capture native screenshots after test-audio-editor-linux.sh has created updated.rbaudio.json.
import os,subprocess,shutil,sys
from pathlib import Path
bundle=Path(sys.argv[1]).resolve()
qa=Path(sys.argv[2]).resolve()
cases=[('compact',1280,800,None),('light',1920,1080,'audio4light'),('retro',1920,1080,'audio4retro'),('scale150',1920,1200,'audio4scale150'),('scale200',2560,1600,'audio4scale200'),('shortcuts',1280,800,'audio4shortcuts')]
env=dict(os.environ,LIBGL_ALWAYS_SOFTWARE='1')
for name,width,height,mode in cases:
 cfg=f'exec ftebroom.cfg\nset ftebroom_skip_setup 1\nset ftebroom_test_import_root "{qa}"\nin 5 ftebroom_test_desktop audio4preview\nin 8 ftebroom_test_desktop audio4previewready\n'
 if mode:cfg+=f'in 9 ftebroom_test_desktop {mode}\n'
 cfg+=f'in 10 screenshot audio4-{name}.png\nin 12 quit force\n'
 (bundle/'ftebroom/audio-visual.cfg').write_text(cfg)
 with (qa/f'visual-{name}.log').open('w') as log:
  p=subprocess.run([str(bundle/'fteqw-gl64'),'-ftebroom-child-processes','-nohome','-noupdates','-nosound','-window','-width',str(width),'-height',str(height),'-basedir',str(bundle),'+exec','ftebroom.cfg','+exec','audio-visual.cfg'],cwd=bundle,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=30)
  assert p.returncode==0,(name,p.returncode)
 text=(qa/f'visual-{name}.log').read_text();assert 'DESKTOP_FAIL' not in text and 'DESKTOP_PASS audio4_worker_completed' in text,name
 shutil.copy2(bundle/f'ftebroom/audio4-{name}.png',qa)
 print('PASS native visual',name,width,height,flush=True)
