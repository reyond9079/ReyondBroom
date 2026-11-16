// SPDX-License-Identifier: GPL-2.0-or-later
float fb_affine_testcommand()
{
    if(argv(0)!="ftebroom_test_affine" || !cvar("ftebroom_skip_setup")) return FALSE;
    if(argv(1)=="geometry") {
        fb_selection_clear();int id=fb_createbox('128 32 64','64 32 48');
        static brushface_t original[6],faces[6];int contents;brush_get(1,id,original,6,&contents);
        cvar_set("ftebroom_texture_lock","1");
        fb_transform_expect(fb_transform_capture(),"affine_capture");
        fb_affine_rotate('0 0 1',37,'0 0 0');fb_transform_clear();
        fb_transform_capture();fb_affine_rotate('0 0 1',-37,'0 0 0');fb_transform_clear();
        brush_get(1,selectedbrushes[0].id,faces,6,&contents);int same=TRUE;
        for(int f=0;f<6;f++) if(vlen(faces[f].planenormal-original[f].planenormal)>0.0001 || fabs(faces[f].planedist-original[f].planedist)>0.001 || vlen(faces[f].sdir-original[f].sdir)>0.0001 || vlen(faces[f].tdir-original[f].tdir)>0.0001) same=FALSE;
        fb_transform_expect(same,"rotate_37_roundtrip_planes_uv");
        fb_affine_t m={0};m.x='2 0 0';m.y='0.5 3 0';m.z='0 0 0.5';m.translation='13 7 -9';
        fb_transform_expect(fb_affine_prepare(&m),"prepare_nonsingular_affine");
        brushface_t face=original[0];vector point=face.planenormal*face.planedist;
        float u=face.sdir*point+face.sbias,v=face.tdir*point+face.tbias;
        fb_affine_face(&m,&face,TRUE);vector transformed=fb_affine_point(&m,point);
        fb_transform_expect(fabs(face.sdir*transformed+face.sbias-u)<0.0001 && fabs(face.tdir*transformed+face.tbias-v)<0.0001,"inverse_transpose_uv_invariant");
        m.x='-1 0 0';m.y='0 1 0';m.z='0 0 1';m.translation='0 0 0';
        fb_transform_capture();int before=history;
        fb_transform_expect(fb_affine_apply(&m,"Flip"),"reflection_valid");fb_transform_clear();
        fb_poly_t poly={0};fb_poly_from_brush(1,selectedbrushes[0].id,&poly);
        fb_transform_expect(fabs(poly.volume-64*32*48)<1,"reflection_preserves_volume_outward_faces");fb_poly_free(&poly);
        brush_undo();fb_transform_expect(history==before,"reflection_one_undo");
        fb_transform_capture();m.x='0 0 0';
        fb_transform_expect(!fb_affine_apply(&m,"Invalid") && history==before,"singular_atomic_rejection");fb_transform_clear();
        fb_transform_capture();m.x='1 0 0';m.translation='65536 0 0';
        fb_transform_expect(!fb_affine_apply(&m,"Invalid") && history==before,"outside_world_atomic_rejection");fb_transform_clear();
    } else if(argv(1)=="entity") {
        fb_document_tick();fb_selection_clear();int entityId=-1;
        for(int n=0;n<fb_node_count;n++) if(fb_nodes[n].kind==FB_NODE_ENTITY && fb_selection_leaf(n)) {entityId=fb_nodes[n].id;break;}
        fb_entity_select(entityId,TRUE);vector entityBefore=fb_entity_origin(entityId);int marker=history;
        fb_transform_capture();fb_affine_rotate('0 0 1',90,'0 0 0');fb_transform_clear();
        fb_transform_expect(vlen(fb_entity_origin(entityId)-[-entityBefore_y,entityBefore_x,entityBefore_z])<0.001,"entity_rotates_about_pivot");
        string angle=fb_clip_field(terrain_edit(TEREDIT_ENT_GET,entityId),"angle");
        fb_transform_expect(fabs(fabs(stof(angle))-180)<0.001,"entity_yaw_rotates");
        brush_undo();fb_transform_expect(history==marker && vlen(fb_entity_origin(entityId)-entityBefore)<0.001,"entity_rotation_one_undo");
        fb_tx_begin("Orientation fixture");
        fb_tx_set_key(entityId,"classname","light",HIST_ENTITY);
        fb_tx_set_key(entityId,"mangle","25 15 0",HIST_ENTITY);
        fb_tx_commit();
        fb_transform_capture();fb_affine_rotate('0 0 1',90,'0 0 0');fb_transform_clear();
        vector lightAngles=stov(fb_clip_field(terrain_edit(TEREDIT_ENT_GET,entityId),"mangle"));
        fb_transform_expect(fabs(lightAngles_x-115)<0.01 && fabs(lightAngles_y-15)<0.01,"light_mangle_yaw_pitch_order");
        brush_undo();brush_undo();
        fb_tx_begin("Missing angle fixture");fb_tx_set_key(entityId,"angle","",HIST_ENTITY);fb_tx_commit();
        fb_transform_capture();fb_affine_rotate('0 0 1',90,'0 0 0');fb_transform_clear();
        fb_transform_expect(fabs(stof(fb_clip_field(terrain_edit(TEREDIT_ENT_GET,entityId),"angle"))-90)<0.001,"missing_angle_created_for_z_rotation");
        brush_undo();brush_undo();
        fb_tx_begin("Vertical angle fixture");fb_tx_set_key(entityId,"angle","-1",HIST_ENTITY);fb_tx_commit();
        fb_transform_capture();fb_affine_rotate('0 0 1',90,'0 0 0');fb_transform_clear();
        fb_transform_expect(fb_clip_field(terrain_edit(TEREDIT_ENT_GET,entityId),"angle")=="-1","vertical_angle_preserved_about_z");
        brush_undo();brush_undo();
        fb_selection_clear();
        fb_entity_create("item_artifact_lava_nails",FALSE,'-224 992 24');int lava=fb_selected_entity();
        fb_entity_create("item_artifact_sharp_shooter",FALSE,'224 976 24');int sharp=fb_selected_entity();
        fb_entity_select(lava,TRUE);fb_document_tick();
        fb_transform_capture();fb_affine_rotate('0 0 1',90,'-224 992 24');fb_transform_clear();
        fb_transform_expect(fb_entity_origin(lava)=='-224 992 24' && fb_entity_origin(sharp)=='-208 1440 24',"artifact_positions_follow_rotation");
        fb_transform_expect(fb_clip_field(terrain_edit(TEREDIT_ENT_GET,lava),"angle")=="" && fb_clip_field(terrain_edit(TEREDIT_ENT_GET,sharp),"angle")=="","artifacts_keep_implicit_orientation");
        brush_undo();
        fb_entity_select(sharp,FALSE);fb_entity_select(lava,TRUE);
        fb_tx_begin("Explicit artifact angle");fb_tx_set_key(lava,"angle","26",HIST_ENTITY);fb_tx_commit();
        fb_transform_capture();fb_affine_rotate('0 0 1',90,'-224 992 24');fb_transform_clear();
        fb_transform_expect(fabs(stof(fb_clip_field(terrain_edit(TEREDIT_ENT_GET,lava),"angle"))-116)<0.001,"explicit_artifact_angle_rotates");
        brush_undo();brush_undo();brush_undo();brush_undo();
    }
    return TRUE;
}
