// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_plant_export.h"
#include "../native/ftebroom_mdl.h"
#include "../native/ftebroom_legacy_models.h"
#include "../native/ftebroom_quake_palette.h"
#include <iostream>
using namespace ftebroom;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
int main()try{
  // A tiny real trunk exercises serializers, not botanical generation.
  plant::Builder builder(200);builder.tube({{0,0,0},{0,0,64}},6,2,6,plant::Bark,plant::Part::Trunk);
  auto mesh=builder.mesh;plant::validate(mesh);
  plant::Textures maps;for(auto* image:{&maps.albedo,&maps.normal,&maps.orm}){image->width=image->height=16;image->rgba.resize(16*16*4,255);}
  auto palette=defaultQuakePalette();
  check(std::size(plantExport::choices)==8,"all implemented exporters have explicit choices");
  for(const auto& spec:plantExport::choices){
    auto path=plantExport::pathFor(std::filesystem::u8path("/tmp/plant.wrong"),spec.format);
    check(path.extension()==spec.extension,"selected format determines the saved suffix");
    auto bundle=plantExport::encode(spec.format,path.filename().u8string(),mesh,maps,palette);
    const auto& bytes=bundle.at(path.filename().u8string());check(!bytes.empty(),spec.label);
    if(spec.format==plantExport::Format::Mdl)check(!readMdl(bytes).triangles.empty(),"Quake MDL production decoder roundtrip");
    if(spec.format==plantExport::Format::Studio)check(!legacyModels::studio(bytes,{}).at(0).triangles.empty(),"GoldSrc MDL production decoder roundtrip");
    if(spec.format==plantExport::Format::Md2){auto model=legacyModels::md2(bytes);check(!model.at(0).triangles.empty()&&bundle.count(model.at(0).image),"MD2 decoder resolves its companion skin");}
    if(spec.format==plantExport::Format::Md3)check(!readMd3(bytes).at(0).triangles.empty(),"MD3 production decoder roundtrip");
    if(spec.format==plantExport::Format::Iqm)check(bytes.substr(0,15)==std::string("INTERQUAKEMODEL\0",15),"IQM signature");
    if(spec.format==plantExport::Format::Md5)check(bytes.find("MD5Version 10")!=bytes.npos&&bytes.find("numJoints 1")!=bytes.npos,"MD5 static root joint");
  }
  std::cout<<"Plant export manager passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
