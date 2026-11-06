#!/usr/bin/env python3
"""Build and test the Qt-free TrenchBroom-compatible profile core."""
import argparse
import os
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('component', choices=['core'])
p.add_argument('--build-dir', required=True, type=Path)
p.add_argument('--cmake', default='cmake')
p.add_argument('--compiler')
p.add_argument('--generator')
p.add_argument('--tests', action='store_true')
a = p.parse_args()

def run(*args):
    subprocess.run([str(x) for x in args], check=True)

if a.component == 'core':
    args = [a.cmake, '-S', repo/'native/profile',
            '-B', a.build_dir, '-DCMAKE_BUILD_TYPE=Release']
    if a.compiler:
        args += ['-DCMAKE_CXX_COMPILER='+a.compiler]
    if a.generator:
        args += ['-G', a.generator]
    if a.tests:
        args += ['-DRB_PROFILE_TESTS=ON']
    run(*args)
    run(a.cmake, '--build', a.build_dir, '--config', 'Release', '-j4')
    if a.tests:
        ctest = Path(a.cmake).with_name('ctest.exe' if os.name == 'nt' else 'ctest')
        run(str(ctest) if ctest.is_file() else 'ctest', '--test-dir', a.build_dir, '-C', 'Release', '--output-on-failure')
