"""Automate the actual editor in a private copy of a clean unpacked release."""
from pathlib import Path
import argparse, json, os, shutil, subprocess, time

p = argparse.ArgumentParser()
p.add_argument('--bundle', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--headless', action='store_true', help='Linux: run with Xvfb/software GL')
a = p.parse_args()
src = Path(__file__).resolve().parents[1]
out = a.output.resolve()
if out.exists():
    raise SystemExit('Output must be a new directory; no existing files are replaced.')
bundle = a.bundle.resolve()
engine_name = 'bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64'
if not (bundle/engine_name).is_file():
    raise SystemExit('Choose a clean unpacked runtime containing '+engine_name)
root = out/'runtime'
shutil.copytree(bundle, root)
game = root/'ftebroom'
data = game/'data'
data.mkdir(exist_ok=True)
# Blank Quake 1 profile: bundled editor textures only, no user mod resources.
(root/'ftebroom-game-profile.txt').write_text('2 0 "" "" "" "" 1 5 "" 1 1\n', encoding='utf-8')
(data/'plugin-autolevel-settings.txt').write_text(
    'AUTOLEVEL_SETTINGS 1\nactiveBuilder "dungeon"\nactiveRepo "reyond-dungeon"\n'
    'repoUids "\\"reyond-dungeon\\" "\nbuilderUids "\\"dungeon\\" "\n', encoding='utf-8')
shutil.copy2(src/'tests/fixtures/autolevel-builder119.txt', data/'plugin-autolevel-builder-dungeon.txt')
shutil.copy2(src/'tests/fixtures/attributes.map', data/'clipboard-standard.map')
env = os.environ.copy()
env['LD_LIBRARY_PATH'] = str(root/'lib')+os.pathsep+env.get('LD_LIBRARY_PATH', '')
if a.headless:
    env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    env['SDL_VIDEODRIVER'] = 'x11'
results = []
for config, required in [
    ('autolevel119', ['external_module_loaded', 'background_preview_ready', 'export_map_written',
                      'viewport_capture_starts', 'click_not_routed_to_brush_tool', 'digit_key_reserved',
                      'escape_releases_capture', 'closed_window_releases_capture',
                      'selection_bounds_read_without_file', 'imported_editable_grouped_brushes',
                      'single_undo_restores_document', 'redo_restores_full_level']),
    ('autolevel119-parser', ['parse_standard_map', 'parse_flags_values', 'parse_colored_patch_without_entity',
                            'patch_preserves_subdivisions_colors', 'reject_missing_value',
                            'reject_unterminated_comment', 'reject_malformed_number',
                            'parse_does_not_mutate_history', 'export_selection', 'parse_own_export',
                            'export_preserves_object_count', 'paste_insert'])]:
    shutil.copy2(src/'tests'/(config+'.cfg'), game/(config+'.cfg'))
    log = game/'qconsole.log'
    log.write_text('', encoding='utf-8')
    command = [str(root/engine_name), '-condebug', '-ftebroom-child-processes', '-window',
               '-width', '1280', '-height', '800', '-nohome', '-noupdates', '-nosound',
               '-basedir', str(root), '+exec', 'ftebroom.cfg', '+exec', config+'.cfg']
    if a.headless:
        command = ['xvfb-run', '-a', *command]
    startup = None
    if os.name == 'nt':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    began = time.monotonic()
    with (out/(config+'-process.log')).open('w', encoding='utf-8') as output:
        subprocess.run(command, cwd=root, env=env, startupinfo=startup,
                       stdout=output, stderr=subprocess.STDOUT, check=True, timeout=100)
    text = log.read_text(encoding='utf-8', errors='replace')
    (out/(config+'.log')).write_text(text, encoding='utf-8')
    assert not any(word in text for word in ['FTEBROOM_DESKTOP_FAIL', 'FTEBROOM_TRANSFORM_FAIL',
                                           'Host_Error', 'QC error', 'Program error']), text
    for name in required:
        assert any('PASS ' in line and name in line for line in text.splitlines()), name
    results.append(dict(test=config, passed=True, seconds=round(time.monotonic()-began, 3)))
    print('PASS '+config, flush=True)
capture = (data/'plugin-autolevel-capture-qa.map').read_text(encoding='utf-8')
assert capture.startswith('BOUNDS ') and '"classname" "light"' in capture
assert capture.count('\n( ')==6, 'Capture must contain exactly one box and its selected point light'
results.append(dict(test='selected brush plus point-light capture', passed=True))
(out/'runtime-results.json').write_text(json.dumps(dict(platform=os.name, headless=a.headless,
                                                       tests=results), indent=2)+'\n', encoding='utf-8')
print('PASS actual editor plugin import, Undo/Redo, input, parser and capture')
