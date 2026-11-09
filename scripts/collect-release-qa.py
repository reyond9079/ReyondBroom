#!/usr/bin/env python3
"""Retain readable release evidence without multi-gigabyte runtime binary clones."""
import argparse,hashlib,json,shutil
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--qa',type=Path,required=True)
p.add_argument('--release',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();assert not a.output.exists();a.output.mkdir(parents=True)
allowed={'.json','.jsonl','.png','.log','.map','.cfg','.txt','.sha256'}
entries={};total=0
for root,prefix in [(a.qa,'qa'),(a.release,'release-sidecars')]:
 if not root.exists():continue
 for source in sorted(root.rglob('*')):
  assert not source.is_symlink(),source
  if not source.is_file():continue
  if (prefix=='qa' and source.suffix.lower() not in allowed) or (prefix=='release-sidecars' and source.suffix!='.sha256'):continue
  relative=Path(prefix)/source.relative_to(root)
  target=a.output/relative;target.parent.mkdir(parents=True,exist_ok=True)
  shutil.copyfile(source,target)
  with source.open('rb') as f:expected=hashlib.file_digest(f,'sha256').hexdigest()
  with target.open('rb') as f:assert hashlib.file_digest(f,'sha256').hexdigest()==expected
  entries[relative.as_posix()]=expected;total+=target.stat().st_size
assert entries,'No release evidence was produced'
assert total<480*1024*1024,('Evidence exceeds compact artifact budget',total)
(a.output/'collection.json').write_text(json.dumps({'files':entries,'bytes':total,'method':'Readable QA outputs retained byte-for-byte; runtime binaries remain in the release archives. This inventory does not assert test success.'},indent=2)+'\n')
print('Retained',len(entries),'evidence files:',total,'bytes')
