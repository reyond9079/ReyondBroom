// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include "../native/ftebroom_engine_launch.h"
#include <cassert>
#include <chrono>
#include <iostream>
namespace fs=std::filesystem;
using namespace ftebroom;
int main(){
 auto root=fs::temp_directory_path()/fs::u8path("reyond launch ü "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root/"engine dir");fs::create_directories(root/"game root/id1");fs::create_directories(root/"game root/custom mod");fs::create_directories(root/"build dir");
 std::ofstream(root/"engine dir/vkquake.exe")<<"test fixture";
 auto bsp=root/"build dir/test.bsp";std::string bytes(124,'\0');bytes[0]=29;std::ofstream(bsp,std::ios::binary)<<bytes;
 nlohmann::json vars={{"GAME_DIR_PATH",(root/"game root").u8string()},{"MODS",{"id1","custom mod"}},{"MAP_BASE_NAME","test"},{"WORK_DIR_PATH",(root/"build dir").u8string()}};
 RunEngineProfile engine{"vk","VKQuake",(root/"engine dir/vkquake.exe").u8string(),""};quakeEngineDefaults(engine);
 auto plan=engineLaunchPlan(engine,vars,root);
 auto argv=profile::parseArguments(plan.arguments);
 assert(argv==std::vector<std::string>({"-basedir",(root/"game root").u8string(),"-game","custom mod","+map","test"}));
 assert(plan.working==root/"engine dir"&&plan.target==root/"game root/custom mod/maps/test.bsp");
 std::ofstream(root/"build dir/test.lit")<<"lighting";stageEngineMap(plan);
 assert(fs::file_size(plan.target)==124&&fs::exists(root/"game root/custom mod/maps/test.lit"));
 fs::remove(root/"build dir/test.lit");stageEngineMap(plan);assert(!fs::exists(root/"game root/custom mod/maps/test.lit"));
 auto restored=runEngineFromJson(nlohmann::json::parse(runEngineToJson(engine).dump()));
 assert(restored.work==engine.work&&restored.mapFile==engine.mapFile&&restored.parameters==engine.parameters&&restored.path==engine.path);
 auto old=runEngineFromJson({{"id","fte"},{"name","FTE"},{"path",engine.path},{"parameters","-window"}});
 auto legacy=engineLaunchPlan(old,vars,root);assert(legacy.target.empty()&&legacy.arguments=="-window");
 // Compiler tasks interpret relative working directories from the editor's
 // process cwd, not from its installation or the chosen engine directory.
 {
  const auto previous=fs::current_path();
  struct RestoreCwd {fs::path path;~RestoreCwd(){std::error_code ec;fs::current_path(path,ec);}} restore{previous};
  const auto application=root/"separate editor installation",cwd=root/"editor launched from";
  fs::create_directories(application);fs::create_directories(cwd/"relative build directory");fs::current_path(cwd);
  auto relativeVars=vars;relativeVars["WORK_DIR_PATH"]="relative build directory";relativeVars["MAP_BASE_NAME"]="relative-map";
  const auto compiled=cwd/"relative build directory/relative-map.bsp";
  std::ofstream(compiled,std::ios::binary)<<bytes;
  auto relative=engineLaunchPlan(engine,relativeVars,application);
  assert(relative.source==compiled&&relative.working==root/"engine dir");
  assert(relativeVars.at("WORK_DIR_PATH")=="relative build directory");
  assert(!fs::exists(application/"relative build directory/relative-map.bsp"));
  assert(!fs::exists(root/"engine dir/relative build directory/relative-map.bsp"));
  stageEngineMap(relative);assert(fs::file_size(relative.target)==124);
  fs::remove(compiled);
  auto cleanedRelative=engineLaunchPlan(engine,relativeVars,application);
  assert(cleanedRelative.source==relative.target&&cleanedRelative.target==relative.target);
  stageEngineMap(cleanedRelative);assert(fs::file_size(relative.target)==124);
 }
 int failures=0;auto fails=[&](const RunEngineProfile& e,const char* expected){try{engineLaunchPlan(e,vars,root);assert(false);}catch(const std::runtime_error& error){assert(std::string(error.what()).find(expected)!=std::string::npos);++failures;}};
 auto broken=engine;broken.path="missing.exe";fails(broken,"executable not found");
 broken=engine;broken.work="missing-folder";fails(broken,"working directory");
 broken=engine;broken.mapFile="missing.bsp";broken.parameters="-basedir \"${GAME_DIR_PATH}\" -game id1 +map not-deployed";fails(broken,"deployed map also missing");
 broken=engine;broken.parameters="-basedir \"${GAME_DIR_PATH}\" -game ../escape +map test";fails(broken,"-game");
 broken=engine;broken.parameters="-basedir \"${GAME_DIR_PATH}\" -game id1 +map ../escape";fails(broken,"relative map name");
 broken=engine;broken.parameters="-basedir \"${GAME_DIR_PATH}\" -game id1";fails(broken,"relative map name");
 bytes[0]=38;std::ofstream(bsp,std::ios::binary)<<bytes;fails(engine,"Quake 1 BSP");
 bytes[0]=29;bytes[8]=127;std::ofstream(bsp,std::ios::binary)<<bytes;fails(engine,"invalid BSP lump");
 std::ofstream(bsp)<<"bad";fails(engine,"truncated");
 // Mirror the customer's copy + cleanup profile: build outputs disappear,
 // while the exact selected game's deployed BSP and lighting remain usable.
 bytes.assign(124,'\0');bytes[0]=29;std::ofstream(bsp,std::ios::binary)<<bytes;
 std::ofstream(root/"build dir/test.lit")<<"deployed lighting";
 stageEngineMap(engineLaunchPlan(engine,vars,root));
 fs::remove(bsp);fs::remove(root/"build dir/test.lit");
 const auto deployed=root/"game root/custom mod/maps/test.bsp";
 const auto deployedLight=root/"game root/custom mod/maps/test.lit";
 auto cleaned=engineLaunchPlan(engine,vars,root);
 assert(cleaned.source==deployed&&cleaned.target==deployed&&cleaned.working==root/"engine dir");
 assert(profile::parseArguments(cleaned.arguments)==argv);
 stageEngineMap(cleaned);
 assert(fs::file_size(deployed)==124&&fs::file_size(deployedLight)==17);
 // A stale deployed map cannot hide an existing invalid new build output.
 std::ofstream(bsp)<<"bad new output";fails(engine,"truncated");fs::remove(bsp);
 // The fallback accepts only the precise +map destination and validates it
 // exactly as a new source, including the format and every BSP lump.
 broken=engine;broken.parameters="-basedir \"${GAME_DIR_PATH}\" -game id1 +map test";fails(broken,"deployed map also missing");
 bytes[0]=38;std::ofstream(deployed,std::ios::binary)<<bytes;fails(engine,"Quake 1 BSP");
 bytes[0]=29;bytes[8]=127;std::ofstream(deployed,std::ios::binary)<<bytes;fails(engine,"invalid BSP lump");
 std::ofstream(deployed)<<"bad deployed output";fails(engine,"truncated");
 fs::remove(deployed);fails(engine,"deployed map also missing");
 assert(failures==15);
 // Windows paths are passed verbatim to TB's tokenizer, then encoded for CreateProcess.
 assert(profile::parseArguments(R"(-basedir "D:\Game Root" -game "custom mod" +map test)")==std::vector<std::string>({"-basedir",R"(D:\Game Root)","-game","custom mod","+map","test"}));
 fs::remove_all(root);std::cout<<"PASS engine launch: argv, staging, copy-cleanup deployed map, relative workdir from process cwd, validation, workdir, old/new profile persistence (15 negative cases)\n";
}
