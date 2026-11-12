# Authored World Architect brush modules

Module packs let generated functional rooms contain authored brush compositions.
Import the pack in **Dressing → Authored brush modules**, choose the furnishing
room, and select its **Room module**. Generate and review before Apply. Select
**Procedural furnishings** to release the assignment. Configuration changes use
settings Undo/Redo; saved projects embed the MAP source and assignments.

The pack is a JSON object with `schemaVersion: 1` and a `modules` array. Each
module has these fields:

| Field | Meaning |
| --- | --- |
| `id` | Unique portable identifier, e.g. `clinic-arch` |
| `profile` | 0 Quake III, 1 FTE, 2 Quake 1; Q2/GoldSrc remain uncertified |
| `function` | An exact generated room function, or `any` |
| `source` | Author/source attribution |
| `license` | The source author's license or an explicit provenance limitation |
| `map` | Original MAP text, embedded as a JSON string |
| `materialSlots` | Explicit source-material-to-theme-slot assignments, 0–5 |

Theme slots are ground/floor, wall, trim, metal, wood and accent. The generator
uses these explicit aliases to provide portable WAD/shader/texture resources.
It retains the source's original material names and UVs in the original MAP.
There is no implicit filesystem traversal, external compiler script or asset
download in the import operation. A pack may contain 1–16 modules, up to 128 KiB
of MAP source in total, at most 64 KiB per module. Each module supports up to
128 convex brushes and 32 light/semantic (`info_null`) point entities.

Placement translates the source bounding box to the room's center and floor.
It preserves brush dimensions, face planes and UV placement without rescaling
or clipping. The complete geometry must fit inside the room with a small wall
margin and must keep all reserved player anchors/connections clear. Oversized
or obstructing content fails explicitly. Imported entity grouping metadata is
replaced by this world's ownership, while original bytes remain retained.
Other runtime entity types need a future profile-specific realization; their
source is retained and assignment currently reports an unsupported feature.

The assigned module replaces procedural furnishings in that room. Architecture
and damage remain procedural; this feature does not replace an entire generated
building shell or reinterpret an AutoLevel WFC/socket graph. Building pins retain
their module recipes and their source dependencies. Room assignments cannot
silently move between incompatible profiles or disappear into missing rooms.

Export writes the original MAP to a content-versioned `modules/` filename and a
manifest with attribution, license and material aliases. The generated world
contains the translated brushes and portable theme materials. Source modules
with different contents coexist instead of overwriting an earlier export.

In **Overview → Legacy AutoLevel migration**, inspect and save the verified
backup first. Choose an extracted module and its explicit portable theme slot,
then **Import selected legacy brushes as a module**. This adds the original
geometry to the same module library. Its original material names remain in the
source; missing legacy license metadata is recorded explicitly. WFC adjacency,
cell boundaries and solver settings remain in the migration backup/report.

For a complete original example, `tests/world-modules.cpp` creates a three-brush
arch and a light with clear player access. `scripts/test-world-modules.py`
writes packs for the three certified profiles, checks source preservation,
assignment/Undo building blocks and replay, and compiles their actual MAPs with
BSP and lighting tools. The focused `--modules-only --module-fixtures ...` UI
scenario imports and assigns those same files using actual OS input.
