// SPDX-License-Identifier: GPL-2.0-or-later
void fb_viewopts_expect(int condition,string name)
{
    print(strcat(condition?"FTEBROOM_VIEWOPTS_PASS ":"FTEBROOM_VIEWOPTS_FAIL ",name,"\n"));
}
int fb_viewopts_picks(int id,int patch)
{
    vector point=patch?'256 0 512':'0 0 512';
    int count=fb_pick_query(point+'0 0 128',point-'0 0 128',project(point),0,patch?FB_HIT_PATCH:FB_HIT_BRUSH,FALSE);
    for(int h=0;h<count;h++)if(fb_hits[h].id==id)return TRUE;
    return FALSE;
}
int fb_viewopts_face_count(int node)
{
    int count=0;for(int f=0;f<fb_render_face_count;f++)if(fb_render_faces[f].node==node)count++;return count;
}
int fb_viewopts_grid_fill(int node,int hiddenTop)
{
    int count=0,hidden=0;
    for(int f=0;f<fb_grid_face_count;f++)if(fb_grid_faces[f].node==node){
        fb_grid_face_t *face=&fb_grid_faces[f];count++;
        if(face->occluder&4){hidden++;if(face->normal[2]<.9 || (face->occluder&3)){print(sprintf("VIEWOPTS_GRID_DIAG unexpected hidden plane normal=%v flags=%i\n",face->normal,face->occluder));return FALSE;}}
        for(int v=0;v<face->count;v++){
            float expected=(hiddenTop && face->normal[2]>.9)?0:.7;
            if(fabs(face->vertices[v].rgba[3]-expected)>.00001 || fabs(face->vertices[v+face->count].rgba[3]-1)>.00001){print(sprintf("VIEWOPTS_GRID_DIAG normal=%v flags=%i alpha=%g edge=%g expected=%g\n",face->normal,face->occluder,face->vertices[v].rgba[3],face->vertices[v+face->count].rgba[3],expected));return FALSE;}
        }
    }
    if(count!=6 || hidden!=hiddenTop)print(sprintf("VIEWOPTS_GRID_DIAG count=%i hidden=%i expected=%i\n",count,hidden,hiddenTop));return count==6 && hidden==hiddenTop;
}
int fb_viewopts_ray_has(int id,vector from,vector to)
{
    int count=fb_pick_query(from,to,project((from+to)*.5),0,FB_HIT_BRUSH,FALSE);
    for(int h=0;h<count;h++)if(fb_hits[h].id==id)return TRUE;return FALSE;
}
void fb_viewopts_tag_tests()
{
    float savedMask=cvar("ftebroom_hidden_tags"),savedClassic=cvar("ftebroom_classic_map"),savedAlpha=cvar("ftebroom_grid_alpha"),savedGrid=cvar("ca_grid"),savedView=cvar("ca_brush_view"),savedFaces=cvar("ftebroom_render_faces"),savedBrushes=cvar("ftebroom_show_brushes");
    fb_tool_deactivate();fb_selection_clear();cvar_set("ftebroom_show_brushes","1");cvar_set("ftebroom_classic_map","1");cvar_set("ca_brush_view","0");cvar_set("ftebroom_render_faces","0");cvar_set("ftebroom_grid_alpha","0.7");cvar_set("ftebroom_hidden_tags","4");
    int brush=fb_createbox('0 768 256','128 128 128');static brushface_t faces[6];int contents;
    brush_get(1,brush,faces,6,&contents);for(int f=0;f<6;f++)faces[f].shadername=f==5?"common/caulk":"ftebroom/grid";
    fb_tx_begin("Partial face tag fixture");brush=brush_history_edit(1,brush,faces,6,contents);fb_tx_commit();fb_selection_clear();
    fb_document_tick();fb_filter_prepare();int node=fb_document_brush_node(1,brush);
    fb_render_faces_prepare();fb_surfacegrid(FALSE);
    fb_viewopts_expect(fb_render_allowed(node,FB_PICK_LOCKED) && fb_viewopts_face_count(node)==5,"one_hidden_face_keeps_brush_and_five_material_faces");
    fb_viewopts_expect(fb_viewopts_grid_fill(node,TRUE),"hidden_face_grid_fill_and_occlusion_removed_edges_retained");
    fb_viewopts_expect(!fb_viewopts_ray_has(brush,'0 768 512','0 768 0') && fb_viewopts_ray_has(brush,'256 768 256','-256 768 256'),"hidden_face_blocks_pick_visible_face_remains_pickable");
    static vector params[6];params[0]=autocvar_ftebroom_grid_origin;params[1]=autocvar_ftebroom_grid_basis_x;params[2]=autocvar_ftebroom_grid_basis_y;params[3]=autocvar_ftebroom_grid_basis_z;params[4]=[fb_grid(),.7,0];params[5]='1 0 0';
    if(checkbuiltin(ftebroom_build_grid)){
        int built=ftebroom_build_grid(fb_grid_faces,fb_grid_face_count,params);fb_grid_filter_alpha(fb_grid_faces,fb_grid_face_count);
        fb_viewopts_expect(built && fb_viewopts_grid_fill(node,TRUE),"native_grid_builder_reapplies_hidden_alpha_without_hiding_edges");
    }
    cvar_set("ca_grid","4");fb_surfacegrid(FALSE);cvar_set("ca_grid","32");fb_surfacegrid(FALSE);
    fb_viewopts_expect(fb_viewopts_grid_fill(node,TRUE),"grid_step_changes_keep_hidden_fill_transparent");
    brush_select(1,brush);fb_filter_prepare();fb_render_faces_prepare();fb_surfacegrid(FALSE);
    fb_viewopts_expect(fb_viewopts_face_count(node)==6 && fb_viewopts_grid_fill(node,FALSE),"selected_whole_brush_restores_all_face_fill_and_grid");
    fb_faces_click(1,brush,6,FALSE);fb_filter_prepare();fb_render_faces_prepare();
    fb_viewopts_expect(fb_viewopts_face_count(node)==6,"individually_selected_hidden_face_is_rendered");
    fb_faces_click(1,brush,1,FALSE);fb_filter_prepare();fb_render_faces_prepare();
    fb_viewopts_expect(fb_viewopts_face_count(node)==5,"selecting_another_face_does_not_unhide_tagged_face");
    int faceEpoch=fb_selection_epoch;brushface_select(1,brush,6);fb_filter_prepare();fb_render_faces_prepare();
    fb_viewopts_expect(fb_selection_epoch>faceEpoch && fb_viewopts_face_count(node)==6,"additive_face_selection_invalidates_visibility_cache");
    faceEpoch=fb_selection_epoch;brushface_select(1,brush,6);
    fb_viewopts_expect(fb_selection_epoch==faceEpoch,"repeated_face_paint_sample_keeps_selection_epoch_stable");
    faceEpoch=fb_selection_epoch;brushface_deselect(1,brush,6);fb_filter_prepare();fb_render_faces_prepare();
    fb_viewopts_expect(fb_selection_epoch>faceEpoch && fb_viewopts_face_count(node)==5,"face_toggle_invalidates_visibility_cache");
    faceEpoch=fb_selection_epoch;brushface_deselect(1,brush,6);
    fb_viewopts_expect(fb_selection_epoch==faceEpoch,"repeated_face_deselect_keeps_selection_epoch_stable");
    brush_get(1,brush,faces,6,&contents);for(int face=0;face<6;face++)faces[face].shadername=face<3?"common/caulk":"common/hint";
    fb_tx_begin("Every face matches a hidden tag");brush=brush_history_edit(1,brush,faces,6,contents);fb_tx_commit();fb_selection_clear();
    cvar_set("ftebroom_hidden_tags","12");fb_document_tick();fb_filter_prepare();node=fb_document_brush_node(1,brush);fb_render_faces_prepare();
    fb_viewopts_expect(!fb_render_allowed(node,FB_PICK_LOCKED) && fb_viewopts_face_count(node)==0,"different_hidden_tags_on_all_faces_hide_whole_brush");
    brush_select(1,brush);fb_filter_prepare();fb_render_faces_prepare();fb_viewopts_expect(fb_render_allowed(node,FB_PICK_LOCKED) && fb_viewopts_face_count(node)==6,"selected_brush_overrides_all_hidden_face_tags");
    fb_incremental_settings();int geometry=fb_grid_geometry_epoch;fb_incremental_settings();
    fb_viewopts_expect(fb_grid_geometry_epoch==geometry,"unchanged_filter_and_selection_do_not_rebuild_geometry_each_frame");
    fb_selection_clear();cvar_set("ftebroom_hidden_tags",ftos(savedMask));cvar_set("ftebroom_classic_map",ftos(savedClassic));cvar_set("ftebroom_grid_alpha",ftos(savedAlpha));cvar_set("ca_grid",ftos(savedGrid));cvar_set("ca_brush_view",ftos(savedView));cvar_set("ftebroom_render_faces",ftos(savedFaces));cvar_set("ftebroom_show_brushes",ftos(savedBrushes));fb_filter_prepare();
}
float fb_viewopts_testcommand()
{
    if(argv(0)!="ftebroom_test_view_options132")return FALSE;
    if(!cvar("ftebroom_skip_setup"))return TRUE;
    if(argv(1)=="tags"){fb_viewopts_tag_tests();return TRUE;}
    float brushes=cvar("ftebroom_show_brushes"),patches=cvar("ftebroom_show_patches"),tags=cvar("ftebroom_hidden_tags"),classic=cvar("ftebroom_classic_map");
    string classes=strzone(cvar_string("ftebroom_hidden_classes"));
    fb_tool_deactivate();fb_selection_clear();
    cvar_set("ftebroom_show_brushes","1");cvar_set("ftebroom_show_patches","1");cvar_set("ftebroom_hidden_tags","0");cvar_set("ftebroom_hidden_classes","");cvar_set("ftebroom_classic_map","0");
    int brush=fb_createbox('0 0 512','64 64 64');
    static patchvert_t cp[9];
    for(int y=0;y<3;y++)for(int x=0;x<3;x++){
        cp[y*3+x].xyz=[224+x*32,-32+y*32,512];cp[y*3+x].s=x;cp[y*3+x].t=y;cp[y*3+x].rgb='1 1 1';cp[y*3+x].a=1;
    }
    patchinfo_t info={0};info.cpwidth=info.cpheight=3;info.tesswidth=info.tessheight=4;info.shadername="ftebroom/grid";info.contents=CONTENTBIT_SOLID;
    fb_tx_begin("Independent visibility fixture");int patch=patch_history_create(1,cp,info,FALSE);fb_tx_commit();fb_selection_clear();
    fb_document_tick();fb_filter_prepare();
    int brushNode=fb_document_brush_node(1,brush),patchNode=fb_document_brush_node(1,patch);
    if(brushNode<2 || patchNode<2){fb_viewopts_expect(FALSE,"brush_and_patch_fixture");return TRUE;}
    fb_viewopts_expect(fb_render_allowed(brushNode,FB_PICK_LOCKED) && fb_render_allowed(patchNode,FB_PICK_LOCKED) && fb_viewopts_picks(brush,FALSE) && fb_viewopts_picks(patch,TRUE),"brush_and_patch_visible_and_pickable");
    cvar_set("ftebroom_show_brushes","0");fb_filter_prepare();fb_visibility_sync();
    fb_viewopts_expect(!fb_render_allowed(brushNode,FB_PICK_LOCKED) && fb_render_allowed(patchNode,FB_PICK_LOCKED) && !fb_viewopts_picks(brush,FALSE) && fb_viewopts_picks(patch,TRUE),"hide_brushes_preserves_patch_render_and_pick");
    cvar_set("ftebroom_show_brushes","1");cvar_set("ftebroom_show_patches","0");fb_filter_prepare();fb_visibility_sync();
    fb_viewopts_expect(fb_render_allowed(brushNode,FB_PICK_LOCKED) && !fb_render_allowed(patchNode,FB_PICK_LOCKED) && fb_viewopts_picks(brush,FALSE) && !fb_viewopts_picks(patch,TRUE),"hide_patches_preserves_brush_render_and_pick");
    cvar_set("ftebroom_show_brushes","0");fb_filter_prepare();
    fb_viewopts_expect(!fb_render_allowed(brushNode,FB_PICK_LOCKED) && !fb_render_allowed(patchNode,FB_PICK_LOCKED) && !fb_viewopts_picks(brush,FALSE) && !fb_viewopts_picks(patch,TRUE),"both_visibility_switches_hide_their_geometry");
    brush_select(1,brush);fb_filter_prepare();
    fb_viewopts_expect(fb_render_allowed(brushNode,FB_PICK_LOCKED) && !fb_render_allowed(patchNode,FB_PICK_LOCKED) && fb_viewopts_picks(brush,FALSE),"selected_brush_remains_visible_when_brushes_hidden");
    fb_selection_clear();brush_select(1,patch);fb_filter_prepare();
    fb_viewopts_expect(!fb_render_allowed(brushNode,FB_PICK_LOCKED) && fb_render_allowed(patchNode,FB_PICK_LOCKED) && fb_viewopts_picks(patch,TRUE),"selected_patch_remains_visible_when_patches_hidden");fb_selection_clear();
    cvar_set("ftebroom_show_patches","1");cvar_set("ftebroom_show_brushes","1");fb_filter_prepare();
    fb_viewopts_expect(fb_render_allowed(brushNode,FB_PICK_LOCKED) && fb_render_allowed(patchNode,FB_PICK_LOCKED) && fb_viewopts_picks(brush,FALSE) && fb_viewopts_picks(patch,TRUE),"visibility_switches_rebuild_render_and_pick_access");
    int wholeEpoch=fb_selection_epoch;brush_select(1,brush);brush_select(1,patch);
    fb_viewopts_expect(fb_selection_epoch==wholeEpoch+2 && brush_selection_index_valid && brush_isselected(1,brush)==1 && brush_isselected(1,patch)==2,"bulk_additive_selection_keeps_hash_valid_and_updates_epoch");fb_selection_clear();
    fb_entity_create("light",FALSE,'768 0 512');int light=fb_selected_entity();fb_document_tick();int lightNode=fb_organize_node(light);
    float points=cvar("ftebroom_show_points");cvar_set("ftebroom_show_points","0");cvar_set("ftebroom_hidden_classes","|light|");fb_filter_prepare();fb_point_render_prepare();int selectedLabel=FALSE;
    for(int p=0;p<fb_point_render_count;p++)if(fb_point_render[p].id==light)selectedLabel=TRUE;
    fb_viewopts_expect(fb_render_allowed(lightNode,FB_PICK_LOCKED) && selectedLabel,"selected_hidden_class_keeps_point_and_label_entry");
    fb_selection_clear();fb_filter_prepare();fb_point_render_prepare();int hiddenLabel=FALSE;
    for(int q=0;q<fb_point_render_count;q++)if(fb_point_render[q].id==light)hiddenLabel=TRUE;
    fb_viewopts_expect(!fb_render_allowed(lightNode,FB_PICK_LOCKED) && !hiddenLabel,"deselected_hidden_class_removes_point_and_label_entry");cvar_set("ftebroom_show_points",ftos(points));cvar_set("ftebroom_hidden_classes","");fb_filter_prepare();
    cvar_set("ftebroom_isolate_entity_classes","1");fb_filter_prepare();
    fb_viewopts_expect(!fb_render_allowed(lightNode,FB_PICK_LOCKED),"hide_all_isolates_no_classes");
    cvar_set("ftebroom_hidden_classes","|light|");fb_filter_prepare();
    fb_viewopts_expect(fb_render_allowed(lightNode,FB_PICK_LOCKED),"one_class_visible_after_hide_all");
    cvar_set("ftebroom_isolate_entity_classes","0");cvar_set("ftebroom_hidden_classes","");
    float old2d=cvar("ftebroom_show_entity_names_2d");
    float names=cvar("ftebroom_show_entity_names"),bounds=cvar("ftebroom_show_brush_entity_bounds"),groups=cvar("ftebroom_show_group_bounds");
    cvar_set("ftebroom_show_entity_names","1");cvar_set("ftebroom_show_entity_names_2d","0");
    fb_viewopts_expect(fb_entity_classnames_visible(FALSE)&&!fb_entity_classnames_visible(TRUE),"entity_names_in_3d_only");
    cvar_set("ftebroom_show_entity_names_2d",ftos(old2d));
    cvar_set("ftebroom_show_entity_names","0");cvar_set("ftebroom_show_brush_entity_bounds","1");cvar_set("ftebroom_show_group_bounds","1");
    fb_viewopts_expect(!fb_container_display_allowed(FALSE,TRUE) && fb_container_display_allowed(FALSE,FALSE),"entity_classname_toggle_keeps_bounds");
    fb_viewopts_expect(fb_container_display_allowed(TRUE,TRUE) && fb_container_display_allowed(TRUE,FALSE),"group_names_and_bounds_independent_of_entity_classnames");
    cvar_set("ftebroom_show_entity_names","1");cvar_set("ftebroom_show_brush_entity_bounds","0");cvar_set("ftebroom_show_group_bounds","0");
    fb_viewopts_expect(fb_container_display_allowed(FALSE,TRUE) && !fb_container_display_allowed(FALSE,FALSE),"entity_bounds_toggle_keeps_classnames");
    fb_viewopts_expect(!fb_container_display_allowed(TRUE,TRUE) && !fb_container_display_allowed(TRUE,FALSE),"group_toggle_hides_both_group_names_and_bounds");
    cvar_set("ftebroom_show_entity_names",ftos(names));cvar_set("ftebroom_show_brush_entity_bounds",ftos(bounds));cvar_set("ftebroom_show_group_bounds",ftos(groups));
    cvar_set("ftebroom_show_brushes",ftos(brushes));cvar_set("ftebroom_show_patches",ftos(patches));cvar_set("ftebroom_hidden_tags",ftos(tags));cvar_set("ftebroom_hidden_classes",classes);strunzone(classes);cvar_set("ftebroom_classic_map",ftos(classic));fb_filter_prepare();fb_visibility_sync();
    return TRUE;
}
