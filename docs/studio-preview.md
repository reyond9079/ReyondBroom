# Viewport labels and FTE studio preview

Entity labels use Source Sans Pro at 13 pixels, white foreground, four-pixel
padding and a 60% black rounded background. Group labels use purple text.
Selected labels use white text over a 60% red background. Labels are centered
above object bounds. The appearance follows TrenchBroom's viewport overlays.
The existing 768-unit fade distance and geometry occlusion are preserved.
Selected labels retain their distance exception, not an occlusion exception.
The accelerated point-label path batches backgrounds and text separately and
caches the measured widths together with its existing visibility snapshot.

FTE Custom game now defaults to **Neutral studio (Unreal-like)** and a clear
analytic sky background. The original studio GLSL provides hemispherical sky
lighting, a warm directional light and a rough dielectric highlight. It is a
preview approximation, not Unreal Engine's renderer, and does not add shadow
maps or global illumination. Ordinary diffuse brush materials retain their
textures and UVs. Masked/translucent surfaces retain their dedicated preview
paths; the optional shader pack's procedural materials retain their programs.
The retained GPU material path uses the same studio lighting as ordinary draws.

Change these choices in **Edit > Preferences > Game profile** using
**Editor defaultwall** and **Clear sky background (editor)**, then load the
resources. The former default `Current FTE default` setting migrates once to
the new studio style; explicit source/mod/custom-file choices are preserved.
Choosing Current FTE default again is saved and respected on the next launch.
Other game profiles retain their original background and material rendering.

The editor sky is a background preview, not a new MAP brush. It works without
downloading the optional shader pack. The pack also defaults to Clear weather
when there is no saved weather choice. Existing saved weather choices remain.
No MAP textures, game scripts or compiled output are rewritten by these options.

The original clear sky and studio lighting are MIT licensed; see
`runtime/glsl/REYOND-STUDIO-LICENSE.txt`. The retained material wrappers remain
GPL-2.0-or-later, matching their FTE source.

## Verification

Windows (OpenGL/NVIDIA) and Linux SDL2 (Mesa software renderer under WSL)
rendered the studio scene successfully. The previous defaultwall=0 settings
file migrated to studio mode 5. A separate Quake III run kept the studio and
sky switches at zero. The 5,101-brush scene used retained GPU materials; its
accelerated and reference viewport captures had identical pixels. Russian
label parsing is isolated to the editor font rather than changing game text
encoding. These checks do not replace testing on native Linux hardware.
