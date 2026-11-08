// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
static std::vector<RunEngineProfile> runEngines;
static std::vector<RunProfile> runBundledProfiles;
static std::map<std::string,std::string> runBundledVariables;
static nlohmann::json runStore=nlohmann::json::object();
static std::string runStoreGame,runGameRoot,runBaseMod;
static std::vector<std::string> runEnabledMods;
static std::string runMigrationMessage;
static bool runModsConfigured=false,runModsPending=false;
static std::string runPendingMods,runPendingModsMap;
static bool runStoreInitialized=false;
static uint64_t runStoreRevision=0;
static nlohmann::json runWorkspaceJson(){
 using J=nlohmann::json;J p=J::array(),e=J::array();for(const auto& profile:runProfiles)p.push_back(runProfileToJson(profile));
 for(const auto& engine:runEngines)e.push_back(runEngineToJson(engine));
 return {{"profiles",p},{"selected",runSelected},{"engines",e},{"variables",runVariables},{"gameRoot",runGameRoot},{"baseMod",runBaseMod},{"enabledMods",runEnabledMods},{"modsConfigured",runModsConfigured}};
}
static void runReadWorkspace(const nlohmann::json& j){
 runProfiles.clear();runEngines.clear();runVariables.clear();
 const std::vector<std::string> tools=gameProfile==2?std::vector<std::string>{"qbsp","vis","light"}:gameProfile==3?std::vector<std::string>{"q2bsp","q2vis","q2light"}:gameProfile==4?std::vector<std::string>{"hlcsg","hlbsp","hlvis","hlrad"}:std::vector<std::string>{"q3map2","bspc"};
 for(const auto& name:tools){auto i=runBundledVariables.find(name);runVariables[name]=i==runBundledVariables.end()?"":i->second;}
 runGameRoot.clear();runEnabledMods.clear();
 runBaseMod=gameProfile==2?"id1":gameProfile==3?"baseq2":gameProfile==4?"valve":"baseq3";
 for(const auto& p:j.value("profiles",nlohmann::json::array()))runProfiles.push_back(runProfileFromJson(p));
 for(const auto& e:j.value("engines",nlohmann::json::array()))runEngines.push_back(runEngineFromJson(e));
 if(j.contains("variables"))for(auto it=j["variables"].begin();it!=j["variables"].end();++it)runVariables[it.key()]=it.value().get<std::string>();
 runGameRoot=j.value("gameRoot",std::string{});runBaseMod=j.value("baseMod",runBaseMod);runEnabledMods=j.value("enabledMods",std::vector<std::string>{});runModsConfigured=j.value("modsConfigured",false);
 runSelected=runProfiles.empty()?-1:0;
 auto selected=j.find("selected");
 if(selected!=j.end()&&selected->is_number_integer()&&*selected>=0&&*selected<runProfiles.size())runSelected=selected->get<int>();
}
static bool runWriteStore(){
 auto path=documentPath("compilation-profiles.json"),temporary=path;temporary+=".tmp";
 try{
  runStore["version"]=1;runStore["games"][runStoreGame]=runWorkspaceJson();
  std::ofstream file(temporary,std::ios::binary);file<<runStore.dump(2)<<'\n';file.close();if(!file)throw std::runtime_error("Cannot write compilation profiles.");
#ifdef _WIN32
  if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace compilation profiles.");
#else
  std::filesystem::rename(temporary,path);
#endif
  ++runStoreRevision;return true;
 }catch(const std::exception& e){runError=e.what();return false;}
}
static void runLoad(){
 const std::string game=gameProfileKey(gameProfile);
 if(!runStoreInitialized){
  runLoadLegacy();runBundledProfiles=runProfiles;runBundledVariables=runVariables;
  auto path=documentPath("compilation-profiles.json");
  try{
   if(std::filesystem::exists(path)){
    std::ifstream input(path);input>>runStore;if(runStore.at("version")!=1||!runStore.at("games").is_object())throw std::runtime_error("Unsupported compilation profile store.");
   }else{
    runStore={{"version",1},{"games",nlohmann::json::object()}};
    auto legacy=documentPath("ftebroom-run-profiles.txt");
    if(std::filesystem::exists(legacy)){
     auto backup=legacy;backup+=".legacy.bak";if(!std::filesystem::exists(backup))std::filesystem::copy_file(legacy,backup);
     // Read only records actually saved by the user, not the preset factory's additions.
     std::ifstream input(legacy);std::string line;std::vector<RunProfile> saved;auto legacyVariables=runBundledVariables;
     while(std::getline(input,line)){std::istringstream row(line);std::string type;row>>type;
      if(type=="profile"){RunProfile p;if(row>>std::quoted(p.name)>>p.launch>>std::quoted(p.work))saved.push_back(std::move(p));}
      else if(type=="task"&&!saved.empty()){RunTask t;int enabled=1;if(row>>t.kind>>std::quoted(t.source)>>std::quoted(t.destination)){row>>enabled>>t.flags;t.enabled=enabled!=0;saved.back().tasks.push_back(std::move(t));}}
      else if(type=="variable"){std::string name,value;if(row>>std::quoted(name)>>std::quoted(value))legacyVariables[name]=value;}
     }
     if(saved.empty())runStore["games"][game]={{"profiles",nlohmann::json::array()},{"variables",legacyVariables}};
     for(const auto& p:saved){auto key=p.name.rfind("Q1:",0)==0?"quake1":p.name.rfind("Q2:",0)==0?"quake2":p.name.rfind("HL:",0)==0?"halflife":p.name.rfind("Q3:",0)==0?"quake3":game;
      auto& w=runStore["games"][key];if(!w.is_object())w={{"profiles",nlohmann::json::array()},{"variables",legacyVariables}};w["profiles"].push_back(runProfileToJson(p));}
     runMigrationMessage="Legacy profiles migrated. Unscoped profiles belong to the game active during migration; the original and .legacy.bak are preserved.";
    }
   }
   runStoreInitialized=true;
  }catch(const std::exception& e){runError="Cannot load compilation profiles: "+std::string(e.what());return;}
 }
 if(runStoreGame!=game){
  if(!runStoreGame.empty()){runStore["games"][runStoreGame]=runWorkspaceJson();if(!runWriteStore())return;}
  runStoreGame=game;
  try{auto i=runStore["games"].find(game);runReadWorkspace(i==runStore["games"].end()?nlohmann::json::object():*i);}
  catch(const std::exception& e){runProfiles.clear();runEngines.clear();runError=e.what();}
 }
}
static bool runSaveProfiles(){runLoad();return runStoreInitialized&&runWriteStore();}
static nlohmann::json runCommonVariables(){
 using J=nlohmann::json;J vars=runVariables;
 auto runtime=gameRuntime();if(runtime.empty())runtime=baseq3;
 auto mod=std::filesystem::u8path(runtime).lexically_normal();if(!mod.empty()&&mod.filename().empty())mod=mod.parent_path();
 auto root=runGameRoot.empty()?(gameProfile==0&&!q3Root.empty()?q3Root:mod.parent_path().u8string()):runGameRoot;
 auto mods=J::array();mods.push_back(runBaseMod);
 std::string serialized,version,mapMods;
 bool live=false;if(qcQuery(7,0,serialized)){std::istringstream stream(serialized);live=bool(stream>>version>>std::quoted(mapMods))&&version=="RBMODS1";}
 if(runModsPending&&runPendingModsMap==documentFile.u8string()){
  if(live&&mapMods==runPendingMods)runModsPending=false;else{live=true;mapMods=runPendingMods;}
 }
 if(live){for(const auto& name:ftebroom::profile::splitMods(mapMods))mods.push_back(name);}
 else if(!runModsConfigured){if(!mod.filename().empty()&&mod.filename().u8string()!=runBaseMod)mods.push_back(mod.filename().u8string());}
 else for(const auto& name:runEnabledMods)mods.push_back(name);
 vars["MAP_BASE_NAME"]=documentFile.stem().u8string();vars["GAME_DIR_PATH"]=root;vars["MODS"]=mods;
 return vars;
}
static std::filesystem::path runApplicationDirectory(){
#ifdef _WIN32
 std::vector<wchar_t> path(32768);auto n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));if(n&&n<path.size())return std::filesystem::path(std::wstring(path.data(),n)).parent_path();
#else
 std::error_code error;auto path=std::filesystem::read_symlink("/proc/self/exe",error);if(!error)return path.parent_path();
#endif
 throw std::runtime_error("Cannot determine the running application directory.");
}
static nlohmann::json runWorkdirVariables(){auto vars=runCommonVariables();vars["MAP_FULL_NAME"]=documentFile.filename().u8string();vars["MAP_DIR_PATH"]=ftebroom::pak::diskFolder(documentFile.parent_path()).u8string();vars["APP_DIR_PATH"]=runApplicationDirectory().u8string();vars.erase("CPU_COUNT");vars.erase("WORK_DIR_PATH");return vars;}
