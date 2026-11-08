// SPDX-License-Identifier: GPL-2.0-or-later
void fb_tutorial_expect(int condition,string name)
{
    print(strcat(condition?"FTEBROOM_TUTORIAL_PASS ":"FTEBROOM_TUTORIAL_FAIL ",name,"\n"));
}
int fb_tutorial_brushes()
{
    static int ids[128];static vector axes[1];static float distances[1];
    return brush_findinvolume(1,axes,distances,0,ids,__NULL__,128);
}
float fb_tutorial_testcommand()
{
    if(argv(0)!="ftebroom_test_tutorial132")return FALSE;
    if(!cvar("ftebroom_skip_setup"))return TRUE;
    string mode=argv(1);
    if(mode=="outside"){
        cvar_set("ftebroom_tour_active","0");int outsideHistory=history,brushes=fb_tutorial_brushes();
        fb_tutorial_prepare(2);fb_tutorial_restore();
        fb_tutorial_expect(history==outsideHistory && fb_tutorial_brushes()==brushes,"outside_lesson_unchanged");
    }else if(mode=="create"){
        cvar_set("ftebroom_tour_active","1");fb_tutorial_prepare(1);int createMarker=history,createCount=fb_tutorial_brushes();
        fb_createbox('128 0 32','64 64 64');fb_createbox('256 0 32','64 64 64');
        fb_tutorial_expect(fb_tutorial_brushes()==createCount+2,"creation_demo_changes_scene");
        fb_tutorial_restore();
        fb_tutorial_expect(history==createMarker && history_max==createMarker && !selectedbrushcount && fb_tutorial_brushes()==createCount,"creation_demo_removed_without_redo");
        fb_tutorial_restore();fb_tutorial_expect(history==createMarker && fb_tutorial_brushes()==createCount,"restore_is_idempotent");
    }else if(mode=="modify"){
        cvar_set("ftebroom_tour_active","1");fb_tutorial_prepare(3);int modifyMarker=history,modifyCount=fb_tutorial_brushes();
        static brushface_t before[128],after[128];int contents;
        int faces=brush_get(1,selectedbrushes[0].id,before,128,&contents);
        for(int f=0;f<faces;f++)before[f].shadername=strzone(before[f].shadername);
        fb_tx_begin("Demonstrate vertical movement");fb_move_numeric('0 0 128');fb_tx_commit();
        int id=selectedbrushes[0].id;
        brush_get(1,id,after,128,__NULL__);
        for(int f=0;f<faces;f++){after[f].planedist+=32;after[f].sbias+=17;after[f].surfaceflags=42;after[f].shadername="ftebroom/qa_red";}
        fb_tx_begin("Demonstrate extrusion");brush_history_edit(1,id,after,faces,contents);fb_tx_commit();
        fb_tutorial_restore();
        int correct=history==modifyMarker && history_max==modifyMarker && selectedbrushcount==1 && fb_tutorial_brushes()==modifyCount;
        if(selectedbrushcount){
            correct&=brush_get(1,selectedbrushes[0].id,after,128,__NULL__)==faces;
            for(int f=0;f<faces;f++)correct&=before[f].planenormal==after[f].planenormal && before[f].planedist==after[f].planedist && before[f].shadername==after[f].shadername && before[f].sdir==after[f].sdir && before[f].tdir==after[f].tdir && before[f].sbias==after[f].sbias && before[f].tbias==after[f].tbias && before[f].surfaceflags==after[f].surfaceflags;
        }
        for(int f=0;f<faces;f++)strunzone(before[f].shadername);
        fb_tutorial_expect(correct,"modified_demo_restores_geometry_material_and_selection");
    }else if(mode=="empty"){
        cvar_set("ftebroom_tour_active","1");int emptyCount=fb_tutorial_brushes();fb_tutorial_prepare(5);
        fb_tutorial_expect(fb_tutorial_brushes()==emptyCount && !selectedbrushcount,"grid_stage_adds_no_example_brush");
        fb_tutorial_prepare(6);fb_tutorial_expect(fb_tutorial_brushes()==emptyCount && !selectedbrushcount,"tools_stage_adds_no_example_brush");
    }
    return TRUE;
}
