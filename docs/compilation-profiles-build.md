# Compilation profiles (REYON-6)

The editor loads a Qt Widgets Compile dialog into its own process through a small UTF-8 JSON C ABI. The editor owns the profile model, persistence, file operations and child processes. Windows uses the existing MinGW host and the official Qt MSVC kit; C++ objects never cross this boundary.

The compatibility core vendors the unmodified EL, argument parsing, path matching and disk operations from TrenchBroom 2026.2, commit `4c756be1a54a28d921d18c77716a5f170bdc31d0`. Qt controls, SVGs, stylesheet, palette and layout constants use that same reference. Import remains available above the Profiles mini toolbar.

## Build

Use GCC with C++20 support for the static core (the existing host stays C++17), CMake 3.20+, and Qt **6.10.3** Widgets/Svg. Linux additionally needs the official Qt ICU archive, OpenGL/X11 development headers and `patchelf`; Windows UI uses Visual Studio 2022.

```sh
python -m pip install aqtinstall==3.3.0
python -m aqt install-qt linux desktop 6.10.3 linux_gcc_64 -O qt --archives qtbase qtsvg icu
export REYOND_QT_ROOT="$PWD/qt/6.10.3/gcc_64"
scripts/build-linux.sh /absolute/dependencies
```

For Windows, install `win64_msvc2022_64` with `qtbase qtsvg`, set `REYOND_QT_ROOT` to `qt/6.10.3/msvc2022_64`, then run `scripts/build-windows.ps1` using the existing MinGW dependencies. The script builds the UI with Visual Studio and the core/host with MinGW.

For an existing runtime:

```sh
python scripts/build-profile-support.py core --build-dir build/profile --tests
python scripts/build-profile-support.py ui --build-dir build/compile-ui --qt-root "$REYOND_QT_ROOT"
python scripts/build-profile-support.py deploy --build-dir build/compile-ui --qt-root "$REYOND_QT_ROOT" --output /disposable/runtime
```

Link `build/profile/libreyond-profile.a` into the native host. Rebuild `csaddon.dat` from current QC sources and install it as `ftebroom/csaddon.dat`; this supplies the per-map mod list and its undoable editor command. A native-module-only update using the old CSQC cannot provide that API.

Deploy the entire `tools/compile-ui` directory and `ftebroom/ui/compilation` resources. Qt is dynamically linked and remains replaceable. Preserve third-party licenses and distribute matching Qt/ICU sources with release sources; the source URLs and exact versions are recorded in `tools/compile-ui/SOURCE-CODE.txt`. Do not distribute an incomplete component containing only the host module.

## Migration

The canonical store is `ftebroom/compilation-profiles.json`, version 1, with independent records keyed by game. The editor writes through a temporary file and atomic replacement. Empty and duplicate profile names are valid; an empty list stays empty. Bundled presets are available through Compilation settings and are not automatically inserted.

On first use without the new store, saved records in `ftebroom-run-profiles.txt` are migrated. The original and a `.legacy.bak` copy remain. Names prefixed `Q1:`, `Q2:`, `HL:`, `Q3:` migrate to that game; unscoped profiles migrate to the active game. Imported/new profiles use TB semantics. Migrated bundled build adapters preserve their previous export/preview hooks and legacy variables.

Restore the original legacy file only together with removing the new store if deliberately rolling back migration. Do not modify or delete user data as part of verification.

## Verification

`native/profile` includes the upstream EL test corpus, original customer profile import/roundtrip, and file/export tests. `tests/compilation-parity-native.cfg` runs real child-process, Unicode file, recursive delete, no-op Test, Stop, engine ID, mod-array and per-game persistence checks inside the editor.

```sh
# Copy the built probe into runtime/tools first.
xvfb-run -a -s '-screen 0 1920x1080x24' python scripts/test-compilation-runtime.py /disposable/runtime qa
```

The runner owns and clears profile state in that disposable runtime. It preserves logs, JSON results and screenshots/geometry of the real Compile dialog at 800×600 and 1200×800, light/dark. Run separately with `QT_SCALE_FACTOR=1`, `1.5`, `2` for DPI comparisons. Compare to the same TB version on the same OS, font, style and DPR; geometry artifacts do not by themselves establish visual identity.

The `Compilation profile parity` workflow builds and runs Windows x64, Linux native and Linux SDL2. It does not publish a release. Acceptance status and remaining differences belong in the PR and Jira evidence, rather than being inferred from a successful build.
