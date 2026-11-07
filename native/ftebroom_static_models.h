// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_raw_preview.h"
#include "ftebroom_quake1.h"
namespace ftebroom {
inline std::string appendShaderList(std::string list,const std::set<std::string>& names){
 std::set<std::string> present;std::istringstream in(list);std::string name;
 while(in>>name){if(name.rfind("//",0)==0){std::getline(in,name);continue;}present.insert(name);}
 for(auto& value:names)if(!present.count(value))list+="\n"+value+"\n";
 return list;
}
inline bool generatedIvyPath(const std::string& name){
 if(name.rfind("models/reyond-plugins/ivy-",0)!=0||name.find_first_of("\\:\r\n")!=name.npos)return false;
 for(auto& part:std::filesystem::u8path(name))if(part==".."||part==".")return false;
 return true;
}
inline bool generatedAuthoringPath(const std::string& name){
 if(name.empty()||name.find_first_of("\\:\r\n")!=name.npos||std::filesystem::u8path(name).is_absolute())return false;
 for(auto& part:std::filesystem::u8path(name))if(part==".."||part==".")return false;
 return true;
}
// FTE supplies a native misc_model fallback for classic Quake progs. The
// ordinary LibreQuake progs has no func_model spawn function. Migrate only the
// known static ivy class in disposable build/preview copies, never the MAP.
inline std::string prepareStaticModels(const std::string& bytes,int profile,size_t& changed){
 changed=0;if(profile!=2)return bytes;auto entities=rawPreviewEntities(bytes);
 for(auto& e:entities)if(e.geometry.empty()&&e.get("classname")=="func_model"&&(generatedIvyPath(e.get("model"))||(e.get("_reyondbroom_authoring")=="1"&&generatedAuthoringPath(e.get("model"))))){e.fields["classname"]="misc_model";changed++;}
 if(!changed)return bytes;std::string result;
 for(auto& e:entities){result+="{\n";for(auto& f:e.fields)result+=prefab::quote(f.first)+" "+prefab::quote(f.second)+"\n";result+=e.geometry+"}\n";}
 return result;
}
inline std::set<std::string> generatedIvyModels(const std::string& bytes){
 std::set<std::string> result;for(auto& e:rawPreviewEntities(bytes))if(generatedIvyPath(e.get("model"))||(e.get("_reyondbroom_authoring")=="1"&&generatedAuthoringPath(e.get("model"))))result.insert(e.get("model"));return result;
}
inline std::vector<std::string> generatedModelDependencies(const std::string& bytes){
 using namespace quake1;std::vector<std::string> result;
 if(bytes.substr(0,4)=="IDP2"){
  range(bytes,0,68);auto count=word(bytes,20),offset=word(bytes,44);if(count>256)throw std::runtime_error("Invalid generated MD2 skin count.");range(bytes,offset,size_t(count)*64);
  for(unsigned i=0;i<count;i++)result.push_back(name(bytes,offset+i*64,64));
 }else if(bytes.substr(0,4)=="IDP3"){
  range(bytes,0,108);auto count=word(bytes,84);size_t at=word(bytes,100);if(count>256)throw std::runtime_error("Invalid generated MD3 surface count.");
  for(unsigned i=0;i<count;i++){
   range(bytes,at,108);auto shaders=word(bytes,at+76),offset=word(bytes,at+92),size=word(bytes,at+104);if(shaders>256||size<108||offset>size||size_t(shaders)*68>size-offset)throw std::runtime_error("Invalid generated MD3 materials.");range(bytes,at,size);
   for(unsigned j=0;j<shaders;j++)result.push_back(name(bytes,at+offset+j*68,64));at+=size;
  }
 }
 return result;
}
}
