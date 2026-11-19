// Copyright (C) 2026 Kristian Duske
// SPDX-License-Identifier: GPL-3.0-or-later
// Brush sculpting adapted from UltimateTrenchBroom SculptTool.
var float autocvar_ftebroom_sculpt_radius=64;
var float autocvar_ftebroom_sculpt_intensity=2;
var float autocvar_ftebroom_sculpt_mode=0;
var float autocvar_ftebroom_sculpt_prevent_collapse=1;
int fb_sculpt_stroke,fb_sculpt_changed,fb_sculpt_hit;
int fb_sculpt_model,fb_sculpt_id,fb_sculpt_face;
vector fb_sculpt_center,fb_sculpt_normal,fb_sculpt_pixel;
float fb_sculpt_time;
enum : int { FB_SCULPT_WRITE_FAILED=-1, FB_SCULPT_INVALID_SAMPLE=-2 };
typedef struct {fb_poly_t source,result;int model,id,contents,offset,changed;} fb_sculpt_brush_t;
typedef struct {vector point;float delta,limit;int fixed;} fb_sculpt_vertex_t;
float fb_sculpt_falloff(float distance,float radius)
{
    float t=1-bound(0,distance/radius,1);return t*t*(3-2*t);
}
// Build all results before editing any source, so shared vertices stay watertight.
int fb_sculpt_at(vector center,vector normal)
{
    float radius=bound(1,autocvar_ftebroom_sculpt_radius,4096),intensity=bound(.1,autocvar_ftebroom_sculpt_intensity,1024);
    int mode=bound(0,autocvar_ftebroom_sculpt_mode,2),protect=autocvar_ftebroom_sculpt_prevent_collapse!=0;
    float depth=max(1,fb_grid()*.25),smoothDepth=max(max(depth,8),max(fb_grid(),radius*.35));
    fb_document_tick();fb_filter_prepare();
    fb_sculpt_brush_t *items=memalloc(128*sizeof(fb_sculpt_brush_t));
    fb_sculpt_vertex_t *vertices=memalloc(4096*sizeof(fb_sculpt_vertex_t));
    int *mapping=memalloc(4096*sizeof(int));int count=0,nv=0,mapped=0,valid=TRUE,changed=0;
    brushface_t *faces=memalloc(64*sizeof(brushface_t));
    for(int node=0;node<fb_node_count&&valid;node++) {
        if(fb_nodes[node].kind!=FB_NODE_BRUSH)continue;if(!fb_pick_allowed(node,0))continue;
        vector bounds[2];int model=fb_nodes[node].model,id=fb_nodes[node].id;
        if(brush_getfacepoints(model,id,0,bounds,2)!=2)continue;
        float distance=0;for(int axis=0;axis<3;axis++){float d=center[axis]-bound(bounds[0][axis],center[axis],bounds[1][axis]);distance+=d*d;}
        if(distance>radius*radius+smoothDepth*smoothDepth)continue;
        if(count>=128){valid=FALSE;fb_geom_error="Sculpt touches more than 128 brushes; reduce the radius";break;}
        fb_sculpt_brush_t *item=&items[count++];item->model=model;item->id=id;item->offset=mapped;
        int nf=brush_get(model,id,faces,64,&item->contents);
        if(nf<4||nf>64||!fb_poly_from_faces(faces,nf,&item->source)){valid=FALSE;break;}
        fb_poly_t *poly=&item->source;
        for(int v=0;v<poly->nv;v++) {
            if(mapped>=4096){valid=FALSE;fb_geom_error="Sculpt vertex budget exceeded; reduce the radius";break;}
            vector point=poly->vertices[v];int index=-1;
            for(int i=0;i<nv;i++)if(vlen(vertices[i].point-point)<.001){index=i;break;}
            if(index<0){index=nv++;vertices[index].point=point;vertices[index].limit=1000000;}
            mapping[mapped++]=index;
            int front=FALSE;float limit=1000000;
            for(int f=0;f<poly->nf;f++) {
                brushface_t *face=&poly->faces[f].attr;float dot=face->planenormal*normal;
                if(dot>.001&&fabs(point*face->planenormal-face->planedist)<.01)front=TRUE;
                if(protect && dot<-.001){float thickness=(face->planedist-point*face->planenormal)/(-dot);if(thickness>.001)limit=min(limit,max(0,thickness-1));}
            }
            if(!front)vertices[index].fixed=TRUE;
            vertices[index].limit=min(vertices[index].limit,limit);
        }
    }
    for(int i=0;i<nv&&valid;i++) {
        fb_sculpt_vertex_t *vertex=&vertices[i];vector delta=vertex->point-center;float height=delta*normal;
        vector planar=delta-normal*height;float distance=vlen(planar);
        if(distance>radius||vertex->fixed)continue;
        float displacement=0;
        if(mode==2) {
            if(fabs(height)>smoothDepth)continue;
            float weights=0,total=0;
            for(int j=0;j<nv;j++) {
                vector other=vertices[j].point-center;float h=other*normal;if(fabs(h)>smoothDepth)continue;
                other-=normal*h;if(vlen(other)>radius)continue;
                float weight=fb_sculpt_falloff(vlen(other-planar),radius);weights+=weight;total+=weight*h;
            }
            if(weights>0)displacement=(total/weights-height)*bound(0,intensity/8,1)*fb_sculpt_falloff(distance,radius);
        } else {
            if(fabs(height)>depth)continue;
            displacement=(mode==1?-1:1)*intensity*fb_sculpt_falloff(distance,radius)*(1-fabs(height)/depth);
        }
        if(protect)displacement=max(displacement,-vertex->limit);
        vertex->delta=displacement;
    }
    for(int b=0;b<count&&valid;b++) {
        fb_sculpt_brush_t *item=&items[b];fb_poly_t *source=&item->source;
        vector *points=memalloc(source->nv*sizeof(vector));
        for(int v=0;v<source->nv;v++){fb_sculpt_vertex_t *vertex=&vertices[mapping[item->offset+v]];points[v]=vertex->point+normal*vertex->delta;if(fabs(vertex->delta)>.001)item->changed=TRUE;}
        if(item->changed) {
            valid=fb_hull(points,source->nv,&item->result);
            if(valid&&protect&&item->result.nv!=source->nv){valid=FALSE;fb_geom_error="Sculpt would collapse vertices; reduce intensity";}
            if(valid){for(int f=0;f<source->nf;f++)faces[f]=source->faces[f].attr;fb_land_attributes(&item->result,faces,source->nf);changed++;}
        }
        memfree(points);
    }
    int preflight_valid=valid;
    if(valid)for(int b=0;b<count;b++) {
        fb_sculpt_brush_t *item=&items[b];if(!item->changed)continue;
        for(int f=0;f<item->result.nf;f++)faces[f]=item->result.faces[f].attr;
        if(!brush_history_edit(item->model,item->id,faces,item->result.nf,item->contents)){valid=FALSE;fb_geom_error="Could not apply sculpt geometry";break;}
    }
    for(int b=0;b<count;b++){fb_poly_free(&items[b].source);fb_poly_free(&items[b].result);}
    memfree(items);memfree(vertices);memfree(mapping);memfree(faces);
    if(!valid){
        cvar_set("ftebroom_selection_status",fb_geom_error);
        return preflight_valid?FB_SCULPT_WRITE_FAILED:FB_SCULPT_INVALID_SAMPLE;
    }
    return changed;
}
// A convex brush has no interior face vertices. On an unsubdivided surface
// move the hit plane when its corners lie outside the brush radius.
int fb_sculpt_face_at(int model,int id,int face,vector center)
{
    int mode=bound(0,autocvar_ftebroom_sculpt_mode,2);
    if(mode==2)return 0;
    brushface_t attributes[64];int contents;
    int count=brush_get(model,id,attributes,64,&contents);
    if(count<4 || count>64 || face<0 || face>=count)return 0;
    vector normal=attributes[face].planenormal;
    float amount=bound(.1,autocvar_ftebroom_sculpt_intensity,1024);
    if(mode==1){
        if(autocvar_ftebroom_sculpt_prevent_collapse)for(int f=0;f<count;f++){
            float oppose=-normal*attributes[f].planenormal;
            if(oppose>.001)amount=min(amount,max(0,(attributes[f].planedist-center*attributes[f].planenormal)/oppose-1));
        }
        amount=-amount;
    }
    if(fabs(amount)<.001)return 0;
    attributes[face].planedist+=amount;
    fb_poly_t result={0};
    int valid=fb_poly_from_faces(attributes,count,&result);
    fb_poly_free(&result);
    if(!valid)return FB_SCULPT_INVALID_SAMPLE;
    if(!brush_history_edit(model,id,attributes,count,contents))return FB_SCULPT_WRITE_FAILED;
    return 1;
}
void fb_sculpt_finish(int apply)
{
    if(fb_sculpt_stroke){fb_sculpt_stroke=FALSE;if(apply&&fb_sculpt_changed)fb_tx_commit();else fb_tx_cancel();fb_document_tick();}
    fb_sculpt_changed=FALSE;fb_active=FALSE;fb_sculpt_hit=FALSE;
}
void fb_sculpt_pick(vector pixel)
{
    fb_sculpt_hit=FALSE;
    int orthographic=cvar("ca_brush_view")!=0;
    fb_pick_query(unproject(pixel),unproject(pixel+[0,0,orthographic?1:8192]),pixel,0,FB_HIT_BRUSH,orthographic);
    if(fb_hit_count){
        fb_hit_t hit=fb_hits[0];
        fb_sculpt_center=hit.point;fb_sculpt_normal=hit.normal;fb_sculpt_hit=TRUE;
        fb_sculpt_model=hit.model;fb_sculpt_id=hit.id;fb_sculpt_face=hit.face-1;
    }
}
void fb_sculpt_sample()
{
    int result=fb_sculpt_at(fb_sculpt_center,fb_sculpt_normal);
    if(!result)result=fb_sculpt_face_at(fb_sculpt_model,fb_sculpt_id,fb_sculpt_face,fb_sculpt_center);
    if(result==FB_SCULPT_WRITE_FAILED){fb_sculpt_finish(FALSE);return;}
    // An invalid hull was rejected before touching any brushes in this sample.
    if(result==FB_SCULPT_INVALID_SAMPLE)return;
    if(result>0)fb_sculpt_changed=TRUE;
    else if(autocvar_ftebroom_sculpt_mode==2)
        cvar_set("ftebroom_selection_status","No uneven vertices to smooth here. Increase Radius or subdivide the terrain.");
}
void fb_sculpt_down(vector pixel)
{
    fb_sculpt_pick(pixel);if(!fb_sculpt_hit)return;
    fb_tx_begin("Sculpt brushes");fb_sculpt_stroke=TRUE;fb_sculpt_changed=FALSE;fb_active=TRUE;
    fb_sculpt_pixel=pixel;fb_sculpt_time=gettime(1);fb_sculpt_sample();
}
void fb_sculpt_move(vector pixel)
{
    float now=gettime(1);
    if(fb_sculpt_stroke&&now-fb_sculpt_time<.033)return;
    fb_sculpt_pick(pixel);if(!fb_sculpt_stroke||!fb_sculpt_hit||vlen(pixel-fb_sculpt_pixel)<1)return;
    fb_sculpt_time=now;fb_sculpt_pixel=pixel;fb_sculpt_sample();
}
void fb_sculpt_up(vector pixel)
{
    fb_sculpt_finish(TRUE);
    fb_sculpt_pick(pixel);
}
void fb_sculpt_draw()
{
    if(cvar("ftebroom_tool")!=FB_TOOL_SCULPT||!fb_sculpt_hit)return;
    vector helper='0 0 1';if(fabs(fb_sculpt_normal_z)>.75)helper='1 0 0';
    vector u=normalize(crossproduct(helper,fb_sculpt_normal)),v=crossproduct(fb_sculpt_normal,u);
    vector colour='0.35 1 .55';if(autocvar_ftebroom_sculpt_mode==1)colour='1 .35 .35';if(autocvar_ftebroom_sculpt_mode==2)colour='.35 .65 1';
    float radius=bound(1,autocvar_ftebroom_sculpt_radius,4096);
    vector center=fb_sculpt_center+fb_sculpt_normal*.5,previous=center+u*radius;
    for(int i=1;i<=48;i++){
        float angle=i*2*M_PI/48;
        vector point=center+(u*cos(angle)+v*sin(angle))*radius;
        fb_visual_line(previous,point,colour,1,1,TRUE);
        previous=point;
    }
}
