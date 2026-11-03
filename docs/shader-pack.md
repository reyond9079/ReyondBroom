# Reyond's GLSL Shader Pack

Select **FTE Custom game**, then **Assets > Shaders > Reyond's GLSL Shader Pack**. Download once in the panel; its progress bar covers downloading and verification. Windows and Linux use the same separate, approximately 6.6 MB ZIP.

The installer pins the release URL, size, SHA-256 and per-file manifest. It rejects unexpected entries, oversized data, unsafe paths and checksum failures. Downloads run outside the editor thread and failures can be retried. No executable or game VM binary is installed by the pack.

## Materials

Open **Material**, select the **textures/reyondpack** collection and apply materials using normal selection or drag and drop:

- **ocean**: original analytic wave normals, Fresnel reflection, sunlight glints and native FTE planar reflection/refraction. Collision geometry is unchanged.
- **sky**: original full-sphere atmosphere, volumetric clouds, cirrus, Sun/Moon, stars and polar aurora. Apply to a sky enclosure.
- **puddle**: thin procedural rain-ripple/reflection material for flat surfaces.
- **fog**: procedural translucent surface with native fog-volume properties for compiled Q3 BSP. The environment panel also offers depth-integrated atmospheric fog. This is not the old reference's shadowed light-shaft implementation.
- **fire / explosion**: original animated effects for vertical effect cards. They do not spawn gameplay entities or cause damage.
- **ambient_gi**: hemispherical ambient irradiance on the chosen material, not full-scene multi-bounce GI.
- **snow / ice**: winter materials following accumulation state. Choose Winter to build coverage. Ice is visual and has no collision.

Internal precipitation/post-process programs are hidden from Materials. Thumbnails use static previews so effects cannot escape their cells. Other profiles hide Assets and the pack collection, deactivate its generated script and skip environment drawing.

## Environment

The panel controls time/day duration, latitude, cloud morphology/altitude/wind, directional cloud regions, polar aurora, stars, Moon phase, disk sizes and optical rays/lens effects. Seasonal solar declination reverses with hemisphere; the clock is art-directed rather than a dated ephemeris.

Choose Clear, Cloudy or Rain / winter snow. Season presets choose matching weather; optional cycles advance seasons/weather automatically. Winter builds snow on exposed upward-facing brush surfaces and places nearby floes. Set **Water level for ice** to match your map. Warming melts snow and ice. These are bounded visual effects, without snow collision, footprints or a fluid simulation. Snow is limited to 256 surfaces / 4096 vertices. Editable MAP brushes are scanned incrementally; collision queries use the editor index without modifying picking results.

**Save settings** persists values in `reyond-shaders-settings.txt` beside project settings. Accumulation resets on map load. Baked lightmaps are not regenerated. Existing distant-cloud contour banding remains.

## Compile, preview and export

Compiler snapshots receive a private resource copy and shader list. Mod preview receives a separate copy in its temporary home. Original MAP files and game code are not replaced.

**Export pack to mod** copies namespaced assets, settings, license and optional SDK into the configured runtime mod directory. Exported `reyond-pack.cfg` records the current sky/weather state for a static preview. Automatic precipitation, accumulation, cycles and optics in an arbitrary game require integrating `sdk/reyond-pack.qc`: include after FTE builtin declarations, call `RSPFrame()` before `renderscene()` and `RSPFinish()` afterward, and execute the configuration. Merge post-processing/user-data usage with an existing CSQC renderer when necessary. The editor never replaces the mod's `progs.dat` or `csprogs.dat` automatically.

## Licensing

All distributed pack files are original work under **MIT**, permitting commercial use with the copyright/license notice retained. This is permissive licensing, not an assertion that copyright does not apply.

Atmosphere, clouds/noise volume, climate, weather, optics, snow and ice reuse original MIT modules of Reyond FTE Assets. Replacement water, flame, explosion, fog and ambient-irradiance effects use independent implementations and are not claimed to be pixel-identical to the old studies. Seascape, legacy Auroras/Sunset, restricted or unknown-license lab programs, their reference sources and adapted simulation code are excluded. See the archive's `PROVENANCE.md`. Editor/engine licenses remain unchanged.

## Test checklist

1. Download in FTE Custom game. Restart and verify installation and saved settings.
2. Apply each material to suitable geometry; use cards for fire/explosion and flat water/puddles.
3. Adjust time, climate, clouds, lunar phase and optics. Check solid-geometry occlusion.
4. Select Winter, wait for accumulation, edit brushes, then choose a warm season. Check outdoor precipitation and the configured water level.
5. Compile and inspect BSP / Mod preview. Export to a test mod and verify its original VM binaries remain unchanged.
6. Switch profiles: Assets and pack materials must disappear; ordinary rendering must remain unchanged.

Developer commands: `python tests/test-shader-pack.py`, `python scripts/package-shader-pack.py --output <folder>`. Console `reyond_shader_pack status` reports installation/materials; `reyond_shader_status` reports precipitation/snow caches. No new FPS target is claimed by this feature.
