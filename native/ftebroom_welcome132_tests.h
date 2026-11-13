// SPDX-License-Identifier: GPL-2.0-or-later
// Included inside testDesktop(). These commands only prepare disposable QA
// fixtures; every acceptance button press comes from real OS input.
if(std::string_view(mode).rfind("welcome132",0)==0){
  firstSetupStep=-1;setupInitialized=true;setupNeeded=false;runUiLive=true;uiLanguage=1;
  if(std::string_view(mode)=="welcome132begin"){
    uiTheme=0;uiScaleChoice=0;uiStyle();ImGui::ClosePopupsOverWindow(nullptr,false);
    recoveryChecked=true;recoveryRequest=true;recoveryOpening=false;
    recoveryFile=documentPath("autosave/welcome132.map");recoverySource=documentPath("maps/welcome132-original.map");
    recentStage=WelcomeStage::Updates;recentWindow=true;
    cvars->SetFloat("ftebroom_dirty",0);frame();frame();
    expect(!ImGui::IsPopupOpen("Recover autosave?",ImGuiPopupFlags_AnyPopupId),"welcome132_no_legacy_recovery_modal");
  }
  if(std::string_view(mode)=="welcome132updates"){
    ImGui::ClosePopupsOverWindow(nullptr,false);cvars->SetFloat("ftebroom_dirty",0);updateCommitted=false;welcomeStartup();updateStart();frame();frame();
  }
  if(std::string_view(mode)=="welcome132splash"){
    welcomeStartup();updateStarted=true;updateCommitted=false;updateAutomaticCheck=true;updateStage="checking";frame();frame();
    const auto item=welcomeQaItems.find("update/splash");
    expect(item!=welcomeQaItems.end()&&item->second["frame"]==ImGui::GetFrameCount(),"welcome132_startup_splash_visible");
    if(item!=welcomeQaItems.end()){
      const auto& r=item->second;const auto center=ImGui::GetMainViewport()->GetCenter();
      expect(std::abs(r["x"].get<float>()+r["width"].get<float>()*.5f-center.x)<2&&std::abs(r["y"].get<float>()+r["height"].get<float>()*.5f-center.y)<2,
             "welcome132_startup_splash_centered");
    }
    for(auto id:{"update/start","update/check"})
      expect(!welcomeQaItems.count(id)||welcomeQaItems[id]["frame"]!=ImGui::GetFrameCount(),"welcome132_checking_hides_action");
    for(auto id:{"update/check-progress","update/skip"})
      expect(welcomeQaItems.count(id)&&welcomeQaItems[id]["frame"]==ImGui::GetFrameCount(),"welcome132_checking_has_progress_and_skip");
  }
  if(std::string_view(mode)=="welcome132check-error"){
    welcomeStartup();updateStarted=true;updateCommitted=false;updateAutomaticCheck=false;updateStage="error";frame();frame();
    expect(welcomeQaItems["update/check"]["frame"]==ImGui::GetFrameCount()&&welcomeQaItems["update/skip"]["frame"]==ImGui::GetFrameCount(),
           "welcome132_error_can_retry_or_continue");
  }
  if(std::string_view(mode)=="welcome132no-update"){
    updateStarted=true;updateSession="qa-no-update";updateCommitted=false;updateStage="current";
    recentStage=WelcomeStage::Updates;recentWindow=true;recoveryChecked=true;recoveryRequest=true;frame();frame();
    expect(recentStage==WelcomeStage::Recovery,"welcome132_no_update_proceeds_to_recovery");
    expect(!ImGui::IsPopupOpen("Recover autosave?",ImGuiPopupFlags_AnyPopupId),"welcome132_no_parallel_recovery_modal");
  }
  if(std::string_view(mode)=="welcome132protocol"){
    const auto session=updateSession,stage=updateStage,message=updateMessage,available=updateAvailable;
    const auto received=updateReceived,total=updateTotal;const bool committed=updateCommitted;
    updateSession="qa-status-session";updateCommitted=false;updateStage="checking";updateMessage="unchanged";updateReceived=updateTotal=0;
    nlohmann::json status={{"session",updateSession},{"current_version",editorVersion()},{"available_version","0.5.0-alpha.999"},
      {"stage","available"},{"message","Available"},{"received",0},{"total",100}};
    auto reject=[&](nlohmann::json invalid,const char* name){
      const auto previousStage=updateStage,previousMessage=updateMessage;const auto previousReceived=updateReceived;
      expect(!updateApplyStatus(invalid)&&updateStage==previousStage&&updateMessage==previousMessage&&updateReceived==previousReceived,name);
    };
    auto invalid=status;invalid["session"]="stale-other-editor";reject(invalid,"welcome132_foreign_status_ignored");
    invalid=status;invalid["current_version"]="0.0.0";reject(invalid,"welcome132_stale_current_version_ignored");
    invalid=status;invalid["stage"]="unknown";reject(invalid,"welcome132_unknown_status_ignored");
    invalid=status;invalid["received"]="malformed";reject(invalid,"welcome132_malformed_status_atomic");
    invalid=status;invalid["received"]=-1;reject(invalid,"welcome132_negative_progress_ignored");
    invalid=status;invalid["total"]=600*1024*1024;reject(invalid,"welcome132_unbounded_progress_ignored");
    invalid=status;invalid["available_version"]="bad\"version";reject(invalid,"welcome132_invalid_version_ignored");
    invalid=status;invalid["stage"]="ready";invalid["received"]=100;reject(invalid,"welcome132_uncommitted_ready_ignored");
    expect(updateApplyStatus(status)&&updateStage=="available"&&updateTotal==100,"welcome132_valid_available_status");
    updateCommitted=true;invalid=status;invalid["stage"]="ready";invalid["received"]=50;reject(invalid,"welcome132_partial_ready_ignored");
    invalid["received"]=100;expect(updateApplyStatus(invalid)&&updateStage=="ready"&&updateReceived==100,"welcome132_verified_ready_status");
    recentOpen();expect(recentStage==WelcomeStage::Updates,"welcome132_open_cannot_skip_committed_update");
    updateSession=session;updateStage=stage;updateMessage=message;updateAvailable=available;updateReceived=received;updateTotal=total;updateCommitted=committed;
    recentStage=WelcomeStage::Updates;frame();frame();
  }
  return;
}
