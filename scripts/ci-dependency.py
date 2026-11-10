#!/usr/bin/env python3
"""Exact-input dependency cache keys and verified build outputs; no fuzzy restores."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

REPO = Path(__file__).resolve().parents[1]
BULLET_FLAGS = ['-DCMAKE_POLICY_VERSION_MINIMUM=3.5', '-DCMAKE_BUILD_TYPE=Release',
    '-DCMAKE_POSITION_INDEPENDENT_CODE=ON', '-DBUILD_SHARED_LIBS=OFF',
    '-DBUILD_BULLET2_DEMOS=OFF', '-DBUILD_OPENGL3_DEMOS=OFF', '-DBUILD_CPU_DEMOS=OFF',
    '-DBUILD_UNIT_TESTS=OFF', '-DBUILD_EXTRAS=OFF', '-DBUILD_BULLET3=OFF', '-DINSTALL_LIBS=OFF']


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inventory(root):
    # CTest results are not build outputs, and MUST be produced on every run.
    return {p.relative_to(root).as_posix(): sha(p) for p in sorted(root.rglob('*'))
            if p.is_file() and not {'.git', 'Testing'} & set(p.relative_to(root).parts)
            and p.name != '.ci-cache.json'}


def toolchain():
    result = {}
    for command in ('g++', 'gcc', 'cmake', 'make', 'ld'):
        binary = shutil.which(command)
        if not binary:
            raise RuntimeError('Missing dependency build tool: '+command)
        result[command] = {'version': subprocess.check_output([binary, '--version'], text=True).splitlines()[0],
                           'binary': sha(binary)}
    for name in ('CC', 'CXX'):
        if os.environ.get(name):
            command = shlex.split(os.environ[name])[0]
            binary = shutil.which(command)
            if not binary:
                raise RuntimeError('Missing configured compiler: '+command)
            result[name] = {'path': binary, 'binary': sha(binary),
                            'version': subprocess.check_output([binary, '--version'], text=True).splitlines()[0]}
    if shutil.which('pacman'):
        result['packages'] = subprocess.check_output(['pacman', '-Q'], text=True)
        if shutil.which('mingw32-make'):
            result['mingw32-make'] = subprocess.check_output(['mingw32-make', '--version'], text=True).splitlines()[0]
    else:
        result['system'] = os.uname().sysname+' '+os.uname().machine
        if shutil.which('dpkg-query'):
            result['packages'] = subprocess.check_output(['dpkg-query', '-W'], text=True)
    return result


def fingerprint(component, source, platform, tools=None, destination=None):
    recipes = {'bullet': ['scripts/ci-dependency.py'],
               'profile': ['scripts/ci-dependency.py', 'scripts/build-profile-support.py'],
               'audio': ['scripts/ci-dependency.py', 'scripts/build-audio.sh']}[component]
    inputs = {name: sha(REPO/name) for name in recipes}
    if component == 'audio':
        inputs['ffmpeg-source'] = sha(source/'ffmpeg-8.0.1.tar.xz')
    elif component == 'bullet':
        inputs['bullet'] = inventory(source)
    else:
        for path in ('native/profile', 'native/third_party/trenchbroom', 'native/third_party/fmt'):
            inputs[path] = inventory(REPO/path)
        # Cached test/probe executables must also match their sources/fixtures.
        names = ['trenchbroom-profiles.cpp', 'compilation-profile-core.cpp', 'engine-launch.cpp',
                 'profile-order.cpp', 'compilation-profile-probe.cpp',
                 'fixtures/CompilationProfiles-original.cfg', 'fixtures/CompilationProfiles-cpu-count.cfg']
        inputs['tests'] = {name: sha(REPO/'tests'/name) for name in names}
    identity = {'schema': 1, 'component': component, 'platform': platform,
                'workspace': str(REPO), 'source': str(source.resolve()),
                'build_directory': str(destination.resolve()) if destination is not None else None,
                'image': os.environ.get('ImageVersion', ''),
                'flags': BULLET_FLAGS if component == 'bullet' else 'recipe',
                'environment': {k: os.environ.get(k, '') for k in
                    ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS',
                     'CMAKE_GENERATOR', 'CMAKE_GENERATOR_PLATFORM', 'CMAKE_GENERATOR_TOOLSET',
                     'CMAKE_TOOLCHAIN_FILE', 'PKG_CONFIG_PATH')},
                'toolchain': tools if tools is not None else toolchain(), 'inputs': inputs}
    if os.environ.get('CMAKE_TOOLCHAIN_FILE'):
        inputs['toolchain_file'] = sha(os.environ['CMAKE_TOOLCHAIN_FILE'])
    return 'rb-dep-v1-'+component+'-'+hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()


def valid(root, key):
    try:
        saved = json.loads((root/'.ci-cache.json').read_text())
        return saved['key'] == key and saved['files'] == inventory(root) and bool(saved['files'])
    except (OSError, ValueError, KeyError):
        return False


def build(component, source, destination):
    if component == 'audio':
        destination.mkdir(parents=True, exist_ok=True)
        subprocess.run(['bash', (REPO/'scripts/build-audio.sh').as_posix(), source.resolve().as_posix()],
                       env={**os.environ, 'RB_AUDIO_OUTPUT_DIR': destination.resolve().as_posix()}, check=True)
    elif component == 'profile':
        args = ['python', str(REPO/'scripts/build-profile-support.py'), 'core', '--build-dir', str(destination), '--tests']
        if os.name == 'nt':
            args += ['--generator', 'MinGW Makefiles']
        subprocess.run(args, check=True)
    else:
        args = ['cmake', '-S', str(source), '-B', str(destination), *BULLET_FLAGS]
        if os.name == 'nt':
            args += ['-G', 'MinGW Makefiles']
        subprocess.run(args, check=True)
        subprocess.run(['cmake', '--build', str(destination), '-j4', '--target',
                        'BulletDynamics', 'BulletCollision', 'LinearMath'], check=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('operation', choices=['key', 'verify', 'build', 'install'])
    p.add_argument('--component', choices=['bullet', 'profile', 'audio'], required=True)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--destination', type=Path, required=True)
    p.add_argument('--platform', required=True)
    p.add_argument('--key')
    a = p.parse_args()
    if a.operation == 'key':
        key = fingerprint(a.component, a.source, a.platform, destination=a.destination)
        output = {'key': key}
    elif a.operation == 'verify':
        assert a.key, 'A pre-build fingerprint is required'
        output = {'valid': str(valid(a.destination, a.key)).lower()}
    elif a.operation == 'install':
        assert a.component == 'audio' and a.key and valid(a.destination, a.key)
        shutil.copytree(a.destination, REPO/'tools/audio', dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns('.ci-cache.json'))
        output = {'installed': 'true'}
    else:
        assert a.key, 'A pre-build fingerprint is required'
        if a.destination.exists():
            # Only the explicit dependency output can be replaced, never sources.
            target = a.destination.resolve()
            assert target != a.source.resolve() and target != REPO and REPO in target.parents
            shutil.rmtree(target)
        build(a.component, a.source, a.destination)
        assert fingerprint(a.component, a.source, a.platform, destination=a.destination) == a.key, 'Dependency inputs changed during build'
        files = inventory(a.destination)
        assert files, 'Dependency build produced no outputs'
        (a.destination/'.ci-cache.json').write_text(json.dumps({'key': a.key, 'files': files}, sort_keys=True))
        assert valid(a.destination, a.key), 'Dependency output verification failed'
        output = {'valid': 'true'}
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
            for key, value in output.items():
                stream.write(key+'='+value+'\n')
    print(json.dumps(output))


if __name__ == '__main__':
    main()
