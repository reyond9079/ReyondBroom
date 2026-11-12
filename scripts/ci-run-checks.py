#!/usr/bin/env python3
"""Run only the suites selected by ci-plan, using the existing real oracles."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--plan', required=True, type=Path)
    p.add_argument('--runtime', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--probe', required=True, type=Path)
    p.add_argument('--platform', required=True)
    a = p.parse_args()
    selected = json.loads(a.plan.read_text()); suites = set(selected['suites'])
    root, out = a.runtime.resolve(), a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    repo = Path(__file__).resolve().parents[1]
    completed = []
    def run(suite, script, *args, gui=True):
        with tempfile.TemporaryDirectory(prefix='reyond-suite-') as directory:
            if root in args and suite != 'fixture':
                isolated = Path(directory)/root.name
                shutil.copytree(root, isolated)
                if suite in {'compilation', 'compilation_ui'}:
                    name = 'compilation-profile-probe.exe' if a.platform == 'Windows-x64' else 'compilation-profile-probe'
                    (isolated/'tools').mkdir(exist_ok=True)
                    shutil.copy2(a.probe, isolated/'tools'/name)
                args = tuple(isolated if arg == root else arg for arg in args)
            command = [sys.executable, str(repo/script), *map(str, args)] if script.endswith('.py') else ['bash', str(repo/script), *map(str, args)]
            if gui and os.name != 'nt':
                command = ['xvfb-run', '-a', '-s', '-screen 0 1920x1080x24', *command]
            print('RUN '+suite+': '+script, flush=True)
            subprocess.run(command, check=True, cwd=repo, env={**os.environ, 'LIBGL_ALWAYS_SOFTWARE': '1'})
        completed.append({'suite': suite, 'script': script})
        (out/'executed-suites.json').write_text(json.dumps(completed, indent=2))
    if suites & {'compilation', 'compilation_ui', 'plant_ui', 'placement', 'model_ui', 'viewport', 'browser', 'onboarding', 'world_ui', 'world_legacy'}:
        run('fixture', 'tests/make-linux-build-fixture.py', root, gui=False)
    if 'world_legacy' in suites:
        run('world_legacy', 'scripts/test-world-legacy.py', out/'world/legacy-core', gui=False)
        run('world_legacy', 'scripts/test-world-architect-ui.py', root, out/'world/legacy-ui', '--legacy-only', '--legacy-fixtures', out/'world/legacy-core')
    if 'world_core' in suites:
        run('world_core', 'scripts/test-world-architect.py', out/'world/core', '--runtime', root,
            '--seeds', '100' if selected['mode'] == 'full' else '3', gui=False)
        run('world_core', 'scripts/test-world-designer.py', root, out/'world/designer-core', gui=False)
    if suites & {'world_core', 'world_ui'}:
        fixture_args = () if 'world_core' in suites else ('--fixtures-only',)
        run('world_core' if 'world_core' in suites else 'world_ui', 'scripts/test-world-modules.py', root, out/'world/modules', *fixture_args, gui=False)
        run('world_core' if 'world_core' in suites else 'world_ui', 'scripts/test-world-gates.py', root, out/'world/gates', *fixture_args, gui=False)
    if 'world_ui' in suites:
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/menu-ui', '--menu-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/ui')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/metro-ui', '--preset', 'metro')
        run('world_ui', 'scripts/test-world-applied.py', root/'tools/q3map2'/('q3map2.exe' if os.name=='nt' else 'q3map2'),
            out/'world/metro-ui', '--fixture-texture', root/'ftebroom/textures/ftebroom/qa_blue.tga', gui=False)
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/regions-ui', '--preset', 'evacuation', '--regions-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/pins-ui', '--preset', 'evacuation', '--pins-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/designer-ui', '--designer-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/themes-ui', '--themes-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/scenarios-ui', '--scenarios-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/local-ui', '--local-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/quality-ui', '--quality-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/planning-ui', '--planning-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/exterior-light-ui', '--exterior-light-only')
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/gates-ui', '--gates-only', '--gate-fixtures', out/'world/gates')
        run('world_ui', 'scripts/test-world-gates.py', root, out/'world/gates-applied', '--applied', out/'world/gates-ui', '--fixture-texture', root/'ftebroom/textures/ftebroom/qa_blue.tga', gui=False)
        run('world_ui', 'scripts/test-world-architect-ui.py', root, out/'world/modules-ui', '--modules-only', '--module-fixtures', out/'world/modules')
    if 'compilation' in suites:
        run('compilation', 'scripts/test-compilation-runtime.py', root, out/'profiles', '--probe', a.probe.resolve(),
            *([] if 'atlas' in suites else ['--targeted']))
        run('compilation', 'scripts/test-engine-cleanup.py', root, out/'engine-cleanup', '--probe', a.probe.resolve())
        run('compilation', 'scripts/test-compilation-paths.py', root, out/'compilation-paths', '--probe', a.probe.resolve())
        run('compilation', 'scripts/test-mod-profile-lighting.py', root, out/'mod-profiles', '--probe', a.probe.resolve())
    if 'compilation_ui' in suites:
        run('compilation_ui', 'scripts/test-compilation-ui.py', root, out/'profile-ui')
    if 'resource_lookup' in suites:
        run('resource_lookup', 'scripts/test-quake2-resources.py', root, out/'quake2')
    if 'audio_ui' in suites and os.name != 'nt':
        run('audio_ui', 'scripts/test-audio-editor-linux.sh', root, out/'audio')
    if suites & {'audio_ui', 'audio_archive'}:
        run('audio_archive', 'scripts/test-audio-archive.py', root, out/'audio-archive')
    if 'plant_ui' in suites:
        run('plant_ui', 'scripts/test-plant-scale.py', out/'plants/scale', gui=False)
        run('plant_ui', 'scripts/test-plant-export-unit.py', out/'plants/export-unit', gui=False)
        run('plant_ui', 'scripts/test-plant-ui.py', root, out/'plants/export-ui', '--export-only')
        run('plant_ui', 'scripts/test-plant-ui.py', root, out/'plants/ui')
        run('plant_ui', 'scripts/test-plant-exports.py', out/'plants/ui', gui=False)
        run('plant_ui', 'scripts/test-plant-map.py', root, out/'plants/ui', gui=False)
        run('plant_ui', 'scripts/test-plant-brush.py', root, out/'plants/brush')
    if 'placement' in suites:
        cases = [('dark', 1920, 1080), ('light', 1920, 1080), ('dark', 1024, 768), ('light', 1024, 768)] if 'matrix_ui' in suites else [('dark', 1280, 800)]
        for theme, width, height in cases:
            run('placement', 'scripts/test-viewport-create.py', root, out/f'create-{theme}-{width}',
                '--theme', theme, '--width', width, '--height', height)
    if 'viewport' in suites:
        if selected['flags'].get('input') or selected['mode'] == 'full':
            run('viewport', 'scripts/test-engine-viewport.py', root, out/'engine-viewport', '--probe', a.probe.resolve())
        cases = [('dark', 1920, 1080), ('light', 1920, 1080), ('dark', 1024, 768), ('light', 1024, 768)] if 'matrix_ui' in suites else [('dark', 1920, 1080)]
        for theme, width, height in cases:
            run('viewport', 'scripts/test-viewport-appearance.py', root, out/f'appearance-{theme}-{width}',
                '--theme', theme, '--width', width, '--height', height)
    if 'browser' in suites:
        run('browser', 'scripts/test-rb10-browsers.py', root, out/'browsers')
    if 'onboarding' in suites:
        run('onboarding', 'scripts/test-editor133-input.py', root, out/'input-onboarding')
    if 'updater' in suites:
        run('updater', 'tests/test-welcome-updater.py', '--platform', a.platform, '--output', out/'updater', '--runtime', root, gui=False)
    (out/'scope.json').write_text(json.dumps({'plan': selected, 'completed': completed}, indent=2))


if __name__ == '__main__':
    main()
