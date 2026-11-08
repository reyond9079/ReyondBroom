int fb_clip_q3_export;
// SPDX-License-Identifier: GPL-2.0-or-later
string fb_clip_vec(vector p) {return sprintf("%.9g %.9g %.9g",p_x,p_y,p_z);}
string fb_clip_face(brushface_t *face,int contents)
{
    vector n=face->planenormal,u;
    if(fabs(n_z)<0.9) u=normalize(crossproduct(n,'0 0 1'));
    else u=normalize(crossproduct(n,'0 1 0'));
    vector v=crossproduct(u,n),p=n*face->planedist;
    string plane=strcat("( ",fb_clip_vec(p)," ) ( ",fb_clip_vec(p+u*64)," ) ( ",fb_clip_vec(p+v*64)," ) ");
    string axes=strcat(" [ ",fb_clip_vec(face->sdir),sprintf(" %.9g ] [ ",face->sbias),
        fb_clip_vec(face->tdir),sprintf(" %.9g ] 0 1 1 %i %i %i\n",face->tbias,contents,face->surfaceflags,face->surfacevalue));
    return strcat(plane,fb_map_quote(face->shadername),axes);
}
int fb_clip_write_geometry(filestream file,int model,int id)
{
    patchinfo_t patch;int count=patch_getcp(model,id,__NULL__,0,&patch);
    if(count) {
        patchvert_t *points=memalloc(sizeof(patchvert_t)*count);patch_getcp(model,id,points,count,&patch);
        int colored=FALSE;
        for(int p=0;p<count;p++) if(points[p].rgb!='1 1 1' || points[p].a!=1) colored=TRUE;
        int explicitSubdiv=patch.tesswidth>=0 && patch.tessheight>=0;
        if(fb_clip_q3_export){colored=FALSE;explicitSubdiv=FALSE;}
        fputs(file,strcat("{\n",explicitSubdiv?"patchDef3":"patchDef2",colored?"WS":"","\n{\n",fb_map_quote(patch.shadername),"\n"));
        fputs(file,strcat("( ",itos(patch.cpwidth)," ",itos(patch.cpheight)," "));
        if(explicitSubdiv) fputs(file,strcat(itos(patch.tesswidth)," ",itos(patch.tessheight)," "));
        fputs(file,"0 1 1 )\n(\n");
        for(int row=0;row<(fb_clip_q3_export?patch.cpwidth:patch.cpheight);row++) {
            fputs(file,"(\n");
            for(int column=0;column<(fb_clip_q3_export?patch.cpheight:patch.cpwidth);column++) {
                // Native MAP patches and Q3 BSP patches have opposite front-face winding.
                // Reverse one parameter while keeping each position/UV pair together.
                int x=fb_clip_q3_export?patch.cpwidth-1-row:column,y=fb_clip_q3_export?column:row;
                patchvert_t cp=points[y*patch.cpwidth+x];
                fputs(file,strcat("( ",fb_clip_vec(cp.xyz),sprintf(" %.9g %.9g",cp.s,cp.t)));
                if(colored) fputs(file,strcat(" ",fb_clip_vec(cp.rgb),sprintf(" %.9g",cp.a)));
                fputs(file," )\n");
            }
            fputs(file,")\n");
        }
        fputs(file,")\n}\n}\n");memfree(points);return TRUE;
    }
    int contents;count=brush_get(model,id,__NULL__,0,&contents);
    if(count<4 || count>64) return FALSE;
    brushface_t *faces=memalloc(sizeof(brushface_t)*count);brush_get(model,id,faces,count,&contents);
    fputs(file,"{\n");
    for(int f=0;f<count;f++) fputs(file,fb_clip_face(&faces[f],contents));
    fputs(file,"}\n");memfree(faces);return TRUE;
}
int fb_clip_export()
{
    fb_document_tick();
    if(!selectedbrushcount && !fb_selected_entity_count) return FALSE;
    filestream file=fopen("clipboard-out.map",FILE_WRITE);
    if((float)file<0) return FALSE;
    if(fb_faces_count()) {
        int index=-1,face=-1;
        for(int b=0;b<selectedbrushcount && index<0;b++) for(int f=0;f<64;f++)
            if(selbrush_face(&selectedbrushes[b],f)) {index=b;face=f;break;}
        if(index>=0) {
            static brushface_t faces[64];int contents;
            int count=brush_get(selectedbrushes[index].model,selectedbrushes[index].id,faces,64,&contents);
            if(face<count) fputs(file,fb_clip_face(&faces[face],contents));
        }
        fclose(file);return index>=0;
    }
    int *included=memalloc(sizeof(int)*fb_node_count);
    included[0]=TRUE;
    for(int n=2;n<fb_node_count;n++) if(fb_selection_leaf(n) && fb_selection_selected(n)) {
        for(int p=n;p>=2;p=fb_nodes[p].parent) included[p]=TRUE;
    }
    int success=TRUE;
    for(int e=0;e<fb_node_count;e++) if(included[e] && fb_nodes[e].kind<FB_NODE_BRUSH && fb_nodes[e].kind!=FB_NODE_LAYER) {
        fputs(file,"{\n");
        if(e==0) fputs(file,"\"classname\" \"worldspawn\"\n");
        else fputs(file,strcat(terrain_edit(TEREDIT_ENT_GET,fb_nodes[e].id),"\n"));
        for(int n=0;n<fb_node_count;n++) if(included[n] && fb_nodes[n].kind>=FB_NODE_BRUSH) {
            int parent=fb_nodes[n].parent;
            if(parent>=0 && fb_nodes[parent].kind==FB_NODE_LAYER) parent=0;
            if(parent==e) if(!fb_clip_write_geometry(file,fb_nodes[n].model,fb_nodes[n].id)) success=FALSE;
        }
        fputs(file,"}\n");
    }
    fclose(file);memfree(included);return success;
}
string fb_clip_field(string fields,string key)
{
    int count=fb_entity_tokenize(fields);
    for(int i=0;i+1<count;i+=2) if(argv(i)==key) return argv(i+1);
    return "";
}
int fb_clip_apply_face()
{
    if(!selectedbrushcount) {fb_map_error="Select destination brushes or faces first";return FALSE;}
    brushface_t source=fb_map_items[0].faces[0];
    fb_tx_begin("Paste Face Attributes");
    for(int b=0;b<selectedbrushcount;b++) {
        int count=brush_get(selectedbrushes[b].model,selectedbrushes[b].id,__NULL__,0,__NULL__);
        if(count<4 || count>64) continue;
        brushface_t *faces=memalloc(sizeof(brushface_t)*count);int contents;
        brush_get(selectedbrushes[b].model,selectedbrushes[b].id,faces,count,&contents);
        for(int f=0;f<count;f++) if(selbrush_whole(&selectedbrushes[b]) || selbrush_face(&selectedbrushes[b],f)) {
            vector normal=faces[f].planenormal;float dist=faces[f].planedist;
            faces[f]=source;faces[f].planenormal=normal;faces[f].planedist=dist;
        }
        // Contents are brush-wide in the engine; all other attributes (UV axes,
        // offsets, material, surface flags/value) travel with the copied face.
        brush_history_edit(selectedbrushes[b].model,selectedbrushes[b].id,faces,count,fb_map_items[0].contents);memfree(faces);
    }
    fb_tx_commit();return TRUE;
}
int fb_clip_bounds(vector *bounds)
{
    bounds[0]='1000000000 1000000000 1000000000';bounds[1]=-bounds[0];int found=FALSE;
    for(int i=0;i<fb_map_count;i++) {
        fb_map_item_t *item=&fb_map_items[i];fb_poly_t poly={0};
        int count=0;vector *points=__NULL__;vector origin;
        if(item->kind==FB_NODE_BRUSH) {
            if(!fb_poly_from_faces(item->faces,item->count,&poly)) return FALSE;
            points=poly.vertices;count=poly.nv;
        } else if(item->kind==FB_NODE_PATCH) count=item->count;
        else {
            int geometry=FALSE;
            for(int child=0;child<fb_map_count;child++) if(fb_map_items[child].parent==i) geometry=TRUE;
            string classname=fb_clip_field(item->fields,"classname");
            if(!geometry && classname!="worldspawn" && classname!="func_group") {
                origin=stov(fb_clip_field(item->fields,"origin"));points=&origin;count=1;
            }
        }
        for(int p=0;p<count;p++) {
            vector point;if(item->kind==FB_NODE_PATCH) point=item->points[p].xyz;else point=points[p];
            if(!fb_geom_point(point)) {fb_poly_free(&poly);fb_map_error="Clipboard outside world bounds";return FALSE;}
            for(int axis=0;axis<3;axis++) {bounds[0][axis]=min(bounds[0][axis],point[axis]);bounds[1][axis]=max(bounds[1][axis],point[axis]);}
            found=TRUE;
        }
        fb_poly_free(&poly);
    }
    return found;
}
int fb_clip_insert(vector delta)
{
    static vector bounds[2];
    if(!fb_clip_bounds(bounds) || !fb_geom_point(bounds[0]+delta) || !fb_geom_point(bounds[1]+delta)) {
        fb_map_error="Paste would exceed world bounds";return FALSE;
    }
    int *entities=memalloc(sizeof(int)*fb_map_count);
    int *models=memalloc(sizeof(int)*fb_map_count);
    int *identities=memalloc(sizeof(int)*fb_map_count);
    fb_document_tick();int nextId=1;
    for(int n=0;n<fb_node_count;n++) nextId=max(nextId,stoi(fb_nodes[n].tbid)+1);
    for(int i=0;i<fb_map_count;i++) entities[i]=-1;
    fb_tx_begin("Paste");fb_selection_clear();int success=TRUE;
    int destination=fb_creation_model();if(!destination) success=FALSE;
    for(int i=0;i<fb_map_count;i++) if(fb_map_items[i].kind==FB_NODE_ENTITY) {
        fb_map_item_t *item=&fb_map_items[i];
        if(fb_clip_field(item->fields,"classname")=="worldspawn") {models[i]=destination;continue;}
        int id=terrain_edit(TEREDIT_ENT_ADD,"");entities[i]=id;
        fb_tx_entity_set(id,item->fields,HIST_ENTITY);
        string oldIdentity=fb_clip_field(item->fields,"_tb_id");
        if(oldIdentity!="") {identities[i]=nextId++;fb_tx_set_key(id,"_tb_id",itos(identities[i]),HIST_TREE);}
        int hasGeometry=FALSE;
        for(int child=0;child<fb_map_count;child++) if(fb_map_items[child].parent==i) hasGeometry=TRUE;
        if(hasGeometry) {
            string name=strzone(brush_model_create());
            if(name=="") {success=FALSE;strunzone(name);break;}
            precache_model(name);if(!fb_document_probe) fb_document_probe=spawn();setmodel(fb_document_probe,name);
            models[i]=fb_document_probe.modelindex;fb_tx_set_key(id,"model",name,HIST_ENTITY);strunzone(name);
            if(!models[i]) {success=FALSE;break;}
        } else if(fb_clip_field(item->fields,"_tb_type")=="") {
            vector origin=stov(fb_clip_field(item->fields,"origin"))+delta;
            fb_tx_set_key(id,"origin",fb_clip_vec(origin),HIST_ENTITY);fb_entity_select(id,TRUE);
        }
    }
    for(int i=0;i<fb_map_count && success;i++) {
        fb_map_item_t *item=&fb_map_items[i];
        if(item->kind==FB_NODE_ENTITY && entities[i]>=0) {
            string oldParent=strzone(fb_clip_field(item->fields,"_tb_group"));int newParent=0;
            for(int p=0;p<fb_map_count;p++) if(identities[p] && oldParent!="" && fb_clip_field(fb_map_items[p].fields,"_tb_id")==oldParent) newParent=identities[p];
            strunzone(oldParent);
            if(newParent) fb_tx_set_key(entities[i],"_tb_group",itos(newParent),HIST_TREE);
            else {
                int opened=fb_selection_open_node();
                fb_tx_set_key(entities[i],"_tb_group",opened>=0?fb_nodes[opened].tbid:"",HIST_TREE);
                fb_tx_set_key(entities[i],"_tb_layer","",HIST_TREE);
            }
        }
        int model=destination;if(item->parent>=0) model=models[item->parent];
        if(item->kind==FB_NODE_BRUSH) {
            for(int f=0;f<item->count;f++) {
                item->faces[f].planedist+=item->faces[f].planenormal*delta;
                item->faces[f].sbias-=item->faces[f].sdir*delta;item->faces[f].tbias-=item->faces[f].tdir*delta;
            }
            if(!brush_history_create(model,item->faces,item->count,item->contents,TRUE)) success=FALSE;
        } else if(item->kind==FB_NODE_PATCH) {
            for(int p=0;p<item->count;p++) item->points[p].xyz+=delta;
            if(!patch_history_create(model,item->points,item->patch,TRUE)) success=FALSE;
        }
    }
    if(success) fb_tx_commit();else {fb_tx_cancel();fb_map_error="Could not insert complete clipboard";}
    memfree(entities);memfree(models);memfree(identities);return success;
}
float fb_clip_command()
{
    if(argv(0)=="ftebroom_copy_file") {
        int cut=stoi(argv(1));
        if(fb_clip_export()) localcmd(strcat("ftebroom_clipboard_ready ",itos(cut && !fb_faces_count()),"\n"));
        else cvar_set("ftebroom_selection_status","Nothing copied");
        return TRUE;
    }
    int importing=argv(0)=="ftebroom_import_file";
    if(argv(0)!="ftebroom_paste_file" && !importing) return FALSE;
    int original=stoi(argv(1));
    int replaceSelected=importing&&stoi(argv(4));
    // Capture command coordinates before the MAP parser changes tokenized arguments.
    vector pixel=[cvar("ftebroom_mouse_x"),cvar("ftebroom_mouse_y"),0];
    if(importing) pixel=[stof(argv(2)),stof(argv(3)),0];
    filestream file=fopen(importing?"import-in.map":"clipboard-in.map",FILE_READ);
    if((float)file<0) {cvar_set("ftebroom_selection_status","Clipboard is unavailable");return TRUE;}
    int bytes=fsize(file);
    if(bytes<1||bytes>16777216){fclose(file);cvar_set("ftebroom_selection_status","Import MAP must be 1 byte to 16 MiB");return TRUE;}
    // Join once; repeated strcat copied the entire MAP per line.
    strbuf lines=buf_create();string line;
    while((line=fgets(file)))bufstr_add(lines,line,TRUE);
    fclose(file);string text=strzone(buf_implode(lines,"\n"));buf_del(lines);
    int success=fb_map_parse(text);
    if(success && importing && cvar("ftebroom_quake1")) {
        for(int i=0;i<fb_map_count;i++) if(fb_map_items[i].kind==FB_NODE_PATCH) {
            success=FALSE;fb_map_error="Quake 1 does not support patches";break;
        }
    }
    if(success) {
        if(fb_map_items[0].kind==-1) success=fb_clip_apply_face();
        else {
            vector delta='0 0 0';static vector bounds[2];
            success=fb_clip_bounds(bounds);
            if(success && !original) {
                vector start=unproject(pixel),end=unproject(pixel+'0 0 8192');
                vector target=start+normalize(end-start)*autocvar_ftebroom_default_distance;
                int view=cvar("ca_brush_view");
                if(view) {
                    int axis=fb_view_depth_axis(view);vector anchor='0 0 0';
                    if(fb_view_has_selection) anchor[axis]=fb_view_lastmax[axis];
                    target=fb_view_plane_point(pixel,view,anchor);
                    delta=target-(bounds[0]+bounds[1])*0.5;delta[axis]=target[axis]-bounds[1][axis];
                } else {
                    if(fb_pick(pixel)) target=fb_hits[0].point;
                    delta=target-(bounds[0]+bounds[1])*0.5;delta_z=target_z-bounds[0][2];
                }
                delta=fb_snap(bounds[0]+delta)-bounds[0];
            }
            if(success&&replaceSelected){fb_tx_begin("Replace selection with generated level");fb_delete_selection();success=fb_clip_insert(delta);if(success)fb_tx_commit();else fb_tx_cancel();}
            else if(success) success=fb_clip_insert(delta);
        }
    }
    cvar_set("ftebroom_selection_status",success?(importing?"Map imported":"Clipboard pasted"):strcat(importing?"Import: ":"Paste: ",fb_map_error));
    fb_map_clear();return TRUE;
}
