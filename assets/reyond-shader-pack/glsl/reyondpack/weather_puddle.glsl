// SPDX-License-Identifier: MIT
// Original thin weather puddles with procedural rain ripples and sky reflection.
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
#include "sys/defs.h"
varying vec2 tc;varying vec3 worldPos;varying vec4 tint;
#ifdef VERTEX_SHADER
void main(){tc=v_texcoord;worldPos=v_position;tint=v_colour;gl_Position=ftetransform();}
#endif
#ifdef FRAGMENT_SHADER
#include "glsl/reyondpack/reyond_atmosphere_shared.glsl"
void main(){
 vec2 p=(tc-.5)*2.;
 float seed=floor(worldPos.x/112.)+floor(worldPos.y/112.)*13.;
 float boundary=length(p*vec2(1.,1.13))+(RynNoise(vec3(tc*4.,seed))-.5)*.25+(RynNoise(vec3(tc*11.,seed+3.))-.5)*.05;
 float size=mix(.25,.96,clamp(tint.a,0.,1.));
 float alpha=(1.-smoothstep(size-.10,size,boundary))*smoothstep(0.,.14,tint.a);
 if(alpha<.002)discard;
 vec2 slope=vec2(0.);
 for(int i=0;i<3;i++){
  float k=float(i),cycle=e_time*1.7+k*.79;
  vec2 center=vec2(.25+.5*fract(sin(floor(cycle)*13.1+k)*437.1),.25+.5*fract(cos(floor(cycle)*17.3+k)*751.7));
  vec2 delta=tc-center;float radius=length(delta),age=fract(cycle);
  float wave=cos((radius-age*.65)*95.)*exp(-abs(radius-age*.65)*32.)*(1.-age);
  slope+=delta/max(radius,.01)*wave*.055*cvar_ryn_weather_rain;
 }
 vec3 normal=normalize(vec3(slope,1.));vec3 view=normalize(e_eyepos-worldPos);
 vec3 reflected=reflect(-view,normal);
 vec3 sky=RynSkyBackground(reflected);
 float fresnel=.05+.95*pow(1.-max(dot(view,normal),0.),5.);
 vec3 color=mix(vec3(.035,.045,.05),sky,.42+.58*fresnel);
 color+=vec3(.07,.09,.11)*smoothstep(.012,.045,length(slope))*cvar_ryn_weather_rain;
 vec3 light=normalize(RynSunDirection());
 color+=vec3(.3)*pow(max(dot(reflect(-light,normal),view),0.),80.)*.3;
 gl_FragColor=vec4(color,alpha*(.30+.60*fresnel));
}
#endif
