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
      'tests/world-architect*', 'tests/world-designer.cpp', 'tests/world-assets.cpp', 'tests/world-modules.cpp', 'scripts/test-world-modules.py', 'tests/world-gates.cpp', 'scripts/test-world-gates.py', 'scripts/test-world-architect.py', 'scripts/test-world-designer.py'], {'world_core'}),
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
      'scripts/editor_os_input.py'], {'input'}),
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
    (['native/*tutorial*', 'native/*onboarding*', 'qc/fb_tutorial*.qc', 'scripts/test-editor133-input.py',
      'scripts/test-tutorial-linux.sh', 'tests/tutorial*.cfg'], {'onboarding'}),
    (['scripts/test-bsp-preview-input.py'], {'viewport'}),
    (['runtime/update-*', 'runtime/ui/update-splash.png', 'scripts/*updater*', 'scripts/test-rb10-welcome.py', 'tests/*updater*',
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
        '  commands->AddCommand("ftebroom_world_architect",worldCommand,"Open World Architect.");\n',
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
        '  const bool opened=ImGui::BeginMenu("Plugins");runUiRecordGlobal("menu/Plugins");\n  if(opened){\n',
        '  if(ImGui::BeginMenu("Plugins")){\n',
        '    if(ImGui::MenuItem("World Architect (development preview)..."))worldOpen();\n',
        '    if(ImGui::MenuItem("World Architect..."))worldOpen();\n',
        '    runUiRecordGlobal("Menu/Plugins/World Architect...");\n',
        '    for(auto& p:editorPlugins)if(p.api)if(ImGui::MenuItem(p.name.c_str()))p.open=true;\n',
        '    for(auto& p:editorPlugins)if(p.api&&p.id!="org.reyondbroom.autolevel"&&p.id!="org.reyondbroom.levelgen")if(ImGui::MenuItem(p.name.c_str()))p.open=true;\n',
        '    if(ImGui::BeginMenu("Legacy generators")){\n      for(auto& p:editorPlugins)if(p.api&&(p.id=="org.reyondbroom.autolevel"||p.id=="org.reyondbroom.levelgen"))if(ImGui::MenuItem(p.name.c_str()))p.open=true;\n      ImGui::EndMenu();\n    }\n',
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


ONBOARDING_HOST_REPLACEMENTS = {
    'native/ftebroom_views.h': [('if (cvars->GetFloat("ftebroom_skip_setup"))',
                                'if (tourActive || cvars->GetFloat("ftebroom_skip_setup"))')],
    'native/ftebroom_desktop_tests.h': [('tourStep==6&&tourDone', 'tourStep==tourToolsStep&&tourDone')],
    'qc/ftebroom_edit.qc': [('    if(valid) {fb_clip_count=0;fb_clip_exact=FALSE;print(sprintf("ReyondBroom: clipped %i brushes.\\n",applied));}',
                            '    if(valid) {\n        fb_clip_count=0;fb_clip_exact=FALSE;print(sprintf("ReyondBroom: clipped %i brushes.\\n",applied));\n'
                            '        if(applied && cvar("ftebroom_tour_step")==7)fb_tour_signal(7);\n    }')],
    'qc/csaddon.qc': [('        cvar_set("ftebroom_tour_anchor",sprintf("%g %g",screen113_x,screen113_y));\n',
                       '        cvar_set("ftebroom_tour_anchor",sprintf("%g %g",screen113_x,screen113_y));\n'
                       '        if(step113==7){\n            // Points on the top face of the 96 x 96 x 64 lesson brush.\n'
                       "            vector clipStart=project('16 -32 64'),clipEnd=project('16 32 64');\n"
                       '            cvar_set("ftebroom_tour_clip_pixels",sprintf("%g %g %g %g",clipStart_x,clipStart_y,clipEnd_x,clipEnd_y));\n        }\n')],
    'native/ftebroom.cpp': [
        ('    tourPanel(w,h);', '    drawClipPoints();tourPanel(w,h);'),
        ('if(tourActive&&(tourDemo||ImGui::GetTime()<tourInputReady))return qtrue;',
         'if(tourActive&&tourKey(key,down,inScene,hit))return qtrue;'),
    ],
}

# These are exact guarded host edits. Any additional change to ordinary tool
# dispatch or browser behavior falls back to its shared dependency checks.
LESSON_HOST_REPLACEMENTS = {
    'qc/csaddon.qc': [
        ('''        if(step113==7){
            // Points on the top face of the 96 x 96 x 64 lesson brush.
            vector clipStart=project('16 -32 64'),clipEnd=project('16 32 64');
            cvar_set("ftebroom_tour_clip_pixels",sprintf("%g %g %g %g",clipStart_x,clipStart_y,clipEnd_x,clipEnd_y));
        }''', '        fb_tutorial_project();'),
        ('\tif (ftebroom_command()) return TRUE;',
         '    int tutorialHistory=history;\n    if(ftebroom_command()){fb_tutorial_observe(tutorialHistory);return TRUE;}'),
    ],
    'qc/ftebroom.qc': [
        ('substring(argv(0),0,16)!="ftebroom_camera_" && argv(0)!="ftebroom_camera")',
         'substring(argv(0),0,16)!="ftebroom_camera_" && argv(0)!="ftebroom_camera" && argv(0)!="ftebroom_context")'),
    ],
    'native/ftebroom_materials.h': [
        ('static void materialBrowser()\n{', 'static void materialBrowser(bool lesson=false)\n{\n  materialTestItems.clear();'),
        ('  static float propertiesHeight=std::min(448*scale,room*.55f);',
         '  static float propertiesHeight=std::min(448*scale,room*.55f);\n  const float savedPropertiesHeight=propertiesHeight;\n  if(lesson)propertiesHeight=std::min(180*scale,room*.32f);'),
        ('  uiInspectorTopEnd("Material browser divider",propertiesHeight);',
         '  uiInspectorTopEnd("Material browser divider",propertiesHeight);\n  if(lesson)propertiesHeight=savedPropertiesHeight;'),
        ('auto query=std::string(filter)', 'auto query=std::to_string(lesson)+":"+std::string(filter)'),
        ('      !removedCollections.count(materialFolder(pair.first))',
         '      (lesson && (pair.first=="textures/ftebroom/qa_blue"||pair.first=="textures/ftebroom/qa_green"||pair.first=="textures/ftebroom/qa_red")) ||\n      (!lesson && !removedCollections.count(materialFolder(pair.first))'),
        ('&& dimensionsMatch(pair.first))', '&& dimensionsMatch(pair.first)))'),
        ('        if(ImGui::InvisibleButton("tile",{float(size+6),rowHeight-caption-4}))chooseMaterial(mat.name);',
         '        if(ImGui::InvisibleButton("tile",{float(size+6),rowHeight-caption-4}))chooseMaterial(mat.name);\n        materialTestItems["tile/"+mat.name]={(ImGui::GetItemRectMin().x+ImGui::GetItemRectMax().x)*.5f,(ImGui::GetItemRectMin().y+ImGui::GetItemRectMax().y)*.5f};'),
    ],
}


def lesson_host_scope(path, before, after):
    replacements = LESSON_HOST_REPLACEMENTS.get(path)
    if not replacements or before == after:
        return None
    for old, new in replacements:
        if before.count(old) != 1:
            return None
        before = before.replace(old, new)
    if before != after:
        return None
    return {'viewport'} if path == 'qc/ftebroom.qc' else {'onboarding'}


def onboarding_host_scope(path, before, after):
    replacements = ONBOARDING_HOST_REPLACEMENTS.get(path)
    if not replacements or before == after:
        return None
    if path == 'native/ftebroom.cpp':
        # The ordinary editor still calls the identical overlay code. Verify
        # the extraction before allowing a tutorial-only validation scope.
        start = 'static void drawClipPoints()\n{\n'
        prefix, marker, tail = after.partition(start)
        block, end, suffix = tail.partition('\n}\n')
        if not marker or not end or not block or before.count(block) != 1:
            return None
        after = prefix + suffix.removeprefix('\n')
        before = before.replace(block, '  drawClipPoints();')
    for old, new in replacements:
        if before.count(old) != 1:
            return None
        before = before.replace(old, new)
    return {'onboarding'} if before == after else None


def semantic_scopes(base, head, paths):
    result = {}
    for path in paths:
        if path not in WORLD_HOST_ADDITIONS and path not in ONBOARDING_HOST_REPLACEMENTS and path not in LESSON_HOST_REPLACEMENTS:
            continue
        def read(ref):
            return subprocess.check_output(['git', 'show', ref+':'+path], stderr=subprocess.PIPE).decode()
        before, after = read(base), read(head)
        groups = world_host_scope(path, before, after) or onboarding_host_scope(path, before, after) or lesson_host_scope(path, before, after)
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
