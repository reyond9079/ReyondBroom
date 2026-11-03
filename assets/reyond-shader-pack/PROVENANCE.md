# Provenance

All distributed files are original work under MIT. Copyright and license notices must be retained. MIT permits commercial use; this pack is not a claim that copyright does not apply.

The sky, cloud volume, original noise, daylight, lunar rendering, optics, winter snow/ice and weather puddles come from independently written MIT modules of Reyond FTE Assets. The wave-water, fire, explosion, material fog, depth fog and ambient-irradiance materials are new implementations for this pack.

Excluded: Seascape and its adapters, legacy Auroras and Sunset, all lab_* shaders, adapted finite-difference puddle simulation, the supplied Shadertoy reference sources, their noise assets, and the standalone reference scenes. No restricted source is renamed or relicensed here.

The replacements use different algorithms and do not promise pixel-identical results: analytic wave normals with native planar reflection/refraction; procedural surface flame/explosion cards; depth-integrated homogeneous atmospheric scattering; hemispherical ambient irradiance. Ambient GI is a material approximation, not arbitrary-scene multi-bounce GI. Brush fog is a translucent material; the optional environment fog pass integrates view depth. Neither is the old reference's shadowed volume renderer.
