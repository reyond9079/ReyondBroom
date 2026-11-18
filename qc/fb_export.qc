// SPDX-License-Identifier: GPL-2.0-or-later
int fb_export_included(int node)
{
    int layer=fb_nodes[node].layer;
    if(layer==1)return fb_nodes[0].fields["_tb_layer_omit_from_export"]!="1";
    return fb_nodes[layer].fields["_tb_layer_omit_from_export"]!="1";
}
int fb_export_owner(int node)
{
    int parent=fb_nodes[node].parent;return parent<=1?0:parent;
}
int fb_export_map()
{
    cvar_set("ftebroom_export_status","-1");fb_document_tick();filestream file=fopen("export-map.map",FILE_WRITE);if((float)file<0)return FALSE;
    fb_clip_q3_export=TRUE;int success=TRUE;
    // Build owner buckets once, preserving the original geometry order.
    int *heads=memalloc(sizeof(int)*fb_node_count);
    int *next=memalloc(sizeof(int)*fb_node_count);
    for(int h=0;h<fb_node_count;h++)heads[h]=0;
    for(int b=fb_node_count-1;b>=2;b--)if(fb_nodes[b].kind>=FB_NODE_BRUSH && fb_export_included(b)) {
        int owner=fb_export_owner(b);
        if(owner>=0 && owner<fb_node_count){next[b]=heads[owner];heads[owner]=b;}
    }
    for(int e=0;e<fb_node_count;e++) {
        if(fb_nodes[e].kind<0 || e==1 || (e && (fb_nodes[e].kind>=FB_NODE_BRUSH || !fb_export_included(e))))continue;
        fputs(file,"{\n");hashtable fields=(hashtable)fb_nodes[e].fields;
        for(int k=0;;k++) {
            string key=hash_getkey(fields,k);if isnull(key)break;
            if(substring(key,0,10)=="_ftebroom_")continue;
            if(key=="model" && substring(fields[key],0,1)=="*")continue;
            fputs(file,strcat(fb_map_quote(key)," ",fb_map_quote(fields[key]),"\n"));
        }
        for(int n=heads[e];n;n=next[n])
            if(!fb_clip_write_geometry(file,fb_nodes[n].model,fb_nodes[n].id))success=FALSE;
        fputs(file,"}\n");
    }
    memfree(heads);memfree(next);fb_clip_q3_export=FALSE;fclose(file);
    cvar_set("ftebroom_export_status",success?"1":"-1");return success;
}

int fb_export_world_brush_selection()
{
    if(!selectedbrushcount)return FALSE;
    for(int i=0;i<selectedbrushcount;i++){
        selbrush_t brush=selectedbrushes[i];
        int node=fb_document_brush_node(brush.model,brush.id);
        if(brush.faceonly || node<2 || fb_nodes[node].kind!=FB_NODE_BRUSH || !fb_pick_allowed(node,0))return FALSE;
        for(int parent=fb_nodes[node].parent,steps=0;parent>=2 && steps++<fb_node_count;parent=fb_nodes[parent].parent)
            if(fb_nodes[parent].kind==FB_NODE_ENTITY)return FALSE;
    }
    return TRUE;
}

selbrush_t *fb_brush_model_source;
int fb_brush_model_count,fb_brush_model_ticket,fb_brush_model_map,fb_brush_model_revision;
vector fb_brush_model_pivot;

void fb_brush_model_clear()
{
    memfree(fb_brush_model_source);
    fb_brush_model_source=__NULL__;
    fb_brush_model_count=fb_brush_model_ticket=0;
}

void fb_brush_model_replace(int ticket,string classname,string modelPath,string legacyKey,int fteQ1)
{
    cvar_set("ftebroom_brush_model_status","stale");
    cvar_set("ftebroom_brush_model_ack",itos(ticket));
    if(!ticket || ticket!=fb_brush_model_ticket || ticket!=cvar("ftebroom_brush_model_request"))return;
    if(fb_brush_model_map!=cvar("ftebroom_map_instance")){
        fb_brush_model_clear();return;
    }
    fb_tool_refresh();fb_document_tick();
    if(fb_brush_model_revision!=fb_document_revision() || !fb_geom_point(fb_brush_model_pivot)){
        fb_brush_model_clear();return;
    }
    int parent=-1;
    for(int b=0;b<fb_brush_model_count;b++){
        selbrush_t brush=fb_brush_model_source[b];
        int node=fb_document_brush_node(brush.model,brush.id);
        if(node<2 || fb_nodes[node].kind!=FB_NODE_BRUSH || !fb_pick_allowed(node,0)){
            fb_brush_model_clear();return;
        }
        if(parent<0)parent=fb_nodes[node].parent;
        while(parent>=2 && !fb_selection_descendant(node,parent))parent=fb_nodes[parent].parent;
    }
    string data=strzone(strcat("\"classname\" \"",fb_organize_escape(classname),"\"\n\"model\" \"",
        fb_organize_escape(modelPath),"\"\n",
        sprintf("\"origin\" \"%.9g %.9g %.9g\"\n",fb_brush_model_pivot_x,fb_brush_model_pivot_y,fb_brush_model_pivot_z)));
    fb_tx_begin("Replace brushes with model");
    // Reuse the original containing group/layer when the brushes share one.
    int entityIndex=fb_tx_empty_entity();
    fb_tx_entity_set(entityIndex,data,HIST_ENTITY);
    if(terrain_edit(TEREDIT_ENT_GET,entityIndex)==""){
        strunzone(data);fb_tx_cancel();fb_brush_model_clear();return;
    }
    fb_organize_parent(entityIndex,parent);
    if(legacyKey=="mdl")fb_tx_set_key(entityIndex,"mdl",modelPath,HIST_ENTITY);
    if(fteQ1){
        fb_tx_set_key(entityIndex,"_reyondbroom_authoring","1",HIST_ENTITY);
        fb_tx_set_key(entityIndex,"_reyondbroom_fte_q1","1",HIST_ENTITY);
    }
    strunzone(data);
    for(int b=0;b<fb_brush_model_count;b++)
        brush_history_delete(fb_brush_model_source[b].model,fb_brush_model_source[b].id);
    fb_selection_clear();fb_entity_select(entityIndex,TRUE);
    cvar_set("ftebroom_brush_model_status",fb_tx_commit()?"ok":"stale");
    fb_brush_model_clear();
}

// Export a brush-only world, without changing selection, history or entity data.
int fb_export_selected_bsp(optional int worldOnly)
{
    cvar_set("ftebroom_export_status","-1");fb_document_tick();
    fb_brush_model_clear();
    if(!selectedbrushcount || (worldOnly && !fb_export_world_brush_selection()))return FALSE;
    vector minimum='0 0 0',maximum='0 0 0';int haveBounds=FALSE;
    // Validate the whole selection before writing: patches are not brush models.
    for(int i=0;i<selectedbrushcount;i++){
        int contents;int count=brush_get(selectedbrushes[i].model,selectedbrushes[i].id,__NULL__,0,&contents);
        if(count<4 || count>64)return FALSE;
        vector bounds[2];if(brush_getfacepoints(selectedbrushes[i].model,selectedbrushes[i].id,0,bounds,2)!=2)return FALSE;
        if(!haveBounds){minimum=bounds[0];maximum=bounds[1];haveBounds=TRUE;}else for(int axis=0;axis<3;axis++){minimum[axis]=min(minimum[axis],bounds[0][axis]);maximum[axis]=max(maximum[axis],bounds[1][axis]);}
    }
    filestream file=fopen("export-map.map",FILE_WRITE);if((float)file<0)return FALSE;
    fputs(file,"{\n\"classname\" \"worldspawn\"\n");
    string wad=fb_nodes[0].fields["wad"];if(wad!="")fputs(file,strcat("\"wad\" ",fb_map_quote(wad),"\n"));
    fb_clip_q3_export=TRUE;int success=TRUE;vector pivot=(minimum+maximum)*.5;cvar_set("ftebroom_bsp_export_pivot",vtos(pivot));
    static brushface_t exported[64];
    for(int b=0;b<selectedbrushcount;b++){
        int contents,count=brush_get(selectedbrushes[b].model,selectedbrushes[b].id,exported,64,&contents);fputs(file,"{\n");
        for(int f=0;f<count;f++){
            exported[f].planedist-=exported[f].planenormal*pivot;exported[f].sbias+=exported[f].sdir*pivot;exported[f].tbias+=exported[f].tdir*pivot;
            fputs(file,fb_clip_face(&exported[f],contents));
        }fputs(file,"}\n");
    }
    fb_clip_q3_export=FALSE;fputs(file,"}\n");
    // q3map2 needs an exterior flood seed to retain surfaces of an open model.
    // The native publisher removes this synthetic entity from the output BSP.
    if(!cvar("ftebroom_classic_map"))fputs(file,strcat("{\n\"classname\" \"info_player_start\"\n\"origin\" ",fb_map_quote(fb_clip_vec(minimum-pivot-'32 32 32')),"\n}\n"));
    fclose(file);
    if(success && worldOnly && cvar("ftebroom_brush_model_request")){
        fb_brush_model_count=selectedbrushcount;
        fb_brush_model_source=memalloc(fb_brush_model_count*sizeof(selbrush_t));
        memcpy(fb_brush_model_source,selectedbrushes,fb_brush_model_count*sizeof(selbrush_t));
        fb_brush_model_ticket=cvar("ftebroom_brush_model_request");
        fb_brush_model_map=cvar("ftebroom_map_instance");
        fb_brush_model_revision=fb_document_revision();
        fb_brush_model_pivot=pivot;
    }
    cvar_set("ftebroom_export_status",success?"1":"-1");return success;
}
