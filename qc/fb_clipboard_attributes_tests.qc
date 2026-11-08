// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the same MAP fragment export/parser/paste path used by face Ctrl+C/V.
brushface_t fb_clipattrs_before[12],fb_clipattrs_source;
selbrush_t fb_clipattrs_selection[2];
int fb_clipattrs_contents[2],fb_clipattrs_saved;
void fb_clipattrs_expect(int condition,string name)
{
    print(strcat(condition?"FTEBROOM_CLIPATTRS_PASS ":"FTEBROOM_CLIPATTRS_FAIL ",name,"\n"));
}
int fb_clipattrs_equal(brushface_t *a,brushface_t *b,int plane)
{
    if(plane)if(vlen(a->planenormal-b->planenormal)>.00001 || fabs(a->planedist-b->planedist)>.00001)return FALSE;
    return a->shadername==b->shadername && vlen(a->sdir-b->sdir)<.00001 && vlen(a->tdir-b->tdir)<.00001 && fabs(a->sbias-b->sbias)<.00001 && fabs(a->tbias-b->tbias)<.00001 && a->surfaceflags==b->surfaceflags && a->surfacevalue==b->surfacevalue;
}
int fb_clipattrs_fixture(int faceOnly)
{
    fb_cancel();fb_tool_deactivate();fb_selection_clear();
    for(int old=0;old<fb_clipattrs_saved;old++)strunzone(fb_clipattrs_before[old].shadername);
    fb_clipattrs_saved=0;strunzone(fb_clipattrs_source.shadername);
    int source=fb_createbox('0 0 256','64 64 64');static brushface_t faces[6];
    brush_get(1,source,faces,6,__NULL__);
    faces[2].shadername="ftebroom/qa_red";faces[2].sdir='0.5 0.25 -1';faces[2].tdir='-0.125 2 0.375';
    faces[2].sbias=13.75;faces[2].tbias=-27.25;faces[2].surfaceflags=42;faces[2].surfacevalue=93;
    fb_clipattrs_source=faces[2];fb_clipattrs_source.shadername=strzone(faces[2].shadername);
    fb_tx_begin("Clipboard source attributes");source=brush_history_edit(1,source,faces,6,CONTENTBIT_WATER);fb_tx_commit();
    fb_faces_click(1,source,3,FALSE);
    int exported=fb_clip_export();
    string fragment=strzone(fb_clip_test_read("clipboard-out.map"));
    int parsed=fb_map_parse(fragment);strunzone(fragment);
    int sourceValid=FALSE;
    if(parsed)if(fb_map_count==1 && fb_map_items[0].kind==-1 && fb_map_items[0].count==1)
        sourceValid=fb_map_items[0].contents==CONTENTBIT_WATER && fb_clipattrs_equal(&fb_clipattrs_source,fb_map_items[0].faces,TRUE);
    fb_clipattrs_expect(exported && parsed && sourceValid,faceOnly?"face_fragment_roundtrip_for_face_paste":"face_fragment_roundtrip_for_brush_paste");
    if(!sourceValid)return FALSE;
    static int targets[2];
    for(int b=0;b<2;b++){
        targets[b]=fb_createbox([192+b*160,0,128],'96 64 128');
        brush_get(1,targets[b],faces,6,__NULL__);
        for(int f=0;f<6;f++){
            faces[f].shadername="ftebroom/qa_blue";faces[f].sdir=[1+b,.125*f,0];faces[f].tdir=[0,.25,1+.25*f];
            faces[f].sbias=b*100+f;faces[f].tbias=-b*50-f*3;faces[f].surfaceflags=100+f;faces[f].surfacevalue=200+b*10+f;
        }
        fb_tx_begin("Clipboard target fixture");targets[b]=brush_history_edit(1,targets[b],faces,6,CONTENTBIT_SOLID);fb_tx_commit();
    }
    fb_selection_clear();
    if(faceOnly){fb_faces_click(1,targets[0],3,FALSE);fb_faces_click(1,targets[1],5,TRUE);}
    else {brush_select(1,targets[0]);brush_select(1,targets[1]);}
    for(int dest=0;dest<2;dest++){
        fb_clipattrs_selection[dest]=selectedbrushes[dest];
        brush_get(1,selectedbrushes[dest].id,fb_clipattrs_before+dest*6,6,&fb_clipattrs_contents[dest]);
        for(int face=0;face<6;face++){fb_clipattrs_before[dest*6+face].shadername=strzone(fb_clipattrs_before[dest*6+face].shadername);fb_clipattrs_saved++;}
    }
    return TRUE;
}
int fb_clipattrs_check(int pasted)
{
    if(selectedbrushcount!=2)return FALSE;
    static brushface_t faces[6];int contents;
    for(int b=0;b<2;b++){
        if(!selbrush_same_mask(&selectedbrushes[b],&fb_clipattrs_selection[b]))return FALSE;
        if(brush_get(1,selectedbrushes[b].id,faces,6,&contents)!=6)return FALSE;
        if(contents!=(pasted?CONTENTBIT_WATER:fb_clipattrs_contents[b]))return FALSE;
        for(int f=0;f<6;f++){
            brushface_t *before=&fb_clipattrs_before[b*6+f];
            if(vlen(faces[f].planenormal-before->planenormal)>.00001 || fabs(faces[f].planedist-before->planedist)>.00001)return FALSE;
            int receives=pasted && (selbrush_whole(&fb_clipattrs_selection[b]) || selbrush_face(&fb_clipattrs_selection[b],f));
            if(receives){if(!fb_clipattrs_equal(&fb_clipattrs_source,&faces[f],FALSE))return FALSE;}
            else if(!fb_clipattrs_equal(before,&faces[f],TRUE))return FALSE;
        }
    }
    return TRUE;
}
void fb_clipattrs_case(int faceOnly)
{
    if(!fb_clipattrs_fixture(faceOnly))return;
    int marker=history,applied=fb_clip_apply_face(),after=history;
    fb_clipattrs_expect(applied && after>marker && fb_clipattrs_check(TRUE),faceOnly?"selected_faces_only_receive_all_attributes":"whole_brush_receives_all_attributes_preserving_planes_selection");
    brush_undo();
    fb_clipattrs_expect(history==marker && fb_clipattrs_check(FALSE),faceOnly?"face_paste_single_undo_restores_full_before":"brush_paste_single_undo_restores_full_before");
    brush_redo();
    fb_clipattrs_expect(history==after && fb_clipattrs_check(TRUE),faceOnly?"face_paste_redo_restores_full_after":"brush_paste_redo_restores_full_after");
}
float fb_clipattrs_testcommand()
{
    if(argv(0)!="ftebroom_test_clipboard_attributes132")return FALSE;
    if(!cvar("ftebroom_skip_setup"))return TRUE;
    string mode=argv(1);
    if(mode=="whole")fb_clipattrs_case(FALSE);
    else if(mode=="face")fb_clipattrs_case(TRUE);
    else if(mode=="empty"){
        static brushface_t emptyBefore[6],emptyAfter[6];int emptyId=selectedbrushes[0].id,emptyContentsBefore,emptyContentsAfter;
        brush_get(1,emptyId,emptyBefore,6,&emptyContentsBefore);
        for(int f=0;f<6;f++)emptyBefore[f].shadername=strzone(emptyBefore[f].shadername);
        fb_selection_clear();int emptyHistory=history,emptyMax=history_max,emptyRevision=fb_document_revision();
        int rejected=!fb_clip_apply_face();
        int unchanged=brush_get(1,emptyId,emptyAfter,6,&emptyContentsAfter)==6 && emptyContentsBefore==emptyContentsAfter;
        for(int face=0;face<6;face++){unchanged&=fb_clipattrs_equal(&emptyBefore[face],&emptyAfter[face],TRUE);strunzone(emptyBefore[face].shadername);}
        fb_clipattrs_expect(rejected && unchanged && !selectedbrushcount && history==emptyHistory && history_max==emptyMax && fb_document_revision()==emptyRevision,"no_selection_fails_without_geometry_or_history_changes");
    }else if(mode=="external"){
        fb_clipattrs_expect(fb_map_parse("( 0 0 0 ) ( 64 0 0 ) ( 0 64 0 ) ftebroom/qa_red [ 0.5 0.25 0 7 ] [ 0 2 0.375 -9 ] 0 1 1"),"external_face_without_optional_attributes_parses");
        fb_clipattrs_expect(fb_map_items[0].contents==CONTENTBIT_SOLID,"external_face_defaults_to_solid_contents");
        fb_createbox('0 320 64','64 64 128');int externalApplied=fb_clip_apply_face(),externalContents;
        brush_get(1,selectedbrushes[0].id,__NULL__,0,&externalContents);
        fb_clipattrs_expect(externalApplied && externalContents==CONTENTBIT_SOLID,"external_face_paste_keeps_brush_solid");
        fb_map_clear();
    }
    return TRUE;
}
