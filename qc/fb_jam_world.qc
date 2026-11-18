// SPDX-License-Identifier: GPL-2.0-or-later
// Preview geometry and players never enter the editable brush or entity lists.
typedef struct {string material; int first,count;} fb_jam_face_t;
fb_jam_face_t *fb_jam_faces;
vector *fb_jam_vertices;
vector *fb_jam_uv;
int fb_jam_faces_count,fb_jam_vertices_count;
.entity fb_jam_next;
.entity fb_jam_pending_next;
.float fb_jam_keep,fb_jam_new;
fb_jam_face_t *fb_jam_pending_faces;
vector *fb_jam_pending_vertices;
vector *fb_jam_pending_uv;
var vector autocvar_ftebroom_jam_plot_half_size='1024 1024 512';
entity fb_jam_models;
entity fb_jam_players[25];
int fb_jam_scene_seen;

void fb_jam_scene_clear()
{
    for(int i=0;i<fb_jam_faces_count;i++)strunzone(fb_jam_faces[i].material);
    fb_jam_faces_count=0;fb_jam_vertices_count=0;
    while(fb_jam_models){entity item=fb_jam_models;fb_jam_models=item.fb_jam_next;remove(item);}
    cvar_set("ftebroom_jam_render_faces","0");cvar_set("ftebroom_jam_render_models","0");
}

int fb_jam_point_inside(vector p)
{
    vector halfSize=autocvar_ftebroom_jam_plot_half_size;
    return fabs(p_x)<=halfSize_x+.002 && fabs(p_y)<=halfSize_y+.002 && fabs(p_z)<=halfSize_z+.002;
}

int fb_jam_brush_inside(int model,int id)
{
    vector bounds[2];
    if(brush_getfacepoints(model,id,0,bounds,2)!=2)return TRUE;
    return fb_jam_point_inside(bounds[0]) && fb_jam_point_inside(bounds[1]);
}

int fb_jam_entity_inside(int id)
{
    string fields=terrain_edit(TEREDIT_ENT_GET,id);
    if(fields=="")return TRUE;
    int count=fb_entity_tokenize(fields);
    for(int i=0;i+1<count;i+=2)if(argv(i)=="origin")return fb_jam_point_inside(stov(argv(i+1)));
    return TRUE;
}

int fb_jam_validate_transaction(int first,int last)
{
    if(!cvar("ftebroom_jam_world"))return TRUE;
    for(int i=first;i<last;i++){
        history_t *record=&historyring[i&(historyring_length-1)];
        if(record->kind==HIST_GEOMETRY && !record->wasdelete)
            if(record->patchdata || !fb_jam_brush_inside(record->brushmodel,record->id))return FALSE;
        if(record->kind==HIST_TERRAIN)return FALSE;
        if(record->kind==HIST_ENTITY && record->entityindex>0)
            if(!fb_jam_entity_inside(record->entityindex))return FALSE;
        if(record->kind==HIST_MOVE_BATCH){
            fb_batch_move_t *batch=record->batch;
            for(int n=0;n<batch->brushCount;n++)
                if(!fb_jam_brush_inside(batch->ids[n*2],batch->ids[n*2+1]))return FALSE;
            for(int n=0;n<batch->pointCount;n++)
                if(!fb_jam_point_inside(batch->points[n].after))return FALSE;
        }
    }
    return TRUE;
}

void fb_jam_capture()
{
    static int seen=-1;
    int request=cvar("ftebroom_jam_capture");
    if(!cvar("ftebroom_jam_world") || request<=0 || request==seen)return;
    seen=request;
    fb_document_tick();
    filestream file=fopen("jam-world-draft.txt",FILE_WRITE);
    if((float)file<0)return;
    fputs(file,sprintf("RBJAM1 %i\n",request));
    static brushface_t faces[64];
    static vector points[64];
    int total=0;
    for(int i=2;i<fb_node_count && total<22000;i++)if(fb_nodes[i].kind==FB_NODE_BRUSH){
        int model=fb_nodes[i].model,id=fb_nodes[i].id;
        int count=brush_get(model,id,faces,64,__NULL__);
        for(int f=0;f<min(count,64) && total<22000;f++){
            int vertices=brush_getfacepoints(model,id,f+1,points,64);
            if(vertices<3 || vertices>64)continue;
            string material=faces[f].shadername;
            if(material=="" || strstrofs(material," ",0)>=0)material="missing";
            vector size=fb_material_image_size(material);
            if(size_x<=0 || size_y<=0)size='64 64 0';
            fputs(file,sprintf("%s %i",material,vertices));
            for(int v=0;v<vertices;v++){
                vector p=points[v];
                float u=(p*faces[f].sdir+faces[f].sbias)/size_x;
                float t=(p*faces[f].tdir+faces[f].tbias)/size_y;
                fputs(file,sprintf(" %.7g %.7g %.7g %.6g %.6g",p_x,p_y,p_z,u,t));
            }
            fputs(file,"\n");
            total+=vertices;
        }
    }
    fputs(file,"RBJAM-END\n");
    fclose(file);
}

// Stage a complete replacement before releasing anything currently visible.
// Existing BSP entities retain their identity across edits to a different plot.
void fb_jam_scene_load()
{
    int current=cvar("ftebroom_jam_scene");
    if(current==fb_jam_scene_seen)return;
    if(!current){fb_jam_scene_clear();fb_jam_scene_seen=0;return;}
    filestream file=fopen("jam-world-scene.txt",FILE_READ);
    if((float)file<0)return;
    string line=fgets(file);
    if(tokenize(line)!=3 || argv(0)!="RBJAMSCENE" || argv(1)!="1" || stoi(argv(2))!=current){fclose(file);return;}
    if(!fb_jam_pending_faces)fb_jam_pending_faces=memalloc(sizeof(fb_jam_face_t)*32768);
    if(!fb_jam_pending_vertices){
        fb_jam_pending_vertices=memalloc(sizeof(vector)*96000);
        fb_jam_pending_uv=memalloc(sizeof(vector)*96000);
    }
    int faceCount=0,vertexCount=0,modelCount=0,complete=FALSE;
    entity pending=world;
    while((line=fgets(file))){
        int argc=tokenize(line);
        if(argc==2 && argv(0)=="RBJAMSCENE-END" && stoi(argv(1))==current){complete=TRUE;break;}
        if(argc==6 && argv(0)=="model"){
            if(++modelCount>64)break;
            string modelPath=argv(1);
            vector position=[stof(argv(2)),stof(argv(3)),stof(argv(4))];
            float yaw=stof(argv(5));
            entity item=world;
            for(entity old=fb_jam_models;old;old=old.fb_jam_next)
                if(!old.fb_jam_keep && old.model==modelPath && old.origin==position && old.angles_y==yaw){item=old;break;}
            if(!item){
                item=spawn();setmodel(item,modelPath);
                if(!item.modelindex){remove(item);break;}
                item.fb_jam_new=TRUE;item.solid=SOLID_NOT;
                item.origin=position;item.angles_y=yaw;
            }
            item.fb_jam_keep=TRUE;
            item.fb_jam_pending_next=pending;pending=item;
        }else if(argc==3 && argv(0)=="face" && faceCount<32768){
            int count=stoi(argv(2));
            if(count<3 || count>64 || vertexCount+count>96000)break;
            fb_jam_face_t *face=&fb_jam_pending_faces[faceCount];
            face->material=strzone(argv(1));face->first=vertexCount;face->count=count;
            int valid=TRUE;
            for(int j=0;j<count;j++){
                if(!(line=fgets(file)) || tokenize(line)!=5){valid=FALSE;break;}
                fb_jam_pending_vertices[vertexCount]=[stof(argv(0)),stof(argv(1)),stof(argv(2))];
                fb_jam_pending_uv[vertexCount++]=[stof(argv(3)),stof(argv(4)),0];
            }
            if(!valid){strunzone(face->material);break;}
            faceCount++;
        }else break;
    }
    fclose(file);
    if(!complete){
        for(int i=0;i<faceCount;i++)strunzone(fb_jam_pending_faces[i].material);
        while(pending){entity item=pending;pending=item.fb_jam_pending_next;item.fb_jam_keep=FALSE;if(item.fb_jam_new)remove(item);}
        return;
    }
    for(int i=0;i<fb_jam_faces_count;i++)strunzone(fb_jam_faces[i].material);
    fb_jam_face_t *previousFaces=fb_jam_faces;
    vector *previousVertices=fb_jam_vertices;
    vector *previousUv=fb_jam_uv;
    fb_jam_faces=fb_jam_pending_faces;fb_jam_pending_faces=previousFaces;
    fb_jam_vertices=fb_jam_pending_vertices;fb_jam_pending_vertices=previousVertices;
    fb_jam_uv=fb_jam_pending_uv;fb_jam_pending_uv=previousUv;
    fb_jam_faces_count=faceCount;fb_jam_vertices_count=vertexCount;
    while(fb_jam_models){entity item=fb_jam_models;fb_jam_models=item.fb_jam_next;if(!item.fb_jam_keep)remove(item);}
    fb_jam_models=pending;
    for(entity item=fb_jam_models;item;item=item.fb_jam_next){
        item.fb_jam_next=item.fb_jam_pending_next;item.fb_jam_keep=FALSE;item.fb_jam_new=FALSE;
    }
    fb_jam_scene_seen=current;
    cvar_set("ftebroom_jam_render_faces",itos(faceCount));
    cvar_set("ftebroom_jam_render_models",itos(modelCount));
}

void fb_jam_draw_players()
{
    string poses=cvar_string("ftebroom_jam_players");
    int start=0;
    while(start<strlen(poses)) {
        int end=strstrofs(poses,";",start);
        if(end<0)break;
        int fields=tokenize(substring(poses,start,end-start));
        start=end+1;
        if(fields!=5)continue;
        int slot=stoi(argv(0));
        if(slot<0 || slot>=25)continue;
        entity avatar=fb_jam_players[slot];
        if(!avatar) {
            avatar=spawn();setmodel(avatar,"progs/player.mdl");
            if(!avatar.modelindex){remove(avatar);continue;}
            avatar.solid=SOLID_NOT;avatar.effects=EF_FULLBRIGHT;
            fb_jam_players[slot]=avatar;
        }
        avatar.origin=[stof(argv(1)),stof(argv(2)),stof(argv(3))];
        avatar.angles_y=stof(argv(4));
        addentity(avatar);
    }
}

void fb_jam_border(vector center,vector halfSize,vector colour)
{
    vector corner[8];
    for(int i=0;i<8;i++)
        corner[i]=center+[(i&1)?halfSize_x:-halfSize_x,(i&2)?halfSize_y:-halfSize_y,(i&4)?halfSize_z:-halfSize_z];
    for(int i=0;i<8;i++)for(int axis=0;axis<3;axis++)if(!(i&(1i<<axis))){
        R_BeginPolygon("ftebroom/jamborder");
        R_PolygonVertex(corner[i],'0 0',colour,.85);
        R_PolygonVertex(corner[i|(1i<<axis)],'0 0',colour,.85);R_EndPolygon();
    }
}

void fb_jam_world_draw()
{
    if(!cvar("ftebroom_jam_world"))return;
    fb_jam_scene_load();
    shaderforname("ftebroom/jamborder","{\ncull none\nsort additive\n{\nmap textures/ftebroom/overlay.tga\nblendfunc blend\nrgbgen vertex\nalphagen vertex\ndepthfunc always\n}\n}");
    vector own=stov(cvar_string("ftebroom_jam_plot_center"));
    for(int y=0;y<5;y++)for(int x=0;x<5;x++){
        if(x>=1 && x<=3 && y>=1 && y<=3)continue;
        vector center=[x*2048,-y*2048,0]-own;
        fb_jam_border(center,autocvar_ftebroom_jam_plot_half_size,center=='0 0 0'?'.20 1 .76':'.15 .62 1');
    }
    fb_jam_border('4096 -4096 0'-own,[3072,3072,autocvar_ftebroom_jam_plot_half_size_z],'.7 .45 .99');
    if(cvar("ca_brush_view"))return;
    fb_jam_draw_players();
    for(entity item=fb_jam_models;item;item=item.fb_jam_next)addentity(item);
    for(int i=0;i<fb_jam_faces_count;i++){
        fb_jam_face_t *face=&fb_jam_faces[i];
        vector *points=&fb_jam_vertices[face->first];
        vector normal=normalize(crossproduct(points[1]-points[0],points[2]-points[0]));
        string shader=fb_surface_preview_shader(face->material,1);
        float shade=fb_surface_shade(normal);
        R_BeginPolygon(shader,3);
        for(int p=0;p<face->count;p++)
            R_PolygonVertex(points[p],fb_jam_uv[face->first+p],'1 1 1'*shade,1);
        R_EndPolygon();
    }
}
