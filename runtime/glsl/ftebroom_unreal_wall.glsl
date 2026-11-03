// SPDX-License-Identifier: MIT
!!ver 100-450
!!permu FOG
!!samps diffuse=0
// Original rough dielectric studio material; no Unreal Engine source is used.
#include "sys/defs.h"
#include "sys/fog.h"
#include "glsl/ftebroom_studio_shared.h"
varying vec2 tc;
varying vec3 worldpos;
varying vec4 tint;
#ifdef VERTEX_SHADER
void main(){
    tc=v_texcoord;worldpos=v_position.xyz;tint=v_colour;
    gl_Position=ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
void main(){
    vec4 base=texture2D(s_diffuse,tc);
#if defined(MASK)
#if defined(MASKLT)
    if(base.a<MASK)discard;
#else
    if(base.a>=MASK)discard;
#endif
#endif
    vec3 colour=reyondStudio(base.rgb,worldpos,e_eyepos);
    gl_FragColor=fog4(vec4(colour,base.a*tint.a));
}
#endif
