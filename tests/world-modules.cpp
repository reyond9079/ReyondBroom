// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/world_architect/assets.h"
#include "../plugins/autolevel/al_brush.cpp"
#include <fstream>
#include <iostream>
using namespace ftebroom::worldArchitect;
int main(int argc,char** argv)try{
  if(argc<2)throw std::runtime_error("registry.json [fixture-directory]");Json json;std::ifstream(argv[1])>>json;auto registry=readRegistry(json);int count=0;
  auto check=[&](bool ok,const char* name){if(!ok)throw std::runtime_error(name);std::cout<<"PASS "<<name<<'\n';count++;};
  auto rejects=[&](auto action,const char* name){try{action();}catch(const std::exception&){check(true,name);return;}throw std::runtime_error(std::string("Expected rejection: ")+name);};
  al::Geometry arch;arch.brushes={al::box({-64,-8,0},{-56,8,96},"authored/stone"),al::box({56,-8,0},{64,8,96},"authored/stone"),al::box({-56,-8,80},{56,8,96},"authored/metal")};arch.entities.push_back({{{"classname","light"},{"origin","0 0 64"},{"light","80"}}});
  AuthoredModule module;module.id="qa-arch";module.function="any";module.source="Original test arch";module.license="GPL-3.0-or-later";module.map=al::writeMap(arch);module.materials={{"authored/stone",2},{"authored/metal",3}};
  auto pack=[&](const AuthoredModule& m){return Json{{"schemaVersion",1},{"modules",Json::array({moduleJson(m)})}};};
  auto read=readModulePack(pack(module));check(read.size()==1&&read[0].map==module.map,"pack parsing preserves the authored source MAP byte for byte");
  Settings settings;settings.candidates=1;auto baseline=generate(settings,registry);auto room=baseline.spatial.buildings[0].rooms[0];settings.modules=read;settings.roomModules[room.id]=module.id;auto world=generate(settings,registry);
  check(std::count_if(world.architecture.elements.begin(),world.architecture.elements.end(),[](const auto& e){return e.kind=="authored module / qa-arch";})==3,"three authored brushes are actually placed in the assigned functional room");
  check(world.diagnostics.valid&&world.authoredSources.size()==1,"the authored arch retains a physically validated complete player route");
  auto outside=[&](const WorldPlan& w){std::string text;for(const auto& e:w.architecture.elements){auto poly=al::polyhedron(e.brush);if(!inside((poly.min+poly.max)*.5,room.low,room.high))text+=al::writeBrush(e.brush);}return text;};
  check(outside(world)==outside(baseline),"module assignment preserves every brush outside the chosen room");
  auto restored=generate(deserialize(serialize(settings),registry),registry);check(restored.map==world.map,"embedded authored sources and room assignments replay exactly after reopen");
  auto bundle=assetBundle(world,registry,ftebroom::defaultQuakePalette());bool raw=false,license=false;for(const auto& [name,bytes]:bundle){if(name.find("modules/qa-arch-")==0)raw=bytes==module.map;if(name.find("modules/manifest-")==0)license=bytes.find(module.license)!=bytes.npos&&bytes.find(module.source)!=bytes.npos;}check(raw&&license,"portable export includes unchanged authored MAP source and its material/license manifest");
  auto missing=module;missing.materials.erase("authored/stone");rejects([&]{readModulePack(pack(missing));},"undeclared authored material substitutions fail explicitly");
  auto wrong=settings;wrong.modules[0].profile=0;rejects([&]{generate(wrong,registry);},"authored modules cannot silently cross incompatible game profiles");
  auto absent=settings;absent.roomModules.clear();absent.roomModules[99999]=module.id;rejects([&]{generate(absent,registry);},"module assignment rejects a missing functional room");
  auto blocked=settings;blocked.modules[0].map=al::writeMap({{al::box({-32,-32,0},{32,32,80},"authored/stone")},{}});rejects([&]{generate(blocked,registry);},"authored geometry cannot obstruct a reserved player anchor");
  auto huge=settings;huge.modules[0].map=al::writeMap({{al::box({-2048,-8,0},{2048,8,80},"authored/stone")},{}});rejects([&]{generate(huge,registry);},"oversized modules fail instead of clipping or rescaling author geometry");
  auto entity=settings;auto monster=arch;monster.entities[0].fields["classname"]="monster_ogre";entity.modules[0].map=al::writeMap(monster);rejects([&]{generate(entity,registry);},"uncertified runtime entity semantics are rejected while original source stays intact");
  auto pinned=pinBuilding(settings,world,registry,100);pinned.seed=57;auto pinWorld=generate(pinned,registry);check(pinWorld.map!=world.map&&outside(pinWorld)==outside(world)&&pinWorld.authoredSources.size()==1,"building pins retain actual authored module geometry and export source provenance");
  if(argc>2){auto out=std::filesystem::path(argv[2]);std::filesystem::create_directories(out);for(int profile:{2,0,1}){auto fixture=module;fixture.profile=profile;std::ofstream(out/("module-pack-"+std::to_string(profile)+".json"))<<pack(fixture).dump(2);auto input=settings;input.profile=profile;input.modules[0]=fixture;auto scene=generate(input,registry);auto folder=out/("profile-"+std::to_string(profile))/"baseq3";std::filesystem::create_directories(folder);for(const auto& [name,bytes]:assetBundle(scene,registry,ftebroom::defaultQuakePalette())){auto target=folder/name;std::filesystem::create_directories(target.parent_path());std::ofstream(target,std::ios::binary).write(bytes.data(),bytes.size());}std::ofstream(folder/"scripts/shaderlist.txt")<<shaderStem(scene)<<'\n';std::ofstream(folder/"module.map")<<withWad(groupedMap(scene,"qa-module"),scene,(std::filesystem::absolute(folder)/wadFilename(scene,ftebroom::defaultQuakePalette())).generic_string());}std::ofstream(out/"original-module.map")<<module.map;}
  std::cout<<count<<" authored module contracts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
