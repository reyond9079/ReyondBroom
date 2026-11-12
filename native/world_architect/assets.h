// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "generator.h"
#include "../ftebroom_wad_import.h"
#include "../ftebroom_quake_palette.h"

namespace ftebroom::worldArchitect {
inline std::string wadFilename(const WorldPlan& w,const std::string& palette){return "world-architect-"+w.theme+"-"+(w.assetKey.empty()?"legacy":w.assetKey)+"-p"+std::to_string(w.profile)+"-"+shortKey(hash(palette))+".wad";}
inline std::string shaderStem(const WorldPlan& w){return "world-architect-"+w.theme+(w.assetKey.empty()?"":"-"+w.assetKey);}
struct Image {unsigned width=128,height=128;std::vector<unsigned char> rgba;};
inline Image texture(const ArtTheme& theme,int slot){
  if(slot<0||slot>6)throw std::runtime_error("Unknown architectural material slot");
  Image image;image.rgba.resize(size_t(image.width)*image.height*4);const auto color=theme.colors[slot==6?3:slot];const auto themeSeed=hash(theme.id);
  for(unsigned y=0;y<image.height;y++)for(unsigned x=0;x<image.width;x++){
    double noise=double(mix(uint64_t(x)+uint64_t(y)*128+themeSeed)%17)-8,shade=0;
    if(slot==0){bool joint=x%32<2||y%32<2;shade=joint?-25:((x/32+y/32)%2?4:-4);}
    if(slot==1){int row=y/16;bool mortar=y%16<2||(x+unsigned(row%2)*32)%64<2;shade=mortar?-24:((mix(uint64_t(row*4+x/32))%7)-3.);}
    if(slot==2){shade=y%32<2?-18:3*std::sin(double(x)*.1);}
    if(slot==3){shade=14*std::sin(double(x+y)*.07)-double(y%23<2)*12;}
    if(slot==4){shade=8*std::sin(x*.35)+4*std::sin(y*.09);if(x%32<3)shade-=20;}
    if(slot==5){shade=((x+y)/16)%2?-28:4;}
    if(slot==6){shade=10*std::sin(x*.12+4*std::sin(y*.04))+6*std::cos(y*.17+x*.03);noise*=.3;}
    if(slot==1&&theme.grammar=="clinical"){shade=x%16<1||y%16<1?-24:5;noise*=.35;}
    if(slot==1&&theme.grammar=="industrial"){shade=12*std::cos(x*6.283185307179586/16);if(x%64<4&&y%32<4)shade-=26;}
    if(slot==1&&theme.grammar=="brutalist"){shade=x%64<3||y%64<3?-22:1;if((x%64<12)&&(y%64<12))shade-=10;}
    if(slot==1&&theme.grammar=="subterranean"){shade=y%12<2||(x+(y/12)%2*16)%32<2?-25:2;}
    if(slot==1&&theme.grammar=="civic"){shade=x%32<3||y%64<4?-32:8;noise*=.6;}
    if(slot==0&&theme.grammar=="industrial")shade=x%64<3||y%32<3?-28:4;
    if(slot==0&&theme.grammar=="clinical")shade=x%16<1||y%16<1?-18:((x/16+y/16)%2?8:-5);
    if(slot<5&&theme.weathering>0){double stain=std::max(0.,std::sin(x*.031+std::sin(y*.017))*std::cos(y*.024+x*.011));shade-=theme.weathering*stain*30;}
    for(int c=0;c<3;c++)image.rgba[(size_t(y)*128+x)*4+c]=static_cast<unsigned char>(std::clamp(color[c]+noise+shade,0.,255.));image.rgba[(size_t(y)*128+x)*4+3]=255;
  }return image;
}
inline std::string tga(const Image& image){
  std::string out(18,0);out[2]=2;out[12]=char(image.width);out[13]=char(image.width>>8);out[14]=char(image.height);out[15]=char(image.height>>8);out[16]=32;out[17]=0x28;
  for(size_t i=0;i<image.rgba.size();i+=4){out+=char(image.rgba[i+2]);out+=char(image.rgba[i+1]);out+=char(image.rgba[i]);out+=char(image.rgba[i+3]);}return out;
}
inline std::map<std::string,std::string> shaderDefinitions(const WorldPlan& w){
  std::map<std::string,std::string> shaders;
  for(int slot=0;slot<6;slot++){
    auto id=materialId(w,slot);shaders["textures/"+id]="{\nqer_editorimage textures/"+id+".tga\n{\nmap $lightmap\nrgbgen identity\n}\n{\nmap textures/"+id+".tga\nblendfunc filter\nrgbgen identity\n}\n}\n";
  }
  if(std::any_of(w.architecture.elements.begin(),w.architecture.elements.end(),[](const auto& e){return e.kind=="floodwater";})){
    auto id=assetId(w,6);shaders["textures/"+id]="{\nqer_editorimage textures/"+id+".tga\nqer_trans 0.72\nsurfaceparm water\nsurfaceparm trans\nsurfaceparm nonsolid\nsurfaceparm nolightmap\n{\nmap textures/"+id+".tga\nblendfunc blend\nrgbgen identity\nalphagen const 0.72\ntcmod turb 0 0.03 0 0.1\n}\n}\n";
  }
  shaders["textures/"+skyMaterial(w)]="{\nqer_editorimage textures/sky.tga\nsurfaceparm sky\nsurfaceparm nolightmap\nq3map_sun 1 0.88 0.7 80 120 50\nskyParms - 512 -\n{\nmap textures/sky.tga\ntcmod scale 2 2\nrgbgen identity\n}\n}\n";
  return shaders;
}
inline std::map<std::string,std::string> assetBundle(const WorldPlan& w,const Registry& registry,const std::string& palette){
  std::map<std::string,std::string> bundle;std::string wad;const bool hl=w.profile==4;
  const bool water=std::any_of(w.architecture.elements.begin(),w.architecture.elements.end(),[](const auto& e){return e.kind=="floodwater";});
  for(int slot=0;slot<(water?7:6);slot++){
    auto image=texture(registry.theme(w.theme),slot);auto name=assetId(w,slot);
    const auto wadName=slot==6?classicWaterId(w,hl):name;
    auto indexed=quantizeFolderImage(image.rgba.data(),image.width,image.height,palette,wadName);
    if(w.profile==2){PaletteQuantizer opaque(palette,224);for(size_t i=0;i<indexed.pixels.size();i++)indexed.pixels[i]=opaque.nearest(image.rgba[i*4],image.rgba[i*4+1],image.rgba[i*4+2]);}
    wad=addWadTexture(wad,std::move(indexed),palette,false,hl);
    bundle["textures/"+name+".tga"]=tga(image);
  }
  // Original sky pixels are deliberately included in the WAD: real qbsp/light
  // validation must not depend on the user's installed texture packs.
  Image sky;sky.width=256;sky.height=128;sky.rgba.resize(256*128*4,255);
  for(unsigned y=0;y<128;y++)for(unsigned x=0;x<256;x++)for(int c=0;c<3;c++)sky.rgba[(y*256+x)*4+c]=static_cast<unsigned char>(40+c*10+y/8+(mix(x+y*256)%11));
  wad=addWadTexture(wad,quantizeFolderImage(sky.rgba.data(),sky.width,sky.height,palette,classicSkyId(w)),palette,false,hl);
  bundle["textures/sky.tga"]=tga(sky);bundle[wadFilename(w,palette)]=std::move(wad);
  std::string shaders;for(const auto& [name,body]:shaderDefinitions(w))shaders+=name+"\n"+body;
  bundle["scripts/"+shaderStem(w)+".shader"]=std::move(shaders);
  bundle["WORLD-ARCHITECT-ASSETS.txt"]="Original ReyondBroom World Architect procedural masonry, concrete, floor, metal, wood and sky. GPL-3.0-or-later. No external asset packs required.\n";
  if(!w.authoredSources.empty()){
    Json manifest={{"schemaVersion",1},{"modules",Json::array()}};
    for(const auto& [key,module]:w.authoredSources){auto name="modules/"+key+".map";bundle[name]=module.map;manifest["modules"].push_back({{"file",name},{"id",module.id},{"source",module.source},{"license",module.license},{"materialSlots",module.materials}});}
    auto identity=std::to_string(hash(manifest.dump()));bundle["modules/manifest-"+identity+".json"]=manifest.dump(2)+"\n";
  }
  return bundle;
}
inline std::string withWad(std::string map,const WorldPlan& w,const std::string& path){
  if(path.find_first_of("\"\r\n")!=path.npos)throw std::runtime_error("Unsafe WAD path");
  if(w.profile==2||w.profile==4){auto at=map.find("\"worldspawn\"\n");if(at==map.npos)throw std::runtime_error("Missing generated worldspawn");map.insert(at+13,"\"wad\" \""+path+"\"\n");}return map;
}
}
