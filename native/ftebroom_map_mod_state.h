// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Included after the compilation store so the resource UI and imported profiles
// share one per-map list and one game root instead of separate global selections.
static std::string mapModsResourceKey;
static uint64_t mapModsPendingGeneration=0;
static bool mapModsReadLive(std::vector<std::string>& mods) {
  std::string text,version,value;if(!qcQuery(7,0,text))return false;
  std::istringstream input(text);if(!(input>>version>>std::quoted(value))||version!="RBMODS1")return false;
  mods=ftebroom::mapModList(value);return true;
}
static std::filesystem::path mapModsRoot() {
  if(!runGameRoot.empty())return std::filesystem::u8path(runGameRoot).lexically_normal();
  if(gameProfile==2&&!gameMapResourceRoot.empty())return gameMapResourceRoot;
  auto selected=std::filesystem::u8path(gameRuntime().empty()?std::string(baseq3):gameRuntime()).lexically_normal();
  if(!selected.empty()&&selected.filename().empty())selected=selected.parent_path();
  return selected.parent_path();
}
static std::vector<std::string> mapModsDefault() {
  auto selected=std::filesystem::u8path(gameRuntime()).lexically_normal();
  if(!selected.empty()&&selected.filename().empty())selected=selected.parent_path();
  if(selected.empty())return {};
  if(gameProfile!=2){auto name=selected.filename().u8string();return name==runBaseMod?std::vector<std::string>{}:std::vector<std::string>{name};}
  auto root=mapModsRoot();auto relative=std::filesystem::absolute(selected).lexically_normal().lexically_relative(std::filesystem::absolute(root.empty()?std::filesystem::path("."):root).lexically_normal());
  auto name=relative.empty()?selected.u8string():relative.generic_u8string();
  return ftebroom::mapModSame(name,runBaseMod)?std::vector<std::string>{}:std::vector<std::string>{name};
}
static std::vector<std::string> mapModsForDocument() {
  std::vector<std::string> mods;bool live=mapModsReadLive(mods);
  if(runModsPending&&mapModsPendingGeneration!=documentGeneration)runModsPending=false;
  if(runModsPending){
    if(live&&ftebroom::mapModString(mods)==runPendingMods)runModsPending=false;
    else return ftebroom::mapModList(runPendingMods);
  }
  if(live)return mods;
  if(gameProfile==2&&gameMapModsKnown)return gameMapMods;
  return runModsConfigured?runEnabledMods:mapModsDefault();
}
static bool mapModsMount(const std::vector<std::string>& mods) {
  if(gameProfile!=2)return true;
  const auto root=mapModsRoot();auto paths=ftebroom::mapModResourcePaths(root,runBaseMod,mods);
  const auto key=std::to_string(gameProfile)+"|"+root.u8string()+"|"+runBaseMod+"|"+ftebroom::mapModString(mods);
  if(key==mapModsResourceKey&&!gameMapResourcePaths.empty()){gameMapMods=mods;gameMapModsKnown=true;gameMapModsError.clear();return true;}
  // LibreQuake is a complete, separately downloaded replacement for id1.
  if(gameProfile==2&&q1UseLibre&&mods.empty())paths={std::filesystem::u8path(q1Authoring())};
  // GAME_DIR_PATH may intentionally be relative in a compilation profile. FTE's
  // resource bridge takes native absolute paths, without changing that variable.
  for(auto& path:paths)path=std::filesystem::absolute(path).lexically_normal();
  for(size_t i=0;i<paths.size();++i){
    if(i==0&&!mods.empty()&&!std::filesystem::is_directory(paths[i]))continue;
    if(!std::filesystem::is_directory(paths[i]))throw std::runtime_error("Map mod directory not found: "+paths[i].u8string()+". Choose the game directory in Compilation profiles settings or correct the map's Mods list.");
    if(paths[i].u8string().find(';')!=std::string::npos)throw std::runtime_error("A game directory cannot contain ';'.");
  }
  paths.erase(std::remove_if(paths.begin(),paths.end(),[](const auto& path){return !std::filesystem::is_directory(path);}),paths.end());
  if(paths.empty())throw std::runtime_error("No resource directories are available for this map.");
  const auto selected=paths.back().u8string();if(selected.size()>=1024)throw std::runtime_error("The map mod directory path is too long.");
  const std::string oldSelected=q1Mod,oldAuthoring=baseq3,oldRoot=runGameRoot;const auto oldMods=gameMapMods;const auto oldPaths=gameMapResourcePaths;const auto oldResourceRoot=gameMapResourceRoot;const bool oldKnown=gameMapModsKnown,oldLibre=q1UseLibre;
  gameMapMods=mods;gameMapModsKnown=true;gameMapResourcePaths=paths;gameMapResourceRoot=root;
  if(q1UseLibre&&paths.back()!=std::filesystem::absolute(std::filesystem::u8path(libreMod())).lexically_normal())q1UseLibre=false;
  std::snprintf(q1Mod,sizeof q1Mod,"%s",selected.c_str());
  try{
    if(!gameProfileSave()||!mountAssets())throw std::runtime_error(assetError.empty()?"Cannot apply map mod resources.":assetError);
    // Keep the game's root independent of the last mod's parent (nested mod
    // paths are valid in TB). Persist it before the runtime folder changes again.
    if(runGameRoot.empty()){runGameRoot=root.empty()?".":root.u8string();if(!runSaveProfiles())throw std::runtime_error(runError);}
  }catch(const std::exception& error){
    const auto message=std::string(error.what());runGameRoot=oldRoot;gameMapResourceRoot=oldResourceRoot;
    gameMapMods=oldMods;gameMapResourcePaths=oldPaths;gameMapModsKnown=oldKnown;q1UseLibre=oldLibre;
    std::snprintf(q1Mod,sizeof q1Mod,"%s",oldSelected.c_str());std::snprintf(baseq3,sizeof baseq3,"%s",oldAuthoring.c_str());
    gameProfileSave();mountAssets();throw std::runtime_error(message);
  }
  mapModsResourceKey=key;gameMapModsError.clear();return true;
}
static void mapModsTick() {
  if(!ready||gameProfile!=2||assetsLoading||savingDocument||savePakDetached||runActive||!pendingDocumentFile.empty()||!jamPreviewRuntime().empty())return;
  try {runLoad();std::vector<std::string> live;if(!mapModsReadLive(live)&&!gameMapModsKnown)return;mapModsMount(mapModsForDocument());}
  catch(const std::exception& error){gameMapModsError=error.what();}
}
static bool mapModsSet(const std::vector<std::string>& mods) {
  try {
    auto joined=ftebroom::mapModString(mods);for(const auto& name:mods)ftebroom::validateMapMod(name);
    if(gameProfile==2)for(const auto& name:mods)if(!std::filesystem::is_directory(mapModsRoot()/std::filesystem::u8path(name)))throw std::runtime_error("Mod directory not found: "+(mapModsRoot()/std::filesystem::u8path(name)).u8string());
    if(gameProfile==2)mapModsMount(mods);
    runPendingMods=joined;runPendingModsMap=documentFile.u8string();mapModsPendingGeneration=documentGeneration;runModsPending=true;
    command(("ftebroom_compile_mods "+organizationQuote(joined)+"\n").c_str());return true;
  }catch(const std::exception& error){gameMapModsError=error.what();return false;}
}
static void mapModsOpenSource(const std::string& source) {
  if(gameProfile!=2)return;runLoad();runModsPending=false;
  gameMapMods=ftebroom::mapModsFromSource(source);gameMapModsKnown=true;
  try{mapModsMount(gameMapMods);}catch(const std::exception& error){gameMapModsError=error.what();}
}
static std::string mapModsPrepareScene(const std::string& source) {
  if(gameProfile!=2)return source;runLoad();
  // A new document inherits the currently selected map mods. On first startup,
  // before a document has its own state, use the explicitly configured folder.
  return ftebroom::mapSourceWithMods(source,gameMapModsKnown?mapModsForDocument():mapModsDefault());
}
static bool mapModsChooseResources() {
  if(gameProfile!=2)return mountAssets();
  runLoad();const auto old=runWorkspaceJson();const auto previousMods=mapModsForDocument();const auto oldResourceRoot=gameMapResourceRoot;
  try {
    auto selected=std::filesystem::u8path(gameRuntime()).lexically_normal();if(selected.filename().empty())selected=selected.parent_path();
    runGameRoot=selected.parent_path().u8string();gameMapResourceRoot=selected.parent_path();mapModsResourceKey.clear();const auto mods=mapModsDefault();
    mapModsMount(mods);if(!runSaveProfiles())throw std::runtime_error(runError);
    return mapModsSet(mods);
  }catch(const std::exception& error){
    const auto message=std::string(error.what());runReadWorkspace(old);gameMapResourceRoot=oldResourceRoot;mapModsResourceKey.clear();
    try{mapModsMount(previousMods);}catch(...){}runSaveProfiles();
    assetError=gameMapModsError=message;return false;
  }
}

static void mapModsQaTick() {
  if(!cvars->GetFloat("ftebroom_skip_setup")||!cvars->GetFloat("ftebroom_input_qa"))return;
  try{
    static uint64_t sequence=0;nlohmann::json widgets=nlohmann::json::object(),paths=nlohmann::json::array();
    for(const auto& item:fileBrowserTestItems)if(item.first.rfind("map_mod_",0)==0)widgets[item.first]={item.second.x,item.second.y};
    for(const auto& path:gameMapResourcePaths)paths.push_back(path.u8string());
    const auto mods=mapModsForDocument();
    nlohmann::json state{{"sequence",++sequence},{"generation",documentGeneration},{"mods",mods},{"mapMods",ftebroom::mapModString(mods)},{"base",runBaseMod},{"root",mapModsRoot().u8string()},
      {"resourcePaths",paths},{"runtime",gameRuntime()},{"error",gameMapModsError},{"loading",assetsLoading},{"document",documentFile.u8string()},{"widgets",widgets}};
    std::ofstream out(documentPath("native-mods134-live.json"),std::ios::binary);out<<state.dump();
  }catch(...){/* Read-only, opt-in diagnostics must never change editor state. */}
}
