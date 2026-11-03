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
!!cvarf ryn_fire_heat=0
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
uniform float cvar_ryn_fire_heat;
void main(){
 float amount=clamp(cvar_ryn_snow_cover,0.,1.);
 float snowNoise=RynNoise(winterPos*.055)*.65+RynNoise(winterPos*.19)*.35;
 float cover=smoothstep(1.-amount-.08,1.-amount+.08,snowNoise);
 if(cvar_ryn_winter_water_scene>.5 && winterPos.x>cvar_sea_minx && winterPos.x<cvar_sea_maxx && winterPos.y>cvar_sea_miny && winterPos.y<cvar_sea_maxy)cover*=smoothstep(cvar_rsp_water_level+1.,cvar_rsp_water_level+12.,winterPos.z);
 cover*=mix(1.,smoothstep(42.,88.,length(winterPos.xy-vec2(80.,0.))),cvar_ryn_fire_heat);
 if(cover<.001)discard;
 vec3 n=WinterNormal();float grain=RynNoise(winterPos*1.2);
 float footprint=max(length(dFdx(winterPos)),length(dFdy(winterPos)));
 grain=mix(grain,.5,smoothstep(.4,2.,footprint));
 vec3 albedo=vec3(.82,.9,1.)*(.94+.06*grain);
 vec3 color=pow(max(albedo*WinterLighting(n),vec3(0.)),vec3(1./2.2));
 gl_FragColor=vec4(color,cover);
}
#endif
