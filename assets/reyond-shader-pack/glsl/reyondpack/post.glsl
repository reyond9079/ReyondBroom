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
!!samps screen=0 depth=1
!!cvarf rsp_fog=0
!!cvarf rsp_fog_density=0.0005
#include "sys/defs.h"

varying vec2 tc;
#ifdef VERTEX_SHADER
void main(){tc=vec2(v_texcoord.x,1.-v_texcoord.y);gl_Position=ftetransform();}
#endif
#ifdef FRAGMENT_SHADER
#include "glsl/reyondpack/reyond_daylight.glsl"
#include "glsl/reyondpack/reyond_lens.glsl"
uniform float cvar_rsp_fog,cvar_rsp_fog_density;
void main(){
 vec3 color=texture2D(s_screen,tc).rgb;
 float z=texture2D(s_depth,tc).r;
 float nearPlane=4.,farPlane=max(w_user[6].x,8192.);
 float distance=2.*nearPlane*farPlane/(farPlane+nearPlane-(z*2.-1.)*(farPlane-nearPlane));
 vec3 ray=normalize(w_user[1].xyz+w_user[2].xyz*(tc.x*2.-1.)*w_user[1].w+w_user[3].xyz*(tc.y*2.-1.)*w_user[2].w);
 float transmittance=exp(-min(distance,6000.)*max(cvar_rsp_fog_density,0.)*cvar_rsp_fog);
 vec3 light=RynSunDirection();float phase=.35+.65*pow(max(dot(ray,light),0.),10.);
 vec3 scattering=mix(vec3(.025,.04,.07),vec3(.48,.55,.6),RynDaylight())*phase;
 color=color*transmittance+scattering*(1.-transmittance);
 gl_FragColor=vec4(color+RynLens(),1.);
}
#endif
