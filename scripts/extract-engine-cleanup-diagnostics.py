#!/usr/bin/env python3
"""Select read-only diagnostics from one hash-pinned historical artifact."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('archive',type=Path)
parser.add_argument('output',type=Path)
a=parser.parse_args()
expected_sha='4c3e3652a6a9d7343fd8713868389a07c5a64cb0609e5b725e0b82653480c3bb'
expected_size=976846098
assert a.archive.stat().st_size==expected_size
sha=hashlib.sha256()
with a.archive.open('rb') as stream:
    for block in iter(lambda:stream.read(4*1024*1024),b''):sha.update(block)
assert sha.hexdigest()==expected_sha
names={'os-clicks.jsonl','last-state.json','native-input-live.json','compilation-ui-live.json','copy-cleanup-launch.png','editor.log','qconsole.log','results.json','provenance.json'}
inventory=[]
selected=[]
with zipfile.ZipFile(a.archive) as archive:
    for entry in archive.infolist():
        path=PurePosixPath(entry.filename)
        assert not path.is_absolute() and '..' not in path.parts,entry.filename
        choose=(not entry.is_dir() and 'engine-cleanup' in path.parts and path.name in names)
        record={'path':entry.filename,'bytes':entry.file_size,'compressed_bytes':entry.compress_size,'crc32':f'{entry.CRC:08x}','header_offset':entry.header_offset,'selected':choose}
        if choose:
            # These are plain JSON/log/image bytes; never import or execute them.
            assert entry.file_size<32*1024*1024,entry.filename
            data=archive.read(entry)
            record['sha256']=hashlib.sha256(data).hexdigest()
            target=a.output/'selected'/Path(*path.parts)
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(data)
            selected.append(dict(record))
        inventory.append(record)
assert selected,'No requested engine-cleanup diagnostics found'
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'original-zip-inventory.json').write_text(json.dumps(inventory,indent=2)+'\n')
proof={'original_artifact_id':13651364828,'original_run_id':42567427945,'original_job_id':128996894946,'original_head_sha':'24202b4c2542de6969bfcf6709eb68b72b9deef6','original_artifact_bytes':expected_size,'original_artifact_sha256':expected_sha,'archive_entry_count':len(inventory),'selected':selected,'method':'Downloaded original Actions artifact on an Ubuntu runner with GITHUB_TOKEN; verified exact ZIP size and SHA256 before selecting original data bytes. No original code executed; no text/image normalization.'}
(a.output/'selection-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
print(json.dumps({'verified':True,'entries':len(inventory),'selected_count':len(selected),'selected_bytes':sum(s['bytes'] for s in selected)},indent=2))
for entry in selected:print(entry['path'],entry['bytes'],entry['sha256'])
