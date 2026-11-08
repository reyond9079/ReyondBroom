// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_run_profile.h"
#include "profile/el.h"
#include "profile/files.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <cctype>
#include <cstdlib>
namespace ftebroom {
inline void quakeEngineDefaults(RunEngineProfile& e) {
  e.work="${ENGINE_DIR_PATH}";
  e.parameters="-basedir \"${GAME_DIR_PATH}\" -game \"${MODS[-1]}\" +map \"${MAP_BASE_NAME}\"";
  e.mapFile="${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp";
}
struct EngineLaunch {
  std::filesystem::path executable,working,source,target;
  std::string arguments;
};
inline std::filesystem::path enginePath(const std::string& text,const std::filesystem::path& base) {
  auto p=profile::parsePath(text);return (p.is_absolute()?p:base/p).lexically_normal();
}
inline EngineLaunch engineLaunchPlan(const RunEngineProfile& e,nlohmann::json vars,
                                    const std::filesystem::path& base) {
  EngineLaunch out;
  const auto path=profile::interpolate(e.path,vars);
  if(path.empty())throw std::runtime_error("Set the engine executable path in Game Engines.");
  out.executable=enginePath(path,base);
  if(!std::filesystem::is_regular_file(out.executable)&&!profile::parsePath(path).has_parent_path()){
    // Preserve ordinary PATH-based engine commands as well as absolute paths.
    std::istringstream paths(std::getenv("PATH")?std::getenv("PATH"):"");std::string entry;
#ifdef _WIN32
    constexpr char separator=';';
#else
    constexpr char separator=':';
#endif
    while(std::getline(paths,entry,separator)){
      if(entry.empty())continue;auto candidate=std::filesystem::u8path(entry)/profile::parsePath(path);
#ifdef _WIN32
      if(candidate.extension().empty())candidate+=".exe";
#endif
      if(std::filesystem::is_regular_file(candidate)){out.executable=std::filesystem::absolute(candidate);break;}
    }
  }
  if(!std::filesystem::is_regular_file(out.executable))
    throw std::runtime_error("Engine executable not found: "+out.executable.u8string());
  vars["ENGINE_DIR_PATH"]=out.executable.parent_path().u8string();
  out.working=e.work.empty()?out.executable.parent_path():enginePath(profile::interpolate(e.work,vars),base);
  if(!std::filesystem::is_directory(out.working))
    throw std::runtime_error("Engine working directory not found: "+out.working.u8string());
  out.arguments=profile::interpolate(e.parameters,vars);
  auto args=profile::parseArguments(out.arguments);
  if(e.mapFile.empty())return out; // Imported/custom engines keep their exact command.
  out.source=enginePath(profile::interpolate(e.mapFile,vars),base);
  std::string root,mod="id1",map;
  for(size_t i=0;i<args.size();i++){
    if(args[i]=="-basedir"||args[i]=="-game"||args[i]=="+map"){
      const auto flag=args[i];if(++i==args.size()||args[i].empty())throw std::runtime_error("Missing value after "+flag);
      if(flag=="-basedir")root=args[i];else if(flag=="-game")mod=args[i];else map=args[i];
    }
    // The automatic map-copy helper targets one active mod. Other engine
    // switches remain available through an unmodified custom launch command.
    else if(args[i]=="-quoth"||args[i]=="-rogue"||args[i]=="-hipnotic")
      throw std::runtime_error("Use an explicit -game directory for automatic Quake map staging, or clear Compiled map to manage deployment yourself.");
  }
  auto safeRelative=[](const std::string& s){auto p=std::filesystem::u8path(s);
    if(p.empty()||p.is_absolute()||s.find_first_of(":\\\r\n")!=s.npos)return false;
    for(auto& part:p)if(part==".."||part==".")return false;return true;};
  if(!safeRelative(mod)||std::filesystem::u8path(mod).has_parent_path())
    throw std::runtime_error("Quake -game must be a mod directory name relative to -basedir.");
  if(map.size()>4&&map.substr(map.size()-4)==".bsp")map.resize(map.size()-4);
  if(!safeRelative(map))throw std::runtime_error("Set +map to a relative map name (without maps/ or .bsp).");
  if(map.rfind("maps/",0)==0)throw std::runtime_error("Use +map name, not +map maps/name.");
  // Quake's console command cannot round-trip whitespace/command separators in map names.
  if(map.find_first_of(" \t;\"")!=map.npos)throw std::runtime_error("Quake map names cannot contain spaces or console separators; rename the map.");
  auto gameRoot=root.empty()?out.working:enginePath(root,out.working);
  if(!std::filesystem::is_directory(gameRoot))throw std::runtime_error("Quake base directory not found: "+gameRoot.u8string());
  if(!std::filesystem::is_directory(gameRoot/"id1"))throw std::runtime_error("Quake base directory must contain id1 game data: "+gameRoot.u8string());
  if(!std::filesystem::is_directory(gameRoot/mod))throw std::runtime_error("Quake mod directory not found: "+(gameRoot/mod).u8string());
  out.target=gameRoot/mod/"maps"/std::filesystem::u8path(map+".bsp");
  if(out.source.extension()!=".bsp")
    throw std::runtime_error("Compiled Quake map not found (expected .bsp): "+out.source.u8string());
  // TrenchBroom profiles commonly copy the BSP into the active game's maps
  // directory and then delete the build output. Launch that exact deployed
  // map when the configured build output is absent; an existing bad output
  // must still fail rather than silently launch an older deployed map.
  if(!std::filesystem::exists(out.source)){
    if(!std::filesystem::is_regular_file(out.target))
      throw std::runtime_error("Compiled Quake map not found: "+out.source.u8string()+" (deployed map also missing: "+out.target.u8string()+")");
    out.source=out.target;
  }
  if(!std::filesystem::is_regular_file(out.source))
    throw std::runtime_error("Compiled Quake map not found (expected .bsp): "+out.source.u8string());
  std::ifstream input(out.source,std::ios::binary);unsigned char header[124]{};
  input.read(reinterpret_cast<char*>(header),sizeof header);
  if(!input)throw std::runtime_error("Compiled map has a truncated BSP header: "+out.source.u8string());
  auto word=[&](int at){return uint32_t(header[at])|uint32_t(header[at+1])<<8|uint32_t(header[at+2])<<16|uint32_t(header[at+3])<<24;};
  const auto magic=word(0);
  if(magic!=29&&magic!=0x32505342&&magic!=0x42535032)
    throw std::runtime_error("VKQuake/Ironwail require a Quake 1 BSP (29/BSP2), not this map format.");
  const auto size=std::filesystem::file_size(out.source);
  for(int i=0;i<15;i++){auto offset=word(4+i*8),length=word(8+i*8);
    if(uint64_t(offset)+length>size)throw std::runtime_error("Compiled map contains an invalid BSP lump.");}
  return out;
}
inline void stageEngineMap(const EngineLaunch& plan) {
  if(plan.source.empty())return;
  std::error_code ec;
  if(std::filesystem::equivalent(plan.source,plan.target,ec))return;
  // Verify the complete input before replacing the engine's last usable map.
  std::filesystem::create_directories(plan.target.parent_path());
  auto copy=[&](const std::filesystem::path& from,const std::filesystem::path& to){
    auto temporary=to;temporary+=".reyond-tmp";
    try{std::filesystem::copy_file(from,temporary,std::filesystem::copy_options::overwrite_existing);
#ifdef _WIN32
      if(!MoveFileExW(temporary.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot install compiled map: "+to.u8string());
#else
      std::filesystem::rename(temporary,to);
#endif
    }catch(...){std::filesystem::remove(temporary,ec);throw;}
  };
  // BSP publication is last; optional lighting/entity sidecars follow the map.
  for(const char* ext:{".lit",".lux",".ent"}){
    auto from=plan.source,to=plan.target;from.replace_extension(ext);to.replace_extension(ext);
    if(std::filesystem::is_regular_file(from))copy(from,to);
    else std::filesystem::remove(to); // Do not retain stale overrides from a previous compile.
  }
  copy(plan.source,plan.target);
}
}
