#!/usr/bin/env python3
"""Real Mods UI -> customer QBSP/VIS/LIGHT -> deploy/cleanup -> engine argv.

Uses generated map/WAD/palette assets and the packaged ericw executables. Engine
launches use a process probe, not an assertion about VKQuake/Ironwail rendering.
Customer tool arguments, including .bspw and the nonzero-exit policy, are retained;
only executable paths and absolute deployment roots move into the fixture.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
from PIL import ImageGrab
from editor_os_input import NativeInput, read_ui_json

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--probe', required=True, type=Path)
a = p.parse_args()
repo = Path(__file__).resolve().parents[1]
original, out, probe = a.runtime.resolve(), a.output.resolve(), a.probe.resolve()
assert original.is_dir() and probe.is_file()
assert original != out and original not in out.parents
out.mkdir(parents=True, exist_ok=True)
root, data = out/'editor-runtime', out/'editor-runtime/ftebroom'
assert not root.exists(), 'Use a fresh output directory'
shutil.copytree(original, root, ignore=shutil.ignore_patterns('updates', 'downloads', '__pycache__'))
windows = os.name == 'nt'
engine = root/('bin/fteqw64.exe' if windows else 'fteqw-gl64')
module = root/('fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so')
fixture = out/'Game resources'
subprocess.run([sys.executable, str(repo/'tests/make-quake1-fixture.py'), str(fixture)], cwd=repo, check=True)
mod = fixture/'AD19b5'
(mod/'textures').mkdir(parents=True)
# Identical virtual names with different physical contents exercise the actual
# patched FTE search stack independently of native metadata and WAD scanning.
for phase in ('initial', 'selected', 'undone', 'redone', 'reopened'):
    for folder,label in ((fixture/'id1','BASE'), (mod,'AD')):
        (folder/('rb134-'+phase+'-resource.txt')).write_text(label+' resource winner\n')
source_dir = out/'External source maps'
source_dir.mkdir()
mapfile = source_dir/'customer_lighting.map'
wad = fixture/'id1/test textures.wad'
scene = (fixture/'editor/ftebroom/maps/build-fixture.map').read_text()
scene = scene.replace('"classname" "worldspawn"', '"classname" "worldspawn"\n"wad" '+json.dumps(wad.as_posix()), 1)
scene = scene.replace('"classname" "light"', '"classname" "light"\n"_color" "1 0.2 0.1"', 1)
assert '_tb_mod' not in scene
mapfile.write_text('// ReyondBroom profile=quake1 format=quake-standard\n'+scene)
for parent in (root, data):
    (parent/'ftebroom-project.txt').write_text(mod.as_posix())
    (parent/'ftebroom-game-profile.txt').write_text('2 0 "" "" "" "" 1 5 "" 1 1\n')
    (parent/'ftebroom-quake1.txt').write_text('"" '+json.dumps(mod.as_posix())+' "" "" 0 0 "" 0\n')
customer_file = repo/'tests/fixtures/CompilationProfiles-original.cfg'
profile = json.loads(customer_file.read_text())['profiles'][0]
profile['trenchBroom'] = True
# Assert the fixture adaptation cannot silently simplify the customer's flags.
original_tasks = json.loads(customer_file.read_text())['profiles'][0]['tasks']
for task in profile['tasks']:
    if task['type'] == 'tool':
        executable = task['tool'].rsplit('/', 1)[-1]
        task['tool'] = (root/'tools/ericw'/(executable if windows else executable.removesuffix('.exe'))).as_posix()
    elif task['type'] == 'copy':
        task['target'] = fixture.as_posix()+'/${MODS[-1]}/maps'
assert all(task.get('parameters') == original.get('parameters') for task,original in zip(profile['tasks'],original_tasks))
launchdir = fixture  # Imported engine defaults to its executable directory.
executable = launchdir/('engine-probe.exe' if windows else 'engine-probe')
shutil.copy2(probe, executable)
receipt = out/'engine-argv.json'
engine_profile = {'id':'imported-vkquake', 'name':'VKQuake command probe', 'path':executable.as_posix(),
    'parameters':'record "'+receipt.as_posix()+'" -game ${MODS[-1]} +map ${MAP_BASE_NAME} -noambient +developer 1 +exec balmapping.cfg'}
workspace = {'profiles':[profile], 'engines':[engine_profile], 'variables':{}, 'gameRoot':fixture.as_posix(),
    'baseMod':'id1', 'enabledMods':['AD19b5'], 'modsConfigured':True}
(data/'compilation-profiles.json').write_text(json.dumps({'version':1, 'games':{'quake1':workspace}}))
state = data/'compilation-ui-live.json'
mods_state = data/'native-mods134-live.json'
for path in (state, mods_state, data/'native-input-live.json'):
    path.unlink(missing_ok=True)
(data/'mod-lighting-qa.cfg').write_text('exec ftebroom.cfg\nset ftebroom_input_qa 1\nin 10 ftebroom_test_desktop reyon8qa dark\nin 11 ftebroom_test_filepath open '+json.dumps(mapfile.as_posix())+'\nin 12 ftebroom_test_run 50\n')
checks = []
process = None
stream = (out/'editor.log').open('wb')

def live():
    return read_ui_json(state)

def mods_live():
    return read_ui_json(mods_state)

def wait(fn, timeout=90):
    end = time.monotonic()+timeout
    while time.monotonic() < end:
        assert process.poll() is None, 'Editor exited; inspect editor.log'
        try:
            if value := fn():
                return value
        except (OSError, ValueError, KeyError):
            pass
        time.sleep(.07)
    raise AssertionError('Timed out; compile state: '+str(live())[:2000])

def check(ok, name):
    assert ok, name
    checks.append(name)
    (out/'results.json').write_text(json.dumps(checks, indent=2))
    print('PASS', name, flush=True)

def click_compile(key):
    ui.settle()
    r = wait(lambda:live()['widgets'].get(key))
    origin = live()['widgets']['window']
    ui.click(origin['x']+r['x']+r['width']*.5, origin['y']+r['y']+r['height']*.5)
    ui.settle()

resource_receipts = []

def check_resource(phase, folder):
    name = 'rb134-'+phase+'-resource.txt'
    expected = (folder/name).as_posix().casefold()
    ui.console('which '+name)
    def receipt():
        for path in (data/'qconsole.log', root/'qconsole.log', out/'editor.log'):
            if path.is_file():
                text = path.read_text(errors='replace').replace('\\','/').casefold()
                if expected in text:
                    return {'phase':phase, 'virtual_name':name, 'actual_path':str(folder/name), 'log':str(path)}
    observed = wait(receipt)
    resource_receipts.append(observed)
    (out/'resource-receipts.json').write_text(json.dumps(resource_receipts,indent=2))
    check(True, phase+'_actual_FTE_filesystem_lookup_uses_expected_resource_directory')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def embedded_test_textures(bsp):
    offset, length = struct.unpack_from('<II',bsp,4+2*8)
    textures = bsp[offset:offset+length]
    count = struct.unpack_from('<I',textures)[0]
    embedded = []
    for index in range(count):
        at = struct.unpack_from('<i',textures,4+4*index)[0]
        if at < 0:
            continue
        name = textures[at:at+16].split(bytes([0]))[0].decode('ascii')
        width,height,first_mip = struct.unpack_from('<III',textures,at+16)
        pixels = textures[at+first_mip:at+first_mip+width*height]
        if first_mip and width == 64 and height == 64 and set(pixels) == {64,200}:
            embedded.append(name)
    return embedded

try:
    render = ['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
    process = subprocess.Popen([str(engine), *render, '+set','ftebroom_skip_setup','1',
        '+set','ftebroom_input_qa','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound',
        '-condebug','-window','-width','1600','-height','1000','-basedir',str(root),'+exec','mod-lighting-qa.cfg'],
        cwd=root, env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'}, stdout=stream, stderr=subprocess.STDOUT)
    wait(lambda:live().get('document') == str(mapfile) and 'compile' in live().get('widgets',{}))
    ui = NativeInput(process, state, live, wait, out, focus_required=True)
    ui.position()
    if not windows:
        ui.window.set_input_focus(ui.X.RevertToParent, ui.X.CurrentTime)
        ui.d.sync()
    wait(lambda:mods_live()['document'] == str(mapfile) and not mods_live()['loading'])
    check(mods_live()['mods'] == [] and mods_live()['resourcePaths'] == [str(fixture/'id1')],
          'existing_vanilla_map_does_not_inherit_unrelated_global_AD_resource_selection')
    check_resource('initial', fixture/'id1')
    click_compile('close')
    def click_mod(key):
        point = wait(lambda:mods_live()['widgets'].get(key))
        ui.click(*point)
        ui.settle()
    if 'map_mod_available/AD19b5' not in mods_live()['widgets']:
        click_mod('map_mod_header')
    click_mod('map_mod_available/AD19b5')
    click_mod('map_mod_add')
    wait(lambda:mods_live()['mods'] == ['AD19b5'] and not mods_live()['loading'])
    check(mods_live()['resourcePaths'] == [str(fixture/'id1'), str(mod)] and
          Path(mods_live()['runtime']) == mod and not mods_live()['error'],
          'actual_Mod_plus_updates_map_list_active_runtime_and_base_mod_resource_stack')
    check(mods_live()['base'] == 'id1' and 'id1' not in mods_live()['mods'],
          'inherited_id1_is_separate_from_editable_enabled_mods')
    check_resource('selected', mod)
    ImageGrab.grab().save(out/'selected-ad-mod.png')
    ui.console('ftebroom_undo')
    wait(lambda:mods_live()['mods'] == [] and not mods_live()['loading'])
    check(mods_live()['resourcePaths'] == [str(fixture/'id1')],
          'Undo_restores_vanilla_map_and_its_actual_resource_stack')
    check_resource('undone', fixture/'id1')
    ui.console('ftebroom_redo')
    wait(lambda:mods_live()['mods'] == ['AD19b5'] and not mods_live()['loading'])
    check(mods_live()['resourcePaths'][-1] == str(mod),
          'Redo_restores_AD_map_and_its_actual_resource_stack')
    check_resource('redone', mod)
    click_mod('map_mod_enabled/AD19b5')
    click_mod('map_mod_remove')
    wait(lambda:mods_live()['mods'] == [] and not mods_live()['loading'])
    check(Path(mods_live()['runtime']) == fixture/'id1',
          'actual_Mod_minus_restores_vanilla_active_runtime')
    ui.console('ftebroom_undo')
    wait(lambda:mods_live()['mods'] == ['AD19b5'] and not mods_live()['loading'])
    # Save through the global shortcut after moving keyboard focus out of the
    # list, then reopen through the same external-file document lifecycle.
    view = live()['views'][0]
    ui.click(view['x']+view['width']*.5, view['y']+view['height']*.5)
    ui.key('s', ('ctrl',))
    wait(lambda:'"_tb_mod" "AD19b5"' in mapfile.read_text())
    check('"wad"' in mapfile.read_text() and wad.name in mapfile.read_text(),
          'Save_persists_selected_map_mod_and_external_WAD_reference')
    old_generation = mods_live()['generation']
    ui.console('ftebroom_test_filepath open '+json.dumps(mapfile.as_posix()))
    wait(lambda:mods_live()['generation'] > old_generation and
         mods_live()['document'] == str(mapfile) and
         mods_live()['mods'] == ['AD19b5'] and not mods_live()['loading'])
    check(Path(mods_live()['runtime']) == mod and not mods_live()['error'],
          'saved_map_reopens_with_the_same_AD_mod_and_resource_stack')
    check_resource('reopened', mod)
    ui.console('ftebroom_test_run 50')
    wait(lambda:live()['compileOpen'] and 'compile' in live()['widgets'])
    original_source = mapfile.read_bytes()
    click_compile('compile')
    wait(lambda:live()['running'] or live()['error'] or 'Completed successfully.' in live()['log'])
    wait(lambda:not live()['running'], 180)
    compiler_log = live()['log']
    (out/'compiler-tasks.log').write_text(compiler_log)
    check(not live()['error'] and 'Completed successfully.' in compiler_log and
          'Tool exited with code' not in compiler_log,
          'actual_customer_qbsp_vis_light_chain_finishes_without_ignored_tool_errors')
    check('customer_lighting.bspw' in compiler_log and 'Lighting Completed.' in compiler_log,
          'literal_bspw_and_full_lighting_arguments_reach_packaged_ericw')
    target = mod/'maps/customer_lighting.bsp'
    target_lit = mod/'maps/customer_lighting.lit'
    check(target.is_file() and target_lit.is_file() and
          not (fixture/'id1/maps/customer_lighting.bsp').exists() and
          not (fixture/'id1/maps/customer_lighting.lit').exists(),
          'both_real_bsp_and_lit_deploy_to_selected_AD_mod_only')
    bsp = target.read_bytes()
    check(bsp[:4] in (b'BSP2', b'2PSB'), 'customer_bsp2_output_format_is_preserved')
    lumps = [struct.unpack_from('<II', bsp, 4+i*8) for i in range(15)]
    check(all(offset+size <= len(bsp) for offset,size in lumps), 'compiled_bsp_lump_ranges_are_valid')
    def lump(i):
        offset, length = lumps[i]
        return bsp[offset:offset+length]
    check(b'"classname" "light"' in lump(0) and b'"_color" "1 0.2 0.1"' in lump(0),
          'strict_export_preserves_light_entity_and_color_in_real_bsp')
    check(lump(0).count(b'"classname" "info_player_start"') == 1 and
          b'"classname" "info_player_deathmatch"' not in lump(0),
          'single_player_fixture_keeps_info_player_start_without_deathmatch_replacement')
    lighting = lump(8)
    check(len(lighting) > 100 and min(lighting) < max(lighting) and max(lighting) > 0,
          'real_bsp_contains_nonuniform_baked_light_data')
    lit = target_lit.read_bytes()
    check(lit[:4] == b'QLIT' and struct.unpack_from('<I',lit,4)[0] == 1 and
          len(lit) == 8+3*len(lighting), 'colored_lighting_sidecar_matches_real_bsp_light_samples')
    rgb = list(zip(lit[8::3], lit[9::3], lit[10::3]))
    check(any(red != green or green != blue for red,green,blue in rgb),
          'lit_sidecar_contains_actual_colored_samples')
    embedded = embedded_test_textures(bsp)
    check('q1test' in embedded, 'external_wad_pixels_are_embedded_in_compiled_map_without_missing_texture_checker')
    check(lumps[4][1] > 0, 'actual_VIS_writes_visibility_data_after_literal_bspw_argument')
    cleanup_names = [mapfile.stem+suffix for suffix in
                     ('-compile.map','.bsp','.lit','.prt','.log','-light.log','-vis.log','.texinfo.json')]
    check(mapfile.read_bytes() == original_source and
          all(not (source_dir/name).exists() for name in cleanup_names),
          'customer_cleanup_removes_every_explicit_target_and_preserves_original_map')
    # ericw releases differ in log naming. Never make the editor delete files
    # the imported profile did not name (v0.18.1 uses light.log and vis.log).
    (out/'cleanup-evidence.json').write_text(json.dumps({'configured_targets':cleanup_names,
        'remaining_files':sorted(path.name for path in source_dir.iterdir())},indent=2))
    ImageGrab.grab().save(out/'compiled-lit-mod.png')
    click_compile('launch')
    wait(lambda:live()['launchOpen'])
    click_compile('launch-engine/0')
    click_compile('engine-launch')
    record = wait(lambda:read_ui_json(receipt))
    wait(lambda:'Engine exited with code 0' in live()['log'])
    check(not live()['error'] and Path(record['cwd']) == launchdir,
          'imported_engine_uses_its_executable_working_directory')
    check(record['argv'][3:] == ['-game','AD19b5','+map','customer_lighting','-noambient',
                                '+developer','1','+exec','balmapping.cfg'],
          'imported_engine_command_receives_same_selected_mod_and_real_map_name')
    check(target.read_bytes() == bsp and target_lit.read_bytes() == lit,
          'imported_engine_launch_keeps_deployed_bsp_and_colored_lighting_unchanged')
    (out/'bsp-evidence.json').write_text(json.dumps({'bsp_sha256':sha(target),
        'lit_sha256':sha(target_lit), 'light_bytes':len(lighting), 'rgb_samples':len(rgb),
        'embedded_textures':embedded, 'engine_receipt':record}, indent=2))
    ImageGrab.grab().save(out/'launch-selected-mod.png')
    click_compile('launch-close')
    click_compile('close')
    # A newly authored map can use an indexed WAD texture without a worldspawn
    # wad key. Save normalizes the disk file, but immediate compilation must
    # use that same resource resolution before any reopen refreshes QC state.
    fresh_dir = out/'New map authoring'
    fresh_dir.mkdir()
    fresh_map = fresh_dir/'fresh_material.map'
    fresh_scene = scene.replace('"wad" '+json.dumps(wad.as_posix())+'\n','',1)
    fresh_scene = fresh_scene.replace('"classname" "worldspawn"',
        '"classname" "worldspawn"\n"_tb_mod" "AD19b5"',1)
    assert '"wad"' not in fresh_scene
    fresh_map.write_text('// ReyondBroom profile=quake1 format=quake-standard\n'+fresh_scene)
    previous_generation = mods_live()['generation']
    ui.console('ftebroom_test_filepath open '+json.dumps(fresh_map.as_posix()))
    wait(lambda:mods_live()['generation'] > previous_generation and
         mods_live()['document'] == str(fresh_map) and not mods_live()['loading'])
    view = live()['views'][0]
    ui.click(view['x']+view['width']*.5,view['y']+view['height']*.5)
    ui.key('s', ('ctrl',))
    wait(lambda:'"wad"' in fresh_map.read_text() and wad.name in fresh_map.read_text())
    check(True,'newly_authored_map_Save_resolves_indexed_WAD_into_saved_document')
    ui.console('ftebroom_test_run 50')
    wait(lambda:live()['compileOpen'])
    click_compile('compile')
    wait(lambda:live()['running'] or live()['error'] or 'Completed successfully.' in live()['log'])
    wait(lambda:not live()['running'],180)
    fresh_log = live()['log']
    (out/'fresh-map-compiler.log').write_text(fresh_log)
    fresh_target = mod/'maps/fresh_material.bsp'
    check(not live()['error'] and fresh_target.is_file(),
          'newly_authored_map_compiles_immediately_after_Save_without_reopen')
    check('q1test' in embedded_test_textures(fresh_target.read_bytes()),
          'Save_then_Compile_uses_resolved_WAD_even_when_live_world_had_no_wad_key')
    ImageGrab.grab().save(out/'fresh-map-immediate-compile.png')
except Exception:
    ImageGrab.grab().save(out/'failure.png')
    try:
        (out/'failure-state.json').write_text(json.dumps(live(), indent=2))
    except (OSError,ValueError):
        pass
    raise
finally:
    if process is not None and process.poll() is None:
        process.kill()
        process.wait(timeout=15)
    stream.close()
    (out/'provenance.json').write_text(json.dumps({'build_info':(original/'build-info.txt').read_text(),
        'module_sha256':sha(module), 'qc_sha256':sha(data/'csaddon.dat'), 'engine_sha256':sha(engine),
        'probe_sha256':sha(executable), 'customer_profile_sha256':sha(customer_file),
        'compiler_sha256':{name:sha(root/'tools/ericw'/(name+'.exe' if windows else name)) for name in ('qbsp','vis','light')},
        'checks':len(checks),
        'method':'Native OS Mods/Compile/Launch controls; actual packaged ericw compiler; generated sealed colored-light MAP/WAD; engine argv/CWD probe, no real-engine visual claim'}, indent=2))
