#!/usr/bin/env python3
"""Restore a verified original Windows payload and retain separate genuine QA proofs.

This does not reconstruct or claim byte identity with the original release ZIP.
Only the original distribution manifest controls which runtime files are restored.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import zipfile

PREFIX = 'qa/rb10/runtime/tutorial132/'
HEAD = '6bd6e2cedcade6e7e4d09cfe8f81901c91655a50'
RUN = 42498112722
ARTIFACT = 13626282101
DIGEST = 'c30cfa600a7f9223bca901bd9b084526dc4fe686d86cc57a67053504e8441313'
VERSION = '0.5.0-alpha.132'
AUDIO_CHECKS = (
    'pk3_file_manager_double_click', 'pk3_sound_in_media_and_timeline',
    'sound_is_inner_window', 'import_queued_while_decoding',
    'long_pk3_and_compressed_audio_imported', 'import_reveals_selected_media',
    'pk3_import_preserves_archive', 'inner_window_can_move', 'inner_window_can_resize',
)


def require(value, message):
    if not value:
        raise ValueError(message)


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_path(name):
    require(isinstance(name, str) and name and '\\' not in name, 'Unsafe ZIP path')
    path = PurePosixPath(name)
    parts = name.rstrip('/').split('/')
    require(not path.is_absolute() and all(p not in ('', '.', '..') for p in parts),
            'Non-canonical or absolute ZIP path: ' + name)
    reserved = {'con', 'prn', 'aux', 'nul', *('com' + str(i) for i in range(1, 10)),
                *('lpt' + str(i) for i in range(1, 10))}
    require(all(not any(ord(c) < 32 or c in '<>:"|?*' for c in p)
                and p.rstrip(' .') == p and p.split('.')[0].casefold() not in reserved
                for p in parts), 'Unsafe Windows ZIP path: ' + name)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--metadata', required=True, type=Path)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--head', default=HEAD)
    parser.add_argument('--run', default=RUN, type=int)
    parser.add_argument('--artifact-id', default=ARTIFACT, type=int)
    parser.add_argument('--sha256', default=DIGEST)
    a = parser.parse_args()
    # The defaults are also the immutable audit pin, not a way to accept another build.
    require((a.head, a.run, a.artifact_id, a.sha256) == (HEAD, RUN, ARTIFACT, DIGEST),
            'This restoration is pinned to the inspected original 6bd6 Windows artifact')
    archive = a.archive.resolve(strict=True)
    metadata = a.metadata.resolve(strict=True)
    meta_bytes = metadata.read_bytes()
    meta = json.loads(meta_bytes)
    require(meta['id'] == a.artifact_id and meta['name'] == 'qa-Windows-x64',
            'Wrong artifact metadata identity')
    require(meta['workflow_run']['id'] == a.run and
            meta['workflow_run']['head_sha'] == a.head, 'Wrong source head or workflow run')
    require(meta['digest'] == 'sha256:' + a.sha256 and
            archive.stat().st_size == meta['size_in_bytes'], 'Artifact size/digest metadata mismatch')
    require(sha256(archive) == a.sha256, 'Artifact bytes do not match the pinned SHA256')
    # Do not resolve the output itself: an existing symlink is an existing destination too.
    output = Path(os.path.abspath(a.output))
    require(not output.exists() and not output.is_symlink(), 'Output must be fresh/nonexistent')
    require(not any(p.is_symlink() for p in output.parents), 'Output parent must not be a symlink')
    with zipfile.ZipFile(archive) as z:
        entries = {}
        folded = set()
        expanded = 0
        for item in z.infolist():
            safe_path(item.filename)
            key = item.filename.rstrip('/').casefold()
            require(key not in folded, 'Duplicate/case-colliding ZIP entry: ' + item.filename)
            folded.add(key)
            mode = item.external_attr >> 16
            require(stat.S_IFMT(mode) in (0, stat.S_IFREG, stat.S_IFDIR),
                    'Symlink or special ZIP entry: ' + item.filename)
            require(not item.flag_bits & 1, 'Encrypted ZIP entries are unsupported')
            expanded += item.file_size
            if not item.is_dir():
                entries[item.filename] = item
        require(len(entries) < 50000 and expanded < 4 * 1024**3, 'Unexpected ZIP expansion')
        manifest_name = PREFIX + 'distribution-manifest.json'
        manifest_bytes = z.read(entries[manifest_name])
        manifest = json.loads(manifest_bytes)
        require(isinstance(manifest, dict) and len(manifest) == 653,
                'Expected all 653 original distribution manifest entries')
        runtime_hashes = {}
        for name, expected in manifest.items():
            safe_path(name)
            require(re.fullmatch('[0-9a-f]{64}', expected) and
                    name != 'distribution-manifest.json', 'Invalid manifest entry: ' + name)
            actual = hashlib.sha256(z.read(entries[PREFIX + name])).hexdigest()
            require(actual == expected, 'Original runtime file hash mismatch: ' + name)
            runtime_hashes[name] = expected
        runtime_hashes['distribution-manifest.json'] = hashlib.sha256(manifest_bytes).hexdigest()
        info = z.read(entries[PREFIX + 'build-info.txt']).decode('utf-8').replace('\r\n', '\n')
        module = manifest['fteplug_ftebroom_x64.dll']
        for line in ('ReyondBroom ' + VERSION, 'Platform: Windows-x64',
                     'Editor revision: ' + a.head, 'Module SHA256: ' + module):
            require(line in info.splitlines(), 'Build-info provenance mismatch: ' + line)

        def obj(name):
            return json.loads(z.read(entries[name]))

        def passed_source(name):
            source = obj(name)
            require(source['module_sha256'] == module and
                    source['build_info'].replace('\r\n', '\n') == info,
                    'Prior QA provenance does not match restored payload: ' + name)

        audio = z.read(entries['qa/audio-archive/combined.log']).decode('utf-8', errors='replace')
        require(not any(x in audio for x in ('DESKTOP_FAIL', 'CSQC_Abort', 'Assertion failed')),
                'Prior archive sound suite contains a failure')
        for check in AUDIO_CHECKS:
            require('DESKTOP_PASS audio4_' + check in audio, 'Missing prior audio check: ' + check)
        for image in ('audio-archive-initial.png', 'audio-archive-window.png'):
            require(z.read(entries['qa/audio-archive/' + image]).startswith(b'\x89PNG\r\n\x1a\n'),
                    'Missing genuine audio screenshot: ' + image)
        groups = ['qa/audio-archive/', 'qa/engine-viewport/', 'qa/rb10/units/']
        for theme in ('dark', 'light'):
            for width in (1024, 1920):
                appearance = f'qa/appearance-{theme}-{width}/'
                create = f'qa/viewport-create-{theme}-{width}/'
                for folder, expected in ((appearance, 21), (create, 40)):
                    checks = obj(folder + 'results.json')
                    require(isinstance(checks, list) and len(checks) == len(set(checks)) == expected,
                            'Incomplete prior passed suite: ' + folder)
                    groups.append(folder)
                passed_source(appearance + 'provenance.json')
        engine_checks = obj('qa/engine-viewport/results.json')
        require(len(engine_checks) == len(set(engine_checks)) == 16,
                'Incomplete prior engine/viewport suite')
        units = obj('qa/rb10/units/plant-targets/results.json')
        require(units['targetCases'] == len(units['targets']) == 242,
                'Incomplete original plant target units')
        for log in ('browser', 'preview', 'tools-layout', 'shortcuts', 'q2-pak', 'workspace-selection'):
            require('PASS' in z.read(entries['qa/rb10/units/' + log + '.log']).decode(),
                    'Missing original passed unit log: ' + log)
        require('qa/rb10/units/q2-roots.log' in entries, 'Missing original Q2 roots unit log')
        proof_names = sorted(n for n in entries if any(n.startswith(p) for p in groups))
        require(all(not n.startswith('qa/rb10/runtime/') for n in proof_names),
                'Partial runtime proof must never be aggregated')
        output.mkdir(parents=True)

        def extract(name, destination):
            item = entries[name]
            destination.parent.mkdir(parents=True, exist_ok=True)
            content = z.read(item)
            with destination.open('xb') as stream:
                stream.write(content)
            os.chmod(destination, ((item.external_attr >> 16) & 0o777) or 0o644)
            return hashlib.sha256(content).hexdigest()

        for name, expected in runtime_hashes.items():
            target = output / 'runtime' / name
            require(extract(PREFIX + name, target) == expected and sha256(target) == expected,
                    'Restored payload verification failed: ' + name)
        proof_hashes = {n: extract(n, output / n) for n in proof_names}
        shutil.copyfile(metadata, output / 'prior-artifact-metadata.json')
        require((output / 'prior-artifact-metadata.json').read_bytes() == meta_bytes,
                'Metadata must be preserved byte-for-byte')
        require(not (output / 'qa/rb10/runtime').exists(), 'Old partial runtime unexpectedly copied')
        require(not (output / 'qa/rb10/updater').exists(), 'Old updater must be freshly rerun')
        report = {
            'schema': 1, 'kind': 'verified-original-Windows-payload-restoration',
            'head': a.head, 'version': VERSION, 'platform': 'Windows-x64',
            'prior_artifact': {'id': a.artifact_id, 'run': a.run, 'sha256': a.sha256,
                               'size_in_bytes': meta['size_in_bytes'], 'archive': str(archive),
                               'metadata_sha256': hashlib.sha256(meta_bytes).hexdigest()},
            'runtime_archive_prefix': PREFIX, 'manifest_entries': len(manifest),
            'runtime_file_sha256': runtime_hashes,
            'module_sha256': module, 'engine_sha256': manifest['bin/fteqw64.exe'],
            'qc_sha256': manifest['ftebroom/csaddon.dat'],
            'build_info_sha256': manifest['build-info.txt'],
            'distribution_manifest_sha256': runtime_hashes['distribution-manifest.json'],
            'prior_proof_groups': groups, 'prior_proof_entry_sha256': proof_hashes,
            'excluded_prior_namespaces': ['qa/rb10/runtime/', 'qa/rb10/updater/'],
            'prior_archive_unchanged': sha256(archive) == a.sha256,
            'original_release_zip_recreated': False,
            'continuation_required': 'Run every remaining RB-10 suite on a separate runtime clone; '
                                     'retain original and continuation artifact metadata separately.',
        }
        (output / 'restoration.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({k: v for k, v in report.items()
                          if k not in ('runtime_file_sha256', 'prior_proof_entry_sha256')}, indent=2))


if __name__ == '__main__':
    main()
