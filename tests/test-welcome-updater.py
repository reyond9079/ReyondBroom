"""Run the shipped updater protocols over a local HTTP fixture.

Only the HTTP transport destination is redirected. Check/prepare sessions, real
byte progress, hash verification, parent waiting, installation and relaunch run
through production code. Windows CI uses Windows PowerShell 5.1; --powershell
also permits a PowerShell 7 preflight on Linux.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit
import zipfile

p=argparse.ArgumentParser()
p.add_argument('--platform',choices=('Linux-x64','Linux-SDL2-x64','Windows-x64'),required=True)
p.add_argument('--powershell',default='powershell.exe')
p.add_argument('--output',type=Path,required=True)
p.add_argument('--runtime',type=Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[1]
helpers=a.runtime/'ftebroom' if a.runtime else repo/'runtime'
windows=a.platform=='Windows-x64'
engine='fteqw64.exe' if windows else 'fteqw-gl64'
module='fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'
name='ReyondBroom-0.5.0-alpha.132-'+a.platform+('.zip' if windows else '.tar.gz')
launcher=b'#!/bin/sh\nprintf restarted > "$(dirname "$0")/restarted.txt"\n'
files={engine:b'new engine',module:b'new plugin','payload.bin':os.urandom(3*1024*1024),'ftebroom/preferences.cfg':b'factory defaults'}
if not windows:files['Launch-ReyondBroom.sh']=launcher
files['distribution-manifest.json']=json.dumps({n:hashlib.sha256(d).hexdigest() for n,d in files.items()}).encode()
buffer=io.BytesIO()
if windows:
 with zipfile.ZipFile(buffer,'w') as z:
  for n,d in files.items():z.writestr('ReyondBroom-Windows-x64/'+n,d)
else:
 with tarfile.open(fileobj=buffer,mode='w:gz') as z:
  for n,d in files.items():
   f=tarfile.TarInfo('ReyondBroom-Linux-x64/'+n);f.size=len(d);f.mode=0o755 if n.endswith('.sh') else 0o644;z.addfile(f,io.BytesIO(d))
payload=buffer.getvalue();digest=hashlib.sha256(payload).hexdigest()
api='https://api.github.com/repos/reyond9079/ReyondBroom/releases'
browser='https://github.com/reyond9079/ReyondBroom/releases/download/v0.5.0-alpha.132/'
assets=[dict(name=n,size=len(d),url=api+'/assets/'+str(i),browser_download_url=browser+n,digest='sha256:'+hashlib.sha256(d).hexdigest())
        for i,n,d in ((1,name,payload),(2,name+'.sha256',(digest+'  '+name+'\n').encode()))]
release=[dict(tag_name='v0.5.0-alpha.132',draft=False,assets=assets)]
requests=[]
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  path=urlsplit(self.path).path;requests.append(path)
  data=json.dumps(release).encode() if path.endswith('/releases') else ((digest+'  '+name+'\n').encode() if path.endswith('/2') else payload)
  self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers()
  try:
   for offset in range(0,len(data),65536):
    self.wfile.write(data[offset:offset+65536]);self.wfile.flush()
    if len(data)>1024*1024:time.sleep(.025)
  except (BrokenPipeError,ConnectionResetError):pass
server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
local='http://127.0.0.1:'+str(server.server_port)
results=[]
def flag(name):return '-'+''.join(word.title() for word in name.split('-')) if windows else '--'+name

def wait_restart_probe(root,timeout_ms=5000):
 # Only our native Windows probe writes this PID into its fresh private root.
 if os.name!='nt' or not (root/'restarted.pid').exists():return
 pid=int((root/'restarted.pid').read_text(encoding='utf-8-sig').strip())
 if not 0<pid<=0xffffffff or pid==os.getpid():raise RuntimeError('Invalid owned restart-probe PID')
 import ctypes
 kernel=ctypes.WinDLL('kernel32',use_last_error=True)
 kernel.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];kernel.OpenProcess.restype=ctypes.c_void_p
 kernel.WaitForSingleObject.argtypes=[ctypes.c_void_p,ctypes.c_uint32];kernel.WaitForSingleObject.restype=ctypes.c_uint32
 kernel.CloseHandle.argtypes=[ctypes.c_void_p];kernel.CloseHandle.restype=ctypes.c_int
 handle=kernel.OpenProcess(0x00100000,False,pid) # SYNCHRONIZE only; never terminate a process.
 if not handle:
  error=ctypes.get_last_error()
  if error==87:return # ERROR_INVALID_PARAMETER: the probe has already exited.
  raise ctypes.WinError(error)
 try:
  result=kernel.WaitForSingleObject(handle,timeout_ms)
  if result==258:raise TimeoutError('Owned restart probe did not exit before fixture cleanup')
  if result==0xffffffff:raise ctypes.WinError(ctypes.get_last_error())
  if result!=0:raise RuntimeError('Unexpected restart-probe wait result: '+str(result))
 finally:kernel.CloseHandle(handle)

def run_case(case,restart=None):
 with tempfile.TemporaryDirectory(prefix='welcome updater with spaces ') as temp:
  root=Path(temp);(root/engine).write_bytes(b'old engine');(root/'ftebroom/maps').mkdir(parents=True)
  (root/'ftebroom/preferences.cfg').write_text('my bindings');(root/'ftebroom/maps/mine.map').write_text('my geometry')
  if a.platform=='Linux-SDL2-x64':(root/'linux-sdl2-build').write_text('keep SDL backend')
  if windows:
   source=(helpers/'update-ftebroom.ps1').read_text();marker='[Net.HttpWebRequest]::Create($fresh)';assert source.count(marker)==1
   source=source.replace(marker,'[Net.HttpWebRequest]::Create((\''+local+'\'+([uri]$fresh).PathAndQuery))')
   driver=root/'updater.ps1';driver.write_text(source,encoding='utf-8')
   command=[a.powershell,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(driver),'-Root',str(root),'-CurrentVersion','0.5.0-alpha.131']
   exe=root/'ReyondBroom.exe'
   if os.name=='nt':
    # A tiny real Windows executable proves Start-Process ran after installation.
    code='using System;using System.IO;class WelcomeRestartProbe {static void Main(){File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"restarted.pid"),System.Diagnostics.Process.GetCurrentProcess().Id.ToString());File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"restarted.txt"),"restarted");}}'
    compile_script=root/'launcher.ps1';compile_script.write_text("Add-Type -TypeDefinition '"+code+"' -OutputAssembly '"+str(exe).replace("'","''")+"' -OutputType ConsoleApplication\n")
    subprocess.run([a.powershell,'-NoProfile','-File',str(compile_script)],check=True,capture_output=True)
   else:
    exe.write_bytes(launcher);exe.chmod(0o755)
  else:
   driver=root/'driver.py';driver.write_text("import urllib.request,urllib.parse,runpy,sys\noriginal=urllib.request.urlopen\ndef redirect(req,*args,**kwargs):\n p=urllib.parse.urlsplit(req.full_url)\n return original(urllib.request.Request("+repr(local)+"+p.path+'?'+p.query,headers=dict(req.header_items())),*args,**kwargs)\nurllib.request.urlopen=redirect\nsys.argv=["+repr(str(helpers/'update-ftebroom.py'))+"]+sys.argv[1:]\nrunpy.run_path(sys.argv[0],run_name='__main__')\n")
   command=[sys.executable,str(driver),'--root',str(root),'--current-version','0.5.0-alpha.131']
  updates=root/'updates';updates.mkdir()
  (updates/'status.json').write_text(json.dumps(dict(session='foreign-old-session',stage='ready',available_version='99.0.0',received=1,total=1)))
  snapshot={str(f.relative_to(root)):f.read_bytes() for f in root.rglob('*') if f.is_file() and 'updates' not in f.relative_to(root).parts}
  before=len(requests);session='welcome-protocol-session'
  if case=='bad-session':session='../invalid'
  command += [flag('mode'),'check' if case in ('check-only','bad-session') else 'prepare',flag('session'),session]
  if case not in ('check-only','bad-session'):command += [flag('target-version'),'0.5.0-alpha.132']
  parent=None
  if case not in ('check-only','bad-session'):
   parent=subprocess.Popen([sys.executable,'-c','import time;time.sleep(60)'])
   command += [flag('parent-id'),str(parent.pid)]
   if restart is not None:(updates/'restart.request').write_text(session if restart else 'foreign-old-session')
  log=(a.output/(case+'.log')).open('wb');worker=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
  progress=[]
  try:
   if parent:
    deadline=time.monotonic()+45
    while time.monotonic()<deadline:
     if worker.poll() is not None:raise AssertionError('Updater exited before parent; see '+str(a.output/(case+'.log')))
     try:state=json.loads((updates/'status.json').read_text(encoding='utf-8-sig'))
     except (ValueError,OSError):time.sleep(.02);continue
     if state.get('session')==session:
      if state.get('stage')=='downloading':progress.append(state['received'])
      if state.get('stage')=='ready':break
     time.sleep(.02)
    else:raise AssertionError('prepare did not become ready')
    assert len(set(progress))>=2 and max(progress)>0,(case,progress)
    assert state['current_version']=='0.5.0-alpha.131' and state['available_version']=='0.5.0-alpha.132'
    assert state['received']==state['total']==len(payload)
    assert (root/engine).read_bytes()==b'old engine' and not (root/'restarted.txt').exists()
    time.sleep(.2);assert worker.poll() is None,'must continue waiting for the actual parent'
    parent.terminate();parent.wait(timeout=10)
   code=worker.wait(timeout=45)
   if case=='bad-session':
    assert code!=0 and len(requests)==before
   else:
    assert code==0,(case,(a.output/(case+'.log')).read_text(errors='replace'))
    state=json.loads((updates/'status.json').read_text(encoding='utf-8-sig'))
    assert state['session']==session and state['current_version']=='0.5.0-alpha.131'
    assert state['available_version']=='0.5.0-alpha.132'
    if case=='check-only':
     assert state['stage']=='available' and state['received']==0
     assert len(requests)==before+1 and requests[-1].endswith('/releases'),'check must never request package/checksum'
     assert not list(updates.glob('stage-*')) and not list(updates.glob('backup-*'))
     after={str(f.relative_to(root)):f.read_bytes() for f in root.rglob('*') if f.is_file() and 'updates' not in f.relative_to(root).parts}
     assert after==snapshot,'check-only changed the installation'
    else:
     assert state['stage']=='installed' and (root/engine).read_bytes()==b'new engine'
     assert list(updates.glob('backup-*/'+engine))
     deadline=time.monotonic()+5
     while restart and not (root/'restarted.txt').exists() and time.monotonic()<deadline:time.sleep(.02)
     assert (root/'restarted.txt').exists()==bool(restart),'restart requires an explicit matching session request'
     if restart:assert not (updates/'restart.request').exists()
     assert (root/'ftebroom/preferences.cfg').read_text()=='my bindings'
     assert (root/'ftebroom/maps/mine.map').read_text()=='my geometry'
   results.append(dict(case=case,passed=True,progress_samples=sorted(set(progress))))
   print('PASS',a.platform,case,flush=True)
  finally:
   if (updates/'status.json').exists():(a.output/(case+'-status.json')).write_bytes((updates/'status.json').read_bytes())
   if parent and parent.poll() is None:parent.kill();parent.wait()
   if worker.poll() is None:worker.kill();worker.wait()
   try:wait_restart_probe(root)
   finally:log.close()
try:
 for case,restart in (('check-only',None),('bad-session',None),('prepare-close',None),('foreign-restart',False),('explicit-restart',True)):run_case(case,restart)
finally:
 server.shutdown();server.server_close();thread.join()
 (a.output/'results.json').write_text(json.dumps(dict(platform=a.platform,cases=results),indent=2))
