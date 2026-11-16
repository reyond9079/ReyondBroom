// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <iomanip>
#include <locale>
#include "ftebroom_jam_chat.h"
#include "ftebroom_jam_pose.h"

namespace {
struct JamWorldPreview {
  std::string revision, model;
  JamJson faces=JamJson::array();
};
struct JamWorldHub {
  std::string version, model;
};
struct JamWorldState {
  std::future<JamJson> sync, content;
  std::future<void> uploadDraft;
  std::future<JamWorldHub> hubCheck;
  std::chrono::steady_clock::time_point nextSync, nextContent, captureDeadline, nextCapture, nextHubCheck;
  JamJson state, draft;
  std::map<std::string,JamWorldPreview> previews;
  std::map<std::string,std::string> online;
  std::string outgoingMode, activePlot, document, lastScene, sceneKey, session, draftDocument;
  std::string syncSession, syncDocument, readyDocument, contentSession, hubSession, requestedPlot, requestedMode, requestedRevision;
  std::optional<std::string> savedFullbright;
  JamWorldHub hub;
  std::chrono::steady_clock::time_point lastSync;
  uint64_t generation=0;
  int capture=0, pendingCapture=0, captureRevision=-1, boundsErrors=0, scene=0;
  unsigned syncErrors=0, obsoletePreviews=0;
  bool knownPresence=false, active=false, compiled=false;
  std::vector<std::pair<std::chrono::steady_clock::time_point,std::string>> notices;
} jamWorld;

void jamWorldIssue(const char* stage, const std::exception* error = nullptr)
{
  ++jamWorld.syncErrors;
  jamRecordTelemetry("error","world_sync");
  if (cvars->GetFloat("ftebroom_skip_setup") && std::getenv("REYONDBROOM_JAM_QA_WORLD"))
    core->Print((std::string("FTEBROOM_JAM_WORLD_ERROR ")+stage+(error?": "+std::string(error->what()):"")+"\n").c_str());
}

bool jamWorldPlot(const std::string& plot)
{
  return plot.size()==2 && plot[0]>='A' && plot[0]<='E' && plot[1]>='1' && plot[1]<='5';
}

std::array<float,3> jamWorldCenter(const std::string& plot)
{
  return {float(plot[1]-'1')*2048.f,-float(plot[0]-'A')*2048.f,0.f};
}

void jamWorldRefreshHub()
{
  using namespace std::chrono;
  const auto now=steady_clock::now();
  if (jamWorld.hubCheck.valid()) {
    if (jamWorld.hubCheck.wait_for(seconds(0))!=std::future_status::ready) return;
    try {
      auto hub=jamWorld.hubCheck.get();
      if (jamWorld.hubSession==jamWorld.session) {
        if (hub.model!=jamWorld.hub.model) command("fs_flush\n");
        jamWorld.hub=std::move(hub);
        jamWorld.nextHubCheck=now+seconds(5);
      }
    } catch (const jam::Error& error) {
      if (jamWorld.hubSession==jamWorld.session) {
        if (error.code!="ARTIFACT_CHANGED") jamWorldIssue("hub",&error);
        jamWorld.nextHubCheck=now+(error.retryable ? seconds(5) : seconds(15));
      }
    } catch (const std::exception& error) {
      if (jamWorld.hubSession==jamWorld.session) {
        jamWorldIssue("hub",&error);
        jamWorld.nextHubCheck=now+seconds(15);
      }
    }
  }
  if (now<jamWorld.nextHubCheck) return;
  const auto target=documentPath("maps/ftebroom-build");
  if (target.empty()) return;
  const auto client=jamLocal.client;
  const auto known=jamWorld.hub.version;
  const auto pack=jamField(jamLocal.rules,"packVersion");
  jamWorld.hubSession=jamWorld.session;
  jamWorld.hubCheck=std::async(std::launch::async,[client,target,known,pack] {
    const auto manifest=client.json("GET",client.prefix+"hub/manifest");
    const auto version=jamField(manifest,"hubVersion");
    const auto hash=jamField(manifest,"sha256");
    if (!jam::hexId(version,64) || !jam::hexId(hash,64) || jamField(manifest,"mapStem")!="jam_hub" ||
        jamField(manifest,"packVersion")!=pack)
      throw std::runtime_error("Invalid Hub manifest or resource pack.");
    // Version includes the portal destinations. The archive hash identifies
    // the render asset even if a map replacement leaves the Hub geometry unchanged.
    const auto folder="jam-hub-"+hash;
    const auto destination=target/folder;
    const auto bsp=ftebroom::filesystemPath(destination/"maps/jam_hub.bsp");
    if (version!=known || !JamFs::is_regular_file(bsp))
      jam::download(client,manifest,destination,false,"jam_hub",true);
    return JamWorldHub{version,"maps/ftebroom-build/"+folder+"/maps/jam_hub.bsp"};
  });
}

bool jamWorldMaterial(const std::string& name)
{
  return !name.empty() && name.size()<=64 && name.find("..")==name.npos &&
    name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+*/_.{}!-")==name.npos;
}

JamJson jamReadWorldCapture(int marker)
{
  const auto path=documentPath("data/jam-world-draft.txt");
  if (path.empty() || !JamFs::is_regular_file(ftebroom::filesystemPath(path))) return nullptr;
  std::istringstream input(jam::fileBytes(path,512*1024));
  input.imbue(std::locale::classic());
  std::string header;
  int found=0;
  if (!(input>>header>>found) || header!="RBJAM1" || found!=marker) return nullptr;
  JamJson faces=JamJson::array();
  std::string line;
  std::getline(input,line);
  bool complete=false;
  while (std::getline(input,line)) {
    if (line=="RBJAM-END") { complete=true; break; }
    if (faces.size()>=8192) continue;
    std::istringstream fields(line);
    fields.imbue(std::locale::classic());
    std::string material;
    int count=0;
    if (!(fields>>material>>count) || count<3 || count>64) continue;
    if (!jamWorldMaterial(material)) material="missing";
    JamJson points=JamJson::array();
    for (int v=0;v<count;v++) {
      std::array<float,5> pos{};
      if (!(fields>>pos[0]>>pos[1]>>pos[2]>>pos[3]>>pos[4]) || !std::isfinite(pos[0]) ||
          !std::isfinite(pos[1]) || !std::isfinite(pos[2]) || !std::isfinite(pos[3]) || !std::isfinite(pos[4])) break;
      const auto quantize=[](float v,float limit) {
        return std::round((std::abs(v)<=limit+.002f ? std::clamp(v,-limit,limit) : v)*100.0)/100.0;
      };
      points.push_back({quantize(pos[0],1024),quantize(pos[1],1024),quantize(pos[2],512),
        std::round(pos[3]*10000.0)/10000.0,std::round(pos[4]*10000.0)/10000.0});
    }
    if (points.size()==std::size_t(count)) faces.push_back({material,std::move(points)});
  }
  if (!complete) return nullptr;
  if (faces.dump().size()>490*1024) throw std::runtime_error("Jam live preview is too large.");
  return faces;
}

void jamWorldPresence(const JamJson& state)
{
  std::map<std::string,std::string> next;
  const auto self=jamField(jamLocal.account,"accountId");
  for (const auto& user:state.at("online")) {
    const auto id=jamField(user,"account");
    if (id!=self && jam::hexId(id,32)) next[id]=jamField(user,"nickname");
  }
  if (jamWorld.knownPresence) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for (const auto& [id,name]:next) if (!jamWorld.online.count(id))
      jamWorld.notices.emplace_back(until,name+jamText(" joined Mapping Jams"," вошёл в Mapping Jams"));
    for (const auto& [id,name]:jamWorld.online) if (!next.count(id))
      jamWorld.notices.emplace_back(until,name+jamText(" left Mapping Jams"," вышел из Mapping Jams"));
    if (jamWorld.notices.size()>8)
      jamWorld.notices.erase(jamWorld.notices.begin(),jamWorld.notices.end()-8);
  }
  jamWorld.knownPresence=true;
  jamWorld.online=std::move(next);
}

void jamWriteWorldScene(const std::string& local)
{
  std::string key=local+"|"+jamWorld.hub.model;
  for(const auto& other:jamWorld.state.at("plots")) {
    const auto plot=jamField(other,"plot"),revision=jamField(other,"revision");
    const auto cached=jamWorld.previews.find(plot);
    if(plot!=local && cached!=jamWorld.previews.end() && cached->second.revision==revision)
      key+="|"+plot+":"+revision+":"+cached->second.model;
  }
  if(key==jamWorld.sceneKey)return;
  std::ostringstream scene;
  scene.imbue(std::locale::classic());
  scene<<std::setprecision(7);
  const auto own=jamWorldCenter(local);
  if (!jamWorld.hub.model.empty()) {
    const auto center=jamWorldCenter("C3");
    scene<<"model "<<jamWorld.hub.model<<' '<<center[0]-own[0]<<' '<<center[1]-own[1]<<" 0 0\n";
  }
  for (const auto& other:jamWorld.state.at("plots")) {
    const auto plot=jamField(other,"plot");
    if (plot==local || !jamWorldPlot(plot)) continue;
    auto offset=jamWorldCenter(plot);
    for (int axis=0;axis<3;axis++) offset[axis]-=own[axis];
    const auto cached=jamWorld.previews.find(plot);
    if (cached==jamWorld.previews.end() || cached->second.revision!=jamField(other,"revision")) continue;
    if (!cached->second.model.empty()) {
      scene<<"model "<<cached->second.model<<' '<<offset[0]<<' '<<offset[1]<<" 0 0\n";
      continue;
    }
    for (const auto& face:cached->second.faces) {
      const auto material=face.at(0).get<std::string>();
      if (!jamWorldMaterial(material)) continue;
      scene<<"face "<<material<<' '<<face.at(1).size()<<'\n';
      for (const auto& position:face.at(1))
        scene<<position.at(0).get<float>()+offset[0]<<' '<<position.at(1).get<float>()+offset[1]
          <<' '<<position.at(2).get<float>()<<' '<<(position.size()==5?position[3].get<float>():position[0].get<float>()/64)
          <<' '<<(position.size()==5?position[4].get<float>():position[2].get<float>()/64)<<'\n';
    }
  }
  auto result=scene.str();
  if(result!=jamWorld.lastScene) {
    jam::writeBytes(documentPath("data/jam-world-scene.txt"),result);
    jamWorld.lastScene=std::move(result);
    cvars->SetFloat("ftebroom_jam_scene",++jamWorld.scene);
  }
  jamWorld.sceneKey=std::move(key);
}

void jamWorldFetchContent(const std::string& local)
{
  using namespace std::chrono;
  const auto now=steady_clock::now();
  if (jamWorld.content.valid()) {
    if (jamWorld.content.wait_for(milliseconds(0))!=std::future_status::ready) return;
    try {
      auto data=jamWorld.content.get();
      if (jamWorld.contentSession==jamWorld.session) {
        if (data.is_null()) {
          // A board snapshot can be overtaken by a new draft or published revision.
          // Wait for current metadata before starting another download.
          ++jamWorld.obsoletePreviews;
          jamWorld.nextSync=now;
          jamWorld.nextContent=now+milliseconds(750);
        } else {
          for (const auto& item:jamWorld.state.at("plots")) {
            if (jamField(item,"plot")!=jamWorld.requestedPlot ||
                jamField(item,"mode")!=jamWorld.requestedMode ||
                jamField(item,"revision")!=jamWorld.requestedRevision) continue;
            const auto model=jamField(data,"model");
            jamWorld.previews[jamWorld.requestedPlot]={jamField(data,"revision"),model,data.value("faces",JamJson::array())};
            if (!model.empty()) command("fs_flush\n");
            break;
          }
        }
      }
    } catch (const std::exception& error) {
      if (jamWorld.contentSession==jamWorld.session) {
        jamWorldIssue("preview",&error);
        jamWorld.nextContent=now+seconds(2);
      }
    }
  }
  if (now<jamWorld.nextContent) return;
  for (const auto& item:jamWorld.state.at("plots")) {
    const auto plot=jamField(item,"plot"),revision=jamField(item,"revision"),mode=jamField(item,"mode");
    if (plot==local || !jamWorldPlot(plot) || !jam::hexId(revision,32)) continue;
    if (jamWorld.previews.count(plot) && jamWorld.previews.at(plot).revision==revision) continue;
    if (mode!="map" && mode!="bsp") continue;
    const auto client=jamLocal.client;
    const auto cache=jamRoot()/"published"/revision;
    const auto model="maps/ftebroom-build/world-"+revision+"/jam_map.bsp";
    const auto target=documentPath(model);
    if (target.empty()) continue;
    jamWorld.requestedPlot=plot;
    jamWorld.requestedMode=mode;
    jamWorld.requestedRevision=revision;
    jamWorld.contentSession=jamWorld.session;
    jamWorld.content=std::async(std::launch::async,[client,plot,revision,mode,cache,model,target]() -> JamJson {
      try {
        if (mode=="map") return client.json("GET",client.prefix+"world/drafts/"+plot+"/"+revision);
        auto manifest=client.json("GET",client.prefix+"plots/"+plot+"/play-manifest");
        if (jamField(manifest,"revisionId")!=revision) return nullptr;
        jam::download(client,manifest,cache,false,"jam_map",true);
      } catch (const jam::Error& error) {
        if ((mode=="map" && (error.code=="DRAFT_NOT_CURRENT" || error.code=="NOT_FOUND")) ||
            (mode=="bsp" && (error.code=="NO_PLAYABLE_REVISION" || error.code=="ARTIFACT_CHANGED")))
          return nullptr;
        throw;
      }
      for (const auto* extension:{".bsp",".lit"}) {
        const auto from=ftebroom::filesystemPath(cache/"maps"/(std::string("jam_map")+extension));
        auto to=target;to.replace_extension(extension);
        if (JamFs::is_regular_file(from)) jam::writeBytes(to,jam::fileBytes(from,32*1024*1024));
      }
      return JamJson{{"revision",revision},{"model",model}};
    });
    break;
  }
}

void jamWorldTick()
{
  using namespace std::chrono;
  const auto now=steady_clock::now();
  const bool joined=jamLocal.liveWorld && !jamLocal.client.token.empty() &&
      jamLocal.screen==JamScreen::Board && (jamLocal.open || jamEditing());
  const auto local=jamLocal.draftPlot;
  const bool editing=joined && jamEditing() && jamWorldPlot(local);
  const auto& session=jamLocal.client.token;
  if (jamWorld.session!=session) {
    jamWorld.session=session;
    jamWorld.state=nullptr;
    jamWorld.previews.clear();
    jamWorld.hub={};
    jamWorld.sceneKey.clear();
    jamWorld.readyDocument.clear();
    jamWorld.nextHubCheck={};
    jamWorld.lastSync={};
    jamWorld.nextSync={};
    jamWorld.nextContent={};
    jamWorld.outgoingMode.clear();
    jamWorld.activePlot.clear();
    jamWorld.knownPresence=false;
    jamWorld.online.clear();
    jamWorld.notices.clear();
    jamChat={};
  }
  if (editing!=jamWorld.active) {
    jamWorld.active=editing;
    cvars->SetFloat("ftebroom_jam_world",editing?1:0);
    if (!editing) {
      cvars->SetFloat("ftebroom_jam_scene",0);
      jamWorld.lastScene.clear();
      jamWorld.sceneKey.clear();
    }
  }
  const bool ownPreview=cvars->GetFloat("ftebroom_bsp_preview")!=0;
  if (editing && !ownPreview) {
    if (!jamWorld.savedFullbright) {
      char previous[32]{};
      cvars->GetString("r_fullbright",previous,sizeof previous);
      jamWorld.savedFullbright=previous;
    }
    // Read-only BSP entities use the engine's baked lightmaps in the MAP view.
    if (cvars->GetFloat("r_fullbright")) cvars->SetString("r_fullbright","0");
  } else if (!editing && !ownPreview && jamWorld.savedFullbright) {
    cvars->SetString("r_fullbright",jamWorld.savedFullbright->c_str());
    jamWorld.savedFullbright.reset();
  }
  if (editing && (jamWorld.activePlot!=local || jamWorld.generation!=documentGeneration)) {
    const auto center=jamWorldCenter(local);
    cvars->SetString("ftebroom_jam_plot_center",(std::to_string(center[0])+" "+std::to_string(center[1])+" 0").c_str());
    jamWorld.activePlot=local;
    jamWorld.generation=documentGeneration;
    jamWorld.document=jam::randomHex(16);
    jamWorld.readyDocument.clear();
    jamWorld.pendingCapture=0;
    jamWorld.captureRevision=-1;
    jamWorld.nextCapture={};
    jamWorld.draft=nullptr;
  }
  if (!editing) jamWorld.activePlot.clear();
  if (!joined) {
    jamWorld.knownPresence=false;
    jamWorld.online.clear();
    jamWorld.notices.clear();
  }
  const auto rejected=int(cvars->GetFloat("ftebroom_jam_bounds_rejected"));
  if (editing && rejected!=jamWorld.boundsErrors) {
    jamRecordTelemetry("error","bounds_rejected");
    jamWorld.notices.emplace_back(now+seconds(5),jamText("Edit stopped at your plot boundary.","Операция отменена на границе участка."));
  }
  jamWorld.boundsErrors=rejected;
  if (jamWorld.sync.valid() && jamWorld.sync.wait_for(milliseconds(0))==std::future_status::ready) {
    try {
      auto state=jamWorld.sync.get();
      if (joined && jamWorld.syncSession==session && state.contains("plots")) {
        jamChatReceive(state);
        jamWorldPresence(state);
        if(editing) jamPoseLegacy(state,now);
        if(editing && jamWorld.syncDocument==jamWorld.document)jamWorld.readyDocument=jamWorld.document;
        jamWorld.state=std::move(state);
        jamWorld.lastSync=now;
      }
    } catch (const std::exception& error) {
      if (joined && jamWorld.syncSession==session) {
        jamWorldIssue("heartbeat",&error);
        jamWorld.nextSync=now+seconds(2);
        jamWorld.captureRevision=-1;
      }
    }
  }
  const bool compiled=editing && (cvars->GetFloat("ftebroom_bsp_preview") || buildPreviewRequested);
  if (compiled!=jamWorld.compiled) {
    jamWorld.compiled=compiled;
    jamWorld.readyDocument.clear();
    jamWorld.pendingCapture=0;
    jamWorld.captureRevision=-1;
    jamWorld.draft=nullptr;
    jamWorld.nextSync=now;
  }
  const bool canCapture=editing && !compiled && pendingDocumentFile.empty();
  if (canCapture) {
    const int revision=buildCurrentContentRevision();
    try {
      if (jamWorld.pendingCapture) {
        auto capture=jamReadWorldCapture(jamWorld.pendingCapture);
        if (!capture.is_null()) {
          jamWorld.draft=std::move(capture);
          jamWorld.pendingCapture=0;
        } else if (now>=jamWorld.captureDeadline)
          throw std::runtime_error("Timed out waiting for the editor's draft geometry.");
      }
    } catch (const std::exception& error) {
      jamWorld.pendingCapture=0;
      jamWorld.captureRevision=-1;
      jamWorld.nextCapture=now+seconds(2);
      jamWorldIssue("capture",&error);
    }
    if (!jamWorld.pendingCapture && jamWorld.captureRevision!=revision && now>=jamWorld.nextCapture) {
      jamWorld.captureRevision=revision;
      std::error_code error;
      const auto path=documentPath("data/jam-world-draft.txt");
      if (!path.empty()) JamFs::remove(ftebroom::filesystemPath(path),error);
      else error=std::make_error_code(std::errc::no_such_file_or_directory);
      if (error) {
        jamWorld.captureRevision=-1;
        jamWorld.nextCapture=now+seconds(2);
        jamWorldIssue("capture-file");
      } else {
        jamWorld.pendingCapture=++jamWorld.capture;
        jamWorld.captureDeadline=now+seconds(4);
      }
      cvars->SetFloat("ftebroom_jam_capture",jamWorld.pendingCapture);
    }
  }
  if (!jamWorld.sync.valid() && now>=jamWorld.nextSync &&
      (joined || (!jamWorld.outgoingMode.empty() && jamWorld.outgoingMode!="leave" && !jamLocal.client.token.empty()))) {
    JamJson update;
    if (editing) {
      char camera[512]{};
      cvars->GetString("ftebroom_camera_state",camera,sizeof camera);
      float pose[7]{};
      cameraParse(camera,pose);
      for (int axis=0;axis<3;axis++) pose[axis]=std::isfinite(pose[axis])?std::clamp(pose[axis],-32768.f,32768.f):0;
      const float yaw=std::isfinite(pose[4])?std::remainder(pose[4],360.f):0;
      update={{"mode",compiled?"compiled":"draft"},{"plot",local},{"document",jamWorld.document},
        {"pose",JamJson::array({pose[0],pose[1],pose[2],yaw})},{"publication",nullptr}};
      const auto plot=jamCell(local);
      if (plot.is_object() && plot.at("publication").is_object())
        update["publication"]=jamField(plot.at("publication"),"revisionId");
    } else update={{"mode",joined?"board":"leave"}};
    if (joined) {
      update["chat"]={{"after",jamChat.after}};
      if (jamChat.pending) update["chat"]["send"]={{"id",jamChat.pending->id},{"text",jamChat.pending->text}};
    }
    const auto client=jamLocal.client;
    jamWorld.outgoingMode=jamField(update,"mode");
    jamWorld.syncSession=session;
    jamWorld.syncDocument=editing?jamWorld.document:std::string{};
    jamWorld.nextSync=now+milliseconds(750);
    jamWorld.sync=std::async(std::launch::async,[client,update=std::move(update)](){
      return client.json("POST",client.prefix+"world",update);
    });
  }
  if(jamWorld.uploadDraft.valid() && jamWorld.uploadDraft.wait_for(milliseconds(0))==std::future_status::ready) {
    try {jamWorld.uploadDraft.get();}
    catch(const std::exception& error) {
      if(canCapture && jamWorld.document==jamWorld.draftDocument) {
        jamWorld.captureRevision=-1;
        jamWorld.nextCapture=now+milliseconds(350);
        const auto* problem=dynamic_cast<const jam::Error*>(&error);
        if(!problem || (problem->code!="DRAFT_INACTIVE" && problem->code!="RATE_LIMITED"))jamWorldIssue("draft-upload",&error);
      }
    }
  }
  if(canCapture && jamWorld.readyDocument==jamWorld.document && !jamWorld.uploadDraft.valid() && !jamWorld.draft.is_null()) {
    auto faces=std::exchange(jamWorld.draft,JamJson());
    const auto client=jamLocal.client;
    jamWorld.draftDocument=jamWorld.document;
    jamWorld.uploadDraft=std::async(std::launch::async,[client,document=jamWorld.draftDocument,faces=std::move(faces)] {
      client.json("POST",client.prefix+"world/draft",{{"document",document},{"faces",faces}});
    });
  }
  const auto poseInterval=jamWorld.state.is_object()?jamWorld.state.value("poseIntervalMs",0):0;
  jamPoseTick(editing,jamLocal.client,jamWorld.document,local,poseInterval,now);
  if (editing && jamWorld.state.contains("plots") && now-jamWorld.lastSync<seconds(15)) {
    try {jamWorldFetchContent(local);jamWorldRefreshHub();jamWriteWorldScene(local);}
    catch (const std::exception& error) {jamWorldIssue("scene",&error);}
  } else if (editing && !jamWorld.lastScene.empty()) {
    jamWorld.lastScene.clear();
    jamWorld.sceneKey.clear();
    cvars->SetFloat("ftebroom_jam_scene",0);
  }
}

void jamWorldNotifications()
{
  using namespace std::chrono;
  const auto now=steady_clock::now();
  auto& notices=jamWorld.notices;
  notices.erase(std::remove_if(notices.begin(),notices.end(),[now](const auto& n){return n.first<=now;}),notices.end());
  if (notices.empty()) return;
  auto* viewport=ImGui::GetMainViewport();
  auto* font=ImGui::GetFont();
  auto* draw=ImGui::GetForegroundDrawList(viewport);
  float y=viewport->WorkPos.y+viewport->WorkSize.y-44*uiScale();
  for (auto item=notices.rbegin();item!=notices.rend();++item) {
    const auto size=font->CalcTextSizeA(ImGui::GetFontSize(),440,0,item->second.c_str());
    const float x=viewport->WorkPos.x+viewport->WorkSize.x-size.x-28*uiScale();
    draw->AddRectFilled({x-8,y-5},{x+size.x+8,y+size.y+5},IM_COL32(16,34,53,240),6);
    draw->AddText({x,y},IM_COL32(214,242,247,255),item->second.c_str());
    y-=size.y+14*uiScale();
  }
}
void jamBackgroundShutdown()
{
  // Join before Windows unloads the plugin. Joining from a static destructor
  // can deadlock when a worker needs the DLL loader lock to exit.
  const auto drain=[](auto& future) {
    if (future.valid()) {try {(void)future.get();} catch (const std::exception&) {}}
  };
  if (jamLocal.resourceTransfer) jamLocal.resourceTransfer->canceled=true;
#ifndef _WIN32
  jamLocal.resourceProcess.stop();
#endif
  drain(jamWorld.sync);
  drain(jamWorld.uploadDraft);
  for(auto& lane:jamPose.lanes)drain(lane.work);
  drain(jamWorld.content);
  drain(jamWorld.hubCheck);
  drain(jamTelemetry.request);
  drain(jamLocal.work);
  drain(jamLocal.presenceWork);
  drain(jamLocal.packCleanup);
  if (jamWorld.savedFullbright) cvars->SetString("r_fullbright",jamWorld.savedFullbright->c_str());
}
} // namespace
