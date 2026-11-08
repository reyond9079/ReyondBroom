// SPDX-License-Identifier: GPL-2.0-or-later
// Demonstrations use the real tools, then restore the complete lesson fixture.
// A journal checkpoint preserves texture attributes, selection and entity data
// as well as geometry; creation demonstrations leave no brush behind.
var int fb_tutorial_checkpoint=-1;
void fb_tutorial_prepare(int step)
{
    if(!cvar("ftebroom_tour_active"))return;
    fb_cancel();fb_tool_deactivate();fb_selection_clear();
    if(step>=2 && step<=4)fb_createbox('0 0 32','96 96 64');
    fb_tutorial_checkpoint=history;
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
