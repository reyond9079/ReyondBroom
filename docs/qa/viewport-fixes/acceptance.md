# Viewport/keyboard/profile-order regression

Scope: the five non-launch items from the user's November8 report. External
VKQuake deployment/cleanup behavior is a separate investigation; this change
does not rewrite the supplied CompilationProfiles.cfg or its `.bspw` entry.

## Reproduction and implementation

The public alpha129 native baseline was copied to a disposable fixture. Its
2D white grid produced zero white pixels in the measured view interiors.
With magenta brush edges, grid-on retained163 fully visible edge pixels versus
1699 with grid-off. The old 2D code multiplied Grid RGB by0.18/0.20, making white
nearly equal to the default48/50/56 background. Since128, the grid was also
drawn over cached geometry, masking edges at aligned coordinates.

The fix removes hidden RGB multipliers from2D,3D surface and world grids, and
caches grid/axes before geometry. Orthographic background projection is
independent of the FBO matrix and its scissor origin is reflected explicitly.
The existing nearest-filtered geometry cache remains enabled. Local render
comparison after the fix measured68400 white grid pixels and2048 magenta edge
pixels with the grid both on and off. These are fixture measurements, not a
universal performance/quality threshold or an OS/GPU hardware guarantee.

Windows acceptance also exposed a second source of coverage changes: its default
8× MSAA window backbuffer smooths thin lines, while the cache texture has one
sample. Orthographic views now always rasterize into the same opaque canvas.
Idle multi-view panes reuse it; dragging, tools, disabled caching and maximized
2D panes redraw it each frame. This keeps line coverage consistent without
disabling 3D MSAA or removing cache reuse. The unchanged cached/uncached strength
assertion detects this difference; screenshots and metrics retain the evidence.

The existing editor sky shader now emits a configurable flat gray when sky
background is disabled. Preferences persist this independent RGB value.
Keyboard actions reuse the real entity/group/history/texture operations;
world-aligned reset keeps its stableID/default binding. Capture instructions
reserve a fixed row, and search handles case and separate terms. Profile order
uses original indices rather than names, so duplicates retain identity.

## Automated acceptance

`scripts/test-viewport-appearance.py` drives actual OS mouse/keyboard input in a
disposable editor, with read-only UI/focus acknowledgments. It checks actual
screenshots/pixels, edits persisted settings, exports real MAPs and restarts:

- white in2D/3D; black/red RGB; white on a different background;
- geometry edge strength with grid on/off and cached/uncached;
- stability across the periodic cache refresh;
- actual dark-gray pixels with sky off;
- Compile up/down/A–Z and Launch/manager ordering/selection;
- case-insensitive multiword search and three assigned/saved shortcuts;
- real func_detail/group MAP content and rotated→world-aligned UV changes;
- restart persistence of profile order and Grid/3D background RGB.

It does not launch an external game engine. `--capture-only` records the old
baseline without pretending those new expectations pass. Local comparison
modules retain their original alpha129 header; provenance records the actual
moduleSHA. Final package evidence identifies the release's own build/source.

`tests/profile-order.cpp` checks duplicate-name stable order, preserved payload
and selection identity, empty lists and move boundaries. CMake runs it with
assertions enabled. Existing profile/engine viewport regression retains real
compiler/process checks; its old dark-grid pixel oracle is updated for direct
RGB blending (default170gray at0.65 over48/50/56 ≈127/128/130).

CI runs the new OS suite on Windows/nativeLinux/SDL2. Release packaging repeats
it in ModernDark/Light at1920×1080 and1024×768 when `viewport_checks` is enabled,
in addition to the existing authoring/save tests. Release130 enables this gate.
Full plant-catalog recapture remains available through `capture_catalog`; it
defaults on and is disabled for this release because plant generation is
unchanged. The original plant visual acceptance belongs to release129.

Publication requires successful exact-head platform checks/packages, identical
built/merged source trees and all twelve runtime/source/checksum assets. The
draft remains unpublished until those gates and actual artifact review pass.
