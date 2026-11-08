#!/usr/bin/env python3
"""Bind genuine prior proofs, fresh QA suites and the unchanged original payload."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

HEAD = '6bd6e2cedcade6e7e4d09cfe8f81901c91655a50'
PRIOR_ID = 13626282101
PRIOR_RUN = 42498112722
PRIOR_SHA = 'c30cfa600a7f9223bca901bd9b084526dc4fe686d86cc57a67053504e8441313'
MANIFEST_SHA = '2c5a137f56d21e82913c8afa7d29d0e0ddb1a481120bc9807188d5075c4e858d'
MODULE_SHA = '758ccd1bc16f941683692d151334704d0dd3d32180c9458a79d1f90d8aaabe42'
QC_SHA = '68af71fe06750c996c257841e9b97622d4bdeb5d2e059e4a144f9da52af3e04a'


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def relative(name):
    p = PurePosixPath(name)
    require(isinstance(name, str) and name and not p.is_absolute() and '\\' not in name
            and all(x not in ('', '.', '..') and ':' not in x for x in name.split('/')),
            'Unsafe recorded entry path: ' + str(name))
    return Path(*p.parts)


def validate_source_evidence(value):
    expected = {'product_revision': HEAD, 'csqc_sha256': QC_SHA,
                'runtime_manifest_files': 653, 'runtime_manifest_sha256': MANIFEST_SHA,
                'module_sha256': MODULE_SHA, 'frozen_editor_file_count': 1766}
    for key, wanted in expected.items():
        require(value.get(key) == wanted, 'Sources evidence mismatch: ' + key)
    require(isinstance(value.get('output_sha256'), str) and
            re.fullmatch('[0-9a-f]{64}', value['output_sha256']),
            'Sources evidence must retain the actual reconstructed ZIP digest')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('restoration', 'qa', 'runtime', 'source-evidence', 'output'):
        p.add_argument('--' + key, required=True, type=Path)
    p.add_argument('--head', required=True)
    p.add_argument('--workflow-head', required=True)
    p.add_argument('--run', required=True, type=int)
    a = p.parse_args()
    require(a.head == HEAD and re.fullmatch('[0-9a-f]{40}', a.workflow_head) and a.run > 0,
            'Invalid product/workflow source identity')
    require(not a.output.exists() and not a.output.is_symlink(), 'Proof output must not exist')
    require(not any(p.is_symlink() for p in a.output.absolute().parents),
            'Proof output parent must not be a symlink')
    report = json.loads(a.restoration.read_text())
    prior = report['prior_artifact']
    require(report['head'] == HEAD and report['manifest_entries'] == 653 and
            prior['id'] == PRIOR_ID and prior['run'] == PRIOR_RUN and prior['sha256'] == PRIOR_SHA,
            'Wrong original restoration identity')
    require(report['distribution_manifest_sha256'] == MANIFEST_SHA and
            report['module_sha256'] == MODULE_SHA and report['qc_sha256'] == QC_SHA,
            'Wrong original restoration payload')
    require(report['prior_archive_unchanged'] is True and
            report['original_release_zip_recreated'] is False,
            'Restoration must retain original artifact provenance')
    runtime = a.runtime.resolve(strict=True)
    files = {}
    for file in runtime.rglob('*'):
        require(not file.is_symlink(), 'Symlink in restored runtime: ' + str(file))
        if file.is_file():
            files[file.relative_to(runtime).as_posix()] = digest(file)
    require(files == report['runtime_file_sha256'] and len(files) == 654,
            'Pristine runtime must contain exactly all 653 originals plus unchanged manifest')
    require(files['distribution-manifest.json'] == MANIFEST_SHA and
            files['fteplug_ftebroom_x64.dll'] == MODULE_SHA and files['ftebroom/csaddon.dat'] == QC_SHA,
            'Pristine runtime hashes do not match fixed production build')
    manifest = json.loads((runtime / 'distribution-manifest.json').read_text())
    require(manifest == {k: v for k, v in files.items() if k != 'distribution-manifest.json'},
            'Original distribution manifest must remain exact')
    info = (runtime / 'build-info.txt').read_text().replace('\r\n', '\n')
    for line in ('ReyondBroom 0.5.0-alpha.132', 'Platform: Windows-x64',
                 'Editor revision: ' + HEAD, 'Module SHA256: ' + MODULE_SHA):
        require(line in info.splitlines(), 'Original build info mismatch: ' + line)
    qa = a.qa.resolve(strict=True)
    require(qa.name == 'qa', 'QA root must be named qa for stable archive entry references')
    carried = report['prior_proof_entry_sha256']
    require(len(carried) == 279, 'Expected all 279 genuine carried proof entries')
    for name, expected in carried.items():
        rel = relative(name)
        require(rel.parts[0] == 'qa' and not name.startswith('qa/rb10/runtime/'),
                'Invalid original proof namespace')
        file = qa.parent / rel
        require(file.is_file() and not file.is_symlink() and digest(file) == expected,
                'Carried genuine proof was changed or lost: ' + name)

    def obj(name):
        return json.loads((qa / relative(name)).read_text())

    counts = {}
    namespaces = []
    evidence = {}

    def record(folder):
        namespaces.append('qa/' + folder)
        for leaf in ('results.json', 'provenance.json', 'metrics.json', 'cases.json', 'performance.json'):
            path = qa / folder / leaf
            if path.is_file():
                evidence['qa/' + folder + '/' + leaf] = digest(path)

    def source(folder, qc=False):
        value = obj(folder + '/provenance.json')
        require(value['module_sha256'] == MODULE_SHA and
                value['build_info'].replace('\r\n', '\n') == info,
                'Fresh suite does not use the unchanged production module: ' + folder)
        if qc:
            require(value['qc_sha256'] == QC_SHA, 'Fresh suite uses wrong QC: ' + folder)
        return value

    def checks(folder, minimum, duplicates=None):
        values = obj(folder + '/results.json')
        require(isinstance(values, list) and all(isinstance(x, str) for x in values)
                and len(values) >= minimum, 'Incomplete fresh suite: ' + folder)
        repeated = {k: v for k, v in Counter(values).items() if v > 1}
        require(repeated == (duplicates or {}), 'Unexpected duplicate checks: ' + folder)
        counts[folder] = len(values)
        record(folder)

    suites = obj('rb10/runtime/results.json')
    expected = {'tutorial132': 7, 'clipboard-attributes132': 12,
                'view-options132': 29, 'native132': 12}
    require(len(suites) == 4 and {x['suite'] for x in suites} == set(expected),
            'All four runtime suites must be freshly rerun')
    for value in suites:
        require(value['qc_sha256'] == QC_SHA and
                len(value['checks']) == len(set(value['checks'])) >= expected[value['suite']],
                'Incomplete or wrong-build runtime suite: ' + value['suite'])
    counts['rb10/runtime'] = {x['suite']: len(x['checks']) for x in suites}
    record('rb10/runtime')
    checks('rb10/browsers', 11)
    checks('rb10/welcome', 20, {'only the Restart button relaunches the editor': 2})
    source('rb10/welcome')
    for folder, minimum in (('quake2', 17), ('engine-cleanup', 12), ('compilation-paths', 35)):
        checks('rb10/' + folder, minimum)
        value = source('rb10/' + folder, qc=True)
        require(value['checks'] == counts['rb10/' + folder], 'Fresh provenance count mismatch')
        if folder == 'engine-cleanup':
            require(value['engine_sha256'] == files['bin/fteqw64.exe'], 'Wrong process-probe engine')
    require(len(obj('rb10/compilation-paths/cases.json')) == 11, 'Expected eleven map-origin cases')
    checks('rb10/plant-target-ui', 10)
    for theme in ('dark', 'light'):
        for width in (1024, 1920):
            folder = f'rb10/views-{theme}-{width}'
            checks(folder, 47 if width == 1024 else 48,
                   {'3D viewport is available': 2, 'View Options is at top-right of viewport 0': 2})
            value = source(folder)
            require(value['theme'] == theme and value['window'] == [width, 768 if width == 1024 else 1080],
                    'Fresh viewport theme/size mismatch')
    updates = obj('rb10/updater/results.json')
    wanted = {'check-only', 'bad-session', 'prepare-close', 'foreign-restart', 'explicit-restart'}
    require(updates['platform'] == 'Windows-x64' and len(updates['cases']) == 5 and
            {v['case'] for v in updates['cases']} == wanted and
            all(v['passed'] is True for v in updates['cases']), 'Fresh updater five-case gate failed')
    counts['rb10/updater'] = 5
    record('rb10/updater')
    require(len(namespaces) == len(set(namespaces)) == 12, 'All twelve fresh QA namespaces required')
    require(all(not any(old.startswith(n + '/') for old in carried) for n in namespaces),
            'Fresh QA namespace collides with carried prior output')
    source_proof = json.loads(a.source_evidence.read_text())
    validate_source_evidence(source_proof)
    result = {
        'schema': 1, 'kind': 'Windows-x64-QA-continuation',
        'product_head': HEAD, 'workflow_head': a.workflow_head, 'continuation_run': a.run,
        'prior_artifact_id': PRIOR_ID, 'prior_run': PRIOR_RUN, 'prior_head': HEAD,
        'prior_artifact_sha256': PRIOR_SHA, 'prior_artifact_size': prior['size_in_bytes'],
        'runtime_manifest_sha256': MANIFEST_SHA, 'module_sha256': MODULE_SHA,
        'qc_sha256': QC_SHA, 'engine_sha256': files['bin/fteqw64.exe'], 'original_files': 653,
        'carried_entry_hashes': carried, 'new_suite_paths': namespaces, 'suite_counts': counts,
        'new_suite_evidence_hashes': evidence, 'runtime_file_sha256': files,
        'restoration_sha256': digest(a.restoration), 'source_evidence_sha256': digest(a.source_evidence),
        'metadata_rewritten': False, 'original_release_zip_recreated': False,
    }
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with a.output.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in
                      ('carried_entry_hashes', 'new_suite_evidence_hashes', 'runtime_file_sha256')}, indent=2))


if __name__ == '__main__':
    main()
