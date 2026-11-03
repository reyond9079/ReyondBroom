// SPDX-License-Identifier: MIT
// Original time-of-day orbit shared by the atmosphere and water adapters.
uniform float cvar_ryn_declination,cvar_ryn_moon_phase;
uniform float cvar_ryn_time_hour,cvar_ryn_sunrise,cvar_ryn_sunset,cvar_ryn_latitude;
vec3 RynSunDirection(){
 float hour=mod(cvar_ryn_time_hour,24.);
 float rise=clamp(cvar_ryn_sunrise,0.,11.5),fall=clamp(cvar_ryn_sunset,12.,23.5);
 float phase;
 if(hour>=rise && hour<fall)phase=(hour-rise)/(fall-rise)*3.14159265;
 else phase=3.14159265+mod(hour-fall+24.,24.)/(24.-fall+rise)*3.14159265;
 // East/north/up equatorial solar orbit with seasonal declination.
 float latitude=clamp(cvar_ryn_latitude,-90.,90.)*.0174532925;
 float declination=clamp(cvar_ryn_declination,-23.44,23.44)*.0174532925;
 return normalize(vec3(cos(phase)*cos(declination),cos(latitude)*sin(declination)-sin(latitude)*cos(declination)*sin(phase),sin(latitude)*sin(declination)+cos(latitude)*cos(declination)*sin(phase)));
}
float RynDaylight(){return smoothstep(-.14,.18,RynSunDirection().z);}

vec3 RynMoonDirection(){
 vec3 sun=RynSunDirection();float latitude=clamp(cvar_ryn_latitude,-90.,90.)*.0174532925;
 vec3 pole=vec3(0.,cos(latitude),sin(latitude));vec3 axis=normalize(pole-sun*dot(pole,sun));float phase=cvar_ryn_moon_phase*6.2831853;
 return normalize(sun*cos(phase)+cross(axis,sun)*sin(phase)+axis*dot(axis,sun)*(1.-cos(phase))+axis*.06*cos(phase));
}
