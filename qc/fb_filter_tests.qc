// SPDX-License-Identifier: GPL-2.0-or-later
void fb_filter_expect(int ok,string label){print(strcat(ok?"FTEBROOM_FILTER_PASS ":"FTEBROOM_FILTER_FAIL ",label,"\n"));}
int fb_filter_selected_node()
{
    if(!selectedbrushcount)return -1;
    for(int n=2;n<fb_node_count;n++)if(fb_nodes[n].model==selectedbrushes[0].model && fb_nodes[n].id==selectedbrushes[0].id)return n;
    return -1;
}
float fb_filter_testcommand()
{
    if(argv(0)=="ftebroom_test_submodels" && cvar("ftebroom_skip_setup")) {
        fb_tool_deactivate();fb_selection_clear();
        cvar_set("ca_newbrushtexture","ftebroom/grid");
        fb_createbox('-128 0 64','64 64 64');fb_group_create("Visible group");
        fb_createbox('0 0 64','64 64 64');fb_entity_create("func_door",TRUE,'0 0 0');
        fb_createbox('128 0 64','64 64 64');fb_entity_create("func_wall",TRUE,'0 0 0');
        fb_selection_clear();fb_document_tick();fb_filter_prepare();
        int total=0;static vector bounds[2];
        for(int n=2;n<fb_node_count;n++) {
            if(fb_nodes[n].kind!=FB_NODE_BRUSH || fb_nodes[n].model==1)continue;
            total++;
            fb_filter_expect(fb_filter_custom(n) && fb_pick_allowed(n,FB_PICK_LOCKED),"unselected_submodel_visible");
            fb_filter_expect(!fb_selection_selected(n),"submodel_not_selected");
            fb_filter_expect(fb_container_bounds(fb_nodes[n].parent,bounds) && bounds[1]-bounds[0]=='64 64 64',"submodel_container_bounds");
        }
        fb_filter_expect(total==3,"all_submodels_present");
        fb_visibility_sync();return TRUE;
    }
    if(argv(0)!="ftebroom_test_filters" || !cvar("ftebroom_skip_setup"))return FALSE;
    fb_tool_deactivate();fb_createbox('192 0 64','64 64 64');fb_filter_tag("caulk",TRUE);fb_document_tick();fb_filter_prepare();int node=fb_filter_selected_node();
    fb_filter_expect(node>=0 && (fb_filter_tags[node]&FB_TAG_CAULK),"caulk_material_tag");
    fb_filter_expect(fb_filter_custom(node),"caulk_transparent_renderer");
    int caulkId=fb_nodes[node].id;
    cvar_set("ftebroom_hidden_tags","4");fb_filter_prepare();fb_filter_expect(fb_pick_allowed(node,0),"caulk_filter_keeps_selected_brush");
    fb_selection_clear();fb_filter_prepare();fb_filter_expect(!fb_pick_allowed(node,0),"caulk_filter_blocks_pick");
    cvar_set("ftebroom_hidden_tags","0");fb_filter_prepare();fb_filter_expect(fb_pick_allowed(node,0),"caulk_filter_restores_pick");brush_select(1,caulkId);
    cvar_set("ca_newbrushtexture","ftebroom/grid");fb_filter_tag("caulk",FALSE);fb_filter_tag("detail",TRUE);fb_document_tick();fb_filter_prepare();node=fb_filter_selected_node();
    int contents;brush_get(fb_nodes[node].model,fb_nodes[node].id,__NULL__,0,&contents);fb_filter_expect((contents&134217728i)!=0,"detail_content_flag");
    brush_undo();fb_document_tick();fb_filter_prepare();node=fb_filter_selected_node();brush_get(fb_nodes[node].model,fb_nodes[node].id,__NULL__,0,&contents);fb_filter_expect(!(contents&134217728i),"detail_undo");
    int visibleId=fb_nodes[node].id;
    cvar_set("ftebroom_show_brushes","0");fb_filter_prepare();fb_filter_expect(fb_pick_allowed(node,0),"hide_brushes_keeps_selected_brush");fb_selection_clear();fb_filter_prepare();fb_filter_expect(!fb_pick_allowed(node,0),"hide_brushes_blocks_pick");cvar_set("ftebroom_show_brushes","1");brush_select(1,visibleId);
    cvar_set("ftebroom_render_faces","1");fb_filter_prepare();fb_filter_expect(fb_filter_custom(node),"flat_renderer_suppresses_original");cvar_set("ftebroom_render_faces","0");
    fb_entity_create("light",FALSE,'128 128 128');int light=fb_selected_entity();fb_document_tick();int lightNode=fb_organize_node(light);
    cvar_set("ftebroom_hidden_classes","|light|");fb_filter_prepare();fb_filter_expect(fb_pick_allowed(lightNode,0),"class_filter_keeps_selected_point");fb_selection_clear();fb_filter_prepare();fb_filter_expect(!fb_pick_allowed(lightNode,0),"class_filter_blocks_pick");cvar_set("ftebroom_hidden_classes","");fb_filter_prepare();
    fb_filter_expect(fb_pick_allowed(lightNode,0),"class_filter_restores_pick");fb_entity_select(light,TRUE);
    fb_entity_property("set","targetname","goal");fb_entity_create("target_relay",FALSE,'0 128 128');fb_entity_property("set","target","goal");fb_document_tick();fb_entity_links_prepare();
    fb_filter_expect(fb_entity_connection_count>=1,"target_connection_found");
    fb_organize_action("layernew",0,"Omitted geometry");int layer=fb_active_layer;fb_createbox('512 0 64','64 64 64');fb_organize_action("layeromit",layer,"1");fb_document_tick();
    node=fb_filter_selected_node();fb_filter_expect(!fb_export_included(node),"omit_layer_excluded_from_export");int marker=history;int revision=fb_document_revision();
    fb_filter_expect(fb_export_map(),"export_succeeds");fb_filter_expect(history==marker && fb_document_revision()==revision,"export_does_not_change_history");
    fb_organize_action("layeractive",0,"");fb_selection_clear();fb_createbox('0 -128 64','64 64 64');fb_group_create("Scope test");fb_document_tick();int group=-1;
    for(int n=2;n<fb_node_count;n++)if(fb_nodes[n].kind==FB_NODE_GROUP&&fb_nodes[n].name=="Scope test")group=n;
    fb_open_group=fb_nodes[group].id;fb_filter_prepare();fb_filter_expect(!fb_pick_allowed(lightNode,0) && fb_pick_allowed(lightNode,FB_PICK_LOCKED),"open_group_locks_outside");fb_open_group=0;
    fb_filter_prepare();fb_visibility_sync();
    fb_organize_action("layernew",0,"Structural layer");int structuralLayer=fb_active_layer;
    fb_createbox('256 -128 64','64 64 64');marker=history;fb_filter_tag("trigger",TRUE);
    fb_document_tick();int trigger=-1;for(int n=2;n<fb_node_count;n++)if(fb_nodes[n].kind==FB_NODE_ENTITY && fb_nodes[n].name=="trigger_multiple")trigger=fb_nodes[n].id;
    fb_filter_expect(trigger>0,"trigger_tag_creates_entity");brush_undo();
    fb_filter_expect(history==marker && terrain_edit(TEREDIT_ENT_GET,trigger)=="","trigger_tag_single_undo");brush_redo();
    fb_organize_action("structural",0,"");fb_document_tick();fb_filter_prepare();node=fb_filter_selected_node();
    fb_filter_expect(fb_nodes[node].parent==fb_organize_node(structuralLayer),"structural_preserves_layer");
    fb_filter_expect(!(fb_filter_tags[node]&(FB_TAG_TRIGGER|FB_TAG_DETAIL)),"structural_removes_tags");
    return TRUE;
}
