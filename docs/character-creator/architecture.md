# Character Creator architecture

RB-21 uses an offline native document and a shared rig. Character sources contain
parameters and asset identities. Runtime files contain the geometry and animation
data needed by the selected engine. Mapping Jams must reject Character Creator
entry points, document opens, export and scene placement at the action boundary.

## Geometry and assets

The base mesh, reference joints, skin weights, outfits, shoes and hairstyles
come from MakeHuman's CC0 bundled graphics data. An offline developer-side
converter takes a pinned, hashed input set and emits a compact runtime asset.
MakeHuman application code and runtime dependencies are outside that pipeline.
The converter removes helper geometry, resolves joints, folds removed joints into
their retained ancestors, and normalizes the four strongest weights per vertex.
The rig and animation bank have separate versions. A document identifies both
contracts and retains its previous appearance when new assets are introduced.

Model space is right-handed, Y-up, and floor-relative. The shared Blockbench
math provides transforms and the basis change to Quake Z-up. The canonical
joint order and rest transforms are fixed. Uniform scale applies after skinning
to both geometry and root motion. Document morphs use immutable reference
positions. Garments use barycentric references, interpolated weights and a
conservative body-face mask before skinning. The asset converter applies the
source offsets to the canonical body and ships the mesh with per-vertex anchors.

## Host and rendering

The character core owns mesh, joint transforms, weights and clip sampling.
The editor host owns ImGui state, viewport input, GPU resource lifetime and
map/document generation checks. Reuse the FTE dynamic mesh and image interfaces
used by Model Editor. Generation and garment fitting should operate on immutable
worker inputs. Pose changes reuse the geometry and update skinned positions and
normals. Renderer restart discards transient graphics resources. QuakeC is
responsible for existing map insertion and undo actions.

Plant Editor demonstrates atomic publication of immutable model bundles and map
placement. Character placement needs a separate binding so later saves can
update the original live entity without recreating a deleted one. Offline profile
selection decides model format and game entity class. Map snapshots use the
existing generated-model dependency stager.

## Export and validation

GLB and IQM store weights, rest transforms and sampled joint channels. The
Character Creator writer is independent of the Model Editor's static IQM
writer. Quake MDL, MD2 and MD3 reuse the existing binary writers after
sampling animation and reducing geometry to the target's limits. GoldSrc
Studio MDL stores the canonical bones and sequences with one weight per vertex.
Runtime support for animation and spawning depends on the game mod.

Baseline checks are the existing Model Editor and Plant Editor suites, followed
by separate character checks for deterministic asset conversion, stable rig IDs,
deformation, document safety, wardrobe fitting, format decoding, real viewport
rendering and compatible standalone runtimes. Software rendering can verify
functionality. The 1080p performance gate requires a real GPU measurement.

Main risks are visual garment intersections at morph extremes, preserving
small facial geometry in the Quake 1 budget, legacy animation limits, and
consistent coordinate conversion across exporters. Completion and publication
remain contingent on the complete RB-21 automated acceptance matrix.
