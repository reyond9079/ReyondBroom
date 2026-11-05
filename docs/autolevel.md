# AutoLevel

AutoLevel replaces the bundled Procedural Level Generator. Open **Plugins >
AutoLevel**. It is an independent C++17 implementation of the data model and
algorithm design of [AutoLevel by Al-Asl](https://github.com/Al-Asl/AutoLevel),
MIT, revision `f27c97ecab604168f7e4dad1d6a4669481edee36`. It produces editable
MAP brushes and point entities and requires neither Unity nor Unity prefabs.

## Three-step start

1. Open AutoLevel and leave **Dungeon** / **Reyond Dungeon** selected.
2. Press **Rebuild All**. Generation runs in the background; inspect its preview.
3. Press **Apply to map**. One Undo removes the complete import; Redo restores it.

The initial layout has 8x8x3 cells, measuring 2048x2048x576 map units. Its modules
include corridors, rooms, 2x2 halls, staircases, two-storey atriums and an upper
decoration layer with lamps/crates. Preview generation does not change the map.
Cancel and contradictions never apply partial geometry. Changes to the document,
profile, material or generator settings invalidate an old preview.

The plugin window resizes and scrolls. Five page buttons wrap across two rows;
foldout sections keep long inspectors manageable.

## Level Builder

**Builder** selects a saved builder. **New**, **New empty**, **Duplicate**,
**Delete** and **Name** manage independent instances. New empty uses the original
AutoLevel 5x5x5 unconstrained starting state. Duplicating clears boundary links.

**Settings** provides:

- **Show Groups**, **Max Iterations**, export chunking and **Export Size**.
- **Seed**: zero chooses a random seed. **Last seed** reports the result; **Lock**
  copies it into Seed. Fixed seeds reproduce layouts, including in MT mode.
- **Cell pick**: Random order, Lowest weight sum (AutoLevel) or Shannon entropy.
- **Keep largest connected component**, **Min coverage**, **Require all floors**.
  An arc-consistent result below quality targets carries an explicit warning.
- **World origin**, independent of the integer cell position.
- **Floor / Wall / Ceiling / Trim / Stair material** pickers. An empty slot uses
  the current new-brush material, or `fb_grey`. Captured custom modules keep their
  own texture names. Materials must exist in the configured game resources.
- **Add player start**, **Light scale**, **Preview** (Cells / Brush edges), and
  **Auto-advance layers**.
- **Preset**: Small 6x6x2, Medium 8x8x3, Large 12x12x4 (enables MT), Tower 5x5x6.
  Tower applies a persistent 1.5x stair-only weight multiplier. Other presets
  remove it; the shared repository and other builders are not modified.

**Blocks Repo** assigns the library. Its groups and layer data are reconciled when
the assignment changes. An unassigned builder cannot generate.

**Level** edits integer cell Position and Size, preserving data in the overlap
when bounds change. **From selected brushes** derives bounds from a selection.
The six axis buttons shift the cell position by one cell. With the current SDK,
**Edit bounds in viewport** enables Move, Scale or Rect handles.

**Selection** edits a box inside the level. Choose a group (or All), then **Add**,
**Remove** or **Set** to paint allowed groups into its input wave. Rebuild Selection
preserves every cell outside that box and checks compatibility at its borders.
The selection AABB and Show Groups overlays display the constraints. Old hosts
use monochrome Empty/Solid/group glyphs instead of colored volumes.

**Connecting** links builders on a chosen side. Select a neighbour and press
**Connect...**, or click a face and then another builder in the viewport. The
neighbour is aligned to the adjoining cell plane. **Disconnect** clears the link
and separates its bounds. Connected builders must use the same repository.

**Boundary Settings** chooses a neighbouring level and fallback allowed groups
for each side. **Group Settings** overrides weights independently of those allowed
groups. The built-in Dungeon uses Solid boundaries, Empty weight 0.3 and Solid
weight 0.05. Built-in Solid is a constraint state without exported geometry.

When the library has several layers, **R** rebuilds lower layers as well. **UP**
requires the current layer to be complete; **Down** clears the upper layer being
left. With R and Auto-advance enabled, Rebuild All builds the base and decoration.
**MT** runs deterministic parallel attempts. **Cancel** stops the worker.

**Clear** clears the selected layer or all affected layers. **Apply to map** imports
a fresh result. **Replace current selection on Apply** explicitly replaces the
selected previous group in the same Undo transaction; select that group first.

**MC** groups output into Export Size cubed chunks. Without MC, each module gets
its own group and connected pieces of a Big Block share an instance group.
**Export** writes `ftebroom/data/plugin-autolevel-export-BUILDER.map`; apply first
to use the editor's other export formats. **Save / Load** persist plugin data.

## Blocks Repo

This page selects, creates, duplicates and deletes repositories. **Reset built-in**
recreates Reyond Dungeon. The **Cell Size** and **Use Filling** controls define its
grid. Cells use Quake Z-up coordinates; the default is 256x256x192.

**Groups** and **Weight Groups** let you add, rename or remove custom entries.
Empty, Solid and Base remain fixed at the start. Renaming/removing groups remaps
builder wave masks by group name rather than by weight-group indices.

**Actions Groups** edits named sequences of RotateX/Y/Z, MirrorX/Y/Z and Flip.
Add/remove sequences and individual actions. Defaults include Full Rotate On Z,
Rotate On Z and Full Mirror. The unchanged author variant is always included.

The **Blocks** section lists name, variants, group, layer and Active state, with
Edit/Delete controls. Create a **New empty block**, **New block from selection**,
or **New Big Block**. Geometry capture accepts 1..512 whole brushes; it also keeps
selected point entities such as lights. Face selections and patches are rejected.

**Workbench** provides **Lay out repo in map**, **Remove workbench from map**,
Workbench origin (default -8192,-8192,0) and Spacing in cells. Lay out imports each
variant-0 module as an `AL block` group under one tagged repository group. Edit its
brushes normally, select them, then recapture on Block Asset; its workbench offset
is subtracted. Other variants and Big Blocks are represented by previews.
**Remove the workbench before compiling the playable level**: it is ordinary MAP
geometry, not a separate Unity scene.

**Repo Entities Settings** controls Draw Self Connections and Max Connections Draw
Count. **Validate repo** compiles in the background and reports states, layers,
unconnected sides and duplicate warnings. Save/Load and Export/Import repo use
versioned `plugin-autolevel-repo-*.txt` files in `ftebroom/data`. **Import levelgen
library** migrates the previous `REYOND_WFC 1` library, including sockets, rotation
choices, materials and clipped multi-cell geometry.

## Block Asset

Select the block and variant. **Recapture geometry from selection** replaces the
author geometry. **Generate fill from geometry** samples the eight cell corners
against its solid brushes. Groups and Weight Groups are edited independently.

Variant 0 is the author shape. **New variant action** and **+** add a variant with
that transform; **-** removes a non-base variant. Edit its ordered action list
with add, remove, Up and Down. As in AutoLevel, editing that list does not
automatically recalculate side IDs; use Connections To Variants on variant 0. Rotations use normalized cell space about the cell
center, so X/Y rotations in a non-cubic cell display a stretching warning.
Mirrors preserve outward planes and locked UVs. **Flip changes filling only**;
it never turns a solid brush inside out.

Each variant has six side IDs and a Weight. ID zero is a normal closed connector,
not a wildcard. With Use Filling enabled, the matching key combines that ID with
the four corner bits on its face. **Walkable**, **Walkable sides** and **Spawn
point enabled / Spawn point** describe physical connectivity and the player start.

**Layer** assigns an AutoLevel layer, distinct from a Z floor. Higher-layer
**Block Resolve** is Matching (same complete actions) or AllToAll. **Block Placement**
is Add, Replace or ReplaceFirst. Dependencies select eligible lower-layer variants;
omitting dependencies allows placement above the preceding Empty state. Missing
author layers are an error. Fillers carry layer relationships without duplicating
brush geometry on export.

Reset Layer Settings, Reset Connections and removal of banned/exclusive links
clear the corresponding metadata. On variant 0, **Layers / Weight / Connections /
Banned Connections / Exclusive Connections To Variants** propagate those values.
Changing base fill propagates transformed corner masks immediately.

**Edit Mode** offers the applicable operations:

- **Connection**: From side, To block, To variant and To side, then Connect, Ban
  or Exclusive. Connect allocates the smallest unused nonzero ID or merges existing
  connector classes. Ban/Exclusive toggle explicit exceptions. Matching connection
  lists show IDs and partners; exceptions can be removed individually.
- **Fill**: eight corner toggles in Top/Bottom rows, available on layer 0 when
  filling is enabled. The viewport exposes colored corner cubes.
- **Layer**: toggle lower-layer variant dependencies in the list or viewport.
- **None**: normal editing. **Select variants in viewport** separately enables picking.

Parts attached to a Big Block show a warning and disable conflicting author
controls. **Detach from Big Block** makes a variant independent. Non-base variants
also expose **Variant preview offset**.

## Big Block

Choose the compound asset, edit its Name and Layer, override Group/Weight Group,
and choose Actions Groups. Size is 1..8 per axis. Shrinking detaches removed cells.

**Cell X/Y/Z** selects a cell. **Selected** lists attached block variants with
Detach; **Attach block / Attach variant / Attach** moves a variant into that cell.
Internal connector classes are generated uniquely for each compound orientation.
The part's weight is divided by the entire grid volume, including empty cells.

**Edit Big Block in viewport** offers Edit Cell and Edit Connections for external
faces. The same Connect/Ban/Exclusive form is available below. **Create from
selection** derives a cell footprint and clips convex brush geometry into its
cells, preserving texture coordinates and point entities.

## Builders Window

Builders Groups are connected components of boundary links. Select a group and
enable its participating builders; all start enabled. Layer control, MT, Rebuild,
Clear, Save and MC/Export mirror Level Builder. Dependencies are resolved before
neighbours; independent builders can run in parallel. A failure cancels the entire
group result. **Apply group to map** imports one fragment and one Undo transaction.

## Viewport controls and compatibility

The extended SDK draws colored bounds, group volumes, workbench labels, side IDs,
face plates and connection lines in every visible 3D/2D view, behind editor panels.

- Move/Scale/Rect drag whole cells. Rect handles nearly parallel to the view are
  disabled. RMB cancels a drag and restores its full original bounds snapshot.
- Connecting: click the source builder face, then its neighbour. RMB cancels.
- Block Connection: click a source and target face. Hold Shift to allow a
  non-opposite target, 1 for banned or 3 for exclusive. RMB cancels.
- Fill clicks toggle corners; Layer clicks toggle lower dependencies. Big Block
  clicks select cells or edit an attached part's external connector.
- Alt+RMB still moves the camera. Escape, window close, changing the document or
  unloading the plugin releases capture so ordinary tools immediately work again.

Old v1 hosts without these optional callbacks retain editable forms, selection
capture and monochrome previews. Hosts missing earlier mandatory callbacks show
“This editor build is too old for AutoLevel” and do not run the generator.
Overlay gizmos are screen-space helpers without scene depth testing.

## Persistence, budgets and differences

Files are `ftebroom/data/plugin-autolevel-{settings,repo-UID,builder-UID}.txt`.
Magic headers are AUTOLEVEL_SETTINGS, AUTOLEVEL_REPO or AUTOLEVEL_BUILDER, version 1.
Quoted, nested text and RLE waves/layers preserve 64-bit state hashes. Unknown
fields are ignored. Autosave runs after two seconds idle and on shutdown. A corrupt
file produces a diagnostic and blocks autosave until explicit Save; it is never
silently overwritten.

Limits: 8 MiB per stored file; 512 blocks; 4096 compiled states; 31 groups; 16 layers;
64 cells per axis / 32768 total cells; 16000 output brushes / 16 MiB per import;
4..64 planes, volume above one cubic unit and coordinates within +/-32000 per brush.
BSP29 users receive a warning outside +/-4096; select BSP2 for larger Quake 1 levels.
Preview is limited to 16384 segments with a current-layer fallback and warning;
the complete geometry is retained for Apply/Export.

The solver bounds support memory to 512 MiB, rollback to two million journal events,
backtracking to 2048 per attempt, time to four seconds per attempt and 15 seconds
per solve, and parallel attempts/builders to eight. Budget errors suggest reducing
bounds or states. Custom repositories can be contradictory; a valid lower-quality
fallback is explicitly identified and is different from a contradiction.

Intentional changes from Unity AutoLevel: Z-up map units; centered, configurable
cells; MAP preview/Apply instead of immediate scene objects; chunk groups instead
of FBX mesh combination; random cell order by default; full bounded rollback;
connectivity/quality acceptance; explicit seeds and deterministic parallel jobs;
fill-only Flip; brush-based corner sampling; portable 64-bit hashes; duplicate
warnings; no repeated filler geometry; group-name wave remapping.

The specification includes actions in persistent hashes but requires symmetric
rotations to collapse. Semantic deduplication is therefore separate from hashing.
Author layer gaps are checked before built-ins can conceal them. Atrium shafts
are not walkable vertical portals. Empty staircase/atrium headroom has no standable
center; physical tests check doorway landings and galleries with the player hull.

The tuned built-in hall/stair weights are 4.32 and 8.64; atrium remains 6. Other
weights follow the specification. No solver special case forces modules into a
layout. The modules themselves are ordinary editable library assets.

## Automated verification

Run `python scripts/test-autolevel.py --output <scratch-directory>` with g++/MinGW.
The default checks all 30 geometry seeds. Add `--legacy-plugin <alpha118-levelgen
.dll-or-so>` to check compiled v1 compatibility. Logs include metrics for all seeds
and seed-1 ASCII floors. See [acceptance evidence](autolevel-acceptance.md) for
the requirement table, platform results and known limits. Human checklist items
were omitted as requested and are not represented as completed automated tests.
