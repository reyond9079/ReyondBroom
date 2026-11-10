// SPDX-License-Identifier: GPL-2.0-or-later
// Native definition parsing publishes a compact cache consumed by the scene renderer.
typedef struct {string classname,model,modelkey,modelskin,modelframe,modelscale;vector mins,maxs,color;int firstRule,ruleCount;} fb_entity_definition_t;
typedef struct {string condition,path,skin,frame,scale;} fb_definition_rule_t;
fb_definition_rule_t *fb_definition_rules;int fb_definition_rule_count,fb_definition_rule_capacity;
hashtable fb_entity_defaults;
hashtable fb_entity_kinds;
hashtable fb_entity_definition_index;
fb_entity_definition_t *fb_entity_definitions;int fb_entity_definition_count;
void fb_entity_definitions_load()
{
    filestream stream=fopen("ftebroom-entity-definitions.txt",FILE_READ);if((float)stream<0) return;
    for(int r=0;r<fb_definition_rule_count;r++){strunzone(fb_definition_rules[r].condition);strunzone(fb_definition_rules[r].path);strunzone(fb_definition_rules[r].skin);strunzone(fb_definition_rules[r].frame);strunzone(fb_definition_rules[r].scale);}memfree(fb_definition_rules);fb_definition_rules=__NULL__;fb_definition_rule_count=fb_definition_rule_capacity=0;
    for(int i=0;i<fb_entity_definition_count;i++) {strunzone(fb_entity_definitions[i].classname);strunzone(fb_entity_definitions[i].model);strunzone(fb_entity_definitions[i].modelkey);strunzone(fb_entity_definitions[i].modelskin);strunzone(fb_entity_definitions[i].modelframe);strunzone(fb_entity_definitions[i].modelscale);}
    memfree(fb_entity_definitions);fb_entity_definitions=__NULL__;fb_entity_definition_count=0;
    if(fb_entity_defaults)hash_destroytab(fb_entity_defaults);fb_entity_defaults=hash_createtab(256,EV_STRING);
    if(fb_entity_kinds)hash_destroytab(fb_entity_kinds);fb_entity_kinds=hash_createtab(256,EV_INTEGER);
    if(fb_entity_definition_index)hash_destroytab(fb_entity_definition_index);fb_entity_definition_index=hash_createtab(256,EV_INTEGER);
    string line;while((line=fgets(stream))) {
        int count=fb_entity_tokenize(line);
        if(count==3 && argv(0)=="kind"){fb_entity_kinds[argv(1)]=stoi(argv(2))+1i;continue;}
        if(count==4 && argv(0)=="property"){
            // Definitions without a default still appear in the native inspector.
            // Do not instantiate their empty values as MAP keys.
            if(argv(3)!="")fb_entity_defaults[strcat(argv(1),"/",argv(2))]=argv(3);
            continue;
        }
        if(count==7 && argv(0)=="modelrule"&&fb_entity_definition_count>0){
            fb_entity_definition_t *owner=&fb_entity_definitions[fb_entity_definition_count-1];if(owner->classname!=argv(1))continue;
            if(fb_definition_rule_count==fb_definition_rule_capacity){fb_definition_rule_capacity=max(32,fb_definition_rule_capacity*2);fb_definition_rule_t *grown=memalloc(fb_definition_rule_capacity*sizeof(fb_definition_rule_t));memcpy(grown,fb_definition_rules,fb_definition_rule_count*sizeof(fb_definition_rule_t));memfree(fb_definition_rules);fb_definition_rules=grown;}
            if(!owner->ruleCount)owner->firstRule=fb_definition_rule_count;owner->ruleCount++;fb_definition_rule_t *rule=&fb_definition_rules[fb_definition_rule_count++];rule->condition=strzone(argv(2));rule->path=strzone(argv(3));rule->skin=strzone(argv(4));rule->frame=strzone(argv(5));rule->scale=strzone(argv(6));continue;
        }
        if(count!=6 && count!=9)continue;
        fb_entity_definition_t *items=memalloc((fb_entity_definition_count+1)*sizeof(fb_entity_definition_t));
        memcpy(items,fb_entity_definitions,fb_entity_definition_count*sizeof(fb_entity_definition_t));memfree(fb_entity_definitions);fb_entity_definitions=items;
        fb_entity_definition_t *def=&items[fb_entity_definition_count++];def->classname=strzone(argv(0));def->mins=stov(argv(1));def->maxs=stov(argv(2));def->color=stov(argv(3));def->model=strzone(argv(4));def->modelkey=strzone(argv(5));def->modelskin=strzone(argv(6));def->modelframe=strzone(argv(7));def->modelscale=strzone(argv(8));
    }
    for(int i=0;i<fb_entity_definition_count;i++)if(!(int)fb_entity_definition_index[fb_entity_definitions[i].classname])fb_entity_definition_index[fb_entity_definitions[i].classname]=i+1i;
    fclose(stream);
}

string fb_fgd_value(string expression,hashtable fields,string classname){
    string value=substring(expression,1,-1);if(substring(expression,0,1)=="=")return value;
    string result=fields[value];if(result=="")result=fb_entity_defaults[strcat(classname,"/",value)];return result;
}
int fb_fgd_condition(string code,hashtable fields,string classname){
    if(code=="")return TRUE;static float stack[128];int size=0,count=tokenizebyseparator(code," ");
    for(int t=0;t<count;t++){
        string op=argv(t);if(op=="!"){if(!size)return FALSE;stack[size-1]=!stack[size-1];continue;}
        if(op=="&"||op=="|"||op=="^"||op=="&&"||op=="||"||op=="=="||op=="!="||op=="<"||op==">"||op=="<="||op==">="||op=="-"){
            if(size<2)return FALSE;float b=stack[--size],a=stack[size-1];
            if(op=="&")a=(int)a&(int)b;else if(op=="|")a=(int)a|(int)b;else if(op=="^")a=(int)a^(int)b;else if(op=="&&")a=a&&b;else if(op=="||")a=a||b;else if(op=="==")a=a==b;else if(op=="!=")a=a!=b;else if(op=="<")a=a<b;else if(op==">")a=a>b;else if(op=="<=")a=a<=b;else if(op==">=")a=a>=b;else a-=b;stack[size-1]=a;
        }else{if(size==128)return FALSE;stack[size++]=stof(substring(op,0,1)=="$"?fb_fgd_value(op,fields,classname):op);}
    }return size==1&&stack[0]!=0;
}

// Comma-separated scale/property alternatives use the last non-empty value.
// Literal numbers, vectors and entity-property references share the same path.
string fb_definition_parameter(string expression,hashtable properties,string fallback)
{
    string result=fallback;int start=0;
    for(int i=0;i<=strlen(expression);i++)if(i==strlen(expression) || substring(expression,i,1)==",") {
        string part=substring(expression,start,i-start);start=i+1;
        while(substring(part,0,1)==" ")part=substring(part,1,-1);
        while(substring(part,strlen(part)-1,1)==" ")part=substring(part,0,strlen(part)-1);
        if(part=="")continue;
        string value=properties[part];
        if(value!="")result=value;
        else {int first=str2chr(part,0);if(first>=48 && first<=57 || first==45 || first==46)result=part;}
    }
    return result;
}
