# Botanical generator reference audit (REYON-9)

Reviewed 2026-11-07. These are product/documentation references, not bundled
content or a claim to reproduce proprietary algorithms. No third-party plant
models, photographs, textures or screenshots are shipped with the presets.

| Source actually inspected | Useful observations | Application in ReyondBroom |
| --- | --- | --- |
| [TreeIt official page](https://www.evolved-software.com/treeit/treeit) and its linked editor screenshot | Joint editing, breaking joints, leaf image rendering, adjustable LOD; the screenshot shows a complete conifer crown at a modest polygon count | Plan every major branch before allocating triangles; spend less tessellation on twigs and use needle sprays rather than an incomplete sequential canopy |
| [SpeedTree 9 leaf mesh generator documentation](https://docs9.speedtree.com/modeler/doku.php?id=leaf_mesh_generator) | Parent/sky leaf orientation, phyllotaxy, random seeds, normal controls, collision and leaf survival during LOD | Seeded phyllotaxy and leaf orientation; separate species architecture from leaf shape; canopy-wide stable sampling and modest survivor scaling for LOD |
| [Bentley VUE / PlantFactory information and 2024 feature list](https://www.bentley.com/software/e-on-software-free-downloads/) | Cutout-leaf border smoothing and normal editing, orthographic/camera views, material hierarchy, UV tools, preset browser, export and native LOD conversion | Curved leaf surfaces, smooth normals, explicit alpha silhouettes, species organs and independently generated LOD meshes; catalogue and editable project/export data remain separate |
| RockPaperTrees official site | HTTP 403 from the available environment; GitHub repository search returned no usable official source | No claim to have inspected its generator, UI or output; no inferred private implementation details |

The older SpeedTree leaf-generator URL explicitly redirected readers toward the
leaf-mesh generator; the latter was used for the audit. PlantFactory's public
page describes the 2024 release as an unfinished beta after development stopped.
Its downloadable stock content was not imported. The licensing conditions for
PlantCatalog assets differ from those for scratch-built PlantFactory content;
ReyondBroom uses neither.

## Baseline defects identified in the actual source and editor

The generator-1 skeleton could exhaust its reserved triangle budget during a
recursive walk. A failed tube allocation returned before that branch's children
and leaf buds existed. Later crown sectors therefore disappeared according to
traversal order. Dense trunks and repeated cylindrical twig caps consumed a
large fraction of the budget. Conifer and broadleaf architecture used a small
set of generic rules; nominal species were often parameter variations. Presets
numbered thirty, with no separate riparian category or botanical search tags.

Generator 2 records a complete skeleton/foliage plan before emission, scales
branch tessellation across the entire plan, and samples foliage throughout the
crown. Family-local organ traits distinguish needle sprays, compound leaves,
fern pinnae, grass heads, corollas, mushroom forms, cactus pads/rosettes, palm
fans/pinnae, bamboo internodes and riparian heads. Species descriptors set crown
shape, phyllotaxy, branching, proportions and organs rather than merely a seed
or recolor. A separate version preserves generator-1 reproduction and requires
an explicit upgrade to change an existing source.

The actual renderer remains FTE's dynamic mesh preview. Directional shading,
cutout alpha and exported normal/ORM maps do not constitute a photogrammetric
material library or a full PBR/shadow renderer. This distinction matters when
assessing reference-product screenshots.
