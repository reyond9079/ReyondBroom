// SPDX-License-Identifier: GPL-2.0-or-later
#include "ftebroom_md3.h"
#include "ftebroom_legacy_models.h"
#include "ftebroom_sprite.h"
#include "ftebroom_bsp_uv.h"
#include "ftebroom_legacy_images.h"
#include "ftebroom_preview_raster.h"
#include "ftebroom_stb_image.h"
#include <filesystem>
#include <future>
struct EntityPreviewPart {qhandle_t image=0;std::vector<std::array<ftebroom::MdlVertex,3>> triangles;ftebroom::preview::Pixels pixels;};
struct EntityPreview {qhandle_t image=0;int spriteWidth=0,spriteHeight=0;std::vector<EntityPreviewPart> parts;std::vector<std::pair<size_t,size_t>> order;qhandle_t thumbnail=0;unsigned long long used=0;};
static std::map<std::string,EntityPreview> entityPreviews;
static unsigned entityPreviewGeneration=0;
static unsigned long long entityPreviewClock=0,entityPreviewResourceReads=0,entityPreviewBuilds=0,entityPreviewQuadCount=0;
static int entityPreviewQuadFrame=-1;
static int entityPreviewBuildFrame=-1;
static std::future<std::string> entityPreviewJob;
static std::string entityPreviewJobKey;
static unsigned entityPreviewJobGeneration=0;
static void entityPreviewRelease(EntityPreview& preview){if(preview.thumbnail)draw->UnloadImage(preview.thumbnail);for(auto& part:preview.parts)if(part.image)draw->UnloadImage(part.image);}
static void clearEntityPreviews(){++entityPreviewGeneration;for(auto& p:entityPreviews)entityPreviewRelease(p.second);entityPreviews.clear();}
static void entityPreviewPump(){
 if(!entityPreviewJob.valid()||entityPreviewJob.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
 auto bytes=entityPreviewJob.get();if(entityPreviewJobGeneration!=entityPreviewGeneration)return;
 auto found=entityPreviews.find(entityPreviewJobKey);if(found==entityPreviews.end()||bytes.empty())return;
 auto name="ftebroom/model-thumbnail-"+std::to_string(entityPreviewGeneration)+"-"+std::to_string(std::hash<std::string>{}(entityPreviewJobKey))+".tga";
 draw->LoadImageData(name.c_str(),"image/tga",bytes.data(),bytes.size());found->second.thumbnail=draw->LoadImage(name.c_str());
}
static ftebroom::preview::Pixels entityPreviewPixels(const std::string& bytes){
 ftebroom::preview::Pixels result;if(bytes.empty()||bytes.size()>32*1024*1024)return result;int channels=0;
 if(!stbi_info_from_memory((const stbi_uc*)bytes.data(),bytes.size(),&result.width,&result.height,&channels)||result.width<=0||result.height<=0||size_t(result.width)*result.height>16777216)return {};
 auto pixels=stbi_load_from_memory((const stbi_uc*)bytes.data(),bytes.size(),&result.width,&result.height,&channels,4);if(!pixels)return {};
 // Source images are only needed while rasterizing a 192px thumbnail.
 int width=std::min(256,result.width),height=std::min(256,result.height);result.rgba.resize(size_t(width)*height*4);
 for(int y=0;y<height;y++)for(int x=0;x<width;x++)for(int c=0;c<4;c++)result.rgba[(size_t(y)*width+x)*4+c]=pixels[(size_t(y*result.height/height)*result.width+x*result.width/width)*4+c];
 stbi_image_free(pixels);result.width=width;result.height=height;return result;
}
static std::string modelResource(const std::string& path){++entityPreviewResourceReads;if(path=="gfx/palette.lmp")return q1Palette();size_t length=0;auto raw=(char*)files->LoadFile(path.c_str(),&length);if(!raw)return {};std::string result(raw,length);core->Free(raw);return result;}
static qhandle_t modelImage(std::string path,ftebroom::preview::Pixels* pixels=nullptr){
 auto found=materials.find(path);if(found!=materials.end()&&!found->second.preview.empty())path=found->second.preview;
 for(auto suffix:{"",".tga",".png",".jpg",".jpeg",".pcx"}){auto bytes=modelResource(path+suffix);if(bytes.empty())continue;
  if(pixels){try{if(bytes.size()>128&&(unsigned char)bytes[0]==10)bytes=ftebroom::quake1::tga(ftebroom::legacy::pcx(bytes),{});*pixels=entityPreviewPixels(bytes);}catch(...){*pixels={};}}
  return draw->LoadImage((path+suffix).c_str());}
 return 0;
}
static bool entityModelTile(const ftebroom::EntityDefinition& def,ImVec2 tile,float size){
 entityPreviewPump();
 if(def.brush||def.model.empty()||def.model.find_first_of("{}()\"'")!=std::string::npos)return false;
 if(!ImGui::IsRectVisible(tile,{tile.x+size,tile.y+size}))return true;
 auto key=def.model+":"+def.modelSkin+(def.modelFrame.empty()?"":":"+def.modelFrame);auto it=entityPreviews.find(key);
 if(it==entityPreviews.end()){
  // Loading engine resources and GPU uploads stay on the render thread. Only
  // one visible cache miss is admitted per frame; rasterization runs in one worker.
  if(entityPreviewJob.valid()||entityPreviewBuildFrame==ImGui::GetFrameCount())return false;
  entityPreviewBuildFrame=ImGui::GetFrameCount();++entityPreviewBuilds;
  if(entityPreviews.size()>=128){auto oldest=std::min_element(entityPreviews.begin(),entityPreviews.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});entityPreviewRelease(oldest->second);entityPreviews.erase(oldest);}
  EntityPreview preview;
  try {
   auto bytes=modelResource(def.model);if(bytes.empty()||bytes.size()>64*1024*1024)throw std::runtime_error("Missing or oversized model");
   const auto frame=std::max(0,std::atoi(def.modelFrame.c_str()));
   if(bytes.substr(0,4)=="IDSP"||bytes.substr(0,4)=="IDS2"){
    auto sprite=ftebroom::readSprite(bytes,bytes.substr(0,4)=="IDSP"&&ftebroom::quake1::word(bytes,4)==1?modelResource("gfx/palette.lmp"):std::string{},frame);
    qhandle_t image=0;if(!sprite.image.empty())image=modelImage(sprite.image);
    else{auto name="ftebroom/sprite-"+std::to_string(entityDefinitionEpoch)+"-"+std::to_string(std::hash<std::string>{}(key))+".tga";draw->LoadImageData(name.c_str(),"image/tga",sprite.tga.data(),sprite.tga.size());
     if(sprite.additive){std::string shader="{\n{\nmap "+name+"\nblendfunc add\nrgbgen vertex\nalphagen vertex\n}\n}";image=draw->LoadImageShader((name+"-additive").c_str(),shader.c_str());}else image=draw->LoadImage(name.c_str());}
    preview.spriteWidth=sprite.width;preview.spriteHeight=sprite.height;preview.parts.push_back({image,{}});
   }else if(bytes.substr(0,4)=="IDST"||bytes.substr(0,4)=="IDP2"){
    std::string companion;
    if(bytes.substr(0,4)=="IDST"&&bytes.size()>=244&&!ftebroom::quake1::word(bytes,180)){
      auto stem=def.model.substr(0,def.model.find_last_of('.'));companion=modelResource(stem+"t.mdl");if(companion.empty())companion=modelResource(stem+"T.mdl");}
    auto parts=bytes.substr(0,4)=="IDP2"?ftebroom::legacyModels::md2(bytes,std::max(0,std::atoi(def.modelSkin.c_str())),frame):ftebroom::legacyModels::studio(bytes,companion,std::max(0,std::atoi(def.modelSkin.c_str())));
    for(size_t i=0;i<parts.size();i++){auto& part=parts[i];qhandle_t image=0;ftebroom::preview::Pixels pixels;
      if(!part.embedded.pixels.empty()){auto tga=ftebroom::quake1::tga(part.embedded,{});auto name="ftebroom/legacy-model-"+std::to_string(entityDefinitionEpoch)+"-"+std::to_string(std::hash<std::string>{}(key))+"-"+std::to_string(i)+".tga";draw->LoadImageData(name.c_str(),"image/tga",tga.data(),tga.size());image=draw->LoadImage(name.c_str());pixels=entityPreviewPixels(tga);}
      else image=modelImage(part.image,&pixels);preview.parts.push_back({image,std::move(part.triangles),std::move(pixels)});
    }
   }else if(bytes.compare(0,4,"IDP3")==0){
    auto surfaces=ftebroom::readMd3(bytes,frame);std::map<std::string,std::string> skins;
    if(def.modelSkin.find(".skin")!=std::string::npos){std::istringstream input(modelResource(def.modelSkin));std::string line;while(std::getline(input,line)){auto comma=line.find(',');if(comma!=line.npos){auto value=line.substr(comma+1);value.erase(0,value.find_first_not_of(" \t"));auto end=value.find_last_not_of(" \r\t");if(end!=value.npos)value.erase(end+1);skins[line.substr(0,comma)]=value;}}}
    for(auto& surface:surfaces){auto name=skins.count(surface.name)?skins[surface.name]:surface.shader;ftebroom::preview::Pixels pixels;auto image=modelImage(name,&pixels);preview.parts.push_back({image,std::move(surface.triangles),std::move(pixels)});}
   }else if(std::filesystem::path(def.model).extension()==".bsp"){
    auto surfaces=ftebroom::bspUV::read(bytes,modelResource("gfx/palette.lmp"),{},true);
    for(auto& surface:surfaces){
     if(surface.triangles.empty())continue;
     auto pixels=entityPreviewPixels(surface.image);
     qhandle_t image=0;
     if(pixels.rgba.empty())image=modelImage(surface.name,&pixels);
     preview.parts.push_back({image,std::move(surface.triangles),std::move(pixels)});
    }
   }else{
    auto mdl=ftebroom::readMdl(bytes,std::atoi(def.modelSkin.c_str()),frame);auto palette=modelResource("gfx/palette.lmp");
    auto base=def.model.substr(0,def.model.find_last_of('.'));ftebroom::preview::Pixels skinPixels;auto skin=modelImage(base+"_"+(def.modelSkin.empty()?"0":def.modelSkin),&skinPixels);
    if(!skin){auto pixels=ftebroom::quake1::tga(mdl.skin,palette);auto name="ftebroom/model-"+std::to_string(entityDefinitionEpoch)+"-"+std::to_string(std::hash<std::string>{}(key))+".tga";
     draw->LoadImageData(name.c_str(),"image/tga",pixels.data(),pixels.size());skin=draw->LoadImage(name.c_str());skinPixels=entityPreviewPixels(pixels);}
    preview.parts.push_back({skin,std::move(mdl.triangles),std::move(skinPixels)});
   }
   // Fit all surfaces together; model thumbnails never execute game code.
   float lo[2]={1e30f,1e30f},hi[2]={-1e30f,-1e30f};
   for(auto& part:preview.parts){if(part.image)preview.image=part.image;for(auto& tri:part.triangles)for(auto& v:tri){auto p=v.point;v.point={.7071f*(p[0]-p[1]),.33f*(p[0]+p[1])-.88f*p[2],.62f*(p[0]+p[1])+.47f*p[2]};for(int a=0;a<2;a++){lo[a]=std::min(lo[a],v.point[a]);hi[a]=std::max(hi[a],v.point[a]);}}}
   float extent=std::max({hi[0]-lo[0],hi[1]-lo[1],1.f});
   for(auto& part:preview.parts){for(auto& tri:part.triangles)for(auto& v:tri)for(int a=0;a<2;a++)v.point[a]=.5f+.88f*(v.point[a]-(hi[a]+lo[a])*.5f)/extent;
    std::stable_sort(part.triangles.begin(),part.triangles.end(),[](const auto& a,const auto& b){return a[0].point[2]+a[1].point[2]+a[2].point[2]<b[0].point[2]+b[1].point[2]+b[2].point[2];});}
   for(size_t part=0;part<preview.parts.size();part++)for(size_t tri=0;tri<preview.parts[part].triangles.size();tri++)preview.order.push_back({part,tri});
   std::stable_sort(preview.order.begin(),preview.order.end(),[&](auto a,auto b){auto& x=preview.parts[a.first].triangles[a.second];auto& y=preview.parts[b.first].triangles[b.second];return x[0].point[2]+x[1].point[2]+x[2].point[2]<y[0].point[2]+y[1].point[2]+y[2].point[2];});
  }catch(...){for(auto& part:preview.parts)if(part.image)draw->UnloadImage(part.image);preview={};}
  if(!preview.order.empty()){
   std::vector<ftebroom::preview::Part> parts;for(auto& part:preview.parts)parts.push_back({std::move(part.pixels),std::move(part.triangles)});preview.order.clear();
   entityPreviewJobKey=key;entityPreviewJobGeneration=entityPreviewGeneration;
   entityPreviewJob=std::async(std::launch::async,[parts=std::move(parts)](){try{return ftebroom::preview::raster(parts);}catch(...){return std::string{};}});
  }
  it=entityPreviews.emplace(key,std::move(preview)).first;
 }
 auto& p=it->second;p.used=++entityPreviewClock;if(!p.image&&!p.thumbnail)return false;auto list=ImGui::GetWindowDrawList();list->PushClipRect(tile,{tile.x+size,tile.y+size},true);
 if(p.spriteWidth>0&&p.spriteHeight>0){float scale=size*.88f/std::max(p.spriteWidth,p.spriteHeight);ImVec2 low{tile.x+(size-p.spriteWidth*scale)*.5f,tile.y+(size-p.spriteHeight*scale)*.5f};list->AddImage((ImTextureID)p.image,low,{low.x+p.spriteWidth*scale,low.y+p.spriteHeight*scale});list->PopClipRect();return true;}
 if(p.thumbnail){if(entityPreviewQuadFrame!=ImGui::GetFrameCount()){entityPreviewQuadFrame=ImGui::GetFrameCount();entityPreviewQuadCount=0;}++entityPreviewQuadCount;list->AddImage((ImTextureID)p.thumbnail,tile,{tile.x+size,tile.y+size});list->PopClipRect();return true;}
 list->PopClipRect();return false;
}
