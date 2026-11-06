// SPDX-License-Identifier: GPL-2.0-or-later
#include "third_party/json.hpp"

struct RunImport {
  std::vector<RunProfile> profiles;
  std::vector<std::string> notices;
  std::map<std::string,std::string> missingVariables;
};

static nlohmann::json runImportRead(const std::filesystem::path& path)
{
  if(!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path)>4*1024*1024)
    throw std::runtime_error("Choose a readable TrenchBroom profile file up to 4 MiB.");
  std::ifstream input(path,std::ios::binary);
  if(!input)throw std::runtime_error("Could not read "+path.filename().u8string());
  auto config=nlohmann::json::parse(input,nullptr,true,true);
  const auto& version=config.at("version");
  if(version!=1&&version!=2)throw std::runtime_error("Unsupported TrenchBroom profile version.");
  if(!config.at("profiles").is_array())throw std::runtime_error("Expected a list of profiles.");
  return config;
}

static std::string runImportText(std::string value,std::map<std::string,std::string>& required)
{
  if(value.find_first_of("\r\n")!=value.npos||value.find('\0')!=value.npos)
    throw std::runtime_error("Profile fields must be single-line text.");
  for(size_t start=0;(start=value.find("${",start))!=value.npos;){
    const auto end=value.find('}',start+2);
    if(end==value.npos)throw std::runtime_error("Unclosed TrenchBroom variable.");
    auto name=value.substr(start+2,end-start-2);
    if(name=="GAME_DIR_PATH")name="TB_GAME_ROOT";
    else if(name=="MODS"||name=="MODS[-1]"||name=="MODS[0]")name="TB_ACTIVE_MOD";
    else if(name!="MAP_BASE_NAME"&&name!="MAP_FULL_NAME"&&name!="MAP_DIR_PATH"&&
            name!="CPU_COUNT"&&name!="WORK_DIR_PATH"&&name!="APP_DIR_PATH"){
      if(name.empty()||name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=name.npos)
        throw std::runtime_error("Unsupported TrenchBroom expression: "+name);
      auto existing=runVariables.find(name);
      if(existing==runVariables.end()||existing->second.empty())required.try_emplace(name);
    }
    value.replace(start+2,end-start-2,name);
    start+=name.size()+3;
  }
  return value;
}

static RunImport runReadTrenchBroom(const std::filesystem::path& path)
{
  auto config=runImportRead(path);
  RunImport result;
  nlohmann::json engines=nlohmann::json::array();
  const auto engineFile=path.parent_path()/"GameEngineProfiles.cfg";
  if(std::filesystem::exists(engineFile)){
    try{engines=runImportRead(engineFile).at("profiles");}
    catch(const std::exception& error){result.notices.push_back("GameEngineProfiles.cfg: "+std::string(error.what()));}
  }
  for(const auto& entry:config.at("profiles")){
    std::string name="Unnamed profile";
    try{
      auto savedName=entry.at("name").get<std::string>();
      if(savedName.find_first_of("\r\n")!=savedName.npos||savedName.find('\0')!=savedName.npos)
        throw std::runtime_error("Profile name must be single-line text.");
      if(!savedName.empty())name=std::move(savedName);
      RunProfile profile;
      profile.name=name;
      std::map<std::string,std::string> required;
      auto translate=[&](const std::string& value){return runImportText(value,required);};
      profile.work=translate(entry.value("workdir",std::string{}));
      if(profile.work.empty())profile.work="${MAP_DIR_PATH}";
      const auto& tasks=entry.at("tasks");
      if(!tasks.is_array())throw std::runtime_error("Expected a list of compile steps.");
      std::vector<std::string> notices;
      for(const auto& step:tasks){
        RunTask task;
        task.enabled=step.value("enabled",true);
        const auto type=step.at("type").get<std::string>();
        auto field=[&](const char* key){return translate(step.at(key).get<std::string>());};
        if(type=="export"){
          if(step.value("stripTbProperties",false)||!step.value("stripEntityPattern",std::string{}).empty()||
             step.contains("entityToAdd"))
            throw std::runtime_error("Export options that change entity data require manual setup.");
          task.source=field("target");
        }else if(type=="tool"){
          task.kind=1;
          task.source=field("tool");
          task.destination=field("parameters");
          if(!step.value("treatNonZeroResultCodeAsError",false))task.flags|=RunAllowNonzero;
        }else if(type=="copy"||type=="rename"){
          task.kind=type=="copy"?2:3;
          if(type=="copy")task.flags|=RunCopyToDirectory;
          task.source=field("source");
          task.destination=field("target");
        }else if(type=="delete"){
          task.kind=4;
          task.source=field("target");
        }else if(type=="launchEngine"){
          task.kind=1;
          task.flags=RunLaunchEngine;
          if(!step.value("treatLaunchFailureAsError",false))task.flags|=RunAllowLaunchFailure;
          const auto id=step.at("engineProfileId").get<std::string>();
          auto engine=std::find_if(engines.begin(),engines.end(),[&](const auto& candidate){
            return candidate.is_object()&&candidate.contains("id")&&candidate["id"]==id;
          });
          if(engine!=engines.end()){
            task.source=translate(engine->at("path").get<std::string>());
            task.destination=translate(engine->at("parameters").get<std::string>());
          }
          if(task.source.empty()){
            task.enabled=false;
            notices.push_back("Launch Engine step disabled. Set its executable and arguments, or place the matching GameEngineProfiles.cfg beside the compile file.");
          }
        }else throw std::runtime_error("Unsupported TrenchBroom task: "+type);
        profile.tasks.push_back(std::move(task));
      }
      result.profiles.push_back(std::move(profile));
      result.missingVariables.merge(required);
      for(const auto& message:notices)result.notices.push_back(name+": "+message);
    }catch(const std::exception& error){result.notices.push_back(name+": skipped. "+error.what());}
  }
  return result;
}

static bool runSaveImported(RunImport imported)
{
  auto oldVariables=runVariables;
  const auto oldCount=runProfiles.size();
  for(auto& profile:imported.profiles){
    const auto base=profile.name;
    for(int suffix=1;std::any_of(runProfiles.begin(),runProfiles.end(),[&](const auto& p){return p.name==profile.name;});++suffix)
      profile.name=base+" (TrenchBroom "+std::to_string(suffix)+")";
    runProfiles.push_back(std::move(profile));
  }
  for(const auto& variable:imported.missingVariables){
    auto& value=runVariables[variable.first];
    if(value.empty())value=variable.second;
  }
  if(!runSaveProfiles()){
    runProfiles.resize(oldCount);
    runVariables=std::move(oldVariables);
    return false;
  }
  runSelected=static_cast<int>(oldCount);
  runError.clear();
  return true;
}

static std::string runImportPath,runImportDirectory,runImportMessage;
static std::string runImportStatus;
static std::string runImportListedDirectory;
static std::vector<std::filesystem::path> runImportEntries;
static RunImport runImportPreview;
static bool runImportReviewed=false;

static void runImportOpen()
{
  runImportPreview={};
  runImportReviewed=false;
  runImportMessage.clear();
  runImportStatus.clear();
  runImportListedDirectory.clear();
  if(runImportPath.empty()){
#ifdef _WIN32
    const auto* home=_wgetenv(L"APPDATA");
    const auto games=home?std::filesystem::path(home)/"TrenchBroom/games":std::filesystem::path{};
#else
    const auto* home=std::getenv("HOME");
    const auto games=home?std::filesystem::u8path(home)/".TrenchBroom/games":std::filesystem::path{};
#endif
    std::error_code ignored;
    runImportDirectory=(std::filesystem::is_directory(games,ignored)?games:documentDefaultFolder()).u8string();
  }
  ImGui::OpenPopup("Import TrenchBroom profiles");
}

static void runImportPanel()
{
  if(!ImGui::IsPopupOpen("Import TrenchBroom profiles"))return;
  ImGui::SetNextWindowSize({660*uiScale(),520*uiScale()},ImGuiCond_FirstUseEver);
  bool open=true;
  if(!ImGui::BeginPopupModal("Import TrenchBroom profiles",&open))return;
  ImGui::TextWrapped("Select a local CompilationProfiles.cfg from TrenchBroom. Imported steps are saved without running them.");
  if(runText("Profile file",runImportPath))runImportReviewed=false;
  if(ImGui::CollapsingHeader("Browse files")){
    runText("Folder",runImportDirectory);
    if(ImGui::Button("Up one level")){
      const auto parent=std::filesystem::u8path(runImportDirectory).parent_path();
      if(!parent.empty())runImportDirectory=parent.u8string();
    }
    if(runImportListedDirectory!=runImportDirectory){
      runImportListedDirectory=runImportDirectory;
      runImportEntries.clear();
      std::error_code error;
      for(std::filesystem::directory_iterator it(std::filesystem::u8path(runImportDirectory),error),end;!error&&it!=end;it.increment(error)){
        std::error_code entryError;
        if(it->is_directory(entryError)||it->path().extension()==".cfg")runImportEntries.push_back(it->path());
      }
      std::sort(runImportEntries.begin(),runImportEntries.end());
    }
    ImGui::BeginChild("Profile files",{0,125*uiScale()},ImGuiChildFlags_Borders);
    ImGuiListClipper clip;
    clip.Begin(static_cast<int>(runImportEntries.size()));
    while(clip.Step())for(int index=clip.DisplayStart;index<clip.DisplayEnd;++index){
      const auto& path=runImportEntries[index];
      std::error_code ignored;
      const bool directory=std::filesystem::is_directory(path,ignored);
      const auto label=(directory?"[Folder] ":"")+path.filename().u8string();
      if(ImGui::Selectable(label.c_str(),false,ImGuiSelectableFlags_NoAutoClosePopups)){
        if(directory)runImportDirectory=path.u8string();
        else{runImportPath=path.u8string();runImportReviewed=false;}
      }
    }
    ImGui::EndChild();
  }
  ImGui::BeginDisabled(runImportPath.empty());
  if(ImGui::Button("Review import")){
    runImportReviewed=false;
    runImportMessage.clear();
    try{
      auto path=runImportPath;
      if(path.size()>1&&path.front()=='"'&&path.back()=='"')path=path.substr(1,path.size()-2);
      runImportPreview=runReadTrenchBroom(std::filesystem::u8path(path));
      runImportReviewed=true;
    }catch(const std::exception& error){runImportMessage=error.what();}
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!runImportReviewed||runImportPreview.profiles.empty());
  if(ImGui::Button("Import profiles")){
    if(runSaveImported(runImportPreview)){
      runImportStatus="Imported "+std::to_string(runImportPreview.profiles.size())+" TrenchBroom profile(s).";
      ImGui::CloseCurrentPopup();
    }else runImportMessage=runError;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
  if(!runImportMessage.empty())ImGui::TextWrapped("%s",runImportMessage.c_str());
  if(runImportReviewed){
    ImGui::BeginChild("Profile review",{0,0},ImGuiChildFlags_Borders);
    for(const auto& profile:runImportPreview.profiles)
      ImGui::BulletText("%s (%zu steps)",profile.name.c_str(),profile.tasks.size());
    for(const auto& warning:runImportPreview.notices)ImGui::TextWrapped("%s",warning.c_str());
    if(!runImportPreview.missingVariables.empty()){
      ImGui::TextWrapped("Fill any missing tool paths or variables here, or configure them after import.");
      for(auto& variable:runImportPreview.missingVariables){
        ImGui::PushID(variable.first.c_str());
        runText(variable.first.c_str(),variable.second);
        ImGui::PopID();
      }
    }
    ImGui::EndChild();
  }
  ImGui::EndPopup();
}
