# Alpha 117: Ivy Grower, FGD models, modular level generation and grid color

## Ivy Grower

Open **Plugins > Ivy Grower**. New ivy uses less saturated leaves and gray-brown
bark, thinner stems (default radius 1.2), continuous stem rings, welded stem
vertices and longitudinal bark UVs. Leaf bases attach to the stems. Existing
ivy assets/materials are not rewritten; the new textures use `ivy_*_v3.tga`.
Quake MDL output uses smaller spatial parts to retain thin-stem precision.

There are two starting workflows:

- **Selected face:** Shift-select the target face, leave **Automatic root /
  cover selected faces** enabled, then **Grow**. The whole face receives a
  connected set of tendrils. Vertical faces are covered from top to bottom.
  **Face coverage spacing** controls density; large faces adapt to the bounded
  node budget. Separate selected faces remain separate plants.
- **Start point:** choose **Pick start point**, then click a brush/patch in the
  viewport. Selection is retained. A cross marks the root; its XYZ fields can
  also be edited. Select the growth target surfaces and press **Grow**.
  **Hanging / trailing** starts downward and uses gravity; **Climbing** seeks
  upward growth. **Escape** cancels picking. **Stop** keeps the growth preview.

**Create ivy** applies the current preview with Undo. Picking/changing the root
requires growing a fresh preview. Point growth follows the selected surfaces;
`Maximum floating length` limits unattached hanging shoots. This is procedural
growth, not a rigid-body or wind simulation.

## Arcane Dimensions FGD

The supplied `ad_1_9beta5.1.fgd` parses all 346 classes. The reader supports
per-choice help text, case-insensitive flag types, adjacent comments and a
bounded data-only subset of TrenchBroom conditional model expressions:
bitwise/logical comparisons, model-path variants, property-based skins, frames
and scales, fallback models and inherited definitions. The viewport reevaluates
the rules when entity properties change. Definitions do not execute game code.

Choose the FGD in the Quake 1 profile's **Entity definitions** field and load
resources again. The actual models still come from the configured mod's loose
files/PAKs. A FGD describes model paths; it does not contain those assets. Use
the matching Arcane Dimensions resources for newer monsters.

Point `light` entities now use an original, depth-tested bulb icon in the 3D
viewport, while retaining selection bounds and picking.

## Procedural Level Generator

The former room-tree generator is replaced with a modular constraint solver.
See the [modular generation guide](modular-levelgen117.md) for block capture,
socket rules, large modules, constraints and partial rebuilding.

## Grid color

Open **Edit > Preferences > Colors > Grid**. The RGB picker updates background
and surface grids, including orthographic views and the retained GPU grid path.
Close Preferences to save the choice. The white default preserves the previous
grid appearance. Brush-edge color is separate and is not changed by this picker.

## Check after updating

1. Create fresh ivy on a vertical face; inspect its top, middle, bottom, stem
   joints, leaf attachments and materials. Try horizontal/sloped faces too.
2. Pick a root, cancel with Escape, pick again and try both growth habits.
3. Save/reopen and compile the ivy map; compare Show BSP and Play mod.
4. Load the AD FGD with matching mod assets; place monsters and change their
   spawnflags/skin properties. Place a `light` and check the bulb behind a wall.
5. Generate a modular level, try different seeds/weights and a volume constraint,
   capture a brush module, rebuild a subregion and test Apply/Undo/Redo.
6. Change grid color, switch between 3D/2D views, close Preferences and restart.

Linux renderer checks use WSL/Xvfb software OpenGL. Native Arch/Wayland hardware
coverage remains a separate user check.
