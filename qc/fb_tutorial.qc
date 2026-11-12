// SPDX-License-Identifier: GPL-2.0-or-later
// Demonstrations use the real tools, then restore the complete lesson fixture.
// A journal checkpoint preserves texture attributes, selection and entity data
// as well as geometry; creation demonstrations leave no brush behind.
var int fb_tutorial_checkpoint=-1;
void fb_tutorial_stage_check();
void fb_tutorial_prepare(int step)
{
    if(!cvar("ftebroom_tour_active"))return;
    fb_cancel();fb_tool_deactivate();fb_selection_clear();
    // Loading a map can restore its last projection after the native layout reset.
    cvar_set("ca_brush_view","0");
    // The grid lesson lets users choose any spacing. Clip points and Vertex
    // drags need a known grid or both demo targets can snap to the same point.
    if(step>=7)cvar_set("ca_grid","16");
    // This command only operates in the lesson's disposable map. Clear both
    // the map's fixture and any practice geometry before preparing a stage.
    static int ids[128];static vector axes[1];static float distances[1];
    int count;
    fb_tx_begin("Prepare lesson");
    while((count=brush_findinvolume(1,axes,distances,0,ids,__NULL__,128))>0){
        // The brush store may remap IDs on deletion. Use the normal selection
        // deletion path, which tracks those remaps, rather than stale IDs.
        for(int i=0;i<min(count,128);i++)brush_select(1,ids[i]);
        if(!selectedbrushcount)break;
        fb_delete_geometry();
    }
    fb_tx_commit();fb_selection_clear();
    // One isolated target; creation and the final tools page are empty.
    cvar_set("ca_newbrushtexture","ftebroom/qa_blue");
    if(step==0 || (step>=2 && step<=11))fb_createbox('0 0 32','96 96 64');
    if(step==0 || step==5 || step==6 || step==10 || step==11)fb_selection_clear();
    if(step==9)fb_tool_activate(FB_TOOL_VERTEX);
    cvar_set("ftebroom_tour_fixture_ready",ftos(step+1));
    fb_tutorial_checkpoint=history;
    if(cvar("ftebroom_skip_setup") && cvar("ftebroom_followup133_qa"))fb_tutorial_stage_check();
}
void fb_tutorial_project()
{
    int step=cvar("ftebroom_tour_step");
    vector a,b;
    if(step==7){
        // Separate points across screen width, not along the foreshortened
        // depth axis: the second click must not grab the first point's handle.
        a=project('-32 0 64');b=project('32 0 64');
        cvar_set("ftebroom_tour_clip_pixels",sprintf("%g %g %g %g",a_x,a_y,b_x,b_y));
    }
    if(step==9){
        a=project('48 -48 64');b=project('80 -48 64');
        cvar_set("ftebroom_tour_vertex_pixels",sprintf("%g %g %g %g",a_x,a_y,b_x,b_y));
    }
    if(step==10 || step==11){
        vector p=project('0 -48 32');
        cvar_set("ftebroom_tour_face_pixel",sprintf("%g %g",p_x,p_y));
    }
}
void fb_tutorial_observe(int previousHistory)
{
    if(!cvar("ftebroom_tour_active"))return;
    int step=cvar("ftebroom_tour_step");
    if(cvar("ftebroom_skip_setup") && cvar("ftebroom_followup133_qa") && history>previousHistory && (step==9 || step==10))
        print(sprintf("FTEBROOM_TUTORIAL_EDIT step=%i demo=%g command=%s\n",step,cvar("ftebroom_tour_demo"),argv(0)));
    if(cvar("ftebroom_tour_demo"))return;
    if(step==9 && argv(0)=="ftebroom_pointer" && argv(1)=="up" && cvar("ftebroom_tool")==FB_TOOL_VERTEX && history>previousHistory)fb_tour_signal(9);
    if(step==10 && argv(0)=="ftebroom_material" && history>previousHistory && selectedbrushcount==1 && selbrush_whole(&selectedbrushes[0]))fb_tour_signal(10);
    if(step==11 && argv(0)=="ftebroom_pointer" && argv(1)=="up" && selectedbrushcount==1 && !selbrush_whole(&selectedbrushes[0]) && fb_faces_count()==1)fb_tour_signal(11);
}
void fb_tutorial_restore()
{
    if(!cvar("ftebroom_tour_active") || fb_tutorial_checkpoint<0)return;
    fb_cancel();fb_tool_deactivate();
    // Demo input is exclusive, so only its transactions follow the checkpoint.
    // Stop if an invalid history operation made no progress, never spin in QC.
    if(fb_tutorial_checkpoint<history_min || fb_tutorial_checkpoint>history)return;
    while(history>fb_tutorial_checkpoint){int previous=history;brush_undo();if(history>=previous)return;}
    // Demonstrations are not user edits and must not remain available as Redo.
    for(int i=history;i<history_max;i++)history_release(&historyring[i&(historyring_length-1)]);
    history_max=history;fb_history_labels();fb_document_tick();
    if(cvar("ftebroom_tour_step")==7 && cvar("ftebroom_skip_setup") && cvar("ftebroom_followup133_qa"))fb_tutorial_stage_check();
}
float fb_tutorial_command()
{
    if(argv(0)=="ftebroom_tour_prepare"){fb_tutorial_prepare(stoi(argv(1)));return TRUE;}
    if(argv(0)=="ftebroom_tour_restore"){fb_tutorial_restore();return TRUE;}
    return FALSE;
}
