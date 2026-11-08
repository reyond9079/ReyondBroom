# Viewport colors, keyboard commands and profile order

Open **View → Preferences → Colors** to set Grid, Brush edges, 2D viewport
background and **3D background without sky**. Grid RGB is used directly in both
orthographic and perspective views. Grid opacity, distance fading and
antialiasing still control coverage. White is white and black is black; neither
is secretly multiplied by the background's brightness.

The orthographic cache contains the grid **behind** brush edges. Its FBO clip
rectangle accounts for the renderer's inverted origin, so the complete grid
remains visible in all three orthographic views. Turning the grid on does not
paint over geometry outlines. Cached and uncached views use the same projection.

Under Appearance, turn off **Show sky background** for a flat dark-gray 3D
background (default `#262626`). Colors can customize that background separately
from the sky tint and the 2D background. This controls the editor backdrop;
explicit sky surfaces and compiled game rendering keep their own materials.

In **Preferences → Keyboard**, search is case-insensitive and matches separate
words in action names and paths. Select a shortcut cell to assign it; Escape
cancels capture. F1–F12 remain reserved by the editor.

- **Create func_detail** converts selected brushes into a brush entity. It is
  available with a map selection and no modal tool, and starts unassigned.
- **Create func_group** creates an editable group from the selection, serialized
  with the editor's existing `func_group` metadata. It starts unassigned. The
  existing Ctrl+G named-group workflow remains available.
- **Reset texture alignment to world aligned** retains its existing action ID
  and default **Alt+Shift+R**. It uses the existing undoable texture projection
  command; existing custom shortcut assignments are preserved.

Compile's Profiles toolbar, Launch Engine and the Game Engines manager offer
**Up / Down** and **A–Z** controls. They keep the selected profile attached to
its tasks or engine parameters, including duplicate names. The stored order is
per game and survives restart. The TrenchBroom profile import button remains.
