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
!!cvarf rsp_wave_height=4
!!cvarf rsp_wave_speed=1
!!cvarf rsp_wave_length=180
!!cvarf rsp_water_alpha=0.8
#include "sys/defs.h"
varying vec3 waterPosition;
#include "glsl/reyondpack/waves.glsl"
#ifdef VERTEX_SHADER
void main(){waterPosition=v_position;gl_Position=ftetransform();}
#endif
#ifdef FRAGMENT_SHADER
#include "glsl/reyondpack/reyond_atmosphere_shared.glsl"
uniform float cvar_rsp_water_alpha;
// Editable MAP meshes have no BSP reflection portal. Reflect the procedural sky;
// normal alpha blending supplies the visible geometry beneath the surface.
void main(){
 vec3 wave=vec3(0.);
 for(int band=0;band<6;band++){
  float b=float(band),heading=.42+b*1.91;
  vec2 direction=vec2(cos(heading),sin(heading));
  float k=6.2831853/max(cvar_rsp_wave_length,8.)*pow(1.79,b);
  float footprint=length(vec2(dot(dFdx(waterPosition.xy),direction),dot(dFdy(waterPosition.xy),direction)))*k;
  float amplitude=cvar_rsp_wave_height*pow(.48,b)*(1.-smoothstep(.35,1.5,footprint));
  float phase=dot(waterPosition.xy,direction)*k-e_time*sqrt(800.*k)*cvar_rsp_wave_speed+b*2.37;
  wave.x+=amplitude*sin(phase);wave.yz+=amplitude*k*cos(phase)*direction;
 }
 vec3 normal=normalize(vec3(-wave.yz,1.)),view=normalize(e_eyepos-waterPosition);
 if(view.z<0.)normal=-normal;
 float fresnel=.025+.975*pow(1.-abs(dot(view,normal)),5.);
 vec3 reflected=RynSky(reflect(-view,normal),e_eyepos);
 vec3 sun=RynSunDirection(),halfway=normalize(view+sun);
 float power=mix(180.,12.,clamp(length(fwidth(normal))*2.,0.,1.));
 float glitter=pow(max(dot(normal,halfway),0.),power)*(power/180.)*RynDaylight();
 vec3 transmission=vec3(.025,.15,.18)*mix(.06,1.,RynDaylight());
 vec3 color=mix(transmission,reflected,clamp(fresnel,.06,.96))+vec3(1.,.83,.55)*glitter*1.4;
 gl_FragColor=vec4(color,clamp(cvar_rsp_water_alpha,.1,1.));
}
#endif
