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
!!samps refraction=0 reflection=1
!!cvarf rsp_wave_height=4
!!cvarf rsp_wave_speed=1
!!cvarf rsp_wave_length=180
!!cvarf rsp_water_alpha=0.8
#include "sys/defs.h"

varying vec3 waterPosition;varying vec4 waterClip;
#include "glsl/reyondpack/waves.glsl"
#ifdef VERTEX_SHADER
void main(){waterPosition=v_position;vec4 clip=ftetransform();waterClip=clip;gl_Position=clip;}
#endif
#ifdef FRAGMENT_SHADER
#include "glsl/reyondpack/reyond_daylight.glsl"
uniform float cvar_rsp_water_alpha;
void main(){
 vec3 wave=RSPWave(waterPosition.xy,e_time);
 vec3 normal=normalize(vec3(-wave.yz,1.)),view=normalize(e_eyepos-waterPosition);
 if(view.z<0.)normal=-normal;
 vec2 uv=clamp(waterClip.xy/waterClip.w*.5+.5+normal.xy*.012,vec2(.002),vec2(.998));
 vec3 behind=texture2D(s_refraction,uv).rgb,reflected=texture2D(s_reflection,vec2(uv.x,1.-uv.y)).rgb;
 float fresnel=.025+.975*pow(1.-abs(dot(view,normal)),5.);
 vec3 sun=RynSunDirection(),halfway=normalize(view+sun);
 float glitter=pow(max(dot(normal,halfway),0.),180.)*RynDaylight();
 vec3 transmission=mix(behind,vec3(.025,.15,.18),.45);
 vec3 color=mix(transmission,reflected,clamp(fresnel,.06,.96))+vec3(1.,.83,.55)*glitter*1.4;
 gl_FragColor=vec4(color,clamp(cvar_rsp_water_alpha,.1,1.));
}
#endif
