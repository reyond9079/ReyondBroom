"""Use the shipped transport against public alpha130 in a disposable real runtime."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True);root=a.output/'installation with spaces';shutil.copytree(a.runtime,root)
def sha(f):
 with f.open('rb') as s:return hashlib.file_digest(s,'sha256').hexdigest()
windows=os.name=='nt';module='fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so';before=sha(root/module)
data=root/'ftebroom';(data/'maps/updater-user.map').write_text('user map survives the real download')
(data/'ftebroom-preferences.cfg').write_text('user bindings survive the real download')
if windows:command=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(a.runtime/'ftebroom/update-ftebroom.ps1'),'-Root',str(root),'-CurrentVersion','0.5.0-alpha.129']
else:command=[sys.executable,str(a.runtime/'ftebroom/update-ftebroom.py'),'--root',str(root),'--current-version','0.5.0-alpha.129']
log=(a.output/'public-update.log').open('wb');worker=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
try:
 assert worker.wait(timeout=240)==0,(data, 'See '+str(a.output/'public-update.log'))
 assert (root/'build-info.txt').read_text().startswith('ReyondBroom 0.5.0-alpha.130\n')
 assert sha(root/module)==before
 assert (data/'maps/updater-user.map').read_text()=='user map survives the real download'
 assert (data/'ftebroom-preferences.cfg').read_text()=='user bindings survive the real download'
 assert list((root/'updates').glob('backup-*')) and 'installed' in (root/'updates/status.txt').read_text(encoding='utf-8-sig')
 assert (root/'linux-sdl2-build').exists()==(a.runtime/'linux-sdl2-build').exists()
 proof={'passed':True,'module_sha256':before,'platform':('Windows-x64' if windows else ('Linux-SDL2-x64' if (root/'linux-sdl2-build').exists() else 'Linux-x64')),'method':'Unauthenticated public GitHub metadata, checksum and archive; real downloaded archive installation with maps/settings and backend preserved.'}
 (a.output/'public-result.json').write_text(json.dumps(proof,indent=2));print(json.dumps(proof))
finally:
 if worker.poll() is None:worker.kill();worker.wait()
 log.close()
