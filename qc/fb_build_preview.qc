// SPDX-License-Identifier: GPL-2.0-or-later
// Switch only the CSQC render world; the editable model and history stay alive.
int fb_build_preview_active;
float(float index) editor_renderworld = #0:editor_renderworld;
float fb_build_source_model;
float fb_build_source_fullbright,fb_build_source_clear;
string fb_build_source_clearcolour;
entity fb_build_entities;
.entity fb_build_next;
.vector fb_build_axes;
void fb_build_preview_leave()
{
    if(!fb_build_preview_active)return;
    if(!editor_renderworld(fb_build_source_model)){print("ReyondBroom: cannot restore editing world\n");return;}
    cvar_set("r_fullbright",ftos(fb_build_source_fullbright));
    cvar_set("gl_clear",ftos(fb_build_source_clear));cvar_set("r_clearcolour",fb_build_source_clearcolour);strunzone(fb_build_source_clearcolour);
    while(fb_build_entities){entity next=fb_build_entities.fb_build_next;remove(fb_build_entities);fb_build_entities=next;}
    fb_build_preview_active=FALSE;cvar_set("ftebroom_bsp_preview","0");
}
void fb_build_preview_enter(string path,int unlit)
{
    fb_build_preview_leave();
    if(substring(path,0,20)!="maps/ftebroom-build/")return;
    // BSP lightmaps and shader permutations must be initialized in lit mode,
    // rather than inheriting the editable MAP's fullbright render state.
    fb_build_source_fullbright=cvar("r_fullbright");cvar_set("r_fullbright","0");
    float index=getmodelindex(path);
    if(!index){cvar_set("r_fullbright",ftos(fb_build_source_fullbright));cvar_set("ftebroom_build_error","Cannot load compiled BSP");return;}
    fb_build_source_model=world.modelindex;if(!fb_build_source_model)fb_build_source_model=1;
    if(!editor_renderworld(index)){cvar_set("r_fullbright",ftos(fb_build_source_fullbright));cvar_set("ftebroom_build_error","Cannot load compiled BSP");return;}
    fb_build_source_clear=cvar("gl_clear");fb_build_source_clearcolour=strzone(cvar_string("r_clearcolour"));
    cvar_set("r_clearcolour","0 0 0");cvar_set("gl_clear","1");
    fb_build_preview_active=TRUE;cvar_set("ftebroom_bsp_preview","1");
    path=strzone(path);int entityCount=0;
    getentitytoken("");
    while(getentitytoken()=="{") {
        string modelPath="",positionText="",anglesText="",yawText="",className="",key;
        float modelScale=1,modelSkin=0,modelFrame=0;vector modelAxes='1 1 1';string scalarText="";
        while((key=getentitytoken())!="}" && key!="") {
            key=strzone(key);string value=getentitytoken();
            if(key=="model")modelPath=strzone(value);
            if(key=="origin")positionText=strzone(value);
            if(key=="angles")anglesText=strzone(value);
            if(key=="angle")yawText=strzone(value);
            if(key=="classname")className=strzone(value);
            if(key=="modelscale")modelScale=stof(value);
            if(key=="scale")scalarText=strzone(value);
            if(key=="modelscale_vec")modelAxes=stov(value);
            if(key=="skin")modelSkin=stof(value);
            if(key=="frame")modelFrame=stof(value);
            strunzone(key);
        }
        int submodel=substring(modelPath,0,1)=="*";
        int external=FALSE;
        if(modelPath!=""&&!submodel)if(className=="func_model"||className=="misc_model"||className=="cycler")external=TRUE;
        // q3map2 bakes misc_model into the world: drawing it again doubles it.
        if(className=="misc_model"&&cvar("ftebroom_game_profile")<2)external=FALSE;
        if(submodel||external) {
            entity item=spawn();setmodel(item,submodel?strcat(modelPath,":",path):modelPath);
            if(!item.modelindex){remove(item);strunzone(modelPath);strunzone(positionText);strunzone(anglesText);strunzone(yawText);strunzone(className);strunzone(scalarText);continue;}
            item.origin=stov(positionText);entityCount++;
            item.angles=stov(anglesText);if(anglesText=="")if(external||!cvar("ftebroom_quake1"))item.angles_y=stof(yawText);
            if(external){
                if(scalarText!="")modelScale=stof(scalarText);if(modelScale==0)modelScale=1;
                item.scale=modelScale;item.fb_build_axes=modelAxes;item.skin=modelSkin;item.frame=modelFrame;
                if(unlit)item.effects|=EF_FULLBRIGHT;
            }
            item.fb_build_next=fb_build_entities;fb_build_entities=item;
        }
        strunzone(modelPath);strunzone(positionText);strunzone(anglesText);strunzone(yawText);strunzone(className);strunzone(scalarText);
    }
    strunzone(path);cvar_set("ftebroom_bsp_entities",itos(entityCount));
}
void fb_build_preview_draw()
{
    for(entity item=fb_build_entities;item;item=item.fb_build_next){
        float savedScale=item.scale;
        if(item.fb_build_axes!='0 0 0' && item.fb_build_axes!='1 1 1'){
            makevectors(item.angles);v_forward*=item.fb_build_axes_x*savedScale;v_right*=item.fb_build_axes_y*savedScale;v_up*=item.fb_build_axes_z*savedScale;
            item.renderflags=RF_USEAXIS;item.scale=1;
        }
        addentity(item);item.scale=savedScale;
    }
}

