# Character Creator

Open **File > Character Creator** in an offline editor session. The command
is also available under **Plugins**.
Use **Character preset** for a starting character, then edit Body, Face,
Appearance and Wardrobe. Choose a clip in the animation selector, press
**Play**, or drag the timeline to a particular time. **Preview blend** mixes
in a second clip. Enable **Upper body** to retain the main clip's leg motion
and root position. The blend controls affect the preview. Exports contain
the named clips individually.

Use the section reset to clear its proportions or choose **Reset all
proportions** in Body to reset both body and face. Undo restores the previous
proportions in one step.

Drag the preview with the left button to orbit, the right or middle button
to pan, or use the wheel to zoom. **Front**, **Side**, and **Frame** reset
parts of the camera. The **Rig** tab shows the skeleton and lets you select
and highlight a joint. Use **Shading > Skin weights** to inspect the selected
joint's influence. **Shading > Normals** displays the orientation of the
animated surface. **Portrait** frames the face, and **Upper body**
shows the shoulders and clothing. **Appearance > Complexion** selects a UV
skin texture. The skin and hair color controls tint the selected textures. Enable **Dockable panes** to arrange the viewport,
properties and timeline independently. The wardrobe lets you choose shoes
and headwear separately from the outfit. A selected hat covers the hairstyle.

Save editable characters as `.rbchar`. The file stores settings, and the
model is generated from the bundled body and wardrobe. The Mod file manager
can open loose `.rbchar` files.

When opening another character, creating a new one, or leaving with unsaved
changes, choose **Save**, **Discard**, or **Cancel**. If the character has no
filename, **Save** opens the file chooser before continuing. The editor also
prompts before normal map or game-profile changes when the character is dirty.

In the timeline, **Playback** can use the clip's default behavior, loop it
continuously, or play it once and hold the last pose. This setting applies to
preview playback. Use the step buttons to move one thirtieth of a second.

The docked inspector uses a section menu when the pane is too narrow for
full tab labels. The docked timeline wraps its controls and scrolls vertically.

To choose a position on a map, right-click a surface in the 3D viewport and
choose **Create > Model > Character**. Saving a character with a recorded
placement writes the target model bundle and updates the linked map entity.
Use the regular map Undo/Redo for changes to the linked entity.

**Export GLB** produces a portable skeletal model. **Export for game
profile** creates the model format used by the current project. Use the
**Export** tab to save a separate target in the character document. Map
placement still uses the active map profile. Use a new
filename to keep existing exports intact, and keep the model and its
companion files together. Runtime animation for a placed model depends on
the mod and entity class. See the separate testing document for the current
validation coverage.

FTE-specific projects use the character skin material automatically in the
preview and on newly placed characters. When using **Export for game profile**,
export into a folder that can serve as the game directory. Keep the generated
`glsl`, `scripts`, and `textures` directories relative to that directory. You
can move the IQM into its `models` directory.

**Face** frames the head and pauses animation. Hover over a facial feature
to see its editing area. Drag to change its width and vertical shape.
Hold Shift while dragging to edit depth. Right-click during a drag to cancel it.
Each completed drag is a single Undo step. The selected feature also has
shape choices and precise sliders. **All facial details** exposes the entire
set of controls. **Body** frames the whole figure and shows body proportions.

In **Face**, use **Face template** and **Facial structure** to choose a starting
head shape. **Eyebrows** sets their silhouette. In **Wardrobe**, **Hair texture**
filters hairstyles by strand type. Tune **Hair volume**, **Hair length** and
**Hairline** independently. The numbered **Character preset** list fills in a
complete appearance that can then be edited.

**Complexion** includes **Fair** for a cooler light complexion and
**Freckled** for pigmentation across the bridge of the nose and cheeks.
The **Skin** color control adjusts the selected texture. Presets can
also vary in stature, build, facial hair, and hairstyles.

**Asymmetry** applies a subtle offset to the eyelids, brows and mouth corners.
The eye material includes a soft upper-lid shadow and small reflections.

In the Wardrobe tab, use the garment import action to add a fitted `.rbw`
with compatible rig data. Place a JSON file named `garment.rbw.license.json`
beside `garment.rbw`. Include the file's lowercase SHA-256 as `sha256`, its
creator as `author`, a provenance or permission reference as `source`, and
one of these values as `license`: `CC0-1.0`, `CC-BY-4.0`, `CC-BY-SA-4.0`,
`LicenseRef-OwnWork`, or `LicenseRef-Permission`. The editor checks the
declaration and the file hash. It keeps the attribution in the saved
character. Characters with garments imported in earlier versions still open.

Use the timeline's animation import action for a
`.glb` with the compatible humanoid skeleton. The editor validates the
hierarchy and rest pose before retargeting. Saved characters store relative
references to their imported assets in the adjoining `rbchar-assets`
directory. Keep that directory with the `.rbchar` when moving the project.

Use **Preview detail** to select Full detail, Balanced (15k) or Light (9k).
This changes the preview mesh budget. The game-profile export still applies
the target's own format limits. Turn on **Gear sockets** in the Rig tab to see
attachment points. Animated GLB exports also contain five named expression
morph targets. Previously saved Field Technician files remain editable under
the legacy face template.
