"""Rebuild a native editor distribution from a verified compatible base runtime.

Engine/CSQC/plugins/game compilers stay pinned to the base distribution. Windows
must supply the rebuilt audio helper used by the current multitrack editor.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import shutil
import subprocess
import tarfile
import zipfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--platform', choices=['Windows-x64', 'Linux-x64', 'Linux-SDL2-x64'], required=True)
p.add_argument('--base-runtime', type=Path, required=True)
p.add_argument('--base-sources', type=Path, required=True)
p.add_argument('--module', type=Path, required=True)
p.add_argument('--audio-dir', type=Path)
p.add_argument('--version', required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
windows = a.platform == 'Windows-x64'
if windows and (not a.audio_dir or not (a.audio_dir / 'ffmpeg.exe').is_file()):
    p.error('Windows requires the rebuilt audio helper and corresponding sources')
src = Path(__file__).resolve().parents[1]
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
if windows:
    with zipfile.ZipFile(a.base_runtime) as archive:
        archive.extractall(out)
else:
    with tarfile.open(a.base_runtime) as archive:
        archive.extractall(out, filter='data')
root = out / ('ReyondBroom-Windows-x64' if windows else 'ReyondBroom-Linux-x64')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

manifest = json.loads((root / 'distribution-manifest.json').read_text())
actual = {f.relative_to(root).as_posix() for f in root.rglob('*') if f.is_file()}
assert actual == set(manifest) | {'distribution-manifest.json'}, 'Unexpected base files'
for name, digest in manifest.items():
    assert sha(root / name) == digest, name
sources = out / 'corresponding-sources'
with zipfile.ZipFile(a.base_sources) as archive:
    archive.extractall(sources)
shutil.rmtree(sources / 'sources/ftebroom')
(sources / 'sources/ftebroom').mkdir()
checkout = subprocess.check_output(['git', 'archive', 'HEAD'], cwd=src)
with tarfile.open(fileobj=io.BytesIO(checkout)) as archive:
    archive.extractall(sources / 'sources/ftebroom', filter='data')
module_name = 'fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'
shutil.copy2(a.module, root / module_name)
subprocess.run(['strip', str(root / module_name)], check=True)
shutil.copy2(src / 'runtime/fonts/FTEBroom-CJK.otf', root / 'ftebroom/fonts/FTEBroom-CJK.otf')
shutil.copytree(src / 'docs', root / 'docs', dirs_exist_ok=True)
if windows:
    shutil.rmtree(root / 'tools/audio')
    (root / 'tools/audio').mkdir()
    audio_sources = sources / 'tools/audio'
    shutil.rmtree(audio_sources, ignore_errors=True)
    audio_sources.mkdir(parents=True)
    for f in a.audio_dir.iterdir():
        if not f.is_file():
            continue
        destination = audio_sources if 'source' in f.name.lower() and f.suffix in ('.zip', '.xz', '.gz') else root / 'tools/audio'
        shutil.copy2(f, destination / f.name)
    for name in ['ffmpeg-8.0.1-source.tar.xz', 'lame-source.zip', 'ogg-source.zip', 'vorbis-source.zip']:
        assert (audio_sources / name).is_file(), name
    for name in ['FFmpeg-LGPL-2.1.txt', 'LAME-LGPL.txt', 'ogg-BSD.txt', 'vorbis-BSD.txt']:
        assert (root / 'tools/audio' / name).is_file(), name
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=src, text=True).strip()
compiler = subprocess.check_output(['g++', '--version'], text=True).splitlines()[0]
info = (f'ReyondBroom {a.version}\nPlatform: {a.platform}\nEditor revision: {revision}\n'
        f'Editor compiler: {compiler}\nModule SHA256: {sha(root / module_name)}\n'
        'Native module update; engine, CSQC, plugins and game compilers retained from the verified base runtime.\n'
        + ('Windows audio helper rebuilt from pinned sources.\n' if windows else 'System FFmpeg is used through the bundled wrapper.\n'))
(root / 'build-info.txt').write_text(info)
(sources / 'RELEASE-BUILD.txt').write_text(info + 'Build/QA: sources/ftebroom/.github/workflows/release-linux-ui-update.yml\n')
source_name = f'ReyondBroom-{a.version}-{a.platform}-Sources.zip'
(root / 'SOURCE-CODE.txt').write_text(f'Complete corresponding sources: {source_name}, in the same GitHub release.\nhttps://github.com/reyond9079/ReyondBroom/releases\nPreserve all licenses.\n')
with zipfile.ZipFile(out / source_name, 'w', zipfile.ZIP_DEFLATED) as archive:
    for f in sorted(sources.rglob('*')):
        if f.is_file():
            archive.write(f, f.relative_to(sources))
manifest = {f.relative_to(root).as_posix(): sha(f) for f in sorted(root.rglob('*')) if f.is_file() and f.name != 'distribution-manifest.json'}
(root / 'distribution-manifest.json').write_text(json.dumps(manifest, indent=2))
runtime_name = f'ReyondBroom-{a.version}-{a.platform}' + ('.zip' if windows else '.tar.gz')
if windows:
    with zipfile.ZipFile(out / runtime_name, 'w', zipfile.ZIP_DEFLATED) as archive:
        for f in sorted(root.rglob('*')):
            if f.is_file():
                archive.write(f, f.relative_to(out))
else:
    with tarfile.open(out / runtime_name, 'w:gz') as archive:
        archive.add(root, arcname=root.name)
for name in (runtime_name, source_name):
    (out / (name + '.sha256')).write_bytes((sha(out / name) + '  ' + name + '\n').encode('ascii'))
print(info)
