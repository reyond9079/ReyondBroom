# REYON-9 visual review

Comparison baseline: `9e5b30a`, original generator 1 / 30 presets. New catalogue:
generator 2 / 121 presets. Real native Plant Editor captures, Modern Dark,
1920×1080, seed 137, default Realistic settings, hero camera yaw 135° / pitch
18°, automatic bounds framing and the same sun direction. This compares the
factory results users actually get, rather than keeping old incorrect species
parameters. Tree triangle budgets remain 32,000. Full parameter JSON accompanies
every image. Source/reference audit: [official references](../../plant-editor-references.md).

The twelve-control sheets were inspected alongside original oak/spruce/fern
images and the actual placed oak. Extended capture records three seeds
137/138/424242 from front, side and top-oblique (85°, the legacy orbit limit),
three LODs, untextured architecture and a closer camera crop for each control.
The packaged native Linux gallery records every preset, retaining full original
PNGs, parameter/camera JSON and SHA256 manifests alongside labelled contact
sheets. Screenshots show the application's actual renderer, not a substitute
renderer or generated illustration.

| Control | Architecture / silhouette vs baseline | Organ proportions | Distribution | Materials / alpha / lighting | Scene / LOD assessment |
| --- | --- | --- | --- | --- | --- |
| oak | Complete spreading crown replaces a few disconnected terminal tufts | Thicker structural trunk, shorter twigs and smaller leaf clusters | Crown-wide sampling retains all sectors when budget is reached | Lobed cutout/veins and multiple leaf tiles; procedural bark remains simpler than scan data | Live Save model visibly renders; LOD reduces fine twigs before foliage, preserving the crown |
| birch | More slender, asymmetric multistem architecture, distinct from oak | Narrow stems and smaller foliage | Leaves occur throughout the planned shoots instead of only a few branch tips | White/dark bark pattern and varied leaf tones; trunk joins are swept intersections, not sculpted junction meshes | LOD2 originally lost foliage to woody geometry; ribbon twigs now reserve leaves and matrix checks enforce their survival |
| willow | Hanging terminal shoots replace the sparse generic branching of the old preset | Long narrow leaves and downward twigs | Pendulous crown hangs across its extent; natural gaps remain | Narrow alpha silhouettes; dark foliage can be hard to read against the preview's dark background | Complete main silhouette retained by spatial LOD sampling; dense closeups still reveal economical leaf meshes |
| pine | Open, broad upper crown differs from spruce's full pyramid and the old regular cone | Needle sprays replace broadleaf-looking clumps | Shoots and needle bunches span each branch zone | Original needle atlas, varied tiles and upward-biased normals; software preview lacks real canopy shadows | Lower LOD uses fewer larger sprays, not a whole-tree crossed billboard |
| spruce | Dense pyramidal architecture with descending secondary shoots | Fine needles on local shoots, not identical oak leaves | Full branch plan avoids traversal-order missing sectors | Needle alpha reads darker than broadleaf foliage; visible detail depends on distance/background | Thin twig tubes are omitted at LOD2 so needles keep their budget; structural branches remain |
| wild-shrub | Dense multistem base with a low, rounded outline | Small foliage on shorter structural stems | Crown volume is filled between stems, retaining irregular gaps | Species leaf/bark variation rather than a recolor of the tree preset | Full Save route is also exercised with this non-default species; LODs retain the multi-stem outline |
| meadow-grass | Tapered, curved clump, distinct from seed-head/sedge/reed presets | Individual blades narrow toward their tips | Unequal blade height and direction; no tree-style branches | Procedural blade shading/alpha; silhouette still deliberately economical for engine use | 2,176 → 1,632 LOD0 triangles, then 568 / 168; the route saves a separate grass func_model |
| bracken | Arched, more finely divided fronds replace broad generic strips | Separate rachis/pinnae, smaller paired leaflets | Leaflets follow each frond with tapered size progression | Actual geometry and cutouts, not a fern photograph pasted on one card | Geometry rises from 7,614 to 23,022 triangles within the same 32k cap; quality has a measurable cost |
| daisy | Recognizable disk/floret head rather than one generic pointed petal layout | Rounded pale petals, yellow centre and narrow stem leaves | Local whorls, multiple flower heights | Original petal/disk tiles and cutout; distant heads remain simplified, not botanical scan assets | Reduced LOD keeps the flower head and stem, reducing individual florets |
| amanita | Cap, stalk, annulus/volva details and clustered growth are more distinct | Thick cap profile and proportioned stems | Different stalk heights/ages within the clump | Cap spots, gill/stem patterns; procedural spots can look regular in close view | Three LOD cap profiles remain closed and recognizable; no disappearing foliage category applies |
| saguaro | Continuous ribbed stem and rising arms replace flower-like terminal artifacts | Thicker rounded arms with taper | Irregular but coherent arm positions | Ribs, areoles/spines and species skin, without imported assets | More geometry is used (11,048 → 31,992), still bounded; low LOD retains the arms and full height |
| date-palm | Pinnate fronds replace broad repeating fan-like strips | Separate rachis and tapered pinnae, scarred trunk | Fronds arranged around the crown, naturally falling outward | Original stem scars and leaf material; this preview does not add HDRI/translucent subsurface lighting | Fewer default triangles (10,260 → 8,112); reduced LOD retains crown/frond directions |

LOD review also found a grass silhouette defect: generic leaf compensation
made LOD2 blades up to 1.85 times taller. Grass and full-height grass-material
blades now compensate width only; their length remains fixed. The independent
matrix rejects a height increase above 12% for every grass/riparian preset and
seed. This also protects the basal blades of cattails.

The full-size catalogue review also found disconnected Opuntia pads. The final
generator attaches each child inside its parent shoulder, flattens and orients
the pads with corrected normals, and uses smooth areole skin rather than
columnar ribs. A separate organ-bounds graph checks pad connectivity across
all seeds and LODs. Its panel exposes pad count instead of unused rib/flower
controls; the original gen1 cactus remains unchanged.

## Scope and limits of the evidence

This is a substantial procedural geometry/library improvement, not a claim of
photorealism or parity with every feature of SpeedTree/PlantFactory. FTE preview
uses two-sided cutout, baked directional vertex lighting and an AO approximation;
normal/ORM maps are exported for supporting renderers, not silently presented
as a new PBR preview. Swept branch junctions can intersect; canopy occlusion is
approximate, small leaves/needles can alias at distance, and low LOD trades
organ detail for complete coverage. No third-party scanned plant assets are
bundled. The deliberately stylized legacy fantasy mushroom is retained among
the 121 entries and is not counted toward a naturalistic species claim.

All 121 presets × 3 seeds × 3 LODs are independently checked for validity,
budgets, deterministic regeneration, material ranges and foliage/culm retention.
This geometric coverage is broader than the compiled runtime sample: actual
compiled/game rendering uses a representative saved oak and prop, not a claim
that every preset was separately played through every third-party mod.
Quake III's sample embeds 31,994 plant triangles and its cutout shader in the
BSP. Quake 1 uses the explicit FTE extension and standard engine fallback.
FTE Custom uses the editor's compiled preview; arbitrary custom game progs
must still implement their desired func_model entity behavior.
