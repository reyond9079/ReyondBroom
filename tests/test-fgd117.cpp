// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_entity_defs.h"
#include <fstream>
#include <iostream>
using namespace ftebroom;
void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);std::cout<<"PASS "<<name<<'\n';}
int main(int argc,char** argv){try{
 for(const char* malformed:{"{{","{{ spawnflags ->"}){bool rejected=false;try{FgdModels(malformed).read();}catch(...){rejected=true;}check(rejected,"incomplete_model_case_rejected");}
 auto defs=entityRead(R"FGD(
 @baseclass = Settings [spawnflags(Flags)=[1:"First":0:"Help text" 2:"Second":0:"More help"] skin(integer):"Skin":0]
 @PointClass base(Settings) model({{
  (spawnflags & 2) == 2 && skin == "1" -> {"path":"progs/alternate.mdl", "skin":skin,"frame":12},
  spawnflags & 1 -> "progs/first.mdl",
  {"path":"progs/default.mdl","skin":0,"frame":0}//adjacent comment
 }}) = sample []
 )FGD",".fgd");
 auto d=defs.at("sample");check(d.model=="progs/default.mdl"&&d.modelFrame=="0","conditional_default_model");check(d.properties.at("spawnflags").type=="flags"&&d.properties.at("spawnflags").choices.size()==2,"flags_help_and_case");
 std::map<std::string,std::string> p{{"spawnflags","2"},{"skin","1"}};check(fgdCondition(d.modelRules[0].condition,p)&&fgdValue(d.modelRules[0].path,p)=="progs/alternate.mdl","bitwise_and_logical_condition");
 p["skin"]="0";check(!fgdCondition(d.modelRules[0].condition,p),"condition_rejects_wrong_skin");p["spawnflags"]="1";check(fgdCondition(d.modelRules[1].condition,p),"string_model_variant");
 if(argc>1){std::ifstream in(argv[1],std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});auto actual=entityRead(bytes,".fgd");check(actual.size()==346,"actual_ad_1_9beta5_1_all_classes");check(actual.at("monster_zombie").model=="progs/mon_zombie.mdl"&&actual.at("monster_zombie").modelFrame=="0","ad_zombie_default_pose");check(actual.at("monster_knight").model=="progs/mon_knight.mdl"&&actual.at("monster_army").model=="progs/mon_soldier.mdl","ad_monster_model_paths");
  for(auto& item:actual)for(auto& rule:item.second.modelRules){std::map<std::string,std::string> properties;for(auto& v:item.second.properties)properties[v.first]=v.second.value;fgdCondition(rule.condition,properties);}check(true,"all_ad_conditions_safe_to_evaluate");}
 }catch(std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
