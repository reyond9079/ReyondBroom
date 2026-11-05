"""Build the bundled SDK example independently of the editor and engine."""
from pathlib import Path
import argparse, os, shutil, subprocess
p=argparse.ArgumentParser()
p.add_argument('--output',type=Path,required=True)
p.add_argument('--compiler',default='g++')
a=p.parse_args();src=Path(__file__).resolve().parents[1]
env=os.environ.copy()
if Path(a.compiler).is_absolute():
    env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
for name in ('rubble','autolevel','ivy'):
    folder=a.output/name;folder.mkdir(parents=True,exist_ok=True)
    windows=os.name=='nt';binary=folder/(name+('.dll' if windows else '.so'))
    args=[a.compiler,'-std=c++17','-O2','-pthread','-shared','-static' if windows else '-fPIC',
          '-I'+str(src/'sdk'),str(src/'plugins'/name/(name+'.cpp')),'-o',str(binary)]
    subprocess.run(args,check=True,env=env)
    shutil.copy2(src/'plugins'/name/'plugin.ini',folder/'plugin.ini')
    for notice in (src/'plugins'/name).glob('LICENSE*.txt'):
        shutil.copy2(notice,folder/notice.name)
    print(binary)
