// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
static std::string runImportReviewGame;
static nlohmann::json runUiState(){
 using J=nlohmann::json;runLoad();J profiles=J::array(),engines=J::array(),presets=J::array();
 for(const auto& p:runProfiles)profiles.push_back(runProfileToJson(p));for(const auto& p:runBundledProfiles)presets.push_back(runProfileToJson(p));
 for(const auto& e:runEngines)engines.push_back(runEngineToJson(e));
 auto workvars=runWorkdirVariables(),taskvars=workvars;
 auto enabled=runCommonVariables().at("MODS");if(!enabled.empty())enabled.erase(enabled.begin());taskvars["CPU_COUNT"]=std::max(1u,std::thread::hardware_concurrency());taskvars["WORK_DIR_PATH"]="";
 if(runSelected>=0&&runSelected<int(runProfiles.size()))try{taskvars["WORK_DIR_PATH"]=ftebroom::profile::interpolate(runProfiles[runSelected].work,workvars);}catch(...){}
 return {{"profiles",profiles},{"engines",engines},{"presets",presets},{"selected",runSelected},{"game",runStoreGame},{"gameName",gameProfileName()},
  {"revision",runStoreRevision},{"darkTheme",uiTheme!=1&&uiTheme!=3},{"variables",runVariables},{"workdirVariables",workvars},{"taskVariables",taskvars},{"gameRoot",runGameRoot.empty()?workvars["GAME_DIR_PATH"].get<std::string>():runGameRoot},{"baseMod",runBaseMod},{"enabledMods",enabled},
  {"running",runActive},{"runningName",runningProfile.name},{"log",runLog},{"error",runError},{"migration",runMigrationMessage}};
}
static void runLaunchConfiguredEngine(const RunEngineProfile& engine){
 if(documentFile.empty())throw std::runtime_error("Save the map before launching an engine.");
 auto oldStrict=runStrict;runStrict=true;
 try{if(gameProfile==2&&!gameMapModsError.empty())throw std::runtime_error(gameMapModsError);auto vars=runWorkdirVariables();
  if(runSelected>=0&&runSelected<int(runProfiles.size()))vars["WORK_DIR_PATH"]=ftebroom::profile::interpolate(runProfiles[runSelected].work,vars);
  else vars["WORK_DIR_PATH"]=vars["MAP_DIR_PATH"];
  auto plan=ftebroom::engineLaunchPlan(engine,vars,runApplicationDirectory());
  ftebroom::stageEngineMap(plan);runStartProcess(plan.executable.u8string(),plan.arguments,plan.working,true);
  runAppend("#### Engine process started: "+plan.executable.u8string()+" "+plan.arguments+"\n");runStrict=oldStrict;}
 catch(...){runStrict=oldStrict;throw;}
}
static nlohmann::json runUiExecute(const nlohmann::json& q){
 try{
  using J=nlohmann::json;auto action=q.at("action").get<std::string>();runLoad();J response={{"ok",true}};
  if(action=="state")response=runUiState();
  else if(action=="profiles"){
   auto incoming=q.at("profiles");if(!incoming.is_array()||incoming.size()>4096)throw std::runtime_error("Invalid profile list.");std::vector<RunProfile> next;
   for(const auto& p:incoming)next.push_back(runProfileFromJson(p));auto old=runProfiles;auto selected=runSelected;runProfiles=std::move(next);runSelected=q.value("selected",-1);
   if(!runSaveProfiles()){runProfiles=std::move(old);runSelected=selected;throw std::runtime_error(runError);}
  }else if(action=="select"){auto n=q.at("selected").get<int>();runSelected=n>=0&&n<int(runProfiles.size())?n:-1;}
  else if(action=="settings"){
   auto nextMods=q.at("enabledMods").get<std::vector<std::string>>();
   const auto joined=ftebroom::mapModString(nextMods);
   std::vector<RunEngineProfile> nextEngines;std::set<std::string> ids;
   for(const auto& e:q.at("engines")){auto p=runEngineFromJson(e);if(p.id.empty()||!ids.insert(p.id).second)throw std::runtime_error("Engine profile IDs must be unique.");nextEngines.push_back(std::move(p));}
   auto nextRoot=q.at("gameRoot").get<std::string>(),nextBase=q.at("baseMod").get<std::string>();auto nextVariables=q.at("variables").get<std::map<std::string,std::string>>();
   auto old=runWorkspaceJson();auto previousMods=runCommonVariables().at("MODS");if(!previousMods.empty())previousMods.erase(previousMods.begin());
   runGameRoot=std::move(nextRoot);runBaseMod=std::move(nextBase);runEnabledMods=std::move(nextMods);runModsConfigured=true;runVariables=std::move(nextVariables);runEngines=std::move(nextEngines);
   try {
    if(gameProfile==2)mapModsMount(runEnabledMods);
    if(!runSaveProfiles())throw std::runtime_error(runError);
   }catch(const std::exception& error){
    const auto message=std::string(error.what());runReadWorkspace(old);runSaveProfiles();
    if(gameProfile==2)try{mapModsMount(previousMods.get<std::vector<std::string>>());}catch(...){}
    throw std::runtime_error(message);
   }
   if(previousMods!=nlohmann::json(runEnabledMods)){if(!mapModsSet(runEnabledMods))throw std::runtime_error(gameMapModsError);}else mapModsTick();
  }else if(action=="run"||action=="test"){
   int n=q.at("selected");if(n<0||n>=int(runProfiles.size()))throw std::runtime_error("Select a compilation profile.");
   if(runActive||runExclusive)throw std::runtime_error("A compilation is already running.");
   if(!runSaveProfiles())throw std::runtime_error(runError);runBegin(runProfiles[n],action=="test");if(!runError.empty())throw std::runtime_error(runError);
  }else if(action=="exitConfirmed"){if(runActive)runStop();runWindow=false;runSaveProfiles();documentRequest("exit");}
  else if(action=="stop"){if(runActive)runStop();}
  else if(action=="closed"){runWindow=false;runSaveProfiles();}
  else if(action=="clearLog"){runLog.clear();}
  else if(action=="reviewImport"){
   runImportReviewed=false;auto path=q.at("path").get<std::string>();if(path.size()>1&&path.front()=='"'&&path.back()=='"')path=path.substr(1,path.size()-2);
   runImportPreview=runReadTrenchBroom(std::filesystem::u8path(path));runImportReviewed=true;runImportReviewGame=runStoreGame;
   auto p=J::array();for(const auto& profile:runImportPreview.profiles)p.push_back(runProfileToJson(profile));response={{"profiles",p},{"notices",runImportPreview.notices},{"missingVariables",runImportPreview.missingVariables}};
  }else if(action=="import"){
   if(!runImportReviewed||runImportReviewGame!=runStoreGame)throw std::runtime_error("Review the import for the active game first.");
   if(q.contains("variables"))for(auto it=q["variables"].begin();it!=q["variables"].end();++it)runImportPreview.missingVariables[it.key()]=it.value().get<std::string>();
   if(!runSaveImported(runImportPreview))throw std::runtime_error(runError);runImportReviewed=false;
  }else if(action=="engineProfiles"){
   std::vector<RunEngineProfile> next;std::set<std::string> ids;
   for(const auto& e:q.at("engines")){auto p=runEngineFromJson(e);if(p.id.empty()||!ids.insert(p.id).second)throw std::runtime_error("Engine profile IDs must be unique.");next.push_back(std::move(p));}
   auto old=runEngines;runEngines=std::move(next);if(!runSaveProfiles()){runEngines=std::move(old);throw std::runtime_error(runError);}
  }else if(action=="engineParameters"){
   for(const auto& e:q.at("engines"))for(auto& saved:runEngines)if(saved.id==e.at("id"))saved.parameters=e.at("parameters").get<std::string>();
   if(!runSaveProfiles())throw std::runtime_error(runError);
  }else if(action=="launchEngine"){
   auto e=q.at("engine");auto engine=runEngineFromJson(e);
   for(auto& saved:runEngines)if(saved.id==engine.id)saved.parameters=engine.parameters;runSaveProfiles();runLaunchConfiguredEngine(engine);
  }else throw std::runtime_error("Unknown Compile action: "+action);
  response["revision"]=runStoreRevision;return response;
 }catch(const std::exception& e){runError=e.what();return nlohmann::json{{"error",e.what()}};}

}
