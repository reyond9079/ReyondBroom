# ReyondBroom — WORLD ARCHITECT

## Full engineering specification and executable coding-agent prompt

**Scope:** Replace AutoLevel with a high-quality, post-apocalyptic procedural level authoring system for playable indoor environments, outdoor streets and urban districts, and seamless indoor/outdoor transitions.

**Status:** Implementation specification, not a claim that functionality already exists.

**Primary outcome:** From a fresh project and a supplied seed, a user can generate an **interesting, coherent, attractive, editable, compile-ready FPS level** with spatial storytelling and meaningful player choices. A technically valid but bland grid dungeon is a failed outcome.

**Codex / coding-agent instruction:** Read and follow this document as implementation requirements. Inspect the repository first, implement working systems in iterative vertical slices, build and test the actual application, and report precisely what has and has not been verified. Do not stop at a UI prototype, proof of concept, generic random box generator, or design document.

---

## 1. Existing ReyondBroom implementation — audit and replacement constraints

The repository currently contains:

* `plugins/autolevel/` — current AutoLevel implementation and builder/repository architecture.
* `plugins/autolevel/al_default_repo.cpp` — a small handcrafted “Reyond Dungeon” library whose cells, halls, stairs, atria, corridors, decorations and details are mostly built from fixed, axis-aligned boxes.
* `plugins/autolevel/al_solver.cpp` — weighted constraint propagation, placement, backtracking, connectivity and coverage checks.
* `plugins/autolevel/al_build_map.cpp` — transforms existing brush geometry into grouped `.map` output.
* `plugins/levelgen/` — legacy procedural WFC generator.
* `plugins/common/geometry.h`, `sdk/reyondbroom_plugin.h`, `native/ftebroom_plugins.h` — geometric helpers, host SDK and plugin integration.
* `docs/autolevel.md`, `docs/autolevel-acceptance.md`, `docs/modular-levelgen117.md` — behavior, compatibility and budgets.

**Verified root limitation:** Existing solver placement randomizes combinations of a restricted set of shapes. It does not own a semantic gameplay plan, procedural architecture grammars, street/block/parcel networks, progressive damage modeling, or scene-composition scoring. Adding more seeds/weights will not resolve this.

**Replacement requirements:**

1. World Architect must be the default first-class level generation tool (e.g. `Plugins > World Architect`), replacing AutoLevel in the normal new-project workflow.
2. Preserve existing AutoLevel data, map content and settings without silent deletion. Legacy settings should be readable/migratable where feasible, and never overwritten during the new generator's first run.
3. Old AutoLevel should remain accessible only by a clearly identified legacy/migration route until new functionality meets acceptance criteria; then retire its default UI entry. Avoid shipping two indistinguishable generators.
4. Use native ReyondBroom API and existing C++/ImGui/editor facilities where possible; avoid external runtime requirements such as launching Unity, Houdini, Unreal, Python, cloud services or an LLM to generate a map.
5. Reuse validated brush serialization, UV, transformations, imports and undo/redo code. Assess whether optional host SDK extensions are truly necessary; preserve compatibility with existing plugins.
6. Do **not** simply reskin AutoLevel, rename its window, add a few new hard-coded block variants, or call static templates a procedural architectural generator.

---

## 2. Research baseline — borrow proven concepts, not proprietary implementation

Study the following ready-made solutions/documentation before deciding algorithms. Implement independent code using the ideas, not copyrighted sources, assets, proprietary UI or licensed sample packs.

| Reference | Proven idea to adapt | What NOT to assume |
| --- | --- | --- |
| Dungeon Architect — Flow/Cell/Snap Grid Flow | Separate **mission graph** (routes, loops, gates, landmarks, role tags) from spatial realization and theme/dressing; stitched authored modules can coexist with generative geometry | No Unreal dependency; do not copy its plugin/source or content |
| SideFX Labs Building Generator / Building from Patterns | Transform low-resolution massing into facades, wall bays, floors, windows, cornices, trims and module variations using parametric grammar | Houdini need not be installed or embedded |
| Esri CityEngine CGA / dynamic streets | Build street topology first; derive blocks, plots and facades via context-sensitive architecture rules | Do not embed proprietary CGA code |
| Unreal PCG Framework and PCG Editor Mode | Hierarchical, attribute-driven scattering; spline/volume/paint masks; constraints and local regeneration | Do not depend on Unreal PCG at runtime |
| Parish & Müller (SIGGRAPH 2001), Procedural Modeling of Cities | Major roads → secondary roads → block/parcel subdivision → building massing | Research inspiration, not a direct license to copy implementation |
| Existing ReyondBroom AutoLevel | Constraint solving for local socket connections, versioned assets, brush output, user overrides, selection-based regeneration and compatibility checks | Do not let simple WFC placement dictate whole-world design |

Design principle: **Mission & story graph → spatial site plan → architectural massing → detailed geometry → post-apocalyptic damage → thematic dressing → navigation/quality evaluation → MAP export.** Each stage is addressable, previewable and reproducible.

Sources and official documentation are linked in section 23.

---

## 3. Concrete gameplay deliverable

The generated result should feel like a human-designed FPS map: a memorable entrance, recognizable destination, at least one surprising spatial reveal, useful side route, several deliberate sightlines, changing compression/expansion, optional rewards, vertical navigation, coherent cover and interesting exploration.

Generate at least these modes:

1. **Abandoned Interior** — hospital, apartment block, school, laboratory, warehouse, metro station, shopping center, factory, bunker; multi-floor circulation and logically arranged rooms, connected to a believable former function.
2. **Ruined City Block** — streets, intersections, alleys, sidewalks, playable buildings, roofs/courtyards, wreckage and landmarks.
3. **Urban Exploration Journey** — multiple blocks with main travel direction, detours, elevated routes, enterable buildings, one or more scripted-by-geometry chokepoints and an objective.
4. **Mixed Urban + Interiors** — a complete playable route from street into building, through a vertical interior, back to street, possibly via subway/rooftops/bridges.
5. **Combat Arena / Hub** — purposeful multi-route combat topology with flanking, cover, sightline management and player re-entry routes; not just a symmetric empty square.

Each generation preset must specify **player capabilities**, gameplay scenario and experience scale. Do not hard-code game-specific weapons, enemies, quests or triggers when engine support is unknown: emit semantic markers, basic supported entities and editable metadata, allowing a user/mod to map these to gameplay entities.

Default user flow: choose preset and size → click `Generate Candidates` → inspect best playable candidates in split top-down/3D preview → open selected candidate → tweak locally → click `Apply to Map` → compile and play. A first-run user must not be required to build a custom module library.

---

## 4. Non-negotiable multi-pass architecture

Represent the world in a **typed semantic intermediate representation (IR)**, not directly as an unstructured string of brushes. Define and test schemas for:

* `WorldPlan` (bounds, seed, site, terrain, biome, damage epoch, style).
* `MissionGraph` (start, goal, landmarks, main route, alternate routes, loops, gates, rewards, beats, tags, dependencies).
* `SpatialGraph` (rooms, streets, lots, buildings, stairs, traversable edges, vertical connections).
* `ArchitecturePlan` (volumes, floor plates, building wings, openings, circulation, facade programs, structural elements).
* `DamageField` (collapse zones, structural integrity, rubble/occlusion masks, aging, water intrusion).
* `AssetPlacement` (semantic anchors, art variants, collision envelope, provenance, style tags).
* `NavigationGraph` (walkable areas, ladder/stair/drop transitions, cover and visibility annotations).
* `GenerationDiagnostics` (stage timing, constraints, coverage, score breakdown, rejects, geometry and compile metrics).

Use versioned serialization, stable per-stage seed derivation, and immutable or copy-on-write stage outputs where helpful. Each generation stage must have explicit inputs/outputs, validation and cancellation. Allow a designer to lock a stage or selected region while regenerating later ones.

Prefer deterministic seeded substreams for each building/district/stage so changing one decoration does not rebuild an entire city.

---

## 5. Stage A — Goal-driven mission and player-experience planner

Generate or accept an editable designer-defined **mission graph** _before_ laying out streets or geometry.

Required semantic nodes:

* Start / safe start; objective / destination; exit.
* Introduction; reveal; challenge; exploration; transition; landmark; optional reward; secret; vista; social hub/safe zone (where appropriate).
* Gate; prerequisite/key/switch; shortcut unlock; loop/junction; vertical transition; risk/reward branch.

Required rules:

* Every required objective reachable from spawn under the profile's locomotion capabilities.
* Keys/items obtainable before gates requiring them; no soft locks and no unavoidable dead ends on critical path.
* Default medium layouts: at least one meaningful loop or return shortcut, at least one optional route, at least one memorable landmark, and multiple scales of spaces. Parameterize requirements by size.
* Optional one-way traversal / blocked street / locked access must have a provably viable alternative, unless intentionally a late-stage inaccessible destination.
* Place main route, alternate route, secret route and return path in the graph; annotate pacing: anticipation, transition, tension, reveal, release.
* Explicit map-gen modes: Exploration, Linear Adventure, Open Hub, Arena. Apply different scoring policies for each.
* Avoid pointless turns, repetitive identical corridor chains, excessive dead ends and inaccessible decoration.

Add an authorable `Experience Profile` with target route length, encounter count/spacing, exploration ratio, loop count, secrets, verticality, openness, danger, navigation complexity, texture/asset budget, expected travel time (heuristic only), and difficulty progression.

### Evaluate candidate mission graphs

Generate several candidate graphs, score and reject weak candidates. Score components must be visible in UI. Use a weighted objective with explicit hard validity constraints and soft preferences, not a single opaque “quality score”. Allow designer-tunable weights.

---

## 6. Stage B — Site planning, roads, districts and public spaces

**Urban levels must be true city plans**, not room modules standing outdoors.

### Road graph

* Primary and secondary streets, local lanes, alleyways, footpaths, plazas, parking lots, intersections, dead ends and pedestrian shortcuts.
* Support grid, irregular/organic, partially radial and hybrid road patterns, with terrain/boundary constraints.
* Build road centerline splines/graphs, then sweep road surfaces and create sidewalks, curbs, drains, crossings and edge details.
* Variable road width, setbacks, elevations, slopes, T/X/Y intersections, overpasses/underpasses (if profile allows) and non-square street blocks.
* Derive blocks from planar road embedding and subdivide into lots with deterministic frontage/depth rules.
* Respect world bounds, minimum lane width, vehicle/footprint collision envelopes, road crossings and polygon robustness.
* Reserve **playable corridors and mission locations first**; urban authenticity must not accidentally destroy gameplay.

### City zoning and readable composition

* District tags: residential, commercial, industrial, medical/civic, transit, utility, military, outskirts and park.
* Connected zones with functional transitions (e.g. apartments near shops, delivery access at warehouses, tram/metro access where applicable).
* Road hierarchy and building height variation must generate a readable skyline and silhouettes.
* Create visually distinct landmarks: collapsed overpass, clock tower, hospital facade, power station, wrecked tram, checkpoint, water tower, flooded underpass, billboard.
* Control prospect/refuge: narrow alleys vs open squares, blocked vista vs reveal, safe elevated overlook vs risky low ground.
* Provide navigable building entrances and connections between streets, courtyards, rooftops and interiors; never assume every facade is enterable.

### Environmental story logic

An implicit coherent historic scenario (evacuation, riot, bombardment, flooding, long abandonment, vegetation reclamation) drives damage and prop placement, rather than independent noise per surface.

---

## 7. Stage C — Interior layout and multi-story circulation

Generate plausible functional interior plans for each building archetype, with distinct circulation principles:

* Hospital: corridors linking wards, treatment rooms, triage, pharmacy, stairwells and service spaces.
* Apartments: stair/elevator core, landings, units, kitchens/rooms, service shafts and roof access.
* Warehouse/factory: high-bay volumes, loading bays, mezzanines, gantries, service passages.
* Metro: platforms, concourse, tunnels, maintenance rooms, stairways, controlled entrances.
* School/mall: central circulation, clusters, atria, services and vertical links.

Implement multiple floor-plan families: corridor spine, ring circulation, courtyard, atrium, branched halls, clustered rooms, offset/rotated wings and carved irregular footprints. Vary room dimension/height/aspect independently; use local snapping for construction only, not to force every space into one uniform 256-unit rectangle.

Generate adequate player hull clearance, doorway widths, vertical headroom, stairs/ramps, platforms, bridge edges, intentional cover and combat sightlines. Explicitly validate connections between floors and between exterior entrances and interior destinations.

Introduce **designer-authored hero modules** for major moments (lobby, atrium, platform, rooftop arena, collapsed hall), plus procedural variations of smaller structures. Hand-authored modules and generative geometry must mix in a single level.

---

## 8. Stage D — Semantic architectural geometry (not tiled boxes)

Create a reusable **parametric architecture kernel** that generates original shapes based on structure and style:

* Convex brush extrusion, insets/outsets, robust clipping/union decomposition as permitted, wedge/bevel/chamfer, splitting, offsets, mirrored/rotated components, polygon outlines, spline sweeps approximated by legal convex segments.
* Walls with door/window openings, wall thickness, columns, pilasters, beams, lintels, railings, pipes, ducts, trims, cornices, parapets and broken finishes.
* Ceilings: ordinary, vaulted/arched (profile-compatible segmented approximations), layered, suspended/damaged, catwalks and roof framing.
* Facade grammar per building type: floors, bays, corner treatments, window spacing, roof setbacks, balconies, awnings, shopfronts, signs, fire escapes, drainage, AC/external utilities.
* Non-rectangular structures: angled rooms, chamfered corners, polygonal plazas, curves made from controlled brush wedges; patch surfaces only where target editor/profile/parser/compiler supports them.
* Height transitions: split-levels, mezzanines, stairs, ramps, catwalks, rooftops and destructed floor connections.

Implement robust geometry validation after each primitive: positive volume, valid closed convex brush, legal face planes, UV mapping without broken texel scale, non-overlapping interior walls, sensible thin-brush guards and no accidental leaks.

The generator must not simply stretch or duplicate fixed cubes. Make geometry parameter-driven, with archetype-specific proportions and an authored language of composition.

---

## 9. Stage E — Physically coherent post-apocalyptic damage

Create a coherent damage simulation **inspired by** structural failure, not arbitrary random removal of brush faces.

Required damage types:

* Broken/missing wall segments, blown-out windows, shattered facade panels, exposed structural frames and rebar-like safe game representations.
* Collapsed floors/ceilings/roofs, fallen slabs, partial stair failures, rubble piles, broken sidewalks, buckled road edges and craters.
* Barricades, improvised partitions, reclaimed vegetation, eroded plaster, rust, water stains and local flooding.
* Roof holes, partial shell buildings, leaning/broken parapets, blocked windows, war damage and overgrowth for abandoned environments.

**Coherent damage modeling:** Sample a damage origin and spatial influence field (blast, collapse load, water exposure, fire path); propagate across semantically related surfaces and structures. Preserve structural plausibility, e.g. load-bearing pillars, damage gravity direction, falling materials; intentionally allow fantastical cases only by preset.

**Gameplay-first constraint:** Keep the mission path traversable. After damage, rebuild navigation/collision and either repair accidental blockage, reroute through an alternate path, or reject the candidate. Damage may purposefully create new routes, flanks, views, risk/reward and discoveries. Expose damage intensity, preservation, vegetation age, collapse concentration and blockage policy. **Never create visible openings that remain unexpectedly invisible-colliding** or logical doorways that are visually/physically sealed.

A destruction “hero event” is a _designed gameplay transformation_: e.g. broken bridge forces street→building→roof→street detour; collapsed stair opens drop shortcut; flooded tunnel shifts path to upper level.

---

## 10. Stage F — Art direction, materials, visual storytelling and dressing

Introduce structured `ArtTheme` and `PropRule` datasets with coordinated palette, weathering, architecture families, debris patterns, light temperature and foliage frequency.

Ship at least these **post-apocalyptic subthemes**:

1. Bombed European city / dense stone and brick.
2. Abandoned post-Soviet-style residential district / concrete panels and courtyards.
3. Overgrown industrial zone / tanks, warehouses, pipelines, service gantries.
4. Ruined hospital or research campus / corridors, wards and courtyards.
5. Flooded metro and subterranean infrastructure / platforms, tunnels and maintenance.
6. Evacuated modern downtown / office facades, shops and blocked streets.

**Do not reuse the same block and material palette as the only difference.** Each theme must affect road graph, building shape rules, interior grammar, damage distribution, prop vocabulary and lighting.

Art passes at multiple scales:

* **Macro:** skyline and landmarks, strong silhouettes, negative space, landmark framing and district identity.
* **Meso:** facade variation, balconies, stairwells, bridges, shopfronts, pipes, broken masonry, barricades, vegetation clusters.
* **Micro:** trim, cracks/decals or baked textures, trash clusters, cables, small rubble, broken glass approximation, signs, stains, moss and local clutter.

Object placement must be semantic: debris below a collapsed wall, papers near desks, pipes near service shafts, burnt wreckage near impact zone, grass where soil/moisture/light permit. Use spline, surface, volume and hand-painted exclusion masks; avoid a uniformly noisy scatter.

**Bundled assets:** deliver legal, original or license-compatible starter materials and several usable procedural/brush-built prop kits so defaults already look good without paid downloads. Support user asset packs via manifest/schema; graceful fallback if an asset is unavailable. Use models, patches and decals only if the target profile genuinely supports them. Generated brushes and baked textures must make a standalone playable base level. Do not promise photorealistic visuals without art assets.

Lighting pass: per-archetype lights, window daylight or sky where feasible, contrast between safe and risky spaces, focal lights and silhouette legibility. Respect compilation/runtime differences between Quake 1, Quake 3 and FTEQW. Place lights according to function and scene composition, not a uniform per-room rule.

---

## 11. Stage G — FPS level design and navigation analysis

**A generation is unacceptable if it is aesthetically impressive but uninteresting or unplayable.** Implement quality metrics and automatic candidate selection.

### Gameplay topology

* Main route with identifiable destination and at least one alternate route or loop at medium+ size.
* Optionally multiple elevations connected by stairs/ramps/doors/catwalks/rooftops.
* Loops with identifiable purpose: flank, escape, reward, shortcut or reveal, not decorative circularity.
* Doorways and passages sized for configured player hull and navigation abilities.
* Required start/end/objectives connected after doors, clutter and structural damage.

### Spatial/combat readability

* Sightline samples at player eye height, with long/medium/short distributions controllable per area.
* Cover positions that do not block corridors, purposeful arena nodes, flanking paths, sight blockers, visibility breakpoints.
* Readable landmarks and distinct spatial sequences; avoid long straight sightline repetition and indistinguishable corridor chains.
* Detect unfair spawn exposure, unreachable exits, isolated roofs, inaccessible stairs, dangling bridge sections and dead-end objective placement.
* Scene rhythm: compression → release, quiet → confrontation, interior → exterior, floor-level → elevated, dark → landmark light.

### Environmental narrative

Every medium+ preset must produce at least one **contextual spatial storytelling beat**, such as: evacuation bottleneck, temporary field hospital, survivor barricade, submerged station, ruined plaza, improvised rooftop crossing. Story markers can be disabled by designers but the default must include meaningful compositions.

### Evaluation and ranking

Build a multi-candidate loop: generate `K` candidates (configurable, e.g. 4/8/16); enforce hard constraints; compute a normalized score breakdown; retain/preview top candidates and allow user selection.

Required diagnostics: validity/reachability, player clearance, route length, loop/branch usefulness, verticality, room/road aspect diversity, distinct landmarks, line-of-sight diversity, navigation choice points, prop occlusion, damage path integrity, performance budgets, thematic cohesion and asset coverage. Expose weights and failure reasons, not only final score.

Preference scoring is a **heuristic**, not proof of fun. Include human playtest gates with a first-person walkthrough and explicit acceptance protocol.

---

## 12. Advanced UI integrated with ReyondBroom

Create a full dedicated **World Architect** window/workspace consistent with native Modern Light and Modern Dark.

**Primary navigation:** `Overview | Mission Graph | Site & Roads | Buildings | Architecture | Destruction | Dressing | Quality | Export`.

**Left panel:** Presets and archetypes; scope (Interior / City / Mixed / Arena); seed, generation scale, theme, region list; stage enable/lock icons.

**Center:** Live top-down site plan, navigable 3D preview, floor/roof cutaway, overlays (main route, loops, gates, landmarks, roads, parcels, collision, visibility, cover, damaged regions), first-person camera at player height.

**Right panel:** Contextual properties: dimensions, budget, road/plaza proportions, building types, floors, ruin level, art theme, traversal/encounter parameters, generated candidate score breakdown and validation warnings.

**Bottom:** Stage timeline / logs, seed gallery of alternatives, actions `Generate 4`, `Generate More`, `Select Best`, `Regenerate Region`, `Apply to Map`, `Export .map`, `Save Preset`, `Undo`.

### Designer controls

* Drag mission graph nodes / edit transitions and optional gates.
* Draw and edit roads using splines; reserve lots/park/plaza zones.
* Pin landmarks, roads, buildings and important rooms.
* Design “must pass”, “must avoid”, “no generation”, “keep authored geometry” and vertical link volumes in the live map.
* Brush/paint destruction intensity, debris/vegetation density, allowed areas and preserve regions (compatible with existing viewport tools).
* Regenerate only a selected block/building/room/detail layer while preserving locked upstream plan and edited geometry.
* Compare 2–4 candidate previews and their score breakdown; choose the best explicitly.
* Undo/redo the entire operation, plus preview-only changes before Apply; selection-safe replacement and cancellation.

Every UI control must have observable, tested behavior. Do not add fake quality sliders, buttons that modify only preview labels, or images presented as actual render results.

---

## 13. Algorithmic and spatial implementation details

Use different generators for distinct spatial domains. Recommended choices:

* Mission graph: constrained graph grammars / graph rewriting, A\*/constraint-driven embedding, stochastic search with hard rules.
* Road graph: hierarchical growth with context-sensitive steering, spatial partition/obstacle checks, road intersections, graph pruning, plot extraction; use robust planar graph operations.
* Interior: room adjacency graphs and high-level functional zoning, followed by constrained packing and corridors that respect occupancy.
* Local modular connections: reuse/extend existing WFC sockets **only as a substage** for compatible small details or authored modules.
* Architecture: grammar-based massing, facade splits and legal convex brush construction; do not force every architectural surface onto a single world grid.
* Damage: semantic stress/exposure fields + dependent fragments, with collision/navigation repair pass.
* Dressing: semantic marker queries, hierarchical density fields, slope/surface/volume filters and limited blue-noise/Poisson-like placement with spatial exclusion.
* Evaluation: navigation graph + LOS sampling + heuristic ranking; stage-level reject/retry and error diagnostics.

Implement independent, documented building and road archetype rules with deterministic PRNG; avoid global RNG hidden in calls. Keep safe memory/time bounds and validation before serialization.

---

## 14. Quake / MAP compatibility and export quality

**ReyondBroom is an editor for Quake-format content; the generator must create actual, editable content for the selected profile.**

* Output valid `worldspawn` brush geometry and appropriate point/brush entities, with profile-correct material names/UV projection; no accidental water-tightness/leak failures when sealed interiors are required.
* Respect `.map` convex brush geometry and Quake profile restrictions. Non-convex architectural forms must be decomposed into valid convex brushes. Curved surfaces can be legal segmented wedges or supported Quake 3 patches when tested.
* Support Quake 1 / Quake 3 / FTEQW with a capability table for sky, patch geometry, models, shader materials, transparent surfaces, entity classes, collision and BSP compiler behavior. Provide explicit fallbacks; never silently emit unsupported features.
* Prefer editor-native grouped brushes and entities, preserve standard UVs, safe group IDs, stable name/metadata and user-edited map geometry.
* Enforce established project budgets as **initial safe defaults** (current AutoLevel documents 16000 brushes, 16 MiB MAP import, coordinates within ±32000, and warns for Quake 1 BSP29 beyond roughly ±4096). Adapt to selected compiler and profile rather than universally hard-coding limits.
* Detect out-of-range and extreme-size candidates before output; offer tiling/chunking or smaller presets as appropriate. City-scale visual worlds may exceed a single classic BSP; favor compact, playable districts unless an engine-specific large-map pipeline is verified.
* Preserve editor-based compile/run profile and avoid modifying source `.map` on preview-only generation. `Apply to Map` is an undoable, atomic operation.
* Generate sealed playable interiors, correct collisions, safe stairs and slopes, and navigation-friendly world; verify using the user's configured compilers where available.
* Report unsupported/runtime-only details and offer a visual fallback, never claim functionality tested without real compile/play evidence.

---

## 15. Serialization, extensibility and migration

Store projects in a versioned human-readable format (e.g. JSON or the existing editor serialization conventions) containing:

`schemaVersion`, `worldSeed`, `profileId`, `presetId`, `stageSeeds`, `missionGraph`, `sitePlan`, `roadGraph`, `lot/building plans`, `style/theme`, `damage`, `assetManifest`, `pinnedNodes`, `authorOverrides`, `qualityWeights`, `selectedCandidate`, `generationMetrics`, `sourceRevision`.

Provide save/load, duplicate, presets, reproducible candidate seeds, stage locking, local regeneration, and safe forward-compatible error messages.

Legacy AutoLevel migration: audit `AUTOLEVEL_*` data format and previous module/asset repositories; support importing authored blocks and reading relevant preset data where possible. Keep backups and offer an explicit migration report for unsupported elements. Do not remove prior user work. Map/group references generated by previous versions must continue to load even if the UI is retired.

Expose an extensible registry of archetypes, grammar rules, templates and asset packs; deliver a documented example extension. Do not lock style and room definitions into large switch statements or source-only constants.

---

---

**FULL SPECIFICATION CONTINUES IN THE FIRST ISSUE COMMENT: sections 16–23 (presets, QA, phases, Definition of Done and source references). The single complete Markdown file is titled** `ReyondBroom_World_Architect_Spec.md`.


## World Architect specification — continuation (sections 16–23)

## 16. User-facing presets — finished, not placeholders

Ship **at least 8 complete example presets**, with real geometry, materials, level-design goals and QA fixtures:

1. `The Last Checkpoint` — ruined street checkpoint, barricade, flank through shop, rooftop overlook, street exit.
2. `Abandoned Hospital` — reception, wards, service route, collapsed central stair, alternate accessible upper-floor route, exit courtyard.
3. `Flooded Metro` — stairs to concourse, platform vista, parallel service tunnel, controlled water blockage, street exit.
4. `Overgrown Factory` — loading yard, warehouses, catwalk/gantry loop, production hall landmark, broken roof.
5. `Residential Courtyard` — apartments, interconnected courtyards, garages, roof/ground loop, improvised barricades.
6. `Collapsed Overpass` — multi-street urban traversal, partial bridge, wreckage, vertical detour through building.
7. `Dead Mall` — retail spaces, central atrium, service corridor shortcut, upper walkway and back entrance.
8. `Evacuation District` — small city blocks with a recognizable destination, branching travel routes, one enterable public building and skyline landmarks.

For all presets, use purpose-built proportions and structural semantics. They must **not** share identical layouts with only a different material palette. Include appropriate original/compatible starter assets or genuinely good brush-based equivalents. Present actual camera screenshots and playthrough notes for representative seeds.

---

## 17. Hard validation and acceptance tests

Automated generation must follow a `generate → validate → score → select → export` pipeline. Failed candidates must never be represented as successful final levels.

**Hard acceptance (100% on all accepted test seeds):**

* Entry and exit reachable through a collision-aware player graph; all mandatory route and prerequisite/gate dependencies satisfied.
* No accidental unreachable required rooms, dead-end mandatory gates, invalid stairs or inadequate hull clearance.
* Any collapsed/broken geometry still preserves allowed critical paths and visually corresponds to collision.
* No invalid brush faces, degenerate volumes, inverted normals where relevant, duplicate invalid geometry or out-of-bounds coordinates.
* Profiles export valid `.map` and roundtrip through the editor; where compiler is configured and environment supports it, compile successfully without fatal leak/geometry issues and launch if feasible.
* Saved generation regenerates deterministically from its seed and complete settings, unless changed external asset dependencies are explicitly noted.
* Undo, selected-region regeneration, other map editing and save/reopen are functional.

**Quality gates (measure, show actual numbers and tune):**

* Batch test at least 100 deterministic seeds per shipped preset, with documented failures/rejections and acceptance rates; candidates may be retried, but default presets must produce usable outputs without frequent manual fixes.
* Representative medium+ seeds include purposeful alternates/loops, readable landmarks, at least two distinct macro-space types, vertical routes where enabled, coherent damage and non-uniform art pass. Programmatically track applicable properties; qualitatively verify the remainder.
* Compare multiple accepted seeds, analyze structural variation (road topology, mission graph edit distance, room size distribution, building masses, paths, damage composition). Seed changes must yield materially different levels, not only shifted crates and wall colors.
* Score coverage must be explained per domain and not double-count mirrored metrics. Keep structured rejection reasons and known unsupported limits.
* First-person human review: inspect an interior, city and mixed map in actual ReyondBroom and compiled in-game view (where available), check sightlines, pacing, legibility and visual polish.
* Compare screenshots between versions to catch regressions in architecture and damage realism; store image evidence if environment permits.

Performance: target interactive preview for modest seeds; heavy stages should be asynchronous with progress, cancellation and per-stage caching. Baseline and report wall clock, memory, brush count, exported MAP size and compile time on agreed sample presets. Never promise a universal runtime threshold without measuring target hardware.

---

## 18. Security, legal and stability requirements

* Preserve existing MAP data on generation failure/cancel; write only when user confirms Apply or explicit save/export.
* Sanitize map/material strings and asset paths; prevent arbitrary files outside approved asset directories from being modified.
* No network calls or paid model dependencies required for core generation.
* Use original public-domain, permissive or compatible licensed materials and models; record provenance and licenses. Independently implement algorithms rather than copying Unreal/Dungeon Architect/CityEngine assets.
* Respect ReyondBroom's GPL obligations and review source dependency licensing.
* Keep runtime limits for brush count, recursion, candidate search, memory, file size and external data.
* Provide clear user-visible diagnostics, cancellation and deterministic fallbacks when assets, compilers or profiles lack capability.

---

## 19. Implementation phases — each produces a real working vertical slice

**Phase 0 — Repository audit and baseline:** Document architectural boundaries, current menu/plugin integration, exporter capabilities, profile compatibility, tests, migration/data safety. Generate baseline AutoLevel sample maps and store metrics/screenshots if possible.

**Phase 1 — WorldPlan and MissionGraph:** Typed IR, seed/substreams, route topology, candidate generation, graph validity, quality diagnostics and lightweight top-down editor. Produce a playable simple first slice and not merely a graph screenshot.

**Phase 2 — Interiors:** Semantic indoor floor plans, multiple archetypes and floor connections, brush architecture, doors/stairs, validated Quake export, first end-to-end in-game test.

**Phase 3 — Urban site:** Road graph, intersections, blocks, parcels, building envelopes, interiors accessible from roads, sidewalks and landmark planning; standalone city and mixed sample compiled.

**Phase 4 — Architecture & ruin:** Facade grammars, non-box geometry, partial destruction, rubble with verified collision, alternate pathways, damaged vertical spaces.

**Phase 5 — Theming and art dressing:** Six post-apocalypse themes, lighting, contextual prop placement, original starter assets, art quality and asset manifests.

**Phase 6 — Designer tooling:** Graph/street/volume editing, pin/lock/paint/regenerate controls, candidate comparisons, save/load, Undo/Redo and migration.

**Phase 7 — Quality optimization and replacing AutoLevel:** Automated 100-seed/preset suites, navigation/collision/compile checks, actual in-editor screenshots, performance profiling and documentation. Switch default UI only once the new tool demonstrably passes acceptance. Retain explicit legacy migration support.

Every phase must document actual delivered code, tests run, failures, limitations and sample outputs. Do not claim acceptance based on mocked previews or a single successful seed.

---

## 20. Mandatory coding-agent working rules

1. Inspect the entire relevant repository before choosing integration points. Confirm exact file paths, currently available API, build steps, plugin load mechanism, map export and supported game profiles.
2. Produce a short implementation plan, then **implement**; prioritize vertical slices that produce usable `.map` files and verify in the real editor. Do not end with only a proposal, UML or shell.
3. Use safe feature branches and targeted changes. Avoid mass rewrites of unrelated editor systems.
4. Introduce precise tests before/alongside algorithms; no unverified placeholder UI or fake scoring. All primary controls must be wired.
5. Keep the generator engine-independent in its planning stages, but export profile-aware and legal Quake brush geometry.
6. Make custom archetypes and seeds reproducible and persist editable configurations.
7. Preserve authored regions through incremental regeneration, detect and resolve overlaps and allow previews without map mutation.
8. Verify a concrete level under at least one supported profile, then add others with documented fallback capability.
9. Capture actual resulting screen images when possible. Report failures without pretending to have launched the app.
10. Update the README/user manual with a first-run workflow, advanced usage, presets, troubleshooting, licensing, compatibility and migration instructions.

## 21. Definition of Done (strict)

A fresh user can open `World Architect`, choose one of the delivered post-apocalyptic presets and a seed, generate multiple viable candidates, select a best candidate with a visible score breakdown, inspect/modify its mission layout, streets or room plan, generate and edit detailed building geometry with coherent damage and meaningful contextual props, save editable configuration, insert the result into the active map atomically, compile it for a supported profile, and walk a readable, interesting level with a landmark, functional route variation and some vertical/architectural interest where applicable.

Urban mode actually contains a street network, blocks, parcels, non-uniform building masses and accessible indoor/outdoor route links. Interior mode contains functionally plausible multi-floor architecture, not repeated one-cell corridors. Diverse outputs differ spatially and semantically, not just in random material/prop assignment.

If these are not accomplished, the work is incomplete. Do not replace the default AutoLevel menu prematurely. Mark unavailable profile features and missing art fidelity explicitly.

## 22. Required final engineering deliverables

* Working source code and native UI (`World Architect`) integrated with ReyondBroom.
* Semantic IR/planning architecture and documented extension API.
* Procedural geometry, city roads, interiors, damage and dressing generators.
* At least eight finished presets and compatible original starter materials/prop kits.
* Versioned save/load, explicit legacy AutoLevel migration, reliable `.map` exports and Undo/Redo.
* Automated generation validity + quality sampling and real performance evidence.
* Actual screenshots/sample MAP files and first-person play/compile evidence for interior, urban and mixed scenes.
* Clear table of supported game profiles and fallback limitations, plus release notes.

## 23. Research and source references

* Dungeon Architect — How it works: https://docs.dungeonarchitect.dev/unreal/start-here/how-it-works/
* Dungeon Architect — Snap Grid Flow: https://docs.dungeonarchitect.dev/unreal/snap-grid-flow/sgf-introduction/
* Dungeon Architect — Flow graph: https://docs.dungeonarchitect.dev/unreal/snap-grid-flow/sgf-create-flow-graph/
* SideFX Labs — tools including Building Generator and patterns: https://www.sidefx.com/docs/houdini/labs/
* SideFX Labs Building Generator Utility: https://www.sidefx.com/docs/houdini/nodes/sop/labs--building_generator_utility-2.0.html
* CityEngine — Procedural modeling and dynamic streets: https://doc.arcgis.com/en/cityengine/latest/get-started/get-started-about-cityengine.htm
* Unreal Engine — PCG Overview: https://dev.epicgames.com/documentation/unreal-engine/procedural-content-generation-overview
* Unreal Engine — Hierarchical/Partitioned PCG: https://dev.epicgames.com/documentation/unreal-engine/using-pcg-generation-modes-in-unreal-engine
* Unreal Engine — PCG Editor Mode (spline, paint, volume): https://dev.epicgames.com/documentation/unreal-engine/pcg-editor-mode-in-unreal-engine
* Parish and Müller (2001), Procedural Modeling of Cities: https://bibbase.org/network/publication/parish-mller-proceduralmodelingofcities-2001
* QuakeForge — `.map` brushes/format: https://quakeforge.net/doxygen/mapformat.html
* ReyondBroom — Existing AutoLevel documentation: https://github.com/reyond9079/ReyondBroom/blob/main/docs/autolevel.md
* ReyondBroom — Default building library: https://github.com/reyond9079/ReyondBroom/blob/main/plugins/autolevel/al_default_repo.cpp
* ReyondBroom — Solver and quality: https://github.com/reyond9079/ReyondBroom/blob/main/plugins/autolevel/al_solver.cpp
* ReyondBroom — Map output: https://github.com/reyond9079/ReyondBroom/blob/main/plugins/autolevel/al_build_map.cpp

**END OF PROMPT.** Implement the full World Architect tool in the repository, not a detached reference implementation or decorative simulation.
