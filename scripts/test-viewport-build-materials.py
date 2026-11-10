#!/usr/bin/env python3
"""Compile the current FTE MAP via the toolbar and read back the Q3 BSP."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time

from PIL import ImageGrab

from editor_os_input import NativeInput, read_ui_json


def tga(width, height, color):
    header = bytearray(18)
    header[2] = 2
    struct.pack_into('<HHBB', header, 12, width, height, 24, 32)
    return bytes(header) + bytes(reversed(color)) * (width * height)


def q3_lumps(path):
    data = path.read_bytes()
    assert data[:8] == b'IBSP' + struct.pack('<i', 46), path
    result = []
    for index in range(17):
        offset, size = struct.unpack_from('<II', data, 8 + index * 8)
        assert offset + size <= len(data), (path, index)
        result.append(data[offset:offset + size])
    return result


def material_vertices(lumps, material):
    names = [lumps[1][at:at + 64].split(b'\0', 1)[0].decode()
             for at in range(0, len(lumps[1]), 72)]
    faces = []
    for at in range(0, len(lumps[13]), 104):
        shader, _, _, first, count = struct.unpack_from('<5i', lumps[13], at)
        if names[shader] == material:
            faces.append([struct.unpack_from('<5f', lumps[10], vertex * 44)
                          for vertex in range(first, first + count)])
    return faces


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    original, out = args.runtime.resolve(), args.output.resolve()
    assert original != out and original not in out.parents
    out.mkdir(parents=True, exist_ok=True)
    root = out / 'runtime'
    shutil.copytree(original, root, ignore=shutil.ignore_patterns('downloads', 'updates'))
    data = root / 'ftebroom'
    game = out / 'fixture' / 'baseq3'
    (game / 'maps').mkdir(parents=True)
    (game / 'scripts').mkdir()
    (game / 'scripts/shaderlist.txt').write_text('// The compiler must add editor-side shaders.\n')
    textures = data / 'textures/rb25'
    textures.mkdir(parents=True)
    (textures / 'old.tga').write_bytes(tga(64, 64, (210, 70, 30)))
    (textures / 'plate.tga').write_bytes(tga(128, 32, (30, 150, 210)))
    (data / 'scripts/rb25-test.shader').write_text(
        'textures/rb25/new\n{\n qer_editorimage textures/rb25/plate.tga\n'
        ' {\n  map $lightmap\n  rgbGen identity\n }\n'
        ' {\n  map textures/rb25/plate.tga\n  blendfunc filter\n }\n}\n')
    source = game / 'maps/room.map'
    subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / 'tests/make-build-fixture.py'),
                    str(source)], check=True)
    scene = source.read_text()
    light = '{\n"classname" "light"\n"origin" "0 0 192"\n"light" "500"\n}\n'
    assert light in scene
    scene = scene.replace(light, '')
    lines = scene.splitlines()
    north = [i for i, line in enumerate(lines) if line.startswith('(')
             and [point.split()[1] for point in re.findall(r'\(([^)]+)\)', line)[:3]] == ['256'] * 3]
    assert north, 'Missing wall on y=256'
    lines[north[-1]] = lines[north[-1]].replace('ftebroom/grid', 'rb25/old')
    source.write_text('// ReyondBroom profile=when-the-sky-went-dark format=quake3\n' + '\n'.join(lines) + '\n')
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    for folder in (root, data):
        (folder / 'ftebroom-project.txt').write_text(game.as_posix())
        (folder / 'ftebroom-game-profile.txt').write_text('1 0 "" "" "" "" 1 5 "" 1 1\n')
    state = data / 'placement-ui-live.json'
    state.unlink(missing_ok=True)
    (data / 'rb25-build-qa.cfg').write_text(
        'exec ftebroom.cfg\nset ftebroom_placement_qa 1\n'
        'in 8 ftebroom_test_filepath open ' + json.dumps(source.as_posix()) + '\n'
        'in 12 set ftebroom_view_layout 1\n')
    engine = root / ('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
    renderer = os.environ.get('RB_QA_RENDERER')
    render = ['+set', 'vid_renderer', renderer] if renderer else []
    with (out / 'editor.log').open('wb') as log:
        process = subprocess.Popen([str(engine), *render, '-ftebroom-child-processes',
            '-nohome', '-noupdates', '-nosound', '-condebug', '-window', '-width', '1600', '-height', '1000',
            '-basedir', str(root), '+set', 'ftebroom_skip_setup', '1', '+set', 'ftebroom_input_qa', '1',
            '+exec', 'rb25-build-qa.cfg'], cwd=root, stdout=log, stderr=subprocess.STDOUT)

        def live():
            return read_ui_json(state)

        def wait(predicate, timeout=90):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                assert process.poll() is None, 'Editor exited'
                try:
                    if value := predicate():
                        return value
                except (OSError, KeyError, ValueError):
                    pass
                time.sleep(.08)
            raise AssertionError('Timed out; see editor.log and the placement receipt')

        ui = NativeInput(process, state, live, wait, out, focus_required=True)

        def compile_map(previous=None):
            button = wait(lambda: live()['widgets'].get('build/compile'))
            ui.click(button['x'] + button['width'] / 2, button['y'] + button['height'] / 2)
            def completed():
                for build in game.glob('maps/_ftebroom-build/*/build.log'):
                    if build.parent != previous and 'Completed successfully.' in build.read_text(errors='replace'):
                        return build.parent
            folder = wait(completed)
            wait(lambda: live()['bspPreview'] and not live()['buildRunning'])
            return folder

        try:
            wait(lambda: Path(live()['mapPath']) == source and live()['profile'] == 1)
            ui.position()
            if os.name != 'nt':
                ui.window.set_input_focus(ui.X.RevertToParent, ui.X.CurrentTime)
                ui.d.sync()
            ui.console('ftebroom_camera_restore 0 -160 96 0 90 0 90')
            first = compile_map()
            initial = q3_lumps(first / 'snapshot.bsp')
            assert material_vertices(initial, 'textures/rb25/old')
            assert initial[14] and not any(initial[14]), 'Compiler should preserve the unlit BSP'
            assert live()['unlitPreview'], 'The unlit BSP should remain visible in Show BSP'
            ui.settle()
            capture = ImageGrab.grab()
            capture.save(out / 'unlit-preview.png')
            view = live()['views'][0]
            if os.name != 'nt':
                ox, oy = ui.position()
                x, y = (int(ox + view['x'] + view['width'] * .73), int(oy + view['y'] + view['height'] * .53))
                assert sum(capture.getpixel((x, y))[:3]) > 40, 'Unlit viewport remains black'
            cache = data / 'maps/ftebroom-build' / first.name / 'snapshot.bsp'
            assert all(q3_lumps(cache)[14]), 'Only the preview copy gets neutral lightmaps'

            ui.key('esc')
            wait(lambda: not live()['bspPreview'])
            ui.console('set ftebroom_material_query textures/rb25/old')
            ui.console('ftebroom_material_select faces')
            ui.console('ftebroom_material textures/rb25/new')
            ui.console('ftebroom_uv sx 0.5 0')
            ui.console('ftebroom_uv sy 2 0')
            second = compile_map(first)
            revised = q3_lumps(second / 'snapshot.bsp')
            faces = material_vertices(revised, 'textures/rb25/new')
            assert faces and not material_vertices(revised, 'textures/rb25/old'), 'BSP retained the previous material'
            for face in faces:
                ranges = [max(v[i] for v in face) - min(v[i] for v in face) for i in (0, 2, 3, 4)]
                assert ranges[0] > 0 and ranges[1] > 0
                assert abs(ranges[2] - ranges[0] / (128 * .5)) < .002, ranges
                assert abs(ranges[3] - ranges[1] / (32 * 2)) < .002, ranges
            compiler_log = (second / 'build.log').read_text(errors='replace')
            assert "Couldn't find image for shader textures/rb25/new" not in compiler_log
            assert hashlib.sha256(source.read_bytes()).hexdigest() == digest, 'Compile modified the source MAP'
            ImageGrab.grab().save(out / 'recompiled-material.png')
            print('PASS: actual toolbar Compile preserves changed material and rectangular-image UVs; unlit preview leaves original BSP and MAP untouched.')
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=15)


if __name__ == '__main__':
    main()
