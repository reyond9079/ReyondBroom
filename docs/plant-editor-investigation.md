# Plant Editor integration audit (REYON-5)

Baseline: `5339247671f8291914009836171b8bbcf626f7a1`, including the merged
TrenchBroom compilation profiles. No GUI or renderer framework is added.

* The frontend is ImGui in `native/ftebroom.cpp`; Modern Light/Dark are owned by
  the existing appearance subsystem. The new window uses its colors and scale.
* Model Editor uses `ReyondBroomDynamicMeshV1` / `ReyondBroomDynamicImageV1` and
  `qc/fb_bb_render.qc`. These already provide a depth-tested render target,
  orthographic/perspective cameras, wireframe, two-sided alpha cutouts and grid.
  The preview material is unlit with vertex color: directional shading/AO must be
  baked into preview colors; it is not a PBR renderer or a shadow-map pipeline.
* `native/ftebroom_model_export.h` supplies validated MD3 and mesh structures.
  Existing MDL/MD2 codecs and Model Editor remain available for conversion.
  Its import/UV modules provide the round-trip oracle. GLB import currently uses
  an optional Blender bridge; the plant generator/exporter must not require it.
* Generated models enter a map through `ftebroom_model_drop`,
  `qc/fb_context.qc`, and the existing entity transaction. This preserves ordinary
  move/rotate/scale, selection, Undo/Redo, MAP save/reopen and model metadata.
* `prepareStaticModels` supplies FTE's static-model fallback for classic Quake
  profiles. This is an FTE extension, not a claim that vanilla Quake progs support
  arbitrary static models. Quake III supports compiled `misc_model`; GoldSrc and
  Quake II have different codecs/entity requirements and must not receive a
  silently incompatible MD3 reference.
* `stageGeneratedModels` already copies generated MD3 textures into build/preview
  snapshots. Plant alpha-cutout shader dependencies also need staging.
* The plugin SDK's small form API does not expose a dedicated three-pane editor
  or a second preview. Integrating the authoring tool in the native host reuses
  the existing mesh interfaces without changing the external plugin ABI.
* Plant history is a separate bounded document history. Applying a plant to a map
  uses the map's transaction history; authoring must never intercept map Undo
  while its window is unfocused. Generation owns no engine pointers and runs on
  a cancellable worker; uploads and other engine calls stay on the main thread.

Algorithms, presets and procedural maps are original implementation. Commercial
references inform the library/hierarchy/viewport/properties workflow only; no
commercial source, artwork, textures or sample models are copied.
