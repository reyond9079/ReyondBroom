// SPDX-License-Identifier: GPL-2.0-or-later
var float autocvar_ftebroom_plant_brush_radius=96;
var float autocvar_ftebroom_plant_brush_erase=0;
var float autocvar_ftebroom_plant_brush_hover=0;

const int FB_PLANT_BRUSH_MAX=512;
typedef struct {int id,marked;vector point;} fb_plant_brush_item_t;
fb_plant_brush_item_t *fb_plant_brush_scene;
int fb_plant_brush_scene_count,fb_plant_brush_count,fb_plant_brush_stroke;
int fb_plant_brush_mode,fb_plant_brush_epoch,fb_plant_brush_revision,fb_plant_brush_parent;
vector fb_plant_brush_points[512],fb_plant_brush_contacts[512];
vector fb_plant_brush_last,fb_plant_brush_last_normal;
float fb_plant_brush_stroke_radius,fb_plant_brush_spacing;
int fb_plant_brush_gap;

hashtable fb_plant_brush_filter;
int fb_plant_brush_filtered;
var string fb_plant_brush_model="",fb_plant_brush_source="",fb_plant_brush_class="",fb_plant_brush_optin="";
vector fb_plant_brush_low,fb_plant_brush_high;
float fb_plant_brush_scale;
var int fb_plant_brush_asset_epoch=-1;

int fb_plant_brush_is_plant(int node)
{
    if(fb_nodes[node].kind!=FB_NODE_ENTITY||!fb_selection_leaf(node))return FALSE;
    string prefix="models/reyond-plugins/plant-";
    return substring(fb_nodes[node].fields["model"],0,strlen(prefix))==prefix;
}
int fb_plant_brush_cancel()
{
    int active=fb_plant_brush_stroke;
    fb_plant_brush_stroke=FALSE;
    fb_plant_brush_count=fb_plant_brush_scene_count=0;
    memfree(fb_plant_brush_scene);fb_plant_brush_scene=__NULL__;
    return active;
}
int fb_plant_brush_surface(vector start,vector end,vector pixel,vector *point,vector *normal)
{
    if(fb_pick_query(start,end,pixel,0,FB_HIT_BRUSH|FB_HIT_PATCH,FALSE)){
        *point=fb_hits[0].point;*normal=fb_hits[0].normal;
        return vlen(*normal)>.1;
    }
    if(cvar("ftebroom_terrain_document")){
        traceline(start,end,TRUE,world);
        if(trace_fraction<1&&vlen(trace_plane_normal)>.1){
            *point=trace_endpos;*normal=trace_plane_normal;return TRUE;
        }
    }
    return FALSE;
}
int fb_plant_brush_pick(vector pixel,vector *point,vector *normal)
{
    vector low=getproperty(VF_MIN),size=getproperty(VF_SIZE);
    if(pixel_x<low_x||pixel_y<low_y||pixel_x>=low_x+size_x||pixel_y>=low_y+size_y)return FALSE;
    return fb_plant_brush_surface(unproject(pixel),unproject(pixel+'0 0 8192'),pixel,point,normal);
}
vector fb_plant_brush_axis(vector normal)
{
    vector helper='0 0 1';if(fabs(normal_z)>.75)helper='1 0 0';
    return normalize(crossproduct(helper,normal));
}
void fb_plant_brush_collect()
{
    fb_document_lists_prepare();fb_filter_prepare();
    fb_plant_brush_scene=memalloc(max(1,fb_point_node_count)*sizeof(fb_plant_brush_item_t));
    for(int i=0;i<fb_point_node_count;i++){
        int node=fb_point_nodes[i];
        if(!fb_plant_brush_is_plant(node)||!fb_pick_allowed(node,0))continue;
        string model=fb_nodes[node].fields["model"];
        if(fb_plant_brush_mode){
            if(fb_plant_brush_filtered){
                float listed=fb_plant_brush_filter[model];if(!listed)continue;
            }
        }else if(model!=fb_plant_brush_model)continue;
        fb_plant_brush_item_t *item=&fb_plant_brush_scene[fb_plant_brush_scene_count++];
        item->id=fb_nodes[node].id;item->marked=FALSE;
        item->point=stov(fb_nodes[node].fields["origin"]);
    }
}
void fb_plant_brush_add(vector point,vector normal)
{
    if(fb_plant_brush_count>=FB_PLANT_BRUSH_MAX)return;
    for(int i=0;i<fb_plant_brush_count;i++)
        if(vlen(fb_plant_brush_contacts[i]-point)<fb_plant_brush_spacing)return;
    for(int i=0;i<fb_plant_brush_scene_count;i++)
        if(vlen(fb_plant_brush_scene[i].point-point)<fb_plant_brush_spacing*.65)return;
    float support=0;
    for(int axis=0;axis<3;axis++)support+=normal[axis]*(normal[axis]>=0?fb_plant_brush_low[axis]:fb_plant_brush_high[axis]);
    vector origin=point-normal*support;
    for(int axis=0;axis<3;axis++)
        if(!fb_geom_finite(origin[axis])||fabs(origin[axis])>autocvar_ftebroom_world_bound)return;
    fb_plant_brush_contacts[fb_plant_brush_count]=point;
    fb_plant_brush_points[fb_plant_brush_count++]=origin;
}
void fb_plant_brush_dab(vector center,vector normal,vector pixel)
{
    float radius=fb_plant_brush_stroke_radius;
    if(fb_plant_brush_mode){
        for(int i=0;i<fb_plant_brush_scene_count;i++){
            fb_plant_brush_item_t *item=&fb_plant_brush_scene[i];
            if(item->marked)continue;
            vector delta=item->point-center;float depth=delta*normal;
            if(fabs(depth)>max(16,radius*.35)||vlen(delta-normal*depth)>radius)continue;
            item->marked=TRUE;fb_plant_brush_count++;
        }
        return;
    }
    fb_plant_brush_add(center,normal);
    if(radius<fb_plant_brush_spacing*1.35)return;
    vector u=fb_plant_brush_axis(normal),v=crossproduct(normal,u);
    float phase=random()*2*M_PI,reach=min(24,radius*.24);
    for(int i=0;i<4;i++){
        float angle=phase+i*M_PI*.5;
        vector offset=(u*cos(angle)+v*sin(angle))*radius*.66;
        vector point,surfaceNormal;
        if(fb_plant_brush_surface(center+offset+normal*reach,center+offset-normal*reach,pixel,&point,&surfaceNormal))
            if(surfaceNormal*normal>.6)fb_plant_brush_add(point,surfaceNormal);
    }
}
void fb_plant_brush_down(vector pixel,int view)
{
    fb_plant_brush_cancel();
    if(view)return;
    fb_plant_brush_mode=autocvar_ftebroom_plant_brush_erase!=0;
    fb_plant_brush_epoch=cvar("ftebroom_map_instance");
    if(!fb_plant_brush_mode)
        if(fb_plant_brush_asset_epoch!=fb_plant_brush_epoch||fb_plant_brush_model=="")return;
    vector point,normal;
    if(!fb_plant_brush_pick(pixel,&point,&normal))return;
    fb_plant_brush_stroke_radius=bound(8,autocvar_ftebroom_plant_brush_radius,1024);
    vector size=fb_plant_brush_high-fb_plant_brush_low;
    fb_plant_brush_spacing=max(12,max(fb_plant_brush_stroke_radius*.28,max(size_x,size_y)*.52));
    fb_plant_brush_parent=fb_selection_open_node();
    if(fb_plant_brush_parent<0)fb_plant_brush_parent=fb_organize_node(fb_active_layer);
    if(!fb_plant_brush_mode&&fb_plant_brush_parent>=2)
        if(!fb_pick_allowed(fb_plant_brush_parent,0))return;
    fb_plant_brush_revision=fb_grid_geometry_epoch;
    fb_plant_brush_gap=FALSE;fb_plant_brush_stroke=TRUE;
    fb_plant_brush_last=point;fb_plant_brush_last_normal=normal;
    fb_plant_brush_collect();
    fb_plant_brush_dab(point,normal,pixel);
}
void fb_plant_brush_move(vector pixel,int view)
{
    if(view||!fb_plant_brush_stroke)return;
    if(!fb_plant_brush_mode&&fb_plant_brush_count>=FB_PLANT_BRUSH_MAX)return;
    if(fb_plant_brush_epoch!=cvar("ftebroom_map_instance")||fb_plant_brush_revision!=fb_grid_geometry_epoch){
        fb_plant_brush_cancel();return;
    }
    vector point,normal;
    if(!fb_plant_brush_pick(pixel,&point,&normal)){fb_plant_brush_gap=TRUE;return;}
    vector delta=point-fb_plant_brush_last;float distance=vlen(delta);
    float step=max(6,fb_plant_brush_stroke_radius*.35);
    if(fb_plant_brush_gap||normal*fb_plant_brush_last_normal<.75||distance>step*12){
        fb_plant_brush_dab(point,normal,pixel);
        fb_plant_brush_gap=FALSE;fb_plant_brush_last=point;fb_plant_brush_last_normal=normal;return;
    }
    if(distance<step)return;
    for(float along=step;along<distance;along+=step){
        vector sample=fb_plant_brush_last+delta*(along/distance),hit,hitNormal;
        if(fb_plant_brush_surface(sample+normal*12,sample-normal*12,pixel,&hit,&hitNormal))
            if(hitNormal*normal>.75)fb_plant_brush_dab(hit,hitNormal,pixel);
    }
    fb_plant_brush_dab(point,normal,pixel);
    fb_plant_brush_last=point;fb_plant_brush_last_normal=normal;
}
void fb_plant_brush_up(vector pixel,int view)
{
    fb_plant_brush_move(pixel,view);
    if(!fb_plant_brush_stroke)return;
    if(fb_plant_brush_count&&fb_plant_brush_epoch==cvar("ftebroom_map_instance")&&fb_plant_brush_revision==fb_grid_geometry_epoch){
        fb_tx_begin(fb_plant_brush_mode?"Erase plants":"Paint plants");
        if(fb_plant_brush_mode){
            fb_entity_selection_batch(TRUE);
            for(int i=0;i<fb_plant_brush_scene_count;i++)if(fb_plant_brush_scene[i].marked){
                int id=fb_plant_brush_scene[i].id;
                if(fb_entity_selected(id))fb_entity_select(id,FALSE);
                fb_tx_entity_set(id,"",HIST_ENTITY);
            }
            fb_entity_selection_batch(FALSE);
        }else {
            int parent=fb_plant_brush_parent;
            string parentField="";
            if(parent>=2){
                string key=fb_nodes[parent].kind==FB_NODE_GROUP?"_tb_group":"_tb_layer";
                parentField=strcat(fb_map_quote(key)," ",fb_map_quote(fb_nodes[parent].tbid),"\n");
            }
            parentField=strzone(parentField);
            for(int i=0;i<fb_plant_brush_count;i++){
                vector origin=fb_plant_brush_points[i];
                string data=sprintf("\"classname\" \"%s\"\n\"origin\" \"%.9g %.9g %.9g\"\n\"angles\" \"0 %.9g 0\"\n",
                    fb_plant_brush_class,origin_x,origin_y,origin_z,floor(random()*360));
                data=strcat(data,"\"model\" ",fb_map_quote(fb_plant_brush_model),"\n\"_reyondbroom_source\" ",
                    fb_map_quote(fb_plant_brush_source),"\n\"_reyondbroom_authoring\" \"1\"\n");
                data=strcat(data,sprintf("\"modelscale\" \"%.9g\"\n\"_reyondbroom_normalization\" \"%.9g\"\n",
                    fb_plant_brush_scale,fb_plant_brush_scale));
                if(parentField!="")data=strcat(data,parentField);
                if(fb_plant_brush_optin=="1")data=strcat(data,"\"_reyondbroom_fte_q1\" \"1\"\n");
                fb_tx_entity_set(fb_tx_empty_entity(),data,HIST_ENTITY);
            }
            strunzone(parentField);
        }
        fb_tx_commit();
    }
    fb_plant_brush_cancel();
}
void fb_plant_brush_draw()
{
    if(cvar("ftebroom_tool")!=FB_TOOL_PLANTBRUSH||cvar("ca_brush_view")||!autocvar_ftebroom_plant_brush_hover)return;
    vector center,normal;
    if(!fb_plant_brush_pick(curmousepos,&center,&normal))return;
    vector u=fb_plant_brush_axis(normal),v=crossproduct(normal,u);
    vector color='0.35 1 .65';if(autocvar_ftebroom_plant_brush_erase)color='1 .35 .25';
    float radius=bound(8,autocvar_ftebroom_plant_brush_radius,1024);
    center+=normal*.5;
    vector prior=center+u*radius;
    for(int i=1;i<=40;i++){
        float angle=i*2*M_PI/40;
        vector point=center+(u*cos(angle)+v*sin(angle))*radius;
        fb_visual_line(prior,point,color,.9,.9,TRUE);prior=point;
    }
    if(!fb_plant_brush_stroke)return;
    int shown=0,limit=fb_plant_brush_mode?fb_plant_brush_scene_count:fb_plant_brush_count;
    for(int i=0;i<limit&&shown<128;i++){
        vector p='0 0 0';
        if(fb_plant_brush_mode){
            if(!fb_plant_brush_scene[i].marked)continue;
            p=fb_plant_brush_scene[i].point;
        }else p=fb_plant_brush_contacts[i];
        p_z+=2;shown++;
        fb_visual_line(p-'5 0 0',p+'5 0 0',color,1,1,TRUE);
        fb_visual_line(p-'0 5 0',p+'0 5 0',color,1,1,TRUE);
    }
}
float fb_plant_brush_command()
{
    if(argv(0)=="ftebroom_plant_brush_asset"){
        fb_plant_brush_cancel();
        strunzone(fb_plant_brush_model);fb_plant_brush_model=strzone(argv(1));
        strunzone(fb_plant_brush_source);fb_plant_brush_source=strzone(argv(2));
        strunzone(fb_plant_brush_class);fb_plant_brush_class=strzone(argv(3));
        strunzone(fb_plant_brush_optin);fb_plant_brush_optin=strzone(argv(4));
        fb_plant_brush_low=stov(argv(5));fb_plant_brush_high=stov(argv(6));
        fb_plant_brush_scale=stof(argv(7));fb_plant_brush_asset_epoch=stoi(argv(8));
        return TRUE;
    }
    if(argv(0)=="ftebroom_plant_brush_filter"){
        if(argv(1)=="reset"){
            if(fb_plant_brush_filter)hash_destroytab(fb_plant_brush_filter);
            fb_plant_brush_filter=hash_createtab(32,EV_FLOAT);
            fb_plant_brush_filtered=stoi(argv(2));
        }else if(fb_plant_brush_filter)fb_plant_brush_filter[argv(1)]=1;
        return TRUE;
    }
    return FALSE;
}
