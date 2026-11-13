// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace {
void jamQaClick(ImVec2 point)
{
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(point.x,point.y);
  io.AddMouseButtonEvent(0,true);
  io.AddMouseButtonEvent(0,false);
}

void jamQaTick()
{
  const auto* stateDirectory = std::getenv("REYONDBROOM_JAM_QA_STATE");
  if (!stateDirectory || !jamEnabled() || !cvars->GetFloat("ftebroom_skip_setup")) return;
  static int stage = 0;
  static std::string plot, email, password;
  static std::string playedRevision;
  static JamFs::path currentDraft;
  static const std::string nonce = jam::randomHex(4);
  static const auto start = std::chrono::steady_clock::now();
  static bool finished = false;
  static const int offlineProfile = gameProfile;
  if (finished) return;

  const auto record = [](int step, const std::string& message, bool passed = false) {
    jam::writeBytes(documentPath("mapping-jams-qa.json"),
      JamJson{{"stage",step},{"message",message},{"passed",passed}}.dump());
  };
  if(std::getenv("REYONDBROOM_JAM_QA_RESUME")){
    jamLocal.open=true;
    if(jamLocal.screen==JamScreen::Board&&!jamLocal.client.token.empty()&&jamLocal.rememberSession){
      record(100,"Restored the remembered session and verified pack after restarting the editor.",true);
      finished=true;command("quit force\n");
    }else if(!jamLocal.error.empty()||std::chrono::steady_clock::now()-start>std::chrono::seconds(30)){
      record(100,jamLocal.error.empty()?"Remembered session was not restored.":jamLocal.error);
      finished=true;command("quit force\n");
    }
    return;
  }
  const auto registerUser = [&](const char* role) {
    const auto nickname = std::string(role) + "-" + nonce;
    email = nickname + "@example.test";
    password = jam::randomHex(16);
    jamAssign(jamLocal.email,sizeof jamLocal.email,email);
    jamAssign(jamLocal.nickname,sizeof jamLocal.nickname,nickname);
    jamAssign(jamLocal.password,sizeof jamLocal.password,password);
    jamLocal.screen = JamScreen::Register;
    jamRegister();
  };
  const auto verifyUser = [&]() {
    const auto path = JamFs::u8path(stateDirectory) / "mail" / (jam::sha256(email) + ".json");
    const auto result = JamJson::parse(jam::fileBytes(path,4096));
    jamAssign(jamLocal.code,sizeof jamLocal.code,jamField(result,"token"));
    jamVerify();
  };
  const auto loginUser = [&]() {
    jamAssign(jamLocal.password,sizeof jamLocal.password,password);
    jamLocal.rememberSession=true;
    jamLogin();
  };
  try {
    if (!jamLocal.error.empty()) throw std::runtime_error(jamLocal.error);
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(120))
      throw std::runtime_error("Editor integration test timed out.");
    if (jamBusy() || savingDocument || assetsLoading) return;
    switch (stage) {
      case 0:
        jamLocal.open = true;
        if (!jamAlphaWarningAccepted()) {
          if (jamQaAlphaButton.x <= 0) return;
          if (std::getenv("REYONDBROOM_JAM_QA_CAPTURE")) {
            static const double shown = ImGui::GetTime();
            static bool captured = false;
            if (ImGui::GetTime()-shown < .5) return;
            if (!captured) {
              command("screenshot mapping-jams-alpha-warning.png\n");
              captured = true;
              return;
            }
          }
          jamQaClick(jamQaAlphaButton);
          return;
        }
        jamAssign(jamLocal.password,sizeof jamLocal.password,"too-short");
        jamRegister();
        if (jamBusy() || jamLocal.error!=jamPasswordRules())
          throw std::runtime_error("Registration did not explain the password length before submission.");
        registerUser("author");
        break;
      case 1:
        verifyUser();
        break;
      case 2:
        if(std::getenv("REYONDBROOM_JAM_QA_CAPTURE")){
          static double captureSignIn=ImGui::GetTime();
          if(ImGui::GetTime()-captureSignIn<.5)return;
          command("screenshot mapping-jams-login.png\n");
        }
        loginUser();
        break;
      case 3:
        if (jamLocal.screen != JamScreen::Profile) return;
        jamAssign(jamLocal.bio,sizeof jamLocal.bio,"Local editor integration test");
        if (const auto* fixture = std::getenv("REYONDBROOM_JAM_QA_AVATAR")) {
          if (!jamLocal.avatarEdited) {
            jamUploadAvatar(JamFs::u8path(fixture));
            return;
          }
          if (!jamLocal.avatarTextures.count(jamLocal.pendingAvatarUrl)) {
            jamLoadAvatar(jamLocal.pendingAvatarUrl);
            return;
          }
          if (!jamLocal.avatarTextures.at(jamLocal.pendingAvatarUrl))
            throw std::runtime_error("The editor could not render the normalized avatar.");
        }
        jamSaveProfile(false);
        break;
      case 4:
        if(jamLocal.screen==JamScreen::Board)break;
        if (jamLocal.screen != JamScreen::Resources) return;
        if(std::getenv("REYONDBROOM_JAM_QA_CAPTURE")){
          static const double started=ImGui::GetTime();
          if(ImGui::GetTime()-started<.5)return;
          command("screenshot mapping-jams-resources.png\n");
        }
        jamResources();
        break;
      case 5:
        if (jamLocal.screen != JamScreen::Board) return;
        if (std::getenv("REYONDBROOM_JAM_QA_CAPTURE")) {
          static const auto captureStart = std::chrono::steady_clock::now();
          static bool captured = false;
          const auto elapsed = std::chrono::steady_clock::now()-captureStart;
          jamLocal.selection = "C3";
          if (jamField(jamCell(),"kind") != "hub") throw std::runtime_error("The central tile did not select the Hub.");
          if (elapsed < std::chrono::milliseconds(600)) return;
          if (!captured) {
            command("screenshot mapping-jams-board.png\n");
            captured = true;
          }
          if (elapsed < std::chrono::seconds(2)) return;
        }
        for (const auto& cell : jamLocal.board.at("cells")) {
          if (jamField(cell,"kind")=="plot" && cell.at("owner").is_null()) {
            plot = jamField(cell,"plotId");
            break;
          }
        }
        if (plot.empty()) throw std::runtime_error("No available test plot.");
        jamLocal.selection = plot;
        jamPurchase();
        break;
      case 6:
        if (jamLocal.wallet.at("spendableRb") != 0) throw std::runtime_error("Purchase did not debit 20 RB.");
        if(jamLocal.purchasedPlot!=plot)throw std::runtime_error("Purchase animation did not start.");
        if(std::getenv("REYONDBROOM_JAM_QA_CAPTURE")){
          const auto elapsed=ImGui::GetTime()-jamLocal.purchasedAt;
          if(elapsed<.35)return;
          command("screenshot mapping-jams-purchase.png\n");
        }
        jamOpenDraft();
        break;
      case 7:
        if (documentFile != jamDraft(plot)) return;
        if (gameProfile!=2 || jamLocal.offlineProfile!=offlineProfile ||
            jamPreviewRuntime()!=jamLocal.pack/"lq1" || !gameMapModsError.empty() ||
            modFilesVisible || !q1MissingWads.empty())
          throw std::runtime_error("The Jam editor did not switch to its own resource pack.");
        for (const auto& wad : q1WadPaths()) {
          if (wad.filename()=="quake1-editor.wad" || q1WadLabels.count(wad.u8string())) continue;
          const auto relative=wad.lexically_relative(jamPreviewRuntime());
          if(relative.empty()||*relative.begin()=="..")
            throw std::runtime_error("A WAD from the offline profile was loaded into the Jam: "+wad.u8string());
        }
        jamRequestUpload(true);
        if (jamLocal.fileDialog!=JamFileDialog::Play || !documentUntitled)
          throw std::runtime_error("Play did not request a name for the unnamed Jam map.");
        jamLocal.fileDialog=JamFileDialog::None;
        runUiLive=true;
        runUiGlobalRects=nlohmann::json::object();
        buildBegin();
        if (jamLocal.fileDialog!=JamFileDialog::Compile || !documentUntitled)
          throw std::runtime_error("An unnamed Jam map did not request a name before compilation.");
        if (jamAcceptDocument("@"+documentPath("maps/start.map").u8string()) ||
            jamCanSaveTo(documentPath("maps/outside-jam.map"),documentName))
          throw std::runtime_error("The Jam workspace accepted an external map path.");
        documentError.clear();
        jamAssign(jamLocal.mapName,sizeof jamLocal.mapName,"../outside");
        if (jamSaveNamed()) throw std::runtime_error("Jam accepted a name with a directory.");
        jamAssign(jamLocal.mapName,sizeof jamLocal.mapName,"Editor's compiled Jam room");
        if (!jamSaveNamed()) throw std::runtime_error("Could not save the Jam map by name: "+jamLocal.fileError);
        jamLocal.fileDialog=JamFileDialog::None;
        stage=70;
        return;
      case 70:
        if (jamLocal.compileReady || buildRunning || runActive) return;
        if (buildResult.empty() || buildResultDocument!=documentFile || documentUntitled ||
            documentFile.parent_path()!=jamDraft(plot).parent_path() ||
            documentName!="Editor's compiled Jam room" || !JamFs::is_regular_file(buildResult))
          throw std::runtime_error("The named Jam map did not compile: "+buildMessage+" "+runError);
        if (const auto maps=jamMaps(); std::find(maps.begin(),maps.end(),documentFile)==maps.end())
          throw std::runtime_error("The Jam map is missing from the restricted map picker.");
        if (modRuntimeDirectory()!=jamLocal.pack/"lq1")
          throw std::runtime_error("Play mod selected the editor's game profile instead of the Jam pack.");
        if (!jamQaPlayButton.x) return;
        if (runUiGlobalRects.contains("tools/Compile") || runUiGlobalRects.contains("tools/Setup compile") ||
            runUiGlobalRects.contains("tools/Play mod"))
          throw std::runtime_error("The standard compile controls are visible in Jam.");
        runUiLive=false;
        currentDraft=documentFile;
        jamQaClick(jamQaPlayButton);
        stage=701;
        return;
      case 701:
        if (!jamLocal.child) return;
        if (jamCell(plot).at("publication").is_null())
          throw std::runtime_error("Play started before publication.");
        playedRevision=jamField(jamCell(plot).at("publication"),"revisionId");
        stage=71;
        return;
      case 71:
        if (jamLocal.child) return;
        if (jamLocal.open || documentFile!=currentDraft ||
            jamLocal.status!=jamText("Returned to editing.","Возврат к редактированию."))
          throw std::runtime_error("Play did not return to the current draft.");
        buildMessage.clear();
        stage=72;
        return;
      case 72: {
        auto* toolbar = ImGui::FindWindowByName("##jam-viewport-actions");
        if (!toolbar || !toolbar->Active) return;
        if (jamCompileButton()!=JamCompileButton::View) throw std::runtime_error("Published draft lacks View compiled map.");
        jamQaClick(jamQaUploadButton);
        stage=721;
        return;
      }
      case 721:
        if (!cvars->GetFloat("ftebroom_bsp_preview")) return;
        jamQaClick(jamQaUploadButton);
        stage=722;
        return;
      case 722:
        if (jamCompileButton()!=JamCompileButton::View) return;
        command("ftebroom_entity_create light 0 \"-48 0 64\"\n");
        stage=723;
        return;
      case 723:
        if (jamCompileButton()!=JamCompileButton::Upload) return;
        stage=724;
        return;
      case 724:
        jamLocal.selection = "C3";
        if (std::getenv("REYONDBROOM_JAM_QA_CAPTURE")) {
          command("screenshot mapping-jams-viewport.png\n");
        }
        jamQaClick(jamQaUploadButton);
        stage=73;
        return;
      case 73:
        if (!jamPublishing() && !jamBusy()) jamRequestUpload();
        if (jamLocal.fileDialog==JamFileDialog::Upload) {
          if (!jamSaveNamed()) throw std::runtime_error(jamLocal.fileError);
          jamLocal.fileDialog=JamFileDialog::None;
        }
        if (!jamPublishing() && !jamBusy()) throw std::runtime_error("The viewport upload did not start.");
        stage=75;
        return;
      case 75:
        if (jamPublishing() || jamField(jamCell(plot).at("publication"),"revisionId")==playedRevision ||
            !cvars->GetFloat("ftebroom_bsp_preview")) return;
        if (jamCompileButton()!=JamCompileButton::Edit) throw std::runtime_error("Published BSP lacks Back to editing.");
        if (std::getenv("REYONDBROOM_JAM_QA_CAPTURE")) command("screenshot mapping-jams-compiled.png\n");
        jamLocal.selection = "C3";
        jamQaClick(jamQaBrowseButton);
        stage=751;
        return;
      case 751:
        if (!jamLocal.open) return;
        jamPlay(true);
        stage=752;
        return;
      case 752:
        if (!jamLocal.child) return;
        stage=753;
        return;
      case 753:
        if (jamLocal.child) return;
        if (jamLocal.open || documentFile!=currentDraft ||
            jamLocal.status!=jamText("Returned to editing.","Возврат к редактированию."))
          throw std::runtime_error("Hub did not return to the current draft.");
        jamLocal.selection = plot;
        jamLocal.open = true;
        stage=8;
        return;
      case 8:
        if (jamPublishing()) return;
        if (jamCell().at("publication").is_null()) return;
        if(std::getenv("REYONDBROOM_JAM_QA_CAPTURE")){
          static const double started=ImGui::GetTime();
          if(ImGui::GetTime()-started<.5)return;
          command("screenshot mapping-jams-own-play.png\n");
        }
        if(jam::savedSession(jamSessionPath())!=jamLocal.client.token)
          throw std::runtime_error("Remember me did not store a usable session.");
        jamPlay();
        stage=80;
        return;
      case 80:
        if (!jamLocal.child) return;
        stage=81;
        return;
      case 81:
        if (jamLocal.child) return;
        if(jamLocal.status!=jamText("Returned to the Jam board.","Возврат на поле Jam."))
          throw std::runtime_error("The owner could not play the published map.");
        jamSignOut();
        stage=8;
        break;
      case 9:
        if (jamLocal.screen != JamScreen::SignIn) return;
        registerUser("player");
        break;
      case 10: verifyUser(); break;
      case 11: loginUser(); break;
      case 12:
        if (jamLocal.screen != JamScreen::Profile) return;
        jamSaveProfile(true);
        break;
      case 13:
        if(jamLocal.screen==JamScreen::Resources)throw std::runtime_error("The verified pack was offered again.");
        if(jamLocal.screen!=JamScreen::Board)return;
        break;
      case 14: {
        if (jamLocal.screen != JamScreen::Board) return;
        jamLocal.selection = plot;
        const auto cell = jamCell();
        if (cell.at("publication").is_null() ||
            jamField(cell.at("owner"),"accountId") == jamField(jamLocal.account,"accountId"))
          throw std::runtime_error("The second account did not receive the author's publication.");
        jamLocal.gift = 3;
        jamGift(jamField(cell.at("owner"),"accountId"));
        break;
      }
      case 15:
        if (jamLocal.wallet.at("giftableRb") != 7) throw std::runtime_error("Gift did not debit the gift wallet.");
        if (std::getenv("REYONDBROOM_JAM_QA_AVATAR")) {
          const auto cell = jamCell();
          const auto url = jamField(cell.at("owner"),"avatarUrl");
          if (url.empty()) throw std::runtime_error("The player's board is missing the author's avatar.");
          if (!jamLocal.avatarTextures.count(url)) { jamLoadAvatar(url); return; }
          if (!jamLocal.avatarTextures.at(url)) throw std::runtime_error("Public avatar failed to render.");
          jamReport("revision",jamField(cell.at("publication"),"revisionId"),"broken_map");
          break;
        }
        ++stage;
        [[fallthrough]];
      case 16:
        jamPlay();
        break;
      case 17:
        if (!jamLocal.child) return;
        break;
      case 18:
        if (jamLocal.child) return;
        if (jamLocal.status != jamText("Returned to the Jam board.","Возврат на поле Jam."))
          throw std::runtime_error("Play mode did not return cleanly.");
        jamLike(plot);
        stage=180;
        return;
      case 180:
        if (jamLocal.wallet.at("giftableRb") != 6 || jamCell(plot).at("rbLikes") != 1)
          throw std::runtime_error("The RB Like did not update the gift balance and plot count.");
        jamLocal.selection = "C3";
        jamPlay(true);
        stage=181;
        return;
      case 181:
        if (!jamLocal.child) return;
        stage=182;
        return;
      case 182:
        if (jamLocal.child) return;
        if (jamLocal.status != jamText("Returned to the Jam board.","Возврат на поле Jam."))
          throw std::runtime_error("Hub play did not return cleanly.");
        stage=18;
        if (std::getenv("REYONDBROOM_JAM_QA_AVATAR")) { jamSignOut(); break; }
        record(stage,"Registered two accounts, published and played a map, sent an RB Like and played the Hub.",true);
        finished = true;
        command("quit force\n");
        return;
      case 23:
        if (jamLocal.screen!=JamScreen::Board) return;
        record(stage,"Verified Play compiles and publishes, Hub returns to the draft, and visitor play, RB Like, and account recovery work.",true);
        finished = true;
        command("quit force\n");
        return;
      case 19:
        jamLocal.screen = JamScreen::Recover;
        jamForgotPassword();
        break;
      case 20: {
        if (jamLocal.screen != JamScreen::Reset) return;
        const auto path = JamFs::u8path(stateDirectory) / "mail" / (jam::sha256(email) + ".reset.json");
        const auto mail = JamJson::parse(jam::fileBytes(path,4096));
        jamAssign(jamLocal.code,sizeof jamLocal.code,jamField(mail,"token"));
        password = jam::randomHex(16);
        jamAssign(jamLocal.password,sizeof jamLocal.password,password);
        jamAssign(jamLocal.passwordConfirmation,sizeof jamLocal.passwordConfirmation,password);
        jamResetPassword();
        break;
      }
      case 21:
        if (jamLocal.screen != JamScreen::SignIn) return;
        loginUser();
        break;
      case 22:
        if (jamLocal.screen != JamScreen::Board) return;
        break;
      default: throw std::runtime_error("Unexpected Mapping Jams QA stage.");
    }
    ++stage;
    record(stage,"running");
  } catch (const std::exception& error) {
    record(stage,error.what());
    finished = true;
    command("quit force\n");
  }
}
} // namespace
