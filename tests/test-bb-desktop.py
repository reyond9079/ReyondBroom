# SPDX-License-Identifier: GPL-3.0-or-later
"""Real FTE/ImGui authoring smoke test in a disposable, self-contained mod.

Linux: run under xvfb-run. --blender-root is an existing managed 5.0.0 runtime
root, containing blender-5.0.0-<platform>/blender[.exe] and its ready marker.
No user game data is required or modified.
"""
import argparse, hashlib, os, pathlib, re, shutil, subprocess, time
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--engine',type=pathlib.Path,required=True);p.add_argument('--plugin',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--blender-root',type=pathlib.Path);p.add_argument('--compiler',default=os.environ.get('CXX','g++'));p.add_argument('--benchmark',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);runtime=out/'runtime';runtime.mkdir()
shutil.copytree(root/'runtime',runtime/'ftebroom')
for name in ('default.fmf','installed.lst'):shutil.copy2(root/'runtime'/name,runtime/name)
shutil.copy2(root/'csaddon.dat',runtime/'ftebroom/csaddon.dat')
windows=os.name=='nt';engine=runtime/('fteqw64.exe' if windows else 'fteqw-gl64');shutil.copy2(a.engine,engine)
shutil.copy2(a.plugin,runtime/('fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'))
if windows:
    for name in ('zlib1.dll','libpng16-16.dll'):
        source=pathlib.Path(a.compiler).resolve().parent/name
        if not source.exists():source=pathlib.Path(shutil.which('g++')).parent/name
        shutil.copy2(source,runtime/('libpng16.dll' if name.startswith('libpng') else name))
mod=out/'testmod';(mod/'gfx').mkdir(parents=True);(mod/'models').mkdir()
(mod/'gfx/palette.lmp').write_bytes(bytes(v for i in range(256) for v in (i,i,i)))
exe=out/('export-fixtures.exe' if windows else 'export-fixtures')
subprocess.run([a.compiler,'-std=c++17','-O2',str(root/'tests/test-bb-export.cpp'),'-o',str(exe)],cwd=root,check=True)
with (out/'fixtures.log').open('w') as log:subprocess.run([str(exe),str(out/'fixtures')],cwd=root,stdout=log,check=True)
shutil.copy2(out/'fixtures/monster.mdl',mod/'models/import.mdl')
(mod/'models/qa').mkdir();shutil.copy2(out/'fixtures/monster.md3',mod/'models/qa/test.md3')
shutil.copy2(root/'tests/fixtures/bb/anim_all_interp.gltf',mod/'import.gltf')
if a.blender_root:
    link=runtime/'downloads/blender-5.0.0';link.parent.mkdir()
    if windows:subprocess.run(['powershell','-NoProfile','-Command',f'New-Item -ItemType Junction -Path "{link}" -Target "{a.blender_root.resolve()}"'],check=True,capture_output=True)
    else:link.symlink_to(a.blender_root.resolve(),target_is_directory=True)
(runtime/'ftebroom-game-profile.txt').write_text('2 0 "" "" "" "" 1 5 "" 1 1\n')
cfg='exec ftebroom.cfg\nset ftebroom_skip_setup 1\nset cl_maxfps 1000\nset cl_netfps 72\nset cl_idlefps 0\nset vid_vsync 0\nset ftebroom_test_import_root "'+mod.as_posix()+'"\nin 3 ftebroom_test_desktop external69load\n'
if a.benchmark:
    cfg+='in 7 ftebroom\nin 9 ftebroom_test_desktop bba5\nin 17 ftebroom_test_desktop bbstop\n'
    for start,mode in ((20,'bba5'),(29,'bbb5'),(38,'bba20'),(47,'bbb20')):
        cfg+=f'in {start} ftebroom_test_desktop {mode}\nin {start+6} screenshot {mode}.png\nin {start+8} ftebroom_test_desktop bbstop\n'
    for start,mode in ((57,'bb121ap5'),(71,'bb121ap20')):
        cfg+=f'in {start} ftebroom_test_desktop {mode}\nin {start+2} ftebroom_test_desktop bb121timingstart\nin {start+8} screenshot {mode}.png\nin {start+12} ftebroom_test_desktop bb121timingstop\n'
    cfg+='in 85 quit force\n';required=[]
else:
    cfg+='''in 7 ftebroom_test_desktop bb121open
in 9 screenshot bb-monster.png
in 10 ftebroom_test_desktop bb121gizmos
in 15 ftebroom_test_desktop bb121uvmouse
in 20 ftebroom_test_desktop bb121dialogs
in 22 ftebroom_test_desktop bb121deep
in 24 ftebroom_test_desktop bb121open
in 28 ftebroom_test_desktop bb121check
in 30 ftebroom_test_desktop bb121paint
in 33 ftebroom_test_desktop bb121animate
in 35 screenshot bb-timeline.png
in 37 ftebroom_test_desktop bb121save
in 40 ftebroom_test_desktop bb121native
in 46 ftebroom_test_desktop bb121nativecheck
'''
    required=['actual_3d_stroke_one_record','actual_timeline_key_drag','native_convert_geometry','automatic_project_autosave','project_actually_rendered','export_preserves_project']
    if a.blender_root:
        cfg+='in 48 ftebroom_test_desktop bb121gltf\nin 65 ftebroom_test_desktop bb121gltfcheck\n';required+=['gltf_y_up_dimensions_preserved','gltf_embedded_skin_imported']
    cfg+='in 67 vid_restart\nin 73 ftebroom_test_desktop bb121restartcheck\nin 74 screenshot bb-import.png\nin 76 quit force\n';required+=['authoring_survives_renderer_restart']
if windows:cfg=re.sub(r'in (\d+) ',lambda m:'in '+str(int(m.group(1))+12)+' ',cfg)
(runtime/'ftebroom/test-bb.cfg').write_text(cfg)
env=dict(os.environ);env.pop('WAYLAND_DISPLAY',None)
with (out/'process.log').open('w') as log:
    command=[str(engine),'-ftebroom-child-processes','-window','-width','1600','-height','1000','-nohome','-noupdates','-nosound','-condebug','-basedir',str(runtime),'+exec','test-bb.cfg']
    if windows and a.benchmark:
        # A background/occluded Windows GL window can be throttled by the
        # compositor/driver despite cl_idlefps=0. Keep only this disposable
        # benchmark window visible and active; its topmost state dies with it.
        import ctypes
        from ctypes import wintypes
        user=ctypes.windll.user32
        callback_type=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
        with subprocess.Popen(command,cwd=runtime,env=env,stdout=log,stderr=subprocess.STDOUT) as process:
            began=time.monotonic()
            while process.poll() is None:
                def focus(hwnd,param):
                    pid=wintypes.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
                    if pid.value==process.pid and user.IsWindowVisible(hwnd):
                        user.SetWindowPos(hwnd,-1,0,0,0,0,0x13)
                        user.SetForegroundWindow(hwnd)
                    return True
                user.EnumWindows(callback_type(focus),0)
                if time.monotonic()-began>150:process.kill();raise TimeoutError('Benchmark timed out')
                time.sleep(.5)
            run=subprocess.CompletedProcess(command,process.returncode)
    else:
        run=subprocess.run(command,cwd=runtime,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=150)
log=(runtime/'ftebroom/qconsole.log').read_text(errors='replace')
for line in log.splitlines():
    if any(x in line for x in ('DESKTOP_','BB_RENDER_BENCH','BB_AUTHOR_BENCH','GL_RENDERER')):print(line)
assert run.returncode==0 and 'DESKTOP_FAIL' not in log and 'CSQC_Abort' not in log,log[-16000:]
for name in required:assert 'PASS bb121_'+name in log,name
images=['bba5','bbb5','bba20','bbb20','bb121ap5','bb121ap20'] if a.benchmark else ['bb-monster','bb-timeline','bb-import']
for name in images:
    image=Image.open(runtime/'ftebroom'/(name+'.png')).convert('RGB');image.thumbnail((160,100))
    assert sum(max(c)>20 for c in image.getdata())>2000,('Blank render is not a valid test or benchmark',name)
print('PASS real authoring '+('benchmark' if a.benchmark else 'UI and input regression'))
