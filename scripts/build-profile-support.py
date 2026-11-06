#!/usr/bin/env python3
"""Build the TB-compatible core and the in-process native Qt Compile component."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

repo = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('component', choices=['core', 'ui', 'deploy'])
p.add_argument('--build-dir', required=True, type=Path)
p.add_argument('--cmake', default='cmake')
p.add_argument('--compiler')
p.add_argument('--generator')
p.add_argument('--qt-root', type=Path)
p.add_argument('--output', type=Path)
p.add_argument('--tests', action='store_true')
a = p.parse_args()

def run(*args):
    subprocess.run([str(x) for x in args], check=True)

if a.component in ['core', 'ui']:
    args = [a.cmake, '-S', repo/'native'/('profile' if a.component == 'core' else 'compile_ui'),
            '-B', a.build_dir, '-DCMAKE_BUILD_TYPE=Release']
    if a.compiler:
        args += ['-DCMAKE_CXX_COMPILER='+a.compiler]
    if a.generator:
        args += ['-G', a.generator]
    if a.component == 'ui':
        if not a.qt_root:
            p.error('--qt-root is required for the UI')
        args += ['-DCMAKE_PREFIX_PATH='+str(a.qt_root.resolve())]
    if a.tests:
        args += ['-DRB_PROFILE_TESTS=ON']
    run(*args)
    run(a.cmake, '--build', a.build_dir, '--config', 'Release', '-j4')
    if a.tests:
        ctest = Path(a.cmake).with_name('ctest.exe' if os.name == 'nt' else 'ctest')
        run(str(ctest) if ctest.is_file() else 'ctest', '--test-dir', a.build_dir, '-C', 'Release', '--output-on-failure')
else:
    if not a.qt_root or not a.output:
        p.error('--qt-root and --output (runtime root) are required for deploy')
    qt = a.qt_root.resolve()
    root = a.output.resolve()
    target = root/'tools/compile-ui'
    target.mkdir(parents=True, exist_ok=True)
    windows = os.name == 'nt'
    filename = 'reyond-compile-ui.dll' if windows else 'libreyond-compile-ui.so'
    candidates = [a.build_dir/filename, a.build_dir/'Release'/filename]
    binary = next((x for x in candidates if x.is_file()), None)
    if not binary:
        p.error('Build the UI component before deploying it')
    shutil.copy2(binary, target/filename)
    if windows:
        run(qt/'bin/windeployqt.exe', '--release', '--no-translations', '--compiler-runtime', target/filename)
    else:
        libraries = target/'lib'
        libraries.mkdir(exist_ok=True)
        names = ['Core', 'Gui', 'Widgets', 'Svg', 'DBus', 'XcbQpa']
        for name in names:
            for path in (qt/'lib').glob('libQt6'+name+'.so.6*'):
                shutil.copy2(path, libraries/path.name)
        icu = list((qt/'lib').glob('libicu*.so*'))
        if not icu:
            raise SystemExit('The official Qt Linux kit needs its pinned ICU archive (aqt --archives icu)')
        for path in icu:
            shutil.copy2(path, libraries/path.name)
        for folder in ['platforms', 'imageformats', 'iconengines', 'platformthemes', 'styles']:
            source = qt/'plugins'/folder
            if source.is_dir():
                shutil.copytree(source, target/'plugins'/folder, dirs_exist_ok=True)
        # The in-process library is relocatable; discard SDK build paths.
        run('patchelf', '--set-rpath', '$ORIGIN/lib', target/filename)
    shutil.copytree(repo/'runtime/ui/compilation', root/'ftebroom/ui/compilation', dirs_exist_ok=True)
    licenses = target/'licenses'
    licenses.mkdir(exist_ok=True)
    shutil.copy2(repo/'native/third_party/trenchbroom/LICENSE.txt', licenses/'TrenchBroom-GPL-3.0.txt')
    for folder in [qt/'LICENSES', qt/'licenses']:
        if folder.is_dir():
            shutil.copytree(folder, licenses/'Qt', dirs_exist_ok=True)
    (target/'SOURCE-CODE.txt').write_text(
        'UI source: native/compile_ui and native/third_party/trenchbroom/TbUiLib in the corresponding ReyondBroom sources.\n'
        'Dynamic Qt 6.10.3 (LGPLv3/GPLv3), unmodified: https://download.qt.io/official_releases/qt/6.10/6.10.3/submodules/\n'
        'Linux ICU 73.2: https://github.com/unicode-org/icu/releases/tag/release-73-2\n'
        'Rebuild/deploy: scripts/build-profile-support.py; complete platform build instructions: docs/compilation-profiles-build.md.\n',
        encoding='utf-8')
