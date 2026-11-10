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


class ScopeTests(unittest.TestCase):
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
            argv = ['checks', '--plan', str(plan), '--runtime', str(root/'runtime'), '--output', str(root/'qa'),
                    '--probe', str(root/'probe'), '--platform', 'Linux-x64']
            with patch.object(sys, 'argv', argv), patch.object(runner.subprocess, 'run') as calls:
                if fail: calls.side_effect = subprocess.CalledProcessError(7, ['failed-check'])
                if fail:
                    with self.assertRaises(subprocess.CalledProcessError): runner.main()
                else: runner.main()
                return [list(map(str, c.args[0])) for c in calls.call_args_list]

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
            plan = root/'plan.json'
            plan.write_text(json.dumps(planner.plan(['native/ftebroom_run.h'])))
            argv = ['checks', '--plan', str(plan), '--runtime', str(runtime),
                    '--output', str(root/'qa'), '--probe', str(root/'probe'), '--platform', 'Linux-x64']
            def mutate_private_runtime(command, **kwargs):
                script = next(i for i, arg in enumerate(command) if str(arg).startswith(str(REPO/'scripts')))
                private_settings = Path(command[script+1])/'settings.cfg'
                self.assertNotEqual(private_settings, settings)
                self.assertEqual(private_settings.read_text(), 'pristine')
                private_settings.write_text('state left by an earlier suite')
            with patch.object(sys, 'argv', argv), patch.object(runner.subprocess, 'run', side_effect=mutate_private_runtime) as calls:
                runner.main()
                self.assertGreater(calls.call_count, 1)
            self.assertEqual(settings.read_text(), 'pristine')


class PackageGateTests(unittest.TestCase):
    def fixture(self, root, platform):
        version = 'ci-test'; windows = platform == 'Windows-x64'
        root_name = 'ReyondBroom-'+('Windows-x64' if windows else 'Linux-x64')
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
        info = f'ReyondBroom {version}\nPlatform: {platform}\nEditor revision: {revision}\n'
        files = {'ftebroom/csaddon.dat': b'qc', 'build-info.txt': info.encode(),
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
        for p in (archive, source):
            p.with_name(p.name+'.sha256').write_text(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n')
        return version, archive

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
