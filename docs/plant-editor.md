# Plant Editor

Open **Edit > Plant Editor…**. This is part of the existing ImGui editor and uses
the existing FTE dynamic-mesh preview. It does not install Qt, Blender, commercial
plant generators or a second rendering framework.

## Authoring

Choose a species in the searchable library. 121 original botanical presets cover
broadleaf trees, conifers, shrubs, grasses, ferns, flowers, mushrooms, cacti,
palms, bamboo, riparian plants and climbing vines. Search also matches Latin names, IDs and habitat tags. The machine-readable offline catalogue is `ftebroom/plants/builtin/catalog.json`. Library thumbnails are rendered from the
generated mesh. Favorites, recent choices and user presets persist separately
from the editable project.

Quick mode exposes the principal shape and material controls. **Advanced** adds
branching, growth/distribution samples, leaf outlines, trunk spline controls,
component transforms, wind and geometry budgets. Controls depend on the family.
The component tree supports visibility, naming, transforms, adding existing
botanical part types, duplicating complete branches and deleting components.
Plant Undo/Redo is separate from map Undo/Redo.

Drag the preview with the left button to orbit, the middle/right button to pan,
and the wheel to zoom. **Frame** fits the current geometry; regeneration retains
the camera. The viewport opens in **Game Preview**, using the active profile's
model limits and the same geometry preparation as **Add to Map**. It shows the
target format and final mesh counts. Choose **LOD0–LOD2** from the same menu to
inspect the editable source. Preview lighting is independent of map lighting.
In an authoring LOD, click a woody branch to select it. The properties panel
offers length, tilt, thickness, and removal. Edits follow the same branch when
the project is reopened or regenerated with the same seed.
New broadleaf and conifer presets smooth the collars of major branches and
share one branching plan between LODs. Older projects can enable the new
settings in **Advanced**.
Choose perspective/orthographic, textured, solid, normals, UV or
wireframe display and inspect each of the three actual LOD meshes. Panes resize;
small windows switch to tabs. Both Modern Light and Modern Dark use the normal
editor appearance settings.

**Randomize** changes the deterministic seed without changing species.
**Keep variation** records that seed in the project; its thumbnail selects it
again. The Textures/UV tab exposes the generated albedo, normal and ORM atlas
and a UV inspector. Advanced material controls accept separate leaf, bark
and cut-wood albedo images. Bark repeats can be adjusted around the stem and
in world units along it. The viewport's **Normal map** mode displays the atlas
on the model.

## Projects and exports

**Save** writes a human-readable `ReyondBroomPlant` version 1 project, including
generator version, seed, parameters, hierarchy, materials, texture reference,
variations and LOD settings. **Open** restores editable source, rather than
attempting to infer parameters from an exported mesh. Unsaved changes have a
Save/Discard/Keep editing guard. Save copies imported albedo files alongside the project with content-based names and stores relative texture paths. Opening an original generator-1 source preserves its exact geometry and atlases; **Upgrade to botanical generator 2** is an explicit, undoable action.

**Save preset** creates or updates an editable user preset. Manage preset offers
duplicate and reset; rename through Plant name and Save preset. Deletion moves
the user preset to the plant trash directory.

**Export** accepts `.obj` or `.glb` for the selected LOD. OBJ includes MTL and PNG
dependencies. GLB embeds geometry, normals, UVs, base color with alpha cutout,
normal and ORM textures in one glTF 2.0 binary. Portable exports use right-handed
Y-up coordinates at the original unit scale. Exports refuse to replace a
different existing asset; choose a new name. Projects can overwrite their own
opened/saved source file.

## Place on a map

Right-click a 3D or orthographic scene view and choose **Create > Model >
3d model** or **plants/trees/grass**. The initial hit, normal, workplane, grid,
active layer, map instance and profile belong to that request. Camera movement
after opening the editor does not move the eventual object.

The first **Save** writes the editable source, the runtime model for the active
profile and its material dependencies, then creates one authoring entity. Later Save / Save As update
that entity and preserve its transforms and layer. Opening a versioned source snapshot starts an editable copy and requires Save As, preserving the original history asset. Runtime bundles are immutable
and content-addressed, so map Undo/Redo restores the previous geometry. Deleting
or undoing the inserted entity cancels its binding; another Save does not revive
it. Changing map/profile cancels pending placement. Ordinary Open/Save has no
automatic insertion. New documents pass through both editors' dirty guards.
Model export and plant generation run in cancellable background jobs; stale
results cannot insert into a different document.

**Add to Map** remains an explicit operation. With an active Create binding it
uses that binding; otherwise it places at the current viewport centre. Generated
resources and manifests live below `models/reyond-plugins`,
`textures/reyond-plugins` and `scripts`, in writable loose roots beside archives.
Shader lists keep existing entries. Build snapshots include their dependencies.

| Target | Authoring document and runtime |
| --- | --- |
| Quake III | MD3 with up to 32 surfaces of 999 vertices and 1,999 triangles each. q3map2 bakes the `misc_model` into the map. |
| FTE Custom | Full-detail IQM with no total MD3 surface or triangle cap. `func_model` renders in the compiled preview. An external game needs a mod that implements this class. |
| Quake 1 with FTE | Explicit **Use FTE static models for Quake 1** opt-in; the disposable copy uses FTE `misc_model` and effective uniform scale. Bake nonuniform scale into the source geometry or use a compatible custom mod; otherwise the build stops with a diagnostic. |
| Vanilla Quake / Quake II / GoldSrc | Authoring is allowed; an explicit build diagnostic requires conversion / a compatible mod entity |

Quake III baked plants use vertex lighting. FTE uses normal-based entity
lighting. Both materials take cutout transparency from the atlas. Profile-specific asset
keys prevent shader conflicts when the same project is placed in both targets.

The inserted entity participates in ordinary selection, position/rotation/scale,
map Undo/Redo and save/reopen. Removing it leaves reusable assets on disk.
Vanilla Quake progs do not gain arbitrary MD3 support. Unsupported profiles display an authoring-only warning and block incompatible build export.

## Rendering and practical limits

Generator 2 plans a complete skeleton and crown before allocating geometry, with spatial foliage sampling, crown envelopes, phyllotaxy, pinnate/palmate/bipinnate leaves, needle sprays and species-specific organs. Generator 1 remains available for old sources. Both use seeded branching, swept spline tubes, botanical leaf
placement, separated fern/palm pinnules, procedural petals, lathed mushroom cap
profiles with gills/pores, cactus ribs/spines and bamboo nodes. Materials and
presets are original and require no downloaded assets. Texture maps are atlased
with UV gutters; normals, indices, bounds and budgets are validated.

The native preview supports depth-tested, two-sided alpha cutouts with baked
directional vertex shading and an AO approximation. It is not a full PBR or
shadow-map renderer. GLB retains normal, roughness and AO maps for supporting
renderers. Wind animates the authoring preview. For FTE Custom game, enable
**Wind in FTE Play mod** to stage a height-based GLSL sway with the placed
model. Portable and classic profile exports use the undeformed static pose.
HDRI, dynamic plant shadows, whole-tree
impostor baking, collision meshes, surface-following vines and vegetation scatter
are not implemented. Distant foliage uses crossed cards, rather than a baked
whole-plant impostor. Procedural materials are suitable starting assets, not a
photogrammetry library.

Generation and thumbnails run off the UI thread. Edits debounce, cancel old
work and discard stale results. Material-only edits reuse geometry; hierarchy
edits reuse the botanical base mesh. Budgets may reduce canopy density; a visible
notice reports that limit. Automatic Save normalizes large Quake III MD3 coordinates and compensates through modelscale. FTE IQM keeps full-precision positions at the authored size. Direct MD3 exports exceeding its limits require a lower LOD or a portable export. Projects allow up to 64 components and
64 saved variations, with bounded generation and document history.

## Verification

`tests/plant-editor.cpp` checks every preset and LOD, seeds, mesh validity,
winding/normals, wired parameters, hierarchy/project round trips, cancellation,
budgets, materials, production OBJ import and MD3 decoding. Test-only
trimesh/Pillow independently load OBJ materials and round-trip embedded GLBs.

`scripts/test-plant-ui.py` drives the actual app through OS mouse/keyboard input;
its JSON observations contain state and widget rectangles, never UI mutations.
It exercises projects, controls, history, exports, presets, themes, placement,
map history/reopen, build dependency staging and a screenshot assertion that the
placed foliage is actually visible after reopening the map. `scripts/test-plant-map.py`
invokes the bundled q3map2 and checks plant triangles/materials in the resulting
BSP. These checks run alongside the existing compilation-profile regression on
Windows, native Linux and Linux SDL2. Screenshots come from the running app.


`tests/plant-botanical.cpp` checks all 121 presets × 3 seeds × 3 LODs,
architectural descriptor uniqueness and category minima. The generator-1 golden
corpus was recorded from the unmodified REYON-8 baseline (9e5b30a); CI rebuilds that frozen baseline on each OS so different libm implementations do not cause false cross-platform bit comparisons. It verifies
270 exact legacy meshes and 270 albedo/normal/ORM atlas hashes.
`scripts/test-viewport-create.py` uses real OS input for both Create/Save routes
and independently inspects saved map history, source links and runtime assets.

REYON-9 evidence: [visual review](qa/reyon-9/quality.md), [performance and raw measurements](qa/reyon-9/performance.md).
