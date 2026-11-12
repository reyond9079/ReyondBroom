# World Architect

World Architect is the primary level generator in Plugins. AutoLevel remains
available under **Plugins → Legacy generators**, with its data and settings
preserved. The workspace is implemented in native C++/ImGui; generation needs no Python, external editor,
cloud service or language model.

## First run

1. Open a brush map under Quake 1, Quake III or FTE Custom.
2. Open **Plugins → World Architect**.
3. Choose a preset, seed, scale, damage and dressing density.
4. Click **Generate candidates**. Preview does not change the authored scene.
5. Select a candidate. Quality compares up to four actual spatial plans and their
   individual score breakdowns side by side; selecting one also selects its
   Overview 3D geometry. Inspect the 3D view, floor cutaway, mission/site plan,
   rejection diagnostics and quality measurements. **Walk from spawn** uses
   grounded collision and step checks; drag with the right mouse button to look.
6. Save the editable project or export its MAP and assets. Use **Apply to current
   map** to insert editable geometry in one map Undo/Redo transaction.

The eight presets have scenario-specific physical structures, including roof
escape stairs, a checkpoint booth/barricade, a factory crane and presses, garage
openings, a genuinely broken overpass, a continuous mall service corridor, and
an evacuation portico/beacon. See [acceptance evidence](world-architect-acceptance.md)
for measured generation, editor, compiler and game results and their limits.

## Iterate while preserving earlier stages

After selecting a valid candidate, choose **Freeze through → Architecture**.
This captures the selected mission, site and architecture recipes. Subsequent
seed changes affect later passes. Scale and preset controls are disabled while
their upstream stages are frozen. Freezing through Damage or Dressing preserves
those results as well. Choose **None** to unlock the plan.

The project stores these recipes, not just a reference to an in-memory result.
Opening the file on another supported platform reproduces them from its saved
registry snapshot. The Quality panel lists stages actually reused from cache;
their displayed construction times are marked as original times. Navigation and
MAP validation always run, including after a player-hull change. A frozen
candidate never exempts new collision requirements.

**Undo settings / Redo settings** changes generator configuration independently
of map Undo/Redo. Changing configuration makes the old candidate stale; regenerate
before export or Apply.

## Preserve a complete building

In **Buildings → Building details**, select a building and click **Pin selected
building**. Its accepted Architecture, Damage and Dressing recipes are retained
independently. New seeds can vary the other buildings. Mission and Site remain
frozen while pins exist: moving their roads, rooms or lots would invalidate the
retained building. Navigation and export still validate every candidate.

Pins are saved inside the editable project, including the registry fingerprint;
they need no external mesh cache. Pin/unpin uses settings Undo/Redo. **Unpin
selected building** releases its recipes and any later global stage freezes;
the shared Mission/Site baseline remains until explicitly unlocked. Unpin all
buildings before choosing **Freeze through → None**, changing presets or
retargeting the profile. Local detail regeneration is disabled for a pinned
building.

## Regenerate a building's details

Apply a selected candidate once. In **Buildings → Building details**, choose a
building and detail seed, then click **Preview selected building details**.
The mission, site and architecture recipes stay frozen; only that building's
Damage/Dressing streams change. Other buildings retain their original streams.
Review the candidate and use **Export → Update selected building only**.

Generated regions have separate owned groups. Updating retains the group's
identity and authored parent/layer; other groups and worldspawn stay intact.
Hidden, locked or manually changed contents are rejected before replacement.
Snapshots inside the MAP retain ownership across save/reopen without requiring
external baseline files. Comparison allows reconstruction error at engine float
precision (normally 1/1024 unit; eight float ULPs at large coordinates), while
entity properties, materials, contents and topology must still match. This is
not a force-overwrite operation. One map Undo/Redo covers the replacement.

**Update this project's regions** replaces the complete tracked world, including
added/removed regions. **Apply** adds a new project; trying to add the same
project again is rejected. A project from a different preset/site cannot replace
one building of an incompatible architecture; update the complete world instead.

## Author the plan before Apply

The workspace has **Overview, Mission Graph, Site & Roads, Buildings,
Architecture, Destruction, Dressing, Quality and Export** panels. Save/open
project controls live in Overview. Settings Undo/Redo remains separate from map
Undo/Redo, and export requires regenerating stale candidates.

In Mission Graph, drag a beat onto a functional room on the selected floor.
The point snaps to that room's player anchor; generation reserves it before
building the walls and reassigns unpinned beats around it. Add optional branches
or edit mission transitions. Invalid disconnections and two pins in one room
fail explicitly. Directed transitions constrain the validated mission traversal. Activation
prerequisites can also instantiate supported physical doors and zones as
described below.

Architecture changes the real circulation grammar, floor count, minimum room
count, building dimensions and gallery width. These fields inherit the preset
until overridden. Site & Roads changes street width and segment endpoints. Enable **Bezier road
curve** and edit its XY control to bend the actual swept street; road brushes,
curbs, collision checks and navigation use the same tessellated path. Edits that cross a building,
leave the sealed site or disconnect required navigation are rejected. Unlock
the relevant upstream stage before editing it.

In Destruction or Dressing, choose a field type and strength, then drag a
rectangle on the floor plan. Preserve suppresses structural destruction and
props; damage/dressing fields override their local amounts, and avoid volumes
reject building overlaps. **Preserve selected building** paints its full
envelope. Fields persist in the editable project. Dressing also selects the art
theme; changing it requires unlocking Architecture and unpinning buildings.

Six theme grammars change real geometry: masonry arch surrounds and chimneys,
exposed concrete buttresses, industrial pipes/flanges/stacks, clinical tile bands
and louvres, reinforced concourse vaults, or civic piers/capitals/radio masts.
Their original textures use different tile, panel, corrugation and weathering
rules. Room functions select treatment beds, machinery, supply shelving,
service counters, desks, benches and utility cabinets. Density controls actual
placements with spatial exclusion and route clearance.

Procedural materials and shader packs have versioned content identities; WAD
filenames also identify profile and palette inputs. Updated packs can coexist
with earlier exports. Existing differing files still fail preflight rather than
being overwritten. Classic material names retain the 15-byte WAD limit.

## Explicit AutoLevel migration

Open **Overview → Legacy AutoLevel migration → Inspect legacy file**. Choose an
`AUTOLEVEL_REPO`, `AUTOLEVEL_BUILDER` or `AUTOLEVEL_SETTINGS` version 1 file from
the legacy plugin's data folder. Inspection retains the source and active map.

**Save migration report and backup** publishes a bundle containing the exact
original bytes in `backup/original.txt`, the parsed legacy form in `parsed/`,
and repository blocks as individual `modules/block-N.map` files. Stored MAP text
and point entities are retained without reserialization. The report inventories
original material dependencies and unsupported solver settings. Keep the
original assets with extracted blocks and inspect/import them through the
editor's existing MAP workflow. This does not yet attach them to procedural
dressing rules.

After saving and verifying a builder backup, **Copy suggested builder seed**
copies its seed explicitly and participates in settings Undo/Redo. Different
algorithms do not reproduce the old WFC cell layout. Boundaries, solved layers,
side IDs, big-block assembly, weights and legacy editor settings remain in the
backup/canonical files. Referenced repositories/builders are inspected separately;
the importer does not follow external paths automatically.

Differing existing bundle files cause a preflight error instead of being
overwritten. Unknown versions and malformed files produce a failure report and
can still be backed up. Source files are limited to 8 MiB. Small module geometry
is validated within a bounded budget; larger or unsupported geometry remains
preserved with an explicit inspection diagnostic.

## Files, profiles and troubleshooting

- Export writes the MAP, `.world.json`, original TGA/WAD assets, shader definitions
  and their license notice. Existing differing assets fail preflight rather than
  being silently overwritten.
- Keep a project's exported assets with its MAP. Quake 1 uses the WAD reference;
  Q3/FTE use the included textures and shader-list entry.
- The project retains its game profile. Opening a project under another active
  profile requires an explicit retarget or switching the editor profile.
- A rejected candidate is a failure with a reason, not a successful empty level.
  Read the diagnostics; reduce scale/scope or adjust incompatible dimensions.
- Apply waits for an executed import acknowledgment. Stale map/profile requests
  and malformed staging files leave the map unchanged.
- Quake II and GoldSrc compiler/play acceptance is not certified.
- Opening World Architect does not overwrite or silently migrate AutoLevel settings,
  modules or maps. Use the explicit migration bundle for backup/extraction; keep
  the existing AutoLevel route for its WFC authoring and solver semantics.

For extension authors, see the [registry interface](world-architect-registry.md).
Original procedural assets are distributed under GPL-3.0-or-later. The reused
convex brush kernel retains its AutoLevel MIT notices.

## Local furnishing and exact volumes

Dressing's **Local room furnishing** chooses a real functional room and a new
furnishing seed. Previewing retains the accepted mission, site, architecture,
damage and other rooms. The frozen Dressing recipe changes only that room's
stream. After review, **Update selected building only** applies the owning group
through the existing manual-edit and Undo guards. Pinned buildings reject room
regeneration. A narrow room or zero density can legitimately have no eligible
placements; changing the seed does not bypass clearance/preserve requirements.

Saved influence fields expose exact minimum/maximum XYZ coordinates. Commit a
numeric edit by Enter or by leaving the field. Invalid/inverted volumes are
rejected without changing the saved settings; a successful edit is one Undo.

Authored brush packs and explicitly migrated legacy modules can be
assigned to functional rooms; see [module format and workflow](world-architect-modules.md).

In **Site & Roads**, drag empty plan space to sketch a pedestrian road. Drag an
endpoint or blue Bezier control to edit the recipe. Blue paths show edits;
Generate validates their bounds, building clearance and physical navigation.
In **Quality**, expand **Candidate count and quality weights** to tune accepted
candidate ranking. These weights never relax hard geometry or route checks.

## Activation prerequisites and doors

In **Mission Graph → Gate prerequisites**, choose an activation beat for a
transition's destination room. The prerequisite applies to every incoming
transition to that destination. Add an accessible optional branch for a switch
room where necessary; deadlocked activations and contradictory prerequisites
fail before generation. The original graph and choice are saved in the project.

**Gate realization** selects `auto`, `semantic` or `standard`. Auto realizes
Quake 1 and Quake III doors. FTE Custom defaults to metadata because its game
code is unknown; choose standard only with compatible `func_door` and
`trigger_multiple` implementations. Q2/GoldSrc remain uncertified and retain
semantic metadata with an explicit diagnostic. The standard backend opens the
door when the activation zone is visited; a colored light marks that location.
The door remains open. First-person preview blocks closed doors and unlocks them
on zone overlap. The game uses its actual animated door behavior.

Physical gates require an enclosed functional room. Garage entrances and mall
service rooms with two entrances need a more specific composition and report an
unsupported layout. Protected lining seals the gated room's windows, walls and
ceiling against damage-based bypasses. It keeps a single real door opening.
Engine targets are isolated per World Architect project so two applied worlds
do not activate one another's doors.

Apply/update preserves real door and trigger brush entities inside the owned
region. One map Undo/Redo includes their models. Ownership survives normal
save/reopen and rejects manual geometry or entity-property changes; generated
model bookkeeping names are compared through actual geometry, rather than
being mistaken for authored changes. One-way graph flags remain mission
traversal metadata; they do not invent an irreversible engine controller.

## Experience and spatial constraints

Overview offers Preset, Exploration, Adventure, Hub and Arena experience modes.
They change mission beats/branches/loops, the circulation grammar and initial
ranking weights. Target route length changes the measured route preference;
it never bypasses hard reachability checks. Authored mission graphs retain their
explicit topology. Unlock Mission and unpin buildings before changing modes.

Site & Roads offers Grid, Organic, Hybrid and Radial patterns. Organic streets
use the same bounded Bezier sweep as authored roads; Hybrid keeps main streets
straight and curves local connections. Radial sites arrange parcels around a
real octagonal gathering plaza and connect their entrances along lot boundaries.
Changing the pattern resets road overrides in an undoable settings transaction.
Presets choose different default patterns. Roads are checked against building
masses and sealed site bounds after curve tessellation.

Destruction/Dressing field tools also offer three hard planning constraints:

- **must-pass:** reserve a supported player anchor and visit it in the validated
  route before returning to the destination;
- **vertical-link:** require a continuous, collision-tested stair connection
  inside the exact XYZ bounds, spanning at least one 256-unit floor;
- **no-generation:** reject generated brushes or point entities inside the
  volume. Shared ground and the outer sky seal remain. Move the volume or revise
  the site if it overlaps content; this control does not silently erase a building.

These constraints belong to Site. Unlock Site to edit them. Vertical-link checks
an existing connection; it does not promise to carve a staircase through an
arbitrary existing layout. All fields save/reopen and use settings Undo/Redo.
Checkpoint and Residential Courtyard have both external fire stairs and an
internal roof return, so their rooftop landmarks participate in a genuine loop.
