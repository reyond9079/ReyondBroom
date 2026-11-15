// SPDX-License-Identifier: GPL-3.0-or-later
// Interactive endpoint editing owns preview geometry and a separate settings history.
typedef struct {vector delta,rotation;float scale,flex;int segments;} fb_spin_state_t;
fb_spin_state_t fb_spin_state,fb_spin_before;
fb_spin_state_t *fb_spin_undo;
fb_spin_state_t *fb_spin_redo;int fb_spin_undo_count,fb_spin_redo_count;
int fb_spin_captured,fb_spin_valid,fb_spin_drag,fb_spin_history,fb_spin_selection,fb_spin_epoch;
vector fb_spin_pixel,fb_spin_lastpixel;float fb_spin_lasttime,fb_spin_vertical_start;
void fb_spin_publish()
{
    cvar_set("ftebroom_spin_segments",itos(fb_spin_state.segments));cvar_set("ftebroom_spin_flex",ftos(fb_spin_state.flex));cvar_set("ftebroom_spin_scale",ftos(fb_spin_state.scale));cvar_set("ftebroom_spin_auto","0");
    for(int a=0;a<3;a++){cvar_set(a==0?"ftebroom_spin_x":a==1?"ftebroom_spin_y":"ftebroom_spin_z",ftos(fb_spin_state.delta[a]));cvar_set(a==0?"ftebroom_spin_rx":a==1?"ftebroom_spin_ry":"ftebroom_spin_rz",ftos(fb_spin_state.rotation[a]));}
    cvar_set("ftebroom_spin_valid",itos(fb_spin_valid));cvar_set("ftebroom_selection_status",fb_geom_error);
}
void fb_spin_clear()
{
    if(fb_spin_captured){fb_generator_clear();fb_csg_clear();}
    fb_spin_captured=fb_spin_valid=fb_spin_drag=0;fb_spin_undo_count=fb_spin_redo_count=0;
    memfree(fb_spin_undo);memfree(fb_spin_redo);fb_spin_undo=fb_spin_redo=__NULL__;cvar_set("ftebroom_spin_valid","0");
}
void fb_spin_save()
{
    if(!fb_spin_undo)fb_spin_undo=memalloc(128*sizeof(fb_spin_state_t));
    if(fb_spin_undo_count==128){for(int i=1;i<128;i++)fb_spin_undo[i-1]=fb_spin_undo[i];fb_spin_undo_count--;}
    fb_spin_undo[fb_spin_undo_count++]=fb_spin_state;fb_spin_redo_count=0;
}
int fb_spin_build()
{
    fb_generator_clear();fb_spin_valid=FALSE;fb_geom_error="";
    if(!fb_spin_captured)return FALSE;
    if(history!=fb_spin_history||fb_selection_epoch!=fb_spin_selection||fb_grid_geometry_epoch!=fb_spin_epoch){fb_geom_error="Selection or document changed; restart Spin";fb_spin_publish();return FALSE;}
    fb_spin_publish();int valid=fb_spin_geometry();
    if(valid){fb_generator_items=memalloc(fb_csg_count*sizeof(fb_generator_preview_t));fb_generator_count=fb_csg_count;
        for(int i=0;i<fb_csg_count&&valid;i++)valid=fb_csg_copy(&fb_csg_results[i].poly,&fb_generator_items[i].poly);
    }
    fb_csg_clear();
    if(valid){fb_generator_hide_original=FALSE;fb_generator_history=history;fb_generator_selection=fb_selection_epoch;fb_generator_geometry=fb_grid_geometry_epoch;fb_spin_valid=TRUE;cvar_set("ftebroom_generator_valid","1");}
    else fb_generator_clear();fb_spin_publish();return valid;
}
int fb_spin_capture()
{
    fb_spin_clear();fb_spin_state.segments=6;fb_spin_state.flex=0;fb_spin_state.scale=1;fb_spin_state.rotation='0 0 0';
    cvar_set("ftebroom_spin_auto","1");cvar_set("ftebroom_spin_segments","6");cvar_set("ftebroom_spin_scale","1");cvar_set("ftebroom_spin_flex","0");cvar_set("ftebroom_spin_rx","0");cvar_set("ftebroom_spin_ry","0");cvar_set("ftebroom_spin_rz","0");
    int valid=fb_spin_geometry();fb_csg_clear();if(!valid)return FALSE;
    fb_spin_state.delta=fb_spin_normal*fb_grid()*6;fb_spin_history=history;fb_spin_selection=fb_selection_epoch;fb_spin_epoch=fb_grid_geometry_epoch;fb_spin_captured=TRUE;
    return fb_spin_build();
}
int fb_spin_tool_apply()
{
    if(!fb_spin_build())return FALSE;
    fb_tx_begin("Spin");int valid=fb_spin_apply();if(valid)valid=fb_tx_commit();else fb_tx_cancel();
    fb_spin_clear();fb_document_tick();cvar_set("ftebroom_tool","0");return valid;
}
void fb_spin_cancel_drag()
{
    if(!fb_spin_drag)return;fb_spin_state=fb_spin_before;if(fb_spin_undo_count)fb_spin_undo_count--;fb_spin_drag=FALSE;fb_active=FALSE;fb_spin_build();
}
var int fb_spin_ring_axis=-1;vector fb_spin_ring_start;
vector fb_spin_ring(int axis,float fraction)
{
    vector saved=fb_rotate_center;fb_rotate_center=fb_spin_center+fb_spin_state.delta;
    vector point=fb_rotate_ring_point(axis,fraction,TRUE);fb_rotate_center=saved;return point;
}
void fb_spin_down(vector pixel,int alt,int view)
{
    if(!fb_spin_captured)return;vector point=fb_spin_center+fb_spin_state.delta;
    makevectors(getproperty(VF_ANGLES));if(!view&&(point-getproperty(VF_ORIGIN))*v_forward<=0)return;
    vector screen=project(point);screen_z=0;fb_spin_ring_axis=-1;
    if(vlen(screen-pixel)>16) {
        float best=7;
        for(int axis=0;axis<3;axis++)for(int i=0;i<48;i++) {
            if(view && axis!=fb_view_depth_axis(view))continue;
            float distance=fb_rotate_screen_distance(pixel,fb_spin_ring(axis,(float)i/48),fb_spin_ring(axis,(float)(i+1)/48));
            if(distance<best){best=distance;fb_spin_ring_axis=axis;}
        }
        if(fb_spin_ring_axis<0)return;
        vector saved=fb_rotate_center,hit;fb_rotate_center=point;int valid=fb_rotate_plane(pixel,fb_rotate_axis(fb_spin_ring_axis),&hit);fb_rotate_center=saved;
        if(!valid || vlen(hit-point)<.001)return;fb_spin_ring_start=normalize(hit-point);
    }
    fb_spin_save();fb_spin_before=fb_spin_state;fb_spin_pixel=fb_spin_lastpixel=pixel;fb_spin_lasttime=time;fb_spin_drag=TRUE;fb_active=TRUE;
    vector ray=normalize(unproject(pixel+'0 0 8192')-unproject(pixel)),size=getproperty(VF_SIZE);
    fb_vertical_fallback=1-ray_z*ray_z<.01;fb_vertical_scale=2*max(1,vlen(point-getproperty(VF_ORIGIN)))*tan(getproperty(VF_FOVY)*M_PI/360)/max(1,size_y);
    fb_spin_vertical_start=fb_vertical_drag(pixel,point);
}
void fb_spin_move(vector pixel,int alt,int view)
{
    if(!fb_spin_drag)return;if(time-fb_spin_lasttime<.033||vlen(pixel-fb_spin_lastpixel)<1)return;fb_spin_lasttime=time;fb_spin_lastpixel=pixel;
    int mode=bound(0,cvar("ftebroom_spin_mode"),2),component=bound(0,cvar("ftebroom_spin_component"),3);
    if(fb_spin_ring_axis>=0) {
        vector saved=fb_rotate_center,hit,center=fb_spin_center+fb_spin_before.delta,ringAxis=fb_rotate_axis(fb_spin_ring_axis);fb_rotate_center=center;
        int valid=fb_rotate_plane(pixel,ringAxis,&hit);fb_rotate_center=saved;if(!valid || vlen(hit-center)<.001)return;
        vector direction=normalize(hit-center);float angle=atan2(ringAxis*crossproduct(fb_spin_ring_start,direction),fb_spin_ring_start*direction)*180/M_PI;
        float step=max(1,cvar("ftebroom_rotate_step"));if(!cvar("ftebroom_ctrl"))angle=rint(angle/step)*step;
        fb_spin_state.rotation=fb_spin_before.rotation;fb_spin_state.rotation[fb_spin_ring_axis]+=angle;
    }
    else if(!mode){vector anchor=fb_spin_center+fb_spin_before.delta;
        vector delta=fb_view_plane_point(pixel,view,anchor)-fb_view_plane_point(fb_spin_pixel,view,anchor);
        if(alt&&!view)delta=[0,0,fb_vertical_drag(pixel,anchor)-fb_spin_vertical_start];
        if(component)for(int a=0;a<3;a++)if(a!=component-1)delta[a]=0;
        vector endpoint=anchor+delta;if(cvar("ftebroom_ctrl"))endpoint=fb_snap(endpoint);fb_spin_state.delta=endpoint-fb_spin_center;
    }else if(mode==1)fb_spin_state.scale=bound(.05,fb_spin_before.scale*pow(2,(fb_spin_pixel_y-pixel_y)/100),8);
    else{int axis=component?component-1:2;fb_spin_state.rotation=fb_spin_before.rotation;fb_spin_state.rotation[axis]+=rint(pixel_x-fb_spin_pixel_x);}
    fb_spin_build();
}
void fb_spin_up(vector pixel,int alt,int view){fb_spin_lasttime=-100;fb_spin_move(pixel,alt,view);fb_spin_drag=FALSE;fb_active=FALSE;}
void fb_spin_draw()
{
    if(!fb_spin_captured)return;
    if(history!=fb_spin_history||fb_selection_epoch!=fb_spin_selection||fb_grid_geometry_epoch!=fb_spin_epoch){fb_spin_clear();return;}
    for(int i=0;i<32;i++)fb_visual_line(fb_spin_section(fb_spin_center,fb_spin_center,fb_spin_normal,fb_spin_state.delta,fb_spin_state.rotation,fb_spin_state.flex,fb_spin_state.scale,(float)i/32),fb_spin_section(fb_spin_center,fb_spin_center,fb_spin_normal,fb_spin_state.delta,fb_spin_state.rotation,fb_spin_state.flex,fb_spin_state.scale,(float)(i+1)/32),'1 .2 .8',1,1,FALSE);
    for(int axis=0;axis<3;axis++)for(int i=0;i<48;i++) {
        if(cvar("ca_brush_view") && axis!=fb_view_depth_axis(cvar("ca_brush_view")))continue;
        vector color='.2 .2 .2';color[axis]=1;fb_visual_line(fb_spin_ring(axis,(float)i/48),fb_spin_ring(axis,(float)(i+1)/48),color,1,1,FALSE);
    }
    vector point=fb_spin_center+fb_spin_state.delta,size=getproperty(VF_SIZE);float radius=vlen(unproject('1 0 0')-unproject('0 0 0'))*5;
    if(!cvar("ca_brush_view"))radius=max(.25,vlen(point-getproperty(VF_ORIGIN))*tan(getproperty(VF_FOVY)*M_PI/360)*10/max(1,size_y));
    vector extent=[radius,radius,radius];fb_visual_box(point-extent,point+extent,'1 1 .2',1,FALSE);
}
int fb_spin_command()
{
    if(!fb_spin_captured)return FALSE;string cmd=argv(0),op=argv(1);
    if(cmd=="ftebroom_undo"){
        if(fb_spin_drag){fb_spin_cancel_drag();return TRUE;}
        if(fb_spin_undo_count){if(!fb_spin_redo)fb_spin_redo=memalloc(128*sizeof(fb_spin_state_t));fb_spin_redo[fb_spin_redo_count++]=fb_spin_state;fb_spin_state=fb_spin_undo[--fb_spin_undo_count];fb_spin_build();}return TRUE;
    }
    if(cmd=="ftebroom_redo"){
        if(fb_spin_drag)return TRUE;if(fb_spin_redo_count){fb_spin_undo[fb_spin_undo_count++]=fb_spin_state;fb_spin_state=fb_spin_redo[--fb_spin_redo_count];fb_spin_build();}return TRUE;
    }
    if(cmd!="ftebroom_spin")return FALSE;
    if(op=="apply"){fb_spin_tool_apply();return TRUE;}if(op=="cancel"){fb_tool_deactivate();return TRUE;}
    if(fb_spin_drag)return TRUE;fb_spin_save();
    if(op=="reset"){fb_spin_state.segments=6;fb_spin_state.flex=0;fb_spin_state.scale=1;fb_spin_state.delta=fb_spin_normal*fb_grid()*6;fb_spin_state.rotation='0 0 0';}
    else if(op=="settings"){fb_spin_state.segments=bound(1,stoi(argv(2)),64);fb_spin_state.flex=bound(-1,stof(argv(3)),1);fb_spin_state.scale=bound(.05,stof(argv(4)),8);}
    else{vector value=[stof(argv(2)),stof(argv(3)),stof(argv(4))];if(fb_geom_point(value)){if(op=="point")fb_spin_state.delta=value;else if(op=="rotation")fb_spin_state.rotation=value;}}
    fb_spin_build();return TRUE;
}
