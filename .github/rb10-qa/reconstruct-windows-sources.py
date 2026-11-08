#!/usr/bin/env python3
"""Recover complete corresponding sources for the already-built Windows runtime.

No product files are rewritten and no binary is rebuilt. ZIP container bytes may
change. Frozen product git blobs, verified base sources, exact pinned audio
archives and original runtime build-info are independently checked and recorded.
"""
import argparse, hashlib, io, json, os, re, stat, subprocess, tarfile, zipfile
from pathlib import Path, PurePosixPath

PRODUCT = '6bd6e2cedcade6e7e4d09cfe8f81901c91655a50'
BASE_WINDOWS = '958dd00d8796402e102559e639f094899db89464d6fa16b2dc8f62d97c3af2d7'
BASE_SDK = '474d4be00ee1265e1894f646a5f68fc42acfc20179fa1d0dc7a06b3a96810e3b'
MODULE = '758ccd1bc16f941683692d151334704d0dd3d32180c9458a79d1f90d8aaabe42'
CSQC = '68af71fe06750c996c257841e9b97622d4bdeb5d2e059e4a144f9da52af3e04a'
AUDIO_SCRIPT = 'e71afa388a13c9e8a3520a8ee5fbf5961930392a1961748fbca5fd3a4163024a'
FFMPEG = '05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41'
PINS = {'lame-source.zip':'af984672a95bb0eedc9f3193604e269c97cca162',
        'ogg-source.zip':'e1774cd77f471443541596e09078e78fdc342e4f',
        'vorbis-source.zip':'0657aee69dec8508a0011f47f3b69d7538e9d262'}

def digest(data): return hashlib.sha256(data).hexdigest()
def filehash(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''): h.update(block)
    return h.hexdigest()
def require(ok,message):
    if not ok: raise RuntimeError(message)
def safe(name):
    p=PurePosixPath(name)
    require(not p.is_absolute() and '..' not in p.parts and '\\' not in name,'Unsafe source archive path: '+name)
    return name

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo',type=Path,required=True);p.add_argument('--revision',required=True)
    p.add_argument('--runtime',type=Path,required=True);p.add_argument('--base-sources',type=Path,required=True)
    p.add_argument('--sdk-sources',type=Path);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--evidence',type=Path,required=True);p.add_argument('--version',default='0.5.0-alpha.132')
    a=p.parse_args();repo=a.repo.resolve();runtime=a.runtime.resolve()
    revision=subprocess.check_output(['git','rev-parse',a.revision+'^{commit}'],cwd=repo,text=True).strip()
    require(revision==PRODUCT,'Only the frozen original product revision is authorized')
    require(filehash(a.base_sources)==BASE_WINDOWS,'131 Windows Sources checksum mismatch')
    if a.sdk_sources: require(filehash(a.sdk_sources)==BASE_SDK,'131 Linux SDK Sources checksum mismatch')
    manifest=json.loads((runtime/'distribution-manifest.json').read_text(encoding='utf-8'))
    actual={f.relative_to(runtime).as_posix() for f in runtime.rglob('*') if f.is_file()}
    require(actual==set(manifest)|{'distribution-manifest.json'},'Runtime has missing/extra files; provide the pristine manifest recovery')
    for name,h in manifest.items(): require(filehash(runtime/safe(name))==h,'Runtime manifest mismatch: '+name)
    require(filehash(runtime/'fteplug_ftebroom_x64.dll')==MODULE,'Original Windows module identity mismatch')
    require(filehash(runtime/'ftebroom/csaddon.dat')==CSQC,'Original Windows CSQC identity mismatch')
    buildinfo=(runtime/'build-info.txt').read_bytes()
    text=buildinfo.decode('utf-8').replace('\r\n','\n')
    release_info=buildinfo+b'Build/QA: sources/ftebroom/.github/workflows/release-linux-ui-update.yml'+(b'\r\n' if b'\r\n' in buildinfo else b'\n')
    require('ReyondBroom '+a.version+'\n' in text and 'Platform: Windows-x64\n' in text and
            'Editor revision: '+PRODUCT+'\n' in text and 'Module SHA256: '+MODULE+'\n' in text,
            'Original runtime build-info does not identify the frozen product')
    script=subprocess.check_output(['git','show',revision+':scripts/build-audio.sh'],cwd=repo)
    require(digest(script)==AUDIO_SCRIPT,'Frozen pinned audio build script identity mismatch')
    require(FFMPEG.encode() in script and all(pin.encode() in script for pin in PINS.values()),'Audio source pins missing from original build script')
    archive=subprocess.check_output(['git','archive','--format=tar',revision],cwd=repo)
    editor={};editor_modes={}
    with tarfile.open(fileobj=io.BytesIO(archive)) as t:
        for m in t:
            if m.isdir():continue
            require(m.isfile(),'Unexpected non-regular frozen git archive member: '+m.name)
            name=safe(m.name);editor[name]=t.extractfile(m).read();editor_modes[name]=m.mode
    require(editor['scripts/build-audio.sh']==script,'Frozen git archive script differs from git show')
    report={'product_revision':revision,'runtime_manifest_files':len(manifest),
            'runtime_manifest_sha256':filehash(runtime/'distribution-manifest.json'),
            'runtime_build_info_sha256':digest(buildinfo),'runtime_build_info':text,
            'module_sha256':MODULE,'csqc_sha256':CSQC,
            'verified_131_windows_sources_sha256':BASE_WINDOWS,
            'verified_131_linux_sdk_sources_sha256':BASE_SDK if a.sdk_sources else None,
            'audio_build_script_sha256':AUDIO_SCRIPT,'audio_sources':{},'sdk_sources':{},
            'frozen_editor_file_count':len(editor),
            'frozen_editor_inventory_sha256':digest(json.dumps({k:digest(v) for k,v in sorted(editor.items())},sort_keys=True,separators=(',',':')).encode()),
            'method':'Sources reconstructed from verified 131 corresponding sources and exact 6bd6 git tree. Original runtime/build-info preserved. No binary rebuild. New Sources ZIP container hash is expected; it is not claimed to be the lost original ZIP.'}
    audio={};sdk_extra={}
    with zipfile.ZipFile(a.base_sources) as base:
        names=base.namelist();require(len(names)==len(set(names)),'Duplicate base source entries')
        require(base.read('sources/ftebroom/scripts/build-audio.sh')==script,'131 and product audio scripts differ; reuse is not proven')
        for name,pin in PINS.items():
            data=base.read('tools/audio/'+name)
            with zipfile.ZipFile(io.BytesIO(data)) as inner:
                require(inner.comment.decode('ascii').strip()==pin,'Pinned git source archive commit mismatch: '+name)
                entries=[n for n in inner.namelist() if not n.endswith('/')]
                require(entries and len(entries)==len(set(entries)),'Invalid source tree: '+name)
                tree={safe(n):digest(inner.read(n)) for n in entries}
            audio[name]=data;report['audio_sources'][name]={'git_commit':pin,'sha256':digest(data),'files':len(tree),'content_inventory_sha256':digest(json.dumps(tree,sort_keys=True,separators=(',',':')).encode())}
        name='ffmpeg-8.0.1-source.tar.xz';audio[name]=base.read('tools/audio/'+name)
        require(digest(audio[name])==FFMPEG,'Exact upstream FFmpeg 8.0.1 tarball checksum mismatch')
        report['audio_sources'][name]={'upstream_sha256':FFMPEG,'sha256':digest(audio[name]),'version':'8.0.1'}
        if a.sdk_sources:
            with zipfile.ZipFile(a.sdk_sources) as sdk:
                for library in ('bullet','fteqw','imgui'):
                    name='sources/'+library+'-source.zip';base_name=name
                    if name not in base.namelist() and library=='fteqw':base_name='sources/fteqw-sdk-source.zip'
                    b=base.read(base_name) if base_name in base.namelist() else None;s=sdk.read(name)
                    def inventory(data):
                        with zipfile.ZipFile(io.BytesIO(data)) as inner:
                            return {n:digest(inner.read(n)) for n in inner.namelist() if not n.endswith('/')}
                    same_tree=b is not None and inventory(b)==inventory(s)
                    report['sdk_sources'][name]={'base_windows_entry':base_name,'base_windows_sha256':digest(b) if b is not None else None,'sdk_sha256':digest(s),'same_archive_bytes':b==s,'same_source_file_contents':same_tree}
                    if not same_tree:sdk_extra['build-sdk/'+name]=s
        a.output.parent.mkdir(parents=True,exist_ok=True)
        require(not a.output.exists(),'Output already exists; choose a fresh output')
        temporary=a.output.with_name(a.output.name+'.tmp')
        try:
            with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED) as out:
                for item in base.infolist():
                    name=safe(item.filename)
                    if item.is_dir() or name.startswith('sources/ftebroom/') or name.startswith('tools/audio/') or name=='RELEASE-BUILD.txt':continue
                    out.writestr(item,base.read(name))
                for name,data in sorted(editor.items()):
                    info=zipfile.ZipInfo('sources/ftebroom/'+name);info.compress_type=zipfile.ZIP_DEFLATED;info.create_system=3;info.external_attr=((stat.S_IFREG|editor_modes[name])<<16)
                    out.writestr(info,data)
                for name,data in audio.items():out.writestr('tools/audio/'+name,data)
                for name,data in sdk_extra.items():out.writestr(name,data)
                out.writestr('RELEASE-BUILD.txt',release_info)
                out.writestr('SOURCE-RECOVERY-PROVENANCE.json',json.dumps(report,indent=2)+'\n')
                out.writestr('SOURCE-RECOVERY-README.txt','This corresponding-source bundle was reconstructed after a QA log-parser failure.\nThe shipped Windows runtime bytes and its original build-info remain unchanged.\nsources/ftebroom is the exact frozen product git archive, not the later QA harness.\nPinned audio source trees and unchanged build flags are verified, not rebuilt.\nAny differing actual SDK dependency archives are included separately under build-sdk/;\noriginal verified Windows engine/plugin/compiler sources remain in their base locations.\nZIP container bytes/hash differ from the unavailable original Sources ZIP.\n')
            with zipfile.ZipFile(temporary) as check:
                actual_editor={n.removeprefix('sources/ftebroom/'):digest(check.read(n)) for n in check.namelist() if n.startswith('sources/ftebroom/')}
                require(actual_editor=={n:digest(d) for n,d in editor.items()},'Reconstructed editor tree differs from frozen git')
                require(check.read('RELEASE-BUILD.txt')==release_info,'Original build-info header changed')
                for name,data in audio.items():require(check.read('tools/audio/'+name)==data,'Audio source payload changed')
                bad=check.testzip();require(bad is None,'ZIP CRC verification failed: '+str(bad))
            os.replace(temporary,a.output)
        finally:
            temporary.unlink(missing_ok=True)
    report['output']=str(a.output.resolve());report['output_sha256']=filehash(a.output);report['output_bytes']=a.output.stat().st_size
    a.output.with_name(a.output.name+'.sha256').write_bytes((report['output_sha256']+'  '+a.output.name+'\n').encode('ascii'))
    a.evidence.parent.mkdir(parents=True,exist_ok=True);a.evidence.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
