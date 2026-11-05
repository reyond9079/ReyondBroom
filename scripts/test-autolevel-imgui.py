"""Check AutoLevel forms against real Dear ImGui instead of a widget mock."""
from pathlib import Path
import argparse, os, subprocess

p = argparse.ArgumentParser()
p.add_argument('--imgui', type=Path, required=True, help='Pinned editor Dear ImGui source directory')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--compiler', default='g++')
a = p.parse_args()
src = Path(__file__).resolve().parents[1]
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
if Path(a.compiler).is_absolute():
    env['PATH'] = str(Path(a.compiler).parent)+os.pathsep+env.get('PATH', '')
binary = out/('autolevel-imgui.exe' if os.name == 'nt' else 'autolevel-imgui')
command = [a.compiler, '-std=c++17', '-O1', '-pthread', '-I'+str(src/'sdk'),
           '-I'+str(a.imgui.resolve()), str(src/'tests/test-autolevel-imgui.cpp')]
command += [str(a.imgui/name) for name in ['imgui.cpp','imgui_draw.cpp','imgui_tables.cpp','imgui_widgets.cpp']]
with (out/'compile.log').open('w', encoding='utf-8') as log:
    subprocess.run([*command, '-o', str(binary)], env=env, stdout=log,
                   stderr=subprocess.STDOUT, check=True)
run = subprocess.run([str(binary)], env=env, capture_output=True, text=True, timeout=60)
(out/'imgui.log').write_text(run.stdout+run.stderr, encoding='utf-8')
print(run.stdout, end='')
if run.returncode:
    print(run.stderr, end='')
    raise SystemExit(run.returncode)
