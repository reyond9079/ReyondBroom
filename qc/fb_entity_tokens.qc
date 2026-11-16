// SPDX-License-Identifier: GPL-2.0-or-later
// Entity/command writers use escaped quotes and backslashes; FTE's default
// tokenizer uses doubled quotes instead. Normalize explicitly without changing
// unknown backslash sequences in legacy MAP paths.
int fb_entity_message_key(string key)
{
    if(substring(key,0,7)!="message")return FALSE;
    for(int i=7;i<strlen(key);i++)
        if(str2chr(key,i)<48 || str2chr(key,i)>57)return FALSE;
    return TRUE;
}
string fb_entity_message_text(string value)
{
    if(strstrofs(value,"\\",0)<0)return value;
    string text="";
    for(int i=0;i<strlen(value);i++) {
        string ch=substring(value,i,1),next=substring(value,i+1,1);
        if(ch=="\\" && (next=="n" || next=="\\")) {
            if(next=="n")ch="\n";
            i++;
        }
        text=strcat(text,ch);
    }
    return text;
}
int fb_tokenize_escaped(string source,int entityFields)
{
    // Ordinary quoted fields need no escape normalization or temporary copies.
    if(strstrofs(source,"\\",0)<0)return tokenize_console(source);
    string normalized="";int cursor=0,length=strlen(source),field=0,message=FALSE;
    while(cursor<length) {
        while(cursor<length && str2chr(source,cursor)<=32)cursor++;
        if(cursor>=length)break;
        string quote=substring(source,cursor,1),value="";
        if(quote=="\"" || quote=="'") {
            cursor++;
            while(cursor<length) {
                string ch=substring(source,cursor++,1);
                if(ch==quote)break;
                if(ch=="\\" && cursor<length) {
                    string next=substring(source,cursor,1);
                    if(next=="\\" || next==quote){ch=next;cursor++;}
                    else if(message && next=="n"){ch="\n";cursor++;}
                }
                value=strcat(value,ch);
            }
        }else {
            int start=cursor;while(cursor<length && str2chr(source,cursor)>32)cursor++;
            value=substring(source,start,cursor-start);
        }
        if(entityFields && (field%2)==0)message=fb_entity_message_key(value);
        else message=FALSE;
        field++;
        value=strreplace("\\","\\\\",value);value=strreplace("\"","\\\"",value);value=strreplace("\n","\\n",value);
        normalized=strcat(normalized,"\\\"",value,"\" ");
    }
    return tokenize_console(normalized);
}
int fb_entity_tokenize(string source) {return fb_tokenize_escaped(source,TRUE);}
