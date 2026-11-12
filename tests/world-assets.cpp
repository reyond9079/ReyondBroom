// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/world_architect/assets.h"
#include "../plugins/autolevel/al_brush.cpp"
#include <fstream>
#include <iostream>
using namespace ftebroom::worldArchitect;
int main(int argc,char** argv)try{
  if(argc!=2)throw std::runtime_error("registry.json");Json json;std::ifstream(argv[1])>>json;auto a=readRegistry(json);Settings settings;settings.preset="metro";auto first=generate(settings,a);
  for(auto& theme:json["themes"])if(theme["id"]==first.theme)theme["colors"][0][0]=int(theme["colors"][0][0])+1;auto b=readRegistry(json);auto second=generate(settings,b);int count=0;
  auto check=[&](bool value,const char* text){if(!value)throw std::runtime_error(text);std::cout<<"PASS "<<text<<'\n';count++;};
  const auto palette=ftebroom::defaultQuakePalette();auto oldBundle=assetBundle(first,a,palette),newBundle=assetBundle(second,b,palette);
  check(first.assetKey!=second.assetKey&&materialId(first,0)!=materialId(second,0),"a changed art dependency receives distinct material identities");
  for(const auto& [file,bytes]:oldBundle){auto found=newBundle.find(file);if(found!=newBundle.end())check(found->second==bytes,"packs only share filenames when the bytes are identical");}
  check(oldBundle.count(wadFilename(first,palette))&&newBundle.count(wadFilename(second,palette))&&wadFilename(first,palette)!=wadFilename(second,palette),"old and updated procedural WADs coexist without replacement");
  check(oldBundle.count("scripts/"+shaderStem(first)+".shader")&&newBundle.count("scripts/"+shaderStem(second)+".shader")&&shaderStem(first)!=shaderStem(second),"shader lists can retain both versions of an art pack");
  for(const auto& element:first.architecture.elements)for(const auto& face:element.brush.faces)if(face.material.size()>15)throw std::runtime_error("Classic WAD material exceeds 15 bytes");check(true,"classic masonry, water and sky identifiers fit their actual WAD name limit");
  auto anotherPalette=palette;anotherPalette[0]=static_cast<char>(static_cast<unsigned char>(anotherPalette[0])^1);check(wadFilename(first,palette)!=wadFilename(first,anotherPalette),"a palette-dependent WAD has a distinct filename");
  check(assetBundle(first,a,palette)==oldBundle,"unchanged art inputs reproduce the same archive bytes and names");
  std::cout<<count<<" World Architect asset contracts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
