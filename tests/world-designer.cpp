// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/world_architect/assets.h"
#include "../plugins/autolevel/al_brush.cpp"
#include <fstream>
#include <iostream>
using namespace ftebroom::worldArchitect;
int main(int argc,char** argv)try{
  if(argc!=3)throw std::runtime_error("registry.json output-directory");Json json;std::ifstream(argv[1])>>json;auto registry=readRegistry(json);std::filesystem::path output=argv[2];Json results=Json::array();
  for(int profile:{2,0,1})for(const auto& name:{"interior","roads","sky-seal"}){
    if(std::string(name)=="sky-seal"&&profile!=2)continue;
    Settings settings;settings.profile=profile;settings.candidates=1;
    if(std::string(name)=="interior"){
      settings.layout="spine";settings.width=1920;settings.theme="brick";auto initial=generate(settings,registry);settings.authoredMission=initial.mission;
      auto& b=initial.spatial.buildings.front();settings.influences.push_back({1,b.low-V{8,8,8},b.high+V{8,8,8},"preserve",1});
      const auto& room=b.rooms.back();MissionNode pin;pin.id=initial.mission.nodes[2].id;pin.position={(room.low.x+room.high.x)/2,(room.low.y+room.high.y)/2,room.low.z+1};settings.missionPins.push_back(pin);
    }else if(std::string(name)=="roads"){
      settings.preset="evacuation";auto initial=generate(settings,registry);auto road=initial.spatial.roads.front();road.width=128;road.control=(road.a+road.b)*.5+V{0,32,0};road.pinned=true;settings.roadPins.push_back(road);
    }else{
      settings.preset="metro";settings.seed=9949357162622361994ULL;
    }
    auto world=generate(deserialize(serialize(settings),registry),registry);auto folder=output/((profile<2?"q3-":"q1-")+std::to_string(profile))/"baseq3";std::filesystem::create_directories(folder);auto bundle=assetBundle(world,registry,ftebroom::defaultQuakePalette());
    for(const auto& [file,bytes]:bundle){auto path=folder/std::filesystem::u8path(file);std::filesystem::create_directories(path.parent_path());std::ofstream(path,std::ios::binary).write(bytes.data(),bytes.size());}
    std::ofstream(folder/"scripts/shaderlist.txt",std::ios::app)<<shaderStem(world)<<'\n';auto path=folder/(std::string(name)+".map");std::ofstream(path)<<withWad(groupedMap(world,"qa-designer"),world,(std::filesystem::absolute(folder)/wadFilename(world,ftebroom::defaultQuakePalette())).generic_string());
    auto record=diagnostics(world);record["path"]=std::filesystem::absolute(path).generic_string();record["case"]=name;record["settings"]=serialize(settings);results.push_back(record);std::cout<<"PASS designer "<<name<<" profile "<<profile<<'\n';
  }
  std::ofstream(output/"results.json")<<results.dump(2);
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
