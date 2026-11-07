// SPDX-License-Identifier: GPL-3.0-or-later
void() editor_dynmesh_draw = #0:editor_dynmesh_draw;
vector(float index,float mode) editor_dynmesh_vertex = #0:editor_dynmesh_vertex;
void fb_modeleditor_guides(vector eye);
void fb_bb_render()
{
 vector low=stov(cvar_string("ftebroom_bb_low")),high=stov(cvar_string("ftebroom_bb_high"));
 vector angles=[cvar("ftebroom_modelviewer_pitch"),cvar("ftebroom_modelviewer_yaw"),0];
 makevectors(angles);vector eye=(low+high)*.5-v_forward*max(24,vlen(high-low)*1.4)*max(.1,cvar("ftebroom_modelviewer_zoom"));
 if(cvar("ftebroom_modeleditor_player")){eye='0 0 0';angles='0 0 0';}
 else if(cvar("ftebroom_modeleditor_freecamera"))eye=stov(cvar_string("ftebroom_modeleditor_camera"));
 cvar_set("ftebroom_modeleditor_eye",vtos(eye));cvar_set("ftebroom_modeleditor_angles",vtos(angles));
 vector size=[max(64,cvar("ftebroom_modelviewer_width")),max(64,cvar("ftebroom_modelviewer_height")),0];
 vector oldOrigin=getproperty(VF_ORIGIN),oldAngles=getproperty(VF_ANGLES),oldMin=getproperty(VF_MIN),oldSize=getproperty(VF_SIZE),oldFov=getproperty(VF_FOV);
 float oldPerspective=getproperty(VF_PERSPECTIVE),oldNear=getproperty(VF_MINDIST),oldFar=getproperty(VF_MAXDIST);
 clearscene();setproperty(VF_PERSPECTIVE,!cvar("ftebroom_bb_ortho"));setproperty(VF_MINDIST,.1);setproperty(VF_MAXDIST,0);setproperty(VF_RT_DESTCOLOUR,"ftebroom_modelviewer",-IMGFMT_R8G8B8A8,size);
 setproperty(VF_MIN,'0 0 0');setproperty(VF_SIZE,size);setproperty(VF_ORIGIN,eye);setproperty(VF_ANGLES,angles);setproperty(VF_DRAWWORLD,0);
 if(cvar("ftebroom_bb_ortho")){setproperty(VF_FOVX,max(16,vlen(high-low))*max(.1,cvar("ftebroom_modelviewer_zoom")));setproperty(VF_FOVY,max(16,vlen(high-low))*max(.1,cvar("ftebroom_modelviewer_zoom"))*size_y/size_x);}
 else{setproperty(VF_FOVY,45);setproperty(VF_FOVX,atan(tan(45*M_PI/360)*size_x/size_y)*360/M_PI);}
 if(cvar("ftebroom_modeleditor_player")){setproperty(VF_FOVX,90);setproperty(VF_FOVY,atan(size_y/size_x)*360/M_PI);}
 vector background='0.08 0.09 0.11';if(cvar("ftebroom_plant_active"))background=stov(cvar_string("ftebroom_plant_background"));
 drawfill('0 0 0',size,background,1,0);
 fb_modeleditor_guides(eye);
 if(cvar("ftebroom_bb_benchmark_b")){
  int vertices=cvar("ftebroom_bb_benchmark_vertices");
  string reference="ftebroom/bb-reference-white";shaderforname(reference,"{\ncull none\nprogram default2d\nsort opaque\n{\nmap $whiteimage\ndepthwrite\nalphafunc GE128\nrgbgen vertex\nalphagen vertex\n}\n}");
  for(int i=0;i<vertices;i+=3){R_BeginPolygon(reference,3);for(int j=0;j<3;j++){vector position=editor_dynmesh_vertex(i+j,0);vector uv=editor_dynmesh_vertex(i+j,1);vector colour=editor_dynmesh_vertex(i+j,2);R_PolygonVertex(position,uv,colour,1);}R_EndPolygon();}
 }else editor_dynmesh_draw();
 renderscene();setproperty(VF_RT_DESTCOLOUR,"");setproperty(VF_ORIGIN,oldOrigin);setproperty(VF_ANGLES,oldAngles);setproperty(VF_MIN,oldMin);setproperty(VF_SIZE,oldSize);setproperty(VF_FOV,oldFov);setproperty(VF_DRAWWORLD,1);setproperty(VF_PERSPECTIVE,oldPerspective);setproperty(VF_MINDIST,oldNear);setproperty(VF_MAXDIST,oldFar);
}
