# RB-10 — alpha.132 acceptance

RB-10 contains 23 requirements from Jira comment 10064. This record maps them to
repeatable checks. Local results below are development validation; release
publication additionally requires the exact-source packaged checks on Windows,
native Linux and Linux SDL2 described below.

## Requirement evidence

| ID | Result | Local validation / packaged regression |
| --- | --- | --- |
| O1 | Every onboarding stage reloads its disposable lesson scene. | Tutorial QC tests and native stage5/map-ready check. |
| O2 | Demonstrations restore complete brush state or remove newly created brushes, without leaving a Redo branch. | Seven tutorial assertions, including geometry/material/selection and idempotence. |
| O3 | Grid stage displays an animated mouse-wheel indicator. | Native stage check compares wheel phases. |
| B1 | Right-click Create works on files and empty grid space; New button removed. | Actual OS browser acceptance. |
| B2 | Highlighted breadcrumbs navigate ancestors; trailing space enables full-path editing. | Disk/archive path unit tests and actual OS navigation/editing. |
| B3 | Bottom-right magnifier/compact size control; hover-wheel zoom in files/entities/materials. | Actual OS browser acceptance, with persisted/clamped thumbnail sizes. |
| W1 | One Welcome shows updates, recovery, then recent maps. | Actual OS Welcome acceptance; Keep editing and Recover branches. |
| W2 | File → Open shows recent maps; Open another map opens browsing. | Actual OS Welcome acceptance. |
| W3 | Map chooser shares breadcrumbs and Up/Refresh navigation with Mod manager. | Actual OS chooser and editable-path checks. |
| W4 | Version offer, real byte progress and mandatory restart; committed close exits editor. | Production updater fixtures, OS progress/restart/close, dirty-map Cancel, write-error and no-premature-token checks. |
| P1 | Visible model thumbnails rasterize once and reuse a bounded cache. | Offscreen zero reads, one generation/frame,128-entry eviction;240-model OS folder test and paired timing. |
| G1 | Construction-grid extent uses world distance independently of cell size. |20,000-brush paired benchmark below; surface-grid distance remains world-based. |
| V1 | View Options button at top-right of3D views; corresponding TrenchBroom visibility/rendering controls. |29 new +22 existing QC filter assertions; OS controls/persistence; compact base panel at1920. |
| T1 | Cold viewport Create opens Plant Editor and preserves the captured world placement. | OS native-target test and existing40-case viewport Create/Save suite. |
| T2 | Curved leaf silhouettes, natural pigments/bark, distinct flower and mushroom structures. |121-presets/three-seeds/three-LODs matrix, exact legacy corpus, native app samples; complete packaged catalogue capture. |
| T3 | FTE foliage survives distance through coverage-preserving mipmaps. | Real FTE texture loader and rendered crown checks at250/1600/3200 units. |
| T4 | Q1 MDL/Q2 MD2+PCX/Q3 MD3 with target limits and explicit game-code capability gates. |242 alias-codec roundtrips, independent vanilla MD2 GL-command reader, dense MD3 limits and real q3map2 bake. |
| L1 | New/reset Tools dock horizontally above the scene; custom docks retained. | Native workspace tests, actual OS layouts and standalone ImGui dock test. |
| L2 | Horizontal Tools wraps, grows and shrinks with its content. | OS height/overflow checks; standalone floating self-resize and manual-orientation regression. |
| E1 | Entity browser offers point entities only. | Actual OS browser class-list check. |
| G2 | Horizontal construction plane is off by default, independently of2D/surface grids. | Fresh profile, OS toggle and restart-persistence checks. |
| C1 | Face clipboard transfers material, all projection axes/offsets, flags/value and contents to faces/whole brushes. |12 real-runtime checks, unchanged planes/selection and one-step Undo/Redo. |
| K1 | Default keys are valid and conflict-free; custom conflicts remain diagnosable and explicitly restorable. |333-action audit using FTE's actual key parser and native conflict/restore functions. |

## Local results and performance

The combined final-QC run passed82 assertions: tutorial7, clipboard12,
View Options29 plus22 previous filter assertions, and native guards/Tools/tour12.
All production QC files were compared byte-for-byte with the compiled snapshot.
The actual OS browser suite passed11 functional assertions; Welcome passed20
OS assertions plus16 native expectations after the cancelled-restart fix.
Native plant target UI passed cold creation, a1200-unit tree, normalization,
source links and one-step Undo/Redo. The previous viewport Create/Save suite
also passed all40 cases.

The botanical matrix passed11,152,161 checks across1,089 preset/seed/LOD cases,
with270 exact legacy geometry cases and legacy texture hashes. Core Plant Editor
checks passed1,160 assertions. Actual generator2 GUI samples include oak, maple,
birch, rose, poppy, tulip, Amanita, boletus and morel. Fixed-camera crown coverage
was retained or increased (oak+1.7%, maple+50.7%, birch+8.5%, pine unchanged).

Model-preview comparison compiled the original and current production headers
against the same ImGui and9 visible models of2,048 triangles each. Across100
paired submissions, median CPU work fell from196.727µs to1.562µs; submitted
vertices fell from55,296 to36. The warm-cache OS test with240 models measured
28.0ms for both the empty and model folder in a quiet software-rendering run,
with9 cached visible images and no rereads/rebuilds. Earlier timings while other
software-rendered editors ran concurrently were excluded from this paired result.
Cold generation remains bounded and visible-only; one worker and a128-entry LRU
prevent folder size from becoming per-frame triangle work.

Construction-grid benchmark used20,000 brushes,1280×800, llvmpipe, an identical
native module, map and camera, with only the old/new construction-plane QC
implementation differing. Each case measured eight seconds stationary and eight
seconds moving after loading/warm-up:

| Grid | Previous idle/moving FPS | World-distance idle/moving FPS |
| --- | --- | --- |
|8 |22.2804 /21.2288 |21.5536 /21.1988 |
|64 |19.8518 /19.6553 |19.6087 /19.6722 |

The mean-FPS difference was0–3.3%, so the equal-distance change was retained.
Raw percentile timings are in the benchmark report; these software-renderer
numbers do not predict a user's GPU FPS. A single analytical quad replaces the
old32-cell line extent. Subpixel lines coarsen in the shader without changing
snapping. The plane is disabled by default; surface and2D grids stay independent.

## Plant compatibility

Q1 plant alias geometry stays within1,024 vertices/2,048 triangles, including
backfaces, with a256×256 palette skin. Q2 uses at most2,048 vertices/4,096
triangles and a native PCX skin. Both use opaque geometric foliage rather than
assuming vanilla alpha-cutout support. MD2 includes a terminated, bounded greedy
GL strip stream as required by original Quake II's OpenGL renderer, alongside
the software-renderer triangle table. An independent reader verifies pixel-centre
UVs, indices, stream bounds and the oriented triangle multiset.

Q3 uses MD3 and conventional texture/shader assets, checked against surface and
per-surface limits. A generated oak's31,994 triangles were baked by actual
q3map2. FTE uses RGBA DDS with explicit per-tile alpha-coverage mipmaps; TGA
compiler/editor companions remain available. Existing TGA shader bytes for other
authoring models are preserved. Scene FTE opt-in does not change immutable asset
bytes or IDs. Tall MD3 normalization is restored by entity scale and survives
Undo/Redo.

Vanilla Q1/Q2 game code cannot spawn arbitrary authoring `func_model` entities.
Native file export works, while compilation retains the existing explicit
compatible-game-code gates. Q1's FTE static-model extension requires opt-in.
This change does not add a vanilla game-code model spawner or a new BSP converter.

## Shortcut handling

No default binding was changed: the baseline already separates Model Editor
focus from map shortcuts. All333 generated actions were checked with FTE's real
key-name table/parser and the editor's actual conflict detector. Injected custom
conflicts remain detectable. Restoring one binding preserves another override
and stable action IDs. Per-binding/all-default restoration is explicit; saved
user overrides are never silently discarded.

## Required release gates and provenance

The additional Quake II report is covered by a custom-named mod fixture with
PAK/WAL resources and a sibling `baseq2`. Choosing a pending profile in Preferences
and opening Game / resources uses the normal guarded profile switch, including
Save/Discard/Cancel. The resource dialog must display the selected game's fields,
not the old Quake III fields or a stale `baseq3` error. Resource commands update
the active game's directory. A missing legacy-game settings file
recovers the last successful resource path; any existing file remains authoritative.
Both updater implementations preserve `reyondbroom-legacy-games.txt`. Browsing
PAKs tolerates official Q2 non-canonical aliases while reads reject traversal
and archive mutations retain strict validation.

Quake engine launch also accepts the exact already-deployed, valid BSP when a
TrenchBroom profile has copied it into the selected game's mod/maps directory
and deleted the temporary build output. An existing invalid build BSP still
fails validation; a missing or invalid deployed BSP also fails. This preserves
the user's copy/cleanup stages and deployed lighting files. Custom engine
commands without automatic map staging keep their exact parameters.

`WORK_DIR_PATH` retains TrenchBroom semantics: the compilation profile's
interpolated Working directory. `${MAP_DIR_PATH}` refers to the source map's
directory. The chosen profile index now survives a workspace restart, so an
unrelated first profile cannot silently replace its working directory. Tests
exercise actual workspace serializers and atomic writes, including duplicate
profile names, per-game selections and older stores without a selection.
Maps loaded directly from mounted game folders resolve their actual read source
instead of an editor-local writable path. Compilation and engine launch require
a saved map, as in TrenchBroom, instead of treating an untitled map's empty
directory as the editor process directory. Saved profiles retain explicitly
empty or relative working directories without rewriting them.

`prepare-rb10.yml` runs when the PR is marked ready. It requires exact-head
`compilation-profile-checks.yml` success for `windows`, `linux (Linux-x64)` and
`linux (Linux-SDL2-x64)`. It reserves draft `v0.5.0-alpha.132`, verifies an immutable
`rb-10-package-<full-head-SHA>` source ref and dispatches packaging against it.
All runtime/dependency bases are verified `v0.5.0-alpha.131` assets.

An empty unpublished draft whose package run has finished can be retried after
test, workflow or QA-documentation repairs. The previous pinned source must be
an ancestor, its platform CI must have passed, and production files must be
unchanged. Draft packaging can then overlap the new revision's CI. Each retry
gets a new immutable source ref; earlier source refs remain unchanged. The final
publication gate still requires successful CI and packaging for the exact final
PR head, identical merged trees and all twelve verified assets.

`release-linux-ui-update.yml` runs with `capture_catalog`, `viewport_checks` and
`rb10_checks` enabled. It compiles the actual source, packages complete matching
sources and verifies checksums. Gates include native target/cache/layout/key
units; actual runtime QC checks; Windows PowerShell5.1 updater protocol; actual
OS browsers/Welcome/plant placement; Modern Dark/Modern Light at1920×1080 and
1024×768; previous viewport appearance/CreateSave checks; and complete native
botanical catalogue capture. QA artifacts contain screenshots, results and
module/build provenance. The build-info and GitHub Actions run identify the
reviewed revision; a local development run alone cannot publish a release.

`publish-rb10.yml` runs only after merge into main. It requires identical reviewed
and merged trees, matching immutable source ref, exact-head successful platform
CI/packaging and exactly12 complete runtime/source/checksum assets. Every sidecar
filename and digest must match its payload and GitHub digest. An unrelated release
tag blocks publication. Only after these checks does it point the release tag at
the equal-tree merge, publish the draft and mark it latest.

Release: https://github.com/reyond9079/ReyondBroom/releases/tag/v0.5.0-alpha.132
