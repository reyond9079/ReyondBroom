// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/plant/export.h"
#include "../native/plant/presets.h"
#include "../native/ftebroom_mdl.h"
#include "../native/ftebroom_legacy_models.h"
#include "../native/ftebroom_legacy_images.h"
#include "../native/ftebroom_quake_palette.h"
#include "../native/ftebroom_static_models.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace ftebroom::plant;
static void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static void write(const std::filesystem::path& path,const std::string& bytes){std::ofstream out(path,std::ios::binary);out.write(bytes.data(),bytes.size());require(bool(out),"write artifact");}
int main(int argc,char** argv)try{
  auto folder=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::path("plant-target-results");std::filesystem::create_directories(folder);
  auto q1Palette=ftebroom::defaultQuakePalette(),q2Palette=q1Palette;
  // CI may supply the active game's palette; both encoders must retain it.
  if(argc>2){std::ifstream in(argv[2],std::ios::binary);std::string pcx((std::istreambuf_iterator<char>(in)),{});q2Palette=ftebroom::legacy::pcx(pcx).palette;}
  Json rows=Json::array();int cases=0;
  for(auto source:presets()){
    auto saved=serialize(source);auto full=generate(source,0);auto maps=textureMaps(source,nullptr,nullptr,16);
    for(int profile:{2,3}){
      auto geometry=targetGeometry(source,full,profile);validate(geometry);auto model=aliasMesh(geometry,"plant");auto& surface=model.surfaces.at(0);
      require(model.surfaces.size()==1&&model.frames==1,"one vanilla alias frame / material");
      require(surface.frames[0].size()<=(profile==2?1024u:2048u)&&surface.triangles.size()<=(profile==2?2048u:4096u),"actual vanilla vertex and triangle limits "+source.preset);
      bool sourceLeaves=std::any_of(full.faces.begin(),full.faces.end(),[](auto& f){return f.part==Part::Leaves;});
      require(!sourceLeaves||std::any_of(geometry.faces.begin(),geometry.faces.end(),[](auto& f){return f.part==Part::Leaves;}),"runtime LOD retains foliage "+source.preset);
      auto skin=aliasSkin(maps.albedo);for(size_t p=3;p<skin.rgba.size();p+=4)require(skin.rgba[p]==255,"vanilla skin is opaque");
      std::string encoded;
      if(profile==2){
        encoded=ftebroom::modelExport::mdl(model,{skin},q1Palette);auto imported=ftebroom::readMdl(encoded);
        require(encoded.substr(0,4)=="IDPO"&&imported.triangles.size()==surface.triangles.size(),"production Quake MDL importer roundtrip");
        require(imported.skin.width==256&&imported.skin.height==256,"vanilla skin dimensions");
        for(auto p:imported.skin.pixels)require(p<224,"Quake skin avoids fullbright entries");
      }else{
        auto exported=ftebroom::modelExport::md2(model,{skin},q2Palette,"models/plant.pcx");encoded=exported.model;
        auto imported=ftebroom::legacyModels::md2(encoded);auto image=ftebroom::legacy::pcx(exported.skin);
        require(encoded.substr(0,4)=="IDP2"&&imported.at(0).triangles.size()==surface.triangles.size()&&imported.at(0).image=="models/plant.pcx","production Quake II MD2 importer roundtrip");
        // Independently follow vanilla ref_gl/gl_mesh.c's command-stream path,
        // which does not use the triangle table accepted by our CPU importer.
        auto word=[&](size_t p){return ftebroom::quake1::word(encoded,p);};
        size_t cursor=word(60),commandEnd=cursor+size_t(word(36))*4;require(commandEnd==encoded.size()&&word(64)==encoded.size(),"MD2 GL command offsets/end are valid");size_t commandTriangles=0;
        auto canonical=[](std::array<unsigned,3> a){auto b=std::array<unsigned,3>{a[1],a[2],a[0]},c=std::array<unsigned,3>{a[2],a[0],a[1]};return std::min(a,std::min(b,c));};
        std::multiset<std::array<unsigned,3>> expectedTriangles,drawnTriangles;for(auto triangle:surface.triangles)expectedTriangles.insert(canonical(triangle));
        for(;;){require(cursor+4<=commandEnd,"MD2 GL stream has a terminator");int count=int32_t(word(cursor));cursor+=4;if(!count)break;
          unsigned points=unsigned(std::abs(count));require(points>=3&&cursor+size_t(points)*12<=commandEnd,"bounded MD2 GL strip/fan");commandTriangles+=points-2;
          std::vector<unsigned> sequence;
          for(unsigned point=0;point<points;point++,cursor+=12){float u=ftebroom::legacyModels::real(encoded,cursor),v=ftebroom::legacyModels::real(encoded,cursor+4);unsigned vertex=word(cursor+8);require(u>=0&&u<=1&&v>=0&&v<=1&&vertex<surface.frames[0].size(),"MD2 GL command has valid UV and vertex");sequence.push_back(vertex);
            auto st=word(48)+vertex*4;float su=(ftebroom::legacyModels::half(encoded,st)+.5f)/word(8),sv=(ftebroom::legacyModels::half(encoded,st+2)+.5f)/word(12);require(std::abs(u-su)<.000001&&std::abs(v-sv)<.000001,"GL commands retain indexed UVs with pixel centers");}
          for(size_t i=2;i<sequence.size();i++){std::array<unsigned,3> triangle=count<0?std::array<unsigned,3>{sequence[0],sequence[i-1],sequence[i]}:std::array<unsigned,3>{sequence[i-2],sequence[i-1],sequence[i]};if(count>0&&i%2)std::swap(triangle[0],triangle[1]);drawnTriangles.insert(canonical(triangle));}
        }
        require(cursor==commandEnd&&commandTriangles==surface.triangles.size()&&drawnTriangles==expectedTriangles,"vanilla Quake II OpenGL stream draws every exported triangle with original winding");
        require(image.palette==q2Palette&&image.width==256&&image.height==256,"Quake II PCX retains active palette and valid dimensions");
        if(source.preset=="oak")write(folder/"oak.pcx",exported.skin);
      }
      if(source.preset=="oak"||source.preset=="pine"||source.preset=="umbrella-acacia")write(folder/(source.preset+targetExtension(profile)),encoded);
      rows.push_back({{"preset",source.preset},{"profile",profile},{"format",targetExtension(profile)},{"vertices",surface.frames[0].size()},{"triangles",surface.triangles.size()},{"decoded",true}});++cases;
    }
    require(serialize(source)==saved,"target baking preserves editable source");
  }
  auto oak=presets().front();auto maps=textureMaps(oak,nullptr,nullptr,64);auto mips=coverageMips(maps.albedo);
  Json mipRows=Json::array();for(auto& mip:mips){
    int tiles=mip.width>=4&&mip.height>=4?4:1;
    for(int material:{Leaf,LeafLight,LeafMature,LeafYoung,Needle,NeedleLight,NeedleMature}){
      int tx=tiles==4?material%4:0,ty=tiles==4?material/4:0;size_t coverage=0;
      for(int y=ty*mip.height/tiles;y<(ty+1)*mip.height/tiles;y++)for(int x=tx*mip.width/tiles;x<(tx+1)*mip.width/tiles;x++)coverage+=mip.rgba[(y*mip.width+x)*4+3]>=128;
      require(coverage>0,"foliage survives every distance mip down to 1x1");
    }
    mipRows.push_back({{"width",mip.width},{"height",mip.height}});
  }
  auto dds=coverageDds(maps.albedo);require(dds.substr(0,4)=="DDS "&&ftebroom::quake1::word(dds,28)==mips.size(),"DDS header has real explicit mip chain");
  size_t bytes=128;for(auto& mip:mips)bytes+=mip.rgba.size();require(dds.size()==bytes,"DDS mip offsets and final size");write(folder/"oak-atlas.dds",dds);write(folder/"oak-atlas.tga",tga(maps.albedo));
  auto shaderText=shader("textures/reyond-plugins/oak","textures/reyond-plugins/oak.dds",true);require(shaderText.find("nomipmaps")==std::string::npos&&shaderText.find("alphaFunc GE128")!=shaderText.npos,"cutout mipmapping stays enabled");
  write(folder/"oak.md3",md3(generate(oak),"textures/reyond-plugins/oak"));write(folder/"oak.shader",shaderText);
  auto dense=oak;dense.p.budget=100000;dense.p.branches=30;dense.p.depth=4;dense.p.leaves=40;
  auto fullDense=generate(dense);auto q3=targetModel(targetGeometry(dense,fullDense,0),"textures/reyond-plugins/oak",0);
  require(q3.surfaces.size()<=32,"Quake III runtime LOD respects 32 MD3 surfaces");
  auto q3Bytes=ftebroom::modelExport::md3(q3);auto q3Imported=ftebroom::readMd3(q3Bytes);require(q3Imported.size()==q3.surfaces.size(),"dense Q3 runtime LOD uses actual MD3 decoder");
  std::string entity="{\n\"classname\" \"worldspawn\"\n}\n{\n\"classname\" \"func_model\"\n\"model\" \"models/reyond-plugins/oak.mdl\"\n\"_reyondbroom_authoring\" \"1\"\n}\n";
  for(int profile:{2,3}){size_t changed=0;bool stopped=false;try{ftebroom::prepareStaticModels(entity,profile,changed);}catch(std::runtime_error&){stopped=true;}require(stopped,"base game without arbitrary-model game code cannot silently compile broken func_model");}
  auto fte=entity;fte.insert(fte.find("\"model\""),"\"_reyondbroom_fte_q1\" \"1\"\n");size_t changed=0;auto adapted=ftebroom::prepareStaticModels(fte,2,changed);require(changed==1&&adapted.find("misc_model")!=adapted.npos&&adapted.find("oak.mdl")!=adapted.npos,"explicit FTE Q1 fallback retains native MDL");
  write(folder/"results.json",Json{{"targetCases",cases},{"targets",rows},{"coverageMips",mipRows},{"vanillaCompileGate",true},{"explicitFteQ1",true}}.dump(2));
  std::cout<<"PASS "<<cases<<" native plant MDL/MD2 importer roundtrips; all mip distances retain foliage; compile capabilities checked\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
