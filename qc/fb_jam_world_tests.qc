// SPDX-License-Identifier: GPL-2.0-or-later
void fb_jam_expect_rejection(int marker,int count,string operation)
{
    fb_transform_expect(history==marker && cvar("ftebroom_jam_bounds_rejected")==count+1,
        strcat("jam_bounds_blocks_",operation));
}

void fb_jam_tool_test()
{
    if(!cvar("ftebroom_skip_setup"))return;
    vector savedHalfSize=autocvar_ftebroom_jam_plot_half_size;
    cvar_set("ftebroom_jam_plot_half_size","1024 1024 512");
    fb_cancel();
    cvar_set("ftebroom_jam_world","1");
    cvar_set("ca_grid","16");
    cvar_set("ftebroom_grid_snap","1");

    int marker=history;
    fb_createbox('1008 0 128','64 64 64');
    fb_transform_expect(history==marker,"jam_bounds_blocks_brush_creation");
    int id=fb_createbox('1008 0 128','32 64 64');
    fb_transform_expect(id && history>marker,"jam_bounds_accepts_touching_the_owned_edge");

    marker=history;
    fb_move_numeric('16 0 0');
    fb_transform_expect(history==marker,"jam_bounds_blocks_move");
    id=selectedbrushes[0].id;
    static brushface_t faces[6];
    int side=0;
    brush_get(1,id,faces,6,__NULL__);
    for(int f=0;f<6;f++)if(faces[f].planenormal[0]>.99)side=f+1;
    if(side && fb_extrude_prepare(1,id,side)){
        fb_extrude_set_distance(32);
        fb_extrude_commit();
        fb_cancel();
    }
    fb_transform_expect(side && history==marker,"jam_bounds_blocks_extrude");

    int rejected=cvar("ftebroom_jam_bounds_rejected");
    fb_scale_numeric('2 1 1',FALSE);
    fb_jam_expect_rejection(marker,rejected,"scale");

    rejected=cvar("ftebroom_jam_bounds_rejected");
    fb_transform_capture();
    fb_affine_rotate('0 0 1',90,'1008 0 128');
    fb_transform_clear();
    fb_jam_expect_rejection(marker,rejected,"rotate");

    rejected=cvar("ftebroom_jam_bounds_rejected");
    fb_transform_capture();
    fb_affine_t shear={0};
    shear.x='1 0 0';shear.y='1 1 0';shear.z='0 0 1';
    fb_transform_expect(!fb_affine_apply(&shear,"Shear bounds test"),"jam_bounds_shear_reports_rejection");
    fb_transform_clear();
    fb_jam_expect_rejection(marker,rejected,"shear");

    string handles[]={"vertex","edge","face"};
    for(int mode=FB_TOOL_VERTEX;mode<=FB_TOOL_FACE;mode++){
        fb_tool_activate(mode);
        fb_vertex_rebuild();
        int handle=-1;
        for(int h=0;h<fb_vertex_count;h++)if(fb_vertex_handles[h].point[0]>=1023.99){handle=h;break;}
        rejected=cvar("ftebroom_jam_bounds_rejected");
        if(handle>=0){
            fb_vertex_handles[handle].selected=TRUE;
            if(fb_vertex_build('32 0 0',FALSE,0))fb_vertex_commit("Handle bounds test");
        }
        fb_jam_expect_rejection(marker,rejected,handles[mode-FB_TOOL_VERTEX]);
        fb_tool_deactivate();
    }

    int cap=fb_createbox('0 128 480','64 64 64');
    fb_document_tick();
    int top=0;
    brush_get(1,cap,faces,6,__NULL__);
    for(int f=0;f<6;f++)if(faces[f].planenormal[2]>.99)top=f+1;
    marker=history;
    fb_transform_expect(cap && top,"jam_bounds_cap_fixture");
    if(cap && top){
        brush_deselectall();brushface_select(1,cap,top);
        fb_tool_activate(FB_TOOL_SWEEP);
        fb_sweep_offset='0 0 32';
        cvar_set("ftebroom_sweep_segments","1");
        cvar_set("ftebroom_sweep_iterations","1");
        cvar_set("ftebroom_sweep_path","straight");
        rejected=cvar("ftebroom_jam_bounds_rejected");
        fb_sweep_perform();
        fb_jam_expect_rejection(marker,rejected,"sweep");

        fb_tool_deactivate();
        brush_deselectall();brushface_select(1,cap,top);
        fb_tool_activate(FB_TOOL_SPIN);
        rejected=cvar("ftebroom_jam_bounds_rejected");
        fb_transform_expect(!fb_spin_tool_apply(),"jam_bounds_spin_reports_rejection");
        fb_jam_expect_rejection(marker,rejected,"spin");
    }

    fb_tool_deactivate();
    fb_createbox('960 192 128','96 64 64');
    fb_document_tick();
    marker=history;
    fb_tool_activate(FB_TOOL_BEND);
    for(int point=0;point<=fb_bend_state.segments;point++)fb_bend_state.points[point]+='64 0 0';
    rejected=cvar("ftebroom_jam_bounds_rejected");
    fb_transform_expect(!fb_bend_apply(),"jam_bounds_bend_reports_rejection");
    fb_jam_expect_rejection(marker,rejected,"bend");
    fb_tool_deactivate();

    fb_createbox('-800 192 64','128 128 128');
    fb_document_tick();
    marker=history;
    fb_polygon_capture(FALSE);
    fb_polygon_test_rectangle('-800 192 0',32,128);
    cvar_set("ftebroom_polygon_depth","0");
    cvar_set("ftebroom_polygon_keep","0");
    int cut=fb_polygon_apply();
    fb_transform_expect(cut && selectedbrushcount>1 && history>marker,"jam_bounds_accepts_polygon_cut");
    if(cut)brush_undo();

    fb_selection_clear();
    marker=history;
    fb_polygon_capture(TRUE);
    fb_polygon_test_rectangle('1056 0 0',16,128);
    cvar_set("ftebroom_polygon_depth","0");
    rejected=cvar("ftebroom_jam_bounds_rejected");
    fb_transform_expect(!fb_polygon_apply(),"jam_bounds_polygon_spawn_reports_rejection");
    fb_jam_expect_rejection(marker,rejected,"polygon_spawn");

    int sculpt=fb_createbox('128 0 480','64 64 64');
    fb_document_tick();
    marker=history;
    cvar_set("ftebroom_sculpt_radius","128");
    cvar_set("ftebroom_sculpt_intensity","32");
    cvar_set("ftebroom_sculpt_mode","0");
    rejected=cvar("ftebroom_jam_bounds_rejected");
    fb_tx_begin("Sculpt bounds test");
    fb_sculpt_stroke=TRUE;
    fb_sculpt_changed=fb_sculpt_at('128 0 512','0 0 1')>0;
    fb_transform_expect(sculpt && fb_sculpt_changed,"jam_bounds_sculpt_prepares_crossing");
    fb_sculpt_finish(TRUE);
    fb_jam_expect_rejection(marker,rejected,"sculpt");

    fb_selection_clear();
    marker=history;
    fb_entity_create("light",FALSE,'1056 0 128');
    fb_transform_expect(history==marker,"jam_bounds_blocks_point_entity_creation");

    marker=history;
    fb_tx_begin("Terrain protection test");
    history_t *record=history_allocate();
    record->kind=HIST_TERRAIN;
    int allowed=fb_jam_validate_transaction(marker,history);
    history_deallocate(record);
    fb_tx_cancel();
    fb_transform_expect(!allowed,"jam_bounds_rejects_terrain_transactions");

    vector gridBasis=autocvar_ftebroom_grid_basis_x;
    fb_tool_activate(FB_TOOL_GRIDROTATE);
    tokenize("ftebroom_grid_rotate 2 30");fb_rotate_tool_command();
    fb_transform_expect(history==marker && vlen(autocvar_ftebroom_grid_basis_x-gridBasis)>.1,
        "jam_bounds_accepts_grid_rotation_without_changing_map");
    fb_grid_default();
    fb_tool_deactivate();

    cvar_set("ftebroom_jam_plot_half_size","1024 1024 1024");
    fb_transform_expect(fb_jam_point_inside('1024 1024 1024') && fb_jam_point_inside('-1024 -1024 -1024') &&
        !fb_jam_point_inside('0 0 1024.01') && !fb_jam_point_inside('0 0 -1024.01'),"jam_cubic_height_matches_horizontal_limits");
    int tall=fb_createbox('0 0 992','64 64 64');
    fb_transform_expect(tall!=0,"jam_cubic_accepts_top_at_1024");
    marker=history;rejected=cvar("ftebroom_jam_bounds_rejected");
    fb_move_numeric('0 0 16');fb_jam_expect_rejection(marker,rejected,"move_above_new_ceiling");
    fb_transform_expect(fb_createbox('0 0 -992','64 64 64')!=0,"jam_cubic_accepts_bottom_at_minus_1024");
    marker=history;rejected=cvar("ftebroom_jam_bounds_rejected");
    fb_move_numeric('0 0 -16');fb_jam_expect_rejection(marker,rejected,"move_below_new_floor");
    cvar_set("ftebroom_jam_plot_half_size",vtos(savedHalfSize));
    // A scene file may be missing or incomplete while a replacement is prepared.
    // Neither condition may discard the visible snapshot or rebuild its BSPs.
    string sceneModel="model maps/ftebroom-build/jam-scene-qa.bsp 2048 0 0 0\n";
    string sceneFace="face wall_pink_b 3\n1472 -64 64 0 0\n1472 64 64 1 0\n1472 0 128 0 1\n";
    filestream sceneFile=fopen("jam-world-scene.txt",FILE_WRITE);
    fputs(sceneFile,strcat("RBJAMSCENE 1 1001\n",sceneModel,sceneFace,"RBJAMSCENE-END 1001\n"));fclose(sceneFile);
    cvar_set("ftebroom_jam_scene","1001");fb_jam_scene_load();
    entity stableModel=fb_jam_models;
    fb_transform_expect(fb_jam_scene_seen==1001 && fb_jam_faces_count==1 && stableModel && stableModel.modelindex,
                        "jam_scene_complete_snapshot_loaded");
    cvar_set("ftebroom_jam_scene","1002");fb_jam_scene_load();
    fb_transform_expect(fb_jam_scene_seen==1001 && fb_jam_faces_count==1 && fb_jam_models==stableModel,
                        "jam_scene_stale_file_generation_keeps_geometry");
    sceneFile=fopen("jam-world-scene.txt",FILE_WRITE);
    fputs(sceneFile,strcat("RBJAMSCENE 1 1002\n",sceneModel,"face wall_pink_b 3\n1472 -64 64 0 0\n"));fclose(sceneFile);
    cvar_set("ftebroom_jam_scene","1002");fb_jam_scene_load();
    fb_transform_expect(fb_jam_scene_seen==1001 && fb_jam_faces_count==1 && fb_jam_models==stableModel,
                        "jam_scene_incomplete_replacement_keeps_geometry_and_bsp");
    fremove("jam-world-scene.txt");fb_jam_scene_load();
    fb_transform_expect(fb_jam_scene_seen==1001 && fb_jam_faces_count==1 && fb_jam_models==stableModel,
                        "jam_scene_missing_replacement_keeps_geometry_and_bsp");
    sceneFile=fopen("jam-world-scene.txt",FILE_WRITE);
    fputs(sceneFile,strcat("RBJAMSCENE 1 1002\n",sceneModel,sceneFace,sceneFace,"RBJAMSCENE-END 1002\n"));fclose(sceneFile);
    fb_jam_scene_load();
    fb_transform_expect(fb_jam_scene_seen==1002 && fb_jam_faces_count==2 && fb_jam_models==stableModel,
                        "jam_scene_complete_replacement_reuses_unchanged_bsp");
    sceneFile=fopen("jam-world-scene.txt",FILE_WRITE);
    fputs(sceneFile,"RBJAMSCENE 1 1003\nRBJAMSCENE-END 1003\n");fclose(sceneFile);
    cvar_set("ftebroom_jam_scene","1003");fb_jam_scene_load();
    fb_transform_expect(fb_jam_scene_seen==1003 && !fb_jam_faces_count && !fb_jam_models,
                        "jam_scene_explicit_empty_snapshot_removes_geometry");
    cvar_set("ftebroom_jam_scene","0");fb_jam_scene_load();
    cvar_set("ftebroom_jam_world","0");
}

// Exercise the same pointer dispatcher used by the native 3D viewport.
int fb_jam_foreign_pick_test(int requireNetworkScene)
{
    if(!cvar("ftebroom_skip_setup"))return FALSE;
    if(requireNetworkScene && (!cvar("ftebroom_jam_world") || !fb_jam_faces_count))return FALSE;
    static int ran;
    if(requireNetworkScene && ran)return TRUE;
    if(requireNetworkScene)ran=TRUE;
    float savedWorld=cvar("ftebroom_jam_world"), savedClassic=cvar("ftebroom_classic_map");
    float savedView=cvar("ca_brush_view");
    vector savedOrigin=getproperty(VF_ORIGIN), savedAngles=getproperty(VF_ANGLES);
    cvar_set("ftebroom_jam_world","1");
    cvar_set("ftebroom_classic_map","1");
    cvar_set("ca_brush_view","0");
    if(!requireNetworkScene) {
        filestream file=fopen("jam-world-scene.txt",FILE_WRITE);
        if((float)file<0)return FALSE;
        fputs(file,"RBJAMSCENE 1 129\nface wall_pink_b 4\n1472 -64 64 0 0\n1472 64 64 1 0\n1472 64 192 1 1\n1472 -64 192 0 1\nRBJAMSCENE-END 129\n");
        fclose(file);
        cvar_set("ftebroom_jam_scene","129");
        fb_jam_scene_load();
    }
    vector aim='0 0 0';
    int surface=-1;
    for(int f=0;f<fb_jam_faces_count && surface<0;f++) {
        fb_jam_face_t *face=&fb_jam_faces[f];
        vector center='0 0 0';
        for(int v=0;v<face->count;v++)center+=fb_jam_vertices[face->first+v];
        center=center/(float)face->count;
        if(fabs(center_x)>1250 || fabs(center_y)>1250){aim=center;surface=f;}
    }
    fb_transform_expect(surface>=0,"jam_remote_render_geometry_loaded");
    if(surface>=0){
        vector eye=aim-'144 0 -48';
        if(aim_x< -1250)eye=aim+'-144 0 48';
        setproperty(VF_PERSPECTIVE,TRUE);
        setproperty(VF_ORIGIN,eye);
        setproperty(VF_ANGLES,vectoangles(aim-eye));
        fb_view_prepare();
        vector pixel=project(aim);pixel_z=0;
        vector end=pixel+'21 24 0';
        fb_selection_clear();
        fb_document_tick();
        int picked=fb_pick_query(unproject(pixel),unproject(pixel+'0 0 8192'),pixel,0,
                                 FB_HIT_BRUSH|FB_HIT_PATCH|FB_HIT_ENTITY,FALSE);
        fb_transform_expect(!picked,"jam_remote_geometry_absent_from_pick_index");
        int geometry=fb_document_revision();
        string names[]={"move","brush","clip","rotate","sweep","scale","shear","vertex","edge","face",
                        "controlpoint","sculpt","bend","polygon_cut","polygon_spawn","spin","rotate_grid","plant_brush"};
        for(int tool=FB_TOOL_NONE;tool<=FB_TOOL_PLANTBRUSH;tool++){
            int previewCount=fb_jam_faces_count;
            fb_tool_deactivate();
            fb_selection_clear();
            if(tool==FB_TOOL_CONTROLPOINT || tool==FB_TOOL_PLANTBRUSH){
                fb_transform_expect(!fb_tool_can_activate(tool),strcat("jam_unsupported_",names[tool]));
                continue;
            }
            fb_tool_activate(tool);
            fb_tool_pointer_down(pixel,0,0);fb_tool_pointer_up(pixel,0,0);
            fb_tool_pointer_down(pixel,0,0);fb_tool_pointer_move(end,0,0);fb_tool_pointer_up(end,0,0);
            int valid=fb_document_revision()==geometry && !selectedbrushcount &&
                !fb_selected_entity_count && fb_jam_faces_count==previewCount;
            fb_transform_expect(valid,strcat("jam_foreign_",names[tool]));
        }
    }
    fb_tool_deactivate();
    if(!requireNetworkScene){cvar_set("ftebroom_jam_scene","0");fb_jam_scene_load();}
    cvar_set("ftebroom_jam_world",ftos(savedWorld));
    cvar_set("ftebroom_classic_map",ftos(savedClassic));
    cvar_set("ca_brush_view",ftos(savedView));
    setproperty(VF_ORIGIN,savedOrigin);setproperty(VF_ANGLES,savedAngles);
    fb_view_prepare();
    if(requireNetworkScene && surface>=0)cvar_set("ftebroom_jam_qa_foreign_done","1");
    return surface>=0;
}

void fb_jam_hub_test(int capture)
{
    if(!cvar("ftebroom_skip_setup") || !cvar("ftebroom_jam_world"))return;
    fb_jam_scene_load();
    vector owned=stov(cvar_string("ftebroom_jam_plot_center"));
    vector center='4096 -4096 0'-owned;
    entity hub=world;
    for(entity item=fb_jam_models;item;item=item.fb_jam_next){
        if(strstrofs(item.model,"maps/ftebroom-build/jam-hub-",0)==0){hub=item;break;}
    }
    if(!hub)return;
    if(capture){
        if(owned=='0 0 0'){
            fb_camera_position(center+'0 -1500 710');
            setproperty(VF_CL_VIEWANGLES,'23 90 0');
        }else{
            fb_camera_position(center+'0 -370 132');
            setproperty(VF_CL_VIEWANGLES,'0 90 0');
        }
        return;
    }
    vector aim=center+'0 755 70', eye=center+'0 0 132';
    vector oldOrigin=getproperty(VF_ORIGIN), oldAngles=getproperty(VF_ANGLES);
    setproperty(VF_ORIGIN,eye);
    setproperty(VF_ANGLES,vectoangles(aim-eye));
    fb_view_prepare();
    vector pixel=project(aim);pixel_z=0;
    fb_tool_deactivate();
    fb_selection_clear();
    fb_document_tick();
    int revision=fb_document_revision();
    int picked=fb_pick_query(eye,aim,pixel,0,FB_HIT_BRUSH|FB_HIT_PATCH|FB_HIT_ENTITY,FALSE);
    fb_tool_pointer_down(pixel,0,0);
    fb_tool_pointer_move(pixel+'26 24 0',0,0);
    fb_tool_pointer_up(pixel+'26 24 0',0,0);
    int visible=hub.modelindex && hub.origin==center && hub.solid==SOLID_NOT;
    int locked=!picked && !selectedbrushcount && !fb_selected_entity_count && revision==fb_document_revision();
    fb_transform_expect(visible && locked,"jam_hub_render_only");
    cvar_set("ftebroom_jam_qa_hub",(visible && locked)?"1":"-1");
    setproperty(VF_ORIGIN,oldOrigin);
    setproperty(VF_ANGLES,oldAngles);
    fb_view_prepare();
}
