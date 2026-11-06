"""Package a rebuilt editor module with a verified, unchanged base distribution.

For UI-only updates: retain the engine, CSQC and tools and their complete sources.
The module and runtime/font inputs must be built from this checkout.
"""
from pathlib import Path
import argparse, hashlib, io, json, shutil, subprocess, tarfile, zipfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--base-runtime', type=Path, required=True)
p.add_argument('--base-sources', type=Path, required=True)
p.add_argument('--module', type=Path, required=True)
p.add_argument('--version', required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
src = Path(__file__).resolve().parents[1]
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
root = out/'ReyondBroom-Linux-x64'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
with tarfile.open(a.base_runtime) as archive:
    archive.extractall(out, filter='data')
manifest = json.loads((root/'distribution-manifest.json').read_text())
actual = {f.relative_to(root).as_posix() for f in root.rglob('*') if f.is_file()}
assert actual == set(manifest) | {'distribution-manifest.json'}, 'Unexpected base files'
for name, digest in manifest.items():
    assert sha(root/name) == digest, name
sources = out/'corresponding-sources'
sources.mkdir()
with zipfile.ZipFile(a.base_sources) as archive:
    archive.extractall(sources)
shutil.rmtree(sources/'sources/ftebroom')
(sources/'sources/ftebroom').mkdir()
checkout = subprocess.check_output(['git', 'archive', 'HEAD'], cwd=src)
with tarfile.open(fileobj=io.BytesIO(checkout)) as archive:
    archive.extractall(sources/'sources/ftebroom', filter='data')
shutil.copy2(a.module, root/'fteplug_ftebroom_amd64.so')
subprocess.run(['strip', str(root/'fteplug_ftebroom_amd64.so')], check=True)
shutil.copy2(src/'runtime/fonts/FTEBroom-CJK.otf', root/'ftebroom/fonts/FTEBroom-CJK.otf')
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=src, text=True).strip()
compiler = subprocess.check_output(['g++', '--version'], text=True).splitlines()[0]
info = (f'ReyondBroom {a.version}\nBackend: native Linux x64\nEditor revision: {revision}\n'
        f'Editor compiler: {compiler}\n'
        'UI-only update: unchanged engine, CSQC, plugins and tools from the supplied base release.\n'
        'See START-LINUX.md for runtime dependencies (glibc 2.39+).\n'
        f'Module SHA256: {sha(root/"fteplug_ftebroom_amd64.so")}\n'
        f'CSQC SHA256: {sha(root/"ftebroom/csaddon.dat")}\n'
        f'Engine SHA256: {sha(root/"fteqw-gl64")}\n')
(root/'build-info.txt').write_text(info)
(sources/'RELEASE-BUILD.txt').write_text(info+
    'See sources/ftebroom/.github/workflows/release-linux-ui-update.yml for the exact build, packaging and QA steps.\n'
    'The complete original dependency sources, licenses and patches are preserved in this archive.\n')
source_name = f'ReyondBroom-{a.version}-Linux-x64-Sources.zip'
(root/'SOURCE-CODE.txt').write_text(
    f'Corresponding sources: {source_name}, available in the same GitHub release.\n'
    'https://github.com/reyond9079/ReyondBroom/releases\nPreserve all licenses.\n')
with zipfile.ZipFile(out/source_name, 'w', zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(sources.rglob('*')):
        if file.is_file(): archive.write(file, file.relative_to(sources))
manifest = {f.relative_to(root).as_posix(): sha(f) for f in sorted(root.rglob('*'))
            if f.is_file() and f.name != 'distribution-manifest.json'}
(root/'distribution-manifest.json').write_text(json.dumps(manifest, indent=2))
runtime_name = f'ReyondBroom-{a.version}-Linux-x64.tar.gz'
with tarfile.open(out/runtime_name, 'w:gz') as archive:
    archive.add(root, arcname=root.name)
for name in (runtime_name, source_name):
    (out/(name+'.sha256')).write_text(sha(out/name)+'  '+name+'\n')
print(info)
