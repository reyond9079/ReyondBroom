# Sprite entity icons, colors and water fixes

## FGD sprites

The entity loader now reads `iconsprite("progs/example.spr")`, fixed-path
`sprite(...)` helpers and property-based `sprite()`. Icon helpers are inherited
from base classes. Explicit `model(...)` / `studio(...)` definitions take
precedence over an icon helper on the same class. Existing conditional model
rules can also reference sprites.

The Entity Browser renders cached, clipped, aspect-correct sprite thumbnails.
The thumbnail reader supports Quake paletted SPR, grouped SPR frames, RGBA SPR,
GoldSrc embedded palettes and its transparency modes, and Quake II SP2 image
references. A thumbnail shows the requested frame (the first subframe of a
group); viewport animation and sprite orientation use FTE's renderer.

The actual sprite files must exist in the configured mod or its PAKs. An FGD
contains paths, not the images themselves. In particular, newer AD editor icons
require the matching AD resources; an older mod can lack those files. Missing
resources retain the entity's normal fallback rather than preventing placement.

The reader checks dimensions, frame counts, offsets and decoded-pixel budgets.
Malformed or missing browser resources fall back to the existing bounds tile.
Editor icons are display metadata: they are not written into an entity's MAP
`model` property or spawned into the game.

## Appearance

Open **Edit > Preferences > Colors**:

- **Grid** defaults to RGB **170, 170, 170**. Previously saved custom colors
  remain saved; the new default does not overwrite an existing preference.
- **Entity bounds** controls point-entity boxes and brush-entity bounds.
  Changing the picker automatically turns off per-class bounds colors.
- **Use entity class colors for bounds** restores the original class colors.
  Selected/locked highlights retain their own colors; groups remain separate.

Both color choices and the class-color option survive restart. Point-bound
render caches and 2D cached views refresh when the setting changes.

The generic light marker now uses a supersampled Lucide Lightbulb outline with
a dark contrast keyline. It keeps square proportions, smooth alpha edges and
normal depth testing. Explicit sprite/model definitions for `light` take
precedence over this fallback marker. SVG source and ISC notices are included.

## Island water

FTE reflection/refraction portal targets use the surface's screen projection.
The previous water shader incorrectly flipped the reflection's Y coordinate a
second time. Reflection and refraction now use the correct coordinates and
opposite distortion offsets.

Wave filtering uses distance, surface incidence, projection focal length and
NDC pixel size. Merely pitching/yawing the camera no longer changes the wave
normals through derivatives of world-space XY. Both editable MAP water and
compiled water use this filter. Editor background sky detection also consults
the existing material counts when sky faces are rendered directly by FTE.

The editor supplies these water/wave corrections when activating, staging or
exporting an already-downloaded 1.0.0 shader pack. The download itself and user
mod files are not rewritten. Reopen/reactivate the pack after updating. For an
older exported standalone mod, export the pack again into your chosen output.

## Check after updating

1. Load your FGD with the corresponding mod resources. Inspect sprite icons in
   the Entity Browser and viewport, their transparent edges and selection.
2. Place a plain `light`; compare its outline against bright sky and dark walls.
3. Change Entity bounds color, deselect objects, switch 2D/3D and restart.
   Toggle class colors back on and check selected/locked highlights.
4. Check Grid's new default in a fresh configuration; existing color choices
   should remain unchanged.
5. Open the sample island and pitch the camera above/below the horizon. Repeat
   in Show BSP and Play mod after updating/staging the shader pack.

Verification includes 67 real AD PAK sprites, parser/format regression checks,
Windows and Linux native/SDL2 browser/viewport/color-persistence checks, and
Windows MAP/BSP plus an isolated FTE/LibreQuake play render. Linux GUI testing
uses WSL/Xvfb; it is not native Arch/Wayland hardware certification.

Sources: [FTE water projection](https://github.com/fte-team/fteqw/blob/master/engine/shaders/glsl/altwater.glsl),
[Lucide Lightbulb](https://lucide.dev/icons/lightbulb).
