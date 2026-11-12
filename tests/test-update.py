from pathlib import Path
from ctypes import WinDLL, get_last_error
from ctypes.wintypes import BOOL, DWORD, HANDLE, LPCWSTR, LPVOID
import tempfile,zipfile,subprocess,json,hashlib
root=Path(tempfile.mkdtemp(prefix='ReyondBroom-update-test-'))
(root/'ftebroom').mkdir();(root/'fteqw64.exe').write_bytes(b'old engine');(root/'ftebroom/appearance.cfg').write_text('4 1 1');(root/'ftebroom/maps').mkdir();(root/'ftebroom/maps/mine.map').write_text('user map');(root/'ftebroom-game-profile.txt').write_text('my mod path')
(root/'ftebroom/custom.wad').write_bytes(b'user-added texture')
(root/'ftebroom/defaults.cfg').write_bytes(b'old defaults')
game_defaults={'reyondbroom-legacy-games.txt':b'"" "" "" "" "" 0\n',
 'ftebroom/reyondbroom-legacy-games.txt':b'"" "" "" "" "" 0\n',
 'ftebroom/reyondbroom-quake2-build-options.txt':b'1 1 1 "" "" ""\n',
 'ftebroom/reyondbroom-halflife-build-options.txt':b'1 1 1 "" "" "" ""\n',
 'ftebroom/compilation-profiles.json':b'{"version":1,"games":{}}\n'}
q2=root/'Quake II custom'/'rogue мод';hl=root/'Half-Life custom'/'cstrike'
q2defs=q2/'mapping/custom quake2.fgd';hldefs=hl/'mapping/custom halflife.fgd';wad=hl/'textures/custom.wad'
for path in (q2defs,hldefs,wad):path.parent.mkdir(parents=True,exist_ok=True)
for path in (q2defs,hldefs):path.write_bytes(b'@PointClass = info_custom : "User entity" []\n')
wad.write_bytes(b'WAD3\0\0\0\0\x0c\0\0\0')
fields=[q2.as_posix(),hl.as_posix(),wad.as_posix(),hldefs.as_posix(),q2defs.as_posix()]
workspace={'version':1,'games':{'quake2':{'gameRoot':q2.parent.as_posix(),'baseMod':'baseq2',
 'enabledMods':[q2.name],'modsConfigured':True,'profiles':[],'engines':[],'variables':{'q2bsp':'custom q2bsp'}}}}
games={'reyondbroom-legacy-games.txt':(' '.join(json.dumps(value,ensure_ascii=False) for value in fields)+' 1\r\n').encode('utf-8'),
 'ftebroom/reyondbroom-legacy-games.txt':(' '.join(json.dumps(value,ensure_ascii=False) for value in fields)+' 1\r\n').encode('utf-8'),
 'ftebroom/reyondbroom-quake2-build-options.txt':b'1 0 1 "-q2bsp -threads 3" "-fast" "-bounce 2"\r\n',
 'ftebroom/reyondbroom-halflife-build-options.txt':b'1 1 1 "-cliptype precise" "-full" "-extra" "-texdata 16384"\r\n',
 'ftebroom/compilation-profiles.json':(json.dumps(workspace,ensure_ascii=False,indent=2)+'\r\n').encode('utf-8')}
for name,value in games.items():(root/name).write_bytes(value)
(root/'distribution-manifest.json').write_text(json.dumps({
 'ftebroom/custom.wad':hashlib.sha256(b'original wad').hexdigest(),
 'ftebroom/defaults.cfg':hashlib.sha256(b'old defaults').hexdigest(),
 **{name:hashlib.sha256(value).hexdigest() for name,value in games.items()}}))
updates=root/'updates';updates.mkdir()
for name in ('stage-old','backup-old'):
 (updates/name).mkdir();(updates/name/'cache').write_text('old')
(updates/'ReyondBroom-0.5.0-alpha.20-Windows-x64.zip.part').write_text('partial')
(updates/'notes.txt').write_text('keep')
package=root/'fixture.zip'
plugin=root/'plugins/autolevel/autolevel.dll'
plugin.parent.mkdir(parents=True)
plugin.write_bytes(b'unchanged plugin')
legacy_tmp=plugin.with_name(plugin.name+'.ftebroom-update.tmp')
legacy_tmp.write_bytes(b'old failed update residue')
with zipfile.ZipFile(package,'w') as z:
 for name,data in [('fteqw64.exe',b'new engine'),('fteplug_ftebroom_x64.dll',b'new plugin'),('ftebroom/appearance.cfg',b'0 0 0'),('ftebroom-game-profile.txt',b'wrong'),('ftebroom/new.txt',b'new file')]:z.writestr('ReyondBroom-Windows-x64/'+name,data)
 z.writestr('ReyondBroom-Windows-x64/ftebroom/custom.wad',b'new stock wad')
 z.writestr('ReyondBroom-Windows-x64/ftebroom/defaults.cfg',b'new defaults')
 for name,value in game_defaults.items():z.writestr('ReyondBroom-Windows-x64/'+name,value)
 z.writestr('ReyondBroom-Windows-x64/plugins/autolevel/autolevel.dll',plugin.read_bytes())
 z.writestr('ReyondBroom-Windows-x64/ftebroom/world-architect/registry.json','{}')
kernel32=WinDLL('kernel32',use_last_error=True)
kernel32.CreateFileW.argtypes=[LPCWSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE]
kernel32.CreateFileW.restype=HANDLE
kernel32.CloseHandle.argtypes=[HANDLE]
kernel32.CloseHandle.restype=BOOL
# Permit reading the loaded plugin, while denying write and delete sharing.
handle=kernel32.CreateFileW(str(plugin),0x80000000,1,None,3,0,None)
assert handle not in (None,HANDLE(-1).value),get_last_error()
try:
 r=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File','runtime/update-ftebroom.ps1','-Root',str(root),'-CurrentVersion','0.5.0-alpha.29','-LocalPackage',str(package)],capture_output=True,text=True)
finally:
 kernel32.CloseHandle(handle)
print(r.returncode,r.stdout,r.stderr);print((root/'updates/status.txt').read_text())
assert r.returncode==0
assert not (updates/'stage-old').exists() and not (updates/'backup-old').exists()
assert not (updates/'ReyondBroom-0.5.0-alpha.20-Windows-x64.zip.part').exists()
assert (updates/'notes.txt').read_text()=='keep' and package.exists()
assert len(list(updates.glob('backup-*')))==1 and not list(updates.glob('stage-*'))
assert (root/'fteqw64.exe').read_bytes()==b'new engine'
assert plugin.read_bytes()==b'unchanged plugin'
assert legacy_tmp.read_bytes()==b'old failed update residue'
assert (root/'ftebroom/world-architect/registry.json').read_text()=='{}'
assert (root/'ftebroom/appearance.cfg').read_text()=='4 1 1'
assert (root/'ftebroom-game-profile.txt').read_text()=='my mod path'
assert (root/'ftebroom/maps/mine.map').read_text()=='user map'
assert (root/'ftebroom/custom.wad').read_bytes()==b'user-added texture'
assert (root/'ftebroom/defaults.cfg').read_bytes()==b'new defaults'
for name,value in games.items():assert (root/name).read_bytes()==value,name
assert list((root/'updates').glob('backup-*/fteqw64.exe'))[0].read_bytes()==b'old engine'
print('PASS: update installed; appearance, profile, custom Q2/HL metadata, build/compile settings, user map and rollback preserved',root)
# Existing user files are protected; a fresh installation still receives defaults.
fresh=Path(tempfile.mkdtemp(prefix='ReyondBroom-update-fresh-game-settings-'));(fresh/'fteqw64.exe').write_bytes(b'old engine')
r=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File','runtime/update-ftebroom.ps1','-Root',str(fresh),'-CurrentVersion','0.5.0-alpha.29','-LocalPackage',str(package)],capture_output=True,text=True)
assert r.returncode==0,(r.stdout,r.stderr)
for name,value in game_defaults.items():assert (fresh/name).read_bytes()==value,name
print('PASS: absent Q2/HL settings receive packaged defaults',fresh)
# Reject traversal before touching the live installation.
malicious=root/'unsafe.zip'
with zipfile.ZipFile(malicious,'w') as z:z.writestr('ReyondBroom-Windows-x64/../escape.txt','bad')
r=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File','runtime/update-ftebroom.ps1','-Root',str(root),'-CurrentVersion','0.5.0-alpha.29','-LocalPackage',str(malicious)],capture_output=True,text=True)
assert r.returncode==1 and not (root/'escape.txt').exists()
assert (root/'fteqw64.exe').read_bytes()==b'new engine'
# Force a failure after normal files have been staged, then verify rollback.
(root/'blocked').write_text('keep')
failed=root/'failed.zip'
with zipfile.ZipFile(failed,'w') as z:
 for name,data in [('fteqw64.exe','should roll back'),('fteplug_ftebroom_x64.dll','plugin'),('blocked/child.txt','cannot install')]:z.writestr('ReyondBroom-Windows-x64/'+name,data)
r=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File','runtime/update-ftebroom.ps1','-Root',str(root),'-CurrentVersion','0.5.0-alpha.29','-LocalPackage',str(failed)],capture_output=True,text=True)
assert r.returncode==1 and (root/'fteqw64.exe').read_bytes()==b'new engine'
assert (root/'blocked').read_text()=='keep'
print('PASS: traversal rejection and failed-install rollback')
