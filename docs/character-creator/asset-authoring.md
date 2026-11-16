# Character asset authoring

The runtime carries RBH1 and RBW1 data with the corresponding provenance
and CC0 notice. The input revision of the canonical rig is in
`runtime/characters/provenance.json`. Download the four named sources to a
developer scratch directory and retain their original basenames. Build the
body with `python scripts/build-character-base.py <scratch> runtime/characters`.

The wardrobe pack URL, file names and input hashes are recorded in
`runtime/characters/wardrobe/provenance.json`. Extract the listed OBJ,
MHCLO and hair diffuse PNG files to the wardrobe scratch directory. Build `sha256.json` in
that directory from the manifest's combined `sources` entries. Rebuild with
`python scripts/build-character-wardrobe.py <base-inputs> <wardrobe-inputs>
runtime/characters/humanoid-v1.rbh runtime/characters/wardrobe` with Pillow installed.
For hair, the converter drops triangles whose atlas region is transparent
even with a filtering margin.

RBH1 and RBW1 are little-endian. Both start with four magic bytes, version,
three counts and the SHA-256 of the canonical rig table. The mesh keeps stable
source vertex identities across UV seams. RBW1 uses one reference triangle
and three coefficients per clothing vertex. The converter also retains
the original body coverage masks and interpolates bone weights from the
same references. Morphs transform the anchor locations and thus translate
the clothing offsets with the body. Exact single-vertex binds use one
nonzero coefficient. Runtime validation checks the same rig digest and
bounds before using an asset.

Each canonical joint has a stable name, parent and Y-up rest position. Keep
these unchanged when adding clothes, hair or an animation. Runtime
conventions, including GoldSrc's rigid skin weights, are described in the
architecture and testing documents.
## Character surface textures

The bundled material atlases use original image samples and texture maps
generated for ReyondBroom. The underlying mesh topology, UVs and fit
anchors retain their CC0 provenance. The registry at
`runtime/characters/materials/provenance.json` records the input and output
SHA-256 hashes. The source swatches include
`assets/characters/reyond-surface-source-v5.png` and
`assets/characters/iris-albedo-source-v3.png`.

Run `python scripts/build-character-original-materials.py` from the source
tree with Pillow and NumPy to regenerate the atlases. The baker uses the
bundled wardrobe UVs to create fabric color and hair opacity. The separate
PBR bake generates skin and fabric detail textures for the FTE profile.

## Adult face shapes

`face-shapes-provenance.json` lists the six CC0 input targets and their
hashes. Download the targets from the pinned revision, then run
`python scripts/build-character-face-shapes.py <scratch>
runtime/characters/humanoid-v1.rbh runtime/characters`. The conversion
anchors displacement at the original head joint and limits it to the
head and upper neck. The wardrobe fitter uses the same displacement for
the corresponding hair, brow and lash anchors.

The body shape manifest records the CC0 adult body targets. Run
`python scripts/build-character-body-shapes.py` with the documented source
targets and output paths to rebuild that asset. A custom `.rbw` uses the
same canonical rig digest, valid bind anchors and normalized skin weights.
