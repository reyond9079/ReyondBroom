# World Architect development status

RB-7 is **not complete**. [The full specification](specs/RB-7-world-architect-spec.md)
remains the acceptance contract. AutoLevel remains the default generator and its
plugin, settings and authored maps are unchanged. The additional native window
is explicitly a development preview, not a finished replacement.

The current vertical slice contains an engine-independent typed WorldPlan,
mission and spatial graphs, convex architectural elements, damage fields,
asset placements, navigation graph and diagnostics. Seven addressable passes
use independent stable seed streams. A versioned external registry defines eight
prototype configurations and six original material palettes. These are test
cases, not the eight finished showcase presets required by RB-7.

The next interior/site pass reserves distinct functional rooms for mission
beats **before** brush generation. The room program expands to satisfy mission
demand, reserved rooms receive wider openings, and physical navigation must
visit the planned required rooms in order. Atrium/ring grammars leave real upper
floor and roof openings with supported galleries and handrails. Parallel
circulation has a solid divider and crossovers. A straight corridor keeps its
floor plate; the layout choice changes structural geometry.

Urban plots now come from deterministic binary subdivision with unequal
frontage and T junctions. Partition boundaries form the road network; masses
are inset within those plots, and each entrance has an explicit pedestrian
connection to its frontage. The original regular grid is no longer the only
site realization. Diagonal streets, bridge events and complete city-design
acceptance remain open.

Circulation scoring checks for an actual alternative walk separated from the
primary polyline by at least 64 units outside endpoint landings, with an actual
solid obstruction or floor opening between the paths. Dense edges or two
parallel lines in a wide empty corridor do not count. This measures a limited
same-floor property, not overall combat quality.

The hospital's first structural event removes the middle of its central ground
flight and provides two alternating exterior fire-stair flights with real
landings and doors. Zero damage restores the central flight; a preserve mask at
its base also suppresses this collapse. Its reception is an editable convex
semicircular annex with arc piers, actual openings and a planned outdoor exit.
The mission route must visit its required upper rooms before reaching that
exit. This is the first particular ruin event, not evidence that the other
seven showcase presets or the complete damage vocabulary are finished.

The station event supplies liquid-content geometry and original water pixels,
dry parallel platforms, upper galleries and a separate street exit. Navigation
does not assume swimming: solid platform barriers keep its sampled routes dry;
the liquid itself is excluded from solid collision. Compiler checks inspect
actual water leaves in Quake BSP29/BSP2 and water-content brushes in Q3/FTE
IBSP46, as well as checking material images and leaks. Circulation has actual
ceiling fixtures and point lights; classic profiles receive a directional sun.

An external development play check has loaded compiled hospital and metro maps
in Ironwail 0.8.2 with existing game resources and moved the player using OS
keyboard input. It found and corrected insufficient corridor illumination.
This is a short physical walk check, not the specification's complete human
review of pacing, sightlines and all eight finished presets.

The native ImGui workspace generates and compares candidates, displays the real
geometry in the existing dynamic-mesh renderer, previews floor cutaways and
provides a grounded first-person inspection camera. Preview/generation leave the
map unchanged. Save/load retains generation inputs. Export writes an editable
MAP, an editable JSON project, original TGA/WAD2 assets (WAD3 for the unverified
GoldSrc profile), and shader definitions.

`Freeze through` captures the selected candidate's actual per-stage recipes,
including each stage's own seed when a plan combines frozen and regenerated
stages. Mission through Dressing can be frozen with their upstream dependencies.
Navigation and Export always run. The bounded in-memory prefix cache reuses
matching outputs; its key includes the complete frozen recipes and registry
fingerprint. Save/reopen can reproduce a frozen plan with an empty cache.
Changing a recipe invalidates the cache, and changing the registry requires
unlocking. Settings Undo/Redo covers independent edits and freezing controls.

Apply uses an independent staging filename and an acknowledged QC transaction,
with execution-time map-epoch/profile guards. Missing/invalid files and stale
requests report errors rather than success. One Undo/Redo covers the complete
insertion; existing authored geometry is retained. Building detail regeneration
now retains all upstream recipes and other regions' streams. Owned groups allow
atomic regional or whole-project replacement, including save/reopen, one
Undo/Redo, and preflight rejection of manual edits, hidden/locked groups,
duplicate insertion and incompatible architecture. Portable numerical snapshots
inside group properties distinguish MAP float reconstruction error from edits.
Entity-slot reuse is scoped to this import path; ordinary clipboard behavior
retains its existing allocation path.

Complete building pins now capture the accepted per-region Architecture, Damage
and Dressing recipes, including composed stage seeds and influence fields.
Shared Mission/Site recipes stay frozen while pins exist; every other building
can still vary under new downstream seeds. Clean bounded recipes are portable
in project JSON, registry/profile mismatches and recursive payloads are rejected,
and pin/unpin participates in settings Undo/Redo. Unpinning drops stale later
stage freezes while retaining other pins. Building-level preservation is separate
from the broader designer toolkit described below.

The authoring slice adds the nine requested primary panels. Architecture inputs
override actual per-stage floor programs and dimensions rather than editing the
global registry. A frozen stage retains its own overrides and art palette.
Authored mission topology, directed transitions and room-position pins are saved
in project JSON. Pins reserve rooms before geometry, and the physical route now
follows a valid prerequisite-aware logical traversal rather than node-list order.
Road endpoint/width overrides alter street/curb geometry and navigation; edits
crossing building envelopes or leaving the sealed site are rejected.

Destruction/Dressing support bounded rectangular field painting and full-building
preserve volumes. Preserve affects structural stair events, masonry damage and
semantic room props. These controls do not claim the complete spline/volume
toolkit, engine gate entities or room-level replacement.

The art slice now adds six configurable facade grammars, material patterns,
weathering and semantic prop families. Masonry arches, concrete buttresses,
industrial risers/flanges, clinical bands/louvres, concourse vaults and civic
piers change actual convex geometry. Room functions select corresponding beds,
presses, shelves, counters, desks, benches and cabinets. Density samples use
stable room streams, spatial exclusion and protected navigation clearance.
These are original brush kits; finished showcase composition and final art
acceptance remain pending.

Art dependency identities version material/shader names and profile/palette WAD
files so new packs coexist with prior files. Classic names fit 15 bytes. A real
metro compiler regression exposed a sky-volume flood path for seed
9949357162622361994: changing names, disabling liquid contents or removing the
new vaults did not fix it; opaque backing around the classic sky enclosure did.
The seed now has a dedicated real BSP/light regression. Current checks pass 339
generator contracts, eight asset coexistence contracts, 72 three-profile input
batches and 12 representative BSP/light cases. FTE output was resumed after a
workspace disk-capacity failure, verifying every compiled-source hash first;
completed profile artifacts remain preserved in verified archives.
All six grammars also rendered in the actual SDL2 editor. A separate four-case
native export check verifies coexistence of old/new versions of one theme and
unchanged prior files. Twenty metro UI cases pass with the new art resources;
the executed Apply/production ExportMap snapshot passes real BSP/light and
binary liquid-content inspection. Full showcase composition remains open.

Explicit legacy migration now reads version 1 AutoLevel repositories, builders
and editor settings using the existing storage codec. It produces a byte-exact
source backup, canonical legacy data, unchanged authored module MAPs, material
dependency inventory and an explicit unsupported-feature report. It never
silently replaces WFC layout/connection semantics with unrelated controls.
Builder seed transfer requires a saved/verified bundle and uses settings Undo.
The active map and original files are retained; geometry inspection is bounded.
Fifteen migration contracts, the original AutoLevel storage test and nine actual
SDL2 migration scenarios pass. Migration has its own dependency-selected CI
suite; a codec-only change does not run the generator corpus or botanical tests.

Physical checks sweep the configured player hull against expanded convex brush
planes, verify support along links, handle stair risers, and require all generated
navigation anchors to be reachable. Logical mission validation searches actual
(position, acquired-prerequisites, visited-required-beats) states, including
one-way traps. Failed candidates are retained as rejection diagnostics and never
become successful levels. Scoring remains a heuristic, not proof of enjoyable
level design.

## Reproduce the focused checks

```sh
g++ -std=c++17 -O1 tests/world-architect-contracts.cpp -o /tmp/wa-contracts
/tmp/wa-contracts runtime/world-architect/registry.json
g++ -std=c++17 -O2 tests/world-architect.cpp -o /tmp/wa-corpus
/tmp/wa-corpus runtime/world-architect/registry.json /tmp/wa-maps '*' 100
python3 scripts/test-world-architect-ui.py /path/to/disposable/runtime /tmp/wa-ui
```

The corpus tests 100 input seeds per configuration, generates four candidates
per input, records every rejected attempt, replays the selected seed exactly,
checks JSON roundtrip and reparses every accepted MAP through the production
brush parser. Only the first representative MAP/assets per configuration are
written; the complete metrics/rejections are in `results.json`. An optional sixth
CLI argument selects the game profile (0 Q3, 1 FTE, 2 Q1, 3 Q2, 4 GoldSrc).

An earlier foundation corpus (external QA folder, not a public release)
recorded 800/800 input batches with at least one accepted candidate, 6 rejected
attempts out of 3200, and a real Quake 1 hospital `qbsp`/`light` run with no leak
and no empty lightmaps. Real UI generation/export, Apply acknowledgment,
authored-content preservation and one Undo/Redo have passed on native Linux.
That corpus predates subsequent architecture and regional work. The regional
slice has passed 322 contracts, 24 regional SDL2 scenarios and eight focused
actual SDL2 pin scenarios, including manual
edit protection, save/reopen, parent preservation and whole-project changes.
The executed metro Apply also passes its production ExportMap snapshot through
real BSP/lighting compilation; binary inspection confirms liquid brush contents.
The subsequent authoring slice passed 334 contracts, ten actual SDL2 designer
scenarios and four focused SDL2 painting scenarios. The designer cases check
changed exported planes, dimension Undo, dragging a beat into its real room,
directed mission authoring, preserve fields and exact save/reopen replay. Six
authored interior/road MAPs compiled and lit across Quake 1, Quake III and FTE;
the dimension case was also rerun with a width distinct from the preset.
These checks do not certify the complete eight-preset acceptance or final art
quality. Rerun the relevant stages after changes, and the final corpus before
replacing AutoLevel.

## Remaining acceptance work

The mission-room realization needs richer gates, pacing and intentionally
different optional branches. More road patterns, archetype-specific interior
families, non-box massing, hero ruin events, material/weathering detail,
and the eight finished presets still need implementation and visual evaluation.

Complete spline/volume editing, richer painted masks, physical gate realization,
room-level regeneration, candidate comparison panes, direct procedural use of
imported module packs, extension-pack validation, collision/gameplay metrics beyond the
initial anchors, and complete three-profile compile/play evidence remain open.
Malformed or incompatible pin recipes fail explicitly instead of silently ignoring
designer intent. Quake II/GoldSrc compilation is not certified. No default AutoLevel menu
entry may be retired before the complete Definition of Done is met.
