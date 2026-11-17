# Character Creator development validation

Run `python scripts/test-character-creator-all.py <evidence-directory>
--runtime <prepared-runtime>` to run the currently automated checks together.
On Linux run it on an X11 desktop or under `xvfb-run`. On Windows use an
active desktop. The script writes a JSON report, a Markdown summary, and
per-check logs. It exits with an error while a required acceptance gate
is still open. The core export fixture includes ten editable example
characters under `exports/legacy/characters`.
Pass `--sanitize` on Linux to run the parser, fitting, and export checks
with AddressSanitizer and UndefinedBehaviorSanitizer.
Pass `--legacy-fixture` with an existing Field Technician `.rbchar` to test
opening and saving the retired preset. The original file is only read.
Pass `--goldsrc-game` on Windows with the path to an installed Steam Half-Life.
The runner exports Casey using the Half-Life profile, compiles the synthetic
test room with the runtime's SDHLT tools, then runs Half-Life in a temporary
local mod. It verifies visible changes in three named animation sequences with
a fixed camera and saves their frames and the exported MDL hash. The test
requires a running Steam client and permission to create the temporary mod
directory inside the Half-Life installation. Game assets stay on that machine.

The suite writes `fit-matrix.csv` and `fit-contacts.csv` after sampling all 13
outfits with five body shapes and eight poses each. It checks actual
skinned triangles by region. A reported contact requires visual follow-up,
especially near cutout hair cards and garment seams. The geometric check
remains red while it detects intersections.

The optional third argument to the native fit executable writes a CSV of
contact locations in the posed mesh and matching bind-pose skin faces. Its
`visible_views` mask records unobstructed views from front, right, back,
left and above in bits 0–4. The depth test treats hair cards as opaque.
`test-character-placement.py <runtime> <output> --fit-fixture <document.rbchar>
--pose-phase 0.25` captures the specified point of the clip in the FTE preview.
Add `--benchmark` with a looping clip to measure 240 live frame intervals
after warm-up at 1920×1080 with Balanced detail and disabled VSync and FPS
cap. Pass the document as `--benchmark-fixture` to the combined acceptance
runner. The report records the OpenGL device, mean FPS and frame-time
percentiles for the full editor frame.

### Draft release check

The native release workflow builds candidate packages as Actions artifacts.
Once acceptance has tested those exact files, a separate dispatch takes the
candidate run ID and the evidence run ID. `rb21-release-evidence` must contain
`test-results.json` with the exact commit, a clean source flag, a completion
timestamp, a PASS result and hashed proof files for every ID listed in
`qa/rb-21/required-gates.txt`, and SHA-256 for all three package sets.
The development report above records partial results for diagnosis. Acceptance
needs the complete report and the required real application and game evidence.

Before any draft upload, the workflow checks both runs, the archive hashes,
and the repository owner's GitHub approval for the `rb21-release` environment.
Configure that environment with the owner as required reviewer and protect the
release branch and tags. Run `python tests/test-character-release-lock.py` for
the local dry run. Public release remains a separate owner action.

Run `python scripts/test-character-core.py --output <test-directory>` with
GCC 13 or MinGW available. The command checks the checksums of the base and
wardrobe, compiles the C++ test, generates models, then decodes the results
using a separate Python reader. The default run uses a temporary directory.
Pass `--stress` to fit and skin 1,000 randomized dressed characters with the
FTE profile and three body shapes. The reported fitting time measures the
CPU work to assemble each character.

The C++ fixture tests 79 rest joints, weighted transforms, 48 morph controls,
six facial expressions, four facial-hair choices, twelve outfits, seed
stability and 1,000 document round trips. The independent readers check skin
weights, changing samples, animation names, sequence counts and the classic
model limits. They exercise GLB, IQM, Quake MDL, Quake II MD2/PCX, Quake III
MD3 and GoldSrc Studio MDL.

The independent GLB and IQM readers also reconstruct the world-space bone
transforms at five shared sample frames for every clip. They compare position
and rotation after converting coordinate systems and report the largest
positional difference in Quake units. This check measures format agreement.

To validate the GLB with Khronos glTF Validator, install the `gltf-validator`
package into the disposable test directory and call `validateBytes` on
`character.glb`. The npm package is a development tool.

Use `--catalog-only` with `test-character-placement.py` to select ten presets,
inspect each tab and render all thirteen outfits, eight hairstyles and four
beard choices. The script captures the actual preview, checks pixel changes
while scrubbing a walking sequence and checks that the retired preset is absent.

Native smoke runs use a copy of the editor with `runtime/characters` installed
as `ftebroom/characters`. Start the editor, use **Plugins > Character Creator**
or the command `ftebroom_character`, and capture the actual viewport. To check
the command guard, start the local Mapping Jams QA flow and run the command
again while the Jam screen is active. The command should print that Character
Creator is available in offline projects. The screenshot and engine console
log should be recorded from the same run.

The standalone FTE model viewer loaded the IQM and classic fixtures with
`modelviewer models/<filename>` from a disposable game directory. IQM and
Studio and MD3 animation advanced between two captures. Half-Life Asset Manager
3.0.0 also rendered a moving exported Studio MDL. The original game engines
still need their own compatibility runs.

For the standalone IQM test, run the placement script with `--fit-fixture`
and `--export-iqm NAME` for Casey, Drew and Jamie from
`runtime/characters/examples`. Then pass the three `bundle` directories to
`scripts/test-character-fte-game.py --bundles`, along with the FTE executable,
FTEQCC and ericw tools. Run under Xvfb at 1280×800. The harness compiles a
sealed test room and starts a standalone game in a temporary directory.
QuakeC checks all 79 joint names and parents, updates each model from its own
fitted bind positions using the same `walk_forward` clock, and frees all
three skeletons on disconnect. The harness checks movement in each model's
screen area and records the loaded model hashes, screenshots and engine log.
The combined runner can export the three fixtures and run the game when
called with `--runtime` and `--fte-game-qcc` on Linux under Xvfb.

Run `python scripts/test-character-placement.py <runtime> <test-directory>`
on an active Windows desktop, or under `xvfb-run` with `xdotool` and Pillow
on Linux. It copies the runtime and uses OS input to exercise Create, Save,
transform preservation, map Undo/Redo, deletion and reopening. It captures
the separate panes and checks that switching back keeps the editor running.
Use `--compile-assets` to build a sealed Quake III room via the toolbar and
check model dependencies in the stage directory. The harness writes the
actual map snapshots, editor log and screenshots into the test directory.
Use `--startup-placement` for the untitled FTE Custom startup scene. This
checks that placement creates a single `func_model` with an IQM and its skin
material while leaving the map untitled.
Windows (GeForce RTX 4060 Laptop GPU), Linux GL and Linux SDL2 passed
placement history and deletion checks. All three compiled the room through
the toolbar, verified the staged assets byte for byte, and opened the BSP
with the character visible. The harness copies the checked BSP and model
bundle to `quake3-runtime`. OpenArena 0.8.8 renders a static prop from the BSP.
Under `devmap`, use `testmodel` with the same loose MD3 and `nextframe` to
advance it through ranges from the bundle's `.framegroups` file. A mod must
advance the frames during ordinary play.

Run the placement script with `--fit-fixture` and `--export-goldsrc` to
export a Studio MDL using the editor's Half-Life profile and bundled materials.
For runtime validation, place a `cycler` for each sequence index in a
disposable Half-Life fixture. A `trigger_auto` activates their shared target
name after a delay. Capture frames before and after activation and compare
each model's region with a fixed camera. The Steam installation accepts a
mod directory name relative to its own game directory.

An offline-only guard check on Linux confirmed that opening Character Creator
from Mapping Jams is blocked.

Core tests also cover shortest-path quaternion interpolation and upper-body
layer masking. Clothing intersections, restarting with rearranged panes,
scaling, separate CPU and GPU frame timings, and the remaining release
checks in RB-21 still require validation.

The legacy Windows `tests/test-model-editor.py` suite timed out with the
same nine logged failures on both the published alpha.3 module and the
development module, using the same engine and LibreQuake input. Native
Character Creator smoke runs and core tests should be read independently
of that unresolved Model Editor result.


The visual iteration checks 65 source and runtime asset hashes, including
the generated material sheet and the bundled diffuse maps. The character
GLB fixture carries the images in its binary buffer, including alpha
cutouts for hair, eyebrows and eyelashes. Khronos Validator reported zero
errors or warnings for the fixture and the native editor export. The
classic-format decoders check that indexed outputs preserve nontrivial
material variation. OpenArena displayed the new clothing in a compiled test
room. Portrait screenshots were inspected in the native Character Creator.

The face shape test verifies that feminine and masculine endpoints and
the different templates change head geometry, leave the torso in its
previous position, move attached hair, and preserve the settings in
saved files. Face shape assets use the canonical rig digest and checksums.
