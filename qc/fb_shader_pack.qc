// SPDX-License-Identifier: GPL-2.0-or-later
#define RSP_EDITOR
void RSPTrace(vector start,vector end,float nomonsters,entity ignore);
float RSPContents(vector point);
#include "../assets/reyond-shader-pack/sdk/reyond-pack.qc"
int *fb_pack_candidates;int fb_pack_capacity;
int fb_pack_candidates_for(vector start,vector end){
 if(fb_pack_capacity<fb_node_count){memfree(fb_pack_candidates);fb_pack_capacity=fb_node_capacity;fb_pack_candidates=memalloc(fb_pack_capacity*sizeof(int));}
 return fb_incremental_pick(start,end,fb_pack_candidates,fb_pack_capacity);
}
// Separate clip queries never mutate the editor's hover/pick results.
void RSPTrace(vector start,vector end,float nomonsters,entity ignore){
 trace_fraction=1;trace_endpos=end;trace_plane_normal='0 0 0';trace_startsolid=0;trace_surfacename="";trace_dphittexturename="";
 vector delta=end-start;float length=vlen(delta);if(length<.0001)return;vector direction=delta/length;
 int candidates=fb_pack_candidates_for(start,end);static vector bounds[2];static brushface_t faces[64];
 for(int entry=0;entry<(candidates<0?fb_node_count:candidates);entry++){
  int node=candidates<0?entry:fb_pack_candidates[entry];if(node<2||fb_nodes[node].kind!=FB_NODE_BRUSH)continue;
  fb_node_t item=fb_nodes[node];if(brush_getfacepoints(item.model,item.id,0,bounds,2)!=2 || fb_pick_box(start,direction,bounds[0],bounds[1],length)<0)continue;
  int contents,count=brush_get(item.model,item.id,faces,64,&contents);if(count<4||count>64)continue;
  int flags=fb_filter_face_tags(&faces[0],contents);if(flags&(FB_TAG_LIQUID|FB_TAG_TRIGGER|FB_TAG_TRANSLUCENT))continue;
  float enter=-1,leave=length;int front=-1,back=-1,valid=TRUE;
  for(int f=0;f<count;f++){
   float distance=faces[f].planedist-start*faces[f].planenormal,denominator=direction*faces[f].planenormal;
   if(fabs(denominator)<.000001){if(distance<0){valid=FALSE;break;}}
   else {float t=distance/denominator;if(denominator<0){if(t>enter){enter=t;front=f;}}else if(t<leave){leave=t;back=f;}if(enter>leave){valid=FALSE;break;}}
  }
  int inside=enter<0;if(inside){enter=leave;front=back;}
  if(valid&&front>=0&&enter>=0&&enter<length*trace_fraction){trace_fraction=enter/length;trace_endpos=start+direction*enter;trace_plane_normal=faces[front].planenormal;trace_surfacename=faces[front].shadername;trace_dphittexturename=trace_surfacename;trace_startsolid=inside;}
 }
}
float RSPContents(vector point){
 int candidates=fb_pack_candidates_for(point,point+'0 0 .01');static vector bounds[2];static brushface_t faces[64];
 for(int entry=0;entry<(candidates<0?fb_node_count:candidates);entry++){
  int node=candidates<0?entry:fb_pack_candidates[entry];if(node<2||fb_nodes[node].kind!=FB_NODE_BRUSH)continue;
  fb_node_t item=fb_nodes[node];if(brush_getfacepoints(item.model,item.id,0,bounds,2)!=2)continue;
  if(point_x<bounds[0][0]||point_y<bounds[0][1]||point_z<bounds[0][2]||point_x>bounds[1][0]||point_y>bounds[1][1]||point_z>bounds[1][2])continue;
  int contents,count=brush_get(item.model,item.id,faces,64,&contents);if(count<4||count>64)continue;
  if(!(fb_filter_face_tags(&faces[0],contents)&FB_TAG_LIQUID))continue;
  int inside=TRUE;for(int f=0;f<count;f++)if(point*faces[f].planenormal>faces[f].planedist+.01){inside=FALSE;break;}
  if(inside)return CONTENT_WATER;
 }
 return CONTENT_EMPTY;
}
int fb_pack_snow_cursor;
void fb_pack_snow_scan(vector eye){
 if(cvar("ryn_winter_cold")<.001 && cvar("ryn_snow_cover")<.001)return;
 // Editable MAP brushes have no compiled BSP surface array. Capture convex face fans.
 rsp_winter_scanned=1;
 static brushface_t faces[64];static vector points[64];
 for(int work=0;work<16 && fb_pack_snow_cursor<fb_node_count;work++){
  int node=fb_pack_snow_cursor++;
  if(node<2 || fb_nodes[node].kind!=FB_NODE_BRUSH)continue;
  int contents,count=brush_get(fb_nodes[node].model,fb_nodes[node].id,faces,64,&contents);
  for(int f=0;f<min(count,64);f++){
   vector normal=faces[f].planenormal;
   if(normal_z<.35 || RSPIsSky(faces[f].shadername))continue;
   if(fb_filter_face_tags(&faces[f],contents)&(FB_TAG_LIQUID|FB_TAG_TRANSLUCENT|FB_TAG_TRIGGER|FB_TAG_CLIP|FB_TAG_CAULK|FB_TAG_HINT))continue;
   int vertices=brush_getfacepoints(fb_nodes[node].model,fb_nodes[node].id,f+1,points,64);
   if(vertices<3 || vertices>64 || rsp_winter_faces>=256 || rsp_winter_vertices+(vertices-2)*3>=4096)continue;
   vector center='0 0 0';for(int p=0;p<vertices;p++)center+=points[p];center=center/(float)vertices;
   if(vlen(center-eye)>1800)continue;
   if(RSPContents(center+'0 0 2')==CONTENT_WATER)continue;
   RSPTrace(center+'0 0 2',center+'0 0 4096',1,world);
   if(trace_fraction<1 && !RSPIsSky(trace_surfacename) && !RSPIsSky(trace_dphittexturename))continue;
   int at=rsp_winter_faces++;rsp_winter_face_start[at]=rsp_winter_vertices;rsp_winter_face_count[at]=(vertices-2)*3;rsp_winter_centers[at]=center;
   for(int t=1;t<vertices-1;t++){
    rsp_winter_points[rsp_winter_vertices++]=points[0]+'0 0 .65';
    rsp_winter_points[rsp_winter_vertices++]=points[t]+'0 0 .65';
    rsp_winter_points[rsp_winter_vertices++]=points[t+1]+'0 0 .65';
   }
  }
 }
}
void fb_shader_pack_frame(){
 if(cvar("ftebroom_game_profile")!=1 || !cvar("ftebroom_shader_pack_enabled") || cvar("ca_brush_view"))return;
 static int revision=-1;int current=fb_grid_geometry_epoch;
 if(current!=revision){revision=current;RSPReset();fb_pack_snow_cursor=2;}
 fb_pack_snow_scan(getproperty(VF_ORIGIN));
 RSPFrame();
}
void fb_shader_pack_finish(){RSPFinish();}
