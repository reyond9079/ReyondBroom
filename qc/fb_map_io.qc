// SPDX-License-Identifier: GPL-2.0-or-later
// Clipboard MAP parsing is data-only and validates every object before insertion.
string fb_map_text,fb_map_token,fb_map_error;
int fb_map_cursor,fb_map_length;
// FTE substring/str2chr inspect the source string. Keep character-level reads
// bounded instead of rescanning a multi-megabyte generated MAP for every byte.
string fb_map_window;
int fb_map_window_start;
string fb_map_slice(int start,int length)
{
    if(start<0||start>=fb_map_length||length<=0)return "";
    if(length>1024)return substring(fb_map_text,start,length);
    if(fb_map_window_start<0||start<fb_map_window_start||start+length>fb_map_window_start+strlen(fb_map_window)){
        strunzone(fb_map_window);fb_map_window_start=start;
        fb_map_window=strzone(substring(fb_map_text,start,2048));
    }
    return substring(fb_map_window,start-fb_map_window_start,length);
}
typedef struct {
    int kind,parent,count,contents;
    string fields;
    brushface_t *faces;
    patchvert_t *points;
    patchinfo_t patch;
} fb_map_item_t;
fb_map_item_t *fb_map_items;
int fb_map_count;
void fb_map_clear()
{
    for(int i=0;i<fb_map_count;i++) {
        fb_map_item_t *item=&fb_map_items[i];
        strunzone(item->fields);
        if(item->faces) for(int f=0;f<item->count;f++) strunzone(item->faces[f].shadername);
        if(item->kind==FB_NODE_PATCH) strunzone(item->patch.shadername);
        memfree(item->faces);memfree(item->points);
    }
    memfree(fb_map_items);fb_map_items=__NULL__;fb_map_count=0;
    strunzone(fb_map_text);strunzone(fb_map_token);fb_map_text=fb_map_token="";
    strunzone(fb_map_window);fb_map_window="";fb_map_window_start=-1;
}
void fb_map_next()
{
    strunzone(fb_map_token);fb_map_token="";
    while(fb_map_cursor<fb_map_length) {
        int code=str2chr(fb_map_slice(fb_map_cursor,1),0);
        if(code<=32) {fb_map_cursor++;continue;}
        if(fb_map_slice(fb_map_cursor,2)=="//") {
            while(fb_map_cursor<fb_map_length && str2chr(fb_map_slice(fb_map_cursor,1),0)!=10) fb_map_cursor++;
            continue;
        }
        if(fb_map_slice(fb_map_cursor,2)=="/*") {
            fb_map_cursor+=2;
            while(fb_map_cursor<fb_map_length && fb_map_slice(fb_map_cursor,2)!="*/") fb_map_cursor++;
            if(fb_map_cursor>=fb_map_length) fb_map_error="Unterminated comment";
            fb_map_cursor+=2;continue;
        }
        break;
    }
    if(fb_map_cursor>=fb_map_length) return;
    string first=fb_map_slice(fb_map_cursor++,1);
    if(first=="{" || first=="}" || first=="(" || first==")" || first=="[" || first=="]") {fb_map_token=strzone(first);return;}
    int start=fb_map_cursor-1;
    if(first=="\"") {
        string value="";int closed=FALSE;
        while(fb_map_cursor<fb_map_length) {
            string ch=fb_map_slice(fb_map_cursor++,1);
            if(ch=="\"") {closed=TRUE;break;}
            if(ch=="\\" && fb_map_cursor<fb_map_length) {
                ch=fb_map_slice(fb_map_cursor++,1);
                if(ch=="n") ch="\n";
            }
            value=strcat(value,ch);
        }
        if(!closed) fb_map_error="Unterminated string";
        fb_map_token=strzone(value);return;
    }
    while(fb_map_cursor<fb_map_length) {
        string nextChar=fb_map_slice(fb_map_cursor,1);
        if(str2chr(nextChar,0)<=32 || nextChar=="{" || nextChar=="}" || nextChar=="(" || nextChar==")" || nextChar=="[" || nextChar=="]") break;
        fb_map_cursor++;
    }
    fb_map_token=strzone(fb_map_slice(start,fb_map_cursor-start));
}
int fb_map_take(string expected)
{
    if(fb_map_token!=expected) {fb_map_error=strcat("Expected ",expected);return FALSE;}
    fb_map_next();return TRUE;
}
float fb_map_number()
{
    int digits=0,pos=0,length=strlen(fb_map_token);
    if(substring(fb_map_token,pos,1)=="+" || substring(fb_map_token,pos,1)=="-") pos++;
    while(pos<length && str2chr(fb_map_token,pos)>=48 && str2chr(fb_map_token,pos)<=57) {digits++;pos++;}
    if(substring(fb_map_token,pos,1)==".") {
        pos++;
        while(pos<length && str2chr(fb_map_token,pos)>=48 && str2chr(fb_map_token,pos)<=57) {digits++;pos++;}
    }
    if(substring(fb_map_token,pos,1)=="e" || substring(fb_map_token,pos,1)=="E") {
        pos++;int exponent=0;
        if(substring(fb_map_token,pos,1)=="+" || substring(fb_map_token,pos,1)=="-") pos++;
        while(pos<length && str2chr(fb_map_token,pos)>=48 && str2chr(fb_map_token,pos)<=57) {exponent++;pos++;}
        if(!exponent) fb_map_error="Invalid exponent";
    }
    if(pos!=length) fb_map_error="Invalid number";
    float value=stof(fb_map_token);
    if(!digits || !fb_geom_finite(value)) fb_map_error="Invalid number";
    fb_map_next();return value;
}
int fb_map_integer()
{
    int value=stoi(fb_map_token);fb_map_number();return value;
}
vector fb_map_vector()
{
    vector value;
    value_x=fb_map_number();value_y=fb_map_number();value_z=fb_map_number();return value;
}
string fb_map_quote(string text)
{
    text=strreplace("\\","\\\\",text);text=strreplace("\"","\\\"",text);
    return strcat("\"",strreplace("\n","\\n",text),"\"");
}
int fb_map_add(int kind,int parent)
{
    fb_map_item_t *items=memalloc(sizeof(fb_map_item_t)*(fb_map_count+1));
    memcpy(items,fb_map_items,sizeof(fb_map_item_t)*fb_map_count);memfree(fb_map_items);fb_map_items=items;
    int index=fb_map_count++;items[index].kind=kind;items[index].parent=parent;return index;
}
int fb_map_face(brushface_t *face,int *contents)
{
    fb_map_take("(");vector a=fb_map_vector();fb_map_take(")");
    fb_map_take("(");vector b=fb_map_vector();fb_map_take(")");
    fb_map_take("(");vector c=fb_map_vector();fb_map_take(")");
    vector normal=crossproduct(c-a,b-a);
    if(vlen(normal)<0.000001) {fb_map_error="Degenerate face";return FALSE;}
    face->planenormal=normalize(normal);face->planedist=a*face->planenormal;
    face->shadername=strzone(fb_map_token);fb_map_next();
    if(fb_map_token=="[") {
        fb_map_next();face->sdir=fb_map_vector();face->sbias=fb_map_number();fb_map_take("]");
        fb_map_take("[");face->tdir=fb_map_vector();face->tbias=fb_map_number();fb_map_take("]");
        fb_map_number();float scaleX=fb_map_number(),scaleY=fb_map_number();
        if(fabs(scaleX)<0.000001 || fabs(scaleY)<0.000001) fb_map_error="Zero texture scale";
        else {face->sdir=face->sdir/scaleX;face->tdir=face->tdir/scaleY;}
    } else {
        fb_texture_basecoords(face);
        face->sbias=fb_map_number();face->tbias=fb_map_number();
        float angle=fb_map_number()*M_PI/180;
        float sx=fb_map_number(),sy=fb_map_number();
        if(fabs(sx)<0.000001 || fabs(sy)<0.000001) fb_map_error="Zero texture scale";
        else {
            vector u=face->sdir,v=face->tdir;
            face->sdir=(u*cos(angle)-v*sin(angle))/sx;face->tdir=(u*sin(angle)+v*cos(angle))/sy;
        }
    }
    face->surfaceflags=face->surfacevalue=0;
    if(fb_map_token!="" && fb_map_token!="(" && fb_map_token!="}") {
        *contents=fb_map_integer();face->surfaceflags=fb_map_integer();face->surfacevalue=fb_map_integer();
    }
    return fb_map_error=="";
}
void fb_map_geometry(int parent,int opened)
{
    if(!opened) fb_map_take("{");
    fb_map_item_t *item;
    int index=fb_map_add(FB_NODE_BRUSH,parent);
    if(fb_map_token=="patchDef2" || fb_map_token=="patchDef3" || fb_map_token=="patchDef2WS" || fb_map_token=="patchDef3WS") {
        int explicitSubdiv=fb_map_token=="patchDef3" || fb_map_token=="patchDef3WS";
        int colored=fb_map_token=="patchDef2WS" || fb_map_token=="patchDef3WS";
        fb_map_items[index].kind=FB_NODE_PATCH;fb_map_next();fb_map_take("{");
        item=&fb_map_items[index];
        item->patch.shadername=strzone(fb_map_token);fb_map_next();fb_map_take("(");
        item->patch.cpwidth=fb_map_integer();item->patch.cpheight=fb_map_integer();
        item->patch.tesswidth=item->patch.tessheight=-1;
        if(explicitSubdiv) {item->patch.tesswidth=fb_map_integer();item->patch.tessheight=fb_map_integer();}
        item->patch.texinfo[2]=fb_map_number();item->patch.texinfo[0]=fb_map_number();item->patch.texinfo[1]=fb_map_number();fb_map_take(")");
        int w=item->patch.cpwidth,h=item->patch.cpheight;
        if(w<3 || h<3 || w>129 || h>129 || !(w&1) || !(h&1)) {fb_map_error="Invalid patch dimensions";return;}
        item->count=w*h;item->points=memalloc(sizeof(patchvert_t)*item->count);
        fb_map_take("(");
        for(int y=0;y<h && fb_map_error=="";y++) {
            fb_map_take("(");
            for(int x=0;x<w && fb_map_error=="";x++) {
                patchvert_t *cp=&item->points[y*w+x];
                fb_map_take("(");cp->xyz=fb_map_vector();
                cp->s=fb_map_number();cp->t=fb_map_number();
                cp->rgb='1 1 1';cp->a=1;
                if(colored) {cp->rgb=fb_map_vector();cp->a=fb_map_number();}
                fb_map_take(")");
                if(!fb_geom_point(cp->xyz)) fb_map_error="Patch outside world bounds";
            }
            fb_map_take(")");
        }
        fb_map_take(")");fb_map_take("}");fb_map_take("}");
    } else {
        fb_map_items[index].faces=memalloc(sizeof(brushface_t)*64);
        fb_map_items[index].contents=CONTENTBIT_SOLID;
        while(fb_map_token=="(" && fb_map_error=="") {
            item=&fb_map_items[index];
            if(item->count==64) {fb_map_error="Clipboard brush exceeds 64 faces";break;}
            fb_map_face(&item->faces[item->count++],&item->contents);
        }
        fb_map_take("}");
        item=&fb_map_items[index];
        if(fb_map_error=="" && !fb_brush_valid(item->faces,item->count)) fb_map_error=fb_geom_error;
    }
}
int fb_map_parse(string text)
{
    fb_map_clear();fb_map_error="";fb_map_cursor=0;fb_map_length=strlen(text);
    if(fb_map_length>16777216) {fb_map_error="Clipboard MAP exceeds 16 MiB";return FALSE;}
    fb_map_text=strzone(text);fb_map_next();
    if(fb_map_token=="(") {
        int face=fb_map_add(-1,-1);fb_map_items[face].faces=memalloc(sizeof(brushface_t));
        fb_map_items[face].count=1;fb_map_items[face].contents=CONTENTBIT_SOLID;
        fb_map_face(fb_map_items[face].faces,&fb_map_items[face].contents);
    } else while(fb_map_token!="" && fb_map_error=="") {
        if(fb_map_count>65536) {fb_map_error="Too many clipboard objects";break;}
        fb_map_take("{");
        if(fb_map_token=="(" || fb_map_token=="patchDef2" || fb_map_token=="patchDef3" || fb_map_token=="patchDef2WS" || fb_map_token=="patchDef3WS") {fb_map_geometry(-1,TRUE);continue;}
        int entityIndex=fb_map_add(FB_NODE_ENTITY,-1);
        while(fb_map_token!="" && fb_map_token!="}" && fb_map_error=="") {
            if(fb_map_token=="{") fb_map_geometry(entityIndex,FALSE);
            else {
                string key=strzone(fb_map_token);fb_map_next();
                if(fb_map_token=="}" || fb_map_token=="{" || fb_map_token=="") {strunzone(key);fb_map_error="Missing entity value";break;}
                string value=fb_map_quote(fb_map_token);
                string fields=strzone(strcat(fb_map_items[entityIndex].fields,fb_map_quote(key)," ",value,"\n"));
                strunzone(fb_map_items[entityIndex].fields);fb_map_items[entityIndex].fields=fields;
                strunzone(key);fb_map_next();
            }
        }
        fb_map_take("}");
    }
    if(fb_map_token!="") fb_map_error="Unexpected trailing MAP data";
    return fb_map_error=="" && fb_map_count>0;
}
