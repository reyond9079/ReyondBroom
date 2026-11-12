#!/usr/bin/env python3
"""CI policy contracts: dependency closure, cache invalidation, real gate failures."""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

REPO = Path(__file__).resolve().parents[1]


def module(name):
    spec = importlib.util.spec_from_file_location(name, REPO/'scripts'/('ci-'+name+'.py'))
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result


planner, cache, runner, smoke = [module(n) for n in ('plan', 'dependency', 'run-checks', 'release-smoke')]
prepared = module('verify-prepared')


class ScopeTests(unittest.TestCase):
    def test_prepared_world_choice_has_a_nonempty_default_and_normal_mode(self):
        import yaml
        workflow = yaml.safe_load((REPO/'.github/workflows/compilation-profile-checks.yml').read_text())
        inputs = workflow['on']['workflow_dispatch']['inputs']
        choice = inputs['world_authoring_case']
        self.assertEqual(choice['default'], 'none')
        self.assertTrue(all(choice['options']))
        self.assertIn(choice['default'], choice['options'])
        steps = workflow['jobs']['windows-input-reproduction']['steps']
        normal = next(s for s in steps if s.get('name') == 'Exercise actual Windows controls with OS input')
        world = next(s for s in steps if s.get('name') == 'Exercise the prepared World Architect with actual OS input')
        self.assertIn("inputs.world_authoring_case == 'none'", normal['if'])
        self.assertIn("inputs.world_authoring_case != 'none'", world['if'])

    def test_world_core_routes_its_editor_consumers_without_botany(self):
        p = planner.plan(['native/world_architect/generator.h'])
        self.assertEqual(set(p['suites']), {'world_core', 'world_ui', 'world_legacy', 'placement'})

    def test_authored_modules_and_gate_contracts_keep_the_world_scope(self):
        for path in ['tests/world-modules.cpp', 'tests/world-gates.cpp', 'scripts/test-world-gates.py']:
            self.assertEqual(set(planner.plan([path])['suites']), {'world_core', 'world_ui', 'world_legacy', 'placement'})

    def test_legacy_codec_has_focused_migration_checks(self):
        p = planner.plan(['native/world_architect/legacy.h'])
        self.assertEqual(set(p['suites']), {'world_legacy'})
        self.assertTrue(p['runtime'])

    def test_legacy_shared_storage_and_brush_consumers(self):
        self.assertEqual(set(planner.plan(['plugins/autolevel/al_storage.cpp'])['suites']), {'world_legacy'})
        p = planner.plan(['plugins/autolevel/al_brush.cpp'])
        self.assertTrue({'world_core', 'world_ui', 'world_legacy'} <= set(p['suites']))
        self.assertFalse(p['flags']['plant_core'])

    def test_world_art_theme_is_geometry_with_its_own_ui_consumers(self):
        p = planner.plan(['native/world_architect/theme_kits.h'])
        self.assertEqual(set(p['suites']), {'world_core', 'world_ui', 'world_legacy', 'placement'})
        self.assertFalse(p['flags']['viewport'] or p['flags']['compilation_ui'] or p['flags']['matrix_ui'])

    def test_world_frame_and_input_include_migration_dialogs(self):
        for path in ['native/ftebroom_world_authoring.h', 'scripts/editor_os_input.py']:
            p = planner.plan([path])
            self.assertTrue({'world_ui', 'world_legacy'} <= set(p['suites']))
            self.assertFalse(p['flags']['plant_core'] or p['flags']['atlas'])
        self.assertEqual(set(planner.plan(['native/ftebroom_world_authoring.h'])['suites']), {'world_ui', 'world_legacy', 'placement'})
        self.assertEqual(set(planner.plan(['native/ftebroom_world_architect.h'])['suites']), {'world_ui', 'world_legacy', 'placement'})

    def test_world_host_registration_and_focus_guards_are_classified_by_code(self):
        before = 'void frame(){modelViewerPanel();bbBenchTick();plantPanel();}\nif(runUiNavigation()||plantFocused())capture();\n'
        after = 'static void worldOpen();\n'+before.replace('plantPanel();', 'plantPanel();worldPanel();').replace('plantFocused()', 'plantFocused()||worldArchitectFocused()')
        groups = planner.world_host_scope('native/ftebroom.cpp', before, after)
        self.assertEqual(groups, {'world_ui', 'input'})
        self.assertIsNone(planner.world_host_scope('native/ftebroom.cpp', before, after.replace('capture();', 'cameraMove();')))
        p = planner.plan(['native/ftebroom.cpp'], semantic={'native/ftebroom.cpp': groups})
        self.assertTrue(p['flags']['compilation_ui'] and p['flags']['onboarding'])
        self.assertFalse(p['flags']['plant_core'])

    def test_world_scoped_clipboard_allocator_keeps_legacy_changes_conservative(self):
        before = 'int id=terrain_edit(TEREDIT_ENT_ADD,"");entities[i]=id;\n'
        after = 'int fb_world_importing;\n'+before.replace('terrain_edit(', 'fb_world_importing?fb_tx_empty_entity():(int)terrain_edit(')
        self.assertEqual(planner.world_host_scope('qc/fb_clipboard.qc', before, after), {'world_ui'})
        self.assertIsNone(planner.world_host_scope('qc/fb_clipboard.qc', before, after.replace('entities[i]=id', 'entities[i]=0')))

    def test_promoted_world_menu_keeps_legacy_entry_and_other_plugin_changes_conservative(self):
        source = (REPO/'native/ftebroom_plugins.h').read_text()
        begin = source.index('    if(ImGui::MenuItem("World Architect..."))')
        end = source.index('    ImGui::EndMenu();\n  }\n}', begin)
        promoted = source[begin:end]
        previous = ('    if(ImGui::MenuItem("World Architect (development preview)..."))worldOpen();\n'
                    '    if(ImGui::MenuItem("Manage plugins..."))pluginManagerOpen=true;\n'
                    '    for(auto& p:editorPlugins)if(p.api)if(ImGui::MenuItem(p.name.c_str()))p.open=true;\n')
        self.assertEqual(planner.world_host_scope('native/ftebroom_plugins.h', previous, promoted), {'world_ui'})
        self.assertIsNone(planner.world_host_scope('native/ftebroom_plugins.h', previous, promoted.replace('pluginManagerOpen=true;', 'pluginManagerOpen=false;')))

    def test_onboarding_clip_overlay_move_requires_identical_shared_rendering(self):
        path = 'native/ftebroom.cpp'
        block = '  if (clipSelected()) { drawExistingPoints(); }'
        before = 'static void panel() {\n    tourPanel(w,h);\n'+block+'\n}\n' \
                 'if(tourActive&&(tourDemo||ImGui::GetTime()<tourInputReady))return qtrue;\n'
        after = 'static void drawClipPoints()\n{\n'+block+'\n}\n\n' + before.replace(
            block, '  drawClipPoints();').replace('tourPanel(w,h);', 'drawClipPoints();tourPanel(w,h);').replace(
            '(tourDemo||ImGui::GetTime()<tourInputReady)', 'tourKey(key,down,inScene,hit)')
        self.assertEqual(planner.onboarding_host_scope(path, before, after), {'onboarding'})
        self.assertIsNone(planner.onboarding_host_scope(path, before, after.replace('drawExistingPoints', 'drawDifferentPoints')))

    def test_lesson_host_scopes_reject_unrelated_changes(self):
        for path, replacements in planner.LESSON_HOST_REPLACEMENTS.items():
            before='\n'.join(old for old,new in replacements)+'\nordinary dispatch stays unchanged\n'
            after=before
            for old,new in replacements:after=after.replace(old,new)
            expected={'viewport'} if path=='qc/ftebroom.qc' else {'onboarding'}
            self.assertEqual(planner.lesson_host_scope(path,before,after),expected)
            self.assertIsNone(planner.lesson_host_scope(path,before,after.replace('ordinary dispatch stays unchanged','ordinary dispatch changed')))

    def test_lesson_and_splash_checks_exclude_unrelated_generators(self):
        p=planner.plan(['native/ftebroom_tutorial.h','qc/fb_tutorial.qc','scripts/test-tutorial-linux.sh',
                        'native/ftebroom_update.h','runtime/ui/update-splash.png','scripts/test-bsp-preview-input.py'])
        self.assertEqual(set(p['suites']),{'onboarding','updater','viewport'})
        self.assertFalse(p['flags']['catalog'] or p['flags']['plant_core'] or p['flags']['world_core'] or p['flags']['atlas'] or p['flags']['matrix_ui'])

    def test_windows_release_build_creates_its_output_when_audio_tests_are_skipped(self):
        import re
        import shlex
        import yaml
        workflow = yaml.safe_load((REPO/'.github/workflows/release-linux-ui-update.yml').read_text())
        step = next(item for item in workflow['jobs']['windows']['steps']
                    if item.get('name') == 'Build native editor and test the profile importer')
        lines = step['run'].splitlines()
        build = next(i for i, line in enumerate(lines) if line.lstrip().startswith(('make ', 'g++ ')))
        prelude = '\n'.join(lines[:build])
        link = next(shlex.split(line) for line in lines if line.lstrip().startswith('g++ '))
        output = link[link.index('-o')+1]
        self.assertTrue(re.fullmatch(r'[A-Za-z0-9_./-]+\.dll', output))
        # Start with no audio test step or build directories, run the actual
        # release prelude, then exercise the linker's file creation contract.
        with tempfile.TemporaryDirectory() as folder:
            subprocess.run(['bash', '-c', prelude], cwd=folder, check=True)
            target = Path(folder)/output
            target.write_bytes(b'owned linker output fixture')
            self.assertTrue(target.is_file())

    def test_profile_change_has_complete_behavior_without_botany_or_atlas(self):
        p = planner.plan(['native/ftebroom_run_ui.h'])
        self.assertEqual(set(p['suites']), {'compilation', 'compilation_ui', 'profile_core'})
        self.assertTrue(p['runtime'])

    def test_shared_input_checks_consumers_without_botanical_algorithms(self):
        p = planner.plan(['patches/zzzzzz-editor-focused-mouse-activation.patch'])
        self.assertTrue({'compilation_ui', 'browser', 'onboarding', 'audio_ui', 'placement', 'viewport'} <= set(p['suites']))
        self.assertFalse(p['flags']['plant_core'])

    def test_resources_include_compiler_sound_and_model_integration(self):
        p = planner.plan(['native/ftebroom_map_mods.h'])
        self.assertTrue({'compilation', 'resource_lookup', 'audio_archive', 'placement'} <= set(p['suites']))
        self.assertFalse(p['flags']['plant_core'])

    def test_rendering_and_plant_ui_are_different_from_generator_math(self):
        self.assertTrue(planner.plan(['runtime/glsl/ftebroom_grid.glsl'])['flags']['grid'])
        self.assertFalse(planner.plan(['native/ftebroom_plant_ui.h'])['flags']['plant_core'])
        self.assertFalse(planner.plan(['tests/plant-export-manager.cpp'])['flags']['plant_core'])
        self.assertTrue(planner.plan(['native/plant/botanical.h'])['flags']['plant_core'])

    def test_unknown_shared_change_fails_closed(self):
        p = planner.plan(['native/new_shared_allocator.h'])
        self.assertTrue(p['flags']['compilation'] and p['flags']['plant_core'])

    def test_manual_full_and_release_without_changes(self):
        self.assertEqual(set(planner.plan([], full=True)['suites']), planner.SUITES)
        p = planner.plan([], release=True)
        self.assertTrue(p['smoke']); self.assertFalse(p['runtime'])

    def test_ci_only_does_not_rebuild_editor_but_mixed_product_changes_do(self):
        files = ['.github/workflows/release-linux-ui-update.yml', 'scripts/test-compilation-runtime.py', 'scripts/ci-plan.py']
        self.assertFalse(planner.plan(files)['runtime'])
        self.assertTrue(planner.plan(files+['native/ftebroom_run.h'])['flags']['compilation'])

    def test_real_git_rename_crosses_subsystems_and_invalid_base_is_not_empty(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            def git(*args): return subprocess.check_output(['git', '-C', folder, *args], stderr=subprocess.DEVNULL).decode().strip()
            git('init', '-q'); git('config', 'user.email', 'ci@example.invalid'); git('config', 'user.name', 'CI')
            (root/'native').mkdir(); (root/'native/ftebroom_run.h').write_text('same')
            git('add', '.'); git('commit', '-qm', 'base'); base = git('rev-parse', 'HEAD')
            git('mv', 'native/ftebroom_run.h', 'native/ftebroom_audio.h'); git('commit', '-qm', 'rename')
            with patch('os.getcwd', return_value=folder):
                previous = os.getcwd(); os.chdir(folder)
                try:
                    changed = planner.changed_paths(base, 'HEAD')
                    self.assertEqual(set(changed), {'native/ftebroom_run.h', 'native/ftebroom_audio.h'})
                    self.assertTrue(planner.plan(changed)['flags']['audio_core'])
                    self.assertEqual(planner.changed_paths('HEAD', base), [])
                    self.assertEqual(set(planner.changed_paths('HEAD', base, release=True)), set(changed))
                    with self.assertRaises(subprocess.CalledProcessError): planner.changed_paths('missing-ref', 'HEAD')
                finally: os.chdir(REPO)


class CacheTests(unittest.TestCase):
    def test_windows_audio_uses_msys_bash_instead_of_system32_wsl(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); compiler = root/'mingw64/bin/gcc.exe'; shell = root/'usr/bin/bash.exe'
            compiler.parent.mkdir(parents=True); compiler.write_bytes(b'gcc')
            shell.parent.mkdir(parents=True); shell.write_bytes(b'msys bash')
            with patch.object(cache.shutil, 'which', return_value=str(compiler)):
                self.assertEqual(cache.audio_shell(windows=True), str(shell))
            with patch.object(cache, 'audio_shell', return_value=str(shell)), patch.object(cache.subprocess, 'run') as run:
                cache.build('audio', root/'source', root/'output')
                self.assertEqual(run.call_args.args[0][0], str(shell))
                self.assertEqual(run.call_args.kwargs['env']['RB_AUDIO_OUTPUT_DIR'], (root/'output').as_posix())

    def test_cached_profile_tests_include_transitive_application_headers(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); (root/'native/profile').mkdir(parents=True)
            (root/'scripts').mkdir(); (root/'tests/fixtures').mkdir(parents=True)
            for name in ('ci-dependency.py', 'build-profile-support.py'):
                (root/'scripts'/name).write_text('recipe')
            for name in ('trenchbroom-profiles.cpp', 'compilation-profile-core.cpp', 'engine-launch.cpp',
                         'profile-order.cpp', 'compilation-profile-probe.cpp',
                         'fixtures/CompilationProfiles-original.cfg', 'fixtures/CompilationProfiles-cpu-count.cfg'):
                (root/'tests'/name).write_text('#include "../native/engine.h"\n')
            (root/'native/engine.h').write_text('#include "profile/nested.h"\n')
            header = root/'native/profile/nested.h'; header.write_text('old')
            with patch.object(cache, 'REPO', root):
                first = cache.fingerprint('profile', root/'native/profile', 'test', tools={})
                (root/'native/engine.h').write_text('#include "profile/nested.h"\n// changed engine behavior\n')
                self.assertNotEqual(first, cache.fingerprint('profile', root/'native/profile', 'test', tools={}))
                dependencies = cache.quoted_dependencies([root/'tests/engine-launch.cpp'])
                self.assertIn('native/engine.h', dependencies)
                self.assertIn('native/profile/nested.h', dependencies)

    def test_header_toolchain_platform_and_flags_invalidate_exact_key(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); header = root/'header.h'; header.write_text('AAAA')
            def key(platform='Linux-x64', compiler='GCC1'):
                return cache.fingerprint('bullet', root, platform, tools={'compiler': compiler})
            original = key(); stat = header.stat()
            header.write_text('BBBB'); os.utime(header, ns=(stat.st_atime_ns, stat.st_mtime_ns))
            self.assertNotEqual(original, key())
            self.assertNotEqual(key(), key('Windows-x64'))
            self.assertNotEqual(key(), key(compiler='GCC2'))
            self.assertNotEqual(cache.fingerprint('bullet', root, 'Linux-x64', tools={}, destination=root/'one'),
                                cache.fingerprint('bullet', root, 'Linux-x64', tools={}, destination=root/'two'))
            before_flags = key()
            with patch.dict(os.environ, CXXFLAGS='-DNEW_ABI'):
                self.assertNotEqual(key(), before_flags)

    def test_output_content_tampering_missing_file_and_wrong_key_reject_cache(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); library = root/'lib.a'; library.write_bytes(b'compiled')
            stamp = {'key': 'exact', 'files': cache.inventory(root)}
            (root/'.ci-cache.json').write_text(json.dumps(stamp))
            self.assertTrue(cache.valid(root, 'exact'))
            self.assertFalse(cache.valid(root, 'other'))
            library.write_bytes(b'corrupt!'); self.assertFalse(cache.valid(root, 'exact'))
            library.unlink(); self.assertFalse(cache.valid(root, 'exact'))


class RunnerTests(unittest.TestCase):
    def run_plan(self, paths, full=False, fail=False):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); plan = root/'plan.json'; plan.write_text(json.dumps(planner.plan(paths, full=full)))
            (root/'runtime').mkdir()
            (root/'probe').write_bytes(b'process probe')
            argv = ['checks', '--plan', str(plan), '--runtime', str(root/'runtime'), '--output', str(root/'qa'),
                    '--probe', str(root/'probe'), '--platform', 'Linux-x64']
            with patch.object(sys, 'argv', argv), patch.object(runner.subprocess, 'run') as calls:
                if fail: calls.side_effect = subprocess.CalledProcessError(7, ['failed-check'])
                if fail:
                    with self.assertRaises(subprocess.CalledProcessError): runner.main()
                else: runner.main()
                return [list(map(str, c.args[0])) for c in calls.call_args_list]

    def test_ui_only_world_changes_prepare_fixtures_without_core_bsp_repeats(self):
        for paths, fixtures_only in [(['qc/fb_world_architect.qc'], True), (['native/world_architect/gates.h'], False)]:
            commands = self.run_plan(paths)
            for script in ['test-world-modules.py', 'test-world-gates.py']:
                standalone = [cmd for cmd in commands if any(Path(arg).name == script for arg in cmd) and '--applied' not in cmd]
                self.assertEqual(len(standalone), 1)
                self.assertEqual('--fixtures-only' in standalone[0], fixtures_only)
            self.assertTrue(any('--applied' in cmd and any(Path(arg).name == 'test-world-gates.py' for arg in cmd) for cmd in commands))

    def test_profiles_execute_import_migration_files_paths_real_compiler_and_launch_only(self):
        calls = self.run_plan(['native/ftebroom_run.h'])
        text = json.dumps(calls)
        for name in ('test-compilation-runtime.py', '--targeted', 'test-engine-cleanup.py',
                     'test-compilation-paths.py', 'test-mod-profile-lighting.py', 'test-compilation-ui.py'):
            self.assertIn(name, text)
        runtime_check = next(command for command in calls if any(arg.endswith('test-compilation-runtime.py') for arg in command))
        self.assertEqual(Path(runtime_check[runtime_check.index('--probe') + 1]).name, 'probe')
        self.assertNotIn('plant', text); self.assertNotIn('viewport-create', text)

    def test_infrastructure_runs_no_editor_and_failure_stops_immediately(self):
        self.assertEqual(self.run_plan(['scripts/ci-plan.py']), [])
        self.assertEqual(len(self.run_plan(['native/ftebroom_run.h'], fail=True)), 1)

    def test_suites_cannot_inherit_each_others_runtime_settings(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); runtime = root/'runtime'; runtime.mkdir()
            settings = runtime/'settings.cfg'; settings.write_text('pristine')
            (root/'probe').write_bytes(b'process probe')
            plan = root/'plan.json'
            plan.write_text(json.dumps(planner.plan(['native/ftebroom_run.h'])))
            argv = ['checks', '--plan', str(plan), '--runtime', str(runtime),
                    '--output', str(root/'qa'), '--probe', str(root/'probe'), '--platform', 'Linux-x64']
            def mutate_private_runtime(command, **kwargs):
                if any(arg.endswith('make-linux-build-fixture.py') for arg in command):
                    (runtime/'fixture.cfg').write_text('prepared')
                    return
                script = next(i for i, arg in enumerate(command) if str(arg).startswith(str(REPO/'scripts')))
                private_settings = Path(command[script+1])/'settings.cfg'
                self.assertNotEqual(private_settings, settings)
                self.assertEqual(private_settings.read_text(), 'pristine')
                self.assertEqual(private_settings.with_name('fixture.cfg').read_text(), 'prepared')
                self.assertEqual((private_settings.parent/'tools/compilation-profile-probe').read_bytes(), b'process probe')
                private_settings.write_text('state left by an earlier suite')
            with patch.object(sys, 'argv', argv), patch.object(runner.subprocess, 'run', side_effect=mutate_private_runtime) as calls:
                runner.main()
                self.assertGreater(calls.call_count, 1)
            self.assertEqual(settings.read_text(), 'pristine')


class PackageGateTests(unittest.TestCase):
    def fixture(self, root, platform, source_splash=b'splash'):
        version = 'ci-test'; windows = platform == 'Windows-x64'
        root_name = 'ReyondBroom-'+('Windows-x64' if windows else 'Linux-x64')
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
        info = f'ReyondBroom {version}\nPlatform: {platform}\nEditor revision: {revision}\n'
        files = {'ftebroom/csaddon.dat': b'qc', 'ftebroom/ui/update-splash.png': b'splash', 'build-info.txt': info.encode(),
                 'bin/fteqw64.exe' if windows else 'fteqw-gl64': b'engine',
                 'fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so': b'plugin'}
        files['distribution-manifest.json'] = json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in files.items()}).encode()
        for name, body in files.items():
            p = root/root_name/name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(body)
        archive = root/('ReyondBroom-'+version+'-'+platform+('.zip' if windows else '.tar.gz'))
        if windows:
            with zipfile.ZipFile(archive, 'w') as z:
                for name, body in files.items(): z.writestr(root_name+'/'+name, body)
        else:
            with tarfile.open(archive, 'w:gz') as t:
                for name, body in files.items():
                    entry = tarfile.TarInfo(root_name+'/'+name); entry.size = len(body); t.addfile(entry, io.BytesIO(body))
        source = root/('ReyondBroom-'+version+'-'+platform+'-Sources.zip')
        with zipfile.ZipFile(source, 'w') as z:
            z.writestr('RELEASE-BUILD.txt', info)
            for name in ('sources/fteqw-source.zip', 'sources/bullet-source.zip', 'sources/imgui-source.zip', 'sources/ftebroom/native/ftebroom.cpp'):
                z.writestr(name, 'fixture')
            z.writestr('sources/ftebroom/runtime/ui/update-splash.png',source_splash)
        for p in (archive, source):
            p.with_name(p.name+'.sha256').write_text(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n')
        return version, archive

    def test_different_packaged_and_source_splash_is_rejected(self):
        for platform in ('Linux-x64','Linux-SDL2-x64','Windows-x64'):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as folder:
                root=Path(folder);version,_=self.fixture(root,platform,source_splash=b'stale artwork')
                with self.assertRaisesRegex(AssertionError,'Splash artwork differs'):smoke.verify(root,platform,version)

    def test_each_platform_archive_and_source_gate_rejects_tampering(self):
        for platform in ('Linux-x64', 'Linux-SDL2-x64', 'Windows-x64'):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as folder:
                root = Path(folder); version, archive = self.fixture(root, platform)
                smoke.verify(root, platform, version)
                archive.write_bytes(archive.read_bytes()+b'changed')
                with self.assertRaises(AssertionError): smoke.verify(root, platform, version)

    def test_wrong_version_and_extracted_binary_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); version, _ = self.fixture(root, 'Linux-x64')
            (root/'ReyondBroom-Linux-x64/fteqw-gl64').write_bytes(b'stale')
            with self.assertRaises(AssertionError): smoke.verify(root, 'Linux-x64', version)


class PreparedReuseTests(unittest.TestCase):
    def test_only_test_driver_steps_can_change_without_rebuilding(self):
        original=(REPO/'.github/workflows/compilation-profile-checks.yml').read_text()
        prepared.verify_recipes(original, original.replace('Exercise actual Windows controls with OS input','Focused input reproduction'))
        jobs=prepared.build_jobs(original)
        changed_job=jobs['windows'].replace('-O1', '-O2', 1)
        self.assertNotEqual(changed_job,jobs['windows'])
        changed=original.replace(jobs['windows'], changed_job)
        with self.assertRaises(AssertionError): prepared.verify_recipes(original, changed)

    def test_fixture_provenance_rejects_changed_inputs_and_path_escape(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo=Path(temporary);(repo/'native').mkdir();source=repo/'native/core.h';source.write_text('first\nline\n')
            fixture=repo/'fixture';fixture.mkdir();manifest=fixture/'provenance.json'
            manifest.write_text(json.dumps({'sourceHashes':{'native/core.h':hashlib.sha256(source.read_bytes()).hexdigest()}}))
            prepared.verify_fixture(fixture,repo)
            source.write_bytes(b'first\r\nline\r\n');prepared.verify_fixture(fixture,repo)
            source.write_text('changed')
            with self.assertRaises(AssertionError): prepared.verify_fixture(fixture,repo)
            manifest.write_text(json.dumps({'sourceHashes':{'../other':'invalid'}}))
            with self.assertRaises(AssertionError): prepared.verify_fixture(fixture,repo)


class WorkflowTests(unittest.TestCase):
    def test_gates_and_caches_are_wired_without_fuzzy_restores(self):
        import yaml
        def read(name): return yaml.safe_load((REPO/'.github'/name).read_text())
        composite = read('actions/build-dependency/action.yml')
        restore = next(s for s in composite['runs']['steps'] if s.get('uses') == 'actions/cache/restore@v4')
        self.assertNotIn('restore-keys', restore['with'])
        for file in ('release-linux-ui-update.yml', 'compilation-profile-checks.yml'):
            wf = read('workflows/'+file)
            self.assertIn('plan', wf['jobs'])
            for job in ('linux', 'windows'):
                self.assertEqual(wf['jobs'][job]['needs'], 'plan')
                if file.startswith('release'):
                    gate = next(s for s in wf['jobs'][job]['steps'] if s.get('name') == 'Validate every packaged release (short smoke)')
                    self.assertNotIn('if', gate)
                else:
                    self.assertIn('needs.plan.outputs.runtime', wf['jobs'][job]['if'])
            dispatch = wf['on']['workflow_dispatch']['inputs']
            self.assertEqual(dispatch['validation_mode']['options'], ['affected', 'full'])


if __name__ == '__main__':
    unittest.main()
