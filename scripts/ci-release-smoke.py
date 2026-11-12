#!/usr/bin/env python3
"""Small, mandatory release gate: archives, manifest, version, native startup."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


def digest(stream):
    return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify(release, platform, version):
    windows = platform == 'Windows-x64'
    prefix = 'ReyondBroom-'+version+'-'+platform
    runtime = release/(prefix+('.zip' if windows else '.tar.gz'))
    source = release/(prefix+'-Sources.zip')
    for path in (runtime, source):
        expected = path.with_name(path.name+'.sha256').read_text().strip()
        with path.open('rb') as f:
            assert expected == digest(f)+'  '+path.name, 'Archive checksum mismatch: '+path.name
    hashes, kept = {}, {}
    root_name = 'ReyondBroom-'+('Windows-x64' if windows else 'Linux-x64')
    def member(name, stream):
        path = PurePosixPath(name)
        assert not path.is_absolute() and '..' not in path.parts and '\\' not in name
        assert path.parts[0] == root_name
        name = '/'.join(path.parts[1:])
        assert name and name not in hashes, 'Duplicate runtime entry'
        if name in ('distribution-manifest.json', 'build-info.txt'):
            body = stream.read(); kept[name] = body
            hashes[name] = hashlib.sha256(body).hexdigest()
        else:
            hashes[name] = digest(stream)
    if windows:
        with zipfile.ZipFile(runtime) as z:
            for entry in z.infolist():
                if not entry.is_dir():
                    with z.open(entry) as stream:
                        member(entry.filename, stream)
    else:
        with tarfile.open(runtime, 'r|gz') as t:
            for entry in t:
                if not entry.isdir():
                    assert entry.isfile(), 'Unexpected runtime link/type'
                    with t.extractfile(entry) as stream:
                        member(entry.name, stream)
    manifest = json.loads(kept['distribution-manifest.json'])
    assert {k:v for k,v in hashes.items() if k != 'distribution-manifest.json'} == manifest, 'Runtime manifest differs'
    info = kept['build-info.txt'].decode().replace('\r\n', '\n')
    assert info.startswith('ReyondBroom '+version+'\n') and '\nPlatform: '+platform+'\n' in info
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    assert '\nEditor revision: '+revision+'\n' in info, 'Wrong packaged source revision'
    names = ['ftebroom/csaddon.dat', 'ftebroom/ui/update-splash.png', 'bin/fteqw64.exe' if windows else 'fteqw-gl64',
             'fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so']
    for name in names:
        assert name in manifest
        with (release/root_name/name).open('rb') as stream:
            assert digest(stream) == manifest[name], 'Extracted launch binary differs from archive'
    with zipfile.ZipFile(source) as z:
        assert z.read('RELEASE-BUILD.txt').decode().replace('\r\n', '\n').startswith(info)
        for name in ('sources/fteqw-source.zip', 'sources/bullet-source.zip',
                     'sources/imgui-source.zip', 'sources/ftebroom/native/ftebroom.cpp'):
            assert name in z.namelist(), 'Missing corresponding source: '+name
        with z.open('sources/ftebroom/runtime/ui/update-splash.png') as stream:
            assert digest(stream)==manifest['ftebroom/ui/update-splash.png'], 'Splash artwork differs from corresponding source'
    return release/root_name, {'platform': platform, 'version': version, 'revision': revision,
                               'manifest_files': len(manifest), 'components': {n:manifest[n] for n in names}}


def launch(root, output):
    with tempfile.TemporaryDirectory(prefix='reyond-release-smoke-') as directory:
        clone = Path(directory)/root.name
        shutil.copytree(root, clone)
        data = clone/'ftebroom'
        (data/'qconsole.log').unlink(missing_ok=True)
        # SDL can spend several seconds retrying OpenGL without multisampling
        # before the first ImGui frame on a software-rendered runner.
        (data/'ci-release-smoke.cfg').write_text('exec ftebroom.cfg\nset ftebroom_skip_setup 1\nin 4 echo RB_RELEASE_SMOKE_BOOT\nin 18 quit force\n')
        engine = clone/('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
        render = ['+set', 'vid_renderer', os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
        with (output/'startup.log').open('wb') as stream:
            subprocess.run([str(engine), *render, '-ftebroom-child-processes', '-nohome', '-noupdates', '-nosound',
                            '-window', '-width', '960', '-height', '640', '-condebug', '-basedir', str(clone),
                            '+exec', 'ci-release-smoke.cfg'], cwd=clone, stdout=stream, stderr=subprocess.STDOUT,
                           env={**os.environ, 'LIBGL_ALWAYS_SOFTWARE': '1'}, check=True, timeout=45)
        text = (output/'startup.log').read_text(errors='replace')
        if (data/'qconsole.log').exists():
            text += '\n'+(data/'qconsole.log').read_text(errors='replace')
        (output/'startup.log').write_text(text)
        assert 'RB_RELEASE_SMOKE_BOOT' in text
        assert re.search(r'Dear ImGui renderer initialized \(atlas [1-9]', text), 'Native editor did not initialize'
        assert not re.search(r'CSQC_Abort|Assertion .*failed|FTEBROOM_\w+_FAIL', text), 'Startup reported failure'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('release', type=Path)
    p.add_argument('--platform', required=True, choices=['Windows-x64', 'Linux-x64', 'Linux-SDL2-x64'])
    p.add_argument('--version', required=True)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    root, proof = verify(a.release.resolve(), a.platform, a.version)
    launch(root, a.output)
    proof['native_startup'] = True
    (a.output/'results.json').write_text(json.dumps(proof, indent=2)+'\n')
    print('PASS packaged version, manifest, sources, archive checksums and native startup: '+a.platform)


if __name__ == '__main__':
    main()
