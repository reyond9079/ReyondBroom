# Plant Editor — Full Implementation Specification

**Goal:** Build a complete native procedural vegetation authoring tool inside ReyondBroom. This must be a genuinely functional editor with realistic, game-ready meshes, not a UI mockup. Use RockPaperTrees, Tree It and PlantFactory as workflow references without copying proprietary code, assets, or interface artwork.

## 1. Objective

Enable users to generate, customize, preview, save, export and insert realistic trees, bushes, grass, ferns, flowers, mushrooms, cacti, palms, bamboo and vines directly into Quake maps, without depending on external modeling applications. The experience must combine approachable procedural controls with advanced authoring capabilities.

## 2. Mandatory Codebase Investigation

Before coding, inspect the ReyondBroom repository, Modern Light/Dark UI, Model Editor, Material Editor, viewport renderer, model/mesh pipeline, texturing/UV facilities, import/export, existing undo/redo, map placement and project assets, plugin architecture and FTEQW integration. Confirm actual engine profile capabilities and reusable modules. Integrate with existing architecture; do not create a standalone app or introduce unnecessary GUI/rendering frameworks. Preserve current editing/compilation functionality.

## 3. User Interface

**Left: Plant Library & Structure.** Search, preview thumbnails, favorites and recent presets; categories Trees, Conifers, Bushes, Grass, Ferns, Flowers, Mushrooms, Cacti, Palms, Vines/Ivy, Reeds/Bamboo and Custom. Editable component hierarchy with selection, show/hide, duplicate, rename, add and delete.
**Center: 3D Viewport.** Live preview, orbit/pan/zoom, perspective/orthographic cameras, ground/grid, sunlight/lighting/shadows where supported, sky/background, optional HDRI, wireframe/solid/textured/normals/UV-debug/LOD modes, geometry statistics, selection and framing. Regeneration must preserve the camera view.
**Right: Procedural Properties.** Expandable groups: General, Shape, Growth, Branching, Leaves, Flowers, Materials, Textures, Randomness, Wind, Seasons, Geometry, LOD, Export. Real wired sliders, numbers, drop-downs, colors, toggles and curves; live regeneration with debouncing/background processing.
**Bottom: Variations & Assets.** Seed input, Randomize/Generate, variation thumbnails and saved variants, materials/textures, UV inspector, progress/errors. Toolbar: New, Open, Save, Presets, Generate, Randomize, Undo, Redo, Export, Add to Map. Quick Mode and Advanced Mode. Use original ReyondBroom-aligned UI supporting Modern Light and Modern Dark, resizable panes, 1920×1080 and smaller layouts.

## 4. Procedural Generation Core

Implement actual deterministic 3D mesh generation with suitable algorithms selected per plant family: L-systems/recursive branching, space colonization where appropriate, spline paths, stable swept-mesh frames, tapered trunks and branches, phyllotaxis, leaf clusters, apical dominance, botanical tropisms (light/gravity), controlled noise and seeded irregularity. Generate valid mesh topology, triangle indices, normals, UVs and material slots. Guard against degenerate geometry and extreme inputs. Never substitute spheres-on-cylinders for realistic botanical structures.

## 5. Distinct Plant Generators

**Trees:** height, trunk width/taper/curvature, root flare, branching depth/distribution/count/angles/lengths/thickness, crown shape/density, leaf size/orientation/density, age, irregularity and dead branches. Presets Oak, Birch, Pine, Spruce, Willow, Maple, Palm. Species architecture must genuinely differ: pines need conifer-like structure, willows droop, palms have specialized fronds and trunks.
**Bushes:** size, multiple stems, crown, leaf groups, bare interior branches, density and asymmetry; ornamental round bushes, hedges, wild shrubs and flowering shrubs.
**Grass:** tapered blades or performant alpha cards, length/width/count, clump radius, bending, color/dryness, random orientation, density and wind.
**Ferns:** realistically separated fronds and pinnules, frond curves/length/count, leaflet shape, growth stage and drooping.
**Flowers:** procedural stem, sepals, leaves, curved petals, center and buds; adjustable petals/layers, color gradients, flower diameter, bloom stage, clusters.
**Mushrooms:** distinct cap profiles, stems, gills or pores, cap thickness/roughness, spots/scales, growth stage; Amanita-like, boletus-like, oyster, chanterelle-like, woodland and oversized fantasy. Avoid generic hemispheres on cylinders.
**Cacti / Bamboo / Palms / Vines:** specialized cactus ribs/spines, bamboo segments/nodes, palm fronds and spline-growing vines with natural leaf placement; optional vine growth along map surfaces.

## 6. Realistic Materials and Textures — Critical

Provide believable procedural or license-compatible textures for bark, wood, foliage, needles, grass, petals, mushroom caps/gills and cactus skin. Support albedo/base color, normals, roughness, AO, alpha cutout, two-sided leaves and translucency approximations as target renderer permits. Add natural irregularities: bark ridges/cracks, leaf veins, gradual color shifts, surface spots, dryness and variation. Generate foliage alpha masks and texture atlases; accept imported textures. Preserve UV/material fidelity on regeneration. Preview PBR-style materials only if supported; bake/adapt textures for actual Quake/FTE shader capability. Do not assume preview rendering capabilities exist in the game.

## 7. Foliage Geometry

Offer detailed leaves, curved low-poly leaves, alpha-cutout leaf cards, crossed foliage cards and distant billboards/impostors. Shapes oval, elliptical, lanceolate, lobed, needle, compound and custom. Ensure realistic silhouettes and correct UVs, alpha, normals and backface behavior. Provide high-quality and optimized modes.

## 8. Presets and Randomization

Ship >=20 genuinely distinct usable species/family presets with appropriate geometry and materials. Versioned, human-readable save format; load, duplicate, rename, reset and delete user presets. Deterministic seed regeneration, randomize and save variations. Preserve species identity during randomization. Provide Quick and Advanced parameter modes.

## 9. Advanced Structure Editing

Editable plant hierarchy: Trunk > Primary Branches > Secondary Branches > Twigs > Leaves, plus Roots and Materials. Select groups, change per-group settings, toggle layers, change growth/distribution curves, regenerate selected components. Where feasible, directly move major spline/control points. A node editor may be optional but must not replace simple controls.

## 10. Optimization & LOD

Polygon/triangle budgets, mesh reduction, material batching/atlases, efficient preview instancing, leaf-card optimization, optional billboards/impostors and collision meshes. LOD0 high, LOD1 medium, LOD2 low with visible triangle statistics and configurable budgets; preserve recognizable silhouettes and maintain target Quake compatibility.

## 11. Wind & Seasons

Interactive wind preview: strength, direction, frequency, branch stiffness, leaf flutter, grass bending. Optionally output wind vertex weights if target supports them; otherwise export correct static fallback and clearly communicate support. Seasonal profiles Spring/Summer/Autumn/Winter can change colors, foliage density, blooms, dryness, snow or leaf loss.

## 12. Save, Load, Export

Use a native versioned editable plant project storing generator version, type, seed, all parameters, hierarchy, materials, texture references and LOD. Offer OBJ and GLB/glTF exports where feasible using existing ReyondBroom infrastructure. Preserve geometry, UVs, normals, materials, textures, origin and scale. Validate with actual round-trip reimport; never pretend a file is usable just because it was written.

## 13. Add to Map Integration

Real Add to Map workflow: create/edit plant > save/export compatible asset > insert model into the active Quake map > position/rotate/scale with existing tools > render in map viewport > save/reopen reference successfully. Respect Quake 1, Quake 3, FTE profile format and shader limitations. Where incompatible, provide an explicit conversion/export solution rather than silently adding broken asset references.
**Optional Vegetation Scatter:** selected surfaces, density, randomized transforms, seed, minimum spacing, surface alignment, slope limits, variations, instance cap and undo/redo, using existing entity placement system.

## 14. Performance and Stability

Regenerate only changed parts; cache geometry and textures; debounce controls; perform heavy generation asynchronously with cancellation; prevent stale results overwriting new state. Avoid GPU/memory leaks, redundant rebuilds and frame stalls. Profile dense mature trees. Enforce safe parameter limits and meaningful diagnostic errors.

## 15. Visual Quality Acceptance

Default must produce believable organic 3D plants with species-appropriate branching, silhouette, proportion, textured leaf forms, subtle variation and practical triangle budgets. No placeholder cones, balls or generic primitives. Profiles: Realistic (default), Optimized Game-Ready, Low Poly and Stylized. Use real rendered comparisons/screenshots to assess output.

## 16. Modular Architecture & Licensing

Separate procedural algorithms, botanical schemas, mesh construction, material/texture generation, LOD, serialization/presets, viewport, UI/data binding, export/import and map integration. Follow existing C++/QuakeC architecture where appropriate. Avoid heavyweight dependencies. Audit third-party licenses and ensure compatibility with ReyondBroom's GPL obligations; never copy proprietary source, UI assets, textures or samples from commercial generators.

## 17. Implementation Roadmap & Tests

**Phase 1:** working native editor UI, param schema, mesh pipeline, preview, save/load.
**Phase 2:** distinct plant family generators and real presets.
**Phase 3:** UVs, natural materials/textures, botanical fidelity, LOD.
**Phase 4:** exports, Add to Map, per-engine compatibility, undo/redo.
**Phase 5:** usability, optimization, tests and visual polish.

Mandatory tests:

* New seed changes the plant; same seed/params reproduces exact output.
* Each active parameter control changes actual model behavior.
* Every plant category produces an identifiable form.
* Mesh indices/UVs/normals validate; foliage alpha renders correctly.
* Presets and editable plants save/load without data loss.
* Export/import works and materials/textures are retained.
* Add to Map and map save/reopen work for supported profiles.
* Undo/redo and existing editor tools remain correct.
* Modern Light/Dark UI both usable and visually coherent.
* Build and launch actual app; when feasible capture screenshots at >=1920×1080 of overall Plant Editor, realistic broadleaf tree, pine, bush, grass, mushroom, flower and model placed in Quake map. Report honestly if UI testing was unavailable.

## 18. Definition of Done

User can open Plant Editor, select type/preset, generate realistic 3D geometry, edit parameters live, randomize deterministically, edit material/texture, save and reopen editable source, export a valid model and place it in a compatible Quake map. All primary UI actions work. No fake buttons, stub generators or silently ignored controls. Disclose any unimplemented capability explicitly.

## 19. Deliverables

Integrated source code, functional procedural generators, usable Modern Light/Dark UI, realistic presets, editable save/load, game-ready exports, map integration, relevant automated tests, actual screenshots, and a final list of implemented/tested behavior, limitations and performance.

**Agent instructions:** Inspect repository before implementing. Implement and verify, do not stop at architectural planning. A polished UI with no working realistic plant generation fails acceptance; so does good generation with poor native UX.

**File prepared for external attachment:** ReyondBroom_Plant_Editor_Spec.md.
