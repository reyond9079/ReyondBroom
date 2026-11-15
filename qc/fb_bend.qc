// Copyright (C) 2026 Kristian Duske
// SPDX-License-Identifier: GPL-3.0-or-later
// Catmull-Rom bend path and continuous joints adapted from UltimateTrenchBroom.
typedef struct {fb_poly_t poly;int model,id,contents;} fb_bend_source_t;
typedef struct {vector points[33],scales[33],rotations[33];int segments,axis,subdivisions,joints;} fb_bend_state_t;
fb_bend_state_t fb_bend_state;
fb_bend_state_t *fb_bend_undo;
fb_bend_state_t *fb_bend_redo;int fb_bend_redo_count;
fb_bend_source_t *fb_bend_sources;
int fb_bend_count,fb_bend_undo_count,fb_bend_valid,fb_bend_history,fb_bend_epoch,fb_bend_selection;
int fb_bend_handle,fb_bend_drag;
vector fb_bend_low,fb_bend_high,fb_bend_pixel,fb_bend_before_point,fb_bend_before_scale,fb_bend_before_rotation;
float fb_bend_vertical_start,fb_bend_lasttime;vector fb_bend_lastpixel;
void fb_bend_publish()
{
    int h=bound(0,fb_bend_handle,fb_bend_state.segments);fb_bend_handle=h;
    cvar_set("ftebroom_bend_handle",itos(h));cvar_set("ftebroom_bend_segments",itos(fb_bend_state.segments));cvar_set("ftebroom_bend_axis",itos(fb_bend_state.axis));
    cvar_set("ftebroom_bend_subdivisions",itos(fb_bend_state.subdivisions));cvar_set("ftebroom_bend_joints",itos(fb_bend_state.joints));
    cvar_set("ftebroom_bend_point",sprintf("%g %g %g",fb_bend_state.points[h][0],fb_bend_state.points[h][1],fb_bend_state.points[h][2]));cvar_set("ftebroom_bend_scale",sprintf("%g %g %g",fb_bend_state.scales[h][0],fb_bend_state.scales[h][1],fb_bend_state.scales[h][2]));cvar_set("ftebroom_bend_rotation",sprintf("%g %g %g",fb_bend_state.rotations[h][0],fb_bend_state.rotations[h][1],fb_bend_state.rotations[h][2]));
    cvar_set("ftebroom_bend_valid",itos(fb_bend_valid));
}
void fb_bend_clear()
{
    if(fb_bend_count)fb_generator_clear();
    for(int b=0;b<fb_bend_count;b++)fb_poly_free(&fb_bend_sources[b].poly);
    memfree(fb_bend_sources);memfree(fb_bend_undo);memfree(fb_bend_redo);fb_bend_redo=__NULL__;fb_bend_redo_count=0;fb_bend_sources=__NULL__;fb_bend_undo=__NULL__;fb_bend_count=fb_bend_valid=fb_bend_drag=fb_bend_undo_count=0;
    cvar_set("ftebroom_bend_valid","0");
}
void fb_bend_save()
{
    fb_bend_redo_count=0;
    if(!fb_bend_undo)fb_bend_undo=memalloc(128*sizeof(fb_bend_state_t));
    if(fb_bend_undo_count==128){for(int i=1;i<128;i++)fb_bend_undo[i-1]=fb_bend_undo[i];fb_bend_undo_count--;}
    fb_bend_undo[fb_bend_undo_count++]=fb_bend_state;
}
void fb_bend_reset_points()
{
    vector start=(fb_bend_low+fb_bend_high)*.5;int axis=fb_bend_state.axis;start[axis]=fb_bend_low[axis];
    for(int i=0;i<=fb_bend_state.segments;i++){fb_bend_state.points[i]=start;fb_bend_state.points[i][axis]+=(fb_bend_high[axis]-fb_bend_low[axis])*i/(float)fb_bend_state.segments;fb_bend_state.scales[i]='1 1 1';fb_bend_state.rotations[i]='0 0 0';}
}
vector fb_bend_curve(float t,int tangent)
{
    float scaled=bound(0,t,1)*fb_bend_state.segments;int index=min(fb_bend_state.segments-1,floor(scaled));float u=scaled-index;
    vector p0=fb_bend_state.points[max(0,index-1)],p1=fb_bend_state.points[index],p2=fb_bend_state.points[index+1],p3=fb_bend_state.points[min(fb_bend_state.segments,index+2)];
    if(tangent)return ((p2-p0)+(p0*4-p1*10+p2*8-p3*2)*u+(-p0*3+p1*9-p2*9+p3*3)*u*u)*.5;
    return (p1*2+(p2-p0)*u+(p0*2-p1*5+p2*4-p3)*u*u+(-p0+p1*3-p2*3+p3)*u*u*u)*.5;
}
vector fb_bend_point(vector point,vector reference)
{
    int axis=fb_bend_state.axis;float extent=fb_bend_high[axis]-fb_bend_low[axis];
    float t=bound(0,(reference[axis]-fb_bend_low[axis])/extent,1),scaled=t*fb_bend_state.segments;
    int index=min(fb_bend_state.segments-1,floor(scaled));float u=scaled-index;
    vector base=(fb_bend_low+fb_bend_high)*.5;base[axis]=reference[axis];
    vector offset=point-base,scale=fb_bend_state.scales[index]*(1-u)+fb_bend_state.scales[index+1]*u,rotation=fb_bend_state.rotations[index]*(1-u)+fb_bend_state.rotations[index+1]*u;
    for(int a=0;a<3;a++)offset[a]*=scale[a];
    offset=fb_affine_rotate_axis(offset,'1 0 0',rotation_x*M_PI/180);offset=fb_affine_rotate_axis(offset,'0 1 0',rotation_y*M_PI/180);offset=fb_affine_rotate_axis(offset,'0 0 1',rotation_z*M_PI/180);
    vector tangent=fb_bend_curve(t,TRUE),direction='0 0 0';direction[axis]=1;
    if(vlen(tangent)>.000001){tangent=normalize(tangent);float dot=bound(-1,direction*tangent,1);vector turn=crossproduct(direction,tangent);if(vlen(turn)<.000001){turn='0 0 0';turn[(axis+1)%3]=1;}offset=fb_affine_rotate_axis(offset,normalize(turn),acos(dot));}
    return fb_bend_curve(t,FALSE)+offset;
}
int fb_bend_build()
{
    fb_generator_clear();fb_bend_valid=FALSE;cvar_set("ftebroom_bend_valid","0");
    if(!fb_bend_count)return FALSE;
    if(history!=fb_bend_history||fb_selection_epoch!=fb_bend_selection||fb_grid_geometry_epoch!=fb_bend_epoch){fb_geom_error="Selection or document changed; restart Bezier Bend";return FALSE;}
    int slices=1i<<bound(0,fb_bend_state.subdivisions,6),axis=fb_bend_state.axis;
    if(fb_bend_count*slices>512){fb_geom_error="Bend exceeds 512 pieces; reduce subdivisions";return FALSE;}
    fb_generator_items=memalloc(fb_bend_count*slices*sizeof(fb_generator_preview_t));int valid=TRUE;
    brushface_t *attrs=memalloc(64*sizeof(brushface_t));
    for(int b=0;b<fb_bend_count&&valid;b++) {
        fb_poly_t *source=&fb_bend_sources[b].poly;float low=1000000,high=-1000000;
        for(int v=0;v<source->nv;v++){low=min(low,source->vertices[v][axis]);high=max(high,source->vertices[v][axis]);}
        for(int f=0;f<source->nf;f++)attrs[f]=source->faces[f].attr;
        for(int s=0;s<slices&&valid;s++) {
            fb_poly_t piece={0};valid=fb_csg_copy(source,&piece);if(!valid)break;
            for(int side=0;side<2&&valid;side++) {
                if((side==0&&s==0)||(side==1&&s==slices-1))continue;
                brushface_t cut=attrs[0];cut.planenormal='0 0 0';cut.planenormal[axis]=1;cut.planedist=low+(high-low)*(s+side)/(float)slices;
                fb_poly_t front={0},back={0};valid=fb_csg_split(&piece,cut,&front,&back);fb_poly_free(&piece);
                if(side==0){piece=front;memfill8((__variant*)&front,0,sizeof(front));}else{piece=back;memfill8((__variant*)&back,0,sizeof(back));}
                fb_poly_free(&front);fb_poly_free(&back);
            }
            if(valid&&piece.nv>=4) {
                vector center='0 0 0';for(int v=0;v<piece.nv;v++)center+=piece.vertices[v];center/=(float)piece.nv;
                vector *points=memalloc(piece.nv*sizeof(vector));for(int v=0;v<piece.nv;v++)points[v]=fb_bend_point(piece.vertices[v],fb_bend_state.joints?piece.vertices[v]:center);
                fb_poly_t *out=&fb_generator_items[fb_generator_count].poly;valid=fb_hull(points,piece.nv,out);memfree(points);
                if(valid){
                    if(!fb_bend_state.joints){
                        fb_affine_t transform={0};transform.translation=fb_bend_point('0 0 0',center);
                        transform.x=fb_bend_point('1 0 0',center)-transform.translation;transform.y=fb_bend_point('0 1 0',center)-transform.translation;transform.z=fb_bend_point('0 0 1',center)-transform.translation;
                        fb_affine_prepare(&transform);brushface_t *transformed=memalloc(source->nf*sizeof(brushface_t));for(int f=0;f<source->nf;f++){transformed[f]=attrs[f];fb_affine_face(&transform,&transformed[f],cvar("ftebroom_texture_lock")!=0);}fb_land_attributes(out,transformed,source->nf);memfree(transformed);
                    }else fb_land_attributes(out,attrs,source->nf);
                    fb_generator_count++;
                }
            }else if(valid){valid=FALSE;fb_geom_error="Bend produced an empty slice";}
            fb_poly_free(&piece);
        }
    }
    memfree(attrs);
    if(!valid){fb_generator_clear();fb_bend_publish();return FALSE;}
    fb_generator_original_count=fb_bend_count;fb_generator_original=memalloc(fb_bend_count*sizeof(selbrush_t));
    for(int b=0;b<fb_bend_count;b++){fb_generator_original[b].model=fb_bend_sources[b].model;fb_generator_original[b].id=fb_bend_sources[b].id;}
    fb_generator_hide_original=TRUE;fb_generator_history=history;fb_generator_selection=fb_selection_epoch;fb_generator_geometry=fb_grid_geometry_epoch;
    fb_bend_valid=TRUE;cvar_set("ftebroom_generator_valid","1");fb_bend_publish();return TRUE;
}
int fb_bend_capture()
{
    fb_bend_clear();if(selectedbrushcount<1||selectedbrushcount>64){fb_geom_error="Select 1..64 brushes for Bezier Bend";return FALSE;}
    fb_bend_count=selectedbrushcount;fb_bend_sources=memalloc(fb_bend_count*sizeof(fb_bend_source_t));fb_bend_low='1000000 1000000 1000000';fb_bend_high=-fb_bend_low;
    for(int b=0;b<fb_bend_count;b++) {
        fb_bend_source_t *item=&fb_bend_sources[b];item->model=selectedbrushes[b].model;item->id=selectedbrushes[b].id;
        if(!fb_poly_from_brush(item->model,item->id,&item->poly)){fb_bend_clear();return FALSE;}
        brush_get(item->model,item->id,__NULL__,0,&item->contents);
        for(int v=0;v<item->poly.nv;v++)for(int a=0;a<3;a++){fb_bend_low[a]=min(fb_bend_low[a],item->poly.vertices[v][a]);fb_bend_high[a]=max(fb_bend_high[a],item->poly.vertices[v][a]);}
    }
    fb_bend_state.axis=0;fb_bend_state.segments=8;fb_bend_state.subdivisions=0;fb_bend_state.joints=TRUE;fb_bend_handle=0;fb_bend_reset_points();
    fb_bend_history=history;fb_bend_selection=fb_selection_epoch;fb_bend_epoch=fb_grid_geometry_epoch;return fb_bend_build();
}
int fb_bend_apply()
{
    if(!fb_bend_build())return FALSE;
    int slices=1i<<fb_bend_state.subdivisions,valid=TRUE;brushface_t *faces=memalloc(64*sizeof(brushface_t));
    fb_tx_begin("Bezier Bend");brush_deselectall();
    for(int i=0;i<fb_generator_count&&valid;i++){fb_poly_t *piece=&fb_generator_items[i].poly;fb_bend_source_t *source=&fb_bend_sources[i/slices];for(int f=0;f<piece->nf;f++)faces[f]=piece->faces[f].attr;valid=brush_history_create(source->model,faces,piece->nf,source->contents,TRUE)!=0;}
    if(valid)for(int b=0;b<fb_bend_count;b++)brush_history_delete(fb_bend_sources[b].model,fb_bend_sources[b].id);
    memfree(faces);if(valid)valid=fb_tx_commit();else fb_tx_cancel();fb_bend_clear();fb_document_tick();cvar_set("ftebroom_tool","0");return valid;
}

void fb_bend_cancel_drag()
{
    if(!fb_bend_drag)return;
    fb_bend_drag=FALSE;fb_active=FALSE;
    if(fb_bend_undo_count>0)fb_bend_state=fb_bend_undo[--fb_bend_undo_count];fb_bend_build();
}
void fb_bend_down(vector pixel,int alt,int view)
{
    if(!fb_bend_count)return;int closest=-1;float distance=14;
    makevectors(getproperty(VF_ANGLES));vector forward=v_forward;
    for(int i=0;i<=fb_bend_state.segments;i++){if(!view&&(fb_bend_state.points[i]-getproperty(VF_ORIGIN))*forward<=0)continue;vector screen=project(fb_bend_state.points[i]);screen_z=0;float d=vlen(screen-pixel);if(d<distance){distance=d;closest=i;}}
    if(closest<0)return;
    fb_bend_lastpixel=pixel;fb_bend_lasttime=time;
    fb_bend_handle=closest;fb_bend_save();fb_bend_drag=TRUE;fb_active=TRUE;fb_bend_pixel=pixel;
    fb_bend_before_point=fb_bend_state.points[closest];fb_bend_before_scale=fb_bend_state.scales[closest];fb_bend_before_rotation=fb_bend_state.rotations[closest];
    vector ray=normalize(unproject(pixel+'0 0 8192')-unproject(pixel)),viewport=getproperty(VF_SIZE);
    fb_vertical_fallback=1-ray_z*ray_z<.01;fb_vertical_scale=2*max(1,vlen(fb_bend_before_point-getproperty(VF_ORIGIN)))*tan(getproperty(VF_FOVY)*M_PI/360)/max(1,viewport_y);
    fb_bend_vertical_start=fb_vertical_drag(pixel,fb_bend_before_point);fb_bend_publish();
}
void fb_bend_move(vector pixel,int alt,int view)
{
    if(!fb_bend_drag)return;
    if(vlen(pixel-fb_bend_lastpixel)<.01)return;fb_bend_lasttime=time;fb_bend_lastpixel=pixel;
    int mode=bound(0,cvar("ftebroom_bend_mode"),2),component=bound(0,cvar("ftebroom_bend_component"),3),h=fb_bend_handle;
    if(mode==0){
        vector delta=fb_view_plane_point(pixel,view,fb_bend_before_point)-fb_view_plane_point(fb_bend_pixel,view,fb_bend_before_point);
        if(alt&&!view)delta=[0,0,fb_vertical_drag(pixel,fb_bend_before_point)-fb_bend_vertical_start];
        if(component)for(int a=0;a<3;a++)if(a!=component-1)delta[a]=0;
        fb_bend_state.points[h]=fb_bend_before_point+delta;if(cvar("ftebroom_ctrl"))fb_bend_state.points[h]=fb_snap(fb_bend_state.points[h]);
    }else if(mode==1){float factor=pow(2,(fb_bend_pixel_y-pixel_y)/100);for(int a=0;a<3;a++)fb_bend_state.scales[h][a]=bound(.01,fb_bend_before_scale[a]*((!component||a==component-1)?factor:1),100);}
    else {int axis=component?component-1:fb_bend_state.axis;fb_bend_state.rotations[h]=fb_bend_before_rotation;fb_bend_state.rotations[h][axis]+=rint(pixel_x-fb_bend_pixel_x);}
    fb_geom_error="";fb_bend_build();cvar_set("ftebroom_selection_status",fb_geom_error);
}
void fb_bend_up(vector pixel,int alt,int view){fb_bend_lasttime=-100;fb_bend_move(pixel,alt,view);fb_bend_drag=FALSE;fb_active=FALSE;}
void fb_bend_draw()
{
    if(cvar("ftebroom_tool")!=FB_TOOL_BEND||!fb_bend_count)return;
    if(history!=fb_bend_history||fb_selection_epoch!=fb_bend_selection||fb_grid_geometry_epoch!=fb_bend_epoch){fb_bend_clear();return;}
    for(int i=0;i<64;i++)fb_visual_line(fb_bend_curve((float)i/64,FALSE),fb_bend_curve((float)(i+1)/64,FALSE),'1 .2 .8',1,1,TRUE);
    vector size=getproperty(VF_SIZE);float units=1;if(cvar("ca_brush_view"))units=vlen(unproject('1 0 0')-unproject('0 0 0'))*4;
    for(int i=0;i<=fb_bend_state.segments;i++) {
        vector point=fb_bend_state.points[i];float radius=units;
        if(!cvar("ca_brush_view"))radius=max(.25,vlen(point-getproperty(VF_ORIGIN))*tan(getproperty(VF_FOVY)*M_PI/360)*8/max(1,size_y));
        vector extent=[radius,radius,radius];fb_visual_box(point-extent,point+extent,i==fb_bend_handle?'1 1 .2':'1 .2 .8',1,FALSE);
    }
}
int fb_bend_command()
{
    if(cvar("ftebroom_tool")!=FB_TOOL_BEND)return FALSE;
    string cmd=argv(0),op=argv(1);
    if(cmd=="ftebroom_undo") {
        if(fb_bend_drag){fb_bend_cancel_drag();return TRUE;}
        if(fb_bend_undo_count){if(!fb_bend_redo)fb_bend_redo=memalloc(128*sizeof(fb_bend_state_t));fb_bend_redo[fb_bend_redo_count++]=fb_bend_state;fb_bend_state=fb_bend_undo[--fb_bend_undo_count];fb_bend_build();}return TRUE;
    }
    if(cmd=="ftebroom_redo") {if(fb_bend_drag)return TRUE;if(fb_bend_redo_count){fb_bend_undo[fb_bend_undo_count++]=fb_bend_state;fb_bend_state=fb_bend_redo[--fb_bend_redo_count];fb_bend_build();}return TRUE;}
    if(cmd!="ftebroom_bend")return FALSE;
    if(op=="apply"){if(!fb_bend_apply())cvar_set("ftebroom_selection_status",fb_geom_error);return TRUE;}
    if(op=="cancel"){fb_tool_deactivate();return TRUE;}
    if(op=="handle"){fb_bend_handle=bound(0,stoi(argv(2)),fb_bend_state.segments);fb_bend_publish();return TRUE;}
    fb_bend_save();
    if(op=="reset")fb_bend_reset_points();
    else if(op=="settings") {int axis=bound(0,stoi(argv(2)),2),segments=bound(1,stoi(argv(3)),32);int reset=axis!=fb_bend_state.axis||segments!=fb_bend_state.segments;fb_bend_state.axis=axis;fb_bend_state.segments=segments;fb_bend_state.subdivisions=bound(0,stoi(argv(4)),6);fb_bend_state.joints=stoi(argv(5))!=0;if(reset)fb_bend_reset_points();}
    else {vector value=[stof(argv(2)),stof(argv(3)),stof(argv(4))];int h=bound(0,fb_bend_handle,fb_bend_state.segments);
        if(op=="point"&&fb_geom_point(value))fb_bend_state.points[h]=value;
        if(op=="scale"){for(int a=0;a<3;a++)value[a]=bound(.01,value[a],100);fb_bend_state.scales[h]=value;}
        if(op=="rotation"&&fb_geom_point(value))fb_bend_state.rotations[h]=value;
    }
    fb_geom_error="";fb_bend_build();cvar_set("ftebroom_selection_status",fb_geom_error);return TRUE;
}
