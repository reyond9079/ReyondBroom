# Alpha 116: ivy survives compilation and mod preview

Two separate failures made an ivy model visible in MAP editing but invisible
after compilation. The BSP viewer rendered inline brush models only; it now
also renders external static models with their origin, rotation, scale, skin
and frame. Quake III `misc_model` objects baked by q3map2 are not drawn twice.

Standard LibreQuake has no `func_model` spawn function. New Quake 1 ivy uses
`misc_model`, which the bundled FTE engine can spawn as a static model without
changing the mod's `progs.dat`. Existing generated ivy saved as `func_model`
is converted in the disposable compile or raw-preview snapshot only. The
source MAP and unrelated custom-mod entities are left unchanged.

Generated ivy models and their MD2/MD3 material dependencies are staged with
the build and in the mod preview's private game root. Quake MDL skins stay
embedded. Missing generated assets now stop the build with a named error,
rather than silently producing invisible ivy.

## Existing maps

Open the map and **Compile** again, then use **Show BSP** or **Play mod**.
The last BSP made by an older version still contains its old entity list;
updating the editor alone does not rewrite that BSP. Keep generated files
under `models/reyond-plugins` and `textures/reyond-plugins` with the project.

`misc_model` is supported here by FTE. This does not add arbitrary model
entities to other Quake engines or vanilla Quake II game code.

## Verification and checks

The isolated regression scene uses real LibreQuake PAKs and a native Quake MDL
created by Ivy Grower. Tests cover compile-copy migration, preserved source
MAP, model staging, external-model BSP rendering and starting/stopping mod
preview on Windows and both Linux backends. A separate FTE launch reproduces
the old unknown-spawn failure and verifies the corrected model is precached
and visible. The original LibreQuake PAKs are not modified.

For your project, check:

1. Recompile a map with ivy made in alpha 115. It should remain visible in
   Show BSP, with its leaves, branches and placement preserved.
2. Start Play mod with LibreQuake. The same ivy should appear there.
3. Create new ivy, save/reopen the map and compile it again.
4. Return to MAP editing. Brushes, ivy selection and Undo should still work.

Linux tests ran under WSL/Xvfb with software OpenGL; they are not an Arch
Wayland or GPU performance certification.
