# Plant Editor

Open **Edit > Plant Editor…**. This is part of the existing ImGui editor and uses
the existing FTE dynamic-mesh preview. It does not install Qt, Blender, commercial
plant generators or a second rendering framework.

## Authoring

Choose a species in the searchable library. Thirty built-in presets cover
broadleaf trees, conifers, shrubs, grasses, ferns, flowers, mushrooms, cacti,
palms, bamboo/reeds and climbing vines. Library thumbnails are rendered from the
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
the camera. Choose perspective/orthographic, textured, solid, normals, UV or
wireframe display and inspect each of the three actual LOD meshes. Panes resize;
small windows switch to tabs. Both Modern Light and Modern Dark use the normal
editor appearance settings.

**Randomize** changes the deterministic seed without changing species.
**Keep variation** records that seed in the project; its thumbnail selects it
again. The Textures/UV tab exposes the generated albedo, normal and ORM atlas
and a UV inspector. Advanced material controls accept a leaf albedo image.

## Projects and exports

**Save** writes a human-readable `ReyondBroomPlant` version 1 project, including
generator version, seed, parameters, hierarchy, materials, texture reference,
variations and LOD settings. **Open** restores editable source, rather than
attempting to infer parameters from an exported mesh. Unsaved changes have a
Save/Discard/Keep editing guard. Imported images remain external references;
keep them available alongside your project.

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

**Add to Map** writes a generated MD3, alpha TGA, shader and editable-project
sidecar under `models/reyond-plugins`, `textures/reyond-plugins` and `scripts`.
It also installs them in the configured mod directory when one is available.
The generated shader is registered in `scripts/shaderlist.txt`; existing entries
are retained. Model and material dependencies travel into compilation/preview
snapshots together.

| Target | Placement |
| --- | --- |
| Quake III | `misc_model`, compiled into BSP by q3map2 |
| FTE Custom | `func_model`, using the existing model pipeline |
| Quake 1 with FTE | Explicit **Use FTE static models for Quake 1** opt-in; the disposable build copy converts the authoring entity to FTE `misc_model` |
| Vanilla Quake / Quake II / GoldSrc | Export OBJ and use Model Editor's target-format conversion with an entity supported by the intended mod |

Quake III baked plants use vertex lighting; FTE/Quake 1 extension models use
normal-based entity lighting because MD3 has no vertex color channel. Both
materials take cutout transparency only from the atlas. Profile-specific asset
keys prevent shader conflicts when the same project is placed in both targets.

The inserted entity participates in ordinary selection, position/rotation/scale,
map Undo/Redo and save/reopen. Removing it leaves reusable assets on disk.
Vanilla Quake progs do not gain arbitrary MD3 support. No incompatible model is
silently inserted in Quake II or GoldSrc.

## Rendering and practical limits

Generation uses seeded recursive branching, swept spline tubes, botanical leaf
placement, separated fern/palm pinnules, procedural petals, lathed mushroom cap
profiles with gills/pores, cactus ribs/spines and bamboo nodes. Materials and
presets are original and require no downloaded assets. Texture maps are atlased
with UV gutters; normals, indices, bounds and budgets are validated.

The native preview supports depth-tested, two-sided alpha cutouts with baked
directional vertex shading and an AO approximation. It is not a full PBR or
shadow-map renderer. GLB retains normal, roughness and AO maps for supporting
renderers. Wind animates the authoring preview; exports preserve the undeformed
static pose. Runtime wind weights, HDRI, dynamic plant shadows, whole-tree
impostor baking, collision meshes, surface-following vines and vegetation scatter
are not implemented. Distant foliage uses crossed cards, rather than a baked
whole-plant impostor. Procedural materials are suitable starting assets, not a
photogrammetry library.

Generation and thumbnails run off the UI thread. Edits debounce, cancel old
work and discard stale results. Material-only edits reuse geometry; hierarchy
edits reuse the botanical base mesh. Budgets may reduce canopy density; a visible
notice reports that limit. Dense assets exceeding MD3's surface/coordinate limits
must use a lower LOD or a portable export. Projects allow up to 64 components and
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
