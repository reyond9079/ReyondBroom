# SPDX-License-Identifier: GPL-3.0-or-later
"""Build and run the standalone authoring core suite without FTE or ImGui."""
import argparse, os, pathlib, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--compiler',default=os.environ.get('CXX','g++'));p.add_argument('--output',type=pathlib.Path);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
out=a.output or pathlib.Path(tempfile.mkdtemp(prefix='reyond-model-tests-'));out.mkdir(parents=True,exist_ok=True)
for name in ('geometry','io','anim','edit','paint','tools','history','export','mesh'):
    exe=(out/('test-bb-'+name+('.exe' if os.name=='nt' else ''))).resolve()
    subprocess.run([a.compiler,'-std=c++17','-O2',str(root/'tests'/('test-bb-'+name+'.cpp')),'-o',str(exe)],cwd=root,check=True)
    run=subprocess.run([str(exe)],cwd=root,capture_output=True,text=True)
    (out/(name+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
    if run.returncode:print(run.stdout+run.stderr);raise SystemExit(run.returncode)
    print(name+': PASS ('+str(run.stdout.count('PASS'))+' checks)',flush=True)
print('All authoring core suites passed; logs:',out)
