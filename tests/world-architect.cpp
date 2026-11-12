// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/world_architect/assets.h"
#include "../plugins/autolevel/al_brush.cpp"
#include <fstream>
#include <iostream>
using namespace ftebroom::worldArchitect;
int main(int argc,char** argv)try{
  if(argc<3)throw std::runtime_error("registry.json output-directory [preset] [seed-count]");
  std::ifstream file(argv[1]);Json json;file>>json;auto registry=readRegistry(json);
  std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
  Json results=Json::array();int count=argc>4?std::stoi(argv[4]):1;
  for(const auto& preset:registry.presets){if(argc>3&&std::string(argv[3])!="*"&&preset.id!=argv[3])continue;
    for(int seed=1;seed<=count;seed++){
      Settings settings;settings.seed=uint64_t(seed);settings.preset=preset.id;settings.damage=preset.damage;if(argc>5)settings.profile=std::stoi(argv[5]);
      auto batch=candidates(settings,registry);if(batch.worlds.empty())throw std::runtime_error(batch.rejects.empty()?"No candidates":batch.rejects.front());
      auto world=std::move(batch.worlds.front());auto proof=diagnostics(world);proof["attempted"]=settings.candidates;proof["rejected"]=batch.rejects;
      auto repeat=settings;repeat.seed=world.seed;auto replay=generate(repeat,registry);if(replay.map!=world.map)throw std::runtime_error("Seed regeneration mismatch");
      auto restored=deserialize(serialize(settings),registry);if(serialize(restored)!=serialize(settings))throw std::runtime_error("Project roundtrip mismatch");
      auto parsed=al::parseMap(world.map);if(parsed.brushes.size()!=brushCount(world))throw std::runtime_error("MAP brush roundtrip mismatch");
      auto path=output/(preset.id+"-"+std::to_string(seed));
      if(seed==1){auto bundle=assetBundle(world,registry,ftebroom::defaultQuakePalette());
      for(const auto& [name,bytes]:bundle){auto asset=output/std::filesystem::u8path(name);std::filesystem::create_directories(asset.parent_path());std::ofstream(asset,std::ios::binary).write(bytes.data(),bytes.size());}
      {auto list=output/"scripts/shaderlist.txt";std::string text;if(std::ifstream previous{list};previous)text.assign(std::istreambuf_iterator<char>(previous),{});auto name=shaderStem(world);if(text.find(name)==text.npos)std::ofstream(list,std::ios::app)<<name<<'\n';}
      std::ofstream(path.string()+".map")<<withWad(argc>6?groupedMap(world,argv[6]):world.map,world,(std::filesystem::absolute(output)/wadFilename(world,ftebroom::defaultQuakePalette())).generic_string());std::ofstream(path.string()+".json")<<proof.dump(2);}
      // Keep one complete spatial proof per preset, then retain compact,
      // independently useful variation/performance records for the seed corpus.
      Json masses=Json::array(),dimensions=Json::array();std::set<int> heights;
      for(const auto& b:world.spatial.buildings){masses.push_back({vector(b.low),vector(b.high)});for(const auto& room:b.rooms){dimensions.push_back(vector(room.high-room.low));heights.insert(int(room.high.z-room.low.z));}}
      proof["variation"]={{"masses",hash(masses.dump())},{"rooms",hash(dimensions.dump())},{"roads",hash(proof.at("roads").dump())},{"mission",hash(proof.at("missionEdges").dump())},{"architecture",world.architectureKey},{"roomCount",dimensions.size()},{"roomHeightKinds",heights.size()}};
      if(seed>1)for(const auto* field:{"rooms","roads","missionNodes","missionEdges","warnings","stageSeeds"})proof.erase(field);
      results.push_back(proof);
      std::cout<<"PASS "<<preset.id<<" seed "<<seed<<" brushes "<<world.diagnostics.brushes<<" route "<<world.diagnostics.routeLength<<'\n'<<std::flush;
    }
  }
  std::ofstream(output/"results.json")<<results.dump(2);
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
