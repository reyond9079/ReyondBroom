// SPDX-License-Identifier: MIT
// Original Reyond shader pack implementation.
!!ver 130 450
!!cvarf ryn_time_hour=12
!!cvarf ryn_declination=23.44
!!cvarf ryn_moon_phase=0.5
!!cvarf ryn_sun_radius=0.266
!!cvarf ryn_moon_radius=0.272
!!cvarf ryn_lens_strength=0.6
!!cvarf ryn_ray_strength=0.7
!!cvarf ryn_weather_cloud=1
!!cvarf ryn_weather_rain=0
!!cvarf ryn_latitude=0
!!cvarf ryn_cloud_region_heading=0
!!cvarf ryn_cloud_region_width=360
!!cvarf ryn_cloud_region_feather=15
!!cvarf ryn_cloud_region_min=0
!!cvarf ryn_cloud_region_max=90
!!cvarf ryn_aurora_strength=1
!!cvarf ryn_sunrise=6
!!cvarf ryn_sunset=18
!!cvarf ryn_cloud_cover=0.5
!!cvarf ryn_cloud_density=1
!!cvarf ryn_cloud_base=1.2
!!cvarf ryn_cloud_depth=1.8
!!cvarf ryn_cloud_scale=1.5
!!cvarf ryn_cloud_wind=18
!!cvarf ryn_star_brightness=1
!!cvarf ryn_sky_exposure=1
!!cvarf ryn_cloud_steps=40
!!cvarf ryn_cloud_heading=35
!!cvarf ryn_cloud_type=0.8
!!cvarf ryn_cloud_cirrus=0.25
!!cvarf ryn_cloud_erosion=0.65
!!cvarf rsp_surface_kind=0
!!cvarf rsp_effect_strength=1
#include "sys/defs.h"

varying vec3 surfacePosition;varying vec2 uv;varying vec4 tint;
#ifdef VERTEX_SHADER
void main(){surfacePosition=v_position;uv=v_texcoord;tint=v_colour;gl_Position=ftetransform();}
#endif
#ifdef FRAGMENT_SHADER
#include "glsl/reyondpack/reyond_noise.glsl"
#include "glsl/reyondpack/reyond_daylight.glsl"
uniform float cvar_rsp_effect_strength;
// Surface kind is selected by a material permutation, not one global toggle.
float RSPTurbulence(vec3 p){float sum=0.,weight=.5;for(int j=0;j<4;j++){sum+=RynNoise(p)*weight;p=p*2.13+vec3(3.,7.,11.);weight*=.5;}return sum;}
void main(){
 vec3 p=surfacePosition;vec3 color=vec3(0.);float alpha=1.;
 vec3 axis=abs(cross(dFdx(p),dFdy(p)));vec2 plane=axis.z>max(axis.x,axis.y)?p.xy:axis.x>axis.y?p.yz:p.xz;vec2 effectUV=plane/vec2(128.,192.);
#ifdef FIRE
 vec2 q=vec2(fract(effectUV.x)*2.-1.,1.-fract(effectUV.y));
 float drift=RSPTurbulence(vec3(q*3.,-e_time*1.3));
 float silhouette=(1.-q.y)*(.85+.32*drift)-abs(q.x+.18*sin(q.y*8.-e_time*2.));
 alpha=smoothstep(-.12,.1,silhouette)*smoothstep(0.,.12,q.y)*(1.-smoothstep(.75,1.,q.y));
 float hot=clamp(silhouette*1.7+(1.-q.y)*.25,0.,1.);
 color=mix(vec3(.8,.045,.006),vec3(1.,.58,.055),smoothstep(.05,.5,hot));
 color=mix(color,vec3(1.,.95,.65),smoothstep(.5,.95,hot));
#elif defined(EXPLOSION)
 vec2 q=fract(effectUV)*2.-1.;float age=fract(e_time*.35),r=length(q);
 float shell=.15+age*.85;
 float n=RSPTurbulence(vec3(q*4.,age*6.));
 alpha=(1.-smoothstep(shell-.15,shell+.05,r+.15*n))*(1.-smoothstep(.6,1.,age));
 color=mix(vec3(.12,.10,.09),vec3(1.,.35,.025),pow(1.-age,2.));color+=vec3(1.,.72,.28)*exp(-r*7.)*(1.-age);
#elif defined(FOG)
 float turbulence=RSPTurbulence(p*.012+vec3(e_time*.03,0.,0.));
 float optical=.25+.35*turbulence;
 alpha=(1.-exp(-optical))*clamp(cvar_rsp_effect_strength,0.,3.);
 float scattering=.35+.65*pow(max(dot(normalize(p-e_eyepos),RynSunDirection()),0.),8.);
 color=mix(vec3(.35,.45,.55),vec3(.9,.8,.6),RynDaylight())*scattering;
#elif defined(GI)
 vec3 n=normalize(cross(dFdx(p),dFdy(p)));if(dot(n,e_eyepos-p)<0.)n=-n;
 vec3 sky=vec3(.20,.34,.52)*(.5+.5*n.z),ground=vec3(.26,.16,.07)*(.5-.5*n.z);
 vec3 direct=vec3(.9,.8,.65)*max(dot(n,RynSunDirection()),0.)*RynDaylight();
 color=(sky+ground+direct)*tint.rgb;
#endif
 gl_FragColor=vec4(color,clamp(alpha,0.,1.));
}
#endif
