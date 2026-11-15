"""Exercise offline MAP and prefab loading using a disposable runtime."""

import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile


def physical(path):
    if os.name == 'nt' and len(str(path)) >= 248:
        return Path('\\\\?\\' + str(path))
    return path


def run_case(bundle, directory, case):
    fixtures = Path(__file__).resolve().parent / 'fixtures'
    runtime = directory / 'editor'
    runtime.parent.mkdir(parents=True)
    shutil.copytree(bundle, runtime)

    working = directory / 'Map projects' / 'Карты'
    if case == 'long':
        working = working.joinpath(*(['Deep project folder with spaces 123456789'] * 6))
    physical(working).mkdir(parents=True)
    source = (fixtures / 'document.map').read_bytes()
    prefab = (fixtures / 'containers.map').read_bytes()
    physical(working / 'existing.map').write_bytes(source)
    physical(working / 'collection.map').write_bytes(prefab)
    if case == 'win32-alias':
        # The ordinary Windows API ignores the trailing dot in this component.
        working = working.with_name(working.name + '.')
    if case == 'pak':
        entry = b'maps/content.map'
        payload_end = 12 + len(source)
        table = struct.pack('<56sII', entry, 12, len(source))
        (working / 'project.pak').write_bytes(
            struct.pack('<4sII', b'PACK', payload_end, len(table)) + source + table)
        target = working / 'project.pak' / 'maps' / 'content.map'
    elif case == 'pk3':
        archive_path = working / 'project.pk3'
        with zipfile.ZipFile(physical(archive_path), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('maps/content.map', source)
        target = archive_path / 'maps' / 'content.map'
    else:
        target = working / 'existing.map'
    saved = working / 'map saved as.map'

    commands = [
        'exec ftebroom.cfg',
        'set ftebroom_skip_setup 1',
        f'in 6 ftebroom_test_filepath open "{target.as_posix()}"',
        'in 9 ftebroom_test_filepath state',
        f'in 10 ftebroom_test_filepath save "{saved.as_posix()}"',
        'in 12 mapedit maps/start.map',
        f'in 13 set ftebroom_test_import_root "{working.as_posix()}"',
        'in 15 ftebroom_test_desktop prefab56',
        'in 17 ftebroom_test_desktop prefab56insert',
        'in 20 ftebroom_test_desktop prefab56one',
        'in 21 quit force',
    ]
    data = runtime / 'ftebroom'
    (data / 'map-template-paths.cfg').write_text('\n'.join(commands) + '\n', encoding='utf-8')
    (data / 'qconsole.log').unlink(missing_ok=True)
    engine = runtime / ('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
    options = {}
    if os.name == 'nt':
        options['creationflags'] = subprocess.CREATE_NO_WINDOW
    environment = dict(os.environ, LIBGL_ALWAYS_SOFTWARE='1', SDL_VIDEODRIVER='x11')
    environment.pop('WAYLAND_DISPLAY', None)
    with (directory / 'console.txt').open('wb') as output:
        result = subprocess.run(
            [str(engine), '-nohome', '-noupdates', '-window', '-width', '1280', '-height', '800',
             '-condebug', '-basedir', str(runtime), '+exec', 'map-template-paths.cfg'],
            cwd=runtime, env=environment, stdout=output, stderr=subprocess.STDOUT,
            timeout=70, **options)
    log_file = data / 'qconsole.log' if os.name == 'nt' else directory / 'console.txt'
    log = log_file.read_text(encoding='utf-8', errors='replace')
    assert result.returncode == 0, log
    assert all(word not in log for word in ('_FAIL ', 'CSQC_Abort', 'ReyondBroom save failed')), log
    # FTE's console may transcode non-ASCII ancestors. The filename is ASCII
    # and the round trip below proves which geometry was loaded.
    states = [line.split(' map=', 1)[1].split(' baseq3=', 1)[0]
              for line in log.splitlines() if 'FTEBROOM_PATH_STATE ' in line]
    assert len(states) == 1 and Path(states[0]).name == target.name, log
    assert 'FTEBROOM_DESKTOP_PASS prefab56_collection' in log, log
    assert 'FTEBROOM_DESKTOP_PASS prefab56_one_instance' in log, log
    roundtrip = physical(saved).read_bytes()
    assert b'patchDef2' in roundtrip and b'worldspawn' in roundtrip
    assert physical(working / 'existing.map').read_bytes() == source
    assert physical(working / 'collection.map').read_bytes() == prefab
    print(f'PASS {case}: opened existing MAP, saved a copy, loaded and placed its prefab collection')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    cases = ['ordinary', 'long', 'pak', 'pk3']
    if os.name == 'nt':
        cases.insert(1, 'win32-alias')
    parser.add_argument('--case', choices=cases, action='append')
    args = parser.parse_args()
    for case in args.case or cases:
        run_case(args.bundle.resolve(), args.output.resolve() / case, case)


if __name__ == '__main__':
    main()
