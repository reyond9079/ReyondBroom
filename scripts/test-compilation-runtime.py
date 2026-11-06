#!/usr/bin/env python3
"""Run profile regression, restart, process/filesystem and native UI probes in a disposable runtime."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
repo = Path(__file__).resolve().parents[1]
root, out = a.runtime.resolve(), a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
data = root/'ftebroom'
for name in ['run.cfg', 'trenchbroom-native.cfg', 'trenchbroom-restore.cfg', 'compilation-parity-native.cfg']:
    shutil.copy2(repo/'tests'/name, data/name)
shutil.copy2(repo/'tests/fixtures/CompilationProfiles-cpu-count.cfg', data/'CompilationProfiles-regression.cfg')
# QA owns this runtime; no personal profile store is used.
for name in ['compilation-profiles.json', 'ftebroom-run-profiles.txt']:
    (data/name).unlink(missing_ok=True)
engine = root/('bin/fteqw64.exe' if os.name == 'nt' else 'fteqw-gl64')
results = {}
for case in ['run', 'trenchbroom-native', 'trenchbroom-restore', 'compilation-parity-native']:
    console = data/'qconsole.log'
    console.unlink(missing_ok=True)
    log = out/(case+'.log')
    with log.open('wb') as stream:
        process = subprocess.Popen([str(engine), '-ftebroom-child-processes', '-nohome', '-noupdates', '-nosound',
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
    fails = re.findall(r'FTEBROOM_(?:RUN|PARITY)_FAIL [^\r\n]*|CSQC_Abort|Assertion .*failed', text)
    passes = sorted(set(re.findall(r'FTEBROOM_(?:RUN|PARITY)_PASS ([^\r\n]*)', text)))
    results[case] = {'exit': code, 'passed': passes, 'failed': fails}
    (out/'results.json').write_text(json.dumps(results, indent=2))
    print(case, 'exit', code, 'passed', len(passes), 'failed', fails, flush=True)
    if code or fails:
        raise RuntimeError(case+' failed; see '+str(log))
    expected = {'run': 15, 'trenchbroom-native': 3, 'trenchbroom-restore': 3, 'compilation-parity-native': 23}[case]
    if len(passes) < expected:
        raise RuntimeError(f'{case}: missing checks ({len(passes)} < {expected})')
# Capture the imported original fixture, with actual native widgets and DIP geometry.
(data/'compilation-profiles.json').unlink(missing_ok=True)
(data/'profile-atlas.cfg').write_text('exec qa-clip.cfg\nin 10 ftebroom_test_run 10\n'+
    '\n'.join(f'in {13+3*i} ftebroom_test_run {40+i}' for i in range(4))+'\nin 27 quit force\n')
with (out/'atlas.log').open('wb') as stream:
    subprocess.run([str(engine), '-ftebroom-child-processes', '-nohome', '-noupdates', '-nosound',
        '-window', '-width', '1280', '-height', '800', '-basedir', str(root), '+exec', 'profile-atlas.cfg'],
        cwd=root, env={**os.environ, 'LIBGL_ALWAYS_SOFTWARE': '1'}, stdout=stream, stderr=subprocess.STDOUT,
        timeout=65, check=True)
atlas_log=(out/'atlas.log').read_text(errors='replace')
if 'FTEBROOM_PARITY_FAIL' in atlas_log or not (data/'compile-ui-atlas').is_dir():
    raise RuntimeError('Native Qt capture failed: '+atlas_log[-5000:])
shutil.copytree(data/'compile-ui-atlas', out/'ui-atlas', dirs_exist_ok=True)
assert len(list((out/'ui-atlas').glob('*/compile.png'))) == 4
