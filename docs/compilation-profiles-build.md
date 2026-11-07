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

## VKQuake / Ironwail launch (REYON-8)

For a Quake 1 map, open **Run > Launch Engine > Configure engines** (also
available from a Launch Engine task in Compile). The **VKQuake** and **Ironwail**
buttons add editable templates. Choose the installed executable, set the game
root in Compilation settings, and use the map's Mods list for the active mod.
The game root contains `id1` and the mod directory; it is independent of the
engine executable directory. Quote paths containing spaces in Arguments.

Each engine profile now saves **Working dir**, **Arguments** and optional
**Compiled map**, alongside its name and executable. A blank working directory
uses the executable directory; `${ENGINE_DIR_PATH}` makes that explicit. The
Quake template uses `-basedir "${GAME_DIR_PATH}" -game "${MODS[-1]}" +map
"${MAP_BASE_NAME}"` and `${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp`. WORK_DIR_PATH is
the selected compilation profile's expanded working directory, including when
using Launch Engine separately. Relative engine/work/map paths use the editor
application directory. Existing profiles without these fields retain their
commands and deployment workflow. TrenchBroom profile import is preserved.

Add a **Launch Engine** task after your export/compiler/light tasks and select
this engine profile. A nonempty Compiled map enables Quake 1 BSP (29/BSP2) header
validation and copies the BSP plus available `.lit`, `.lux`, `.ent` sidecars to
the `-basedir` / `-game` / `maps` destination named by `+map`. Missing sidecars
remove stale overrides for that output map. Use a filename without spaces for
the map itself, as required by Quake's console; parent directories may contain
spaces/Unicode. Missing executables/workdirs/maps and incompatible or truncated
BSPs stop the launch with a diagnostic. The game installation must supply its
own licensed data. For special mission-pack switches or an existing Copy task,
leave Compiled map blank and manage deployment explicitly.

The log distinguishes compilation completion, successful process creation and
the engine's eventual exit status. A nonzero game exit is reported even after
the compiler has finished. Stop compilation does not kill a separately launched
game. Test mode interpolates/tokenizes the command without copying/launching.

`scripts/test-engine-viewport.py` performs an actual compiler -> BSP deployment
-> OS process test using explicitly labelled argv probes, and optionally a real
installed engine via `--engine`, `--game-root`, `--mod`. This distinction matters:
an argv probe is not evidence that a game renderer loaded the map.

## Viewport colors (REYON-8)

Open **View > Preferences > Colors**. **Brush edges** controls ordinary brush
wire outlines in XY/XZ/YZ and the 3D edge passes (including retained GPU meshes).
Selected, hovered and locked geometry keep their own color controls. Hidden
geometry stays hidden. Color changes apply immediately without a geometry
rebuild. **2D viewport background** defaults to `#303238` in both Modern themes.
The shared preferences file stores explicit custom choices; startup/default
registration and theme changes do not reset them. Grid and axes remain visible.

The native update packager includes the current GLSL programs as well as CSQC
and the host module. Updating only the module would leave the old 3D colors.

## Profile migration

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
