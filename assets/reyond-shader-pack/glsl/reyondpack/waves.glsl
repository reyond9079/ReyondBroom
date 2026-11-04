// SPDX-License-Identifier: MIT
uniform float cvar_rsp_wave_height,cvar_rsp_wave_speed,cvar_rsp_wave_length;
vec3 RSPWave(vec2 position,float clock){
 vec3 result=vec3(0.);float lengthScale=max(cvar_rsp_wave_length,8.);
 for(int band=0;band<6;band++){
  float b=float(band),heading=.42+b*1.91;
  vec2 direction=vec2(cos(heading),sin(heading));
  float k=6.2831853/lengthScale*pow(1.79,b);
  float amplitude=cvar_rsp_wave_height*pow(.48,b);
  float phase=dot(position,direction)*k-clock*sqrt(800.*k)*cvar_rsp_wave_speed+b*2.37;
  result.x+=amplitude*sin(phase);result.yz+=amplitude*k*cos(phase)*direction;
 }
 return result;
}

// Filter in the surface's angular footprint, independently of camera pitch/yaw.
// Screen-space derivatives of world XY changed the wave normals while merely
// looking around. NDC pixel size and the projection focal length stay constant.
vec3 RSPWaveFiltered(vec2 position,vec3 eye,float clock,float angularPixel){
 vec3 result=vec3(0.);float distance=max(length(eye),.001);vec3 ray=eye/distance;
 for(int band=0;band<6;band++){
  float b=float(band),heading=.42+b*1.91;
  vec2 direction=vec2(cos(heading),sin(heading));
  float k=6.2831853/max(cvar_rsp_wave_length,8.)*pow(1.79,b);
  float grazing=dot(direction,ray.xy)/max(abs(ray.z),.025);
  float footprint=distance*angularPixel*sqrt(1.+grazing*grazing)*k;
  float amplitude=cvar_rsp_wave_height*pow(.48,b)*(1.-smoothstep(.35,1.5,footprint));
  float phase=dot(position,direction)*k-clock*sqrt(800.*k)*cvar_rsp_wave_speed+b*2.37;
  result.x+=amplitude*sin(phase);result.yz+=amplitude*k*cos(phase)*direction;
 }
 return result;
}
