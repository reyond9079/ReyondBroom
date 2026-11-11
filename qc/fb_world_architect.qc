// SPDX-License-Identifier: GPL-3.0-or-later
// Dedicated, acknowledged import. No SDK return value is interpreted as an
// executed map mutation, and queued requests cannot cross map/profile epochs.
void fb_world_architect_apply()
{
    string request=strzone(argv(1));int epoch=stoi(argv(2)),profile=stoi(argv(3));
    string status="invalid";
    if(request==itos(stoi(request)) && stoi(request)>0) {
        fb_tool_refresh();fb_document_tick();
        if(epoch!=cvar("ftebroom_map_instance") || profile!=cvar("ftebroom_game_profile"))status="stale";
        else {
            filestream file=fopen(strcat("wa-import-",request,".map"),FILE_READ);
            if((float)file>=0) {
                int bytes=fsize(file);
                if(bytes>0 && bytes<=16777216) {
                    strbuf lines=buf_create();string line;
                    while((line=fgets(file)))bufstr_add(lines,line,TRUE);
                    string text=strzone(buf_implode(lines,"\n"));buf_del(lines);fclose(file);
                    int valid=fb_map_parse(text);strunzone(text);
                    if(valid && profile>=2)for(int i=0;i<fb_map_count;i++)if(fb_map_items[i].kind==FB_NODE_PATCH)valid=FALSE;
                    if(valid)status=fb_clip_insert('0 0 0')?"ok":"insert-failed";
                    else status="parse-failed";
                    fb_map_clear();
                } else fclose(file);
            } else status="missing";
        }
    }
    cvar_set("ftebroom_world_apply_status",status);
    cvar_set("ftebroom_world_apply_ack",request);strunzone(request);
}
