// Copyright (C) 2026 ReyondBroom contributors
// SPDX-License-Identifier: GPL-3.0-or-later
// Planar polygon decomposition for Cut and Spawn. No document mutation occurs here.
typedef struct {int indices[64];int count;} fb_polygon_piece_t;
vector fb_polygon_points[64],fb_polygon_normal;
int fb_polygon_count,fb_polygon_closed;
fb_polygon_piece_t *fb_polygon_pieces;int fb_polygon_piece_count;
float fb_polygon_orientation(vector a,vector b,vector c){return crossproduct(b-a,c-a)*fb_polygon_normal;}
int fb_polygon_on_segment(vector a,vector b,vector p)
{
    return fabs(fb_polygon_orientation(a,b,p))<.001&&(p-a)*(p-b)<=.001;
}
int fb_polygon_crosses(vector a,vector b,vector c,vector d)
{
    float first=fb_polygon_orientation(a,b,c),second=fb_polygon_orientation(a,b,d),third=fb_polygon_orientation(c,d,a),fourth=fb_polygon_orientation(c,d,b);
    if(((first>.001&&second<-.001)||(first<-.001&&second>.001))&&((third>.001&&fourth<-.001)||(third<-.001&&fourth>.001)))return TRUE;
    return fb_polygon_on_segment(a,b,c)||fb_polygon_on_segment(a,b,d)||fb_polygon_on_segment(c,d,a)||fb_polygon_on_segment(c,d,b);
}
void fb_polygon_free_pieces(){memfree(fb_polygon_pieces);fb_polygon_pieces=__NULL__;fb_polygon_piece_count=0;}
int fb_polygon_convex(fb_polygon_piece_t *piece)
{
    for(int i=0;i<piece->count;i++)if(fb_polygon_orientation(fb_polygon_points[piece->indices[i]],fb_polygon_points[piece->indices[(i+1i)%piece->count]],fb_polygon_points[piece->indices[(i+2i)%piece->count]])<-.001)return FALSE;
    return TRUE;
}
int fb_polygon_decompose()
{
    fb_polygon_free_pieces();
    if(fb_polygon_count<3||!fb_polygon_closed){fb_geom_error="Close a polygon with at least three points";return FALSE;}
    float area=0;
    for(int i=0;i<fb_polygon_count;i++){
        if(fabs((fb_polygon_points[i]-fb_polygon_points[0])*fb_polygon_normal)>.01){fb_geom_error="Polygon points must be coplanar";return FALSE;}
        int next=(i+1i)%fb_polygon_count;
        if(vlen(fb_polygon_points[i]-fb_polygon_points[next])<.01){fb_geom_error="Duplicate polygon point";return FALSE;}
        area+=fb_polygon_orientation(fb_polygon_points[0],fb_polygon_points[i],fb_polygon_points[next]);
        for(int j=i+1;j<fb_polygon_count;j++){
            int end=(j+1i)%fb_polygon_count;if(next==j||end==i)continue;
            if(fb_polygon_crosses(fb_polygon_points[i],fb_polygon_points[next],fb_polygon_points[j],fb_polygon_points[end])){fb_geom_error="Self-intersecting polygon";return FALSE;}
        }
    }
    if(fabs(area)<.01){fb_geom_error="Polygon has no area";return FALSE;}
    int indices[64];for(int i=0;i<fb_polygon_count;i++)indices[i]=area>0?i:fb_polygon_count-1-i;
    int remaining=fb_polygon_count,pruned=TRUE;
    while(pruned&&remaining>3){pruned=FALSE;for(int i=0;i<remaining;i++){
        vector a=fb_polygon_points[indices[(i+remaining-1i)%remaining]],b=fb_polygon_points[indices[i]],c=fb_polygon_points[indices[(i+1i)%remaining]];
        if(fb_polygon_on_segment(a,c,b)){for(int j=i;j<remaining-1;j++)indices[j]=indices[j+1];remaining--;pruned=TRUE;break;}
    }}
    fb_polygon_pieces=memalloc(64*sizeof(fb_polygon_piece_t));
    while(remaining>3){
        int ear=-1;
        for(int i=0;i<remaining;i++){
            int previous=(i+remaining-1i)%remaining,next=(i+1i)%remaining;vector a=fb_polygon_points[indices[previous]],b=fb_polygon_points[indices[i]],c=fb_polygon_points[indices[next]];
            if(fb_polygon_orientation(a,b,c)<=.001)continue;
            int occupied=FALSE;
            for(int j=0;j<remaining&&!occupied;j++)if(j!=previous&&j!=i&&j!=next){vector point=fb_polygon_points[indices[j]];if(fb_polygon_orientation(a,b,point)>=-.001&&fb_polygon_orientation(b,c,point)>=-.001&&fb_polygon_orientation(c,a,point)>=-.001)occupied=TRUE;}
            if(!occupied){ear=i;fb_polygon_piece_t *piece=&fb_polygon_pieces[fb_polygon_piece_count++];piece->count=3;piece->indices[0]=indices[previous];piece->indices[1]=indices[i];piece->indices[2]=indices[next];break;}
        }
        if(ear<0){fb_polygon_free_pieces();fb_geom_error="Could not triangulate polygon; remove collinear or overlapping points";return FALSE;}
        for(int i=ear;i<remaining-1;i++)indices[i]=indices[i+1];remaining--;
    }
    fb_polygon_piece_t *last=&fb_polygon_pieces[fb_polygon_piece_count++];last->count=3;for(int i=0;i<3;i++)last->indices[i]=indices[i];
    int merged=TRUE;
    while(merged){merged=FALSE;
        for(int a=0;a<fb_polygon_piece_count&&!merged;a++)for(int b=a+1;b<fb_polygon_piece_count&&!merged;b++){
            fb_polygon_piece_t *left=&fb_polygon_pieces[a];fb_polygon_piece_t *right=&fb_polygon_pieces[b];
            if(left->count+right->count-2>62)continue;
            for(int i=0;i<left->count&&!merged;i++)for(int j=0;j<right->count&&!merged;j++){
                if(left->indices[i]!=right->indices[(j+1i)%right->count]||left->indices[(i+1i)%left->count]!=right->indices[j])continue;
                fb_polygon_piece_t candidate;memfill8((__variant*)&candidate,0,sizeof(candidate));
                int combined=0;
                for(int k=0;k<left->count;k++){candidate.indices[combined]=left->indices[(i+1i+k)%left->count];combined++;}
                for(int k=1;k<right->count-1;k++){candidate.indices[combined]=right->indices[(j+1i+k)%right->count];combined++;}
                candidate.count=combined;
                if(!fb_polygon_convex(&candidate))continue;
                *left=candidate;for(int k=b;k<fb_polygon_piece_count-1;k++)fb_polygon_pieces[k]=fb_polygon_pieces[k+1];fb_polygon_piece_count--;merged=TRUE;
            }
        }
    }
    return TRUE;
}
int fb_polygon_prism(fb_polygon_piece_t *piece,float low,float high,fb_poly_t *out)
{
    if(high-low<.01){fb_geom_error="Polygon extrusion has no thickness";return FALSE;}
    vector *points=memalloc(piece->count*2*sizeof(vector));float plane=fb_polygon_points[0]*fb_polygon_normal;
    for(int i=0;i<piece->count;i++){vector p=fb_polygon_points[piece->indices[i]];points[i*2]=p+fb_polygon_normal*(low-plane);points[i*2+1]=p+fb_polygon_normal*(high-plane);}
    int valid=fb_hull(points,piece->count*2,out);memfree(points);return valid;
}

fb_csg_item_t *fb_polygon_sources;int fb_polygon_source_count,fb_polygon_spawn,fb_polygon_model;
int fb_polygon_history,fb_polygon_selection,fb_polygon_epoch,fb_polygon_valid;
vector fb_polygon_anchor;int fb_polygon_captured;
void fb_polygon_publish()
{
    cvar_set("ftebroom_polygon_count",itos(fb_polygon_count));cvar_set("ftebroom_polygon_closed",itos(fb_polygon_closed));cvar_set("ftebroom_polygon_valid",itos(fb_polygon_valid));
    cvar_set("ftebroom_selection_status",fb_geom_error);
}
void fb_polygon_clear()
{
    if(fb_polygon_captured){fb_generator_clear();fb_csg_clear();}fb_polygon_captured=FALSE;
    for(int i=0;i<fb_polygon_source_count;i++)fb_poly_free(&fb_polygon_sources[i].poly);
    memfree(fb_polygon_sources);fb_polygon_sources=__NULL__;fb_polygon_source_count=0;fb_polygon_free_pieces();
    fb_polygon_count=fb_polygon_closed=fb_polygon_valid=0;cvar_set("ftebroom_polygon_valid","0");
}
int fb_polygon_capture(int spawnMode)
{
    fb_polygon_clear();fb_generator_clear();fb_polygon_captured=TRUE;fb_polygon_spawn=spawnMode;fb_polygon_model=fb_creation_model();fb_polygon_anchor='0 0 0';
    int selected=selectedbrushcount;
    if(!spawnMode&&(!selected||selected>64)){fb_geom_error="Select 1..64 convex brushes for Polygon Cut";return FALSE;}
    if(!spawnMode){fb_polygon_sources=memalloc(selected*sizeof(fb_csg_item_t));fb_polygon_source_count=selected;int vertices=0;
        for(int b=0;b<selected;b++){fb_csg_item_t *source=&fb_polygon_sources[b];source->model=selectedbrushes[b].model;source->id=selectedbrushes[b].id;
            if(!fb_poly_from_brush(source->model,source->id,&source->poly)){fb_polygon_clear();return FALSE;}
            brush_get(source->model,source->id,__NULL__,0,&source->contents);for(int v=0;v<source->poly.nv;v++){fb_polygon_anchor+=source->poly.vertices[v];vertices++;}
        }
        if(vertices)fb_polygon_anchor/=(float)vertices;
    }
    fb_polygon_history=history;fb_polygon_selection=fb_selection_epoch;fb_polygon_epoch=fb_grid_geometry_epoch;fb_geom_error="";fb_polygon_publish();return TRUE;
}
int fb_polygon_clip_volume(fb_poly_t *source,fb_poly_t *cutter,int model,int contents)
{
    fb_poly_t rest={0};int ok=fb_csg_copy(source,&rest);
    for(int f=0;f<cutter->nf&&ok&&rest.nf;f++){fb_poly_t front={0},back={0};ok=fb_csg_split(&rest,cutter->faces[f].attr,&front,&back);fb_poly_free(&front);fb_poly_free(&rest);rest=back;}
    if(ok&&rest.nf)ok=fb_csg_append(&rest,model,contents);fb_poly_free(&rest);return ok;
}
int fb_polygon_build()
{
    fb_generator_clear();fb_csg_clear();fb_polygon_valid=FALSE;
    if(history!=fb_polygon_history||fb_selection_epoch!=fb_polygon_selection||fb_grid_geometry_epoch!=fb_polygon_epoch){fb_geom_error="Selection or document changed; restart the polygon tool";fb_polygon_publish();return FALSE;}
    if(!fb_polygon_decompose()){fb_polygon_publish();return FALSE;}
    fb_csg_results=memalloc(2048*sizeof(fb_csg_item_t));int valid=TRUE;float plane=fb_polygon_points[0]*fb_polygon_normal;
    if(fb_polygon_spawn){
        for(int piece=0;piece<fb_polygon_piece_count&&valid;piece++){fb_poly_t prism={0};valid=fb_polygon_prism(&fb_polygon_pieces[piece],plane,plane+fb_grid(),&prism);if(valid)valid=fb_csg_append(&prism,fb_polygon_model,CONTENTBIT_SOLID);fb_poly_free(&prism);}
    }else for(int b=0;b<fb_polygon_source_count&&valid;b++){
        fb_csg_item_t *source=&fb_polygon_sources[b];float low=1000000,high=-1000000;
        for(int v=0;v<source->poly.nv;v++){float d=source->poly.vertices[v]*fb_polygon_normal;low=min(low,d);high=max(high,d);}
        int depth=bound(0,cvar("ftebroom_polygon_depth"),4096);
        if(depth){float inward=fabs(plane-low)<=fabs(high-plane)?1:-1;float start=plane-inward*max(.002,fb_grid()*.05),end=plane+inward*depth*fb_grid();low=min(start,end);high=max(start,end);}
        else{float margin=max(1,fb_grid());low=max(-autocvar_ftebroom_world_bound,low-margin);high=min(autocvar_ftebroom_world_bound,high+margin);}
        int begin=fb_csg_count;fb_poly_t copy={0};valid=fb_csg_copy(&source->poly,&copy);if(valid)valid=fb_csg_append(&copy,source->model,source->contents);fb_poly_free(&copy);
        for(int piece=0;piece<fb_polygon_piece_count&&valid;piece++){
            fb_poly_t cutter={0};valid=fb_polygon_prism(&fb_polygon_pieces[piece],low,high,&cutter);if(!valid){fb_poly_free(&cutter);break;}
            int size=fb_csg_count-begin;fb_csg_item_t *previous=memalloc(size*sizeof(fb_csg_item_t));memcpy(previous,&fb_csg_results[begin],size*sizeof(fb_csg_item_t));fb_csg_count=begin;
            for(int part=0;part<size;part++){if(valid)valid=fb_csg_difference(&previous[part].poly,&cutter,source->model,source->contents);fb_poly_free(&previous[part].poly);}memfree(previous);fb_poly_free(&cutter);
        }
        if(cvar("ftebroom_polygon_keep"))for(int piece=0;piece<fb_polygon_piece_count&&valid;piece++){
            fb_poly_t cutter={0};valid=fb_polygon_prism(&fb_polygon_pieces[piece],low,high,&cutter);if(valid)valid=fb_polygon_clip_volume(&source->poly,&cutter,source->model,source->contents);fb_poly_free(&cutter);
        }
    }
    if(valid&&fb_csg_count>512){valid=FALSE;fb_geom_error="Polygon operation exceeds 512 brushes";}
    if(valid){
        fb_generator_items=memalloc(fb_csg_count*sizeof(fb_generator_preview_t));fb_generator_count=fb_csg_count;
        for(int i=0;i<fb_csg_count&&valid;i++)valid=fb_csg_copy(&fb_csg_results[i].poly,&fb_generator_items[i].poly);
        if(valid){fb_generator_hide_original=!fb_polygon_spawn;fb_generator_original_count=fb_polygon_source_count;fb_generator_original=memalloc(fb_polygon_source_count*sizeof(selbrush_t));
            for(int i=0;i<fb_polygon_source_count;i++){fb_generator_original[i].model=fb_polygon_sources[i].model;fb_generator_original[i].id=fb_polygon_sources[i].id;}
            fb_generator_history=history;fb_generator_selection=fb_selection_epoch;fb_generator_geometry=fb_grid_geometry_epoch;fb_polygon_valid=TRUE;cvar_set("ftebroom_generator_valid","1");
        }
    }
    if(!valid){fb_generator_clear();fb_csg_clear();}fb_polygon_publish();return valid;
}
int fb_polygon_apply()
{
    if(!fb_polygon_build())return FALSE;
    fb_tx_begin(fb_polygon_spawn?"Polygon Spawn":"Polygon Cut");brush_deselectall();int valid=TRUE;brushface_t *faces=memalloc(64*sizeof(brushface_t));
    for(int i=0;i<fb_csg_count&&valid;i++){fb_csg_item_t *item=&fb_csg_results[i];for(int f=0;f<item->poly.nf;f++)faces[f]=item->poly.faces[f].attr;valid=brush_history_create(item->model,faces,item->poly.nf,item->contents,TRUE)!=0;}
    if(valid)for(int i=0;i<fb_polygon_source_count;i++)brush_history_delete(fb_polygon_sources[i].model,fb_polygon_sources[i].id);
    memfree(faces);if(valid)valid=fb_tx_commit();else fb_tx_cancel();fb_csg_clear();fb_generator_clear();fb_polygon_clear();fb_document_tick();cvar_set("ftebroom_tool","0");return valid;
}

int fb_polygon_drag,fb_polygon_saved_count;vector fb_polygon_before,fb_polygon_lastpixel;float fb_polygon_lasttime;
int fb_polygon_pick(vector pixel,int view,vector *point)
{
    vector start=unproject(pixel),ray=unproject(pixel+'0 0 8192')-start;
    if(!fb_polygon_count){
        if(view){fb_polygon_normal='0 0 0';fb_polygon_normal[fb_view_depth_axis(view)]=1;*point=fb_view_plane_point(pixel,view,fb_polygon_anchor);}
        else if(fb_pick_query(start,start+ray,pixel,0,FB_HIT_BRUSH,FALSE)){fb_polygon_normal=fb_hits[0].normal;*point=fb_hits[0].point;}
        else{fb_polygon_normal='0 0 1';float initialDenominator=ray_z;if(fabs(initialDenominator)<.00001)return FALSE;float initialT=(fb_polygon_anchor_z-start_z)/initialDenominator;if(initialT<0)return FALSE;*point=start+ray*initialT;}
        *point=fb_snap_to_plane(*point,fb_polygon_normal,(*point)*fb_polygon_normal);return TRUE;
    }
    float denominator=ray*fb_polygon_normal;if(fabs(denominator)<.00001)return FALSE;
    float t=(fb_polygon_points[0]-start)*fb_polygon_normal/denominator;if(t<0&&!view)return FALSE;
    *point=fb_snap_to_plane(start+ray*t,fb_polygon_normal,fb_polygon_points[0]*fb_polygon_normal);return TRUE;
}
void fb_polygon_cancel_drag()
{
    if(!fb_polygon_drag)return;fb_polygon_points[fb_polygon_drag-1]=fb_polygon_before;fb_polygon_count=fb_polygon_saved_count;fb_polygon_drag=0;fb_active=FALSE;
    if(fb_polygon_closed)fb_polygon_build();else fb_polygon_publish();
}
void fb_polygon_down(vector pixel,int view)
{
    if(!fb_polygon_captured)return;int hit=-1;float nearest=12;makevectors(getproperty(VF_ANGLES));vector forward=v_forward;
    for(int i=0;i<fb_polygon_count;i++){if(!view&&(fb_polygon_points[i]-getproperty(VF_ORIGIN))*forward<=0)continue;vector screen=project(fb_polygon_points[i]);screen_z=0;float distance=vlen(screen-pixel);if(distance<nearest){nearest=distance;hit=i;}}
    if(hit==0&&fb_polygon_count>=3&&!fb_polygon_closed){fb_polygon_closed=TRUE;fb_polygon_build();return;}
    fb_polygon_saved_count=fb_polygon_count;
    if(hit<0){
        if(fb_polygon_closed||fb_polygon_count==64)return;vector point;if(!fb_polygon_pick(pixel,view,&point))return;
        for(int i=0;i<fb_polygon_count;i++)if(vlen(point-fb_polygon_points[i])<.01)return;
        hit=fb_polygon_count;fb_polygon_points[fb_polygon_count++]=point;
    }
    fb_polygon_drag=hit+1;fb_polygon_before=fb_polygon_points[hit];fb_polygon_lastpixel=pixel;fb_polygon_lasttime=time;fb_active=TRUE;fb_geom_error="";fb_polygon_publish();
}
void fb_polygon_move(vector pixel,int view)
{
    if(!fb_polygon_drag)return;if(time-fb_polygon_lasttime<.033||vlen(pixel-fb_polygon_lastpixel)<1)return;
    vector point;if(!fb_polygon_pick(pixel,view,&point))return;fb_polygon_lastpixel=pixel;fb_polygon_lasttime=time;fb_polygon_points[fb_polygon_drag-1]=point;
    if(fb_polygon_closed)fb_polygon_build();
}
void fb_polygon_up(vector pixel,int view){fb_polygon_lasttime=-100;fb_polygon_move(pixel,view);fb_polygon_drag=0;fb_active=FALSE;}
void fb_polygon_draw()
{
    if(!fb_polygon_captured)return;
    if(history!=fb_polygon_history||fb_selection_epoch!=fb_polygon_selection||fb_grid_geometry_epoch!=fb_polygon_epoch){fb_polygon_clear();return;}
    for(int i=1;i<fb_polygon_count;i++)fb_visual_line(fb_polygon_points[i-1],fb_polygon_points[i],'0 .9 1',1,1,FALSE);
    if(fb_polygon_closed&&fb_polygon_count>2)fb_visual_line(fb_polygon_points[fb_polygon_count-1],fb_polygon_points[0],'0 .9 1',1,1,FALSE);
    vector size=getproperty(VF_SIZE);
    for(int i=0;i<fb_polygon_count;i++){
        float radius=vlen(unproject('1 0 0')-unproject('0 0 0'))*4;
        if(!cvar("ca_brush_view"))radius=max(.25,vlen(fb_polygon_points[i]-getproperty(VF_ORIGIN))*tan(getproperty(VF_FOVY)*M_PI/360)*8/max(1,size_y));
        vector extent=[radius,radius,radius];fb_visual_box(fb_polygon_points[i]-extent,fb_polygon_points[i]+extent,i==0?'1 1 0':'0 .9 1',1,FALSE);
    }
}
int fb_polygon_command()
{
    if(argv(0)!="ftebroom_polygon")return FALSE;if(!fb_polygon_captured)return TRUE;
    string op=argv(1);
    if(op=="apply"){if(fb_polygon_count>=3)fb_polygon_closed=TRUE;fb_polygon_apply();}
    else if(op=="close"){if(fb_polygon_count>=3)fb_polygon_closed=TRUE;fb_polygon_build();}
    else if(op=="preview"){if(fb_polygon_closed)fb_polygon_build();}
    else if(op=="back"){fb_polygon_cancel_drag();if(fb_polygon_closed)fb_polygon_closed=FALSE;else if(fb_polygon_count)fb_polygon_count--;fb_generator_clear();fb_csg_clear();fb_polygon_valid=FALSE;fb_geom_error="";fb_polygon_publish();}
    else if(op=="reset"){fb_polygon_cancel_drag();fb_polygon_count=fb_polygon_closed=fb_polygon_valid=0;fb_generator_clear();fb_csg_clear();fb_geom_error="";fb_polygon_publish();}
    else if(op=="cancel")fb_tool_deactivate();
    return TRUE;
}
