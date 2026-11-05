# SPDX-License-Identifier: GPL-2.0-or-later
"""Inspect Modern Light in a disposable actual editor installation.

Supply an existing runtime ZIP/tar.gz and the module built from this checkout.
--benchmark uses the same legacy theme command for before/after comparisons.
"""
import argparse, pathlib, re, shutil, subprocess, zipfile, tarfile, os, time, ctypes
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--archive',type=pathlib.Path,required=True);p.add_argument('--plugin',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--mod',type=pathlib.Path,required=True);p.add_argument('--csqc',type=pathlib.Path);p.add_argument('--benchmark',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
if a.archive.suffix=='.zip':
 with zipfile.ZipFile(a.archive) as z:z.extractall(out)
else:
 with tarfile.open(a.archive) as t:t.extractall(out,filter='data')
root=next(p for p in out.iterdir() if p.is_dir());windows=os.name=='nt'
shutil.copy2(a.plugin,root/('fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'))
if a.csqc:shutil.copy2(a.csqc,root/'ftebroom/csaddon.dat')
mod=out/'qa-mod';shutil.copytree(a.mod,mod)
cfg='exec ftebroom.cfg\nset ftebroom_skip_setup 1\nset cl_maxfps 1000\nset cl_idlefps 0\nset vid_vsync 0\nset ftebroom_test_import_root "'+mod.as_posix()+'"\nin 12 ftebroom_test_desktop external69load\n'
if a.benchmark:
 cfg+='in 20 ftebroom_test_desktop light36\nin 26 set ftebroom_benchmark 1\nin 36 screenshot light-benchmark.png\nin 39 quit force\n';images=['light-benchmark']
else:
 cfg+='in 20 ftebroom_test_desktop soft122open\nin 24 ftebroom_test_desktop soft122check\nin 26 screenshot soft-main.png\n'
 for start,mode,name in [(28,'horizontal','horizontal'),(33,'scale','scale'),(38,'prefs','preferences'),(43,'setup','setup'),(48,'generator','generator'),(53,'gallery','controls')]:
  cfg+=f'in {start} ftebroom_test_desktop soft122{mode}\nin {start+3} screenshot soft-{name}.png\n'
 cfg+='in 59 ftebroom_test_desktop soft122files\nin 62 screenshot soft-files.png\nin 64 ftebroom_test_desktop soft122build\nin 67 screenshot soft-build.png\n';images=['soft-'+s for s in ['main','horizontal','scale','preferences','setup','generator','controls','files','build']]
 if a.csqc:
  for index,mode in enumerate(('prepare','unselected','entity','deselect','brush','model','modelcheck')):
   cfg+=f'in {69+index*3} ftebroom_test_desktop rb123{mode}\n'
  cfg+='in 74 screenshot soft-properties.png\nin 83 screenshot soft-links.png\nin 89 screenshot soft-model.png\nin 92 quit force\n'
  images+=['soft-properties','soft-links','soft-model']
 else:
  cfg+='in 70 quit force\n'
(root/'ftebroom/qa-light.cfg').write_text(cfg,encoding='utf-8')
engine=root/('bin/fteqw64.exe' if windows else 'fteqw-gl64')
command=[str(engine),'-ftebroom-child-processes','-nohome','-noupdates','-nosound','-window','-width','1920','-height','1080','-condebug','-basedir',str(root),'+exec','qa-light.cfg']
env=dict(os.environ);env.pop('WAYLAND_DISPLAY',None)
with (out/'process.log').open('w') as log:
 with subprocess.Popen(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT) as run:
  began=time.monotonic()
  while run.poll() is None:
   if windows:
    from ctypes import wintypes
    user=ctypes.windll.user32
    callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    def focus(hwnd,param):
     pid=wintypes.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
     if pid.value==run.pid and user.IsWindowVisible(hwnd):user.SetWindowPos(hwnd,-1,0,0,0,0,0x13);user.SetForegroundWindow(hwnd)
     return True
    user.EnumWindows(callback(focus),0)
   if time.monotonic()-began>100:run.kill();raise TimeoutError('Editor QA timed out')
   time.sleep(.5)
log=(root/'ftebroom/qconsole.log').read_text(errors='replace')
for line in log.splitlines():
 if any(x in line for x in ['DESKTOP_','BENCHMARK','FRAME_TIMES','GL_RENDERER']):print(line)
assert run.returncode==0 and 'DESKTOP_FAIL' not in log and 'CSQC_Abort' not in log,log[-9000:]
if not a.benchmark:
 for item in ['style_stacks_balanced','existing_id_and_scaled_style','existing_multilingual_fonts','saved_theme_compatibility','compact_entity_default_and_margins','footer_inside_panel','advanced_search_opens','advanced_search_closes','definitions_footer_opens','workspace_gutter_excludes_viewport_input','dock_resize_preserves_gutter','reference_tool_rail','rail_options_open']:assert 'PASS soft122_'+item in log,item
 if a.csqc:
  for item in ['entity_links_default_all','no_selection_collapses_properties','entity_selection_opens_properties','property_table_fits_rows','deselect_collapses_properties','brush_selection_opens_properties','links_scene_is_deselected','native_open_routes_to_authoring']:assert 'PASS rb123_'+item in log,item
for name in images:
 file=root/'ftebroom'/(name+'.png');image=Image.open(file).convert('RGB');image.thumbnail((192,108));assert sum(max(c)>30 for c in image.getdata())>4000,name
 shutil.copy2(file,out/(name+'.png'))
if not a.benchmark:
 main=Image.open(out/'soft-main.png').convert('RGB')
 sample=re.search(r'FTEBROOM_GUTTER_SAMPLE ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)',log)
 assert sample, 'Missing workspace gutter coordinates'
 x,y,w,h=map(float,sample.groups())
 px,py=int(x*main.width/w),int(y*main.height/h)
 gutter=main.getpixel((px,py));outer=main.getpixel((3,py))
 assert max(abs(a-b) for a,b in zip(gutter,outer))<=8,('Workspace background must show between dock panels',gutter,outer)
 appearance=root/'ftebroom/appearance.cfg'
 assert appearance.is_file() and appearance.read_text().split()[2]=='3'
 restart='exec ftebroom.cfg\nset ftebroom_skip_setup 0\nin 14 screenshot soft-restart.png\nin 17 quit force\n'
 (root/'ftebroom/qa-light-restart.cfg').write_text(restart)
 restart_command=command[:-1]+['qa-light-restart.cfg']
 with (out/'restart.log').open('w') as log:
  completed=subprocess.run(restart_command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=60)
 assert completed.returncode==0
 picture=Image.open(root/'ftebroom/soft-restart.png').convert('RGB')
 restored=picture.getpixel((3,45));reference=main.getpixel((3,45))
 assert max(abs(a-b) for a,b in zip(restored,reference))<=8,('Modern Light was not restored after process restart',restored,reference)
 shutil.copy2(root/'ftebroom/soft-restart.png',out/'soft-restart.png')
 print('PASS actual process restart restores saved Modern Light theme ID 3')
print('PASS actual Modern Light '+('benchmark' if a.benchmark else 'panels, dialogs, languages, scaling, docking and settings'))
