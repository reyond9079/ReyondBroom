// SPDX-License-Identifier: GPL-2.0-or-later
#include "ftebroom_run_import_data.h"

static RunImport runReadTrenchBroom(const std::filesystem::path& path)
{
  return runReadTrenchBroom(path,runVariables);
}

static bool runSaveImported(RunImport imported)
{
  auto oldVariables=runVariables;
  const auto oldCount=runProfiles.size();
  const auto oldEngines=runEngines;
  for(auto engine:imported.engines){
    const auto id=engine.id;int suffix=1;
    while(std::any_of(runEngines.begin(),runEngines.end(),[&](const auto& e){return e.id==engine.id;}))engine.id=id+"-import-"+std::to_string(suffix++);
    if(id!=engine.id)for(auto& p:imported.profiles)for(auto& t:p.tasks)if(t.engineProfileId==id)t.engineProfileId=engine.id;
    runEngines.push_back(std::move(engine));
  }
  for(auto& profile:imported.profiles)runProfiles.push_back(std::move(profile));
  for(const auto& variable:imported.missingVariables){
    auto& value=runVariables[variable.first];
    if(value.empty())value=variable.second;
  }
  if(!runSaveProfiles()){
    runProfiles.resize(oldCount);runEngines=oldEngines;
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
