// SPDX-License-Identifier: MIT
!!ver 130 450
!!cvarf ryn_time_hour=12
!!cvarf ryn_latitude=0
!!cvarf ryn_declination=23.44
!!cvarf ryn_moon_phase=0.5
!!cvarf ryn_sunrise=6
!!cvarf ryn_sunset=18
!!cvarf ryn_snow_cover=0
!!cvarf ryn_ice_amount=0
!!cvarf ryn_winter_water_scene=1
!!cvarf rsp_water_level=0
!!cvarf sea_minx=-1024
!!cvarf sea_maxx=1024
!!cvarf sea_miny=-1024
!!cvarf sea_maxy=1024
#include "sys/defs.h"
varying vec3 winterPos;varying vec4 winterTint;
#ifdef VERTEX_SHADER
void main(){winterPos=v_position;winterTint=v_colour;gl_Position=ftetransform();}
#endif
#ifdef FRAGMENT_SHADER
#include "glsl/reyondpack/reyond_daylight.glsl"
#include "glsl/reyondpack/reyond_noise.glsl"
uniform float cvar_ryn_snow_cover,cvar_ryn_ice_amount,cvar_ryn_winter_water_scene;
uniform float cvar_rsp_water_level,cvar_sea_minx,cvar_sea_miny,cvar_sea_maxx,cvar_sea_maxy;
vec3 WinterNormal(){vec3 n=normalize(cross(dFdx(winterPos),dFdy(winterPos)));if(dot(n,e_eyepos-winterPos)<0.)n=-n;return n;}
vec3 WinterLighting(vec3 n){
 vec3 sun=RynSunDirection(),moon=RynMoonDirection();float day=RynDaylight();
 float lunar=.5-.5*dot(sun,moon);
 return vec3(.045,.065,.10)+vec3(.23,.28,.36)*day+vec3(.78,.73,.64)*max(dot(n,sun),0.)*day+vec3(.10,.15,.24)*max(dot(n,moon),0.)*(1.-day)*lunar;
}
void main(){
 vec2 p=winterPos.xy/26.;ivec2 cell=ivec2(floor(p));vec2 local=fract(p);float nearest=10.,second=10.;
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  ivec2 id=cell+ivec2(x,y);vec2 feature=vec2(RynRandom(ivec3(id,3)),RynRandom(ivec3(id,17)))+vec2(x,y);
  float distance=length(feature-local);if(distance<nearest){second=nearest;nearest=distance;}else second=min(second,distance);
 }
 float crack=1.-smoothstep(.01,.045,second-nearest);vec3 n=WinterNormal();
 vec3 base=mix(vec3(.35,.62,.75),vec3(.82,.9,.96),clamp(cvar_ryn_snow_cover,0.,1.)*.65);
 base=mix(base,vec3(.12,.28,.40),crack*.45);
 vec3 color=pow(max(base*WinterLighting(n),vec3(0.)),vec3(1./2.2))*winterTint.rgb;
 gl_FragColor=vec4(color,clamp(cvar_ryn_ice_amount*5.,0.,1.));
}
#endif
