# Compilation profiles (REYON-6)

Compile, Launch Engine, Game Engines, settings and import use the editor's existing **ImGui** backend. No Qt SDK, Qt library or additional UI process is built, loaded or packaged.

The user approved matching TrenchBroom's layout, sizes, spacing, icons, density and interactions with ImGui rendering. Pixel equality, Qt system controls and Qt fonts are explicitly outside acceptance. The 2026.2 reference is pinned to `4c756be1a54a28d921d18c77716a5f170bdc31d0`.

The compatibility core vendors the original TrenchBroom EL, argument parsing, path matching and disk operations. The editor owns the model, persistence, export and process lifecycle. Profile import remains above the Profiles mini toolbar.

## Build

Use GCC with C++20 support and CMake 3.20+ for the static core. The existing native host stays C++17. Windows continues using MinGW; no MSVC or Qt kit is introduced.

```sh
python scripts/build-profile-support.py core --build-dir build/profile --tests
```

Link `build/profile/libreyond-profile.a` into the host. The normal Linux/Windows build scripts and native-update workflow do this. Rebuild `csaddon.dat` and install it as `ftebroom/csaddon.dat`; it supplies the per-map mod list and its undoable editor command. Updating only the host with old CSQC cannot provide that API.

SDL2 also requires rebuilding the engine with `bash scripts/build-sdl2-engine.sh /dedicated/fteqw`. Its SDL window-close event must pass through the same editor confirmation as native Linux and Windows. The native-update workflow supplies this rebuilt executable and corresponding patched engine sources to the packager; it rejects an SDL2 package without them.

## Migration

The canonical store is `ftebroom/compilation-profiles.json`, version 1, with independent records keyed by game. Writes use a temporary file and atomic replacement. Empty and duplicate profile names are valid; an empty list stays empty. Bundled presets are available through Compilation settings and are not inserted automatically.

On first use without the new store, saved records in `ftebroom-run-profiles.txt` are migrated. The original and a `.legacy.bak` copy remain. Names prefixed `Q1:`, `Q2:`, `HL:`, `Q3:` migrate to that game; unscoped profiles migrate to the active game. Imported/new profiles use TB semantics. Legacy build adapters preserve export/preview hooks and legacy variables.

To deliberately roll back migration, restore the legacy file together with removing the new store. Verification must use disposable copies, not user data.

## Observed reference behavior

TrenchBroom 2026.2's Delete Files runner stops its deletion batch on an individual deletion error but still continues to the next task. A real readonly-folder experiment confirmed this behavior: the protected file remained, the following process ran, and no deletion failure was printed. The compatible runner preserves that behavior. Missing-directory/traversal errors still fail the task. Core tests include this distinction.

## Verification

`native/profile` includes the upstream EL corpus, original customer profile import/roundtrip and file/export tests. `tests/compilation-parity-native.cfg` exercises actual child processes, Unicode files, recursive delete, no-op Test, Stop, engine IDs, mod arrays and per-game persistence inside the editor.

```sh
# Copy the built compilation-profile-probe executable into runtime/tools first.
xvfb-run -a -s '-screen 0 2560x1800x24' python scripts/test-compilation-runtime.py /disposable/runtime qa
```

The runner owns and clears profile state in that disposable runtime. Artifacts include logs, JSON results, actual editor screenshots and measured control geometry at 800×600 / 1200×800 in light/dark themes and 100/150/200% editor scaling. Screenshot dimensions are recorded honestly when a display constrains the requested window size.

The `Compilation profile parity` workflow builds and runs Windows x64, native Linux and Linux SDL2. It does not publish a release. Acceptance coverage, manual comparisons and remaining differences are reported separately from build success.

For an OS-input-only Windows reproduction, dispatch the same workflow with
`prepared_run_id` set to a run containing `compilation-Windows-editor`. It verifies
that native, CSQC and probe sources match the artifact before reusing those binaries.
This allows changing the OS input driver without rebuilding unchanged C++ each time.
The reproduction has a separate concurrency group and does not cancel the full run.
