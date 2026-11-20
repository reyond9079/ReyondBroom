#!/usr/bin/env python3
"""Check the real Duplicate shortcut, all viewports, and a single Undo."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import subprocess
from threading import Event
import time

from PIL import ImageChops, ImageGrab
from editor_os_input import NativeInput, read_ui_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    root, out = args.runtime.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    data = root / 'ftebroom'
    state = data / 'compilation-ui-live.json'
    state.unlink(missing_ok=True)
    maps = data / 'maps'
    maps.mkdir(exist_ok=True)
    (maps / 'duplicate-flash.map').write_text('''{
"classname" "worldspawn"
{
( -64 64 64 ) ( -64 -64 64 ) ( -64 -64 -64 ) ftebroom/grid 0 0 0 1 1
( 64 -64 -64 ) ( 64 -64 64 ) ( 64 64 64 ) ftebroom/grid 0 0 0 1 1
( 64 -64 64 ) ( 64 -64 -64 ) ( -64 -64 -64 ) ftebroom/grid 0 0 0 1 1
( -64 64 -64 ) ( 64 64 -64 ) ( 64 64 64 ) ftebroom/grid 0 0 0 1 1
( 64 64 -64 ) ( -64 64 -64 ) ( -64 -64 -64 ) ftebroom/grid 0 0 0 1 1
( -64 -64 64 ) ( -64 64 64 ) ( 64 64 64 ) ftebroom/grid 0 0 0 1 1
}
}
''', encoding='ascii')
    cfg = data / 'duplicate-flash.cfg'
    cfg.write_text('exec ftebroom.cfg\nin 6 mapedit maps/duplicate-flash.map\n'
                   'in 9 ftebroom_camera_restore 192 -256 160 24 125 0 90\n'
                   'in 10 ftebroom_test_desktop reyon8qa dark\n'
                   'in 11 ftebroom_select all\n', encoding='ascii')
    engine = root / ('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
    render = ['+set', 'vid_renderer', os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
    with (out / 'editor.log').open('wb') as log:
        process = subprocess.Popen([str(engine), *render, '-nohome', '-noupdates', '-nosound',
                                    '-window', '-width', '1280', '-height', '800',
                                    '+set', 'ftebroom_skip_setup', '1', '+set', 'ftebroom_input_qa', '1', '+exec', cfg.name],
                                   cwd=root, stdout=log, stderr=subprocess.STDOUT)

        def live():
            return read_ui_json(state)

        def wait(predicate, timeout=35):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                assert process.poll() is None, 'Editor exited'
                try:
                    if result := predicate():
                        return result
                except (OSError, ValueError, KeyError):
                    pass
                time.sleep(.05)
            raise AssertionError('Timed out in duplicate flash test')

        try:
            views = wait(lambda: live()['views'] if {v['type'] for v in live()['views'] if v['width'] > 0} == {0, 1, 2, 3} else None)
            ui = NativeInput(process, state, live, wait, out)
            ox, oy = ui.position()
            sx, sy = getattr(ui, 'scale', (1, 1))
            ui.move(views[0]['x'] + 30, views[0]['y'] + 35)
            time.sleep(2)
            bounds = [(int(ox + (v['x'] + 4) * sx), int(oy + (v['y'] + 28) * sy),
                       int(ox + (v['x'] + v['width'] - 4) * sx), int(oy + (v['y'] + v['height'] - 4) * sy)) for v in views]

            def amber_counts(frame):
                counts = []
                for view, area in zip(views, bounds):
                    red, green, blue = frame.crop(area).convert('RGB').split()
                    # The perspective pass tints the brush faces as well as drawing its edges.
                    r, g, b = (218, 150, 85) if view['type'] else (195, 175, 200)
                    warm = ImageChops.multiply(red.point(lambda i: 255 if i > r else 0),
                                               green.point(lambda i: 255 if i > g else 0))
                    counts.append(ImageChops.multiply(warm, blue.point(lambda i: 255 if i < b else 0)).histogram()[255])
                return counts

            baseline = amber_counts(ImageGrab.grab())
            stop = Event()

            def capture():
                best = None
                while not stop.is_set():
                    frame = ImageGrab.grab()
                    delta = [count - before for count, before in zip(amber_counts(frame), baseline)]
                    if best is None or (min(delta), sum(delta)) > (min(best[0]), sum(best[0])):
                        best = delta, frame
                    stop.wait(.04)
                return best

            with ThreadPoolExecutor(max_workers=1) as pool:
                captured = pool.submit(capture)
                try:
                    ui.key('d', ('ctrl',))
                    time.sleep(1.1)
                finally:
                    stop.set()
                delta, flash = captured.result()
            flash.save(out / 'duplicate-flash.png')
            expired_frame = ImageGrab.grab()
            remaining = [count - before for count, before in zip(amber_counts(expired_frame), baseline)]

            def planes(name):
                target = maps / (name + '.map')
                target.unlink(missing_ok=True)
                ui.console(f'mod_terrain_save "" {name}')
                wait(target.is_file)
                return sorted(line.strip() for line in target.read_text().splitlines() if line.lstrip().startswith('('))

            duplicated = planes('duplicate-result')
            ui.key('z', ('ctrl',))
            original = planes('duplicate-undo')
            assert len(original) == 6 and duplicated == sorted(original * 2), 'Duplicate changed the geometry or created more than one Undo step'
            assert min(delta) > 30, f'No brush highlight in one or more viewports: {delta}'
            if any(left >= max(30, peak * .1) for left, peak in zip(remaining, delta)):
                expired_frame.save(out / 'duplicate-expired.png')
                raise AssertionError(f'Flash did not expire: {remaining} remained from {delta}')
            print(f'PASS Ctrl+D flashes all four viewports, expires, and preserves geometry and Undo ({delta})')
        finally:
            process.kill()
            process.wait(timeout=15)


if __name__ == '__main__':
    main()
