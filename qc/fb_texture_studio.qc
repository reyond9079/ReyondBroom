// SPDX-License-Identifier: GPL-3.0-or-later
// The material transaction uses the exact face masks captured before baking.
static selbrush_t studio_selection[2048];
static int studio_count, studio_epoch, studio_revision, studio_request;

int studio_selection_matches()
{
    if(fb_tx_depth || studio_count<=0 || studio_count!=selectedbrushcount)return FALSE;
    if(studio_epoch!=(int)cvar("ftebroom_map_instance") || studio_revision!=fb_document_revision())return FALSE;
    for(int i=0;i<studio_count;i++){
        if(studio_selection[i].model!=selectedbrushes[i].model || studio_selection[i].id!=selectedbrushes[i].id)return FALSE;
        if(!selbrush_same_mask(&studio_selection[i],&selectedbrushes[i]))return FALSE;
    }
    return TRUE;
}

int fb_texture_studio_command()
{
    if(argv(0)!="ftebroom_texture_apply")return FALSE;
    string mode=argv(1);
    int ticket=stoi(argv(2));
    if(mode=="capture"){
        studio_request=ticket;
        studio_count=0;
        if(!fb_tx_depth && selectedbrushcount>0 && selectedbrushcount<=2048){
            studio_epoch=(int)cvar("ftebroom_map_instance");
            studio_revision=fb_document_revision();
            studio_count=selectedbrushcount;
            for(int i=0;i<studio_count;i++)studio_selection[i]=selectedbrushes[i];
        }
        if(studio_count)cvar_set("ftebroom_texture_apply_state","captured");
        else cvar_set("ftebroom_texture_apply_state","no selection");
    }else if(ticket!=studio_request || !studio_selection_matches()){
        cvar_set("ftebroom_texture_apply_state","selection changed");
    }else if(mode=="check"){
        cvar_set("ftebroom_texture_apply_state","checked");
    }else if(mode=="commit" && argv(3)!=""){
        int before=fb_document_revision();
        string material=strzone(argv(3)),wad=strzone(argv(4));
        fb_tx_begin("Apply Texture Studio material");
        int start=history;
        cvar_set("ca_newbrushtexture",material);
        fb_material_edit("material",0);
        if(fb_tx_depth){
            if(history>start && wad!="")fb_attach_wad(wad);
            fb_tx_commit();
        }
        strunzone(material);strunzone(wad);
        if(before!=fb_document_revision())cvar_set("ftebroom_texture_apply_state","applied");
        else cvar_set("ftebroom_texture_apply_state","unchanged");
        studio_count=0;
    }
    cvar_set("ftebroom_texture_apply_ack",itos(ticket));
    return TRUE;
}
