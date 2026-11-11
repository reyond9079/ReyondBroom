#!/usr/bin/env python3
"""Test Plant Brush with real viewport input and independent saved MAP checks."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import struct
import time
from PIL import ImageGrab
from editor_os_input import NativeInput, read_ui_json

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
root, out = a.runtime.resolve(), a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
data = root / 'ftebroom'
profile = root / 'ftebroom-game-profile.txt'
previous_profile = profile.read_bytes() if profile.is_file() else None
profile.write_text('1 0 "" "" "" "" 1 0 "" 1 1\n')
state = data / 'placement-ui-live.json'
state.unlink(missing_ok=True)
fixture = data / 'maps' / 'plant-brush-fixture.map'
fixture.parent.mkdir(exist_ok=True)
fixture.write_text('''{
"classname" "worldspawn"
{
( -512 512 0 ) ( -512 -512 0 ) ( -512 -512 -16 ) ftebroom/grid 0 0 0 1 1
( 512 -512 -16 ) ( 512 -512 0 ) ( 512 512 0 ) ftebroom/grid 0 0 0 1 1
( 512 -512 0 ) ( 512 -512 -16 ) ( -512 -512 -16 ) ftebroom/grid 0 0 0 1 1
( -512 512 -16 ) ( 512 512 -16 ) ( 512 512 0 ) ftebroom/grid 0 0 0 1 1
( 512 512 -16 ) ( -512 512 -16 ) ( -512 -512 -16 ) ftebroom/grid 0 0 0 1 1
( -512 -512 0 ) ( -512 512 0 ) ( 512 512 0 ) ftebroom/grid 0 0 0 1 1
}
}
{
"classname" "info_player_deathmatch"
"origin" "0 0 24"
}
''')
(data/'plant-brush-qa.cfg').write_text('''exec ftebroom.cfg
set ftebroom_placement_qa 1
in 6 mapedit maps/plant-brush-fixture.map
in 9 ftebroom_camera_restore 0 -320 340 45 90 0 80
in 10 echo RB_PLANT_BRUSH_FIXTURE_READY
''')
console_log = data / 'qconsole.log'
console_log.unlink(missing_ok=True)
log = (out / 'editor.log').open('wb')
engine = root/('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
renderer = os.environ.get('RB_QA_RENDERER')
render = ['+set','vid_renderer',renderer] if renderer else []
process = subprocess.Popen([str(engine), *render, '-basedir', str(root), '-nohome',
    '-noupdates', '-nosound', '-condebug', '-window', '-width', '1920', '-height', '1080',
    '+set', 'ftebroom_skip_setup', '1', '+set', 'ftebroom_input_qa', '1', '+exec',
    'plant-brush-qa.cfg'], cwd=root, stdout=log, stderr=subprocess.STDOUT)
checks = []

def live():
    return read_ui_json(state)

def wait(fn, timeout=90):
    end = time.monotonic()+timeout
    while time.monotonic()<end:
        assert process.poll() is None, 'Editor exited'
        try:
            if value := fn(): return value
        except (OSError, KeyError, ValueError):
            pass
        time.sleep(.1)
    raise AssertionError('Timed out in Plant Brush test')

ui = NativeInput(process, state, live, wait, out, focus_required=True)

def widget(key):
    r = wait(lambda: live()['widgets'].get(key))
    return r['x']+r['width']*.45, r['y']+r['height']*.5

def click(key, mods=()):
    ui.click(*widget(key), mods=mods)

def hover(key):
    ui.move(*widget(key))
    time.sleep(.55)

def check(value, label):
    assert value, label
    checks.append(label)
    (out/'results.json').write_text(json.dumps(checks, indent=2))
    print('PASS', label, flush=True)

def snapshot(label):
    target = data/'maps'/('plant-brush-'+label+'.map')
    target.unlink(missing_ok=True)
    ui.console('mod_terrain_save "" plant-brush-'+label)
    wait(target.is_file)
    text = target.read_text(errors='replace')
    (out/target.name).write_text(text)
    rows = [dict(re.findall(r'"([^"\n]+)"\s+"([^"\n]*)"', item)) for item in re.findall(r'\{([^{}]*)\}', text)]
    return [item for item in rows if item.get('model','').startswith('models/reyond-plugins/plant-')], rows

def select(name, preset):
    click('plant-brush/search')
    ui.key('a', ('ctrl',))
    ui.text(name)
    click('plant-brush/preset/'+preset)
    wait(lambda: live()['plantBrush']['selection']==preset and live()['plantBrush']['ready'])

def drag(x, y, distance):
    ui.move(x,y)
    if os.name=='nt': ui.user.mouse_event(2,0,0,0,0)
    else: ui.xtest.fake_input(ui.d,ui.X.ButtonPress,1);ui.d.sync()
    try:
        ui.settle()
        for step in range(1,9): ui.move(x+distance*step/8,y)
        ui.settle()
    finally:
        if os.name=='nt': ui.user.mouse_event(4,0,0,0,0)
        else: ui.xtest.fake_input(ui.d,ui.X.ButtonRelease,1);ui.d.sync()
    ui.settle()

try:
    wait(lambda: live()['views'] and live()['mapPath'].replace('\\','/').endswith('/plant-brush-fixture.map'))
    ui.position()
    if os.name != 'nt':
        ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
    wait(lambda: any('RB_PLANT_BRUSH_FIXTURE_READY' in file.read_text(errors='replace')
        for file in (out/'editor.log',console_log,root/'qconsole.log') if file.exists()))
    view = live()['views'][0]
    x, y = view['x']+view['width']*.55, view['y']+view['height']*.57
    ui.click(x,y,button=3)
    hover('context/create');hover('context/model');click('context/plant-brush')
    brush = wait(lambda: (b if (b:=live()['plantBrush'])['active'] and b['dock'] else None))
    check(brush['dock']==brush['inspectorDock'] and not brush['editorVisible'], 'context opens a docked Plant Brush tab')
    click('plant-brush/radius',('ctrl',))
    ui.key('a',('ctrl',));ui.text('8');ui.key('enter')
    check(wait(lambda: abs(live()['plantBrush']['radius']-8)<.01), 'world-space radius can be edited')
    select('Fly agaric','amanita')
    ui.click(x,y)
    plants, _ = snapshot('paint-one')
    check(len(plants)==1 and plants[0]['model'].endswith('.iqm') and plants[0]['classname']=='func_model',
          'paint places exactly one profile-native FTE plant at small radius')
    first = plants[0]['model']
    check((data/first).read_bytes().startswith(b'INTERQUAKEMODEL') and (data/plants[0]['_reyondbroom_source']).is_file(),
          'placed plant includes its reusable IQM and editable source')
    source=json.loads((data/plants[0]['_reyondbroom_source']).read_text())
    check(abs(source['parameters']['height']-7.2)<.001,'Plant Brush defaults use Half-Life player scale for FTE')
    encoded=(data/first).read_bytes();arrays,vertices,offset=struct.unpack_from('<III',encoded,44)
    positions=[]
    for i in range(arrays):
        kind,flags,format,size,start=struct.unpack_from('<5I',encoded,offset+i*20)
        if kind==0:
            assert format==7 and size==3
            positions=[struct.unpack_from('<3f',encoded,start+v*12)[2] for v in range(vertices)]
    check(positions and .04<(max(positions)-min(positions))/72<.2,'painted IQM geometry has mushroom stature relative to a 72-unit player')
    select('Scots pine','pine')
    ui.click(x,y)
    plants, _ = snapshot('paint-two')
    check(len(plants)==2 and any(item['model']==first for item in plants),'different catalogue plants can share the same area')
    click('plant-brush/search');ui.key('a',('ctrl',));ui.key('backspace');ui.key('enter')
    click('plant-brush/erase')
    wait(lambda: 'plant-brush/scene/amanita' in live()['widgets'] and 'plant-brush/scene/pine' in live()['widgets'])
    check('plant-brush/scene/oak' not in live()['widgets'],'erase catalogue contains scene plants only')
    click('plant-brush/scene/amanita')
    ui.click(x,y)
    plants, _ = snapshot('erase-filtered')
    check(len(plants)==1 and plants[0]['model']!=first,'filtered erase removes only the chosen species')
    click('plant-brush/all')
    ui.click(x,y)
    plants, rows = snapshot('erase-all')
    check(not plants and any(row.get('classname')=='info_player_deathmatch' for row in rows),
          'unfiltered erase removes remaining plants and preserves unrelated entities')
    ui.console('ftebroom_undo');check(len(snapshot('undo-erase-all')[0])==1,'erase is a single Undo step')
    ui.console('ftebroom_undo');check(len(snapshot('undo-erase-filter')[0])==2,'filtered erase is a single Undo step')
    click('plant-brush/paint');select('Fly agaric','amanita')
    # Start on the near half of the fixture floor. At the click-test height
    # the left part of the viewport can be sky, depending on its aspect ratio.
    view = live()['views'][0]
    drag(view['x']+view['width']*.38,view['y']+view['height']*.76,view['width']*.13)
    check(len(snapshot('paint-drag')[0])>3,'a single drag places plants along its path')
    ui.move(x-view['width']*.10,y)
    ui.settle()
    ImageGrab.grab().save(out/'plant-brush-paint.png')
    ui.console('ftebroom_undo')
    check(len(snapshot('undo-paint-drag')[0])==2,'one Undo removes the whole paint stroke')
    click('plant-brush/new')
    wait(lambda: live()['plantBrush']['editorVisible'] and live()['plantReady'])
    check(not live()['plantBrush']['active'],'create new plant opens Plant Editor and releases scene input')
    click('plant/to-brush')
    wait(lambda: (b:=live()['plantBrush'])['active'] and b['ready'] and not b['editorVisible'])
    current = wait(lambda: live()['widgets'].get('plant-brush/current',{}).get('visible'))
    check(current and live()['plantBrush']['selection']=='@current',
          'Plant Editor reveals and selects the current draft in the brush catalogue')
    ui.click(x+view['width']*.18,y)
    plants, _ = snapshot('paint-draft')
    check(any(json.loads((data/item['_reyondbroom_source']).read_text())['name']=='Untitled oak' for item in plants),
          'the transferred draft can be painted without saving it as a preset first')
    check(any(json.loads((data/item['_reyondbroom_source']).read_text())['parameters']['height']==960 for item in plants
          if json.loads((data/item['_reyondbroom_source']).read_text())['name']=='Untitled oak'),
          'Create new plant and editor-to-brush transfer retain the FTE oak scale')
    ImageGrab.grab().save(out/'plant-brush.png')
except Exception:
    ImageGrab.grab().save(out/'failure.png')
    try: (out/'failure-state.json').write_text(json.dumps(live(),indent=2))
    except Exception: pass
    raise
finally:
    if process.poll() is None: process.kill();process.wait(timeout=20)
    log.close()
    if previous_profile is None: profile.unlink(missing_ok=True)
    else: profile.write_bytes(previous_profile)
