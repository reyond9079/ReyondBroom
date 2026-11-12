// SPDX-License-Identifier: GPL-3.0-or-later
// Portable ownership snapshots live inside the generated group. MAP reloads
// reconstruct planes from three points and can change their last float bits.
// Keep the original numbers, then compare with bounded engine precision rather
// than coarsely quantizing hashes or forgetting ownership after a reload.
int fb_world_snapshot_key(string key)
{
    return substring(key,0,12)=="_wa_snapshot"||substring(key,0,12)=="_wa_baseline";
}
string fb_world_snapshot(int group)
{
    strbuf records=buf_create();int valid=TRUE,bytes=0,number=0;static vector bounds[2];
    for(int n=0;n<fb_node_count&&valid;n++)if(n==group||fb_selection_descendant(n,group)){
        if(++number>32768){valid=FALSE;break;}
        string record="";
        if(fb_nodes[n].kind==FB_NODE_GROUP||fb_nodes[n].kind==FB_NODE_ENTITY)
            record=strcat("E ",fb_world_fields_digest(fb_nodes[n].fields),";");
        else if(fb_nodes[n].kind==FB_NODE_BRUSH){
            int contents,count=brush_get(fb_nodes[n].model,fb_nodes[n].id,__NULL__,0,&contents);
            if(count<4||count>64||brush_getfacepoints(fb_nodes[n].model,fb_nodes[n].id,0,bounds,2)!=2){valid=FALSE;break;}
            brushface_t *faces=memalloc(count*sizeof(brushface_t));brush_get(fb_nodes[n].model,fb_nodes[n].id,faces,count,&contents);
            strbuf parts=buf_create();bufstr_add(parts,sprintf("B %i %i %s %s",count,contents,fb_clip_vec(bounds[0]),fb_clip_vec(bounds[1])),TRUE);
            for(int f=0;f<count;f++){
                brushface_t *face=&faces[f];string material=face->shadername;
                // Original generated material identifiers contain no whitespace,
                // quoting, escapes or record delimiters. Never serialize an
                // ambiguous snapshot if new generators violate that contract.
                for(int c=0;c<strlen(material);c++){int ch=str2chr(material,c);if(ch<=32||ch==34||ch==59||ch==92)valid=FALSE;}
                bufstr_add(parts,strcat(sprintf(" %s %.9g %s %.9g",fb_clip_vec(face->planenormal),face->planedist,fb_clip_vec(face->sdir),face->sbias),
                    sprintf(" %s %.9g %i %i %s",fb_clip_vec(face->tdir),face->tbias,face->surfaceflags,face->surfacevalue,material)),TRUE);
            }
            record=strcat(buf_implode(parts,""),";");buf_del(parts);memfree(faces);
        }else {valid=FALSE;break;}
        bytes+=strlen(record);if(bytes>1966080){valid=FALSE;break;}bufstr_add(records,record,TRUE);
    }
    string result=valid?buf_implode(records,""):"";buf_del(records);return result;
}
string fb_world_saved_snapshot(hashtable fields)
{
    int count=stoi(fields["_wa_snapshot_count"]);
    if(fields["_wa_snapshot_version"]!="1"||count<1||count>4096)return "";
    strbuf chunks=buf_create();int valid=TRUE;
    for(int i=0;i<count;i++){
        string part=fields[strcat("_wa_snapshot_",itos(i))];
        if(strlen(part)<1||strlen(part)>480)valid=FALSE;bufstr_add(chunks,part,TRUE);
    }
    string result=valid?buf_implode(chunks,""):"";buf_del(chunks);
    if(digest_hex("SHA256",result)!=fields["_wa_baseline"])return "";return result;
}
int fb_world_snapshot_stamp(int id,string snapshot)
{
    if(snapshot==""||strlen(snapshot)>1966080)return FALSE;
    string previous=strzone(terrain_edit(TEREDIT_ENT_GET,id));int count=fb_entity_tokenize(previous);
    strbuf fields=buf_create();
    for(int i=0;i<count;i+=2)if(!fb_world_snapshot_key(argv(i)))bufstr_add(fields,strcat(fb_map_quote(argv(i))," ",fb_map_quote(argv(i+1)),"\n"),TRUE);
    // Stay below classic compiler key/value token limits even when a user
    // compiles the saved authoring MAP directly instead of using Export.
    strunzone(previous);int chunks=ceil(strlen(snapshot)/480.);
    bufstr_add(fields,strcat("\"_wa_snapshot_version\" \"1\"\n\"_wa_snapshot_count\" \"",itos(chunks),"\"\n\"_wa_baseline\" \"",digest_hex("SHA256",snapshot),"\"\n"),TRUE);
    for(int i=0;i<chunks;i++)bufstr_add(fields,strcat("\"_wa_snapshot_",itos(i),"\" \"",substring(snapshot,i*480,480),"\"\n"),TRUE);
    string text=strzone(buf_implode(fields,""));buf_del(fields);fb_tx_entity_set(id,text,HIST_ENTITY);strunzone(text);return TRUE;
}
float fb_world_float_tolerance(float value)
{
    // Eight float ULPs at large coordinates; at normal Quake scale the floor
    // is 1/1024 unit, far below the editor's ordinary 1/8-unit minimum grid.
    return max(1./1024.,fabs(value)*0.00000096);
}
int fb_world_face_equal(brushface_t *a,brushface_t *b)
{
    if(a->shadername!=b->shadername||a->surfaceflags!=b->surfaceflags||a->surfacevalue!=b->surfacevalue)return FALSE;
    if(vlen(a->planenormal-b->planenormal)>0.000002||fabs(a->planedist-b->planedist)>fb_world_float_tolerance(a->planedist))return FALSE;
    if(vlen(a->sdir-b->sdir)>0.000002||vlen(a->tdir-b->tdir)>0.000002)return FALSE;
    return fabs(a->sbias-b->sbias)<=fb_world_float_tolerance(a->sbias)&&fabs(a->tbias-b->tbias)<=fb_world_float_tolerance(a->tbias);
}
int fb_world_snapshot_matches(int group)
{
    string saved=strzone(fb_world_saved_snapshot(fb_nodes[group].fields));if(saved==""){strunzone(saved);return FALSE;}
    // Exact comparisons cover normal in-session updates without quadratic
    // matching. The tolerant path is necessary only after reconstruction.
    string actual=strzone(fb_world_snapshot(group));
    if(actual==saved){strunzone(actual);strunzone(saved);return TRUE;}strunzone(actual);
    int *nodes=memalloc(fb_node_count*sizeof(int));int *matched=memalloc(fb_node_count*sizeof(int));
    vector *boxes=memalloc(fb_node_count*2*sizeof(vector));string *entities=memalloc(fb_node_count*sizeof(string));int number=0,valid=TRUE;
    static vector bounds[2];
    for(int n=0;n<fb_node_count&&valid;n++)if(n==group||fb_selection_descendant(n,group)){
        nodes[number]=n;
        if(fb_nodes[n].kind==FB_NODE_BRUSH){if(brush_getfacepoints(fb_nodes[n].model,fb_nodes[n].id,0,bounds,2)!=2)valid=FALSE;else {boxes[number*2]=bounds[0];boxes[number*2+1]=bounds[1];}}
        else if(fb_nodes[n].kind==FB_NODE_GROUP||fb_nodes[n].kind==FB_NODE_ENTITY)entities[number]=strzone(fb_world_fields_digest(fb_nodes[n].fields));
        else valid=FALSE;number++;
    }
    int cursor=0,records=0,length=strlen(saved);
    while(cursor<length&&valid){
        int end=strstrofs(saved,";",cursor);if(end<cursor){valid=FALSE;break;}
        string record=strzone(substring(saved,cursor,end-cursor));cursor=end+1;records++;
        int tokens=tokenize_console(record),found=FALSE;
        if(tokens==2&&argv(0)=="E"){
            string hash=strzone(argv(1));for(int i=0;i<number&&!found;i++)if(!matched[i]&&entities[i]==hash){matched[i]=TRUE;found=TRUE;}strunzone(hash);
        }else if(tokens>=9&&argv(0)=="B"){
            int count=stoi(argv(1)),contents=stoi(argv(2));
            vector low=[stof(argv(3)),stof(argv(4)),stof(argv(5))],high=[stof(argv(6)),stof(argv(7)),stof(argv(8))];
            if(count<4||count>64||tokens!=9+count*15){valid=FALSE;strunzone(record);break;}
            brushface_t *expected=memalloc(count*sizeof(brushface_t));
            for(int f=0;f<count;f++){
                int at=9+f*15;brushface_t *face=&expected[f];
                face->planenormal=[stof(argv(at)),stof(argv(at+1)),stof(argv(at+2))];face->planedist=stof(argv(at+3));
                face->sdir=[stof(argv(at+4)),stof(argv(at+5)),stof(argv(at+6))];face->sbias=stof(argv(at+7));
                face->tdir=[stof(argv(at+8)),stof(argv(at+9)),stof(argv(at+10))];face->tbias=stof(argv(at+11));
                face->surfaceflags=stoi(argv(at+12));face->surfacevalue=stoi(argv(at+13));face->shadername=strzone(argv(at+14));
            }
            for(int i=0;i<number&&!found;i++)if(!matched[i]&&entities[i]==""){
                // Bounds of a clipped rubble wedge amplify plane-rounding
                // error near an acute intersection. This is only a candidate
                // lookup: every actual plane and UV still passes the tighter
                // comparison below before the brush can match.
                int near=TRUE;for(int axis=0;axis<3;axis++)if(fabs(boxes[i*2][axis]-low[axis])>max(1./64.,fb_world_float_tolerance(low[axis]))||fabs(boxes[i*2+1][axis]-high[axis])>max(1./64.,fb_world_float_tolerance(high[axis])))near=FALSE;
                if(!near)continue;int n=nodes[i],currentContents,currentCount=brush_get(fb_nodes[n].model,fb_nodes[n].id,__NULL__,0,&currentContents);
                if(currentCount!=count||contents!=currentContents)continue;
                brushface_t *current=memalloc(count*sizeof(brushface_t));brush_get(fb_nodes[n].model,fb_nodes[n].id,current,count,&currentContents);
                int *used=memalloc(count*sizeof(int));int same=TRUE;
                for(int f=0;f<count&&same;f++){int match=FALSE;for(int c=0;c<count&&!match;c++)if(!used[c]&&fb_world_face_equal(&expected[f],&current[c])){used[c]=TRUE;match=TRUE;}if(!match)same=FALSE;}
                memfree(current);memfree(used);if(same){matched[i]=TRUE;found=TRUE;}
            }
            for(int f=0;f<count;f++)strunzone(expected[f].shadername);memfree(expected);
        }
        if(!found)print(sprintf("World Architect region %s: ownership mismatch at record %i (%s)\n",fb_nodes[group].fields["_wa_region"],records,substring(record,0,80)));
        strunzone(record);if(!found)valid=FALSE;
    }
    if(records!=number)valid=FALSE;
    for(int i=0;i<number;i++)if(entities[i]!="")strunzone(entities[i]);memfree(nodes);memfree(matched);memfree(boxes);memfree(entities);strunzone(saved);return valid;
}
