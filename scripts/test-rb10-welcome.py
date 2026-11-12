#!/usr/bin/env python3
"""Real OS-input Welcome/update acceptance in disposable copies of a runtime.

The updater is the shipped script with only its HTTP destination redirected to
an in-process fixture. It stages a hash-verified package, waits for the actual
editor, and installs only into the disposable copy. The restart launcher is a
probe executable, so the original runtime and user files cannot be modified.
"""
import argparse,hashlib,io,json,os,re,shutil,subprocess,tarfile,threading,time,zipfile
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from PIL import ImageGrab
from editor_os_input import NativeInput,read_ui_json

p=argparse.ArgumentParser(description=__doc__);p.add_argument('runtime',type=Path);p.add_argument('output',type=Path)
p.add_argument('--width',type=int,default=1280);p.add_argument('--height',type=int,default=800);p.add_argument('--powershell',default='powershell.exe')
a=p.parse_args();original,out=a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
assert original!=out and original not in out.parents,'Keep QA output outside the source runtime'
windows=os.name=='nt';engine_name='bin/fteqw64.exe' if windows and (original/'bin/fteqw64.exe').exists() else 'fteqw64.exe' if windows else 'fteqw-gl64'
module_name='fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'
current=(original/'build-info.txt').read_text().splitlines()[0].split()[1]
m=re.fullmatch(r'(\d+\.\d+\.\d+)-alpha\.(\d+)',current);assert m,'QA fixture expects a versioned alpha runtime'
latest=m[1]+'-alpha.'+str(int(m[2])+1)
platform='Windows-x64' if windows else 'Linux-SDL2-x64' if (original/'linux-sdl2-build').exists() else 'Linux-x64'
name='ReyondBroom-'+latest+'-'+platform+('.zip' if windows else '.tar.gz')
protected=[engine_name,module_name,'ftebroom/update-ftebroom.ps1' if windows else 'ftebroom/update-ftebroom.py']
original_hashes={n:hashlib.sha256((original/n).read_bytes()).hexdigest() for n in protected}
launcher=b'#!/bin/sh\nprintf restarted > "$(dirname "$0")/restarted.txt"\n'
if windows:
 exe=out/'restart-probe.exe';code='using System;using System.IO;class WelcomeProbe {static void Main(){File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"restarted.txt"),"restarted");}}'
 exe.unlink(missing_ok=True)
 ps=out/'build-probe.ps1';ps.write_text("Add-Type -TypeDefinition '"+code+"' -OutputAssembly '"+str(exe).replace("'","''")+"' -OutputType ConsoleApplication\n")
 subprocess.run([a.powershell,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(ps)],check=True,capture_output=True);launcher=exe.read_bytes()
files={'fteqw64.exe' if windows else 'fteqw-gl64':b'installed fixture engine',module_name:b'installed fixture module',
       'ReyondBroom.exe' if windows else 'Launch-ReyondBroom.sh':launcher,'payload.bin':os.urandom(8*1024*1024)}
files['distribution-manifest.json']=json.dumps({n:hashlib.sha256(d).hexdigest() for n,d in files.items()}).encode()
buffer=io.BytesIO()
if windows:
 with zipfile.ZipFile(buffer,'w') as z:
  for n,d in files.items():z.writestr('ReyondBroom-Windows-x64/'+n,d)
else:
 with tarfile.open(fileobj=buffer,mode='w:gz') as z:
  for n,d in files.items():
   f=tarfile.TarInfo('ReyondBroom-Linux-x64/'+n);f.size=len(d);f.mode=0o755 if n.endswith('.sh') else 0o644;z.addfile(f,io.BytesIO(d))
payload=buffer.getvalue();digest=hashlib.sha256(payload).hexdigest();api='https://api.github.com/repos/reyond9079/ReyondBroom/releases';browser='https://github.com/reyond9079/ReyondBroom/releases/download/v'+latest+'/'
assets=[dict(name=n,size=len(d),url=api+'/assets/'+str(i),browser_download_url=browser+n,digest='sha256:'+hashlib.sha256(d).hexdigest()) for i,n,d in ((1,name,payload),(2,name+'.sha256',(digest+'  '+name+'\n').encode()))]
release=[dict(tag_name='v'+latest,draft=False,assets=assets)];requests=[]
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  path=urlsplit(self.path).path;requests.append(path)
  data=json.dumps(release).encode() if path.endswith('/releases') else ((digest+'  '+name+'\n').encode() if path.endswith('/2') else payload)
  self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers()
  try:
   for offset in range(0,len(data),65536):
    self.wfile.write(data[offset:offset+65536]);self.wfile.flush()
    if len(data)>1024*1024:time.sleep(.04)
  except (BrokenPipeError,ConnectionResetError):pass
server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start();local='http://127.0.0.1:'+str(server.server_port)
checks=[];process=None;stream=None

def check(ok,label):
 assert ok,label
 checks.append(label);(out/'results.json').write_text(json.dumps(checks,indent=2));print('PASS',label,flush=True)
def live():return read_ui_json(state)
def wait(fn,timeout=75):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  if process.poll() is not None:raise RuntimeError('Editor exited; see '+str(caseout/'editor.log'))
  try:
   if value:=fn():return value
  except (FileNotFoundError,PermissionError,KeyError,json.JSONDecodeError):pass
  time.sleep(.06)
 raise AssertionError('Welcome QA timeout: '+(state.read_text()[:1400] if state.exists() else 'no state'))
def click(key,global_=False,final=False):
 ui.settle();r=wait(lambda:live()['globalWidgets' if global_ else 'widgets'].get(key))
 with (caseout/'os-clicks.jsonl').open('a') as f:f.write(json.dumps(dict(key=key,rect=r,time=time.monotonic()))+'\n')
 try:ui.click(r['x']+r['width']*.45,r['y']+r['height']*.5)
 except (AssertionError,RuntimeError):
  if not final or process.poll() is None:raise

def shot(name):
 ui.settle();ImageGrab.grab().save(caseout/(name+'.png'));(caseout/(name+'.json')).write_text(json.dumps(live(),indent=2))
def start(label):
 global process,stream,ui,root,data,state,caseout
 caseout=out/label;caseout.mkdir(exist_ok=True);root=caseout/'editor-runtime'
 if root.exists():shutil.rmtree(root)
 shutil.copytree(original,root,ignore=shutil.ignore_patterns('updates','downloads','__pycache__'))
 # Keep the fixture independent of previously configured external game paths.
 (root/'ftebroom-game-profile.txt').write_text('0\n')
 data=root/'ftebroom';state=data/'welcome-ui-live.json';state.unlink(missing_ok=True)
 script=data/('update-ftebroom.ps1' if windows else 'update-ftebroom.py');source=script.read_text()
 if windows:
  marker='[Net.HttpWebRequest]::Create($fresh)';assert source.count(marker)==1
  source=source.replace(marker,'[Net.HttpWebRequest]::Create((\''+local+'\'+([uri]$fresh).PathAndQuery))')
 else:
  marker='urllib.request.urlopen(urllib.request.Request(fresh, headers=headers), timeout=60)';assert source.count(marker)==1
  source=source.replace(marker,'urllib.request.urlopen(urllib.request.Request('+repr(local)+'+urllib.parse.urlsplit(fresh).path+"?"+urllib.parse.urlsplit(fresh).query, headers=headers), timeout=60)')
 script.write_text(source,encoding='utf-8')
 (data/'autosave').mkdir(exist_ok=True);source_map=data/'maps/welcome132-original.map'
 sample=next(f for f in (data/'maps/start.map',data/'maps/start-quake1.map') if f.is_file())
 source_map.write_bytes(sample.read_bytes());backup=data/'autosave/welcome132.map';backup.write_bytes(source_map.read_bytes());Path(str(backup)+'.source').write_text(str(source_map)+'\n')
 (data/'welcome132.cfg').write_text('exec ftebroom.cfg\nset ftebroom_welcome_qa 1\nset ftebroom_input_qa 1\nin 7 mapedit maps/'+sample.name+'\nin 12 ftebroom_test_desktop welcome132begin\n')
 render=['+set','vid_renderer',os.environ['RB_QA_RENDERER']] if os.environ.get('RB_QA_RENDERER') else []
 stream=(caseout/'editor.log').open('wb')
 process=subprocess.Popen([str(root/engine_name),*render,'+set','ftebroom_skip_setup','1','+set','ftebroom_welcome_qa','1','+set','ftebroom_input_qa','1','-ftebroom-child-processes','-nohome','-noupdates','-nosound','-condebug','-window','-width',str(a.width),'-height',str(a.height),'-basedir',str(root),'+exec','welcome132.cfg'],cwd=root,stdout=stream,stderr=subprocess.STDOUT,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
 wait(lambda:live()['stage']=='available' and live()['windowOpen'] and live()['widgets'].get('update/start'))
 ui=NativeInput(process,state,live,wait,caseout,focus_required=True);ui.position()
 if not windows:ui.window.set_input_focus(ui.X.RevertToParent,ui.X.CurrentTime);ui.d.sync()
 # Wait for fixture setup to finish before issuing its first console command.
 wait(lambda:(data/'qconsole.log').exists() and 'welcome132_no_legacy_recovery_modal' in (data/'qconsole.log').read_text(errors='replace'))
 ui.settle()
def stop():
 if process and process.poll() is None:process.kill();process.wait(timeout=15)
 if stream:stream.close()
def wait_installed(restart):
 process.wait(timeout=25)
 deadline=time.monotonic()+30
 while time.monotonic()<deadline:
  try:
   status=json.loads((root/'updates/status.json').read_text(encoding='utf-8-sig'))
   if status['stage']=='installed' and ((root/'restarted.txt').exists() or not restart):break
  except (OSError,ValueError):pass
  time.sleep(.1)
 else:raise AssertionError('Updater did not finish after real editor exit')
 check((root/('fteqw64.exe' if windows else 'fteqw-gl64')).read_bytes()==b'installed fixture engine','verified package installed after '+('Restart' if restart else 'closing committed Welcome'))
 check((root/'restarted.txt').exists()==restart,'only the Restart button relaunches the editor')
 (caseout/'installed-status.json').write_text(json.dumps(status,indent=2))

try:
 start('restart')
 check(live()['currentVersion']==current and live()['availableVersion']==latest,'welcome clearly reports current and available versions')
 ui.console('ftebroom_test_desktop welcome132protocol');wait(lambda:'welcome132_verified_ready_status' in (data/'qconsole.log').read_text(errors='replace'))
 check('FTEBROOM_DESKTOP_FAIL welcome132' not in (data/'qconsole.log').read_text(errors='replace'),'malformed, stale and foreign updater records cannot change welcome state')
 shot('01-update-offer');click('update/skip');wait(lambda:live()['welcomeStage']=='recovery' and 'recovery/keep-editing' in live()['widgets'])
 check(not live()['legacyRecoveryPopup'] and 'recent/another-map' not in live()['widgets'],'recovery uses one Welcome window before recent maps');shot('02-recovery')
 click('recovery/keep-editing');wait(lambda:live()['welcomeStage']=='recent' and 'recent/another-map' in live()['widgets']);shot('03-recent-maps')
 click('menu/File',True);click('Menu/File/Open...',True);wait(lambda:live()['windowOpen'] and live()['welcomeStage']=='recent')
 check(not live()['mapFilesOpen'],'File > Open defaults to recently edited maps')
 click('recent/another-map');wait(lambda:live()['mapFilesOpen'] and 'files/cancel' in live()['widgets'])
 check(not live()['windowOpen'],'Open another map replaces Welcome with the map browser');shot('04-map-browser');click('files/cancel');wait(lambda:not live()['mapFilesOpen'])
 ui.console('ftebroom_test_desktop welcome132no-update');wait(lambda:live()['welcomeStage']=='recovery' and 'recovery/recover' in live()['widgets'])
 check(not live()['legacyRecoveryPopup'],'no-update result proceeds directly to recovery without a parallel popup')
 click('recovery/recover');wait(lambda:not live()['windowOpen'] and not live()['recoveryOpening'])
 check(live()['document']=='','Recover opens the autosave as an unsaved document')
 ui.console('ftebroom_test_desktop welcome132updates');wait(lambda:live()['stage']=='available' and 'update/start' in live()['widgets'])
 r=live()['widgets']['welcome/window'];ui.click(r['x']+r['width']-14,r['y']+14)
 wait(lambda:not live()['windowOpen'])
 click('menu/Help',True);click('Menu/Help/Updates...',True)
 wait(lambda:live()['windowOpen'] and live()['welcomeStage']=='updates' and 'update/start' in live()['widgets'])
 check('update/skip' not in live()['widgets'],'Help > Updates opens a focused manual update window')
 click('update/start');wait(lambda:live()['committed'] and live()['stage']=='downloading')
 progress=[];deadline=time.monotonic()+35
 while time.monotonic()<deadline:
  value=live();progress.append(value['received'])
  if value['stage']=='ready':break
  time.sleep(.12)
 else:raise AssertionError('Update did not become ready')
 check(len(set(progress))>1 and max(progress)>0,'Update and continue displays actual advancing download bytes')
 check(live()['committed'] and live()['received']==live()['total'] and 'update/skip' not in live()['widgets'],'verified update requires Restart and provides no skip control')
 click('menu/File',True);click('Menu/File/Open...',True)
 check(live()['welcomeStage']=='updates' and not live()['mapFilesOpen'],'File > Open cannot bypass a committed update')
 (caseout/'progress.json').write_text(json.dumps(progress));shot('05-restart-required')
 blocked=root/'updates'/('restart-'+live()['session']+'.tmp');blocked.mkdir()
 click('update/restart');wait(lambda:'error/ok' in live()['widgets'])
 check(process.poll() is None and live()['windowOpen'] and not (root/'updates/restart.request').exists(),'failed restart request keeps the editor open and leaves no armed token')
 click('error/ok')
 ui.console('ftebroom_box 256 0 64 64 64 64');wait(lambda:live()['dirty'])
 click('update/restart');wait(lambda:live()['saveChangesOpen'] and 'save/discard' in live()['widgets'])
 check(not (root/'updates/restart.request').exists(),'Restart leaves no token before the dirty map exit is confirmed')
 click('save/discard',final=True);wait_installed(True);stop()
 start('close');click('update/start');wait(lambda:live()['committed'] and live()['stage']=='ready')
 ui.console('ftebroom_box 256 0 64 64 64 64');wait(lambda:live()['dirty'])
 click('update/restart');wait(lambda:live()['saveChangesOpen'] and 'save/cancel' in live()['widgets'])
 check(not (root/'updates/restart.request').exists(),'a deferred Restart cannot arm relaunch while Save changes is pending')
 click('save/cancel');wait(lambda:not live()['saveChangesOpen'])
 check(not (root/'updates/restart.request').exists() and live()['pendingDocument']=='' and live()['windowOpen'] and live()['committed'],'Cancel preserves the mandatory Welcome update and leaves no restart intent')
 shot('06-cancelled-restart');r=live()['widgets']['welcome/window']
 try:ui.click(r['x']+r['width']-14,r['y']+14)
 except (AssertionError,RuntimeError):
  if process.poll() is None:raise
 wait(lambda:live()['saveChangesOpen'] and 'save/discard' in live()['widgets'])
 check(not (root/'updates/restart.request').exists(),'ordinary Welcome close after Cancel does not arm relaunch')
 click('save/discard',final=True)
 wait_installed(False);stop()
 check({n:hashlib.sha256((original/n).read_bytes()).hexdigest() for n in protected}==original_hashes,'all installs and GUI actions leave the supplied runtime unchanged')
except BaseException:
 try:ImageGrab.grab().save(caseout/'failure.png')
 except Exception:pass
 try:(caseout/'failure-state.json').write_text(json.dumps(live(),indent=2))
 except Exception:pass
 raise
finally:
 stop();server.shutdown();server.server_close();thread.join()
 (out/'requests.json').write_text(json.dumps(requests,indent=2))
 (out/'provenance.json').write_text(json.dumps(dict(build_info=(original/'build-info.txt').read_text(),module_sha256=original_hashes[module_name],method='Real OS input; unmodified updater logic with HTTP-only redirection; installs into disposable runtime copies',window=[a.width,a.height],platform=platform),indent=2))
