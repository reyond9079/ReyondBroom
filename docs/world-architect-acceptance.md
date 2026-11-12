# World Architect acceptance evidence

This report supplements the [user guide](world-architect.md) and [RB-7 specification](specs/RB-7-world-architect-spec.md). Architecture/export were validated at `049be4c`; `88cce47` additionally repairs automatic navigation across stair-shaft corners. Independent old/new binaries produced byte-identical MAP output for 48 fixed preset/seed/profile inputs. Menu promotion preserves the explicit legacy route. Merge and release acceptance are recorded separately in PR45.

## Final generation and compiler corpus

The final post-repair corpus accepted **2400 batches / 9600 candidate attempts**: 100 deterministic seeds for each of eight presets under Quake 1, Quake III and FTE. There were **63 rejected candidates (0.66%)**, with structured rejection reasons retained in the QA logs, and **no empty batch**. Each format compiled and lit all eight representative outputs: **24 real compiler pipelines**. Quake 1 water leaves and Q3/FTE water shaders were inspected.

Every preset produced 100 distinct room/architecture fingerprints. Urban presets also produced 100 distinct massing and road fingerprints; hospital, metro and mall intentionally retain their scenario envelope while varying interiors. Mission fingerprints vary across 80–92 of the 100 seeds, depending on preset. These measurements describe structural changes, not texture-only differences.

The profile pipelines took 605.01 seconds (Quake 1) and 1037.55 seconds (Quake III/FTE), running concurrently with actual editor/game QA. These clocks include fixture compilation, deterministic replay, validation and representative BSP/light builds. A standalone final-core evacuation/seed1 sample took 0.179 seconds and 9668 KiB peak RSS, excluding the editor, GL and compilers. These are development-container measurements, not a universal runtime promise. The earlier complete pipeline's compiler-inclusive memory/wall-clock baseline is retained in the JSON evidence. Quake 1 sample results:

| Preset | Brush range | Mean MAP MiB | Mean generation ms | Sample BSP/light s |
| --- | ---: | ---: | ---: | ---: |
| checkpoint | 791–1137 | 1.36 | 151.9 | 3.36 |
| courtyard | 771–1110 | 1.28 | 169.7 | 1.86 |
| evacuation | 722–1164 | 1.20 | 115.5 | 3.32 |
| factory | 982–1256 | 1.49 | 171.8 | 4.65 |
| hospital | 965–1089 | 1.30 | 104.1 | 2.06 |
| mall | 669–748 | 0.91 | 75.0 | 1.86 |
| metro | 496–556 | 0.67 | 57.7 | 0.68 |
| overpass | 789–1222 | 1.32 | 167.8 | 2.36 |

[Machine-readable measurements and capture provenance](world-architect-acceptance/measurements.json) include the other formats and detailed distributions. The pipeline uses asynchronous stages, cancellation, bounded candidate search and input-aware prefix caching.

## Real editor and game checks

414 final core contracts passed. Actual SDL editor checks include all eight presets applied as editable owned geometry and undone in one transaction (25 checks), six scenario previews and candidate comparisons (20), modes/counts/patterns/volumes (20), road sketch/curve/room furnishing/field editing (9), and stair-light export/Apply/Undo (8). Earlier focused runs also cover regional replacement, pins, migration, authored modules, physical gates and save/reopen; their checkpoint details remain in the engineering journal.

A seven-face rubble brush was rejected by the editor despite compiler acceptance: a bevel passed 0.027 units from a corner, below engine clipping/merge tolerances. Construction now keeps a robust corner margin. The original and corrected brushes were imported independently, and every preset subsequently passed real Apply/Undo. The shared editor geometry validator was not weakened.

Actual Ironwail 0.8.2 OS-input walkthroughs completed all 108 hospital, 60 metro and 169 repaired city-route waypoints without teleport or noclip. The city used the current compiled Quake 1 geometry and the corrected navigation route from its real spawn to the mission destination. Checkpoint's earlier controller attempt remains partial and is not claimed as a full route pass.

The original city walk exposed an unsafe diagonal after a stair flight. A controlled exact-position reproduction confirmed a real navigation defect: full-radius hull support could touch the adjacent landing while the player's centre was over the shaft. Automatically added proximity links now require centre support at eight-unit intervals; deliberate staircase links retain step/hull support. An adversarial support case and the real city's long-link regression pass. The corrected actual game walk passes the formerly failing landing and reaches the destination. All eight best candidates again passed real editable Apply/Undo (25 checks).

Controlled physical-door checks confirm blocked-before-activation and passage-after-activation. Their setup used setpos, then explicitly disabled noclip before collision tests; they are separate from the complete no-teleport walks.

[Repaired upper landing](world-architect-acceptance/city-landing-after-fix.png) · [City mission destination](world-architect-acceptance/city-destination.png). These two new captures are whole desktop frames with unchanged game pixels.


The user reviewed real hospital, metro and city captures inline and replied **«Принимаю текущий стиль»** on 2026-11-12. This records visual-style acceptance; it does not claim a complete human walkthrough. Captures preserve game pixels; only blank desktop margins were removed. Hospital highlights remain bright in the demonstrated style.

![Hospital in game](world-architect-acceptance/hospital.png)
![Metro in game](world-architect-acceptance/metro.png)
![City in game](world-architect-acceptance/city.png)
![Exterior stair lighting](world-architect-acceptance/stair-lighting.png)

Actual editor examples: [checkpoint](world-architect-acceptance/editor-checkpoint.png), [factory](world-architect-acceptance/editor-factory.png), [courtyard](world-architect-acceptance/editor-courtyard.png), [overpass](world-architect-acceptance/editor-overpass.png), [mall](world-architect-acceptance/editor-mall.png), [evacuation](world-architect-acceptance/editor-evacuation.png).

## Compatibility, preservation and limits

Generation, actual MAP compilation and editor roundtrip are verified for Quake 1, Quake III and FTE Custom. Quake II/GoldSrc compiler/play certification is unavailable and is not promised. FTE uses semantic gates by default unless standard entities are explicitly selected. Required vertical volumes constrain existing circulation; they do not magically carve arbitrary new stairs. No-generation volumes reject occupied architecture while retaining shared ground/sealing geometry. Exact legacy WFC solver semantics remain accessible through Legacy generators and are reported as unsupported by conversion rather than silently reinterpreted.

Original procedural materials/props require no paid asset download. Their GPL notices and the reused MIT convex-kernel notices are retained. Export bundles contain required generated textures/WAD/shaders; copyrighted game PAKs used for private game testing are not distributed. Assets have versioned identities and differing existing files are not overwritten. Existing maps, legacy settings and authored modules remain intact. No Qt was added.

Final platform acceptance includes focused Windows/Linux/Linux SDL2 source checks and, for the published version, mandatory packaged startup/version/archive/source/checksum gates on all three platforms. After promotion, the rebuilt SDL editor passed 26 workspace/Apply/history/save checks. Actual OS clicks reopened World Architect through its primary entry and AutoLevel through Legacy generators. [Primary menu](world-architect-acceptance/plugins-menu.png), [legacy submenu](world-architect-acceptance/legacy-menu.png), [legacy workspace](world-architect-acceptance/legacy-workspace.png). Thirty-two CI policy contracts passed. A source test-driver failure is distinguished from a product failure; prepared binary reuse verifies matching native, QC, engine and build recipe inputs before running corrected OS input.


## Windows module-source assertion

The complete Windows source job passed legacy, generation, Apply/history,
regional updates, pins, designer controls, all six World art themes, scenarios,
road authoring, quality, planning, stair lights and physical gates. Its authored
module assertion then compared against the wrong fixture representation:
`original-module.map` was written by a C++ text stream and gained CRLF, while
the imported JSON's MAP payload retained LF. The actual exported source was
3820 bytes and matched that JSON payload exactly (SHA-256
`b1cfc755ffec853d676929ba1b8f9382c50339fcd35d2fd5c85ecd47c383d73c`);
the auxiliary text fixture was 3852 bytes.

The assertion now compares to the actual imported payload without normalizing
exported bytes. The focused continuation reuses the verified unchanged compiled
editor and portable fixtures, then runs the complete corrected module/history/
reopen case and the remaining single dark/1280 placement case. Its final result
is recorded in PR45. The editor/exporter was unchanged by this CI correction;
all normal build recipes and native/QC/engine inputs must match before reuse.
