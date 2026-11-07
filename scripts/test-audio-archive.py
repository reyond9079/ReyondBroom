#!/usr/bin/env python3
"""Exercise PK3 audio opening and the movable native Sound Editor in a QA runtime."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--width', type=int, default=1920)
p.add_argument('--height', type=int, default=1080)
a = p.parse_args()
runtime, output = a.runtime.resolve(), a.output.resolve()
output.mkdir(parents=True, exist_ok=True)
data = runtime / 'ftebroom'
repo = Path(__file__).resolve().parents[1]
converter = runtime / ('tools/audio/ffmpeg.exe' if os.name == 'nt' else 'tools/audio/ffmpeg')
fixtures = output / 'fixtures'
subprocess.run([os.sys.executable, str(repo / 'scripts/audio-editor-fixtures.py'),
                str(fixtures), str(converter)], check=True)
shutil.copy2(repo / 'tests/audio-archive.cfg', data / 'audio-archive.cfg')
(data / 'audio-archive-run.cfg').write_text(
    'set ftebroom_test_import_root "' + fixtures.as_posix() + '"\nexec audio-archive.cfg\n')
logs = [data / 'qconsole.log', runtime / 'qconsole.log']
offsets = {path: path.stat().st_size if path.exists() else 0 for path in logs}
engine = runtime / ('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
renderer = ['+set', 'vid_renderer', os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
with (output / 'native.log').open('wb') as log:
    subprocess.run([str(engine), *renderer, '+set', 'ftebroom_skip_setup', '1',
                    '-ftebroom-child-processes', '-nohome', '-noupdates', '-nosound',
                    '-condebug', '-window', '-width', str(a.width), '-height', str(a.height),
                    '-basedir', str(runtime), '+exec', 'audio-archive-run.cfg'],
                   cwd=runtime, env={**os.environ, 'LIBGL_ALWAYS_SOFTWARE': '1'},
                   stdout=log, stderr=subprocess.STDOUT, timeout=90, check=True)
text = (output / 'native.log').read_text(errors='replace')
for path, offset in offsets.items():
    if path.exists():
        with path.open('rb') as log:
            log.seek(offset)
            text += '\n' + log.read().decode(errors='replace')
(output / 'combined.log').write_text(text)
assert not any(error in text for error in ['DESKTOP_FAIL', 'CSQC_Abort', 'Assertion failed']), text[-5000:]
checks = ['pk3_file_manager_double_click', 'pk3_sound_in_media_and_timeline',
          'sound_is_inner_window', 'import_queued_while_decoding',
          'long_pk3_and_compressed_audio_imported', 'import_reveals_selected_media',
          'pk3_import_preserves_archive', 'inner_window_can_move', 'inner_window_can_resize']
for check in checks:
    assert 'DESKTOP_PASS audio4_' + check in text, (check, text[-5000:])
    print('PASS', check)
shutil.copy2(data / 'audio-archive-window.png', output / 'audio-archive-window.png')
