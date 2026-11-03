// SPDX-License-Identifier: GPL-2.0-or-later
// Preserve the original retained mesh plane test and sampling.
!!ver 400-450
!!cvarf ftebroom_studio_material=0
!!samps sourcetex=0
uniform vec3 e_eyepos;
varying vec3 worldpos;
#include "glsl/ftebroom_studio_shared.h"
#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
attribute vec2 v_lmcoord;
attribute vec3 v_normal;
attribute vec4 v_colour;
varying vec2 tc;
varying vec2 material;
varying vec4 vc;
void main(){worldpos=v_position.xyz;tc=v_texcoord;material=v_lmcoord;vc=v_colour;precise vec3 products=e_eyepos*v_normal;precise float side=((products.x+products.y)+products.z)-v_lmcoord.y;gl_Position=side < -0.01 ? vec4(2.0,2.0,2.0,1.0) : ftetransform();}
#endif
#ifdef FRAGMENT_SHADER
uniform float cvar_ftebroom_studio_material;
varying vec2 tc;
varying vec2 material;
varying vec4 vc;
void main(){
#ifdef UNTEXTURED
vec4 fc=vc;
#else
vec4 texel=texture2D(s_sourcetex,tc);vec4 fc=texel*vc;if(cvar_ftebroom_studio_material>0.5)fc=vec4(reyondStudio(texel.rgb,worldpos,e_eyepos),texel.a*vc.a);
#endif
int a=int(material.x+0.5);if((a==1&&!(fc.a>0.0))||(a==2&&!(fc.a<0.5))||(a==3&&!(fc.a>=0.5)))discard;gl_FragColor=fc;}
#endif
