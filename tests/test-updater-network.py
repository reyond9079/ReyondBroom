"""Exercise the real updater against HTTP failures and real archive installation.

Only the HTTP destination is redirected to a local fixture; production retry,
route selection, hashing, staging, parent waiting and installation run unchanged.
Windows CI uses the same Windows PowerShell 5.1 executable as the editor.
"""
from pathlib import Path
import argparse, collections, hashlib, io, json, os, shutil, subprocess, sys
import tarfile, tempfile, threading, time, zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

p=argparse.ArgumentParser();p.add_argument('--platform',choices=['Windows-x64','Linux-x64','Linux-SDL2-x64'],required=True)
p.add_argument('--powershell',default='powershell.exe');p.add_argument('--output',type=Path,required=True)
p.add_argument('--scenario',default='')
p.add_argument('--runtime',type=Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);repo=Path(__file__).resolve().parents[1];helpers=a.runtime/'ftebroom' if a.runtime else repo/'runtime'
windows=a.platform=='Windows-x64';name='ReyondBroom-0.5.0-alpha.131-'+a.platform+('.zip' if windows else '.tar.gz')
engine='fteqw64.exe' if windows else 'fteqw-gl64';module='fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'
files={engine:b'new engine',module:b'new module','Launch-ReyondBroom.sh':b'launcher','ftebroom/preferences.cfg':b'new defaults'}
files['distribution-manifest.json']=json.dumps({n:hashlib.sha256(d).hexdigest() for n,d in files.items()}).encode()
buffer=io.BytesIO();prefix='ReyondBroom-Windows-x64/' if windows else 'ReyondBroom-Linux-x64/'
if windows:
 with zipfile.ZipFile(buffer,'w') as z:
  for n,d in files.items():z.writestr(prefix+n,d)
else:
 with tarfile.open(fileobj=buffer,mode='w:gz') as z:
  for n,d in files.items():
   f=tarfile.TarInfo(prefix+n);f.size=len(d);f.mode=0o755 if n.endswith('.sh') else 0o644;z.addfile(f,io.BytesIO(d))
payload=buffer.getvalue();digest=hashlib.sha256(payload).hexdigest();api='https://api.github.com/repos/reyond9079/ReyondBroom/releases'
browser='https://github.com/reyond9079/ReyondBroom/releases/download/v0.5.0-alpha.131/'
assets=[{'id':i,'name':n,'size':len(d),'url':api+'/assets/'+str(i),'browser_download_url':browser+n,'digest':'sha256:'+hashlib.sha256(d).hexdigest()}
        for i,n,d in [(1,name,payload),(2,name+'.sha256',(digest+'  '+name+'\n').encode())]]
release=[{'tag_name':'v0.5.0-alpha.131','draft':False,'assets':assets}]
checks=[]
for scenario in ['api-primary-metadata-504','api-504-browser-fallback','truncated-transfer','checksum-mismatch']:
 if a.scenario and scenario!=a.scenario:continue
 counts=collections.Counter();requests=[]
 class Handler(BaseHTTPRequestHandler):
  def log_message(self,*args):pass
  def do_GET(self):
   from urllib.parse import urlsplit
   path=urlsplit(self.path).path;kind='metadata' if path.endswith('/releases') else ('api' if '/assets/' in path else 'browser')
   checksum=path.endswith('/2') or path.endswith('.sha256');key=kind+('-sum' if checksum else '-payload')
   counts[key]+=1;requests.append({'route':key,'accept':self.headers.get('Accept'),'fresh_query':'_rb_update=' in self.path})
   if kind=='metadata':
    if scenario=='api-primary-metadata-504' and counts[key]==1:self.send_error(504);return
    data=json.dumps(release).encode()
   else:
    if kind=='api' and self.headers.get('Accept')!='application/octet-stream':self.send_error(406);return
    if (scenario=='api-primary-metadata-504' and kind=='browser') or (scenario=='api-504-browser-fallback' and kind=='api'):
     self.send_error(504);return
    data=(digest+'  '+name+'\n').encode() if checksum else payload
    if scenario=='checksum-mismatch' and not checksum:data=bytes([data[0]^1])+data[1:]
   self.send_response(200);self.send_header('Content-Length',str(len(data)));self.send_header('Content-Type','application/octet-stream' if kind!='metadata' else 'application/json');self.end_headers()
   if scenario=='truncated-transfer' and kind=='api' and not checksum and counts[key]==1:
    self.wfile.write(data[:len(data)//2]);self.close_connection=True;return
   self.wfile.write(data)
 server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
 with tempfile.TemporaryDirectory(prefix='updater network with spaces ') as temp:
  root=Path(temp);(root/engine).write_bytes(b'old engine');(root/'ftebroom/maps').mkdir(parents=True)
  (root/'ftebroom/preferences.cfg').write_text('my bindings');(root/'ftebroom/maps/mine.map').write_text('my geometry')
  if a.platform=='Linux-SDL2-x64':(root/'linux-sdl2-build').write_text('retain backend')
  local='http://127.0.0.1:'+str(server.server_port)
  if windows:
   source=(helpers/'update-ftebroom.ps1').read_text();marker='[Net.HttpWebRequest]::Create($fresh)';assert source.count(marker)==1
   source=source.replace(marker,'[Net.HttpWebRequest]::Create((\''+local+'\'+([uri]$fresh).PathAndQuery))')
   driver=root/'updater.ps1';driver.write_text(source,encoding='utf-8')
   command=[a.powershell,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(driver),'-Root',str(root),'-CurrentVersion','0.5.0-alpha.130']
  else:
   driver=root/'driver.py';driver.write_text("import urllib.request,urllib.parse,runpy,sys\noriginal=urllib.request.urlopen\ndef redirect(req,*args,**kwargs):\n p=urllib.parse.urlsplit(req.full_url)\n mapped=urllib.request.Request("+repr(local)+"+p.path+'?'+p.query,headers=dict(req.header_items()))\n return original(mapped,*args,**kwargs)\nurllib.request.urlopen=redirect\nsys.argv=["+repr(str(helpers/'update-ftebroom.py'))+"]+sys.argv[1:]\nrunpy.run_path(sys.argv[0],run_name='__main__')\n")
   command=[sys.executable,str(driver),'--root',str(root),'--current-version','0.5.0-alpha.130']
  parent=subprocess.Popen([sys.executable,'-c','import time;time.sleep(60)']) if scenario=='api-primary-metadata-504' else None
  if parent:command+=['-ParentId' if windows else '--parent-id',str(parent.pid)]
  log=(a.output/(scenario+'.log')).open('wb');worker=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
  try:
   if parent:
    deadline=time.monotonic()+40
    while time.monotonic()<deadline:
     if worker.poll() is not None:raise AssertionError('Updater exited before parent wait; inspect '+str(a.output/(scenario+'.log')))
     status=root/'updates/status.txt'
     if status.exists() and ('downloaded' in status.read_text(encoding='utf-8-sig').lower()):break
     time.sleep(.1)
    else:raise AssertionError('Download/parent wait timed out')
    assert (root/engine).read_bytes()==b'old engine';parent.terminate();parent.wait(timeout=10)
   code=worker.wait(timeout=60)
   if scenario=='checksum-mismatch':
    assert code!=0 and (root/engine).read_bytes()==b'old engine';assert not list((root/'updates').glob('stage-*'))
    assert counts['api-payload']==1 and not counts['browser-payload']
   else:
    assert code==0,(scenario,(a.output/(scenario+'.log')).read_text(errors='replace'))
    assert (root/engine).read_bytes()==b'new engine';assert list((root/'updates').glob('backup-*/'+engine))
   assert (root/'ftebroom/preferences.cfg').read_text()=='my bindings';assert (root/'ftebroom/maps/mine.map').read_text()=='my geometry'
   assert not list((root/'updates').glob('*.part'))
   if scenario=='api-primary-metadata-504':assert counts['metadata-payload']==2 and not counts['browser-sum'] and not counts['browser-payload']
   if scenario=='api-504-browser-fallback':assert counts['api-sum']==3 and counts['api-payload']==3 and counts['browser-sum']==1 and counts['browser-payload']==1
   if scenario=='truncated-transfer':assert counts['api-payload']==2
   assert all(r['fresh_query'] for r in requests)
   checks.append({'scenario':scenario,'passed':True,'requests':dict(counts)})
   print('PASS',a.platform,scenario,dict(counts),flush=True)
  finally:
   status=root/'updates/status.txt'
   if status.exists():(a.output/(scenario+'-status.txt')).write_bytes(status.read_bytes())
   (a.output/(scenario+'-requests.json')).write_text(json.dumps(requests,indent=2))
   if parent and parent.poll() is None:parent.kill();parent.wait()
   if worker.poll() is None:worker.kill();worker.wait()
   log.close();server.shutdown();server.server_close();thread.join()
(a.output/'results.json').write_text(json.dumps({'platform':a.platform,'cases':checks},indent=2))
