import hashlib,io,json,os,pathlib,shutil,subprocess,urllib.request,urllib.error,zipfile
out=pathlib.Path('diagnostic');out.mkdir(exist_ok=True)
head='4443a11fb4bd6ddb4d0c6be6a1687b0fdc2150ad'
repo=os.environ['GITHUB_REPOSITORY'];token=os.environ['GH_TOKEN']
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
base=pathlib.Path('downloads');base.mkdir(exist_ok=True)
runtime=base/'ReyondBroom-0.5.0-alpha.134-Windows-x64.zip';sources=base/'ReyondBroom-0.5.0-alpha.134-Windows-x64-Sources.zip'
assert sha(runtime)=='8bbb904a6443a5b7359d5dd2851442b3db9a27a2826e7ff4b41cc027b49ed8b4'
assert sha(sources)=='8bccfcaa28bdd4da775a9a680b1bd518b9ce3450f2f292f9c99dd6bd41da0c9c'
with zipfile.ZipFile(runtime) as z:z.extractall('bundle')
root=pathlib.Path('bundle/ReyondBroom-Windows-x64')
manifest=json.loads((root/'distribution-manifest.json').read_text());actual={p.relative_to(root).as_posix():sha(p) for p in root.rglob('*') if p.is_file() and p.name!='distribution-manifest.json'}
assert actual==manifest,'Runtime manifest mismatch'
info=(root/'build-info.txt').read_text();assert head in info,info
subprocess.run(['git','archive',head,'-o','git-product.zip'],check=True)
with zipfile.ZipFile(sources) as z,zipfile.ZipFile('git-product.zip') as g:
 expected={p.filename:hashlib.sha256(g.read(p)).hexdigest() for p in g.infolist() if not p.is_dir()}
 received={p.filename[len('sources/ftebroom/'):]:hashlib.sha256(z.read(p)).hexdigest() for p in z.infolist() if not p.is_dir() and p.filename.startswith('sources/ftebroom/')}
 assert received==expected,'Corresponding product source mismatch'
 assert z.read('RELEASE-BUILD.txt').decode().replace('\r\n','\n').startswith(info.replace('\r\n','\n'))
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args,**kwargs):return None
api=f'https://api.github.com/repos/{repo}/actions/artifacts/13652833158/zip'
try:
 try:r=urllib.request.build_opener(NoRedirect).open(urllib.request.Request(api,headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json'}),timeout=30)
 except urllib.error.HTTPError as e:r=e
 assert r.status==302
 location=r.headers['Location'];r.close()
 with urllib.request.urlopen(location,timeout=120) as response,(base/'source-qa.zip').open('wb') as output:shutil.copyfileobj(response,output)
except Exception as error:raise RuntimeError('Authenticated artifact download failed: '+type(error).__name__) from None
assert (base/'source-qa.zip').stat().st_size==976844089
assert sha(base/'source-qa.zip')=='d03437686ed388a581e572383ab2f32ad0cd327346f334d36e8b2b4b8b9b2c2f'
probe=out/'authentic-probe.exe';overlays={}
with zipfile.ZipFile(base/'source-qa.zip') as z:
 probe.write_bytes(z.read('qa/rb10/engine-cleanup/Engine Bin/engine-argv-probe.exe'))
 assert sha(probe)=='6276bcf8811bd2fb748af054cebc4ae681b01f17844cf12df3d28035c4db9154'
 prefix='qa/rb10/engine-cleanup/editor-runtime/bin/'
 for p in z.infolist():
  if p.filename.startswith(prefix) and p.filename.endswith('.dll'):
   name=p.filename[len(prefix):];assert '/' not in name
   target=root/'bin'/name;overlays[name]={'before':sha(target) if target.exists() else None,'after':hashlib.sha256(z.read(p)).hexdigest()};target.write_bytes(z.read(p))
assert 'rb_qa_opengl.dll' in overlays
components=['fteplug_ftebroom_x64.dll','ftebroom/csaddon.dat','bin/fteqw64.exe']
assert all(sha(root/name)==actual[name] for name in components),'Product component changed'
(out/'provenance.json').write_text(json.dumps({'product_head':head,'runtime_sha256':sha(runtime),'sources_sha256':sha(sources),'source_files':len(expected),'manifest_files':len(actual),'components':{name:sha(root/name) for name in components},'probe_sha256':sha(probe),'probe_artifact':13652833158,'renderer_diagnostic_overlays':overlays,'method':'Authentic staged runtime and complete source archives verified before renderer-only overlay. No product binary or assertion patched.'},indent=2))
(out/'diagnostic-source.diff').write_bytes(subprocess.check_output(['git','diff',head,'HEAD','--','scripts','.github/workflows']))
print('Verified authentic staged product, full source/manifest and original process probe',len(actual),len(expected))
