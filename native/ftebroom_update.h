// SPDX-License-Identifier: GPL-2.0-or-later
#include "ftebroom_version.h"
static const std::string& editorVersion(){
  static const auto version=ftebroom::readEditorVersion(documentPath("ftebroom.cfg").parent_path().parent_path()/"build-info.txt");
  return version;
}
static bool updateStarted=false,updateCommitted=false;
#ifdef _WIN32
static HANDLE updateProcess=nullptr;
#else
static pid_t updateProcess=-1;
#endif
static std::string updateMessage,updateStage="checking",updateAvailable,updateSession;
static uint64_t updateReceived=0,updateTotal=0;
static bool welcomeQaEnabled(){return cvars->GetFloat("ftebroom_skip_setup")&&cvars->GetFloat("ftebroom_welcome_qa");}
static std::map<std::string,nlohmann::json> welcomeQaItems;
static void welcomeQaItem(const char* id){
  if(!welcomeQaEnabled())return;auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
  welcomeQaItems[id]={{"x",a.x},{"y",a.y},{"width",b.x-a.x},{"height",b.y-a.y},{"frame",ImGui::GetFrameCount()}};
}
static void welcomeQaWindow(const char* id){
  if(!welcomeQaEnabled())return;auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();
  welcomeQaItems[id]={{"x",p.x},{"y",p.y},{"width",s.x},{"height",s.y},{"frame",ImGui::GetFrameCount()}};
}
static void welcomeQaPublish(){
  if(!welcomeQaEnabled())return;
  try {
    nlohmann::json widgets=nlohmann::json::object();for(const auto& item:welcomeQaItems)if(item.second["frame"]==ImGui::GetFrameCount())widgets[item.first]=item.second;
    auto value=nlohmann::json{{"widgets",widgets},{"globalWidgets",runUiGlobalRects},{"display",{ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y}},
      {"windowOpen",recentWindow},{"welcomeStage",recentStage==WelcomeStage::Updates?"updates":recentStage==WelcomeStage::Recovery?"recovery":"recent"},
      {"stage",updateStage},{"committed",updateCommitted},{"currentVersion",editorVersion()},{"availableVersion",updateAvailable},{"session",updateSession},
      {"received",updateReceived},{"total",updateTotal},{"message",updateMessage},{"recoveryPending",recoveryRequest},{"recoveryOpening",recoveryOpening},
      {"dirty",bool(cvars->GetFloat("ftebroom_dirty"))},{"pendingDocument",documentNext},{"saveChangesOpen",widgets.contains("save/cancel")},
      {"mapFilesOpen",ImGui::IsPopupOpen("Map files",ImGuiPopupFlags_AnyPopupId)},{"legacyRecoveryPopup",ImGui::IsPopupOpen("Recover autosave?",ImGuiPopupFlags_AnyPopupId)},
      {"gameProfile",gameProfile},{"resourceError",assetError},{"resourcesOpen",widgets.contains("resources/window")},
      {"resourcesLoading",assetsLoading},{"resourceFolder",baseq3},{"q2Mod",q2Mod},
      {"folder",fileBrowserFolder.u8string()},{"document",documentFile.u8string()}};
    auto path=documentPath("welcome-ui-live.json");std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<value.dump();
  }catch(...){} // Read-only telemetry must never interrupt the editor.
}
static bool updateRunning(){
#ifdef _WIN32
  if(!updateProcess)return false;DWORD code=0;
  if(GetExitCodeProcess(updateProcess,&code)&&code==STILL_ACTIVE)return true;
  CloseHandle(updateProcess);updateProcess=nullptr;
#else
  if(updateProcess<=0)return false;
  if(waitpid(updateProcess,nullptr,WNOHANG)==0)return true;
  updateProcess=-1;
#endif
  return false;
}
static bool updateStart(const std::string& mode="check"){
  if(updateRunning())return false;
  try {
    auto root=settingsPath().parent_path();
    std::filesystem::create_directories(root/"updates");
    auto target=mode=="prepare"?updateAvailable:std::string();
    updateSession=std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    updateStage=mode=="prepare"?"downloading":"checking";updateReceived=updateTotal=0;
    updateMessage=mode=="prepare"?"Downloading update...":"Checking GitHub releases...";
#ifdef _WIN32
    auto script=documentPath("update-ftebroom.ps1");
    if(!std::filesystem::is_regular_file(script))throw std::runtime_error("Updater script is missing.");
    auto wide=[](const std::string& s){return std::wstring(s.begin(),s.end());};
    auto args=L"powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File \""+script.wstring()+L"\" -Root \""+root.wstring()+L"\" -CurrentVersion "+wide(editorVersion())+L" -ParentId "+std::to_wstring(GetCurrentProcessId())+L" -Mode "+wide(mode)+L" -Session "+wide(updateSession);
    if(!target.empty())args+=L" -TargetVersion "+wide(target);
    STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
    if(!CreateProcessW(nullptr,args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))throw std::runtime_error("Cannot start update manager.");
    CloseHandle(pi.hThread);updateProcess=pi.hProcess;
#else
    auto script=documentPath("update-ftebroom.py");
    if(!std::filesystem::is_regular_file(script))throw std::runtime_error("Updater script is missing.");
    std::vector<std::string> args={"python3",script.u8string(),"--root",root.u8string(),"--current-version",editorVersion(),"--parent-id",std::to_string(getpid()),"--mode",mode,"--session",updateSession};
    if(!target.empty()){args.push_back("--target-version");args.push_back(target);}
    std::vector<char*> argv;for(auto& arg:args)argv.push_back(arg.data());argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    auto log=(root/"updates/updater.log").u8string();posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,log.c_str(),O_CREAT|O_WRONLY|O_APPEND,0600);
    posix_spawn_file_actions_adddup2(&actions,STDOUT_FILENO,STDERR_FILENO);
    int result=posix_spawnp(&updateProcess,"python3",&actions,nullptr,argv.data(),environ);posix_spawn_file_actions_destroy(&actions);
    if(result){updateProcess=-1;throw std::runtime_error("Cannot start update manager. Install Python 3.");}
#endif
    updateStarted=true;return true;
  } catch(const std::exception& e){updateStage="error";updateMessage=e.what();return false;}
}
static bool welcomeUpdateCommitted(){return updateCommitted;}
static bool updateDiscardRestart(){
  try {
    auto request=settingsPath().parent_path()/"updates/restart.request";
    std::ifstream input(request);std::string session;std::getline(input,session);input.close();
    if(!updateSession.empty()&&session==updateSession)std::filesystem::remove(request);
    return true;
  }catch(const std::exception& e){documentError=updateMessage=e.what();return false;} // Never remove another editor session's request.
}
static bool updateCommitRestart(){
  std::filesystem::path temporary;
  try {
    if(!updateCommitted||updateStage!="ready"||updateSession.empty())throw std::runtime_error("The update must finish before restarting the editor.");
    auto request=settingsPath().parent_path()/"updates/restart.request";
    temporary=request.parent_path()/("restart-"+updateSession+".tmp");
    std::ofstream out(temporary,std::ios::trunc);out<<updateSession<<'\n';out.close();
    if(!out)throw std::runtime_error("Cannot request an editor restart.");
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),request.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot request an editor restart.");
#else
    std::filesystem::rename(temporary,request);
#endif
    return true;
  }catch(const std::exception& e){std::error_code error;if(!temporary.empty())std::filesystem::remove(temporary,error);updateDiscardRestart();documentError=updateMessage=e.what();return false;}
}
static bool updateApplyStatus(const nlohmann::json& value){
  try {
    if(updateSession.empty()||value.at("session").get<std::string>()!=updateSession||value.at("current_version").get<std::string>()!=editorVersion())return false;
    auto stage=value.at("stage").get<std::string>(),message=value.at("message").get<std::string>(),available=value.at("available_version").get<std::string>();
    if(stage!="checking"&&stage!="current"&&stage!="available"&&stage!="downloading"&&stage!="verifying"&&stage!="ready"&&stage!="installing"&&stage!="installed"&&stage!="error")return false;
    if(message.size()>4096||(!available.empty()&&!std::regex_match(available,std::regex(R"([0-9]+\.[0-9]+\.[0-9]+(-alpha\.[0-9]+)?)"))))return false;
    if(!value.at("received").is_number_integer()||!value.at("total").is_number_integer())return false;
    auto received=value.at("received").get<int64_t>(),total=value.at("total").get<int64_t>();
    if(received<0||total<0||received>total||total>512*1024*1024)return false;
    if((stage=="available"||stage=="ready")&&(available.empty()||total==0))return false;
    if(stage=="ready"&&(!updateCommitted||received!=total))return false;
    // Validate the entire record before changing UI state; foreign or truncated
    // records can never turn a download into an apparent completed update.
    updateMessage=message;updateStage=stage;
    if(!available.empty()||!updateCommitted)updateAvailable=available;
    updateReceived=uint64_t(received);updateTotal=uint64_t(total);return true;
  }catch(...){return false;}
}
static void updateTick(){
  if(!ready||(cvars->GetFloat("ftebroom_skip_setup")&&!welcomeQaEnabled())||firstSetupStep>=0)return;
  if(!updateStarted){welcomeStartup();updateStarted=true;updateStart();}
  static quintptr_t last=0;auto now=core->GetMilliseconds();if(now-last<100)return;last=now;
  try {
    std::ifstream input(settingsPath().parent_path()/"updates/status.json");
    if(input){std::string bytes((std::istreambuf_iterator<char>(input)),{});if(bytes.size()<16384)updateApplyStatus(nlohmann::json::parse(bytes));}
  } catch(...){} // Atomic status updates may briefly be unavailable on Windows.
  if(!updateRunning()&&(updateStage=="checking"||updateStage=="downloading"||updateStage=="verifying")){
    updateStage="error";updateMessage="The update manager stopped. Check updates/updater.log and try again.";
  }
}
static void updateVersionCard(){
  ImGui::Spacing();ImGui::BeginChild("update-versions",{0,ImGui::GetFrameHeightWithSpacing()*2.7f},ImGuiChildFlags_Borders);
  ImGui::TextDisabled("YOUR VERSION");ImGui::TextUnformatted(editorVersion().c_str());
  if(!updateAvailable.empty()){ImGui::SameLine();ImGui::TextColored(ImVec4(.42f,.78f,.58f,1.f),"  ->  %s",updateAvailable.c_str());}
  ImGui::EndChild();ImGui::Spacing();
}
static bool welcomeUpdateContent(){
  ImGui::TextUnformatted(updateCommitted?"Update ReyondBroom":"Check for updates");updateVersionCard();
  ImGui::TextWrapped("%s",updateMessage.c_str());ImGui::Spacing();
  if(updateCommitted){
    const float progress=updateStage=="ready"?1.f:updateTotal?std::min(1.f,float(double(updateReceived)/double(updateTotal))):0.f;
    ImGui::ProgressBar(progress,{-1,0},updateStage=="verifying"?"Verifying package":nullptr);
    if(updateStage=="ready"){
      ImGui::TextWrapped("The update is ready. Restart the editor to install it and continue. Your settings and maps are retained.");
      bool restart=ImGui::Button("Restart editor",{-1,0});welcomeQaItem("update/restart");if(restart){
        documentRequest("__update_restart");
      }
    }else if(updateStage=="error"){
      bool retry=ImGui::Button("Retry update",{-1,0});welcomeQaItem("update/retry");if(retry)updateStart("prepare");
      bool close=ImGui::Button("Close editor",{-1,0});welcomeQaItem("update/close");if(close)documentRequest("exit");
    }
    return false;
  }
  if(updateStage=="current"&&!recentManualUpdates)return true;
  if(updateStage=="available"){
    ImGui::TextWrapped("A new release is available. Download it now to continue with the latest version.");
    ImGui::BeginDisabled(updateRunning());
    bool start=ImGui::Button("Update and continue",{-1,0});welcomeQaItem("update/start");
    ImGui::EndDisabled();
    if(start&&updateStart("prepare"))updateCommitted=true;
  }
  if(!recentManualUpdates){
    bool skip=ImGui::Button("Continue without updating",{-1,0});welcomeQaItem("update/skip");if(skip)return true;
  }
  if(updateStage=="error"||(recentManualUpdates&&updateStage=="current")){
    ImGui::BeginDisabled(updateRunning());
    if(ImGui::Button("Check again",{-1,0}))updateStart();
    ImGui::EndDisabled();
  }
  return false;
}
static bool welcomeUpdatePanel(){
  // Keep the artwork in the update window throughout checking, downloading,
  // verification and the restart prompt. It never covers status or controls.
  static qhandle_t splash=0;
  if(!splash&&std::filesystem::is_regular_file(documentPath("ui/update-splash.png")))splash=draw->LoadImage("ui/update-splash.png");
  float width=0,height=0;
  if(!splash||draw->ImageSize(splash,&width,&height)<=0||width<=0||height<=0)return welcomeUpdateContent();
  bool done=false;
  if(ImGui::BeginTable("update-splash-layout",2,ImGuiTableFlags_SizingStretchSame)){
    ImGui::TableNextColumn();
    const auto room=ImGui::GetContentRegionAvail();
    const float factor=std::min(room.x/width,std::max(1.f,room.y)/height);
    const ImVec2 size{width*factor,height*factor};
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(room.x-size.x)*.5f));
    ImGui::Image((ImTextureID)splash,size);welcomeQaItem("update/splash");
    ImGui::TableNextColumn();done=welcomeUpdateContent();ImGui::EndTable();
  }
  return done;
}
static void updatePanel(){
  ImGui::Text("ReyondBroom %s",editorVersion().c_str());
  if(!updateAvailable.empty())ImGui::TextColored(ImVec4(.42f,.78f,.58f,1.f),"Available: %s",updateAvailable.c_str());
  ImGui::TextWrapped("%s",updateMessage.empty()?"Updates are checked automatically at startup.":updateMessage.c_str());
  bool showProgress=false;
  if(updateCommitted){showProgress=ImGui::Button("Show update progress");welcomeQaItem("updates/show-progress");}
  else {
    ImGui::BeginDisabled(updateRunning());
    if(updateStage=="available"){
      const bool start=ImGui::Button("Update and continue");welcomeQaItem("updates/start");
      if(start&&updateStart("prepare"))updateCommitted=showProgress=true;
    }
    if(ImGui::Button("Check for updates"))updateStart();
    ImGui::EndDisabled();
  }
  if(showProgress){
    recentOpenUpdates();
    ImGui::CloseCurrentPopup();
  }
}
