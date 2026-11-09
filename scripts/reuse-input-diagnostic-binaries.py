#!/usr/bin/env python3
"""Reuse only hash-authenticated engine/probe bytes from prior trusted builds."""
from pathlib import Path
import hashlib,json,shutil,zipfile
out=Path('diagnostics');out.mkdir(exist_ok=True)
artifacts=[('engine',13654118230,4029100,'e318d80764e563d114e33f072ca5cd753d6bec2d2139cb34fbcdc2a3506b3ffa',42573529300,'5b8fd0f95c9bc4beca852c8ce8c94df278182a99'),('probe',13652832603,7398600,'41fdaff09372d1695aaba1d2293f6e1832c6f6cc5f4345a03815e30dc630a627',42570281480,'4443a11fb4bd6ddb4d0c6be6a1687b0fdc2150ad')]
proof={}
for kind,id,size,digest,run,head in artifacts:
    meta=json.loads((out/(kind+'-artifact-metadata.json')).read_text());archive=Path('reused-'+kind+'.zip')
    assert meta['id']==id and meta['size_in_bytes']==size and meta['digest']=='sha256:'+digest
    assert meta['workflow_run']['id']==run and meta['workflow_run']['head_sha']==head
    assert archive.stat().st_size==size and hashlib.sha256(archive.read_bytes()).hexdigest()==digest
    proof[kind]={'artifact_id':id,'artifact_bytes':size,'artifact_sha256':digest,'build_run':run,'build_head':head}
    with zipfile.ZipFile(archive) as z:
        if kind=='engine':
            for name in ['engine-build.log','engine-source-diff.patch','instrumentation-hashes.json','instrumentation-header.h','instrumented-fteglqw64.exe']:(out/name).write_bytes(z.read(name))
            exe=out/'instrumented-fteglqw64.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='7c6fa083d6664c2ffe0f65d38f0e7e0878d86605d9cb5ba93a4a9b204d5df693'
            target=Path('engine-diagnostic/engine/release-diagnostic/fteglqw64.exe');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(exe,target)
            proof[kind]['binary_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest()
        else:
            names=[x for x in z.namelist() if Path(x).name=='compilation-profile-probe.exe'];assert len(names)==1
            data=z.read(names[0]);(out/'compilation-profile-probe.exe').write_bytes(data)
            revisions=[x for x in z.namelist() if Path(x).name=='compilation-editor-revision.txt'];assert len(revisions)==1
            assert z.read(revisions[0]).decode().strip()==head
            proof[kind]['binary_sha256']=hashlib.sha256(data).hexdigest();proof[kind]['entry']=names[0]
(out/'reused-binary-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
print('PASS exact authenticated logger engine and canonical source444 probe',json.dumps({k:v['binary_sha256'] for k,v in proof.items()}))
