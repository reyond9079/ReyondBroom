#!/usr/bin/env python3
"""Select CI suites through a subsystem dependency graph (no GitHub API required)."""
import argparse
import fnmatch
import json
import os
from pathlib import Path
import subprocess

# Edges are consumers/integration checks, not just matching file names. Shared
# resource/input/render changes exercise authoring integration, not all botany.
DEPENDENCIES = {
    'world_architect': {'world_core', 'world_ui', 'world_legacy'},
    'world_core': {'world_ui'},
    'world_ui': {'placement', 'world_legacy'},
    'profiles': {'profile_core', 'compilation'},
    'resources': {'compilation', 'resource_lookup', 'audio_archive', 'placement'},
    'input': {'compilation_ui', 'viewport', 'browser', 'onboarding', 'placement', 'audio_ui', 'world_ui'},
    'render': {'grid', 'viewport', 'placement'},
    'plants': {'plant_core', 'plant_ui', 'placement'},
    'plant_ui': {'placement'},
    'models': {'model_ui', 'placement'},
    'audio': {'audio_core', 'audio_ui', 'audio_archive'},
    'audio_ui': {'audio_archive'},
    'browser': {'audio_archive'},
    'compilation': {'profile_core', 'compilation_ui'},
    'updater': set(), 'onboarding': set(), 'ui': {'viewport', 'compilation_ui'},
    'build': {'smoke'}, 'ci': {'ci_unit'},
}
SUITES = {'world_core', 'world_ui', 'world_legacy', 'profile_core', 'compilation', 'compilation_ui', 'resource_lookup',
          'audio_core', 'audio_ui', 'audio_archive', 'plant_core', 'plant_ui',
          'model_ui', 'placement', 'viewport', 'grid', 'browser', 'onboarding',
          'updater', 'ci_unit', 'atlas', 'reference', 'catalog', 'matrix_ui'}
RULES = [
    (['native/world_architect/**', 'runtime/world-architect/**',
      'tests/world-architect*', 'tests/world-designer.cpp', 'tests/world-assets.cpp', 'tests/world-modules.cpp', 'scripts/test-world-modules.py', 'scripts/test-world-architect.py', 'scripts/test-world-designer.py'], {'world_core'}),
    (['native/ftebroom_world_architect.h'], {'world_ui'}),
    (['native/ftebroom_world_authoring.h'], {'world_ui'}),
    (['qc/fb_world_architect.qc', 'qc/fb_world_baseline.qc', 'scripts/test-world-architect-ui.py', 'scripts/test-world-applied.py'], {'world_ui'}),
    (['tests/world-legacy.cpp', 'scripts/test-world-legacy.py'], {'world_legacy'}),
    (['plugins/autolevel/al_storage.*', 'tests/test-autolevel-storage.cpp'], {'world_legacy'}),
    (['plugins/autolevel/al_brush.*', 'plugins/autolevel/al_model.h',
      'plugins/autolevel/al_fill.h', 'plugins/autolevel/al_math.h'], {'world_core'}),
    (['AGENTS.md', 'docs/**', '*.md'], {'docs'}),
    (['.github/**', 'scripts/ci-*', 'tests/ci-*'], {'ci'}),
    (['scripts/setup-ci-renderer.ps1'], {'ci'}),
    (['native/profile/**', 'native/third_party/trenchbroom/**',
      'native/ftebroom_run*', 'native/ftebroom_engine_launch*',
      'native/ftebroom_profile_*',
      'tests/*profile*', 'tests/engine-launch*', 'tests/legacy-profiles*',
      'tests/fixtures/CompilationProfiles*', 'scripts/*compilation*',
      'scripts/test-engine-cleanup.py', 'scripts/test-mod-profile*',
      'scripts/test-trenchbroom*'], {'profiles'}),
    (['native/plant/**', 'runtime/plants/**', 'tests/plant*',
      'scripts/build-plant*'], {'plants'}),
    (['native/ftebroom_plant*', 'qc/*plant*', 'scripts/*plant*'], {'plant_ui'}),
    (['native/*audio*', 'native/ftebroom_sound*', 'tests/audio*', 'scripts/*audio*'], {'audio'}),
    (['native/ftebroom_model*', 'native/ftebroom_bb*', 'tests/*model*'], {'models'}),
    (['native/*browser*', 'native/*file_view*', 'native/*disk_cache*',
      'scripts/test-rb10-browsers.py', 'tests/browser*'], {'browser'}),
    (['native/*input*', 'native/*shortcut*', 'native/ftebroom_actions*',
      'native/ftebroom_context*', 'native/ftebroom_desktop*',
      'scripts/editor_os_input.py', 'scripts/test-editor133-input.py'], {'input'}),
    (['scripts/test-engine-viewport.py'], {'input'}),
    (['patches/*input*', 'patches/*mouse*', 'patches/*window*'], {'input'}),
    (['patches/*resource*', 'patches/*mapasset*'], {'resources'}),
    (['patches/*grid*', 'patches/*render*', 'patches/*shader*'], {'render'}),
    (['native/ftebroom_map_mod*', 'native/ftebroom_map_panel*',
      'native/ftebroom_*assets*', 'native/ftebroom_*wad*',
      'native/ftebroom_game_profile*', 'native/ftebroom_mod*',
      'native/ftebroom_build*', 'qc/*build*', 'runtime/*.ent',
      'runtime/*.fgd', 'tests/*resource*', 'tests/*wad*',
      'scripts/test-quake2*'], {'resources'}),
    (['runtime/glsl/**', 'runtime/scripts/**', 'native/*render*',
      'native/*viewport*', 'native/*camera*', 'native/*grid*',
      'native/ftebroom_resolution*', 'scripts/test-viewport-appearance.py',
      'scripts/test-engine-viewport.py', 'tests/*grid*'], {'render'}),
    (['native/*tutorial*', 'native/*onboarding*'], {'onboarding'}),
    (['runtime/update-*', 'scripts/*updater*', 'tests/*updater*',
      'native/ftebroom_updat*', 'native/ftebroom_welcome*'], {'updater'}),
    (['native/*theme*', 'native/*translations*', 'native/*appearance*',
      'native/*fonts*', 'runtime/fonts/**'], {'ui'}),
    (['scripts/build-*', 'scripts/package-*', 'scripts/apply-engine-patches.py'], {'build'}),
]

HARNESS = {'scripts/test-compilation-runtime.py', 'scripts/test-audio-archive-windows.ps1',
           'scripts/setup-ci-renderer.ps1'}


def infrastructure(path):
    return (path.startswith(('.github/', 'scripts/ci-', 'tests/ci-', 'docs/'))
            or path in HARNESS or path == 'AGENTS.md' or path.endswith('.md'))


def plan(paths, full=False, release=False, semantic=None):
    paths = sorted(set(paths))
    components, reasons = set(), {}
    infrastructure_only = bool(paths) and all(infrastructure(path) for path in paths)
    for path in paths:
        matched = set()
        if infrastructure_only:
            matched = {'ci'}
        elif semantic and path in semantic:
            matched = set(semantic[path])
        elif path == 'tests/plant-export-manager.cpp':
            matched = {'plant_ui'}
        elif path == 'native/world_architect/legacy.h':
            # This read-only codec is consumed by migration, not generation.
            matched = {'world_legacy'}
        elif path.startswith('native/world_architect/'):
            # Art-theme geometry belongs to the generator. Broad native UI
            # theme rules must not capture this independently namespaced code.
            matched = {'world_core'}
        else:
            for patterns, groups in RULES:
                if any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns):
                    matched |= groups
        if not matched:
            if path.startswith(('native/', 'qc/', 'patches/', 'runtime/', 'tests/', 'scripts/')):
                # An unclassified shared edit must not silently evade integration.
                matched = {'profiles', 'resources', 'input', 'render', 'audio', 'plants', 'models', 'updater', 'world_architect'}
            else:
                matched = {'ci'}
        components |= matched
        reasons[path] = sorted(matched)
    selected = set(components)
    while True:
        expanded = selected | set().union(*(DEPENDENCIES.get(c, set()) for c in selected))
        if expanded == selected:
            break
        selected = expanded
    suites = set(SUITES) if full else selected & SUITES
    runtime = full or bool(suites - {'ci_unit'}) or 'build' in selected
    return {'schema': 1, 'mode': 'full' if full else 'affected', 'paths': paths,
            'components': sorted(components), 'reasons': reasons,
            'suites': sorted(suites), 'runtime': runtime,
            'smoke': release or runtime,
            'flags': {name: name in suites or name in selected for name in sorted(SUITES | DEPENDENCIES.keys())}}


# Only these exact additions may be normalized away. Comparing the complete
# remaining file prevents a nearby renderer/input/resource edit from inheriting
# the narrow scope of a tool registration in the same large translation unit.
WORLD_HOST_ADDITIONS = {
    'native/ftebroom.cpp': [
        'static void worldOpen();\n',
        '#include "ftebroom_world_architect.h"\n',
        '  if(worldArchitect.work)worldArchitect.work->cancel=true;worldRelease();\n',
        '  commands->AddCommand("ftebroom_world_architect",worldCommand,"Open the World Architect development preview.");\n',
        '||worldArchitectFocused()', '&&!worldArchitectFocused()', 'worldPanel();',
    ],
    'qc/fb_clipboard.qc': [
        'int fb_world_importing;\n',
        'fb_world_importing?fb_tx_empty_entity():(int)',
        '#include "fb_world_architect.qc"\n',
        '    if(argv(0)=="ftebroom_world_apply"){fb_world_architect_apply();return TRUE;}\n',
    ],
    'qc/csaddon.qc': ['    registercommand("ftebroom_world_apply");\n'],
    'native/ftebroom_plugins.h': [
        '    if(ImGui::MenuItem("World Architect (development preview)..."))worldOpen();\n',
    ],
}


def world_host_scope(path, before, after):
    additions = WORLD_HOST_ADDITIONS.get(path)
    if not additions or before == after:
        return None
    def normalize(source):
        for addition in additions:
            source = source.replace(addition, '')
        return source
    if normalize(before) != normalize(after):
        return None
    groups = {'world_ui'}
    if before.count('worldArchitectFocused()') != after.count('worldArchitectFocused()'):
        groups.add('input')
    return groups


def semantic_scopes(base, head, paths):
    result = {}
    for path in paths:
        if path not in WORLD_HOST_ADDITIONS:
            continue
        def read(ref):
            return subprocess.check_output(['git', 'show', ref+':'+path], stderr=subprocess.PIPE).decode()
        groups = world_host_scope(path, read(base), read(head))
        if groups:
            result[path] = groups
    return result


def changed_paths(base, head, release=False):
    # --no-renames includes BOTH removed and added paths, including renames
    # across subsystems. PRs use their merge base; releases compare actual trees
    # so a rollback to an older ancestor cannot look like an empty change.
    comparison = base+('..' if release else '...')+head
    body = subprocess.check_output(['git', 'diff', '--no-renames', '--name-only', '-z', comparison], stderr=subprocess.PIPE)
    return [os.fsdecode(p) for p in body.split(b'\0') if p]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base')
    p.add_argument('--head', default='HEAD')
    p.add_argument('--full', action='store_true')
    p.add_argument('--release', action='store_true')
    p.add_argument('--files', nargs='*')
    p.add_argument('--output', type=Path)
    a = p.parse_args()
    if a.files is None and not a.base:
        p.error('--base or --files is required; a missing base must not mean no changes')
    paths = a.files if a.files is not None else changed_paths(a.base, a.head, a.release)
    # PR scope must compare the same merge base used by changed_paths. Release
    # scope compares the actual endpoints, including rollback changes.
    base = a.base
    if a.files is None and not a.release:
        base = subprocess.check_output(['git', 'merge-base', a.base, a.head]).decode().strip()
    semantic = semantic_scopes(base, a.head, paths) if a.files is None else {}
    result = plan(paths, a.full, a.release, semantic)
    text = json.dumps(result, sort_keys=True)
    if a.output:
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(text+'\n')
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a') as f:
            f.write('plan='+text+'\n')
            f.write('runtime='+str(result['runtime']).lower()+'\n')
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as f:
            f.write('### Validation scope\n\nMode: '+result['mode']+'\n\nSuites: '+', '.join(result['suites'])+'\n\n')
            for path, components in result['reasons'].items():
                f.write('- `'+path+'`: '+', '.join(components)+'\n')
    print(text)


if __name__ == '__main__':
    main()
