#!/usr/bin/env python3
"""Compile-path acceptance using actual OS input and real process argv/CWD.

Clone the supplied runtime before preparing fixtures. A process probe is supplied
explicitly from the build tree; it is never added to the distributable package.
The editor's read-only live snapshots provide rectangles/document state only.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import zipfile
from PIL import ImageGrab
from editor_os_input import NativeInput, read_ui_json

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--probe', required=True, type=Path)
p.add_argument('--width', type=int, default=1600)
p.add_argument('--height', type=int, default=1000)
a = p.parse_args()
original, out, probe = a.runtime.resolve(), a.output.resolve(), a.probe.resolve()
assert original.is_dir() and probe.is_file()
out.mkdir(parents=True, exist_ok=True)
root, data = out/'editor-runtime', out/'editor-runtime'/'ftebroom'
assert not root.exists(), 'Use a fresh output folder; never overwrite a running runtime'
shutil.copytree(original, root)
live_path, store = data/'compilation-ui-live.json', data/'compilation-profiles.json'
assets = out/'Game resources with spaces'/'baseq3'
(assets/'textures').mkdir(parents=True)
(assets/'maps').mkdir()
authoring = out/'External authoring directory'
authoring.mkdir()
savedir = out/'Different Save As directory'
savedir.mkdir()
build = out/'Explicit profile working directory'
build.mkdir()


def box(texture):
    return ('{\n"classname" "worldspawn"\n{\n'
            '( -64 64 128 ) ( -64 -64 128 ) ( -64 -64 0 ) '+texture+' 0 0 0 1 1\n'
            '( 64 -64 0 ) ( 64 -64 128 ) ( 64 64 128 ) '+texture+' 0 0 0 1 1\n'
            '( 64 -64 128 ) ( 64 -64 0 ) ( -64 -64 0 ) '+texture+' 0 0 0 1 1\n'
            '( -64 64 0 ) ( 64 64 0 ) ( 64 64 128 ) '+texture+' 0 0 0 1 1\n'
            '( 64 64 0 ) ( -64 64 0 ) ( -64 -64 0 ) '+texture+' 0 0 0 1 1\n'
            '( -64 -64 128 ) ( -64 64 128 ) ( 64 64 128 ) '+texture+' 0 0 0 1 1\n}\n}\n')


q3 = '// ReyondBroom profile=quake3 format=quake3\n'+box('ftebroom/grid')
external = authoring/'real_authoring_name.map'
external.write_text(q3)
saveas = savedir/'renamed_saved_source.map'
control = data/'maps/compilation_paths_control.map'
control.parent.mkdir(exist_ok=True)
control.write_text(q3)
mounted = assets/'maps/mounted_loose_source.map'
mounted.write_text(q3)
# Packages exercise the actual resource winner returned by FS_GAME, rather
# than assuming the map lives in the editor's writable directory.
pak = assets/'pak0.pak'
member = 'maps/mounted_pak_source.map'
raw = q3.encode()
pak.write_bytes(b'PACK'+struct.pack('<II', 12+len(raw), 64)+raw+
                struct.pack('<56sII', member.encode(), 12, len(raw)))
packed = Path(str(pak)+'/'+member)
pk3 = assets/'pak1.pk3'
zipmember = 'maps/mounted_pk3_source.map'
with zipfile.ZipFile(pk3, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr(zipmember, q3)
zipped = Path(str(pk3)+'/'+zipmember)
# An external Quake 1 map switches game profile and discovers a per-map WAD,
# so resource restarts happen before its staged map receives mapReady().
id1 = out/'Quake resources with spaces'/'id1'
(id1/'gfx').mkdir(parents=True)
(id1/'gfx/palette.lmp').write_bytes(bytes(c for i in range(256) for c in (i, i, 255-i)))
waddir = out/'Per-map texture library'
waddir.mkdir()
name = b'q1test'+bytes(10)
pixels = b''.join(bytes([140])*(64*64//(4**m)) for m in range(4))
lump = name+struct.pack('<6I', 64, 64, 40, 40+4096, 40+4096+1024, 40+4096+1024+256)+pixels
wad = waddir/'map textures.wad'
wad.write_bytes(struct.pack('<4sII', b'WAD2', 1, 12+len(lump))+lump+
                struct.pack('<III4B16s', 12, len(lump), len(lump), 68, 0, 0, 0, name))
q1map = authoring/'quake1_distinct_source.map'
q1map.write_text('// ReyondBroom profile=quake1 format=quake-standard\n'+
                 box('q1test').replace('"classname" "worldspawn"',
                 '"classname" "worldspawn"\n"wad" '+json.dumps(wad.as_posix()), 1))
for folder in (root, data):
    (folder/'ftebroom-project.txt').write_text(assets.as_posix())
    (folder/'ftebroom-game-profile.txt').write_text('0 0 "" "" "" "" 1 5 "" 1 1\n')
    (folder/'ftebroom-quake1.txt').write_text('"" '+json.dumps(id1.as_posix())+
        ' "" "" 0 0 '+json.dumps(assets.as_posix())+' 0\n')
receipts = [out/('process-'+str(i)+'.json') for i in range(4)]
variables = ('MAP_DIR_PATH', 'MAP_BASE_NAME', 'MAP_FULL_NAME', 'WORK_DIR_PATH', 'APP_DIR_PATH')
profiles = []
for i, (label, work) in enumerate((('Map directory', '${MAP_DIR_PATH}'),
                                  ('Explicit working directory', build.as_posix()),
                                  ('Empty working directory', ''),
                                  ('Unsaved export guard', '${MAP_DIR_PATH}'))):
    params = 'record "'+receipts[i].as_posix()+'" '+ ' '.join('"'+k+'=${'+k+'}"' for k in variables)
    tasks = [{'type':'tool', 'tool':probe.as_posix(), 'parameters':params, 'treatNonZeroResultCodeAsError':True}]
    if i == 3:
        tasks.insert(0, {'type':'export', 'target':(out/'must-not-export-unsaved.map').as_posix()})
    profiles.append({'name':label, 'workdir':work, 'trenchBroom':True, 'tasks':tasks})
engine_receipt = out/'unsaved-engine-process.json'
engine_profile = {'id':'paths-probe-engine', 'name':'Process probe', 'path':probe.as_posix(),
                  'parameters':'record "'+engine_receipt.as_posix()+'"', 'workdir':'', 'mapFile':''}
workspace = {'profiles':profiles, 'engines':[engine_profile], 'variables':{}}
store.write_text(json.dumps({'version':1, 'games':{game:workspace for game in ('quake3', 'quake1')}}))
live_path.unlink(missing_ok=True)
config = data/'compilation-paths.cfg'
config.write_text('set ftebroom_skip_setup 1\nexec ftebroom.cfg\nin 7 mapedit maps/compilation_paths_control.map\nin 12 ftebroom_test_run 50\n')
engine = root/('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
render = ['+set', 'vid_renderer', os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
stream = (out/'editor.log').open('wb')
process = subprocess.Popen([str(engine), *render, '-ftebroom-child-processes', '-nohome', '-noupdates', '-nosound',
    '-window', '-width', str(a.width), '-height', str(a.height), '-condebug', '-basedir', str(root),
    '+set', 'ftebroom_skip_setup', '1', '+set', 'ftebroom_input_qa', '1', '+exec', config.name],
    cwd=root, stdout=stream, stderr=subprocess.STDOUT, env={**os.environ, 'LIBGL_ALWAYS_SOFTWARE':'1'})
checks, cases = [], []


def live():
    return read_ui_json(live_path)


def wait(fn, timeout=60):
    deadline = time.monotonic()+timeout
    while time.monotonic() < deadline:
        assert process.poll() is None, 'Editor exited; inspect editor.log'
        try:
            result = fn()
            if result:
                return result
        except (FileNotFoundError, PermissionError, KeyError, json.JSONDecodeError):
            pass
        time.sleep(.06)
    raise AssertionError('Timed out; latest state: '+str(live()))


ui = NativeInput(process, live_path, live, wait, out, focus_required=True)


def expect(ok, label):
    assert ok, label
    checks.append(label)
    (out/'results.json').write_text(json.dumps(checks, indent=2))
    print('PASS', label, flush=True)


def same_path(left, right):
    return os.path.normcase(os.path.normpath(str(left))) == os.path.normcase(os.path.normpath(str(right)))


def click(name):
    r = wait(lambda:live()['widgets'].get(name))
    w = live()['widgets']['window']
    ui.click(w['x']+r['x']+r['width']*.5, w['y']+r['y']+r['height']*.5)


def capture(label, source, selected=0, directory=None, cwd=None):
    if not live()['compileOpen']:
        ui.console('ftebroom_test_run 50')
    receipts[selected].unlink(missing_ok=True)
    click('profile/'+str(selected))
    click('compile')
    receipt = wait(lambda:read_ui_json(receipts[selected]))
    wait(lambda:not live()['running'])
    state = live()
    fields = dict(arg.split('=', 1) for arg in receipt['argv'][3:])
    actual_work = json.loads(store.read_text())['games'][state['game']]['profiles'][selected]['workdir']
    directory = directory or source.parent
    expected_work = '' if selected == 2 else str(build if selected == 1 else directory)
    expected_cwd = cwd or (root if selected == 2 else Path(expected_work))
    expect(same_path(state['document'], source), label+': original document path is retained')
    ui.position()
    caption=lambda:ui.Desktop(backend='win32').window(handle=ui.hwnd).window_text() if os.name=='nt' else ui.window.get_wm_name()
    wait(lambda:caption()=='ReyondBroom: '+source.stem+'.map')
    expect(True,label+': OS window title uses the original map name')
    expect(same_path(fields['MAP_DIR_PATH'], directory) and fields['MAP_BASE_NAME'] == source.stem and
           fields['MAP_FULL_NAME'] == source.name, label+': MAP variables describe the real source')
    expect((fields['WORK_DIR_PATH'] == '' if selected == 2 else same_path(fields['WORK_DIR_PATH'], expected_work)) and
           same_path(receipt['cwd'], expected_cwd), label+': selected profile controls actual process working directory')
    cases.append({'case':label, 'document':state['document'], 'workExpression':actual_work,
                  'variables':fields, 'actualProcessCwd':receipt['cwd'], 'log':state['log']})
    (out/'cases.json').write_text(json.dumps(cases, indent=2))


def open_map(path):
    previous = live()['document']
    ui.console('ftebroom_test_filepath open '+json.dumps(path.as_posix()))
    wait(lambda:live()['document'] != previous)
    ui.settle()


try:
    wait(lambda:live()['compileOpen'] and 'compile' in live()['widgets'])
    ui.position()
    if os.name != 'nt':
        ui.window.set_input_focus(ui.X.RevertToParent, ui.X.CurrentTime)
        ui.d.sync()
    capture('internal control', control)
    open_map(q1map)
    capture('external Quake1 profile and per-map WAD mount', q1map)
    open_map(external)
    capture('external Quake3 map', external)
    ui.console('fs_restart')
    ui.console('ftebroom_assets_ready')
    ui.settle()
    capture('resource restart', external)
    ui.console('ftebroom_test_filepath save '+json.dumps(saveas.as_posix()))
    wait(saveas.is_file)
    ui.settle()
    capture('Save As to another directory and name', saveas)
    # The last newly staged alias is still the engine's map after Save As.
    aliases = sorted((data/'maps').glob('_ftebroom_open_*.map'), key=lambda path:path.stat().st_mtime_ns)
    ui.console('mapedit maps/'+aliases[-1].name)
    ui.settle()
    capture('reload of the same staged alias', saveas)
    for path, mapname, label, directory in ((mounted, 'mounted_loose_source', 'console mounted loose source', mounted.parent),
        (packed, 'mounted_pak_source', 'console mounted PAK source', pak.parent),
        (zipped, 'mounted_pk3_source', 'console mounted PK3 source', pk3.parent)):
        previous = live()['document']
        ui.console('mapedit maps/'+mapname+'.map')
        wait(lambda:live()['document'] != previous)
        ui.settle()
        capture(label, path, directory=directory)
        ImageGrab.grab().save(out/(mapname+'.png'))
    open_map(external)
    capture('explicit absolute working directory', external, 1)
    capture('saved map with intentionally empty working directory', external, 2)
    ui.console('mapedit maps/start.map')
    wait(lambda:live()['document'] == '')
    receipts[3].unlink(missing_ok=True)
    click('profile/3')
    click('compile')
    wait(lambda:live()['error'])
    ui.settle()
    expect('Save' in live()['error'] and not live()['running'] and not receipts[3].exists() and
           not (out/'must-not-export-unsaved.map').exists(), 'unsaved map cannot export or spawn a strict compilation profile')
    ImageGrab.grab().save(out/'unsaved-map-compile-blocked.png')
    engine_receipt.unlink(missing_ok=True)
    click('launch')
    wait(lambda:live()['launchOpen'])
    click('launch-engine/0')
    click('engine-launch')
    wait(lambda:'launching an engine' in live()['error'])
    ui.settle()
    expect(not engine_receipt.exists() and not live()['running'], 'unsaved map cannot launch an engine process')
    ImageGrab.grab().save(out/'unsaved-map-launch-blocked.png')
    module = 'fteplug_ftebroom_x64.dll' if os.name == 'nt' else 'fteplug_ftebroom_amd64.so'
    info = (original/'build-info.txt').read_text()
    platform = re.search(r'^Platform: (.+)$', info, re.M).group(1)
    (out/'provenance.json').write_text(json.dumps({'build_info':info, 'platform':platform,
        'module_sha256':hashlib.sha256((original/module).read_bytes()).hexdigest(),
        'qc_sha256':hashlib.sha256((original/'ftebroom/csaddon.dat').read_bytes()).hexdigest(),
        'probe_sha256':hashlib.sha256(probe.read_bytes()).hexdigest(),
        'method':'Actual OS console input/Compile button; immutable runtime clone; real process argv/CWD receipts; documentRequest/resource reload/Save As and mounted loose/PAK/PK3 paths'}, indent=2))
    print('PASS compilation path acceptance', len(checks), flush=True)
except Exception:
    ImageGrab.grab().save(out/'failure.png')
    try:
        (out/'failure-state.json').write_text(json.dumps(live(), indent=2))
    except Exception:
        pass
    raise
finally:
    if process.poll() is None:
        process.kill()
        process.wait(timeout=15)
    stream.close()
