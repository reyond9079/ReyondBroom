// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the actual plugin creation path through the public host boundary.
#include "../plugins/ivy/ivy.cpp"
#include "../native/ftebroom_model_md2_export.h"
#include "../native/ftebroom_legacy_models.h"
#include "../native/ftebroom_quake_palette.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
static std::map<std::string,std::string> assets;
static int profile,models;static std::string imported;static std::filesystem::path target;
static double number(const char* key){return !std::strcmp(key,"rb.profile")?profile:42;}
static int stringValue(const char*,char* out,size_t n){return std::snprintf(out,n,"%s",profile==0?"misc_model":"func_model");}
static int writeAsset(const char* name,const char* bytes,size_t n){assets[name]=std::string(bytes,n);return 1;}
static void save(const std::string& name,const std::string& bytes){auto file=target/std::filesystem::path(name).filename();std::ofstream out(file,std::ios::binary);out.write(bytes.data(),bytes.size());}
static int writeModel(const char* name,const char* bytes,size_t n){
 using namespace ftebroom;auto mesh=modelExport::parse(std::string(bytes,n));std::vector<modelExport::Skin> skins;
 for(auto& s:mesh.surfaces){auto image=assets.at(s.material);unsigned w=quake1::word(image,12)&65535,h=((unsigned char)image[14])|((unsigned char)image[15]<<8);modelExport::Skin skin;skin.width=w;skin.height=h;
  for(size_t p=18;p<image.size();p+=4)for(int a:{2,1,0,3})skin.rgba.push_back((unsigned char)image[p+a]);skins.push_back(std::move(skin));}
 std::vector<std::array<MdlVertex,3>> decoded;std::string output;
 if(profile==2){if(std::filesystem::path(name).extension()!=".mdl")throw std::runtime_error("wrong Quake extension");output=modelExport::mdl(mesh,skins,defaultQuakePalette());auto m=readMdl(output);decoded=m.triangles;if(m.skin.width<256||m.skin.pixels.empty())throw std::runtime_error("missing embedded skin");}
 else if(profile==3){if(std::filesystem::path(name).extension()!=".md2")throw std::runtime_error("wrong Quake II extension");auto m=modelExport::md2(mesh,skins,defaultQuakePalette(),"models/ivy.pcx");output=m.model;decoded=legacyModels::md2(output)[0].triangles;save(std::string(name)+".pcx",m.skin);}
 else {if(std::filesystem::path(name).extension()!=".md3")throw std::runtime_error("wrong Quake III extension");output=modelExport::md3(mesh);for(auto& s:readMd3(output))decoded.insert(decoded.end(),s.triangles.begin(),s.triangles.end());}
 size_t triangle=0;for(auto& s:mesh.surfaces)for(auto t:s.triangles){if(triangle>=decoded.size())throw std::runtime_error("missing triangles");for(int v=0;v<3;v++)for(int a=0;a<3;a++)if(std::abs(decoded[triangle][v].point[a]-s.frames[0][t[v]].position[a])>1)throw std::runtime_error("model position/scale changed");triangle++;}
 if(triangle!=decoded.size())throw std::runtime_error("extra triangles");save(name,output);save(std::string(name)+".mesh",std::string(bytes,n));models++;return 1;
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("Pass an isolated output directory");target=argv[1];std::filesystem::create_directories(target);
 RBHostV1 api{};api.api_version=1;api.struct_size=sizeof api;api.number=number;api.string=stringValue;api.write_asset=writeAsset;api.write_model=writeModel;api.import_map=[](const char* p,size_t n){imported.assign(p,n);return 1;};api.preview_lines=[](const float*,size_t){};host=&api;
 growth.nodes.clear();for(int i=0;i<270;i++)growth.nodes.push_back({{60+std::sin(i*.12)*12,30,64+i*3.},{0,-1,0},i-1,i});
 sourceGeneration=42;settings.iterations=600;state=0;output=1;
 for(int game:{2,3,0})for(int custom:{0,1}){profile=game;
  assets.clear();models=0;imported.clear();branchMaterial[0]=leafMaterial[0]=0;
  if(custom){std::snprintf(branchMaterial,sizeof branchMaterial,"textures/custom-bark");std::snprintf(leafMaterial,sizeof leafMaterial,"textures/custom-leaf");assets[branchMaterial]=ivy::texture(false);assets[leafMaterial]=ivy::texture(true);}
  create();if(models<2||imported.find("\"model\"")==std::string::npos||message.rfind("Ivy import queued:",0)!=0)throw std::runtime_error("plugin creation failed: "+message);
  if(!custom)for(auto& asset:assets)save(std::filesystem::path(asset.first).filename().string(),asset.second);
  std::cout<<"PASS ivy profile "<<profile<<" "<<(custom?"selected materials":"generated materials")<<": "<<models<<" native parts, decoded coordinates/textures/limits preserved\n";
 }
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
