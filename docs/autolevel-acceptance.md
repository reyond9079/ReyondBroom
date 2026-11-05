# AutoLevel acceptance — alpha 119

The active bundled generator is now **AutoLevel**, plugin ID
`org.reyondbroom.autolevel`. The former levelgen plugin is excluded from new
runtime packages and hidden when AutoLevel is installed. Its source and old data
remain available for migration and compiled v1 compatibility checks.

The implementation was checked against AutoLevel upstream revision
`f27c97ecab604168f7e4dad1d6a4669481edee36` and the supplied November 5 specification.
It independently implements the algorithms in C++17, with the upstream MIT notice.
No Unity runtime, downloaded C# source tree or task specification is shipped.

## Requirement traceability

The [requirement → implementation → automated test table](autolevel-acceptance/requirements.csv)
covers every explicit mandatory item, the repository compilation steps, and the
remaining functional sections. The optional host SDK extension was committed
separately before the plugin. Its v1 offsets and API number are preserved.

## Automated results

All **12 test executables plus the pre-extension binary compatibility test** pass
on both Windows x64 (Windows 11 Pro, MinGW GCC 15.2) and Linux x64 (Ubuntu 24.04
under WSL2, GCC 13.3). The Linux results establish Linux compilation/execution;
they are not a claim of a native Arch/Wayland desktop test.

- [Windows machine-readable results](autolevel-acceptance/windows-results.json)
- [Linux machine-readable results](autolevel-acceptance/linux-results.json)
- [Windows default metrics and seed-1 ASCII floors](autolevel-acceptance/defaults-windows.txt)
- [Linux default metrics and seed-1 ASCII floors](autolevel-acceptance/defaults-linux.txt)
- [Physical geometry results, seeds 1–30](autolevel-acceptance/geometry-windows.txt)
- [Windows UI lifecycle checks](autolevel-acceptance/ui-windows.txt) and [Linux UI checks](autolevel-acceptance/ui-linux.txt)
- [Actual Windows editor assertions](autolevel-acceptance/runtime-windows.txt)

Default 8×8×3, single-threaded, seeds **1–30**:

- Accepted: **30/30**. Mean attempts: **2.0**.
- Mean largest-component coverage: **79.44%**. All accepted layouts reach all three floors.
- Mean vertical modules: **11.63**, minimum **7**.
- At least one hall or atrium: **100%**. Mean corridor fraction: **44.17%**.
- Maximum brushes: **2427**, below the 4000-brush acceptance limit.
- Median solver + decoration + MAP construction: **198.953 ms Windows**, **99.4363 ms Linux**,
  below the 1500 ms requirement. These are CPU generation timings, not import latency or FPS.

The geometry run independently checks all 30 layouts: valid finite convex brushes,
no overlapping brush volume above one cubic unit, sealed 8-unit voxel flood fill,
and traversal with a conservative 32×32×56 player hull and 16-unit steps (below
18). It checks ordinary cell centers and physical doorway/gallery landings for
large modules. Profile-specific starts and Half-Life light properties are also checked.

The solver tests include a deliberately contradictory non-commuting 2×2 fixture:
full ban/support rollback must recover the unique valid fixed point. They also
cover seeded ST/MT equality, partial rebuild borders, failed-group atomicity,
concurrent independent builders, wave constraints, four Big Block orientations,
upper-layer eligibility and active cancellation after 10 ms with acknowledgement
below 100 ms. AddressSanitizer, UndefinedBehaviorSanitizer and leak checking cover
solver, UI and viewport lifecycles.

UI tests drive all five pages with a fake host, enforce main-thread host ownership
and balanced disabled scopes, generate more than 100 preview segments, import more
than 10 KiB with groups/player, capture modules, persist/reload, invalidate stale
jobs, cancel, and protect corrupt files from autosave. Viewport ray tests cover
Move/Scale/Rect, snapping and RMB restoration, fill corners, connections, ban/
exclusive keys, lower-layer dependencies, Big Block cell picking, Escape and old-host
fallback. Projection round trips cover perspective and all three orthographic views.

Automated Windows editor execution loads the actual external DLL, generates and
exports a level, imports over 1000 editable grouped brushes, and verifies that one
Undo restores the original brush count and Redo restores the entire result. It
also checks that plugin clicks/keys do not reach ordinary brush tools and that
Escape/window close releases capture. Import remains synchronous in the existing
editor; the old quadratic string assembly/character scanning was removed so these
multi-megabyte fragments complete instead of appearing indefinitely hung.

The generated seed-30 geometry compiles without a leak through bundled Quake 1
`qbsp -bsp2` and Quake III `q3map2 -meta`. Quake 1 light compilation also completes
with no empty lightmaps. This checks compiler integration, not manual gameplay.
The Q3 compiler's high structural-geometry percentage warning is expected for
editable brush output.

The human checklist in specification §15.8 was deliberately omitted as requested.
It is not represented as an automated or completed manual test.

## Contract clarifications and limits

- The specification includes actions in persistent hashes but also requires
  symmetric rotations to collapse. Persistent hashing and semantic deduplication
  are separate, satisfying stable saved hashes and the symmetric-cross test.
- Missing author layers are rejected before built-ins are injected; otherwise
  built-ins conceal the required missing-layer diagnostic.
- Empty upper headroom cells in a staircase have no standable center, just as
  atrium openings do. Traversal checks their real lower/upper door landings;
  atrium shafts are not treated as climbable vertical portals. This avoids a
  false graph-level pass without a physical path.
- Hall and stair author weights were tuned in the permitted 20% increments to
  **4.32** and **8.64**; atrium remains **6**. Tower applies a separate persistent
  **1.5× stair-only multiplier**, without changing other builders or the repo.
- Plugin overlays use the background ImGui layer so they remain behind floating
  windows; the specified foreground layer would draw through the plugin panel.
  Gizmos are screen overlays without scene depth testing, as required by SDK v1.
- Inputs are convex Standard/Valve 220 MAP brushes and point entities. Patch and
  brushDef inputs are rejected. No Unity prefab/FBX importer is implied.
- Preview is limited to 16384 segments, falling back to the current layer with a
  warning. This never removes full-resolution geometry from the MAP export.
- Limits remain 8 MiB per stored file, 512 block assets, 4096 compiled states,
  31 groups, 16 layers, 32768 cells, 512 MiB support storage, two million journal
  events, 16000 output brushes and 16 MiB per import. Large or contradictory
  custom repositories can reach a budget and report an error. Quality outside
  the validated seed set is a target: a valid lower-coverage result is explicitly
  labeled as such and is not confused with a contradiction.

## Reproduce

Run `python scripts/test-autolevel.py --output <scratch> --geometry-seeds 30`.
On Windows, supply `--compiler <mingw-bin>/g++.exe`. Optionally provide
`--legacy-plugin <alpha118-levelgen.dll-or-so>` for the binary compatibility check.
The test runner writes per-executable logs and `results.json`; it does not touch
user maps, WADs or configured mods. See [AutoLevel guide](autolevel.md) for use.

Actual packaged-editor automation: `python scripts/test-autolevel-runtime.py
--bundle <unpacked-runtime> --output <new-private-directory>`. On Linux add
`--headless` to use Xvfb and software OpenGL. It copies the clean runtime,
uses bundled editor resources, and leaves all existing installations untouched.
