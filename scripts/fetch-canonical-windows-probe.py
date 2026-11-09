#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,zipfile
meta=json.loads(Path('qa/provenance/probe-artifact-metadata.json').read_text())
expected='41fdaff09372d1695aaba1d2293f6e1832c6f6cc5f4345a03815e30dc630a627'
assert meta['id']==13652832603 and meta['size_in_bytes']==7398600 and meta['digest']=='sha256:'+expected
assert meta['workflow_run']['id']==42570281480 and meta['workflow_run']['head_sha']=='4443a11fb4bd6ddb4d0c6be6a1687b0fdc2150ad'
archive=Path('canonical-probe.zip');assert archive.stat().st_size==7398600 and hashlib.sha256(archive.read_bytes()).hexdigest()==expected
with zipfile.ZipFile(archive) as z:
    names=[name for name in z.namelist() if Path(name).name=='compilation-profile-probe.exe'];assert len(names)==1
    data=z.read(names[0]);digest=hashlib.sha256(data).hexdigest();assert digest=='6276bcf8811bd2fb748af054cebc4ae681b01f17844cf12df3d28035c4db9154'
    Path('canonical-probe.exe').write_bytes(data)
    revision=[name for name in z.namelist() if Path(name).name=='compilation-editor-revision.txt'];assert len(revision)==1
    label=z.read(revision[0]);Path('qa/provenance/prepared-probe-revision.txt').write_bytes(label)
Path('qa/provenance/probe-proof.json').write_text(json.dumps({'artifact_id':13652832603,'artifact_bytes':7398600,'artifact_sha256':expected,'build_run':42570281480,'run_head':'4443a11fb4bd6ddb4d0c6be6a1687b0fdc2150ad','actual_pr_merge_label':label.decode().strip(),'probe_sha256':digest,'source_entry':names[0],'purpose':'standalone process/argv test fixture; no replacement of editor or actual game engines'},indent=2)+'\n')
print('PASS canonical authenticated probe',digest)
