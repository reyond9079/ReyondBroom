"""Rebuild a native editor distribution from a verified compatible base runtime.

Plugins/game compilers stay pinned to the base distribution. A rebuilt engine
and its sources can be supplied for each platform; SDL2 requires them. Current CSQC is rebuilt. Windows
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
p.add_argument('--engine', type=Path)
p.add_argument('--engine-source', type=Path)
p.add_argument('--imgui-source', type=Path)
p.add_argument('--audio-dir', type=Path)
p.add_argument('--csprogs', type=Path, required=True)
p.add_argument('--version', required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
windows = a.platform == 'Windows-x64'
if bool(a.engine) != bool(a.engine_source) or (a.platform == 'Linux-SDL2-x64' and not a.engine):
    p.error('SDL2 requires --engine and --engine-source for its window-close guard; supply both together')
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
if a.imgui_source:
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=a.imgui_source).decode().split('\0')
    with zipfile.ZipFile(sources / 'sources/imgui-source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(tracked):
            if name and (a.imgui_source / name).is_file():
                archive.write(a.imgui_source / name, name)
shutil.rmtree(sources / 'sources/ftebroom')
(sources / 'sources/ftebroom').mkdir()
checkout = subprocess.check_output(['git', 'archive', 'HEAD'], cwd=src)
with tarfile.open(fileobj=io.BytesIO(checkout)) as archive:
    archive.extractall(sources / 'sources/ftebroom', filter='data')
module_name = 'fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'
shutil.copy2(a.module, root / module_name)
shutil.copy2(a.csprogs, root/'ftebroom/csaddon.dat')
# Application defaults belong to this source revision, just like the updater.
for name in ('ftebroom.cfg', 'quake1.ent', 'update-ftebroom.py', 'update-ftebroom.ps1'):
    shutil.copy2(src/'runtime'/name, root/'ftebroom'/name)
if a.engine:
    shutil.copy2(a.engine, root/('bin/fteqw64.exe' if windows else 'fteqw-gl64'))
    if not windows:
        shutil.copy2(a.engine, root/'tools/runtimes/fteqw-preview64')
    # Use the actual patched source files; exclude Git metadata and build output.
    tracked = set(subprocess.check_output(['git', 'ls-files', '-z'], cwd=a.engine_source).decode().split('\0'))
    # Engine patches may add source files that are absent from the pinned Git
    # tree. Include these exact build inputs without sweeping in build output.
    for patch in (src/'patches').glob('*.patch'):
        tracked.update(line[6:].split('\t')[0] for line in patch.read_text(encoding='utf-8').splitlines()
                       if line.startswith('+++ b/'))
    with zipfile.ZipFile(sources/'sources/fteqw-source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(tracked):
            if name and (a.engine_source/name).is_file():
                archive.write(a.engine_source/name, name)
subprocess.run(['strip', str(root / module_name)], check=True)
shutil.copy2(src / 'runtime/fonts/FTEBroom-CJK.otf', root / 'ftebroom/fonts/FTEBroom-CJK.otf')
shutil.copytree(src / 'docs', root / 'docs', dirs_exist_ok=True)
shutil.copytree(src / 'runtime/plants', root / 'ftebroom/plants', dirs_exist_ok=True)
shutil.copytree(src / 'runtime/world-architect', root / 'ftebroom/world-architect', dirs_exist_ok=True)
shutil.copytree(src / 'runtime/glsl', root / 'ftebroom/glsl', dirs_exist_ok=True)
(root/'ftebroom/ui').mkdir(parents=True,exist_ok=True)
shutil.copy2(src/'runtime/ui/update-splash.png',root/'ftebroom/ui/update-splash.png')
shutil.copy2(src / 'runtime/scripts/ftebroom.shader', root / 'ftebroom/scripts/ftebroom.shader')
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
        + ('Engine rebuilt from pinned FTE sources with the current editor patch stack.\n' if a.engine else 'Engine retained from the verified base runtime.\n')+
        'Plugins and game compilers retained from the verified base runtime.\n'
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
