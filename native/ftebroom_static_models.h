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
// Authoring entities stay func_model in the document. Only disposable build
// copies use q3map2's misc_model or the explicitly requested FTE Q1 fallback.
inline std::string prepareStaticModels(const std::string& bytes,int profile,size_t& changed){
 changed=0;auto entities=rawPreviewEntities(bytes);
 for(auto& e:entities){
  if(!e.geometry.empty()||e.get("classname")!="func_model")continue;
  bool marked=e.get("_reyondbroom_authoring")=="1"&&generatedAuthoringPath(e.get("model"));
  bool legacy=generatedIvyPath(e.get("model"));
  if(!marked&&!legacy)continue;
  if(marked&&(profile==3||profile==4))throw std::runtime_error("This map contains ReyondBroom authoring func_model entities. Quake II / GoldSrc runtime export is unsupported; convert the source model and use a compatible mod entity before compiling.");
  if(marked&&profile==2&&e.get("_reyondbroom_fte_q1")!="1")throw std::runtime_error("Authoring func_model requires the explicit FTE static-model extension for Quake 1. Enable it in Plant Editor and Save, or convert the model for a compatible mod; vanilla Quake cannot spawn this entity.");
  if(marked&&profile==2){
   double scale=1;auto value=e.get("scale");if(value.empty())value=e.get("modelscale");
   if(!value.empty()){std::istringstream in(value);in.imbue(std::locale::classic());if(!(in>>scale)||!std::isfinite(scale))throw std::runtime_error("Invalid authoring model scale in Quake 1 build copy.");}
   auto axes=e.get("modelscale_vec");if(!axes.empty()){
    std::istringstream in(axes);in.imbue(std::locale::classic());std::array<double,3> v{};for(auto& x:v)if(!(in>>x)||!std::isfinite(x))throw std::runtime_error("Invalid authoring model vector scale in Quake 1 build copy.");
    if(v[0]!=v[1]||v[0]!=v[2])throw std::runtime_error("Quake 1/FTE static-model fallback supports uniform scale only. Apply the non-uniform scale to the source geometry or use a compatible custom mod before compiling.");
    scale*=v[0];
   }
   std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(12)<<scale;e.fields["scale"]=out.str();
  }
  if((profile==0&&marked)||(profile==2&&(marked||legacy))){
   e.fields["classname"]="misc_model";changed++;
   if(profile==0){
    double scale=1;auto value=e.get("scale");if(value.empty())value=e.get("modelscale");
    if(!value.empty()){std::istringstream in(value);in.imbue(std::locale::classic());if(!(in>>scale)||!std::isfinite(scale))throw std::runtime_error("Invalid authoring model scale in build copy.");}
    auto axes=e.get("modelscale_vec");
    if(!axes.empty()){
     std::istringstream in(axes);in.imbue(std::locale::classic());std::array<double,3> vector{};for(auto& n:vector)if(!(in>>n)||!std::isfinite(n))throw std::runtime_error("Invalid authoring model vector scale in build copy.");
     std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(12)<<vector[0]*scale<<' '<<vector[1]*scale<<' '<<vector[2]*scale;e.fields["modelscale_vec"]=out.str();e.fields["modelscale"]="1";
    }else{std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(12)<<scale;e.fields["modelscale"]=out.str();}
   }
  }
 }
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
