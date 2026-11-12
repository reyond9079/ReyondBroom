#!/usr/bin/env python3
"""Reject prepared editor/fixture reuse when its actual build inputs differ."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = '.github/workflows/compilation-profile-checks.yml'


def build_jobs(text):
    markers = list(re.finditer(r'^  ([a-zA-Z][a-zA-Z0-9_-]*):\s*$', text, re.M))
    jobs = {match.group(1): text[match.start():markers[i+1].start() if i+1 < len(markers) else len(text)]
            for i, match in enumerate(markers) if match.group(1) in {'windows', 'linux'}}
    assert set(jobs) == {'windows', 'linux'}, 'Missing compiled-editor build recipes'
    return jobs


def verify_recipes(previous, current):
    assert build_jobs(previous) == build_jobs(current), 'Prepared editor build recipes differ'


def verify_fixture(folder, repo=REPO):
    manifest = json.loads((Path(folder)/'provenance.json').read_text())
    assert manifest['sourceHashes'], 'Empty prepared fixture provenance'
    for name, digest in manifest['sourceHashes'].items():
        path = Path(name.replace('\\', '/'))
        assert not path.is_absolute() and '..' not in path.parts, 'Unsafe fixture provenance path'
        assert hashlib.sha256((repo/path).read_bytes()).hexdigest() == digest, 'Prepared fixture source changed: '+name


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision')
    parser.add_argument('--fixtures', type=Path, nargs='+')
    args = parser.parse_args()
    if args.revision:
        assert re.fullmatch(r'[a-fA-F0-9]{40}', args.revision), 'Invalid prepared source revision'
        subprocess.run(['git', 'diff', '--exit-code', args.revision, 'HEAD', '--', 'native', 'qc',
                        'tests/compilation-profile-probe.cpp', 'patches', '.github/actions',
                        'scripts/build-*', 'scripts/apply-engine-patches.py'], cwd=REPO, check=True)
        previous = subprocess.check_output(['git', 'show', args.revision+':'+WORKFLOW], cwd=REPO, text=True)
        verify_recipes(previous, (REPO/WORKFLOW).read_text())
    for folder in args.fixtures or []:
        verify_fixture(folder)
    assert args.revision or args.fixtures, 'Specify a prepared revision or fixture provenance'


if __name__ == '__main__':
    main()
