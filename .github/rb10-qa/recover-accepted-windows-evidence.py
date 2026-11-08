#!/usr/bin/env python3
"""Recover every genuine compact acceptance artifact entry without rerunning QA."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import zipfile

HEAD = '877b0a6e89d9f8412b45b0ffc9fa2d60f9846c5c'
RUN = 42510451845
ARTIFACT = 13630111106
SHA = 'a1442892a49cb19525ca3a23a01b47edf7d74d2f61546e29bd4adb25997176b6'
SIZE = 314234025


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('metadata', 'archive', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    a = parser.parse_args()
    meta = json.loads(a.metadata.read_text())
    require(meta['id'] == ARTIFACT and meta['name'] == 'qa-Windows-x64' and
            meta['workflow_run']['id'] == RUN and meta['workflow_run']['head_sha'] == HEAD and
            meta['size_in_bytes'] == SIZE and meta['digest'] == 'sha256:' + SHA,
            'Wrong immutable passed-suite artifact metadata')
    require(a.archive.stat().st_size == SIZE and digest(a.archive) == SHA,
            'Passed-suite artifact does not match pinned GitHub bytes')
    output = a.output.absolute()
    require(not output.exists() and not output.is_symlink(), 'Recovery output must be fresh')
    require(not any(p.is_symlink() for p in output.parents), 'Symlink in recovery output parent')
    hashes = {}
    with zipfile.ZipFile(a.archive) as z:
        entries = z.infolist()
        require(len(entries) == 2504, 'Expected every original compact artifact entry')
        seen = set()
        for item in entries:
            path = PurePosixPath(item.filename)
            require(not path.is_absolute() and '\\' not in item.filename and
                    all(p not in ('', '.', '..') and ':' not in p
                        for p in item.filename.split('/')), 'Unsafe compact artifact entry')
            require(item.filename.casefold() not in seen, 'Duplicate or case-colliding entry')
            seen.add(item.filename.casefold())
            mode = item.external_attr >> 16
            require(stat.S_IFMT(mode) in (0, stat.S_IFREG) and not item.flag_bits & 1,
                    'Symlink, special or encrypted compact artifact entry')
        output.mkdir(parents=True)
        for item in entries:
            target = output.joinpath(*PurePosixPath(item.filename).parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            content = z.read(item)
            expected = hashlib.sha256(content).hexdigest()
            with target.open('xb') as stream:
                stream.write(content)
            os.chmod(target, ((item.external_attr >> 16) & 0o777) or 0o644)
            require(digest(target) == expected, 'Recovered evidence bytes changed')
            hashes[item.filename] = expected
    report = {'schema': 1, 'kind': 'byte-identical-acceptance-evidence-recovery',
              'source_test_run': RUN, 'source_test_head': HEAD, 'source_test_artifact_id': ARTIFACT,
              'source_test_artifact_sha256': SHA, 'source_test_artifact_size': SIZE,
              'source_test_metadata_sha256': digest(a.metadata), 'original_entries': len(hashes),
              'entry_sha256': hashes, 'acceptance_suites_rerun': False,
              'metadata_rewritten': False}
    proof = output / 'proof'
    proof.mkdir(exist_ok=True)
    receipt = proof / 'recovery-extraction.json'
    require(not receipt.exists(), 'Recovery receipt collides with original evidence')
    receipt.write_text(json.dumps(report, indent=2) + '\n')
    copied_meta = proof / 'source-test-artifact-metadata.json'
    require(not copied_meta.exists(), 'Source metadata collides with original evidence')
    shutil.copyfile(a.metadata, copied_meta)
    require(digest(copied_meta) == report['source_test_metadata_sha256'], 'Source metadata changed')
    print(json.dumps({k: v for k, v in report.items() if k != 'entry_sha256'}, indent=2))


if __name__ == '__main__':
    main()
