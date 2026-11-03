// SPDX-License-Identifier: MIT
textures/reyondpack/ocean
{
qer_editorimage textures/reyondpack/ocean.tga
surfaceparm water
surfaceparm trans
qer_trans .8
{
program glsl/reyondpack/water.glsl
map $refraction
map $reflection
depthwrite
}
}
textures/reyondpack/sky
{
qer_editorimage textures/reyondpack/sky.tga
surfaceparm sky
surfaceparm nodlight
{
program glsl/reyondpack/sky.glsl
map $whiteimage
map $linear:$nosrgb:$3d:textures/reyondpack/cloud-volume.ktx
}
}
textures/reyondpack/puddle
{
qer_editorimage textures/reyondpack/puddle.tga
surfaceparm trans
qer_trans .7
polygonoffset
{
program glsl/reyondpack/weather_puddle.glsl
map $whiteimage
map $linear:$nosrgb:$3d:textures/reyondpack/cloud-volume.ktx
blendfunc blend
}
}
textures/reyondpack/fog
{
qer_editorimage textures/reyondpack/fog.tga
surfaceparm fog
fogparms ( 0.35 0.42 0.50 ) 256
surfaceparm trans
surfaceparm nonsolid
qer_trans .5
cull none
{
program glsl/reyondpack/surface.glsl#FOG
map textures/reyondpack/fog.tga
rgbgen vertex
alphagen vertex
blendfunc blend
}
}
textures/reyondpack/fire
{
qer_editorimage textures/reyondpack/fire.tga
surfaceparm trans
surfaceparm nonsolid
qer_trans .5
cull none
{
program glsl/reyondpack/surface.glsl#FIRE
map textures/reyondpack/fire.tga
rgbgen vertex
alphagen vertex
blendfunc blend
}
}
textures/reyondpack/explosion
{
qer_editorimage textures/reyondpack/explosion.tga
surfaceparm trans
surfaceparm nonsolid
qer_trans .5
cull none
{
program glsl/reyondpack/surface.glsl#EXPLOSION
map textures/reyondpack/explosion.tga
rgbgen vertex
alphagen vertex
blendfunc blend
}
}
textures/reyondpack/ambient_gi
{
qer_editorimage textures/reyondpack/ambient_gi.tga
{
program glsl/reyondpack/surface.glsl#GI
map textures/reyondpack/ambient_gi.tga
rgbgen vertex
alphagen vertex
depthwrite
}
}
textures/reyondpack/snow
{
qer_editorimage textures/reyondpack/snow.tga
polygonoffset
{
program glsl/reyondpack/winter_snow.glsl
map $whiteimage
blendfunc blend
}
}
textures/reyondpack/ice
{
qer_editorimage textures/reyondpack/ice.tga
{
program glsl/reyondpack/winter_ice.glsl
map $whiteimage
depthwrite
}
}
reyondpack/post
{
affine
{
program glsl/reyondpack/post.glsl
map $sourcecolour
map $sourcedepth
nodepthtest
}
}
reyondpack/rain
{
cull none
{
map textures/reyondpack/rain.tga
rgbgen vertex
alphagen vertex
blendfunc blend
}
}
reyondpack/snow
{
cull none
{
map textures/reyondpack/snowflake.tga
rgbgen vertex
alphagen vertex
blendfunc blend
}
}
