#!/usr/bin/env python3
"""Run profile regression, restart, process/filesystem and native UI probes in a disposable runtime."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
repo = Path(__file__).resolve().parents[1]
root, out = a.runtime.resolve(), a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
data = root/'ftebroom'
for name in ['run.cfg', 'trenchbroom-native.cfg', 'trenchbroom-restore.cfg', 'compilation-parity-native.cfg', 'context-navigation-native.cfg']:
    shutil.copy2(repo/'tests'/name, data/name)
shutil.copy2(repo/'tests/fixtures/CompilationProfiles-original.cfg', data/'CompilationProfiles-regression.cfg')
# QA owns this runtime; no personal profile store is used.
for name in ['compilation-profiles.json', 'ftebroom-run-profiles.txt', 'ftebroom-run-profiles.txt.legacy.bak']:
    (data/name).unlink(missing_ok=True)
engine = root/('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
renderer_args=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
results = {}
for case in ['run', 'trenchbroom-native', 'trenchbroom-restore', 'compilation-parity-native', 'context-navigation-native']:
    console = data/'qconsole.log'
    console.unlink(missing_ok=True)
    log = out/(case+'.log')
    with log.open('wb') as stream:
        process = subprocess.Popen([str(engine), *renderer_args, '-ftebroom-child-processes', '-nohome', '-noupdates', '-nosound',
            '-window', '-width', '1280', '-height', '800', '-condebug', '-basedir', str(root), '+exec', case+'.cfg'],
            cwd=root, env={**os.environ, 'LIBGL_ALWAYS_SOFTWARE': '1'}, stdout=stream, stderr=subprocess.STDOUT)
        try:
            code = process.wait(timeout=75)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            raise RuntimeError(case+' timed out')
    text = log.read_text(errors='replace')
    if console.is_file():
        text += '\n'+console.read_text(errors='replace')
        log.write_text(text)
    fails = re.findall(r'FTEBROOM_(?:RUN|PARITY|NATIVE_CAMERA)_FAIL [^\r\n]*|CSQC_Abort|Assertion .*failed', text)
    passes = sorted(set(re.findall(r'FTEBROOM_(?:RUN|PARITY|NATIVE_CAMERA)_PASS ([^\r\n]*)', text)))
    results[case] = {'exit': code, 'passed': passes, 'failed': fails}
    (out/'results.json').write_text(json.dumps(results, indent=2))
    print(case, 'exit', code, 'passed', len(passes), 'failed', fails, flush=True)
    if code or fails:
        raise RuntimeError(case+' failed; see '+str(log))
    expected = {'run': 15, 'trenchbroom-native': 3, 'trenchbroom-restore': 3, 'compilation-parity-native': 32, 'context-navigation-native': 14}[case]
    if len(passes) < expected:
        raise RuntimeError(f'{case}: missing checks ({len(passes)} < {expected})')
# Exercise migration and its second-start idempotence with actual saved legacy records.
(data/'compilation-profiles.json').unlink(missing_ok=True)
legacy = data/'ftebroom-run-profiles.txt'
legacy.write_text('variable "migration_variable" "custom value"\n'
    'profile "Custom imported" 0 ""\ntask 1 "${migration_variable}" "args" 0 2\n'
    'profile "Q1: Saved compile" 0 "${MAP_DIR_PATH}"\ntask 0 "copy.map" "" 1 0\n'
    'profile "Launch custom" 1 "${APP_DIR_PATH}"\ntask 1 "engine" "args" 1 12\n')
for stage in [70, 71]:
    (data/'qconsole.log').unlink(missing_ok=True)
    (data/'migration.cfg').write_text(f'exec qa-clip.cfg\nin 6 ftebroom_test_run {stage}\nin 8 quit force\n')
    with (out/f'migration-{stage}.log').open('wb') as stream:
        subprocess.run([str(engine), *renderer_args, '-ftebroom-child-processes', '-nohome', '-noupdates', '-nosound', '-window', '-condebug', '-basedir', str(root), '+exec', 'migration.cfg'],
            cwd=root, env={**os.environ, 'LIBGL_ALWAYS_SOFTWARE':'1'}, stdout=stream, stderr=subprocess.STDOUT, timeout=40, check=True)
    text=(out/f'migration-{stage}.log').read_text(errors='replace')
    if (data/'qconsole.log').is_file():text+='\n'+(data/'qconsole.log').read_text(errors='replace')
    (out/f'migration-{stage}.log').write_text(text)
    assert 'FTEBROOM_PARITY_FAIL' not in text and len(set(re.findall(r'FTEBROOM_PARITY_PASS ([^\r\n]*)',text)))==6, text
legacy.unlink()
# Capture the imported fixture using the actual ImGui editor and its measured geometry.
for scale in ['1', '1.5', '2']:
    (data/'compilation-profiles.json').unlink(missing_ok=True)
    shutil.rmtree(data/'compile-ui-atlas', ignore_errors=True)
    (data/'qconsole.log').unlink(missing_ok=True)
    choice={'1':0,'1.5':2,'2':4}[scale]
    (data/'profile-atlas.cfg').write_text('exec qa-clip.cfg\nin 5 ftebroom_test_run '+str(60+choice)+'\nin 6 ftebroom_test_run 10\n'+
        '\n'.join(f'in {9+3*i} ftebroom_test_run {40+i}' for i in range(4))+'\nin 23 quit force\n')
    with (out/f'atlas-{scale}.log').open('wb') as stream:
        subprocess.run([str(engine), *renderer_args, '-ftebroom-child-processes', '-nohome', '-noupdates', '-nosound',
            '-window', '-width', '2520', '-height', '1700', '-condebug', '-basedir', str(root), '+exec', 'profile-atlas.cfg'],
            cwd=root, env={**os.environ, 'LIBGL_ALWAYS_SOFTWARE': '1'}, stdout=stream, stderr=subprocess.STDOUT,
            timeout=65, check=True)
    atlas_log=(out/f'atlas-{scale}.log').read_text(errors='replace')
    if (data/'qconsole.log').is_file():atlas_log+='\n'+(data/'qconsole.log').read_text(errors='replace')
    (out/f'atlas-{scale}.log').write_text(atlas_log)
    if 'FTEBROOM_PARITY_FAIL' in atlas_log or not (data/'compile-ui-atlas').is_dir():
        raise RuntimeError('ImGui capture failed: '+atlas_log[-5000:])
    shutil.copytree(data/'compile-ui-atlas', out/'ui-atlas'/scale, dirs_exist_ok=True)
    assert len(list((out/'ui-atlas'/scale).glob('*/compile.png'))) == 4

# Assert structural layout invariants from real rendered widgets, not a mockup.
layout_checks=[]
for meta in (out/'ui-atlas').glob('*/*/geometry.json'):
    g=json.loads(meta.read_text());w=g['widgets'];u=g['scale']
    assert w['profile-remove']['x']>w['profile-add']['x']
    assert abs(w['profile-remove']['y']-w['profile-add']['y'])<u
    assert w['import']['y']+w['import']['height']<=w['profile-add']['y']+u
    assert [w[k]['x'] for k in ['task-add','task-remove','task-up','task-down']]==sorted(w[k]['x'] for k in ['task-add','task-remove','task-up','task-down'])
    footer=[w[k] for k in ['launch','stop','test','compile','close']]
    assert all(footer[i]['x']+footer[i]['width']<=footer[i+1]['x']+u for i in range(4))
    assert all(abs(v['width']-80*u)<u for v in footer)
    assert w['output']['height']>0 and w['output']['y']>=w['splitter']['y']+w['splitter']['height']-u
    assert w['close']['y']+w['close']['height']<=w['window']['height']+u
    assert abs(w['Profile name']['x']-w['Working directory']['x'])<u
    layout_checks.append(str(meta.relative_to(out)))
(out/'layout-results.json').write_text(json.dumps({'passed':layout_checks,'toolkit':'ImGui','pixel_equality_required':False},indent=2))
print('PASS',len(layout_checks),'real UI layouts',flush=True)

# Real bundled compiler, without legacy staging/preview hooks: export, compile,
# deploy the resulting BSP, then remove the compiler's portal file.
subprocess.run([sys.executable,str(repo/'tests/make-linux-build-fixture.py'),str(root)],check=True)
(data/'strict-compiler.cfg').write_text('exec ftebroom.cfg\nset ftebroom_skip_setup 1\nin 7 mapedit maps/build-fixture.map\nin 11 ftebroom_test_run 100\nin 25 ftebroom_test_run 101\nin 27 quit force\n')
(data/'qconsole.log').unlink(missing_ok=True)
with (out/'strict-compiler.log').open('wb') as stream:
    subprocess.run([str(engine),*renderer_args,'-ftebroom-child-processes','-nohome','-noupdates','-nosound','-window','-condebug','-basedir',str(root),'+exec','strict-compiler.cfg'],cwd=root,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'},stdout=stream,stderr=subprocess.STDOUT,timeout=65,check=True)
text=(out/'strict-compiler.log').read_text(errors='replace')
if (data/'qconsole.log').is_file():text+='\n'+(data/'qconsole.log').read_text(errors='replace')
(out/'strict-compiler.log').write_text(text)
shutil.copy2(data/'strict-compiler.log',out/'strict-compiler-tasks.log')
assert 'FTEBROOM_PARITY_FAIL' not in text and 'FTEBROOM_PARITY_PASS strict_export_compile_copy_cleanup' in text,text[-5000:]
print('PASS actual bundled compiler / deploy / cleanup',flush=True)
