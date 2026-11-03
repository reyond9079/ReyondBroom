// SPDX-License-Identifier: MIT
// Camera optics sample visible image contrast; solid occlusion comes from CSQC.
uniform float cvar_ryn_sun_radius,cvar_ryn_moon_radius,cvar_ryn_lens_strength,cvar_ryn_ray_strength;
float RynLuma(vec3 c){return dot(c,vec3(.2126,.7152,.0722));}
vec3 RynLensSource(vec3 light,vec3 tint,float radius,float solidVisibility,float power){
 float forward=dot(light,w_user[1].xyz);
 if(forward<=.01 || light.z<=0. || solidVisibility<=.001)return vec3(0.);
 vec2 center=.5+.5*vec2(dot(light,w_user[2].xyz)/w_user[1].w,dot(light,w_user[3].xyz)/w_user[2].w)/forward;
 if(any(lessThan(center,vec2(.01))) || any(greaterThan(center,vec2(.99))))return vec3(0.);
 vec2 angular=radius*.0174532925/(2.*vec2(w_user[1].w,w_user[2].w)*forward);
 vec3 core=texture2D(s_screen,center).rgb;vec3 surround=vec3(0.);
 for(int j=0;j<4;j++){
  float a=float(j)*1.5707963;vec2 offset=vec2(cos(a),sin(a));
  core+=texture2D(s_screen,center+offset*angular*.45).rgb;
  surround+=texture2D(s_screen,center+offset*angular*3.5).rgb;
 }
 float contrast=RynLuma(core/5.)-RynLuma(surround/4.);
 float visibility=smoothstep(.015,.18,contrast)*solidVisibility*power;
 float aspect=w_user[1].w/w_user[2].w;vec2 delta=(tc-center)*vec2(aspect,1.);
 float r=length(delta),angle=atan(delta.y,delta.x);
 float spokes=pow(abs(cos(angle*6.)),28.)+pow(abs(cos(angle*4.+.37)),48.)*.4;
 vec3 rays=tint*spokes*exp(-r*10.)*.18*clamp(cvar_ryn_ray_strength,0.,2.);
 float halo=exp(-r*r/ .0025)*.06+exp(-pow((r-.095)*90.,2.))*.008;
 float streak=exp(-abs(delta.y)*520.)*exp(-abs(delta.x)*12.)*.025;
 vec3 optics=tint*(halo+streak);
 for(int j=0;j<4;j++){
  float k=float(j);vec2 ghost=.5+(.5-center)*(.55+k*.65);
  float d=length((tc-ghost)*vec2(aspect,1.));float size=.015+k*.009;
  vec3 chroma=.55+.45*cos(vec3(0.,2.,4.)+k*1.7);
  optics+=chroma*exp(-d*d/(size*size))*.014;
 }
 return visibility*(rays+optics*clamp(cvar_ryn_lens_strength,0.,2.));
}
vec3 RynLens(){
 if(cvar_ryn_lens_strength+cvar_ryn_ray_strength<.001)return vec3(0.);
 vec3 sun=RynSunDirection(),moon=RynMoonDirection();
 return RynLensSource(sun,mix(vec3(1.,.26,.06),vec3(1.,.76,.43),smoothstep(0.,.35,sun.z)),cvar_ryn_sun_radius,w_user[5].x,1.)+
 RynLensSource(moon,mix(vec3(1.,.56,.24),vec3(.52,.69,1.),smoothstep(0.,.25,moon.z)),cvar_ryn_moon_radius,w_user[5].y,(.5-.5*dot(sun,moon))*.20);
}
