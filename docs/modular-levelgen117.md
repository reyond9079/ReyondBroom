# Modular procedural level generation

Open **Plugins > Procedural Level Generator**. The implementation was developed
after studying [AutoLevel](https://github.com/Al-Asl/AutoLevel), including its
block repositories, socket connections, weighted WFC solver, constrained volumes,
boundaries, multi-cell assets and selection rebuilding. This is an independent
C++ implementation for MAP editing; it does not load Unity scenes or prefabs.

## Start with the built-in library

1. Choose the grid's X/Y/Z dimensions, cell width/height, door width and origin.
   The default is a 6×6 single-floor grid.
2. Leave **Require connected room network** enabled for a connected layout.
   The generator constrains a spanning tree before solving neighboring sockets;
   it does not cut random doors through the solved result.
3. Choose **Generate preview**. The solver runs in a background worker with
   cancellation, entropy-based weighted choices, propagation and backtracking.
   The editor remains usable. A contradiction never applies partial geometry.
4. Inspect the preview and choose **Apply to map**. The result is ordinary
   editable brushes and optional lights/player start. One Undo reverses import.

The built-in modules are cross/T/corner/straight/end/closed rooms, Solid and
Empty. Floors/ceilings are generated inside the cell boundaries. Disable
ceilings for open layouts; compiler open-map settings are still independent.
Custom multi-floor layouts need modules with actual vertical openings and
matching +Z/-Z sockets. Matching sockets cannot invent stairs in custom art.

## Modules and sockets

In **Block library and weights**, choose a module and edit its weight, group and
six socket IDs. Equal IDs on opposite faces match. `0` means a closed side.
Weight `0` disables a module. Group weights multiply the module weights.
90-degree rotations rotate geometry and socket directions together.

To add your own module:

1. Select 1–64 whole brushes. Select geometry only, rather than a face or patch.
2. Choose **Capture selected brushes as module**.
3. Set the module's six socket IDs to describe its actual openings. The generator
   checks connection IDs; it cannot infer the traversability of arbitrary art.
4. Set its cell footprint and rotation option. A multi-cell module is represented
   by linked internal cell states and is always placed as one complete module.
   Its geometry is emitted only once. Parts cannot spill outside the grid.
5. Save the library/settings.

Captured brush dimensions and Valve 220 UV axes are retained through placement
and rotation. Geometry is not resized to fill a cell. Make it fit the configured
footprint; the generator reports an error if it does not fit.

## Constraints and partial rebuilding

**Boundary constraints** specify the required outside socket for each of six
grid faces. `0` closes the outside; `-1` accepts any external connector.

**Volume constraints and pinned modules** restrict a box to a group or module.
Coordinates are cell indices; maximum coordinates are exclusive. Multiple boxes
intersect their restrictions. Contradictory restrictions produce an error with
the affected cell, without changing the map.

After generating a layout, set a box in **Rebuild region** and choose **Rebuild
region**. Cells outside it retain the previous solution, including their socket
constraints. A new seed changes the interior. **Use selected brush bounds as
region** converts the current selection bounds to grid cells.

Partial rebuilding edits the preview. **Apply to map** normally inserts a new
layout. To replace an already-applied layout, select only that earlier layout
and explicitly enable **Replace current selection on Apply**. The replacement
is one undoable transaction. Unselected map objects remain intact.

## Save and export

**Save library / settings** and **Load library / settings** use the editor's
`ftebroom/data/plugin-levelgen-settings.txt`. Captured module geometry is embedded
there. The plugin restores saved settings on startup.

**Export MAP to plugin data** writes
`ftebroom/data/plugin-levelgen-export.map`. After Apply, normal editor export
tools can export the generated geometry in their supported formats. Unity's
AutoLevel FBX exporter and Unity-specific component/layer tooling are not bundled.

Budgets are explicit: 4096 grid cells, 256 rotated part states, footprints up to
4×4×4 cells, 128 constraint boxes, 16000 brushes and 16 MiB per MAP import.
The worker has time/backtracking/memory limits and a Cancel button. Add constraints
or reduce the library/grid when a budget is reached.
