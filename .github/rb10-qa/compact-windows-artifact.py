#!/usr/bin/env python3
"""Copy genuine QA evidence into a smaller artifact without asserting QA success."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil

PROOF_EXTENSIONS = {'.png', '.json', '.jsonl', '.log', '.map', '.cfg', '.txt'}
RELEASE_NAMES = (
    'ReyondBroom-0.5.0-alpha.132-Windows-x64.zip',
    'ReyondBroom-0.5.0-alpha.132-Windows-x64.zip.sha256',
    'ReyondBroom-0.5.0-alpha.132-Windows-x64-Sources.zip',
    'ReyondBroom-0.5.0-alpha.132-Windows-x64-Sources.zip.sha256',
)


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_relative(name):
    p = PurePosixPath(name)
    require(isinstance(name, str) and name and not p.is_absolute() and '\\' not in name
            and all(x not in ('', '.', '..') and ':' not in x for x in name.split('/')),
            'Unsafe artifact entry path: ' + str(name))
    return Path(*p.parts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('qa', 'restoration', 'proof', 'release', 'output'):
        parser.add_argument('--' + key, required=True, type=Path)
    a = parser.parse_args()
    output = a.output.absolute()
    require(not output.exists() and not output.is_symlink(), 'Compact destination must be fresh')
    require(not any(p.is_symlink() for p in output.parents), 'Symlink in output parent')
    for source in (a.qa, a.proof, a.release):
        source = source.absolute()
        require(source != output and output not in source.parents and source not in output.parents,
                'Output and source directories must be separate')
    files = {}
    carried = {}
    restoration = None
    if a.restoration.is_file():
        require(not a.restoration.is_symlink(), 'Restoration cannot be a symlink')
        restoration = json.loads(a.restoration.read_text())
        carried = restoration['prior_proof_entry_sha256']
        require(len(carried) == 279, 'Original restoration must describe all 279 genuine proof files')
        for name, expected in carried.items():
            rel = safe_relative(name)
            require(rel.parts[0] == 'qa' and len(rel.parts) > 1, 'Carried entry must be under qa')
            source = a.qa.joinpath(*rel.parts[1:])
            require(source.is_file() and not source.is_symlink() and digest(source) == expected,
                    'Missing or changed genuine prior proof: ' + name)
            files[name] = source
        files['restoration.json'] = a.restoration
        original_metadata = a.restoration.parent / 'prior-artifact-metadata.json'
        if original_metadata.is_file():
            require(digest(original_metadata) == restoration['prior_artifact']['metadata_sha256'],
                    'Original artifact metadata changed')
            files['proof/prior-artifact-metadata.json'] = original_metadata
    # On an early setup failure there may be no restoration. Keep available
    # diagnostics, record that limitation, and never turn it into a QA pass.
    skipped_files = 0
    skipped_bytes = 0
    if a.qa.is_dir():
        require(not a.qa.is_symlink(), 'QA input must not be a symlink')
        for source in a.qa.rglob('*'):
            if source.is_file():
                name = 'qa/' + source.relative_to(a.qa).as_posix()
                if name in carried or source.suffix.lower() in PROOF_EXTENSIONS:
                    require(not source.is_symlink(), 'Selected evidence cannot be a symlink: ' + name)
                    safe_relative(name)
                    files[name] = source
                else:
                    skipped_files += 1
                    skipped_bytes += source.stat().st_size
    if a.proof.is_dir():
        for source in a.proof.glob('*.json'):
            require(source.is_file() and not source.is_symlink(), 'Invalid proof JSON input')
            name = 'proof/' + source.name
            if name in files:
                require(digest(files[name]) == digest(source), 'Conflicting genuine proof inputs: ' + name)
            files[name] = source
    for name in RELEASE_NAMES:
        source = a.release / name
        if source.is_file():
            require(not source.is_symlink(), 'Release input cannot be a symlink')
            files['out/release/' + name] = source
    output.mkdir(parents=True)
    hashes = {}
    total_bytes = 0
    for name, source in sorted(files.items()):
        target = output / safe_relative(name)
        target.parent.mkdir(parents=True, exist_ok=True)
        require(not target.exists(), 'Duplicate compact evidence output: ' + name)
        expected = digest(source)
        shutil.copyfile(source, target)
        require(digest(target) == expected, 'Compact evidence bytes changed: ' + name)
        hashes[name] = expected
        total_bytes += target.stat().st_size
    require(all(hashes.get(name) == expected for name, expected in carried.items()),
            'All original carried files must remain byte-identical')
    summary = {
        'schema': 1, 'kind': 'compact-genuine-QA-evidence',
        'qa_completion_asserted': False, 'restoration_available': restoration is not None,
        'prior_carried_files': len(carried), 'copied_files': len(hashes),
        'copied_bytes': total_bytes, 'skipped_new_binary_files': skipped_files,
        'skipped_new_binary_bytes': skipped_bytes,
        'entry_sha256': hashes,
        'method': 'Original source evidence is unchanged. All restored prior proof entries are '
                  'retained byte-for-byte, including their probes; new runtime binary clones are '
                  'omitted. This inventory makes no claim that a QA suite completed or passed.',
    }
    summary_path = output / 'proof/compact-artifact.json'
    require(not summary_path.exists(), 'Input already uses the reserved compaction summary path')
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({k: v for k, v in summary.items() if k != 'entry_sha256'}, indent=2))


if __name__ == '__main__':
    main()
