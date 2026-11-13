// SPDX-License-Identifier: GPL-2.0-or-later
#include "ftebroom_static_models.h"
static void stageGeneratedModels(const std::string& map,const std::filesystem::path& destination,const std::filesystem::path& snapshot={}){
 if(jamEditing())return;
 std::set<std::string> copied,shaders;
 auto safe=[](const std::string& name){return ftebroom::pak::zipName(name)&&name.find_first_of("\\:\r\n")==name.npos&&(name.rfind("models/",0)==0||name.rfind("textures/",0)==0||name.rfind("progs/",0)==0||name=="glsl/reyond_plant_wind_v1.glsl"||((name.rfind("scripts/plant-",0)==0||name.rfind("scripts/model-",0)==0)&&std::filesystem::u8path(name).extension()==".shader"));};
 auto read=[&](const std::string& name){
  if(!safe(name))throw std::runtime_error("Invalid generated model dependency path.");
  for(auto folder:{snapshot,documentPath("ftebroom.cfg").parent_path()})if(!folder.empty()){
   auto file=folder/std::filesystem::u8path(name);if(std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=64*1024*1024)return q1Read(file);
  }
  return modelResource(name);
 };
 auto copy=[&](const std::string& name,const std::string& bytes){
  if(bytes.empty()||bytes.size()>64*1024*1024)throw std::runtime_error("Generated model asset is missing or too large: "+name);
  if(!copied.insert(name).second)return;
  if((name.rfind("scripts/plant-",0)==0||name.rfind("scripts/model-",0)==0)&&std::filesystem::u8path(name).extension()==".shader")shaders.insert(std::filesystem::u8path(name).stem().u8string());
  auto target=destination/std::filesystem::u8path(name);if(std::filesystem::exists(target)&&q1Read(target)==bytes)return;
  buildWriteTexture(target,bytes);
 };
 for(const auto& path:ftebroom::generatedIvyModels(map)){
  auto bytes=read(path);copy(path,bytes);
  if(path.rfind("models/reyond-plugins/plant-",0)==0||path.rfind("models/reyond-plugins/model-",0)==0){
   auto metadata=path+".reyond.json";auto text=read(metadata);auto json=nlohmann::json::parse(text);
   if(json.value("kind","")!="reyondbroom-authoring-model"||!json.contains("dependencies")||!json["dependencies"].is_array()||json["dependencies"].size()>512)throw std::runtime_error("Invalid generated plant manifest.");
   copy(metadata,text);for(auto& entry:json["dependencies"]){auto dependency=entry.get<std::string>();if(!safe(dependency))throw std::runtime_error("Invalid generated plant dependency.");copy(dependency,read(dependency));}
  }
  for(auto dependency:ftebroom::generatedModelDependencies(bytes)){
   if(!safe(dependency))throw std::runtime_error("Invalid generated model material path.");
   bool found=false;for(auto suffix:{"",".tga",".png",".jpg",".jpeg",".pcx"}){auto name=dependency+suffix;auto image=read(name);if(!image.empty()){copy(name,image);found=true;break;}}
   if(!found&&dependency.rfind("textures/reyond-plugins/",0)==0)throw std::runtime_error("Generated ivy texture is missing: "+dependency);
  }
 }
 if(!shaders.empty()){
  auto target=destination/"scripts/shaderlist.txt";std::string list;
  if(std::filesystem::is_regular_file(target))list=q1Read(target);
  else {size_t size=0;auto bytes=static_cast<char*>(files->LoadFile("scripts/shaderlist.txt",&size));if(bytes){list.assign(bytes,size);core->Free(bytes);}}
  buildWriteTexture(target,ftebroom::appendShaderList(list,shaders));
 }
}
