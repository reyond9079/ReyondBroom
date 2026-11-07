# REYON-9 implementation and acceptance

Task: https://reyond.atlassian.net/browse/REYON-9

PR: https://github.com/reyond9079/ReyondBroom/pull/12

Baseline `9e5b30a`; delivered through the native FTE/C++/QuakeC/ImGui editor.
No Qt or second GUI/rendering framework is added. See [visual review](quality.md)
and [same-machine performance data](performance.md).

## Implemented behavior

- Generator 2 plans a complete skeleton and spatially distributes foliage before
  emitting a bounded mesh. Crown/phyllotaxy traits, compound leaves, needle
  sprays and family-specific organs distinguish 121 built-ins. Catalogue counts:
  trees 24, conifers 12, shrubs 14, grasses 12, ferns 8, flowers 14, mushrooms 9,
  cacti/succulents 8, palms 6, vines 6, bamboo 4, riparian 4. The extra mushroom
  preserves the old fantasy preset. Latin names, habitat/architecture tags,
  category filtering, favorites/recent/user entries and lazy mesh thumbnails
  remain available. Resources are original, GPL-compatible descriptors/materials.
- Old 30 IDs and generator-1 projects retain exact geometry/material output.
  Upgrade is explicit, leaving the existing source unchanged until Save. Native
  and Windows tests compare against a separately compiled frozen baseline on
  the same platform to avoid mistaking different libm implementations for a
  compatibility regression.
- Short RMB in any scene view offers Create > Model > 3d model or
  plants/trees/grass. It captures world hit/normal, grid/workplane basis, layer,
  map epoch, profile and roots before either authoring editor opens. Existing
  drag navigation and context actions remain. Dirty document guards run before
  replacement, and a guard's Save waits for the map transaction acknowledgement.
- First successful Save publishes source, versioned MD3/materials/manifest and
  one source `func_model`. Further Save/Save As update the existing identity,
  preserving user transforms/organization. Immutable revisions support visible
  map Undo/Redo. A stale map/profile, deleted/undone object or ambiguous GUID
  invalidates automatic placement. Ordinary source opens are unbound; opening
  a historical source forces Save As to avoid overwriting history.
- Relative dependencies support writable loose roots alongside PK3 and Unicode
  paths/spaces. Failed export/Save As does not insert a broken scene entity or
  overwrite another source. Model bake/export is cancellable background work;
  shader/engine calls and the final document transaction remain on the UI thread.
- Source maps always retain func_model. Quake III adapts only marked authoring
  objects in a disposable build copy, including effective scalar/vector scale;
  FTE Custom's compiled preview renders external models. Quake 1 requires an
  explicit FTE-extension opt-in, adapts to misc_model and writes effective
  uniform scale. Nonuniform Quake 1 fallback scale, vanilla/Q2/GoldSrc
  incompatible output stop with a useful diagnostic. Custom mods still own
  their runtime spawn functions; source maps are never silently rewritten.
- SDL2 now resumes native UTF-8 text input for focused ImGui inputs after the
  console closes. The regression was found with real OS input and is covered
  by Save As and Unicode path scenarios on the patched engine.

## Reproducible checks

- `tests/plant-editor.cpp`: 1,160 core checks, project/library/history/cancel,
  hierarchy, original/new parameters, materials and real model codecs.
- `tests/plant-botanical.cpp`: all 121 × 3 seeds × 3 LODs, exact regeneration,
  finite meshes/winding/bounds/materials/budgets, strict LOD reduction, leaf and
  culm survival, connected Opuntia pads at every LOD and bounded grass/riparian height; frozen legacy corpus, atlases and catalogue parity. The local
  complete matrix passes 10,679,083 assertions. The independent platform corpus
  retains the original 30 × 3 × 3 geometry outputs and texture fingerprints.
- `tests/test-static-models.cpp`: build-only adaptation, source preservation,
  unsupported-profile and nonuniform-scale diagnostics, portable dependencies,
  real MD3/MD2 parsing and truncation rejection.
- `scripts/test-plant-ui.py`, `test-plant-exports.py`, `test-plant-map.py`:
  OS controls, project/variation/preset operations, independent import, real
  q3map2 compilation, dependency staging and visible map reopen/Undo/Redo.
- `scripts/test-viewport-create.py`: 40 OS-input assertions for both commands,
  3D/orthographic views, frozen click after camera movement, Save/update/Save As,
  Unicode paths, short ASCII typing after the console, overwrite refusal,
  failed empty export, dirty-guard Save,
  history-source copies, stale maps, deleted entities, tree/shrub/grass routes
  and real scene pixels. Clicking visible crown pixels verifies full transformed
  model bounds rather than only the origin marker. Screenshots retain menu,
  both editors and placed model.
- CI retains compilation-profile, model/sound/file-manager and engine viewport
  regression suites on Windows, native Linux and SDL2. Release packaging runs
  the actual produced binaries in Modern Dark/Light at 1920×1080 and 1024×768.
  The native package additionally captures all 121 originals and the extended
  twelve-control gallery with real OS input. No renderer or callback is mocked.

## Actual compiled/runtime sample

A saved authoring oak and its exact dependencies were staged into fixture roots
with spaces. Ericw qbsp/light produced a Quake 1 BSP; a real FTE game session
with LibreQuake data displayed the model and cutout material. This verifies the
explicit extension, not vanilla Quake support. q3map2 produced a lit Quake III
BSP with **31,994 plant triangles** and the generated shader. The FTE Custom
BSP preserves func_model as an external runtime-preview object rather than
baking it into world geometry. Raw BSP entity/shader evidence is retained in
[bsp-results.json](bsp-results.json). Original screenshots and runtime/preview
logs are attached to the Jira task, alongside before/after evidence.

## Publication

The PR stays draft through platform CI. Its ready transition reserves the next
alpha.129 draft and dispatches packaging against the exact PR head. Publication
runs only after merge into main, verifies identical reviewed/merged source trees,
all platform CI/package jobs, twelve nonempty runtime/source/checksum assets and
GitHub SHA256 asset digests. The unpublished tag then points at the merge commit;
release becomes latest only after those checks. Each package records its actual
revision/compiler/module hash and version in build-info.txt.
