#!/usr/bin/env python3
"""Edit loose and packed images in the native file manager and compare exported pixels."""
import argparse
import os
from pathlib import Path
import subprocess
import time
from zipfile import ZipFile

from PIL import Image, ImageGrab
from editor_os_input import NativeInput, read_ui_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    root, out = args.runtime.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    data = root / 'ftebroom'
    folder = data / 'browser132'
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'rb24-source.png'
    archive = folder / 'rb24-fixture.pk3'
    for copy in (folder / 'rb24-source-edited.png', folder / 'rb24-rotate.tga',
                 folder / 'rb24-crop.png', data / 'rb24-member-edited.png', data / 'rb24-resize.png'):
        copy.unlink(missing_ok=True)
    fixture = Image.new('RGBA', (24, 12))
    fixture.putdata([(x * 10, y * 20, x + y, 80 + y * 10) for y in range(12) for x in range(24)])
    fixture.save(source)
    original = source.read_bytes()
    with ZipFile(archive, 'w') as package:
        package.writestr('rb24-member.png', original)
    archive_original = archive.read_bytes()
    state = data / 'browser132-live.json'
    state.unlink(missing_ok=True)
    cfg = data / 'image-editor-qa.cfg'
    cfg.write_text('exec ftebroom.cfg\nin 6 mapedit maps/start.map\n'
                   'in 9 ftebroom_test_desktop browser132\n', encoding='ascii')
    engine = root / ('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
    renderer = ['+set', 'vid_renderer', os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []

    with (out / 'editor.log').open('wb') as log:
        process = subprocess.Popen([str(engine), *renderer, '-nohome', '-noupdates', '-nosound',
                                    '-window', '-width', '1280', '-height', '800',
                                    '+set', 'ftebroom_skip_setup', '1', '+set', 'ftebroom_input_qa', '1',
                                    '+exec', cfg.name], cwd=root, stdout=log, stderr=subprocess.STDOUT)

        def live():
            return read_ui_json(state)

        def wait(predicate, timeout=40):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                assert process.poll() is None, 'Editor exited'
                try:
                    if result := predicate():
                        return result
                except (OSError, ValueError, KeyError):
                    pass
                time.sleep(.07)
            raise AssertionError('Timed out waiting for the image editor')

        ui = NativeInput(process, state, live, wait, out, focus_required=True)

        def rect(key):
            return wait(lambda: live()['widgets'].get(key))

        def click(key, button=1, u=.5, v=.5):
            ui.settle()
            box = rect(key)
            assert box['visible'], f'{key} is clipped'
            ui.click(box['x'] + box['width'] * u, box['y'] + box['height'] * v, button)

        def open_image(name):
            click('files/item/' + name, button=3)
            click('files/edit-image')
            rect('image-editor/canvas')

        def fill(key, value):
            click(key, u=.25)
            ui.key('a', ('ctrl',))
            ui.text(value)

        def save(target, use_default=False):
            click('image-editor/save')
            if not use_default:
                fill('image-editor/name', target.name)
            click('image-editor/save-confirm')
            wait(target.is_file)
            with Image.open(target) as exported:
                return exported.convert('RGBA')

        def menu(label):
            click('image-editor/transform')
            click('image-editor/' + label)

        def close_image():
            window = rect('image-editor/window')
            ui.click(window['x'] + window['width'] - 14, window['y'] + 11)

        def capture(name):
            x, y = ui.position()
            w, h = live()['display']
            sx, sy = getattr(ui, 'scale', (1, 1))
            ImageGrab.grab(bbox=(x, y, x + int(w * sx), y + int(h * sy))).save(out / name)

        try:
            wait(lambda: live()['folder'].endswith('/browser132'))
            open_image(source.name)
            menu('Rotate right')
            ui.key('z', ('ctrl',))
            result = save(folder / 'rb24-source-edited.png', use_default=True)
            assert result.size == fixture.size and result.tobytes() == fixture.tobytes()
            click('image-editor/redo')
            rotated = fixture.transpose(Image.Transpose.ROTATE_270)
            result = save(folder / 'rb24-rotate.tga')
            assert result.size == rotated.size and result.tobytes() == rotated.tobytes()
            assert source.read_bytes() == original
            print('PASS loose PNG, clockwise rotation, Undo/Redo and alpha-preserving PNG/TGA', flush=True)

            click('image-editor/tool-Brush')
            click('image-editor/canvas', u=.45, v=.45)
            click('image-editor/tool-Eraser')
            click('image-editor/canvas', u=.45, v=.45)
            click('image-editor/tool-Crop')
            click('image-editor/canvas', u=.45, v=.45)
            click('image-editor/apply-crop')
            close_image()
            click('image-editor/keep-editing')
            result = save(folder / 'rb24-crop.png')
            assert result.size == (1, 1) and result.getpixel((0, 0)) == (51, 166, 255, 0)
            click('image-editor/save')
            fill('image-editor/name', source.name)
            click('image-editor/save-confirm')
            assert 'image-editor/save-cancel' in live()['widgets'] and source.read_bytes() == original
            click('image-editor/save-cancel')
            print('PASS brush, eraser, crop and protection from closing or replacing the source', flush=True)

            close_image()
            wait(lambda: 'image-editor/canvas' not in live()['widgets'])
            click('files/item/' + archive.name)
            wait(lambda: live()['folder'].endswith('/' + archive.name))
            open_image('rb24-member.png')
            menu('Flip horizontally')
            result = save(data / 'rb24-member-edited.png', use_default=True)
            flipped = fixture.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            assert result.tobytes() == flipped.tobytes()
            assert archive.read_bytes() == archive_original
            print('PASS archive image opens, flips and exports without changing its archive', flush=True)
            click('image-editor/resize')
            fill('image-editor/width', '12')
            fill('image-editor/height', '6')
            click('image-editor/resize-apply')
            click('image-editor/adjust')
            click('image-editor/brightness', u=.64)
            click('image-editor/adjust-apply')
            result = save(data / 'rb24-resize.png')
            assert result.size == (12, 6)
            expected = [flipped.getpixel((x * 2, y * 2)) for y in range(6) for x in range(12)]
            assert list(result.getchannel('A').getdata()) == [p[3] for p in expected]
            assert result.getpixel((8, 2))[0] > flipped.getpixel((16, 4))[0]
            print('PASS resize and brightness adjustment preserve expected alpha and pixels', flush=True)
            capture('image-editor.png')
        except Exception:
            if process.poll() is None and state.is_file():
                try:
                    capture('failure.png')
                except Exception:
                    pass
            raise
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=15)


if __name__ == '__main__':
    main()
