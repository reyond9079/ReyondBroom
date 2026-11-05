"""Build and execute AutoLevel acceptance tests without user/game resources."""
from pathlib import Path
import argparse, concurrent.futures, json, os, subprocess, time

p = argparse.ArgumentParser()
p.add_argument('--compiler', default='g++')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--jobs', type=int, default=2)
p.add_argument('--geometry-seeds', type=int, default=30)
p.add_argument('--legacy-plugin', type=Path)
a = p.parse_args()
src = Path(__file__).resolve().parents[1]
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
if Path(a.compiler).is_absolute():
    env['PATH'] = str(Path(a.compiler).parent) + os.pathsep + env.get('PATH', '')
tests = ['math', 'storage', 'migration', 'brush', 'repo', 'solver', 'defaults', 'geometry',
         'ui', 'viewport', 'host', 'scaffold']

def compile_test(name):
    binary = out / (name + ('.exe' if os.name == 'nt' else ''))
    result = subprocess.run([a.compiler, '-std=c++17', '-O2', '-pthread',
                             '-I' + str(src/'sdk'), str(src/'tests'/f'test-autolevel-{name}.cpp'),
                             '-o', str(binary)], env=env, capture_output=True, text=True)
    (out/f'{name}-compile.log').write_text(result.stdout+result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{name} compilation failed; see {out/name}-compile.log')
    return name, binary

with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, min(a.jobs, 4))) as pool:
    binaries = dict(pool.map(compile_test, tests))
results = []
for name in tests:
    logs = []
    began = time.monotonic()
    for seed in range(1, a.geometry_seeds+1) if name == 'geometry' else [None]:
        command = [str(binaries[name])]
        if seed is not None:
            command += [str(out/'dungeon.map'), str(seed)]
        run = subprocess.run(command, env=env, capture_output=True, text=True, timeout=180)
        logs += [run.stdout, run.stderr]
        if run.returncode:
            (out/f'{name}.log').write_text(''.join(logs), encoding='utf-8')
            raise RuntimeError(f'{name} failed (seed {seed}); see {out/name}.log')
    text = ''.join(logs)
    (out/f'{name}.log').write_text(text, encoding='utf-8')
    results.append(dict(test=name, passed=True, seconds=round(time.monotonic()-began, 4),
                        cases=a.geometry_seeds if name == 'geometry' else 1,
                        pass_messages=text.count('PASS ')))
    print(f'PASS {name}: {results[-1]["seconds"]} s', flush=True)
if a.legacy_plugin:
    binary = out / ('v1-compat.exe' if os.name == 'nt' else 'v1-compat')
    subprocess.run([a.compiler, '-std=c++17', '-O2', str(src/'tests/test-plugin-v1-compat.cpp'),
                    *([] if os.name == 'nt' else ['-ldl']), '-o', str(binary)], env=env, check=True)
    run = subprocess.run([str(binary), str(a.legacy_plugin.resolve())], env=env,
                         capture_output=True, text=True, check=True)
    (out/'v1-compat.log').write_text(run.stdout+run.stderr, encoding='utf-8')
    results.append(dict(test='pre-extension binary compatibility', passed=True))
(out/'results.json').write_text(json.dumps(dict(platform=os.name, tests=results), indent=2)+'\n', encoding='utf-8')
print('PASS all AutoLevel acceptance tests', flush=True)
