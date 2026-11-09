#!/usr/bin/env python3
"""Drive isolated diagnostics; preserve failed test exits rather than claiming QA success."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,zipfile
p=argparse.ArgumentParser();p.add_argument('mode',choices=['prepare','run-cleanup','run-viewport','collect']);p.add_argument('baseline',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
a.baseline=a.baseline.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
root=a.baseline/'ReyondBroom-Windows-x64';info=a.output/'baseline-proof.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(path,obj):path.write_text(json.dumps(obj,indent=2)+'\n')
if a.mode=='prepare':
    payloads={'ReyondBroom-0.5.0-alpha.134-Windows-x64.zip':('8bbb904a6443a5b7359d5dd2851442b3db9a27a2826e7ff4b41cc027b49ed8b4',78724981),'ReyondBroom-0.5.0-alpha.134-Windows-x64-Sources.zip':('8bccfcaa28bdd4da775a9a680b1bd518b9ce3450f2f292f9c99dd6bd41da0c9c',126680487)}
    for name,(digest,size) in payloads.items():
        file=a.baseline/name;assert file.stat().st_size==size and sha(file)==digest,name
    with zipfile.ZipFile(a.baseline/'ReyondBroom-0.5.0-alpha.134-Windows-x64.zip') as archive:archive.extractall(a.baseline)
    manifest=json.loads((root/'distribution-manifest.json').read_text());actual={x.relative_to(root).as_posix() for x in root.rglob('*') if x.is_file()}
    assert actual==set(manifest)|{'distribution-manifest.json'}
    for name,digest in manifest.items():assert sha(root/name)==digest,name
    build=(root/'build-info.txt').read_text();assert 'ReyondBroom 0.5.0-alpha.134' in build and '4443a11fb4bd6ddb4d0c6be6a1687b0fdc2150ad' in build
    engine=root/('bin/fteqw64.exe' if (root/'bin/fteqw64.exe').exists() else 'fteqw64.exe')
    tests={}
    for name in ['scripts/editor_os_input.py','scripts/test-engine-cleanup.py','scripts/test-engine-viewport.py']:
        git=subprocess.check_output(['git','show','4443a11fb4bd6ddb4d0c6be6a1687b0fdc2150ad:'+name]);current=Path(name).read_bytes();assert current==git,name
        tests[name]=hashlib.sha256(current).hexdigest()
    write(info,{'verified_original_manifest':True,'build_info':build,'payloads':payloads,'runtime':str(root),'engine_path':str(engine),'engine_sha256':sha(engine),'module_sha256':sha(root/'fteplug_ftebroom_x64.dll'),'qc_sha256':sha(root/'ftebroom/csaddon.dat'),'manifest_sha256':sha(root/'distribution-manifest.json'),'unchanged_test_sources_444':tests})
    print('PASS exact draft444 payload hashes, complete manifest and unchanged test sources')
elif a.mode.startswith('run-'):
    proof=json.loads(info.read_text());engine=Path(proof['engine_path']);rebuilt=Path('engine-diagnostic/engine/release-diagnostic/fteglqw64.exe').resolve()
    assert rebuilt.is_file()
    if a.mode=='run-cleanup':
        assert sha(engine)==proof['engine_sha256'];shutil.copy2(rebuilt,engine)
        write(a.output/'diagnostic-components.json',{'baseline_engine_sha256':proof['engine_sha256'],'instrumented_engine_sha256':sha(engine),'module_unchanged':sha(root/'fteplug_ftebroom_x64.dll')==proof['module_sha256'],'qc_unchanged':sha(root/'ftebroom/csaddon.dat')==proof['qc_sha256']})
    case='cleanup' if a.mode=='run-cleanup' else 'viewport';out=a.output/case
    assert not out.exists();log=a.output/(case+'-engine-input.log');env={**os.environ,'RB_INPUT_DIAGNOSTIC':str(log)}
    if case=='viewport':subprocess.run([sys.executable,'tests/make-linux-build-fixture.py',str(root)],check=True,env=env)
    cmd=[sys.executable,'scripts/test-engine-cleanup.py' if case=='cleanup' else 'scripts/test-engine-viewport.py',str(root),str(out),'--probe',str((a.output/'compilation-profile-probe.exe').resolve())]
    with (a.output/(case+'-driver.log')).open('wb') as stream:
        try:result=subprocess.run(cmd,stdout=stream,stderr=subprocess.STDOUT,env=env,timeout=260);exitcode=result.returncode;timedout=False
        except subprocess.TimeoutExpired:exitcode=None;timedout=True
    write(a.output/(case+'-test-exit.json'),{'command':cmd,'exit_code':exitcode,'timed_out':timedout,'test_passed':exitcode==0,'diagnostic_execution_only':True})
    print(case,'actual test exit',exitcode,'timeout',timedout)
else:
    compact=a.output/'compact';compact.mkdir(exist_ok=True)
    keep={'os-clicks.jsonl','windows-input-receipts.jsonl','last-state.json','native-input-live.json','compilation-ui-live.json','qconsole.log','editor.log','results.json','provenance.json'}
    selected=[]
    for file in list(a.output.rglob('*')):
        if not file.is_file() or compact in file.parents:continue
        relative=file.relative_to(a.output)
        if len(relative.parts)==1 or file.name in keep or file.suffix=='.png':
            if file.stat().st_size>48*1024*1024:continue
            target=compact/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,target)
            selected.append({'path':relative.as_posix(),'bytes':file.stat().st_size,'sha256':sha(file)})
    rebuilt=Path('engine-diagnostic/engine/release-diagnostic/fteglqw64.exe')
    if rebuilt.is_file():shutil.copy2(rebuilt,compact/'instrumented-fteglqw64.exe')
    write(compact/'selected-original-hashes.json',selected)
    print('Collected',len(selected),'raw diagnostic files; test-exit.json carries actual test results')
