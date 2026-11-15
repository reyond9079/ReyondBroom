// SPDX-License-Identifier: GPL-2.0-or-later
typedef struct {int model,count;vector *points;brushface_t face;} fb_sweep_face_t;
fb_sweep_face_t *fb_sweep_faces;int fb_sweep_count;
fb_poly_t *fb_sweep_solids;int fb_sweep_solid_count,fb_sweep_valid;
vector fb_sweep_center,fb_sweep_normal,fb_sweep_offset,fb_sweep_quaternion;
var float fb_sweep_qw=1,fb_sweep_scale=1;
var float autocvar_ftebroom_sweep_segments=4,autocvar_ftebroom_sweep_iterations=1,autocvar_ftebroom_sweep_integer=0;
var string autocvar_ftebroom_sweep_path="straight";
void fb_sweep_clear_solids()
{
    for(int i=0;i<fb_sweep_solid_count;i++)fb_poly_free(&fb_sweep_solids[i]);
    memfree(fb_sweep_solids);fb_sweep_solids=__NULL__;fb_sweep_solid_count=fb_sweep_valid=0;
    cvar_set("ftebroom_sweep_valid","0");
}
void fb_sweep_clear()
{
    fb_sweep_clear_solids();
    for(int i=0;i<fb_sweep_count;i++){memfree(fb_sweep_faces[i].points);strunzone(fb_sweep_faces[i].face.shadername);}
    memfree(fb_sweep_faces);fb_sweep_faces=__NULL__;fb_sweep_count=0;
}
void fb_sweep_reset()
{
    fb_sweep_offset=fb_sweep_quaternion='0 0 0';fb_sweep_qw=fb_sweep_scale=1;fb_sweep_clear_solids();
}
int fb_sweep_capture()
{
    fb_sweep_clear();fb_sweep_reset();int faces=fb_faces_count();if(!faces)return FALSE;
    fb_sweep_faces=memalloc(faces*sizeof(fb_sweep_face_t));
    vector low='1000000 1000000 1000000',high=-low;fb_sweep_normal='0 0 0';
    for(int b=0;b<selectedbrushcount;b++) {
        if(selbrush_whole(&selectedbrushes[b]))continue;fb_poly_t poly={0};
        if(!fb_poly_from_brush(selectedbrushes[b].model,selectedbrushes[b].id,&poly)){fb_poly_free(&poly);fb_sweep_clear();return FALSE;}
        for(int f=0;f<poly.nf;f++)if(selbrush_face(&selectedbrushes[b],f)) {
            fb_sweep_face_t *out=&fb_sweep_faces[fb_sweep_count++];fb_poly_face_t face=poly.faces[f];
            out->model=selectedbrushes[b].model;out->count=face.count;out->face=face.attr;out->face.shadername=strzone(face.attr.shadername);
            out->points=memalloc(face.count*sizeof(vector));fb_sweep_normal+=face.attr.planenormal;
            for(int p=0;p<face.count;p++) {
                vector point=poly.vertices[poly.indices[face.start+p]];out->points[p]=point;
                for(int a=0;a<3;a++){low[a]=min(low[a],point[a]);high[a]=max(high[a],point[a]);}
            }
        }
        fb_poly_free(&poly);
    }
    fb_sweep_center=(low+high)*0.5;fb_sweep_normal=normalize(fb_sweep_normal);if(vlen(fb_sweep_normal)<0.1)fb_sweep_normal='0 0 1';
    return fb_sweep_count>0;
}
void fb_sweep_rotate(vector axis,float degrees)
{
    axis=normalize(axis);float half=degrees*M_PI/360;vector q=axis*sin(half);float w=cos(half);
    vector result=fb_sweep_quaternion*w+q*fb_sweep_qw+crossproduct(q,fb_sweep_quaternion);
    float scalar=w*fb_sweep_qw-q*fb_sweep_quaternion,length=sqrt(result*result+scalar*scalar);
    if(length<0.000001)return;
    fb_sweep_quaternion=result/length;fb_sweep_qw=scalar/length;
    if(fb_sweep_qw<0){fb_sweep_qw=-fb_sweep_qw;fb_sweep_quaternion=-fb_sweep_quaternion;}
}
void fb_sweep_map(float t,fb_affine_t *map)
{
    float sine=vlen(fb_sweep_quaternion),angle=2*atan2(sine,fb_sweep_qw);vector axis='0 0 1';
    if(sine>0.000001)axis=fb_sweep_quaternion/sine;
    vector center=fb_sweep_center+fb_sweep_offset*t;
    if(autocvar_ftebroom_sweep_path=="arc" && fabs(angle)>0.00001) {
        float rise=fb_sweep_offset*axis;vector chord=fb_sweep_offset-axis*rise;
        vector pivot=fb_sweep_center+chord*0.5+crossproduct(axis,chord)*(0.5*cos(angle*0.5)/sin(angle*0.5));
        center=pivot+fb_affine_rotate_axis(fb_sweep_center-pivot,axis,angle*t)+axis*(rise*t);
    }else if(autocvar_ftebroom_sweep_path=="sbend") {
        float t2=t*t,t3=t2*t,length=vlen(fb_sweep_offset);
        vector endNormal=fb_affine_rotate_axis(fb_sweep_normal,axis,angle);
        center=fb_sweep_center+fb_sweep_offset*(-2*t3+3*t2)+fb_sweep_normal*(length*(t3-2*t2+t))+endNormal*(length*(t3-t2));
    }
    float scale=1+(fb_sweep_scale-1)*t;
    map->x=fb_affine_rotate_axis('1 0 0',axis,angle*t)*scale;
    map->y=fb_affine_rotate_axis('0 1 0',axis,angle*t)*scale;
    map->z=fb_affine_rotate_axis('0 0 1',axis,angle*t)*scale;
    map->translation=center-fb_affine_direction(map,fb_sweep_center);fb_affine_prepare(map);
}
int fb_sweep_build()
{
    fb_sweep_clear_solids();if(!fb_sweep_count)return FALSE;
    int segments=bound(1,autocvar_ftebroom_sweep_segments,64),iterations=bound(1,autocvar_ftebroom_sweep_iterations,32);
    int total=segments*iterations*fb_sweep_count;
    if(total>512 || fb_sweep_scale<0.01 || fb_sweep_scale>100 || !fb_geom_point(fb_sweep_offset))return FALSE;
    fb_sweep_solids=memalloc(total*sizeof(fb_poly_t));fb_sweep_solid_count=total;int valid=TRUE,index=0;
    fb_affine_t power,cap;fb_linked_matrix("",&power);fb_sweep_map(1,&cap);
    for(int iteration=0;iteration<iterations && valid;iteration++) {
        for(int segment=0;segment<segments && valid;segment++) {
            fb_affine_t localA,localB,start,end;fb_sweep_map((float)segment/(float)segments,&localA);fb_sweep_map((float)(segment+1)/(float)segments,&localB);
            fb_linked_compose(&power,&localA,&start);fb_linked_compose(&power,&localB,&end);
            for(int f=0;f<fb_sweep_count && valid;f++) {
                fb_sweep_face_t *face=&fb_sweep_faces[f];vector *points=memalloc(face->count*2*sizeof(vector));
                for(int p=0;p<face->count;p++) {
                    points[p]=fb_affine_point(&start,face->points[p]);points[p+face->count]=fb_affine_point(&end,face->points[p]);
                }
                if(autocvar_ftebroom_sweep_integer)for(int p=0;p<face->count*2;p++)for(int axis=0;axis<3;axis++)points[p][axis]=rint(points[p][axis]);
                fb_poly_t *poly=&fb_sweep_solids[index++];valid=fb_hull(points,face->count*2,poly);memfree(points);
                if(!valid)break;
                brushface_t front=face->face,back=face->face;fb_affine_face(&start,&front,TRUE);fb_affine_face(&end,&back,TRUE);
                for(int s=0;s<poly->nf;s++) {
                    brushface_t *out=&poly->faces[s].attr;vector normal=out->planenormal;float distance=out->planedist;
                    strunzone(out->shadername);*out=face->face;out->shadername=strzone(face->face.shadername);out->planenormal=normal;out->planedist=distance;
                    if(fabs(normal*front.planenormal)>0.999){out->sdir=front.sdir;out->tdir=front.tdir;out->sbias=front.sbias;out->tbias=front.tbias;}
                    else if(fabs(normal*back.planenormal)>0.999){out->sdir=back.sdir;out->tdir=back.tdir;out->sbias=back.sbias;out->tbias=back.tbias;}
                    else {reset_texturecoords(out);out->sdir=normalize(out->sdir)*vlen(face->face.sdir);out->tdir=normalize(out->tdir)*vlen(face->face.tdir);}
                }
            }
        }
        fb_affine_t next;fb_linked_compose(&power,&cap,&next);power=next;
    }
    fb_sweep_valid=valid;cvar_set("ftebroom_sweep_valid",itos(valid));return valid;
}
void fb_sweep_perform()
{
    if(!fb_sweep_build()){cvar_set("ftebroom_selection_status","Sweep requires non-flat segments within the world bounds (maximum 512 brushes)");return;}
    fb_tx_begin("Sweep");fb_selection_clear();int valid=TRUE;
    for(int i=0;i<fb_sweep_solid_count;i++) {
        fb_poly_t *poly=&fb_sweep_solids[i];brushface_t faces[64];for(int f=0;f<poly->nf;f++)faces[f]=poly->faces[f].attr;
        if(!brush_history_create(fb_sweep_faces[i%fb_sweep_count].model,faces,poly->nf,CONTENTBIT_SOLID,TRUE)){valid=FALSE;break;}
    }
    if(!valid){fb_tx_cancel();return;}
    if(fb_tx_commit()){fb_tool_deactivate();fb_sweep_clear();}
}
int fb_sweep_drag,fb_sweep_view,fb_sweep_alt;
vector fb_sweep_pixel,fb_sweep_before_offset,fb_sweep_before_quaternion,fb_sweep_drag_start;
float fb_sweep_before_qw,fb_sweep_before_scale;
void fb_sweep_cancel()
{
    if(fb_sweep_drag) {
        fb_sweep_offset=fb_sweep_before_offset;fb_sweep_quaternion=fb_sweep_before_quaternion;
        fb_sweep_qw=fb_sweep_before_qw;fb_sweep_scale=fb_sweep_before_scale;fb_sweep_build();
    }
    fb_sweep_drag=0;
}
float fb_sweep_radius()
{
    vector before=fb_rotate_center;fb_rotate_center=fb_sweep_center+fb_sweep_offset;
    float radius=fb_rotate_radius();fb_rotate_center=before;return radius;
}
vector fb_sweep_ring(int axis,float t)
{
    vector before=fb_rotate_center;fb_rotate_center=fb_sweep_center+fb_sweep_offset;
    vector point=fb_rotate_ring_point(axis,t,TRUE);fb_rotate_center=before;return point;
}
void fb_sweep_down(vector pixel,int alt,int view)
{
    if(!fb_sweep_count)return;
    vector center=fb_sweep_center+fb_sweep_offset,screen=project(center);screen_z=0;
    vector scalePoint=project(center+fb_rotate_axis(0)*fb_sweep_radius()*1.25);scalePoint_z=0;
    fb_sweep_drag=0;
    if(vlen(pixel-scalePoint)<9)fb_sweep_drag=5;
    else if(vlen(pixel-screen)<10)fb_sweep_drag=1;
    else for(int axis=0;axis<3;axis++)for(int s=0;s<48;s++)
        if(fb_rotate_screen_distance(pixel,fb_sweep_ring(axis,(float)s/48),fb_sweep_ring(axis,(float)(s+1)/48))<6)fb_sweep_drag=axis+2;
    if(!fb_sweep_drag)return;
    fb_sweep_pixel=pixel;fb_sweep_view=view;fb_sweep_alt=alt;
    fb_sweep_before_offset=fb_sweep_offset;fb_sweep_before_quaternion=fb_sweep_quaternion;
    fb_sweep_before_qw=fb_sweep_qw;fb_sweep_before_scale=fb_sweep_scale;
    if(fb_sweep_drag>=2 && fb_sweep_drag<=4) {
        vector before=fb_rotate_center;fb_rotate_center=center;
        int valid=fb_rotate_plane(pixel,fb_rotate_axis(fb_sweep_drag-2),&fb_sweep_drag_start);fb_rotate_center=before;
        if(!valid){fb_sweep_drag=0;return;}fb_sweep_drag_start=normalize(fb_sweep_drag_start-center);
    }
    fb_active=TRUE;
}
void fb_sweep_move(vector pixel,int alt,int view)
{
    if(!fb_sweep_drag || view!=fb_sweep_view)return;
    vector oldOffset=fb_sweep_offset,oldRotation=fb_sweep_quaternion;float oldScale=fb_sweep_scale,oldW=fb_sweep_qw;
    vector center=fb_sweep_center+fb_sweep_before_offset;
    if(fb_sweep_drag==1) {
        vector delta='0 0 0';
        if(!view && alt)delta_z=(fb_sweep_pixel_y-pixel_y)*fb_grid()/8;
        else delta=fb_view_plane_point(pixel,view,center)-fb_view_plane_point(fb_sweep_pixel,view,center);
        fb_sweep_offset=fb_sweep_before_offset+fb_snap_delta(delta);
    }else if(fb_sweep_drag==5)fb_sweep_scale=bound(0.01,fb_sweep_before_scale*pow(2,(pixel_x-fb_sweep_pixel_x+fb_sweep_pixel_y-pixel_y)/100),100);
    else {
        vector axis=fb_rotate_axis(fb_sweep_drag-2),point='0 0 0',before=fb_rotate_center;fb_rotate_center=center;
        int valid=fb_rotate_plane(pixel,axis,&point);fb_rotate_center=before;if(!valid)return;
        vector direction=normalize(point-center);float angle=atan2(axis*crossproduct(fb_sweep_drag_start,direction),fb_sweep_drag_start*direction)*180/M_PI;
        float step=max(0.01,cvar("ftebroom_rotate_step"));angle=rint(angle/step)*step;
        fb_sweep_quaternion=fb_sweep_before_quaternion;fb_sweep_qw=fb_sweep_before_qw;fb_sweep_rotate(axis,angle);
    }
    if(oldOffset!=fb_sweep_offset || oldRotation!=fb_sweep_quaternion || oldScale!=fb_sweep_scale || oldW!=fb_sweep_qw)fb_sweep_build();
}
void fb_sweep_up(vector pixel,int alt,int view){fb_sweep_move(pixel,alt,view);fb_sweep_drag=0;fb_active=FALSE;}
void fb_sweep_draw()
{
    if(cvar("ftebroom_tool")!=FB_TOOL_SWEEP || !fb_sweep_count)return;
    shaderforname("ftebroom/sweepwire","{\n{\nmap $whiteimage\nblendfunc blend\nrgbgen vertex\nalphagen vertex\n}\n}");
    if(fb_sweep_valid)for(int i=0;i<fb_sweep_solid_count;i++) {
        fb_poly_t *poly=&fb_sweep_solids[i];brushface_t faces[64];for(int f=0;f<poly->nf;f++)faces[f]=poly->faces[f].attr;
        DrawQCBrushTextured(faces,poly->nf,'1 1 1',0.55);DrawQCBrushWireframe(faces,poly->nf,"ftebroom/sweepwire",'1 0.8 0',0.8);
    }
    fb_affine_t cap;fb_sweep_map(1,&cap);
    for(int f=0;f<fb_sweep_count;f++)for(int p=0;p<fb_sweep_faces[f].count;p++) {
        fb_sweep_face_t *face=&fb_sweep_faces[f];
        fb_visual_line(fb_affine_point(&cap,face->points[p]),fb_affine_point(&cap,face->points[(p+1)%face->count]),'1 0.8 0',1,1,TRUE);
    }
    for(int axis=0;axis<3;axis++)for(int s=0;s<48;s++) {
        vector color='0.2 0.4 1';if(axis==0)color='1 0.2 0.2';if(axis==1)color='0.2 1 0.2';
        fb_visual_line(fb_sweep_ring(axis,(float)s/48),fb_sweep_ring(axis,(float)(s+1)/48),color,0.8,0.8,TRUE);
    }
    vector center=fb_sweep_center+fb_sweep_offset,scalePoint=center+fb_rotate_axis(0)*fb_sweep_radius()*1.25;
    float handle=fb_sweep_radius()*0.05;vector extent=[handle,handle,handle];
    fb_visual_box(center-extent,center+extent,'1 0.8 0',1,TRUE);fb_visual_box(scalePoint-extent,scalePoint+extent,'0.2 1 0.2',1,TRUE);
    cvar_set("ftebroom_sweep_offset",sprintf("%g %g %g",fb_sweep_offset_x,fb_sweep_offset_y,fb_sweep_offset_z));cvar_set("ftebroom_sweep_scale",ftos(fb_sweep_scale));
}
int fb_sweep_escape()
{
    if(fb_sweep_offset!='0 0 0' || fb_sweep_scale!=1 || fb_sweep_quaternion!='0 0 0'){fb_sweep_reset();return TRUE;}return FALSE;
}
int fb_sweep_command()
{
    if(cvar("ftebroom_tool")!=FB_TOOL_SWEEP)return FALSE;
    string cmd=argv(0),op=argv(1);
    if(cmd=="ftebroom_sweep") {
        if(op=="apply"){fb_sweep_perform();return TRUE;}
        if(op=="reset"){fb_sweep_reset();return TRUE;}
        if(op=="move")fb_sweep_offset=[stof(argv(2)),stof(argv(3)),stof(argv(4))];
        if(op=="scale")fb_sweep_scale=bound(0.01,stof(argv(2)),100);
        if(op=="grow")fb_sweep_scale=bound(0.01,fb_sweep_scale+stof(argv(2))*fb_grid()/64,100);
        if(op=="rotate")fb_sweep_rotate(fb_rotate_axis(bound(0,stoi(argv(2)),2)),stof(argv(3)));
        fb_sweep_build();return TRUE;
    }
    if(cmd=="ftebroom_nudge") {
        fb_sweep_offset+=fb_transform_nudge_axis(bound(0,stoi(argv(1)),2),cvar("ca_brush_view"),getproperty(VF_ANGLES))*stof(argv(2))*fb_grid();
        fb_sweep_build();return TRUE;
    }
    if(cmd=="ftebroom_transform_key") {
        int view=cvar("ca_brush_view");makevectors(getproperty(VF_ANGLES));vector axis=fb_affine_axis(v_forward);
        if(op=="yaw")axis='0 0 1';if(op=="pitch")axis=fb_affine_axis(v_right);
        if(view && op=="roll")axis=fb_rotate_axis(fb_view_depth_axis(view));
        fb_sweep_rotate(axis,stof(argv(2))*max(0.01,cvar("ftebroom_rotate_step")));fb_sweep_build();return TRUE;
    }
    return FALSE;
}
