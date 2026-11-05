# Modern Light appearance

Modern Light retains theme identifier `3` and the existing appearance settings
format. It changes interface chrome only. It does not reset workspace layouts,
select a different default theme, modify map data, or alter scene rendering.

The palette and surface helpers live in `native/ftebroom_light_theme.h`. The
UI Lab design uses a neutral gray workspace (#AAA9A7), separate rounded panels
(#BABAB8), recessed groups (#B2B2B0), and charcoal Tools chrome (#242426).
At 100% scale the workspace has 18 px side margins and 16 px dock dividers.
The background shows through the dividers, which remain draggable. Lightweight
draw-list geometry adds subtle shading, rims, and shadows. The 3D viewport is
framed outside its render rectangle and uses the same rectangle for picking
and camera input. Existing fonts and multilingual fallback glyphs are reused.

A fresh Modern Light workspace uses a slim Tools rail on the left where space
allows. Choose **Workspace > Reset panel layout** to apply this arrangement to
an existing workspace. The rail shows compact tool and build icons. The separate
Tool options button opens the active tool's controls in a wider popup. A saved
top toolbar still works, including its inline options.

Hardcoded interface colors in browsers, sound controls, viewport headers,
notifications, onboarding and model timeline labels now use semantic roles for
Modern Light. Asset pixels, geometry overlays, axes and diagnostic scene colors
retain their existing rendering. Other themes receive their original colors.

All dimensions are applied before the existing UI scale conversion. Docked and
floating windows keep the existing docking behavior; the dark tool container
adapts to both horizontal and vertical Tools panes without changing their size.

The entity browser now reserves a fixed footer for Entity definitions and a
compact magnifier/thumbnail-size control. Advanced filters are initially hidden
behind the sliders icon beside search. The creation hint and optional defaults
switch are removed; definition defaults are enabled on UI load and entity
creation. Browser previews occupy roughly 60% of a thumbnail with a complete
clip region and centered captions. File manager/model preview sizing is unchanged.

## Validation

`tests/test-modern-light.cpp` uses the pinned ImGui dependency. It checks:

- All 25 legacy theme/scale signatures against the pre-change fixture.
- Modern Light at all five UI scales.
- Palette ranges, text contrast and scoped color-stack restoration.
- Exact legacy color passthrough outside Modern Light.

Compile the test with the same ImGui sources and headers used by the editor,
then run it from the repository root. The fixture is intentionally fixed: do not
regenerate it from a changed implementation to hide an unrelated theme change.

`tests/test-modern-light.py` runs a disposable editor installation using an
existing runtime archive, the newly built module, and a copied test mod:

```text
python tests/test-modern-light.py --archive <runtime.zip-or-tar.gz> \
  --plugin <built-module> --output <new-empty-directory> --mod <test-mod>
```

It captures the main workspace, both Tools orientations, 150% Russian UI,
Preferences, onboarding, AutoLevel, a control gallery, file/entity browsers and
compile settings. It checks native assertions, multilingual glyph availability,
balanced style stacks and restoration of saved theme ID 3 in a second process.
It also clicks the advanced-filter toggle and footer definitions button, checks
the footer stays within its panel, validates preview margins, and drags a dock
divider to verify that resizing preserves the gap. A screenshot sample checks
that the dock host actually leaves the workspace visible between panels. The
desktop test also clicks the Tool options button in the narrow rail.
The test-only commands do not affect normal application startup.
