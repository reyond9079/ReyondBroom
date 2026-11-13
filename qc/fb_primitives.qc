// SPDX-License-Identifier: GPL-2.0-or-later
// Shape generation is pure until all convex parts pass validation.
vector fb_primitive_outer[96],fb_primitive_inner[96];
int fb_primitive_sides,fb_primitive_axis,fb_primitive_u,fb_primitive_v;
var string autocvar_ftebroom_shape="cuboid";
var string autocvar_ftebroom_shape_circle="edge";
var float autocvar_ftebroom_shape_axis=2,autocvar_ftebroom_shape_sides=8,autocvar_ftebroom_shape_hollow=0;
var float autocvar_ftebroom_shape_thickness=16,autocvar_ftebroom_shape_rings=8,autocvar_ftebroom_shape_accuracy=0;
var float autocvar_ftebroom_shape_step=16,autocvar_ftebroom_shape_stairdir=0,autocvar_ftebroom_shape_spandrel=0,autocvar_ftebroom_shape_group=1;
vector fb_primitive_map(vector point,vector low)
{
    vector mapped=low;mapped[fb_primitive_u]+=point_x;mapped[fb_primitive_v]+=point_y;mapped[fb_primitive_axis]+=point_z;return mapped;
}
int fb_primitive_circle(float width,float height)
{
    int sides=bound(3,rint(autocvar_ftebroom_shape_sides),96);string mode=autocvar_ftebroom_shape_circle;
    if(mode=="scalable") {
        static vector base[12]={'-0.25 1 0','-0.75 0.75 0','-1 0.25 0','-1 -0.25 0','-0.75 -0.75 0','-0.25 -1 0','0.25 -1 0','0.75 -0.75 0','1 -0.25 0','1 0.25 0','0.75 0.75 0','0.25 1 0'};
        memcpy(fb_primitive_outer,base,sizeof(base));int count=12;
        while(count<sides && count<96) {
            static vector next[96];
            for(int i=0;i<count;i++) {
                int prev=i-1,after=i+1;if(prev<0) prev=count-1;if(after==count) after=0;
                next[i*2]=fb_primitive_outer[prev]*0.25+fb_primitive_outer[i]*0.75;
                next[i*2+1]=fb_primitive_outer[i]*0.75+fb_primitive_outer[after]*0.25;
            }
            count*=2;memcpy(fb_primitive_outer,next,count*sizeof(vector));
        }
        sides=count;float size=min(width,height);
        for(int i=0;i<sides;i++) {
            vector p=(fb_primitive_outer[i]+'1 1 0')*(size*0.5);
            if(p_x>size*0.5) p_x+=width-size;if(p_y>size*0.5) p_y+=height-size;fb_primitive_outer[i]=p;
        }
    } else for(int i=0;i<sides;i++) {
        float offset=0,radius=1;if(mode=="edge") {offset=0.5;radius=1/cos(M_PI/sides);}
        float angle=(i+offset)*2*M_PI/sides-M_PI*0.5;
        fb_primitive_outer[i]=[width*(0.5+0.5*cos(angle)*radius),height*(0.5+0.5*sin(angle)*radius),0];
    }
    fb_primitive_sides=sides;return TRUE;
}
int fb_primitive_inset(float thickness,float width,float height,int arch)
{
    float fullHeight=height;if(arch)fullHeight*=2;
    if(!(thickness>0) || !(width>2*thickness) || !(fullHeight>2*thickness)) {fb_geom_error="Thickness leaves no inner opening";return FALSE;}
    // Generate the same circle mode in an inset box, including scalable circles.
    static vector saved[96];int count=fb_primitive_sides;
    memcpy(saved,fb_primitive_outer,count*sizeof(vector));
    fb_primitive_circle(width-2*thickness,fullHeight-2*thickness);
    for(int i=0;i<count;i++) {
        fb_primitive_inner[i]=fb_primitive_outer[i]+[thickness,thickness,0];
        if(arch)fb_primitive_inner[i][1]-=height;
    }
    memcpy(fb_primitive_outer,saved,count*sizeof(vector));return TRUE;
}
int fb_primitive_hull(vector *points,int count)
{
    fb_poly_t poly={0};int valid=fb_hull(points,count,&poly);
    if(valid) valid=fb_csg_append(&poly,0,CONTENTBIT_SOLID);fb_poly_free(&poly);return valid;
}
int fb_primitive_box(vector low,vector high,fb_poly_t *poly)
{
    vector points[8];for(int v=0;v<8;v++) for(int a=0;a<3;a++) {points[v][a]=low[a];if(v&(1i<<a)) points[v][a]=high[a];}
    return fb_hull(points,8,poly);
}
int fb_primitive_build(string shape,vector low,vector high)
{
    fb_csg_clear();fb_csg_results=memalloc(2048*sizeof(fb_csg_item_t));vector size=high-low;
    if(!fb_geom_finite(autocvar_ftebroom_shape_axis) || !fb_geom_finite(autocvar_ftebroom_shape_sides) || !fb_geom_finite(autocvar_ftebroom_shape_rings) || !fb_geom_finite(autocvar_ftebroom_shape_step) || !fb_geom_finite(autocvar_ftebroom_shape_thickness) || !fb_geom_finite(autocvar_ftebroom_shape_accuracy)) {fb_geom_error="Shape parameters must be finite";return FALSE;}
    for(int a=0;a<3;a++) if(!(size[a]>=0.125) || fabs(low[a])>1000000 || fabs(high[a])>1000000) {fb_geom_error="Shape bounds must have volume";return FALSE;}
    fb_primitive_axis=bound(0,rint(autocvar_ftebroom_shape_axis),2);
    fb_primitive_u=(fb_primitive_axis+1)%3;fb_primitive_v=(fb_primitive_axis+2)%3;
    if(fb_primitive_axis==1) {fb_primitive_u=0;fb_primitive_v=2;}
    if(shape=="cuboid") {fb_poly_t box={0};int ok=fb_primitive_box(low,high,&box);if(ok) ok=fb_csg_append(&box,0,CONTENTBIT_SOLID);fb_poly_free(&box);return ok;}
    if(shape=="stairs") {
        float step=max(1,autocvar_ftebroom_shape_step);int stepCount=ceil(size_z/step),direction=bound(0,rint(autocvar_ftebroom_shape_stairdir),3),stepAxis=0;
        if(direction>=2) stepAxis=1;if(stepCount>2048) {fb_geom_error="Too many stairs (maximum 2048)";return FALSE;}
        for(int i=0;i<stepCount;i++) {
            vector a=low,b=high;float start=(float)i/stepCount,end=(float)(i+1)/stepCount;
            if(direction&1) {start=1-end;end=1-(float)i/stepCount;}
            a[stepAxis]=low[stepAxis]+size[stepAxis]*start;b[stepAxis]=low[stepAxis]+size[stepAxis]*end;b_z=min(high_z,low_z+step*(i+1));
            fb_poly_t stepPoly={0};int stepValid=fb_primitive_box(a,b,&stepPoly);if(stepValid) stepValid=fb_csg_append(&stepPoly,0,CONTENTBIT_SOLID);fb_poly_free(&stepPoly);if(!stepValid) return FALSE;
        }
        return TRUE;
    }
    if(shape=="icosphere") {
        if(autocvar_ftebroom_shape_accuracy>0) {fb_geom_error="Icosahedron accuracy 1 needs 80 faces; this engine supports 64. Use accuracy 0";return FALSE;}
        float phi=(1+sqrt(5))*0.5;vector icoPoints[12];int icoCount=0;
        for(int plane=0;plane<3;plane++) for(int a=-1;a<=1;a+=2) for(int b=-1;b<=1;b+=2) {
            vector p='0 0 0';p[plane]=a;p[(plane+1)%3]=b*phi;p=normalize(p);
            for(int axis=0;axis<3;axis++) icoPoints[icoCount][axis]=low[axis]+size[axis]*(0.5+0.5*p[axis]);icoCount++;
        }
        return fb_primitive_hull(icoPoints,icoCount);
    }
    if(shape!="cylinder" && shape!="cone" && shape!="uvsphere" && shape!="arch") {fb_geom_error="Unknown shape";return FALSE;}
    int arch=shape=="arch";float width=size[fb_primitive_u],height=size[fb_primitive_v],depth=size[fb_primitive_axis];
    fb_primitive_circle(width,height*(arch?2:1));
    if(arch) for(int i=0;i<fb_primitive_sides;i++) fb_primitive_outer[i][1]-=height;
    static vector points[512];int count=0;
    if(shape=="uvsphere") {
        int rings=bound(2,rint(autocvar_ftebroom_shape_rings),32);
        if(fb_primitive_sides*rings>64) {fb_geom_error="Sphere sides times rings must not exceed 64 faces";return FALSE;}
        points[count++]=fb_primitive_map([width*0.5,height*0.5,0],low);
        for(int r=1;r<rings;r++) {
            float angle=M_PI*r/rings;
            for(int i=0;i<fb_primitive_sides;i++) {
                vector p=fb_primitive_outer[i];p_x=width*0.5+(p_x-width*0.5)*sin(angle);p_y=height*0.5+(p_y-height*0.5)*sin(angle);p_z=depth*(0.5-0.5*cos(angle));points[count++]=fb_primitive_map(p,low);
            }
        }
        points[count++]=fb_primitive_map([width*0.5,height*0.5,depth],low);return fb_primitive_hull(points,count);
    }
    if(arch || (shape=="cylinder" && autocvar_ftebroom_shape_hollow)) {
        if(!fb_primitive_inset(autocvar_ftebroom_shape_thickness,width,height,arch)) return FALSE;
        if(arch && autocvar_ftebroom_shape_spandrel) {
            fb_poly_t archBox={0},cutter={0};int archValid=fb_primitive_box(low,high,&archBox);
            for(int i=0;i<fb_primitive_sides;i++) {vector p=fb_primitive_inner[i];points[count++]=fb_primitive_map(p,low);p_z=depth;points[count++]=fb_primitive_map(p,low);}
            if(archValid) archValid=fb_hull(points,count,&cutter);
            if(archValid) archValid=fb_csg_difference(&archBox,&cutter,0,CONTENTBIT_SOLID);
            fb_poly_free(&archBox);fb_poly_free(&cutter);return archValid;
        }
        for(int i=0;i<fb_primitive_sides;i++) {
            int next=i+1;if(next==fb_primitive_sides) next=0;count=0;
            vector quad[4];quad[0]=fb_primitive_outer[i];quad[1]=fb_primitive_outer[next];quad[2]=fb_primitive_inner[next];quad[3]=fb_primitive_inner[i];
            for(int q=0;q<4;q++) {vector p=quad[q];points[count++]=fb_primitive_map(p,low);p_z=depth;points[count++]=fb_primitive_map(p,low);}
            fb_poly_t part={0};int valid=fb_hull(points,count,&part);
            if(valid && arch) {
                brushface_t plane={0};plane.planenormal[fb_primitive_v]=-1;plane.planedist=-low[fb_primitive_v];
                fb_poly_t discarded={0},kept={0};valid=fb_csg_split(&part,plane,&discarded,&kept);fb_poly_free(&part);fb_poly_free(&discarded);part=kept;
            }
            if(valid && part.nf) valid=fb_csg_append(&part,0,CONTENTBIT_SOLID);fb_poly_free(&part);if(!valid) return FALSE;
        }
        return fb_csg_count>0;
    }
    int faceCount=fb_primitive_sides+1;if(shape=="cylinder") faceCount++;
    if(faceCount>64) {fb_geom_error="Cylinder/cone resolution exceeds the 64-face limit";return FALSE;}
    for(int i=0;i<fb_primitive_sides;i++) {
        vector p=fb_primitive_outer[i];points[count++]=fb_primitive_map(p,low);
        if(shape=="cylinder") {p_z=depth;points[count++]=fb_primitive_map(p,low);}
    }
    if(shape=="cone") points[count++]=fb_primitive_map([width*0.5,height*0.5,depth],low);
    return fb_primitive_hull(points,count);
}
int fb_primitive_rotate_results()
{
    for(int b=0;b<fb_csg_count;b++) {
        fb_poly_t *p=&fb_csg_results[b].poly;
        for(int v=0;v<p->nv;v++){p->vertices[v]=fb_grid_world(p->vertices[v]);if(!fb_geom_point(p->vertices[v]))return FALSE;}
        for(int f=0;f<p->nf;f++){p->faces[f].attr.planenormal=fb_grid_direction_world(p->faces[f].attr.planenormal);p->faces[f].attr.sdir=fb_grid_direction_world(p->faces[f].attr.sdir);p->faces[f].attr.tdir=fb_grid_direction_world(p->faces[f].attr.tdir);p->faces[f].attr.planedist+=p->faces[f].attr.planenormal*autocvar_ftebroom_grid_origin;p->faces[f].attr.sbias-=p->faces[f].attr.sdir*autocvar_ftebroom_grid_origin;p->faces[f].attr.tbias-=p->faces[f].attr.tdir*autocvar_ftebroom_grid_origin;}
    }
    return TRUE;
}
int fb_primitive_create(vector low,vector high,int replace)
{
    if(!fb_primitive_build(autocvar_ftebroom_shape,low,high)) {
        cvar_set("ftebroom_selection_status",strcat("Shape rejected: ",fb_geom_error));fb_csg_clear();return FALSE;
    }
    if(!replace&&!fb_primitive_rotate_results()){fb_csg_clear();cvar_set("ftebroom_selection_status","Rotated shape exceeds world bounds");return FALSE;}
    int total=selectedbrushcount;selbrush_t *selection=memalloc(total*sizeof(selbrush_t));memcpy(selection,selectedbrushes,total*sizeof(selbrush_t));
    fb_tx_begin("Create Shape");int model=0;
    if(replace && total) model=selection[0].model;else model=fb_creation_model();
    int valid=model!=0;brush_deselectall();fb_restore_entity_selection(-1);
    if(replace) for(int i=0;i<total;i++) brush_history_delete(selection[i].model,selection[i].id);
    for(int b=0;b<fb_csg_count && valid;b++) {
        static brushface_t faces[64];fb_poly_t *poly=&fb_csg_results[b].poly;
        for(int f=0;f<poly->nf;f++) faces[f]=poly->faces[f].attr;
        if(!brush_history_create(model,faces,poly->nf,CONTENTBIT_SOLID,TRUE)) valid=FALSE;
    }
    if(valid && autocvar_ftebroom_shape_group && fb_csg_count>1) {
        fb_document_rebuild();int parent=1;
        for(int n=2;n<fb_node_count;n++) if(fb_nodes[n].kind>=FB_NODE_BRUSH && fb_nodes[n].model==model) {parent=fb_nodes[n].parent;break;}
        if(parent>=2 && fb_nodes[parent].kind==FB_NODE_ENTITY) parent=fb_nodes[parent].parent;
        int group=fb_organize_new("group","Shape",parent);fb_document_rebuild();
        valid=fb_organize_move(group,fb_organize_node(group),FALSE);
    }
    if(valid) fb_tx_commit();else fb_tx_cancel();memfree(selection);fb_csg_clear();return valid;
}
void fb_primitive_apply()
{
    fb_tool_refresh();if(!selectedbrushcount) return;
    vector low='1000000 1000000 1000000',high=-low,bounds[2];
    for(int b=0;b<selectedbrushcount;b++) {
        if(!brush_get(selectedbrushes[b].model,selectedbrushes[b].id,__NULL__,0,__NULL__)) {cvar_set("ftebroom_selection_status","Shape replacement requires brushes");return;}
        brush_getfacepoints(selectedbrushes[b].model,selectedbrushes[b].id,0,bounds,2);
        for(int a=0;a<3;a++) {low[a]=min(low[a],bounds[0][a]);high[a]=max(high[a],bounds[1][a]);}
    }
    fb_primitive_create(low,high,TRUE);
}
void fb_primitive_preview(vector low,vector high)
{
    int ortho=cvar("ca_brush_view");
    if(!ortho)
        shaderforname("ftebroom/shape-preview","{\ncull none\nsort opaque\n{\nmap $whiteimage\nrgbgen vertex\ndepthwrite\n}\n}");
    if(fb_primitive_build(autocvar_ftebroom_shape,low,high)&&fb_primitive_rotate_results()) for(int b=0;b<fb_csg_count;b++) {
        static brushface_t faces[64];fb_poly_t *poly=&fb_csg_results[b].poly;
        for(int f=0;f<poly->nf;f++) faces[f]=poly->faces[f].attr;
        if(ortho)DrawQCBrushWireframe(faces,poly->nf,"chop",'1 0.8 0',1);
        else DrawQCBrushSolid(faces,poly->nf,"ftebroom/shape-preview",'0.76 0.55 0.17',1);
    }
    fb_csg_clear();
}
