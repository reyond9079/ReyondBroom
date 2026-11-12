# World Architect development registry

The native generator loads `runtime/world-architect/registry.json`. Projects save
the complete registry snapshot, so editing the installed registry does not
silently change a saved project's dependencies. This API is a development
interface; the full extension-pack/migration contract is still in RB-7.

An archetype can be added without recompiling C++ by adding a preset with a new
identifier, an existing theme and one of the supported grammars. For example:

```json
{
  "id": "municipal-library",
  "name": "Abandoned Municipal Library",
  "theme": "brick",
  "archetype": "library",
  "story": "Reading galleries above the public hall",
  "layout": "atrium",
  "scope": 0,
  "floors": 2,
  "rooms": 6,
  "blocks": 1,
  "width": 1664,
  "depth": 960,
  "street": 160,
  "damage": 0.25,
  "beats": 7,
  "branches": 2,
  "loops": 1,
  "hallHalfWidth": 160,
  "roomFunctions": ["reception", "reading", "archive", "staff", "catalogue"]
}
```

`roomFunctions` provides the functional program, rather than a source-code
switch on the archetype name. `hallHalfWidth` controls corridor/gallery width.
Both are optional when reading early development projects; their defaults are
explicit and deterministic. Unsupported grammar names, malformed functions,
nonfinite dimensions, duplicate IDs and colliding material prefixes fail with
an error. Six-color themes use original procedural textures and must have
unique first seven characters. Generated classic material IDs are bounded to
15 bytes and include an art dependency identity.

Theme entries may also define `grammar` (`plain`, `masonry`, `brutalist`,
`industrial`, `clinical`, `subterranean` or `civic`), `weathering` (0..1),
`baySpacing` (96..384) and an RGB `lightColor` (three channels in 0..1).
Omitted fields retain the original plain palette behavior for old registry
snapshots. These grammars add real facade/utility/vault geometry and alter
material pattern generation; they are not labels for a palette-only switch.
New grammar IDs require native implementations and their convex/export checks.
Room-program functions also select the semantic furnishing kit. Every prop uses
the selected theme's original materials and records its procedural provenance.

The optional `ruinEvent: "collapsed-stair"` currently provides the hospital's
broken central flight, exterior fire stairs, reception annex and courtyard exit.
The central flight collapses only above 0.2 damage and outside a preserve mask.
This event requires an interior-sized envelope and is not an arbitrary prop
pack; unsupported event names fail explicitly. Additional events remain part of
the ongoing implementation. `ruinEvent: "flooded-platform"` supplies the metro's
liquid volume, dry platform barriers, upper gallery opening and ground-level
street exit. Classic liquid WAD names use the engine's `*`/`!` prefix; exported
TGA filenames use portable names without those characters. Q3/FTE receive a
water-content shader referencing the included original texture.

Supported grammar IDs are `spine`, `ring`, `atrium`, `parallel`, `courtyard`,
`offset` and `branched`. Interior `ring`/`atrium` provide open galleries;
`parallel` provides divided circulation. Urban `courtyard` changes the initial
subdivision direction; subdivision depth and dimensions determine subsequent
cuts. `offset` and `branched` currently share the binary subdivision machinery;
neither claims a completed bridge or evacuation event yet. A new grammar
algorithm requires native code and its collision/export tests.

Coordinates, brush counts, navigation nodes and MAP bytes have hard budgets.
An extension may be valid JSON and still fail generation because its requested
rooms, galleries and stairs do not fit. Such a candidate is rejected and is
never applied to the authored map. Run `scripts/test-world-architect.py` with
the modified registry and inspect actual editor/compiled output before shipping
an extension as a finished preset.
