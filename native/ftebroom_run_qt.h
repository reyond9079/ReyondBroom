// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifndef _WIN32
#include <dlfcn.h>
#endif
static bool runQtInitialized=false;
static bool runQtFailed=false;
static std::string runQtLoadError;
static int runQtPendingMode=-1;
static int (*runQtInitialize)(const char* (*)(const char*),const char*);
static void (*runQtShowFunction)(int),(*runQtTickFunction)(),(*runQtShutdownFunction)();
static int (*runQtVisibleFunction)();
static int (*runQtCaptureFunction)(const char*,int,int,int);
static std::string runImportReviewGame;
static nlohmann::json runQtState(){
 using J=nlohmann::json;runLoad();J profiles=J::array(),engines=J::array(),presets=J::array();
 for(const auto& p:runProfiles)profiles.push_back(runProfileToJson(p));for(const auto& p:runBundledProfiles)presets.push_back(runProfileToJson(p));
 for(const auto& e:runEngines)engines.push_back({{"id",e.id},{"name",e.name},{"path",e.path},{"parameters",e.parameters}});
 auto workvars=runWorkdirVariables(),taskvars=workvars;
 auto enabled=runCommonVariables().at("MODS");if(!enabled.empty())enabled.erase(enabled.begin());taskvars["CPU_COUNT"]=std::max(1u,std::thread::hardware_concurrency());taskvars["WORK_DIR_PATH"]="";
 if(runSelected>=0&&runSelected<int(runProfiles.size()))try{taskvars["WORK_DIR_PATH"]=ftebroom::profile::interpolate(runProfiles[runSelected].work,workvars);}catch(...){}
 return {{"profiles",profiles},{"engines",engines},{"presets",presets},{"selected",runSelected},{"game",runStoreGame},{"gameName",gameProfileName()},
  {"revision",runStoreRevision},{"darkTheme",uiTheme!=1&&uiTheme!=3},{"variables",runVariables},{"workdirVariables",workvars},{"taskVariables",taskvars},{"gameRoot",runGameRoot.empty()?workvars["GAME_DIR_PATH"].get<std::string>():runGameRoot},{"baseMod",runBaseMod},{"enabledMods",enabled},
  {"running",runActive},{"runningName",runningProfile.name},{"log",runLog},{"error",runError},{"migration",runMigrationMessage}};
}
static void runLaunchConfiguredEngine(const RunEngineProfile& engine){
 auto oldStrict=runStrict;runStrict=true;
 try{auto args=ftebroom::profile::interpolate(engine.parameters,runCommonVariables());runStartProcess(engine.path,args,std::filesystem::u8path(engine.path).parent_path(),true);runStrict=oldStrict;}
 catch(...){runStrict=oldStrict;throw;}
}
static const char* runQtRequest(const char* encoded){
 static std::string result;
 try{
  using J=nlohmann::json;auto q=J::parse(encoded);auto action=q.at("action").get<std::string>();runLoad();J response={{"ok",true}};
  if(action=="state")response=runQtState();
  else if(action=="profiles"){
   auto incoming=q.at("profiles");if(!incoming.is_array()||incoming.size()>4096)throw std::runtime_error("Invalid profile list.");std::vector<RunProfile> next;
   for(const auto& p:incoming)next.push_back(runProfileFromJson(p));auto old=runProfiles;auto selected=runSelected;runProfiles=std::move(next);runSelected=q.value("selected",-1);
   if(!runSaveProfiles()){runProfiles=std::move(old);runSelected=selected;throw std::runtime_error(runError);}
  }else if(action=="select"){auto n=q.at("selected").get<int>();runSelected=n>=0&&n<int(runProfiles.size())?n:-1;}
  else if(action=="settings"){
   auto nextMods=q.at("enabledMods").get<std::vector<std::string>>();
   std::string joined;for(const auto& mod:nextMods){if(mod.find_first_of(";\r\n")!=std::string::npos)throw std::runtime_error("A mod name must not contain a separator or newline.");if(!joined.empty())joined+=';';joined+=mod;}
   std::vector<RunEngineProfile> nextEngines;std::set<std::string> ids;
   for(const auto& e:q.at("engines")){RunEngineProfile p{e.at("id"),e.at("name"),e.at("path"),e.at("parameters")};if(p.id.empty()||!ids.insert(p.id).second)throw std::runtime_error("Engine profile IDs must be unique.");nextEngines.push_back(std::move(p));}
   auto nextRoot=q.at("gameRoot").get<std::string>(),nextBase=q.at("baseMod").get<std::string>();auto nextVariables=q.at("variables").get<std::map<std::string,std::string>>();
   auto old=runWorkspaceJson();auto previousMods=runCommonVariables().at("MODS");if(!previousMods.empty())previousMods.erase(previousMods.begin());
   runGameRoot=std::move(nextRoot);runBaseMod=std::move(nextBase);runEnabledMods=std::move(nextMods);runModsConfigured=true;runVariables=std::move(nextVariables);runEngines=std::move(nextEngines);
   if(!runSaveProfiles()){runReadWorkspace(old);throw std::runtime_error(runError);}
   if(previousMods!=nlohmann::json(runEnabledMods)){runPendingMods=joined;runPendingModsMap=documentFile.u8string();runModsPending=true;command(("ftebroom_compile_mods "+organizationQuote(joined)+"\n").c_str());}
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
  }else if(action=="launchEngine"){
   auto e=q.at("engine");RunEngineProfile engine{e.at("id"),e.at("name"),e.at("path"),e.at("parameters")};
   for(auto& saved:runEngines)if(saved.id==engine.id)saved.parameters=engine.parameters;runSaveProfiles();runLaunchConfiguredEngine(engine);
  }else throw std::runtime_error("Unknown Compile action: "+action);
  response["revision"]=runStoreRevision;result=response.dump();
 }catch(const std::exception& e){result=nlohmann::json{{"error",e.what()}}.dump();}
 return result.c_str();
}
static bool runQtLoad(){
 if(runQtInitialized)return true;if(runQtFailed)return false;runLoad();
 auto root=documentPath("ftebroom.cfg").parent_path().parent_path();
#ifdef _WIN32
 auto path=root/"tools/compile-ui/reyond-compile-ui.dll";auto library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
 auto symbol=[&](const char* n){return library?reinterpret_cast<void*>(GetProcAddress(library,n)):nullptr;};
#else
 auto path=root/"tools/compile-ui/libreyond-compile-ui.so";auto library=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
 auto symbol=[&](const char* n){return library?dlsym(library,n):nullptr;};
#endif
 runQtCaptureFunction=reinterpret_cast<decltype(runQtCaptureFunction)>(symbol("reyondCompileCapture"));
 runQtInitialize=reinterpret_cast<decltype(runQtInitialize)>(symbol("reyondCompileInitialize"));runQtShowFunction=reinterpret_cast<decltype(runQtShowFunction)>(symbol("reyondCompileShow"));runQtTickFunction=reinterpret_cast<decltype(runQtTickFunction)>(symbol("reyondCompileTick"));runQtVisibleFunction=reinterpret_cast<decltype(runQtVisibleFunction)>(symbol("reyondCompileVisible"));runQtShutdownFunction=reinterpret_cast<decltype(runQtShutdownFunction)>(symbol("reyondCompileShutdown"));
 if(!runQtInitialize||!runQtShowFunction||!runQtTickFunction||!runQtVisibleFunction||!runQtShutdownFunction){runQtFailed=true;runError="The native Compile UI component is missing or incompatible. Install the complete editor package.";
#ifndef _WIN32
 if(const char* error=dlerror())core->Print((std::string("Compile UI: ")+error+"\n").c_str());
#endif
 runQtLoadError=runError;return false;}
 runQtInitialized=runQtInitialize(runQtRequest,documentPath("ui/compilation").u8string().c_str())!=0;
 if(!runQtInitialized){runQtFailed=true;runError="Cannot initialize the native Compile UI.";}return runQtInitialized;
}
static void runQtShow(int mode){runQtPendingMode=mode;}
static void runOpenSettings(){runQtShow(1);}
static void runQtPump(){
 if(runQtPendingMode>=0){int mode=runQtPendingMode;runQtPendingMode=-1;if(runQtLoad())runQtShowFunction(mode);}
 if(runQtInitialized)runQtTickFunction();
}
static void runQtPanel(){
 if(runWindow&&runQtLoad()&&!runQtVisibleFunction())runQtShowFunction(0);
 if(runWindow&&runQtFailed){ImGui::SetNextWindowSize({560,150},ImGuiCond_FirstUseEver);if(ImGui::Begin("Compile",&runWindow))ImGui::TextWrapped("%s",runQtLoadError.empty()?"Install the complete native Compile UI component.":runQtLoadError.c_str());ImGui::End();}
}
static void runQtShutdown(){if(runQtInitialized){runQtShutdownFunction();runQtInitialized=false;}}
