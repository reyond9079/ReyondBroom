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

The full mission graph must drive spatial realization and meaningful alternate
routes; the initial grammar currently provides a shared corridor/stair structural
foundation. Distinct road patterns, parcel subdivision, archetype-specific
interior families, non-box massing, hero ruin events, material/weathering detail,
and the eight finished presets still need implementation and visual evaluation.

Stage locks, pinned-region preservation, graph/road/volume editing, painted masks,
selection-safe regional replacement, candidate comparison panes, legacy module
migration, extension-pack validation, collision/gameplay metrics beyond the
initial anchors, and complete three-profile compile/play evidence remain open.
Unsupported lock fields fail explicitly instead of silently ignoring designer
intent. Quake II/GoldSrc compilation is not certified. No default AutoLevel menu
entry may be retired before the complete Definition of Done is met.
