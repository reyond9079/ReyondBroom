// SPDX-License-Identifier: GPL-2.0-or-later
float fb_extrude_pick_testcommand()
{
    if(argv(0)=="ftebroom_extrude_pick") {
        vector queryPixel=[stof(argv(1)),stof(argv(2)),0];
        if(fb_extrude_pick(queryPixel,cvar("ca_brush_view")))print(sprintf("EXTRUDE_PICK brush=%i face=%i support=%s normal=%v handle=%v\n",fb_hits[0].id,fb_hits[0].face,fb_extrude_plane_support?"plane":"line",fb_hits[0].normal,fb_hits[0].point));
        else print("EXTRUDE_PICK none\n");return TRUE;
    }
    if(argv(0)=="ftebroom_test_extrude_benchmark" && cvar("ftebroom_skip_setup")) {
        fb_document_tick();fb_extrude_cache();
        vector cursor='400 300 0';float startTime=gettime(1);
        for(int iteration=0;iteration<50;iteration++){cursor_x=400+iteration;fb_extrude_pick(cursor,0);}
        print(sprintf("EXTRUDE_BENCHMARK selected=%i milliseconds=%g\n",selectedbrushcount,(gettime(1)-startTime)*1000/50));return TRUE;
    }
    if(argv(0)!="ftebroom_test_extrude_pick" || !cvar("ftebroom_skip_setup"))return FALSE;
    fb_cancel();fb_selection_clear();int id=fb_createbox('0 0 128','64 64 64');fb_document_tick();
    setproperty(VF_MIN,'0 0 0');setproperty(VF_SIZE,'800 600 0');setproperty(VF_PERSPECTIVE,TRUE);
    setproperty(VF_ORIGIN,'256 -256 256');setproperty(VF_ANGLES,'20 135 0');setproperty(VF_FOV,'90 70 0');
    vector pixel=project('0 0 160');pixel_z=0;
    int found=fb_extrude_pick(pixel,0);
    fb_transform_expect(found && !fb_extrude_plane_support && fb_hits[0].normal[2]>0.99,"pick_direct_top_line");
    cvar_set("ftebroom_shift","1");cvar_set("ftebroom_camera_captured","0");
    cvar_set("ftebroom_mouse_x",ftos(pixel_x));cvar_set("ftebroom_mouse_y",ftos(pixel_y));
    fb_extrude_draw();
    fb_transform_expect(fb_extrude_outline_count==8,"hover_face_four_yellow_edges");
    vector *cachedOutline=fb_extrude_outline;vector firstOutline=fb_extrude_outline[0];
    fb_extrude_draw();
    fb_transform_expect(fb_extrude_outline==cachedOutline && fb_extrude_outline[0]==firstOutline && fb_extrude_outline_count==8,"hover_outline_reuses_storage");
    cvar_set("ftebroom_shift","0");
    float classic=cvar("ftebroom_classic_map");cvar_set("ftebroom_classic_map","1");
    fb_transform_expect(!fb_tool_can_activate(FB_TOOL_CONTROLPOINT) && fb_tool_can_activate(FB_TOOL_VERTEX),"quake1_no_patch_control_tool");
    cvar_set("ftebroom_classic_map",ftos(classic));
    pixel=project('0 -32 96');pixel_y+=40;pixel_z=0;found=fb_extrude_pick(pixel,0);
    fb_transform_expect(found && fb_extrude_plane_support && fb_hits[0].normal[2]< -0.99,"pick_lower_silhouette_bottom");
    pixel_x+=300;found=fb_extrude_pick(pixel,0);fb_transform_expect(found && fb_extrude_plane_support,"pick_far_cursor_has_no_pixel_cutoff");
    cvar_set("ca_brush_view","3");cvar_set("ca_brush_viewsize","512");fb_view_centers[3]='0 0 128';fb_view_prepare();
    pixel=project('64 0 128');pixel_z=0;found=fb_extrude_pick(pixel,3);
    fb_transform_expect(found && fb_extrude_plane_support && fb_hits[0].normal[0]>0.99,"pick_xy_right_plane");
    if(found) {
        fb_extrude_anchor=fb_hits[0].point;fb_extrude_prepare(1,id,fb_hits[0].face);
        float distance=0;fb_extrude_ray_distance(pixel,&fb_extrude_start_distance);pixel_x+=50;fb_extrude_ray_distance(pixel,&distance);
        fb_transform_expect(distance-fb_extrude_start_distance>0,"pick_xy_drag_outward");fb_cancel();
    }
    pixel=project('0 0 128');pixel_z=0;fb_transform_expect(!fb_extrude_pick(pixel,3),"pick_xy_inside_has_no_face");
    int insideEdge=TRUE;
    for(int view=1;view<=3;view++) {
        for(int zoom=128;zoom<=4096;zoom*=4) {
            cvar_set("ca_brush_view",itos(view));cvar_set("ca_brush_viewsize",itos(zoom));
            fb_view_centers[view]='0 0 128';fb_view_prepare();
            pixel=project('0 0 128'+fb_view_right*32);pixel_z=0;pixel_x-=1;
            if(!fb_extrude_pick(pixel,view) || !fb_extrude_plane_support || fb_hits[0].normal*fb_view_right<0.99)
                insideEdge=FALSE;
        }
    }
    fb_transform_expect(insideEdge,"pick_ortho_near_edge_from_inside_at_all_zooms");
    cvar_set("ca_brush_view","0");
    fb_selection_clear();int a=fb_createbox('-32 0 128','64 64 64');fb_createbox('32 0 128','64 64 64');brush_select(1,a);fb_document_tick();
    fb_extrude_cache();int seamCount=0;for(int e=0;e<fb_extrude_edge_count;e++)if(fb_extrude_seam(e))seamCount++;
    fb_transform_expect(seamCount==8,"pick_two_cubes_shared_seam_edges");
    cvar_set("ca_brush_view","3");fb_view_prepare();pixel=project('0 0 128');pixel_z=0;
    found=fb_extrude_pick(pixel,3);
    fb_transform_expect(found && fb_extrude_plane_support && fabs(fb_hits[0].normal[0])>0.99,"pick_seam_overrides_direct_face");
    if(found) {
        fb_extrude_prepare(fb_hits[0].model,fb_hits[0].id,fb_hits[0].face);int seamFaces=0;
        for(int i=0;i<fb_transform_count;i++)if(fb_extrude_faces[i]>=0)seamFaces++;
        fb_transform_expect(seamFaces==2,"pick_seam_moves_both_opposing_faces");fb_cancel();
    }
    cvar_set("ca_brush_view","0");
    fb_selection_clear();brush_select(1,a);fb_createbox('256 0 128','64 64 64');brush_select(1,a);
    static brushface_t faces[6];brush_get(1,a,faces,6,__NULL__);int top=0;for(int f=0;f<6;f++)if(faces[f].planenormal[2]>0.99)top=f+1;
    fb_extrude_prepare(1,a,top);int moving=0;for(int i=0;i<fb_transform_count;i++)if(fb_extrude_faces[i]>=0)moving++;
    fb_transform_expect(moving==2,"pick_disconnected_coplanar_faces_move_together");fb_cancel();
    return TRUE;
}
