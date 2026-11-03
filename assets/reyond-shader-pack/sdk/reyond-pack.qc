// SPDX-License-Identifier: MIT
// Optional CSQC integration. Include after FTE builtin declarations.
// Call RSPFrame immediately before renderscene and RSPFinish immediately after it.
#ifndef RSP_EDITOR
void RSPTrace(vector start,vector end,float nomonsters,entity ignore){traceline(start,end,nomonsters,ignore);}
float RSPContents(vector point){return pointcontents(point);}
#endif
float RSPIsSky(string name){return substring(name,0,3)=="sky" || name=="textures/reyondpack/sky" || name=="reyondpack/sky" || substring(name,0,15)=="textures/skies/";}
float RSPMod(float x,float y){return x-y*floor(x/y);}
float RSPFract(float x){return x-floor(x);}
float RSPSmooth(float a,float b,float x){x=bound(0,(x-a)/(b-a),1);return x*x*(3-2*x);}
float RSPFloor(vector p){RSPTrace(p+'0 0 512',p-'0 0 2048',1,world);return trace_endpos_z;}
float RSPHeight(vector p,float clock){
 float height=cvar("rsp_water_level");
 for(float band=0;band<6;band+=1){float heading=.42+band*1.91,k=6.2831853/max(8,cvar("rsp_wave_length"))*pow(1.79,band);
 height+=cvar("rsp_wave_height")*pow(.48,band)*sin((p_x*cos(heading)+p_y*sin(heading))*k-clock*sqrt(800*k)*cvar("rsp_wave_speed")+band*2.37);}
 return height;
}
// SPDX-License-Identifier: MIT
// CPU counterpart of the shader orbit, plus bounded geometry visibility checks.
var float rsp_celestial_sun_visible=0,rsp_celestial_moon_visible=0,rsp_celestial_clock=-1;
vector RSPCelestialSun(){
 float hour=cvar("ryn_time_hour"),rise=bound(0,cvar("ryn_sunrise"),11.5),fall=bound(12,cvar("ryn_sunset"),23.5),phase;
 hour=hour-24*floor(hour/24);
 if(hour>=rise && hour<fall)phase=(hour-rise)/(fall-rise)*3.14159265;
 else phase=3.14159265+RSPMod(hour-fall+24,24)/(24-fall+rise)*3.14159265;
 float latitude=bound(-90,cvar("ryn_latitude"),90)*.0174532925,declination=cvar("ryn_declination")*.0174532925;
 return normalize([cos(phase)*cos(declination),cos(latitude)*sin(declination)-sin(latitude)*cos(declination)*sin(phase),sin(latitude)*sin(declination)+cos(latitude)*cos(declination)*sin(phase)]);
}
vector RSPCelestialMoon(){
 vector sun=RSPCelestialSun();float latitude=cvar("ryn_latitude")*.0174532925,phase=cvar("ryn_moon_phase")*6.2831853;
 vector pole=[0,cos(latitude),sin(latitude)];vector axis=normalize(pole-sun*(pole*sun));
 vector cross=[axis_y*sun_z-axis_z*sun_y,axis_z*sun_x-axis_x*sun_z,axis_x*sun_y-axis_y*sun_x];
 return normalize(sun*cos(phase)+cross*sin(phase)+axis*(axis*sun)*(1-cos(phase))+axis*.06*cos(phase));
}
float RSPCelestialVisible(vector eye,vector light,float radius){
 if(light_z<=0)return 0;
 vector reference=fabs(light_z)>.95?'0 1 0':'0 0 1';
 vector side=normalize([reference_y*light_z-reference_z*light_y,reference_z*light_x-reference_x*light_z,reference_x*light_y-reference_y*light_x]);
 vector up=[light_y*side_z-light_z*side_y,light_z*side_x-light_x*side_z,light_x*side_y-light_y*side_x];
 float visible=0;
 for(float i=0;i<5;i+=1){
  vector direction=light;
  if(i==1)direction+=side*radius;if(i==2)direction-=side*radius;if(i==3)direction+=up*radius;if(i==4)direction-=up*radius;
  RSPTrace(eye,eye+normalize(direction)*8192,1,world);
  if(trace_fraction>=1 || RSPIsSky(trace_surfacename) || RSPIsSky(trace_dphittexturename))visible+=.2;
 }
 return visible;
}
void RSPCelestialUpdate(float now,vector eye){
 if(now-rsp_celestial_clock<.05)return;rsp_celestial_clock=now;
 rsp_celestial_sun_visible=RSPCelestialVisible(eye,RSPCelestialSun(),cvar("ryn_sun_radius")*.0174532925);
 rsp_celestial_moon_visible=RSPCelestialVisible(eye,RSPCelestialMoon(),cvar("ryn_moon_radius")*.0174532925);
}
// SPDX-License-Identifier: MIT
// Bounded local weather state, outdoor precipitation and ground-water patches.
var float rsp_weather_cloud=1,rsp_weather_rain=0,rsp_weather_wet=0,rsp_weather_mode=1;
var float rsp_weather_next=0,rsp_weather_cycle=0,rsp_weather_cursor=0,rsp_weather_interval=120;
vector rsp_weather_positions[49],rsp_weather_drop_positions[490];
float rsp_weather_drop_valid[490],rsp_weather_water[49];
var float rsp_winter_snow=0,rsp_winter_ice=0,rsp_winter_cold=0;
var float rsp_winter_faces=0,rsp_winter_vertices=0,rsp_winter_scan=0,rsp_winter_empty=0,rsp_winter_scanned=0;
float rsp_winter_face_id[256],rsp_winter_face_start[256],rsp_winter_face_count[256];
vector rsp_winter_points[4096],rsp_winter_centers[256];
float rsp_weather_keys_x[49],rsp_weather_keys_y[49],rsp_weather_ready[49],rsp_weather_open[49],rsp_weather_flat[49];
void RSPsonUpdate(float dt){
 float phase=cvar("ryn_season");
 if(cvar("ryn_season_auto"))phase+=dt*4./(max(1,cvar("ryn_year_minutes"))*60.);
 phase=RSPMod(phase,4);cvar_set("ryn_season",sprintf("%.9g",phase));
 float previousCold=rsp_winter_cold;float cold=max(0,cos((phase-3)*1.5707963268));rsp_winter_cold=RSPSmooth(.45,.85,cold);
 if(cvar("ryn_season_auto")){
  if(previousCold<.5 && rsp_winter_cold>=.5)cvar_set("ryn_weather_mode","2");
  if(previousCold>=.5 && rsp_winter_cold<.5)cvar_set("ryn_weather_mode","1");
 }
 float hemisphere=cvar("ryn_latitude")>=0?1:-1;
 cvar_set("ryn_declination",ftos(sin(phase*1.5707963268)*23.44*hemisphere));
 float snowfall=rsp_weather_rain*rsp_winter_cold;
 if(snowfall>.001)rsp_winter_snow=min(1,rsp_winter_snow+dt*snowfall/18.);
 else rsp_winter_snow=max(0,rsp_winter_snow-dt*(1-rsp_winter_cold)/30.);
 rsp_winter_ice=bound(0,rsp_winter_ice+dt*(rsp_winter_cold/45.-(1-rsp_winter_cold)/35.),1);
 cvar_set("ryn_winter_cold",ftos(rsp_winter_cold));cvar_set("ryn_snow_cover",ftos(rsp_winter_snow));cvar_set("ryn_ice_amount",ftos(rsp_winter_ice));cvar_set("ryn_snowfall",ftos(snowfall));
}
void RSPWinterCapture(vector pos,vector normal,float surface){
 for(float i=0;i<rsp_winter_faces;i+=1)if(rsp_winter_face_id[i]==surface)return;
 float triangles=getsurfacenumtriangles(world,surface),count=triangles*3;
 if(count<3 || count>768 || rsp_winter_faces>=256 || rsp_winter_vertices+count>=4096)return;
 float face=rsp_winter_faces;rsp_winter_faces+=1;rsp_winter_face_id[face]=surface;rsp_winter_face_start[face]=rsp_winter_vertices;rsp_winter_face_count[face]=count;
 for(float triangle=0;triangle<triangles;triangle+=1){
  vector indices=getsurfacetriangle(world,surface,triangle);
  for(float point=0;point<3;point+=1){rsp_winter_points[rsp_winter_vertices]=getsurfacepoint(world,surface,indices[point])+'0 0 .65';rsp_winter_vertices+=1;}
 }
 rsp_winter_centers[face]=pos;
}
void RSPWinterScan(){
 if(rsp_winter_scanned || (rsp_winter_cold<.001 && rsp_winter_snow<.001))return;
 // Enumerate exact BSP surface triangles incrementally, including faces not hit by rain probes.
 for(float work=0;work<16;work+=1){
  if(rsp_winter_scan>=8192 || rsp_winter_empty>=32){rsp_winter_scanned=1;return;}
  float surface=rsp_winter_scan;rsp_winter_scan+=1;float count=getsurfacenumpoints(world,surface);
  if(count<3){rsp_winter_empty+=1;continue;}rsp_winter_empty=0;
  vector normal=getsurfacenormal(world,surface);string texture=getsurfacetexture(world,surface);
  if(normal_z<.35 || count>768 || substring(texture,0,1)=="*" || RSPIsSky(texture))continue;
  vector center='0 0 0';float highest=-99999;
  for(float j=0;j<count;j+=1){vector point=getsurfacepoint(world,surface,j);center+=point;highest=max(highest,point_z);}
  center=center/count;
  if(cvar("ryn_winter_water_scene") && highest<=cvar("rsp_water_level")+1)continue;
  RSPTrace(center+normal*2,center+normal*2+'0 0 4096',1,world);
  float exposed=trace_fraction>=1 || RSPIsSky(trace_surfacename) || RSPIsSky(trace_dphittexturename);
  for(float sample=0;sample<4 && !exposed;sample+=1){
   vector point=getsurfacepoint(world,surface,floor(sample*count/4));point=point*.95+center*.05+normal*2;
   RSPTrace(point,point+'0 0 4096',1,world);
   exposed=trace_fraction>=1 || RSPIsSky(trace_surfacename) || RSPIsSky(trace_dphittexturename);
  }
  if(!exposed)continue;
  RSPWinterCapture(center,normal,surface);
 }
}
void RSPWinterDraw(vector eye){
 if(rsp_winter_snow>.001){
  for(float face=0;face<rsp_winter_faces;face+=1){
   if(vlen(rsp_winter_centers[face]-eye)>1800)continue;
   for(float triangle=0;triangle<rsp_winter_face_count[face];triangle+=3){
    R_BeginPolygon("textures/reyondpack/snow",DRAWFLAG_TWOSIDED,0);
    for(float j=0;j<3;j+=1){vector p=rsp_winter_points[rsp_winter_face_start[face]+triangle+j];R_PolygonVertex(p,[p_x/96,p_y/96,0],'1 1 1',rsp_winter_snow);}R_EndPolygon();
   }
  }
 }
 if(rsp_winter_ice<.02)return;
 for(float cell=0;cell<49;cell+=1){
  if(!rsp_weather_open[cell] || !rsp_weather_water[cell])continue;
  vector center=rsp_weather_positions[cell];
  if(vlen(center-eye)>850 || RSPFloor(center)>cvar("rsp_water_level")-42)continue;
  float seed=sin(center_x*.017+center_y*.029)*231.7;
  if(RSPFract(seed*93.1)<.3)continue;
  center_x+=sin(seed*16)*18;center_y+=cos(seed*37)*18;
  float radius=(22+RSPFract(seed)*14)*sqrt(rsp_winter_ice),thickness=6*rsp_winter_ice;
  if(RSPFloor(center+[radius,0,0])>cvar("rsp_water_level")-24 || RSPFloor(center-[radius,0,0])>cvar("rsp_water_level")-24 || RSPFloor(center+[0,radius,0])>cvar("rsp_water_level")-24 || RSPFloor(center-[0,radius,0])>cvar("rsp_water_level")-24)continue;
  center_z=RSPHeight(center,gettime(0))+3;
  float slopeX=bound(-.15,(RSPHeight(center+'20 0 0',gettime(0))-RSPHeight(center-'20 0 0',gettime(0)))/40,.15);
  float slopeY=bound(-.15,(RSPHeight(center+'0 20 0',gettime(0))-RSPHeight(center-'0 20 0',gettime(0)))/40,.15);
  vector points[8];
  for(float j=0;j<8;j+=1){float angle=j*.7853981634+RSPFract(seed)*6.28;float size=radius*(.85+.15*sin(seed+j*9.3));vector offset=[cos(angle)*size,sin(angle)*size,0];offset_z=offset_x*slopeX+offset_y*slopeY;points[j]=center+offset;}
  R_BeginPolygon("textures/reyondpack/ice",DRAWFLAG_TWOSIDED,0);
  for(float j=0;j<8;j+=1)R_PolygonVertex(points[j],[j/8,0,0],'1 1 1',rsp_winter_ice);R_EndPolygon();
  for(float j=0;j<8;j+=1){float next=j+1;if(next==8)next=0;vector down=[0,0,thickness];
   R_BeginPolygon("textures/reyondpack/ice",DRAWFLAG_TWOSIDED,0);
   R_PolygonVertex(points[j], '0 0 0','.55 .72 .84',rsp_winter_ice);R_PolygonVertex(points[next],'1 0 0','.55 .72 .84',rsp_winter_ice);
   R_PolygonVertex(points[next]-down,'1 1 0','.3 .48 .62',rsp_winter_ice);R_PolygonVertex(points[j]-down,'0 1 0','.3 .48 .62',rsp_winter_ice);R_EndPolygon();
  }
 }
}


float RSPWeatherApproach(float value,float target,float step){
 if(value<target)return min(target,value+step);return max(target,value-step);
}
float RSPWeatherInterval(){
 float interval=max(15,max(cvar("ryn_weather_transition")+5,cvar("ryn_weather_interval")));
 if(interval!=cvar("ryn_weather_interval"))cvar_set("ryn_weather_interval",ftos(interval));
 return interval;
}
void RSPWeatherInit(){
 cvar_set("ryn_fire_heat","0");
 cvar_set("ryn_winter_water_scene","0");
 rsp_weather_mode=bound(0,floor(cvar("ryn_weather_mode")+.5),2);
 rsp_weather_cloud=rsp_weather_mode>0;rsp_weather_rain=rsp_weather_mode==2;rsp_weather_wet=0;RSPsonUpdate(0);
 rsp_weather_interval=RSPWeatherInterval();rsp_weather_next=gettime(0)+rsp_weather_interval;rsp_weather_cycle=rsp_weather_mode;
 cvar_set("ryn_weather_cloud",ftos(rsp_weather_cloud));cvar_set("ryn_weather_rain",ftos(rsp_weather_rain));cvar_set("ryn_weather_wetness","0");
}
void RSPWeatherUpdate(float now,float dt){
 float target=bound(0,floor(cvar("ryn_weather_mode")+.5),2);
 float interval=RSPWeatherInterval();
 if(interval!=rsp_weather_interval){rsp_weather_interval=interval;rsp_weather_next=now+interval;}
 if(target!=rsp_weather_mode){rsp_weather_next=now+interval;rsp_weather_cycle=target;rsp_weather_mode=target;}
 if(!cvar("ryn_weather_auto"))rsp_weather_next=now+interval;
 else if(now>=rsp_weather_next){
  rsp_weather_cycle=rsp_weather_cycle+1;if(rsp_weather_cycle>3)rsp_weather_cycle=0;
  target=rsp_weather_cycle==3?1:rsp_weather_cycle;rsp_weather_mode=target;
  cvar_set("ryn_weather_mode",ftos(target));rsp_weather_next=now+interval;
 }
 float step=dt/max(.1,cvar("ryn_weather_transition"));
 rsp_weather_cloud=RSPWeatherApproach(rsp_weather_cloud,target>0,step);
 rsp_weather_rain=RSPWeatherApproach(rsp_weather_rain,target==2,step);
 RSPsonUpdate(dt);
 if(rsp_weather_rain*(1-rsp_winter_cold)>.001)rsp_weather_wet=min(1,rsp_weather_wet+dt*rsp_weather_rain*(1-rsp_winter_cold)/18.);
 else rsp_weather_wet=max(0,rsp_weather_wet-dt/45.);
 cvar_set("ryn_weather_cloud",ftos(rsp_weather_cloud));cvar_set("ryn_weather_rain",ftos(rsp_weather_rain));cvar_set("ryn_weather_wetness",ftos(rsp_weather_wet));
}
float RSPWeatherSky(vector pos){
 RSPTrace(pos+'0 0 3',pos+'0 0 4096',1,world);
 return trace_fraction>=1 || RSPIsSky(trace_surfacename) || RSPIsSky(trace_dphittexturename);
}
vector RSPRainTop(vector p){RSPTrace(p,p+'0 0 512',1,world);return trace_fraction<1?trace_endpos-'0 0 4':p+'0 0 512';}
void RSPWeatherCache(vector eye){
 // Four columns per frame only when their world-grid keys change.
 for(float work=0;work<4;work+=1){
  float i=rsp_weather_cursor;rsp_weather_cursor+=1;if(rsp_weather_cursor>=49)rsp_weather_cursor=0;
  float gx=floor(eye_x/112)+(i-7*floor(i/7))-3,gy=floor(eye_y/112)+floor(i/7)-3;
  if(rsp_weather_ready[i] && rsp_weather_keys_x[i]==gx && rsp_weather_keys_y[i]==gy)continue;
  rsp_weather_ready[i]=1;rsp_weather_keys_x[i]=gx;rsp_weather_keys_y[i]=gy;rsp_weather_open[i]=0;rsp_weather_flat[i]=0;rsp_weather_water[i]=0;
  for(float drop=0;drop<10;drop+=1)rsp_weather_drop_valid[i*10+drop]=0;
  vector pos=[gx*112+56,gy*112+56,eye_z];
  RSPTrace(RSPRainTop(pos),pos-'0 0 2048',1,world);
  if(trace_fraction>=1 || trace_startsolid)continue;
  vector floorpos=trace_endpos,normal=trace_plane_normal;float flat=normal_z>.99;
  float water=RSPContents(floorpos+'0 0 4')==CONTENT_WATER;rsp_weather_water[i]=water;
  pos=floorpos;if(water)pos_z=RSPHeight(pos,gettime(0));
  rsp_weather_positions[i]=pos;
  if(!RSPWeatherSky(pos))continue;
  rsp_weather_open[i]=1;

  // Cache each drop column above its own exposed surface, including roofs.
  for(float drop=0;drop<10;drop+=1){
   float index=i*10+drop;vector dropPos=pos+[sin(index*17.3)*54,cos(index*23.7)*54,0];
   vector top=dropPos;top_z=eye_z;top=RSPRainTop(top);vector bottom=top-'0 0 2560';
   RSPTrace(top,bottom,1,world);
   if(trace_fraction>=1 || trace_startsolid)continue;
   dropPos=trace_endpos;
   if(RSPContents(dropPos+'0 0 4')==CONTENT_WATER)dropPos_z=max(dropPos_z,RSPHeight(dropPos,gettime(0)));
   if(!RSPWeatherSky(dropPos))continue;
   rsp_weather_drop_positions[index]=dropPos;rsp_weather_drop_valid[index]=1;
  }
  if(!flat || water)continue;
  // Reject patches crossing a ledge, wall or non-flat piece of geometry.
  for(float corner=0;corner<4;corner+=1){
   vector delta=[corner<2?-44:44,(corner==0 || corner==2)?-44:44,0];
   RSPTrace(floorpos+delta+'0 0 12',floorpos+delta-'0 0 12',1,world);
   if(trace_fraction>=1 || trace_startsolid || trace_plane_normal_z<.99 || fabs(trace_endpos_z-floorpos_z)>.5)flat=0;
  }
  rsp_weather_flat[i]=flat;
 }
}
void RSPWeatherDraw(vector eye){
 if(rsp_weather_rain<.001 && rsp_weather_wet<.001 && rsp_winter_ice<.001 && rsp_winter_snow<.001)return;
 RSPWeatherCache(eye);RSPWinterScan();RSPWinterDraw(eye);makevectors(getviewprop(VF_ANGLES));vector right=v_right,up=v_up;
 for(float i=0;i<49;i+=1){
  if(!rsp_weather_open[i])continue;
  vector base=rsp_weather_positions[i];float distance=vlen(base-eye);
  if(distance>850)continue;
  if(rsp_weather_flat[i] && rsp_weather_wet>.02){
   float size=32+RSPFract(sin(base_x*.17+base_y*.31)*138.2)*10;
   vector dx=[size,0,0],dy=[0,size,0],center=base+'0 0 .35';
   R_BeginPolygon("textures/reyondpack/puddle",0,0);
   R_PolygonVertex(center-dx-dy,'0 0 0','1 1 1',rsp_weather_wet);R_PolygonVertex(center+dx-dy,'1 0 0','1 1 1',rsp_weather_wet);
   R_PolygonVertex(center+dx+dy,'1 1 0','1 1 1',rsp_weather_wet);R_PolygonVertex(center-dx+dy,'0 1 0','1 1 1',rsp_weather_wet);R_EndPolygon();
  }
  if(rsp_weather_rain<.001 || RSPContents(eye)==CONTENT_WATER)continue;
  for(float j=0;j<10;j+=1){
   float seed=i*10+j;if(!rsp_weather_drop_valid[seed])continue;
   vector pos=rsp_weather_drop_positions[seed]+[0,0,8+RSPFract(seed*.618034-gettime(0)*(1.15*(1-rsp_winter_cold)+.14*rsp_winter_cold))*480];
   float alpha=rsp_weather_rain*.55*bound(0,1-distance/950,1);vector width=right*.45,streak='-4 0 20';
   if(rsp_winter_cold>.01){
    pos_x+=sin(gettime(0)*.8+seed)*8;pos_y+=cos(gettime(0)*.6+seed*1.1)*8;
    float snowSize=.5+RSPFract(seed*.37);vector snowDX=right*snowSize,snowDY=up*snowSize;
    float opacity=alpha*rsp_winter_cold*1.8*RSPSmooth(4,20,vlen(pos-eye));
    R_BeginPolygon("reyondpack/snow",DRAWFLAG_TWOSIDED,0);
    R_PolygonVertex(pos-snowDX-snowDY,'0 1 0','.9 .95 1',opacity);R_PolygonVertex(pos+snowDX-snowDY,'1 1 0','.9 .95 1',opacity);
    R_PolygonVertex(pos+snowDX+snowDY,'1 0 0','.9 .95 1',opacity);R_PolygonVertex(pos-snowDX+snowDY,'0 0 0','.9 .95 1',opacity);R_EndPolygon();
   }
   if(rsp_winter_cold<.99){
    R_BeginPolygon("reyondpack/rain",DRAWFLAG_TWOSIDED,0);
    R_PolygonVertex(pos-width,'0 1 0','.65 .74 .83',alpha*(1-rsp_winter_cold));R_PolygonVertex(pos+width,'1 1 0','.65 .74 .83',alpha*(1-rsp_winter_cold));
    R_PolygonVertex(pos+width+streak,'1 0 0','.65 .74 .83',alpha*(1-rsp_winter_cold));R_PolygonVertex(pos-width+streak,'0 0 0','.65 .74 .83',alpha*(1-rsp_winter_cold));R_EndPolygon();
   }
  }
 }
}

var float rsp_clock=0,rsp_initialized=0;
float rsp_view[28];string rsp_previous_post;
void RSPFrame(){
 float now=gettime(0),dt=min(.25,max(0,now-rsp_clock));rsp_clock=now;
 if(!rsp_initialized){RSPWeatherInit();rsp_initialized=1;}
 if(cvar("ryn_day_animate")){float hour=cvar("ryn_time_hour")+dt*24/(max(1,cvar("ryn_day_minutes"))*60);cvar_set("ryn_time_hour",ftos(RSPMod(hour,24)));}
 RSPWeatherUpdate(now,dt);vector eye=getproperty(VF_ORIGIN);RSPWeatherDraw(eye);
 makevectors(getproperty(VF_ANGLES));vector fov=getproperty(VF_FOV);
 for(float j=0;j<3;j+=1){rsp_view[j]=eye[j];rsp_view[4+j]=v_forward[j];rsp_view[8+j]=v_right[j];rsp_view[12+j]=v_up[j];}
 rsp_view[7]=tan(fov_x*.0087266463);rsp_view[11]=tan(fov_y*.0087266463);
 RSPCelestialUpdate(now,eye);rsp_view[20]=rsp_celestial_sun_visible;rsp_view[21]=rsp_celestial_moon_visible;
 rsp_view[24]=getproperty(VF_MAXDIST);if(rsp_view[24]<1)rsp_view[24]=65536;
 setproperty(VF_USERDATA,&rsp_view,(unsigned int)sizeof(rsp_view));
 rsp_previous_post=strzone(cvar_string("r_postprocshader"));cvar_set("r_postprocshader","reyondpack/post");
}
void RSPFinish(){if(rsp_previous_post){cvar_set("r_postprocshader",rsp_previous_post);strunzone(rsp_previous_post);rsp_previous_post=__NULL__;}}

void RSPReset(){rsp_initialized=0;rsp_winter_faces=0;rsp_winter_vertices=0;rsp_winter_scan=0;rsp_winter_empty=0;rsp_winter_scanned=0;
 for(float i=0;i<49;i+=1)rsp_weather_ready[i]=0;}

void RSPStatus(){float columns=0;for(float i=0;i<49;i+=1)columns+=rsp_weather_open[i];print(sprintf("RSP_WEATHER snow=%g ice=%g surfaces=%g vertices=%g columns=%g\n",rsp_winter_snow,rsp_winter_ice,rsp_winter_faces,rsp_winter_vertices,columns));}
