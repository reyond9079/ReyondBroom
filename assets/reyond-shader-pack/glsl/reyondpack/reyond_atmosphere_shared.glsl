// SPDX-License-Identifier: MIT
// Original Reyond FTE Assets atmosphere. No third-party shader source is used.
#include "glsl/reyondpack/reyond_daylight.glsl"
uniform float cvar_ryn_cloud_cover,cvar_ryn_cloud_density,cvar_ryn_cloud_base;
uniform float cvar_ryn_cloud_depth,cvar_ryn_cloud_scale,cvar_ryn_cloud_wind;
uniform float cvar_ryn_star_brightness,cvar_ryn_sky_exposure,cvar_ryn_cloud_steps;
uniform float cvar_ryn_cloud_heading;
uniform float cvar_ryn_weather_cloud,cvar_ryn_weather_rain;
float RynWeatherCover(){return mix(max(cvar_ryn_cloud_cover,.45),1.,cvar_ryn_weather_rain)*clamp(cvar_ryn_weather_cloud,0.,1.);}
uniform float cvar_ryn_cloud_region_heading,cvar_ryn_cloud_region_width,cvar_ryn_cloud_region_feather;
uniform float cvar_ryn_cloud_region_min,cvar_ryn_cloud_region_max,cvar_ryn_aurora_strength;
uniform float cvar_ryn_cloud_type,cvar_ryn_cloud_cirrus,cvar_ryn_cloud_erosion;

#include "glsl/reyondpack/reyond_noise.glsl"
uniform float cvar_ryn_sun_radius,cvar_ryn_moon_radius;
vec3 RynAtmosphere(vec3 direction){
 vec3 d=normalize(direction),sun=RynSunDirection();
 float day=RynDaylight(),alt=max(d.z,0.);
 float rim=pow(1.-alt,.65),forward=pow(max(dot(d,sun),0.),10.);
 vec3 night=mix(vec3(.0018,.0035,.012),vec3(.014,.023,.043),rim);
 vec3 noon=mix(vec3(.055,.23,.59),vec3(.57,.78,1.05),rim);
 float twilight=(1.-smoothstep(.04,.36,abs(sun.z)))*smoothstep(-.23,.04,sun.z);
 vec3 warm=vec3(1.35,.27,.065)*pow(1.-alt,3.)*(.18+.82*forward);
 vec3 color=mix(night,noon,day)+warm*twilight;
 // Continue the lower hemisphere with atmospheric ground haze, without an empty cap.
 float ground=smoothstep(-.22,.015,d.z);
 vec3 groundHaze=mix(vec3(.003,.006,.013),vec3(.16,.23,.28),day);
 vec3 result=mix(groundHaze,color,ground);
 float gray=dot(result,vec3(.2126,.7152,.0722));
 return mix(result,mix(result,vec3(gray),.7)*.50,clamp(cvar_ryn_weather_rain,0.,1.));
}
vec3 RynDisplay(vec3 radiance){
 vec3 mapped=1.-exp(-max(radiance,vec3(0.))*clamp(cvar_ryn_sky_exposure,.2,3.));
 return pow(mapped,vec3(1./2.2));
}
vec3 RynSkyBackground(vec3 direction){return RynDisplay(RynAtmosphere(direction));}
// A directional weather sector stays aligned to world north, not the camera yaw.
float RynCloudRegion(vec3 direction){
 vec3 d=normalize(direction);float width=mix(clamp(cvar_ryn_cloud_region_width,0.,360.),360.,cvar_ryn_weather_rain);
 if(width<.001)return 0.;
 float heading=atan(d.x,d.y)*57.2957795;
 float delta=abs(mod(heading-cvar_ryn_cloud_region_heading+540.,360.)-180.);
 float feather=max(cvar_ryn_cloud_region_feather,.1);
 float azimuth=width>=359.99?1.:1.-smoothstep(max(width*.5-feather,0.),width*.5,delta);
 // Azimuth is undefined at the zenith: fade its influence smoothly there.
 azimuth=mix(1.,azimuth,smoothstep(.001,.03,length(d.xy)));
 float elevation=asin(clamp(d.z,-1.,1.))*57.2957795;
 float low=mix(clamp(cvar_ryn_cloud_region_min,0.,89.),0.,cvar_ryn_weather_rain);
 float high=max(low+.1,mix(clamp(cvar_ryn_cloud_region_max,0.,90.),90.,cvar_ryn_weather_rain));
 float lower=low<.01?1.:smoothstep(low,min(low+feather,high),elevation);
 float upper=high>=89.99?1.:1.-smoothstep(max(low,high-feather),high,elevation);
 return azimuth*lower*upper;
}
vec3 RynCelestialDirection(vec3 d){
 float latitude=clamp(cvar_ryn_latitude,-90.,90.)*.0174532925;
 vec3 pole=vec3(0.,cos(latitude),sin(latitude));
 vec3 meridian=vec3(0.,-sin(latitude),cos(latitude));
 return vec3(d.x,dot(d,meridian),dot(d,pole));
}
// Original animated emission curtains. No legacy aurora shader code is used.
vec3 RynPolarAurora(vec3 direction){
 float polar=smoothstep(55.,75.,abs(cvar_ryn_latitude));
 float night=pow(1.-smoothstep(-.20,.015,RynSunDirection().z),2.);
 if(polar*night*cvar_ryn_aurora_strength<.001 || direction.z<=0.)return vec3(0.);
 vec3 d=normalize(direction);float hemisphere=cvar_ryn_latitude>=0.?1.:-1.;
 float azimuth=atan(d.x,d.y*hemisphere);
 float elevation=atan(d.z,max(length(d.xy),.001));
 float facing=smoothstep(-.45,.5,cos(azimuth));
 float t=e_time*.08;vec3 emission=vec3(0.);
 for(int layer=0;layer<5;layer++){
  float k=float(layer),a=azimuth+k*.13;
  float arc=.24+polar*.13+k*.07+.075*sin(a*2.3+t)+.035*sin(a*7.1-t*.7+k);
  float height=elevation-arc;
  float curtain=smoothstep(-.04,.015,height)*exp(-max(height,0.)*8.);
  float folds=.25+.75*pow(.5+.5*sin(a*42.+sin(a*9.+t)*3.+t*1.7+k),3.);
  float drift=.45+.55*RynNoise(vec3(a*6.+k,t*.5,k*4.));
  vec3 color=mix(vec3(.08,1.1,.42),vec3(.55,.12,.85),smoothstep(.07,.28,height));
  emission+=color*curtain*folds*drift*.32;
 }
 return emission*facing*polar*night*clamp(cvar_ryn_aurora_strength,0.,3.);
}
vec3 RynStars(vec3 direction){
 float night=pow(1.-smoothstep(-.22,.04,RynSunDirection().z),3.);
 if(night<.002 || cvar_ryn_star_brightness<=0.)return vec3(0.);
 vec3 celestial=RynCelestialDirection(normalize(direction));
 vec3 q=celestial*96.;ivec3 cell=ivec3(floor(q));vec3 stars=vec3(0.);
 float pixel=max(length(dFdx(q)),length(dFdy(q)));
 for(int z=-1;z<=1;z++)for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  ivec3 id=cell+ivec3(x,y,z);float r=RynRandom(id);
  if(r<.72)continue;
  vec3 center=vec3(id)+vec3(RynRandom(id+ivec3(41,17,3)),RynRandom(id+ivec3(7,63,19)),RynRandom(id+ivec3(23,5,71)));
  float radius=mix(.035,.085,r),variance=radius*radius+pixel*pixel*.18;
  vec3 delta=q-center;float energy=exp(-dot(delta,delta)/variance)*radius*radius/variance;
  vec3 tint=mix(vec3(1.,.62,.36),vec3(.55,.74,1.),RynRandom(id+ivec3(79,11,37)));
  float twinkle=.94+.06*sin(e_time*(.8+r)+r*153.);
  stars+=tint*energy*(.8+3.*pow(r,8.))*twinkle;
 }
 vec3 beltAxis=normalize(vec3(.24,-.61,.75));
 float belt=exp(-pow(dot(celestial,beltAxis)*7.,2.));
 float wisps=.35+.65*RynNoise(celestial*13.+vec3(12.,5.,29.));
 return (stars+vec3(.025,.032,.055)*belt*wisps)*night*clamp(cvar_ryn_star_brightness,0.,3.);
}
float RynLayerHeight(vec3 p){
 // Rationalized height avoids cancelling two planet-sized floating point values.
 return (dot(p,p)+12742.*p.z)/(length(vec3(p.xy,p.z+6371.))+6371.);
}
float RynLayerExit(vec3 origin,vec3 direction,float height){
 // Expanded quadratic avoids subtracting two large squared planet radii.
 float b=dot(origin,direction)+6371.*direction.z;
 float c=dot(origin,origin)+2.*6371.*(origin.z-height)-height*height;
 float root=sqrt(max(b*b-c,0.));
 return b>=0.?-c/max(b+root,.00001):-b+root;
}
// Native volume is shared by sky and reflections, with hardware trilinear filtering.
#ifndef RYN_CLOUD_SLOT
#define RYN_CLOUD_SLOT s_t1
#endif
uniform sampler3D RYN_CLOUD_SLOT;
vec3 RynCloudField(vec3 p){
 return textureLod(RYN_CLOUD_SLOT,p+vec3(.5/126.),0.).rgb;
}
// Weather controls cluster placement independently of individual cloud billows.
// Distances are kilometres; all density and lighting paths use the same field.
float RynCloudDensity(vec3 point,bool detail,float footprint){
 float base=max(cvar_ryn_cloud_base,.25),depth=max(cvar_ryn_cloud_depth,.2);
 float region=RynCloudRegion(point-e_eyepos*.00005);
 if(region<.001)return 0.;
 float h=(RynLayerHeight(point)-base)/depth;
 if(h<=0. || h>=1.)return 0.;
 float angle=cvar_ryn_cloud_heading*.0174532925;
 vec2 wind=vec2(cos(angle),sin(angle))*e_time*cvar_ryn_cloud_wind/3600.;
 vec2 world=point.xy-wind;
 vec3 weather=RynCloudField(vec3(world/48.,.37));
 float cover=RynWeatherCover();
 float coverage=smoothstep(1.-cover-.20,1.-cover+.18,weather.r);
 if(coverage<.001)return 0.;
 float type=mix(clamp(cvar_ryn_cloud_type+(weather.g-.5)*.65,0.,1.),.25,cvar_ryn_weather_rain);
 // A broad, flat condensation base and varying convective tops.
 float top=mix(.29,.98,type)*mix(.8,1.,weather.b);
 float profile=smoothstep(.0,.035,h)*(1.-smoothstep(top*.75,top,h));
 if(profile<.001)return 0.;
 vec3 p=vec3(world+vec2(h*h*.65,h*.22),point.z-base)/max(cvar_ryn_cloud_scale,.3);
 vec3 field=RynCloudField(p*.33);
 // Integrate unresolved distant billows toward their mean instead of aliasing.
 field.r=mix(field.r,.49,smoothstep(.15,.65,footprint/max(cvar_ryn_cloud_scale,.3)));
 float threshold=mix(.76,.29,coverage)+(1.-profile)*.30;
 float body=clamp((field.r-threshold)/max(1.-threshold,.05),0.,1.);
 float detailFade=1.-smoothstep(.18,.5,footprint/max(cvar_ryn_cloud_scale,.3));
 if(detail && body>.001 && detailFade>.001){
  vec3 erosion=RynCloudField(p*1.13+vec3(.14,.71,.32));
  body=mix(body,clamp(body+(erosion.r-.5)*.32,0.,1.),detailFade);
  // Fine detail erodes only the boundary, leaving a coherent dense interior.
  float edge=mix(erosion.g,1.-erosion.g,smoothstep(.15,.8,h));
  body=max(0.,body-(edge*.65+erosion.b*.35)*.42*cvar_ryn_cloud_erosion*detailFade*(1.-body));
 }
 body=mix(body,max(body,.25),cvar_ryn_weather_rain);
 return region*pow(body,.7)*profile*mix(clamp(cvar_ryn_cloud_density,0.,3.)*.7,2.2,cvar_ryn_weather_rain)*5.5;
}
float RynPhase(float cosine,float g){
 return (1.-g*g)/(12.5663706*pow(max(1.+g*g-2.*g*cosine,.025),1.5));
}
float RynCloudShadow(vec3 point,vec3 light,float depth){
 float optical=0.,previous=0.;
 // Increasing intervals cover nearby detail and the entire cloud thickness.
 for(int j=0;j<3;j++){
  float end=depth*(exp2(float(j+1)*1.44)-1.)/19.;
  optical+=RynCloudDensity(point+light*(previous+end)*.5,false,end-previous)*(end-previous);
  previous=end;
 }
 return optical*2.1;
}
vec3 RynCirrus(vec3 d,vec3 origin,vec3 background){
 if(cvar_ryn_cloud_cirrus*cvar_ryn_weather_cloud<.001 || d.z<.005)return background;
 float distance=RynLayerExit(origin,d,8.);
 vec2 p=(origin+d*distance).xy;
 float angle=cvar_ryn_cloud_heading*.0174532925;
 p-=vec2(cos(angle),sin(angle))*e_time*cvar_ryn_cloud_wind/1800.;
 vec3 field=RynCloudField(vec3(p.x/60.+p.y/120.,p.y/7.,.82));
 float veil=smoothstep(.47,.74,field.r)*(.35+.65*field.g)*cvar_ryn_cloud_cirrus*cvar_ryn_weather_cloud;
 veil*=smoothstep(.005,.08,d.z)*RynCloudRegion(d);
 float day=RynDaylight();
 vec3 color=mix(vec3(.025,.034,.065),mix(vec3(.92,.32,.14),vec3(.85,.91,1.),smoothstep(.0,.35,RynSunDirection().z)),day);
 return mix(background,color,clamp(veil*.55,0.,.7));
}
vec3 RynClouds(vec3 direction,vec3 origin,vec3 background){
 background=RynCirrus(direction,origin,background);
 if(RynWeatherCover()<.001 || (cvar_ryn_cloud_density<.001 && cvar_ryn_weather_rain<.001) || direction.z<-.015)return background;
 float base=max(cvar_ryn_cloud_base,.25),depth=max(cvar_ryn_cloud_depth,.2);
 float first=max(RynLayerExit(origin,direction,base),0.);
 float last=min(RynLayerExit(origin,direction,base+depth),190.);
 if(last<=first)return background;
 int count=int(clamp(cvar_ryn_cloud_steps,24.,96.));
 float day=RynDaylight();vec3 sun=RynSunDirection();vec3 light=day>.1?sun:RynMoonDirection();
 float cosine=dot(direction,light);
 float phase=.85*RynPhase(cosine,.64)+.15*RynPhase(cosine,-.24);
 vec3 direct=mix(vec3(.05,.065,.10),mix(vec3(5.6,1.45,.38),vec3(4.5,4.35,4.0),smoothstep(.0,.5,sun.z)),day);
 vec3 skyAmbient=mix(vec3(.006,.011,.025),vec3(.28,.40,.60),day);
 vec3 groundAmbient=mix(vec3(.002,.003,.006),vec3(.07,.085,.09),day);
 direct*=mix(.1+.9*(.5-.5*dot(sun,RynMoonDirection())),1.,day);
 direct*=mix(1.,.40,cvar_ryn_weather_rain);skyAmbient*=mix(1.45,.30,cvar_ryn_weather_rain);groundAmbient*=mix(1.3,.4,cvar_ryn_weather_rain);
 float transmission=1.;vec3 scattered=vec3(0.);

 // Quadratic spacing resolves nearby silhouettes without deleting horizon clouds.
 for(int i=0;i<96;i++){
  if(i>=count || transmission<.008)break;
  float u0=float(i)/float(count),u1=float(i+1)/float(count);
  float start=first+(last-first)*mix(u0,u0*u0,.65);
  float end=first+(last-first)*mix(u1,u1*u1,.65);
  float stepLength=end-start;
  bool detail=true;
  float densities[3];float sumDensity=0.;
  for(int q=0;q<3;q++){
   float fraction=(float(q)+RynRandom(ivec3(gl_FragCoord.xy,float(q))))/3.;
   densities[q]=RynCloudDensity(origin+direction*mix(start,end,fraction),detail,stepLength/3.);
   sumDensity+=densities[q];
  }
  if(sumDensity<.001)continue;
  // Interpolate light visibility across the interval; constant step lighting
  // otherwise leaves visible contour bands even with supersampled density.
  float shadowStart=RynCloudShadow(origin+direction*start,light,depth);
  float shadowEnd=RynCloudShadow(origin+direction*end,light,depth);
  for(int q=0;q<3;q++){
   float fraction=(float(q)+RynRandom(ivec3(gl_FragCoord.xy,float(q))))/3.;
   vec3 point=origin+direction*mix(start,end,fraction);
   float density=densities[q],shadow=mix(shadowStart,shadowEnd,fraction);
   float height=clamp((RynLayerHeight(point)-base)/depth,0.,1.);
   vec3 multi=direct*(exp(-shadow)*phase+exp(-shadow*.38)*.055+exp(-shadow*.13)*.025);
   float skyVisibility=exp(-density*(1.-height)*.7);
   vec3 ambient=skyAmbient*(.35+.65*height)*skyVisibility+groundAmbient*(1.-height);
   vec3 lighting=ambient+multi;
   float opacity=1.-exp(-density*stepLength*2.1/3.);
   float haze=1.-exp(-mix(start,end,fraction)*.006);
   lighting=mix(lighting,RynAtmosphere(direction),haze);
   scattered+=transmission*opacity*lighting;transmission*=1.-opacity;
  }
 }
 return mix(background,background*transmission+scattered,smoothstep(-.015,.02,direction.z));
}
// Original procedural photosphere and cratered lunar sphere.
vec3 RynSunRadiance(vec3 d){
 vec3 sun=RynSunDirection();float radius=clamp(cvar_ryn_sun_radius,.1,3.)*.0174532925;
 float angle=acos(clamp(dot(d,sun),-1.,1.));
 vec3 vertical=(vec3(0.,0.,1.)-sun*sun.z)/max(sqrt(max(0.,1.-sun.z*sun.z)),.0001);
 float flatten=mix(.82,1.,smoothstep(0.,.12,sun.z));
 angle=sqrt(angle*angle+pow(dot(d,vertical),2.)*(1./(flatten*flatten)-1.));
 float pixel=max(length(dFdx(d)),length(dFdy(d)));
 float mask=1.-smoothstep(radius-pixel*.5,radius+pixel*.5,angle);
 float mu=sqrt(max(0.,1.-pow(angle/radius,2.)));
 float limb=.42+.58*mu;
 vec3 color=mix(vec3(24.,4.5,.6),vec3(42.,38.,32.),smoothstep(0.,.5,sun.z));
 float haze=exp(-pow(angle/max(radius*5.,.02),1.4))*.14;
 return (color*mask*limb+vec3(1.,.55,.20)*haze)*smoothstep(-.015,.015,sun.z);
}
vec3 RynMoonRadiance(vec3 d){
 vec3 moon=RynMoonDirection(),sun=RynSunDirection();float radius=clamp(cvar_ryn_moon_radius,.1,3.)*.0174532925;
 float angle=acos(clamp(dot(d,moon),-1.,1.));float pixel=max(length(dFdx(d)),length(dFdy(d)));
 if(angle>radius+pixel)return vec3(0.);
 float latitude=cvar_ryn_latitude*.0174532925;vec3 pole=vec3(0.,cos(latitude),sin(latitude));
 vec3 east=normalize(cross(pole,moon)),north=cross(moon,east);
 vec2 uv=vec2(dot(d,east),dot(d,north))/sin(radius);float r2=dot(uv,uv);
 vec3 sphere=vec3(uv,sqrt(max(0.,1.-r2)));
 vec3 normal=normalize(east*sphere.x+north*sphere.y-moon*sphere.z);
 float maria=smoothstep(.34,.61,RynNoise(sphere*3.1+vec3(4.,8.,1.)));
 float albedo=mix(.13,.36,maria)*(.85+.15*RynNoise(sphere*38.));
 float relief=1.;
 for(int k=0;k<12;k++){
  float id=float(k);vec2 center=vec2(RynRandom(ivec3(k,7,13)),RynRandom(ivec3(k,23,9)))*1.7-.85;
  float cr=.035+.12*RynRandom(ivec3(k,1,42));float dist=length(uv-center)/cr;
  relief-=.23*exp(-dist*dist*3.);relief+=.15*exp(-pow((dist-1.)*9.,2.));
 }
 float illumination=max(dot(normal,sun),0.);
 vec3 color=vec3(.93,.95,1.)*albedo*max(relief,.2)*(illumination*2.1+.012);
 float mask=1.-smoothstep(radius-pixel*.5,radius+pixel*.5,angle);
 vec3 transmission=exp(-vec3(.025,.06,.14)*(1./max(moon.z+.08,.08)-1./1.08));
 return color*transmission*mask*smoothstep(-.015,.02,moon.z);
}
vec3 RynSky(vec3 direction,vec3 camera){
 vec3 d=normalize(direction);
 vec3 stars=RynStars(d);
 vec3 radiance=RynAtmosphere(d)+(stars+RynPolarAurora(d))*smoothstep(-.06,.02,d.z);
 float moonRadius=clamp(cvar_ryn_moon_radius,.1,3.)*.0174532925;
 float lunarAngle=acos(clamp(dot(d,RynMoonDirection()),-1.,1.));
 float pixel=max(length(dFdx(d)),length(dFdy(d)));
 float moonMask=1.-smoothstep(moonRadius-pixel*.5,moonRadius+pixel*.5,lunarAngle);
 radiance-=stars*smoothstep(-.06,.02,d.z)*moonMask;
 radiance+=RynSunRadiance(d)*(1.-moonMask)+RynMoonRadiance(d);
 float phaseLight=.5-.5*dot(RynSunDirection(),RynMoonDirection());
 radiance+=vec3(.012,.019,.03)*exp(-lunarAngle/max(moonRadius*5.,.01))*phaseLight*smoothstep(0.,.04,RynMoonDirection().z);
 return RynDisplay(RynClouds(d,camera*.00005,radiance));
}
