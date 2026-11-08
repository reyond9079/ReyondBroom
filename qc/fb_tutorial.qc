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
    // Camera and grid lessons need one reference brush; creation starts empty.
    if(step==0 || (step>=2 && step<=5))fb_createbox('0 0 32','96 96 64');
    if(step==0 || step==5)fb_selection_clear();
    if(cvar("ftebroom_skip_setup"))cvar_set("ftebroom_tour_fixture_ready",ftos(step+1));
    fb_tutorial_checkpoint=history;
    if(cvar("ftebroom_skip_setup") && cvar("ftebroom_followup133_qa"))fb_tutorial_stage_check();
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
}
float fb_tutorial_command()
{
    if(argv(0)=="ftebroom_tour_prepare"){fb_tutorial_prepare(stoi(argv(1)));return TRUE;}
    if(argv(0)=="ftebroom_tour_restore"){fb_tutorial_restore();return TRUE;}
    return FALSE;
}
