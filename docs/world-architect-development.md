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

Apply uses an independent staging filename and an acknowledged QC transaction,
with execution-time map-epoch/profile guards. Missing/invalid files and stale
requests report errors rather than success. One Undo/Redo covers the complete
insertion; existing authored geometry is retained. Region replacement is not
exposed yet.

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

The current development evidence (external QA folder, not a public release)
records 800/800 input batches with at least one accepted candidate, 6 rejected
attempts out of 3200, and a real Quake 1 hospital `qbsp`/`light` run with no leak
and no empty lightmaps. Real UI generation/export, Apply acknowledgment,
authored-content preservation and one Undo/Redo have passed on native Linux.
These results do not certify a finished generator, Windows/SDL2 parity or art
quality. The tests must be rerun for subsequent changes in the relevant stages.

## Remaining acceptance work

The mission-room realization needs richer gates, pacing and intentionally
different optional branches. More road patterns, archetype-specific interior
families, non-box massing, hero ruin events, material/weathering detail,
and the eight finished presets still need implementation and visual evaluation.

Stage locks, pinned-region preservation, graph/road/volume editing, painted masks,
selection-safe regional replacement, candidate comparison panes, legacy module
migration, extension-pack validation, collision/gameplay metrics beyond the
initial anchors, and complete three-profile compile/play evidence remain open.
Unsupported lock fields fail explicitly instead of silently ignoring designer
intent. Quake II/GoldSrc compilation is not certified. No default AutoLevel menu
entry may be retired before the complete Definition of Done is met.
