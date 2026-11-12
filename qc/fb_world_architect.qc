// SPDX-License-Identifier: GPL-3.0-or-later
// Dedicated, acknowledged import. No SDK return value is interpreted as an
// executed map mutation, and queued requests cannot cross map/profile epochs.
int fb_world_identifier(string text)
{
    if(strlen(text)<1||strlen(text)>64||strstrofs(text,"..",0)>=0)return FALSE;
    for(int i=0;i<strlen(text);i++){int c=str2chr(text,i);if(!((c>=48&&c<=57)||(c>=65&&c<=90)||(c>=97&&c<=122)||c==45||c==46||c==95))return FALSE;}
    return TRUE;
}
int fb_world_owned_group(int node,string owner,string region)
{
    if(fb_nodes[node].kind!=FB_NODE_GROUP)return FALSE;
    return fb_nodes[node].fields["_wa_project"]==owner&&(region=="all"||fb_nodes[node].fields["_wa_region"]==region);
}
int fb_world_snapshot_key(string key);
string fb_world_fields_digest(hashtable fields)
{
    strbuf values=buf_create();
    for(int i=0;;i++){
        string key=hash_getkey(fields,i);if isnull(key)break;
        if(key=="model"&&fields["classname"]=="func_group"&&fields["_tb_type"]=="_tb_group")continue;
        if(fb_world_snapshot_key(key)||key=="_tb_id"||key=="_tb_group"||key=="_tb_layer"||key=="_tb_layer_hidden"||key=="_tb_layer_locked")continue;
        bufstr_add(values,strcat(fb_map_quote(key)," ",fb_map_quote(fields[key])),TRUE);
    }
    buf_sort(values,0,FALSE);string result=digest_hex("SHA256",buf_implode(values,"\n"));buf_del(values);return result;
}
#include "fb_world_baseline.qc"
int fb_world_unique_fields(string fields)
{
    int count=fb_entity_tokenize(fields);if(count>512)return FALSE;
    hashtable keys=hash_createtab(32,EV_INTEGER);int valid=TRUE;
    for(int i=0;i<count;i+=2){if((int)keys[argv(i)])valid=FALSE;keys[argv(i)]=1i;}
    hash_destroytab(keys);return valid;
}
int fb_world_import_scope(string owner,string region)
{
    int groups=0,worlds=0;
    for(int i=0;i<fb_map_count;i++){
        fb_map_item_t *item=&fb_map_items[i];
        if(item->kind==FB_NODE_ENTITY){
            if(!fb_world_unique_fields(item->fields))return FALSE;
            if(fb_clip_field(item->fields,"_tb_layer")!="")return FALSE;
            string classname=strzone(fb_clip_field(item->fields,"classname"));int valid=TRUE;
            if(classname=="worldspawn"){
                worlds++;for(int child=0;child<fb_map_count;child++)if(fb_map_items[child].parent==i)valid=FALSE;
            }else if(classname=="func_group"){
                string id=strzone(fb_clip_field(item->fields,"_tb_id")),own=strzone(fb_clip_field(item->fields,"_wa_project")),part=strzone(fb_clip_field(item->fields,"_wa_region"));
                if(id==""||own!=owner||(region!="all"&&part!=region)||fb_clip_field(item->fields,"_tb_type")!="_tb_group"||fb_clip_field(item->fields,"_tb_group")!="")valid=FALSE;
                for(int p=0;p<i;p++)if(fb_map_items[p].kind==FB_NODE_ENTITY&&fb_clip_field(fb_map_items[p].fields,"_tb_id")==id)valid=FALSE;
                strunzone(id);strunzone(own);strunzone(part);groups++;
            }else{
                if(classname!="light"&&classname!="info_null"&&classname!="info_player_start"&&classname!="info_player_deathmatch")valid=FALSE;
                string parent=strzone(fb_clip_field(item->fields,"_tb_group"));int found=FALSE;
                for(int p=0;p<fb_map_count;p++)if(fb_map_items[p].kind==FB_NODE_ENTITY&&fb_clip_field(fb_map_items[p].fields,"_wa_project")==owner&&fb_clip_field(fb_map_items[p].fields,"_tb_id")==parent)found=TRUE;
                if(!found||fb_clip_field(item->fields,"model")!=""||fb_clip_field(item->fields,"_tb_id")!="")valid=FALSE;strunzone(parent);
            }
            strunzone(classname);if(!valid)return FALSE;
        }else{
            if(item->kind!=FB_NODE_BRUSH)return FALSE;
            if(item->parent<0||item->parent>=fb_map_count)return FALSE;
            if(fb_clip_field(fb_map_items[item->parent].fields,"_wa_project")!=owner||fb_clip_field(fb_map_items[item->parent].fields,"classname")!="func_group")return FALSE;
        }
    }
    return worlds==1&&groups>0&&groups<=64&&(region=="all"||groups==1);
}
void fb_world_liquid_contents()
{
    // Classic Valve 220 has no Q3 flag tuple. Its liquid-name convention
    // must still become the editor's real water contents after import.
    for(int i=0;i<fb_map_count;i++)if(fb_map_items[i].kind==FB_NODE_BRUSH){
        fb_map_item_t *item=&fb_map_items[i];if(item->count<4)continue;
        string name=strzone(item->faces[0].shadername);int liquid=substring(name,0,3)=="*wa"||substring(name,0,3)=="!wa";
        for(int f=1;f<item->count;f++)if(item->faces[f].shadername!=name)liquid=FALSE;
        if(liquid)item->contents=CONTENTBIT_WATER;strunzone(name);
    }
}
int fb_world_group_node(int id)
{
    for(int n=2;n<fb_node_count;n++)if(fb_nodes[n].kind==FB_NODE_GROUP&&fb_nodes[n].id==id)return n;return -1;
}
int fb_world_clip_insert()
{
    fb_world_importing=TRUE;int success=fb_clip_insert('0 0 0');fb_world_importing=FALSE;return success;
}
int fb_world_clear_group(int group)
{
    int *pairs=memalloc(sizeof(int)*fb_node_count*2);int *entities=memalloc(sizeof(int)*fb_node_count);int brushes=0,number=0,valid=TRUE;
    for(int n=2;n<fb_node_count;n++)if(n!=group&&fb_selection_descendant(n,group)){
        if(fb_nodes[n].kind==FB_NODE_BRUSH){pairs[brushes*2]=fb_nodes[n].model;pairs[brushes*2+1]=fb_nodes[n].id;brushes++;}
        else if(fb_nodes[n].kind==FB_NODE_ENTITY)entities[number++]=fb_nodes[n].id;
        else valid=FALSE;
    }
    // Delete only this group's exact descendants. Generic selection deletion
    // could also remove an empty, authored ancestor containing the region.
    if(valid){for(int i=0;i<brushes;i++)brush_history_delete(pairs[i*2],pairs[i*2+1]);for(int i=0;i<number;i++)fb_tx_entity_set(entities[i],"",HIST_ENTITY);}
    memfree(pairs);memfree(entities);return valid;
}
int fb_world_paste_group(int source,int destination)
{
    fb_map_item_t *whole=fb_map_items;int wholeCount=fb_map_count;
    string identity=strzone(fb_clip_field(whole[source].fields,"_tb_id"));
    fb_map_item_t *part=memalloc(sizeof(fb_map_item_t)*wholeCount);part[0]=whole[source];part[0].parent=-1;int number=1;
    for(int i=0;i<wholeCount;i++)if(i!=source){
        int include=whole[i].parent==source;
        if(whole[i].kind==FB_NODE_ENTITY&&fb_clip_field(whole[i].fields,"_tb_group")==identity)include=TRUE;
        if(include){part[number]=whole[i];part[number].parent=whole[i].kind==FB_NODE_ENTITY?-1:0;number++;}
    }
    string replacement="";
    if(destination>=0){
        // Paste into the retained model, keeping its entity, parent/layer and
        // identity instead of allocating orphaned models on every update.
        if(destination==0||fb_world_group_node(destination)<2){strunzone(identity);memfree(part);return FALSE;}
        fb_open_group=destination;replacement=strzone("\"classname\" \"worldspawn\"\n");part[0].fields=replacement;
    }
    fb_map_items=part;fb_map_count=number;int success=fb_world_clip_insert();fb_map_items=whole;fb_map_count=wholeCount;
    if(replacement!="")strunzone(replacement);strunzone(identity);memfree(part);return success;
}
string fb_world_owned_insert(string owner,string region,string mode)
{
    if(!fb_world_import_scope(owner,region))return "scope-failed";
    fb_world_liquid_contents();
    fb_document_tick();int groups[64],count=0;
    for(int n=2;n<fb_node_count;n++)if(fb_world_owned_group(n,owner,region)){
        if(count>=64)return "too-many-regions";for(int i=0;i<count;i++)if(fb_nodes[groups[i]].fields["_wa_region"]==fb_nodes[n].fields["_wa_region"])return "ambiguous-region";groups[count++]=n;
        if(mode=="add")return "exists";
        for(int child=0;child<fb_node_count;child++)if(child==n||fb_selection_descendant(child,n))if(fb_nodes[child].flags&(FB_NODE_LOCKED|FB_NODE_HIDDEN))return "locked";
        if(!fb_world_snapshot_matches(n))return "modified";
        if(region!="all"){
            int compatible=FALSE;string expected=strzone(fb_nodes[n].fields["_wa_structure"]);
            for(int source=0;source<fb_map_count;source++)if(fb_map_items[source].kind==FB_NODE_ENTITY&&fb_clip_field(fb_map_items[source].fields,"_wa_region")==region&&fb_clip_field(fb_map_items[source].fields,"_wa_structure")==expected&&expected!="")compatible=TRUE;
            strunzone(expected);if(!compatible)return "structure-changed";
        }
    }
    if(mode=="replace"&&!count)return "missing-region";
    int oldOpen=fb_open_group,oldIds[64];string oldRegions[64],parent="",layer="";
    for(int i=0;i<count;i++){oldIds[i]=fb_nodes[groups[i]].id;oldRegions[i]=strzone(fb_nodes[groups[i]].fields["_wa_region"]);}
    if(count){parent=strzone(fb_nodes[groups[0]].fields["_tb_group"]);layer=strzone(fb_nodes[groups[0]].fields["_tb_layer"]);}
    fb_tx_begin(mode=="replace"?"World Architect region update":"World Architect insert");int success=TRUE;
    if(mode=="add")success=fb_world_clip_insert();
    else{
        for(int source=0;source<fb_map_count&&success;source++)if(fb_map_items[source].kind==FB_NODE_ENTITY&&fb_clip_field(fb_map_items[source].fields,"classname")=="func_group"){
            string part=strzone(fb_clip_field(fb_map_items[source].fields,"_wa_region"));fb_document_tick();int destination=-1;
            for(int n=2;n<fb_node_count;n++)if(fb_world_owned_group(n,owner,part))destination=n;
            // Keep the integer entity reference explicit: QCC can promote a
            // conditional struct-member load with -1 to float and truncate it.
            int id=-1;if(destination>=0)id=fb_nodes[destination].id;
            if(id==0){success=FALSE;strunzone(part);break;}
            if(destination>=0)success=fb_world_clear_group(destination);else fb_open_group=oldOpen;
            if(success)success=fb_world_paste_group(source,id);
            if(success&&id>=0){string structure=strzone(fb_clip_field(fb_map_items[source].fields,"_wa_structure"));fb_tx_set_key(id,"_wa_structure",structure,HIST_ENTITY);strunzone(structure);}
            if(success&&id<0){
                fb_document_tick();for(int n=2;n<fb_node_count;n++)if(fb_world_owned_group(n,owner,part)){
                    fb_tx_set_key(fb_nodes[n].id,"_tb_group",parent,HIST_TREE);fb_tx_set_key(fb_nodes[n].id,"_tb_layer",layer,HIST_TREE);
                }
            }
            strunzone(part);
        }
        for(int i=0;i<count&&success;i++){
            int retained=FALSE;
            for(int source=0;source<fb_map_count;source++)if(fb_map_items[source].kind==FB_NODE_ENTITY&&fb_clip_field(fb_map_items[source].fields,"_wa_region")==oldRegions[i])retained=TRUE;
            if(!retained){fb_document_tick();int group=fb_world_group_node(oldIds[i]);if(group<0)success=FALSE;else {success=fb_world_clear_group(group);if(success)fb_tx_entity_set(oldIds[i],"",HIST_TREE);}}
        }
    }
    if(success){
        fb_document_tick();int ids[64],number=0;string baselines[64];
        for(int n=2;n<fb_node_count&&success;n++)if(fb_world_owned_group(n,owner,region)){
            if(number>=64){success=FALSE;break;}ids[number]=fb_nodes[n].id;baselines[number]=strzone(fb_world_snapshot(n));if(baselines[number]=="")success=FALSE;number++;
        }
        for(int i=0;i<number;i++){if(success)success=fb_world_snapshot_stamp(ids[i],baselines[i]);strunzone(baselines[i]);}
    }
    if(oldOpen>0&&terrain_edit(TEREDIT_ENT_GET,oldOpen)!="")fb_open_group=oldOpen;else fb_open_group=0;
    if(success)fb_tx_commit();else fb_tx_cancel();for(int i=0;i<count;i++)strunzone(oldRegions[i]);if(count){strunzone(parent);strunzone(layer);}return success?"ok":"insert-failed";
}
void fb_world_architect_apply()
{
    string request=strzone(argv(1));int epoch=stoi(argv(2)),profile=stoi(argv(3));
    string owner=strzone(argv(4)),region=strzone(argv(5)),mode=strzone(argv(6));
    string status="invalid";
    int safe=strlen(request)>0 && strlen(request)<=24;
    for(int i=0;i<strlen(request)&&safe;i++){int ch=str2chr(request,i);if(ch<48||ch>57)safe=FALSE;}
    if(owner!=""&&(!fb_world_identifier(owner)||(mode!="add"&&mode!="replace")||(region!="all"&&(region!=itos(stoi(region))||stoi(region)<0))))safe=FALSE;
    if(safe) {
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
                    if(valid)status=owner!=""?fb_world_owned_insert(owner,region,mode):(fb_clip_insert('0 0 0')?"ok":"insert-failed");
                    else status="parse-failed";
                    fb_map_clear();
                } else fclose(file);
            } else status="missing";
        }
    }
    cvar_set("ftebroom_world_apply_status",status);
    cvar_set("ftebroom_world_apply_ack",request);strunzone(request);strunzone(owner);strunzone(region);strunzone(mode);
}
