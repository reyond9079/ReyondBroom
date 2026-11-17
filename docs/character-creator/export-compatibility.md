# Character Creator export compatibility

| Profile | Format | Animation | Texture output |
| --- | --- | --- | --- |
| Portable | GLB | Weighted skeleton, named clips | Embedded JPEG skin, eyes and clothing, and PNG cutouts |
| FTE Custom | IQM | Weighted skeleton, named framegroups | JPEG diffuse textures, PNG cutouts, materials and GLSL shaders |
| Quake 1 | MDL | Vertex animation, at most 1,024 vertices and 2,048 triangles | Embedded game-palette diffuse maps |
| Quake II | MD2 | Vertex animation, at most 2,048 vertices and 4,096 triangles | Companion PCX with indexed diffuse maps |
| Quake III | MD3 | Vertex animation, at most 999 vertices and 1,999 triangles per surface | Companion TGA files |
| Half-Life | Studio MDL | Named sequences and one bone per vertex | Embedded material-specific indexed textures |

The original game code controls playback, entities and interactions. A placed
model file defines geometry, textures and animation data for the game.
Quake MDL, MD2 and MD3 use a reduced mesh to satisfy each game's limits.
The baked formats omit the eyebrow and eyelash cards and their silhouette
and deformation differ from the editor preview.
For baked formats, FTE reads `<model>.framegroups` next to the model file.
The sidecar records the start pose, count, frame rate and loop mode for each
named clip. Other engines use their own animation conventions.

Use **Export IQM** or select **FTE Custom game** in the **Export** tab to
write the FTE model and its material bundle. Keep their relative paths when
copying them into the mod. Quake 1 and Quake II need the active game palettes.
Review validation results before distributing game content.
