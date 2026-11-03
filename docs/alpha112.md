# Alpha 112: FTE shaders and clearer onboarding

The previously separate FTE shader/studio preview is now included in the main
editor. FTE Custom game defaults to a cloudless analytic sky and an original
rough dielectric studio material; existing explicit custom GLSL choices remain.
Entity and group labels use readable proportional fonts and dark rounded
backgrounds, with a separate selected state and literal UTF-8 names. The GPU
material path retains batching. Other game profiles keep their material defaults.

**Assets > Shaders > Reyond's GLSL Shader Pack** downloads the optional MIT-only
water, sky, weather and effect pack. In Material collections, enable
**textures/reyondpack** and search for **reyondpack**. The pack is available
only for FTE Custom game. See [shader usage and limits](shader-pack.md).

The first-time game selector now includes Quake II and Half-Life symbols.
The final step has a prominent blurple **Join Discord** button, a community
message and translations. The button opens the invite in a browser. Joining
is optional; **Start editing** remains independently available.

## Checks

- Choose Quake II and Half-Life in first-time setup; confirm each icon and
  the corresponding resource fields.
- On the final step, open Join Discord and confirm the correct community.
  Start editing should also work without joining. Check the Russian and
  Chinese layouts if used.
- Open a map and check entity/group labels, including Russian names, selection
  colors and hiding behind geometry.
- Select FTE Custom game and check the default sky/material; switch to a
  different profile and confirm its original rendering.
- Download the optional pack, apply an ocean/sky material and reopen the map.

Runtime archives contain editor binaries and resources. Complete corresponding
sources, dependency archives, patches and build scripts are separate assets
in the same release.
