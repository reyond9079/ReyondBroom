# World Architect development preview

RB-7 is still in development. AutoLevel remains available and remains the default
generator until the complete specification passes acceptance. The preview below
is implemented in native C++/ImGui; generation needs no Python, external editor,
cloud service or language model.

## First run

1. Open a brush map under Quake 1, Quake III or FTE Custom.
2. Open **Plugins → World Architect (development preview)**.
3. Choose a prototype preset, seed, scale, damage and dressing density.
4. Click **Generate candidates**. Preview does not change the authored scene.
5. Select a candidate. Inspect Overview's 3D view, floor cutaway, mission/site plan,
   rejection diagnostics and quality measurements. **Walk from spawn** uses
   grounded collision and step checks; drag with the right mouse button to look.
6. Save the editable project or export its MAP and assets. Use **Apply to current
   map** to insert editable geometry in one map Undo/Redo transaction.

The eight registry entries are still prototype configurations, not eight
finished showcase maps. Hospital and metro currently have the most specific
structural events. See [development status](world-architect-development.md) for
the delivered and remaining work.

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
fail explicitly. Directed transitions constrain the validated mission traversal;
they do not yet instantiate physical engine gates.

Architecture changes the real circulation grammar, floor count, minimum room
count, building dimensions and gallery width. These fields inherit the preset
until overridden. Site & Roads changes street width and segment endpoints;
road brushes, curbs and navigation change together. Edits that cross a building,
leave the sealed site or disconnect required navigation are rejected. Unlock
the relevant upstream stage before editing it.

In Destruction or Dressing, choose a field type and strength, then drag a
rectangle on the floor plan. Preserve suppresses structural destruction and
props; damage/dressing fields override their local amounts, and avoid volumes
reject building overlaps. **Preserve selected building** paints its full
envelope. Fields persist in the editable project. Dressing also selects the
original art palette; a new palette requires unlocking Architecture and
unpinning buildings. Rich theme-specific grammar remains in development.

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
- Quake II and GoldSrc compiler/play acceptance is not certified for this preview.
- Opening the preview does not overwrite or silently migrate AutoLevel settings,
  modules or maps. Use the explicit migration bundle for backup/extraction; keep
  the existing AutoLevel route for its WFC authoring and solver semantics.

For extension authors, see the [registry interface](world-architect-registry.md).
Original procedural assets are distributed under GPL-3.0-or-later. The reused
convex brush kernel retains its AutoLevel MIT notices.
