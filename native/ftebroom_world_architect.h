// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifdef near
// Win32's legacy memory-model macro collides with al::near in the brush
// kernel. Confine the override to this integration header and restore it below.
#pragma push_macro("near")
#undef near
#define FTEBROOM_WORLD_RESTORE_NEAR
#endif
#include "world_architect/assets.h"
#include "world_architect/legacy.h"
#ifndef _WIN32
#include <unistd.h>
#endif
// Reuse the validated MIT convex brush kernel. The external legacy AutoLevel
// plugin retains its own ABI and data; no solver or legacy state is replaced.
#include "../plugins/autolevel/al_brush.cpp"
#include "../plugins/autolevel/al_storage.cpp"

namespace wa=ftebroom::worldArchitect;
struct WorldArchitectWork {std::atomic<bool> cancel{false};std::atomic<int> stage{0};};
struct WorldArchitectState {
  bool open=false,owner=false,dirty=true,qa=false,walk=false,ortho=false,focusRequest=false;
  ImGuiID gesture=0;int selected=0,tab=0,cutaway=-1;float yaw=135,pitch=35,zoom=1;
  wa::V eye{};wa::Settings settings;wa::Registry registry;wa::Json registrySource;std::optional<uint64_t> desiredSeed;
  std::future<wa::CandidateBatch> job;std::shared_ptr<WorldArchitectWork> work;
  std::shared_ptr<wa::PrefixCache> prefixCache=std::make_shared<wa::PrefixCache>();
  wa::CandidateBatch batch;std::string generated,pendingKey,error,message,request;
  std::string project,requestMode,requestRegion;uint64_t detailRegion=0,detailSeed=1;
  uint64_t missionNode=0,dragMission=0,roadSelection=0,detailRoom=0;int missionEdge=0,planFloor=0,maskSelection=-1;
  bool painting=false;wa::V paintStart{};float paintStrength=1;int paintType=0;
  std::optional<wa::LegacyMigration> legacy;bool legacySaved=false;
  double requestAt=0;std::filesystem::path requestPath;
  std::vector<wa::Json> undo,redo;
  std::map<std::string,std::pair<std::array<float,3>,bool>> vectorDrafts;
  std::map<std::string,std::string> pendingShaders;std::map<std::string,qhandle_t> materialHandles;
  wa::Json widgets=wa::Json::object();
  wa::Json inputTrace=wa::Json::array();
} worldArchitect;
static bool worldArchitectFocused(){
  if(!worldArchitect.open||!ImGui::GetCurrentContext())return false;
  for(auto* window=ImGui::GetCurrentContext()->NavWindow;window;window=window->ParentWindowInBeginStack)
    if(std::strstr(window->Name,"World Architect"))return true;
  return false;
}
static void worldRect(const std::string& key){if(!worldArchitect.qa)return;auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();worldArchitect.widgets[key]={{"x",a.x},{"y",a.y},{"width",b.x-a.x},{"height",b.y-a.y},{"active",ImGui::IsItemActive()}};}
static bool worldButton(const char* label,const char* key){bool clicked=ImGui::Button(label);worldRect(key);return clicked;}
template<class F>static void worldAction(F action){try{action();}catch(const std::exception& e){worldArchitect.error=e.what();}}
static wa::WorldPlan* worldSelected(){auto& s=worldArchitect;return s.selected>=0&&size_t(s.selected)<s.batch.worlds.size()?&s.batch.worlds[s.selected]:nullptr;}
static std::string worldIdentity(){
#ifdef _WIN32
  const uint64_t process=GetCurrentProcessId();
#else
  const uint64_t process=uint64_t(getpid());
#endif
  static uint64_t next=wa::mix(process^uint64_t(std::chrono::steady_clock::now().time_since_epoch().count()));return std::to_string(++next);
}
static void worldRegistry(){
  auto& s=worldArchitect;if(s.project.empty())s.project="wa-"+worldIdentity();if(!s.registry.presets.empty())return;size_t size=0;auto raw=static_cast<char*>(files->LoadFile("world-architect/registry.json",&size));
  if(!raw)throw std::runtime_error("World Architect registry is missing from the editor package");std::string bytes(raw,size);core->Free(raw);if(size>1024*1024)throw std::runtime_error("World Architect registry exceeds 1 MiB");s.registrySource=wa::Json::parse(bytes);s.registry=wa::readRegistry(s.registrySource);s.settings.preset=s.registry.presets.front().id;for(const auto& preset:s.registry.presets)if(preset.id=="hospital")s.settings.preset=preset.id;s.settings.profile=gameProfile;s.error.clear();
}
static std::string worldInputKey(){return wa::serialize(worldArchitect.settings).dump()+worldArchitect.registrySource.dump();}
static wa::Json worldProject(){auto& s=worldArchitect;auto json=wa::serialize(s.settings);json["registry"]=s.registrySource;json["projectId"]=s.project;if(auto world=worldSelected();world&&s.generated==worldInputKey()){json["selectedCandidateSeed"]=world->seed;json["diagnostics"]=wa::diagnostics(*world);}return json;}
static void worldLoadProject(const std::string& name){
  if(worldArchitect.job.valid()||!worldArchitect.request.empty())throw std::runtime_error("Wait for generation and map insertion to finish before opening a project");
  auto path=std::filesystem::u8path(name);if(std::filesystem::file_size(path)>8*1024*1024)throw std::runtime_error("World project exceeds 8 MiB");
  const auto json=wa::Json::parse(q1Read(path));auto& s=worldArchitect;auto registry=json.contains("registry")?wa::readRegistry(json.at("registry")):s.registry;auto settings=wa::deserialize(json,registry);
  auto project=json.value("projectId","wa-"+worldIdentity());wa::safeToken(project);if(project.size()>64)throw std::runtime_error("Project identity exceeds its ownership budget");auto desired=json.contains("selectedCandidateSeed")?std::optional<uint64_t>{json.at("selectedCandidateSeed").get<uint64_t>()}:std::nullopt;
  s.project=project;s.registry=std::move(registry);if(json.contains("registry"))s.registrySource=json.at("registry");s.settings=std::move(settings);s.desiredSeed=desired;s.batch={};s.undo.clear();s.redo.clear();s.selected=0;s.generated.clear();s.error.clear();s.message="Project loaded with its preset snapshot. Generate to restore its saved candidate.";
}
static void worldRelease(){auto& s=worldArchitect;if(!s.owner)return;s.owner=false;cvars->SetFloat("ftebroom_bb_active",0);cvars->SetFloat("ftebroom_modelviewer_open",0);cvars->SetFloat("ftebroom_modeleditor_active",0);cvars->SetFloat("ftebroom_modeleditor_freecamera",0);if(auto api=bbAPI())api->clear();}
static void worldClaim(){
  if(modelEditorActive&&modelEditDirty)throw std::runtime_error("Export and close the edited model before taking the shared world preview.");
  if(modelConverterPending||modelImportJob.valid())throw std::runtime_error("Wait for model import / export to finish before taking the shared world preview.");
  // Regeneration does not change preview ownership. Re-entering the model
  // camera/menu focus path here can disturb an SDL2 mouse button transition.
  if(worldArchitect.owner&&!bbActive&&!modelViewerOpen&&!modelEditorActive&&!plantState.owner){worldArchitect.dirty=true;worldArchitect.error.clear();return;}
  bbLeave([]{
    if(!worldArchitect.open)return;
    bbRegister();bbActive=false;modelViewerOpen=false;modelEditorActive=false;modelEditorCameraEnd();plantState.owner=false;cvars->SetFloat("ftebroom_plant_active",0);
    worldArchitect.owner=true;worldArchitect.dirty=true;worldArchitect.error.clear();
  });
}
static void worldGenerate(){
  auto& s=worldArchitect;if(s.job.valid())return;worldRegistry();if(s.settings.profile!=gameProfile)throw std::runtime_error("Project profile differs from the active map. Switch the editor profile or explicitly retarget the project.");
  wa::validate(s.settings,s.registry);s.error.clear();s.message="Generating; the current map is unchanged.";
  s.work=std::make_shared<WorldArchitectWork>();auto settings=s.settings;auto registry=s.registry;auto work=s.work;auto cache=s.prefixCache;
  s.pendingKey=worldInputKey();
  s.job=std::async(std::launch::async,[settings,registry,work,cache](){return wa::candidates(settings,registry,&work->cancel,&work->stage,cache.get());});
}
static void worldChanged(const wa::Json& before){auto& s=worldArchitect;auto active=ImGui::GetActiveID();if(before==wa::serialize(s.settings)){if(!active)s.gesture=0;return;}if(!active||active!=s.gesture){s.undo.push_back(before);if(s.undo.size()>100)s.undo.erase(s.undo.begin());}s.gesture=active;s.redo.clear();s.error.clear();}
static void worldUndo(bool redo=false){auto& s=worldArchitect;auto& from=redo?s.redo:s.undo;auto& to=redo?s.undo:s.redo;if(from.empty()||s.job.valid())return;to.push_back(wa::serialize(s.settings));s.settings=wa::deserialize(from.back(),s.registry);from.pop_back();s.dirty=true;}
static void worldPublish(const std::filesystem::path& root,const std::map<std::string,std::string>& bundle){
  for(const auto& [name,bytes]:bundle){auto target=root/std::filesystem::u8path(name);if(std::filesystem::exists(target)&&q1Read(target)!=bytes)throw std::runtime_error("Existing World Architect asset differs: "+target.u8string());}
  std::set<std::string> shaders;for(const auto& [name,bytes]:bundle)if(name.rfind("scripts/",0)==0&&std::filesystem::u8path(name).extension()==".shader")shaders.insert(std::filesystem::u8path(name).stem().u8string());
  std::vector<std::filesystem::path> added;
  try{for(const auto& [name,bytes]:bundle){auto target=root/std::filesystem::u8path(name);if(!std::filesystem::exists(target)){added.push_back(target);std::filesystem::create_directories(target.parent_path());bb::atomicWrite(target,bytes);}}
    if(!shaders.empty()){auto list=root/"scripts/shaderlist.txt";auto previous=std::filesystem::exists(list)?q1Read(list):std::string{};auto next=ftebroom::appendShaderList(previous,shaders);if(next!=previous)bb::atomicWrite(list,next);}
  }
  catch(...){std::error_code ec;for(const auto& p:added)std::filesystem::remove(p,ec);throw;}
}
static void worldExport(std::filesystem::path target){
  auto& s=worldArchitect;auto* selected=worldSelected();if(!selected||!selected->diagnostics.valid)throw std::runtime_error("Generate and select a valid candidate first");if(s.generated!=worldInputKey())throw std::runtime_error("Settings changed; regenerate before export");target.replace_extension(".map");
  const auto palette=gameProfile==2?q1Palette():ftebroom::defaultQuakePalette();auto bundle=wa::assetBundle(*selected,s.registry,palette);
  bundle[target.filename().u8string()]=wa::withWad(wa::groupedMap(*selected,s.project),*selected,wa::wadFilename(*selected,palette));
  bundle[target.stem().u8string()+".world.json"]=worldProject().dump(2);
  worldPublish(target.parent_path(),bundle);s.message="Exported MAP, editable project and original texture assets.";
}
static void worldApply(bool replace=false,std::optional<uint64_t> region={}){
  auto& s=worldArchitect;auto* selected=worldSelected();
  if(!selected||!selected->diagnostics.valid||s.job.valid()||!s.request.empty())throw std::runtime_error("Select a validated current candidate before Apply");
  if(s.generated!=worldInputKey())throw std::runtime_error("Settings changed; regenerate before Apply");
  if(modEmbedded||documentTerrain||selected->profile!=gameProfile)throw std::runtime_error("Open a brush map with the candidate's game profile before Apply");
  auto importMap=wa::groupedMap(*selected,s.project,region);s.error.clear();
  const auto palette=gameProfile==2?q1Palette():ftebroom::defaultQuakePalette();auto bundle=wa::assetBundle(*selected,s.registry,palette);
  placementPublish(bundle);
  if(gameWad()){
    auto wad=documentPath(wa::wadFilename(*selected,palette)).u8string();std::string list=gameProfile==4?hlWads:q1Wads;
    if(list.find(wad)==list.npos){if(!list.empty())list+='\n';list+=wad;if(list.size()>=sizeof q1Wads)throw std::runtime_error("Configured WAD list is full");std::snprintf(gameProfile==4?hlWads:q1Wads,sizeof q1Wads,"%s",list.c_str());q1FindWads(true);}
  }
  do{s.request=worldIdentity();}while(std::filesystem::exists(documentPath("data/wa-import-"+s.request+".map")));
s.requestMode=replace?"replace":"add";s.requestRegion=region?std::to_string(*region):"all";
s.requestAt=ImGui::GetTime();s.requestPath=documentPath("data/wa-import-"+s.request+".map");
  qhandle_t file;
  if(files->Open(("data/wa-import-"+s.request+".map").c_str(),&file,2)<0){s.request.clear();throw std::runtime_error("Cannot stage World Architect MAP");}
  int written=files->Write(file,importMap.data(),int(importMap.size()));files->Close(file);
  if(written!=int(importMap.size())){s.request.clear();throw std::runtime_error("Incomplete World Architect MAP staging");}
  cvars->SetString("ftebroom_world_apply_ack","");cvars->SetString("ftebroom_world_apply_status","pending");
  s.pendingShaders=selected->profile<2?wa::shaderDefinitions(*selected):std::map<std::string,std::string>{};
  command(("ftebroom_world_architect materials "+s.request+"\n").c_str());
  command(("ftebroom_world_apply "+s.request+" "+std::to_string(int(cvars->GetFloat("ftebroom_map_instance")))+" "+std::to_string(gameProfile)+" "+s.project+" "+s.requestRegion+" "+s.requestMode+"\n").c_str());
  s.message="Apply queued; waiting for the map transaction acknowledgment.";
}
static void worldTick(){
  auto& s=worldArchitect;
  if(s.job.valid()&&s.job.wait_for(std::chrono::seconds(0))==std::future_status::ready){
    worldAction([&]{s.batch=s.job.get();s.generated=s.pendingKey;s.selected=0;s.dirty=true;if(s.desiredSeed){for(size_t i=0;i<s.batch.worlds.size();i++)if(s.batch.worlds[i].seed==*s.desiredSeed)s.selected=int(i);s.desiredSeed.reset();}if(s.batch.worlds.empty())s.error=s.batch.rejects.empty()?"No accepted candidate":"No accepted candidate: "+s.batch.rejects.front();else{s.message=std::to_string(s.batch.worlds.size())+" candidates passed physical validation; "+std::to_string(s.batch.rejects.size())+" rejected.";if(s.open)worldClaim();}});
  }
  if(!s.request.empty()){
    char ack[128]={},status[128]={};cvars->GetString("ftebroom_world_apply_ack",ack,sizeof ack);cvars->GetString("ftebroom_world_apply_status",status,sizeof status);
    if(s.request==ack||ImGui::GetTime()-s.requestAt>20){
      if(s.request==ack&&std::string(status)=="ok")s.message=s.requestMode=="replace"?"Generated regions updated. One map Undo restores their previous contents.":"World inserted. One map Undo removes the complete operation.";
      else if(s.request==ack&&std::string(status)=="modified")s.error="A generated region has manual edits or no ownership baseline. Nothing was replaced; keep it or export the preview.";
      else if(s.request==ack&&std::string(status)=="locked")s.error="A generated region is hidden or locked. Nothing was replaced.";
      else if(s.request==ack&&std::string(status)=="exists")s.error="This project's regions are already on the map. Use Update instead of adding a duplicate.";
      else if(s.request==ack&&std::string(status)=="structure-changed")s.error="The preview has different architecture. Update the complete project first, or reopen its applied project before updating building details.";
      else s.error=s.request==ack?std::string("Apply was rejected: ")+status:"Apply acknowledgment timed out; inspect the map before retrying.";
      s.request.clear();std::error_code ec;std::filesystem::remove(s.requestPath,ec);
    }
  }
}
static std::string worldPreviewBuffer(const wa::WorldPlan& world,int cutaway){
  using namespace ftebroom;std::map<std::string,std::vector<std::array<wa::V,3>>> batches;
  for(const auto& element:world.architecture.elements){
    if(element.kind.find("sealed sky")!=std::string::npos)continue;auto poly=al::polyhedron(element.brush);
    if(cutaway>=0&&(poly.min.z>=double((cutaway+1)*256-20)||poly.max.z<double(cutaway*256-20)))continue;
    for(size_t f=0;f<poly.polygons.size();f++){const auto& polygon=poly.polygons[f];for(size_t i=1;i+1<polygon.size();i++)batches[element.brush.faces[f].material].push_back({polygon[0],polygon[i],polygon[i+1]});}
  }
  unsigned count=0;for(const auto& [name,tris]:batches)count+=unsigned((tris.size()+21844)/21845);std::string out="RBB1";bb::word(out,count);
  for(const auto& [name,tris]:batches)for(size_t start=0;start<tris.size();start+=21845){size_t n=std::min<size_t>(21845,tris.size()-start);bb::word(out,unsigned(n*3));bb::word(out,unsigned(n*3));auto texture="bbtex/"+(name==wa::materialId(world,6)?wa::assetId(world,6):name)+".tga";out+=texture;out.append(128-texture.size(),0);
    for(size_t i=start;i<start+n;i++){
      const auto& triangle=tris[i];auto normal=al::unit(al::cross(triangle[1]-triangle[0],triangle[2]-triangle[0]));float shade=float(.52+.48*std::max(0.,al::dot(normal,al::unit(wa::V{-.4,-.6,1}))));
      for(auto p:triangle){float vertex[9]={float(p.x),float(p.y),float(p.z),float((std::abs(normal.z)>.5?p.x:p.x+p.y)/128.),float((std::abs(normal.z)>.5?p.y:-p.z)/128.),shade,shade,shade,1};out.append(reinterpret_cast<char*>(vertex),sizeof vertex);}
    }for(unsigned i=0;i<n*3;i++)bb::word(out,i);
  }return out;
}
static void worldPreview(){
  auto& s=worldArchitect;auto* world=worldSelected();if(!world)return;
  if(s.owner&&(bbActive||modelViewerOpen||plantState.owner))s.owner=false;
  if(!s.owner){ImGui::TextUnformatted("Another authoring tool owns the preview.");if(worldButton("Resume world preview","resume-preview"))worldClaim();return;}
  auto api=bbAPI();auto images=bbImageAPI();if(!api||!images){ImGui::TextUnformatted("Dynamic preview interface unavailable");return;}
  if(s.dirty){
    bool water=std::any_of(world->architecture.elements.begin(),world->architecture.elements.end(),[](const auto& e){return e.kind=="floodwater";});
    for(int slot=0;slot<(water?7:6);slot++){auto image=wa::texture(s.registry.theme(world->theme),slot);images->upload(("bbtex/"+wa::assetId(*world,slot)+".tga").c_str(),image.rgba.data(),image.width,image.height);}
    auto buffer=worldPreviewBuffer(*world,s.cutaway);if(!api->upload(buffer.data(),buffer.size()))throw std::runtime_error("Engine rejected World Architect preview geometry");s.dirty=false;
  }
  if(worldButton(s.walk?"Orbit view":"Walk from spawn","walk")){s.walk=!s.walk;if(s.walk){s.eye=world->navigation.nodes[world->navigation.mainRoute.front()].feet;s.yaw=0;s.pitch=0;}}ImGui::SameLine();ImGui::Checkbox("Top view",&s.ortho);worldRect("top-view");
  int cut=s.cutaway;ImGui::SameLine();ImGui::SetNextItemWidth(140*uiScale());if(ImGui::SliderInt("Cutaway floor",&cut,-1,4)){s.cutaway=cut;s.dirty=true;}worldRect("cutaway");
  auto size=ImGui::GetContentRegionAvail();size.x=std::max(80.f,size.x);size.y=std::max(100.f,size.y);auto pos=ImGui::GetCursorScreenPos();
  if(!modelViewerImage)modelViewerImage=draw->LoadImageShader("ftebroom/modelviewer-image","{ program default2d { map $rt:ftebroom_modelviewer rgbgen vertex alphagen vertex } }");
  if(modelViewerImage)ImGui::Image((ImTextureID)modelViewerImage,size);else ImGui::Dummy(size);worldRect("preview");
  const bool hovered=ImGui::IsItemHovered();auto& io=ImGui::GetIO();
  if(hovered){
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Right)){s.yaw-=io.MouseDelta.x*.35f;s.pitch=std::clamp(s.pitch+io.MouseDelta.y*.3f,-85.f,85.f);}
    if(!s.walk&&io.MouseWheel)s.zoom=std::clamp(s.zoom*float(std::pow(.88,io.MouseWheel)),.15f,5.f);
    if(s.walk){
      double angle=s.yaw*3.141592653589793/180;wa::V forward{std::cos(angle),std::sin(angle),0},right{-forward.y,forward.x,0},delta{};
      if(ImGui::IsKeyDown(ImGuiKey_W))delta=delta+forward;if(ImGui::IsKeyDown(ImGuiKey_S))delta=delta-forward;if(ImGui::IsKeyDown(ImGuiKey_D))delta=delta+right;if(ImGui::IsKeyDown(ImGuiKey_A))delta=delta-right;
      if(al::length(delta)>.1){delta=al::unit(delta)*std::min(.05f,io.DeltaTime)*180;wa::Collision collision(*world);auto next=collision.move(s.eye,s.eye+delta,s.settings.player);if(next)s.eye=*next;}
    }
  }
  cvars->SetFloat("ftebroom_bb_active",1);cvars->SetFloat("ftebroom_modelviewer_open",1);cvars->SetFloat("ftebroom_modeleditor_active",1);cvars->SetFloat("ftebroom_plant_active",0);
  cvars->SetFloat("ftebroom_modelviewer_width",size.x);cvars->SetFloat("ftebroom_modelviewer_height",size.y);cvars->SetFloat("ftebroom_modelviewer_yaw",s.yaw);cvars->SetFloat("ftebroom_modelviewer_pitch",s.pitch);cvars->SetFloat("ftebroom_modelviewer_zoom",s.zoom);
  cvars->SetString("ftebroom_bb_low",wa::vectorText(world->low).c_str());cvars->SetString("ftebroom_bb_high",wa::vectorText(world->high).c_str());
  cvars->SetFloat("ftebroom_bb_ortho",s.ortho);cvars->SetFloat("ftebroom_bb_cull",0);cvars->SetFloat("ftebroom_modeleditor_floor",0);cvars->SetFloat("ftebroom_modeleditor_axes",0);cvars->SetFloat("ftebroom_modeleditor_player",0);
  cvars->SetFloat("ftebroom_modeleditor_freecamera",s.walk);cvars->SetString("ftebroom_modeleditor_camera",wa::vectorText(s.eye+wa::V{0,0,48}).c_str());
}
static void worldPlanCanvas(bool roads){
  auto& s=worldArchitect;auto* world=worldSelected();if(!world)return;auto pos=ImGui::GetCursorScreenPos(),size=ImGui::GetContentRegionAvail();size.x=std::max(100.f,size.x);size.y=std::max(180.f,size.y);
  ImGui::InvisibleButton("World plan canvas",size);worldRect("plan-canvas");auto* drawList=ImGui::GetWindowDrawList();drawList->AddRectFilled(pos,{pos.x+size.x,pos.y+size.y},IM_COL32(25,32,34,255));
  double scale=std::min((size.x-32)/(world->high.x-world->low.x),(size.y-32)/(world->high.y-world->low.y));
  auto point=[&](wa::V p){return ImVec2{pos.x+16+float((p.x-world->low.x)*scale),pos.y+16+float((p.y-world->low.y)*scale)};};
  for(const auto& road:world->spatial.roads){auto points=wa::roadPoints(road);for(size_t n=1;n<points.size();n++)drawList->AddLine(point(points[n-1]),point(points[n]),IM_COL32(80,84,78,255),float(road.width*scale));if(road.control)drawList->AddCircleFilled(point(*road.control),5,IM_COL32(120,210,240,255));}
  for(const auto& b:world->spatial.buildings){drawList->AddRectFilled(point(b.low),point(b.high),IM_COL32(102,112,94,200));if(s.settings.regionRecipes.count(b.id))drawList->AddRect(point(b.low),point(b.high),IM_COL32(250,210,70,255),0,0,3);for(const auto& r:b.rooms){if(r.floor!=s.planFloor)continue;drawList->AddRect(point(r.low),point(r.high),IM_COL32(170,165,140,255));}}
  auto graph=s.settings.authoredMission?*s.settings.authoredMission:world->mission;
  for(auto& node:graph.nodes){auto old=std::find_if(world->mission.nodes.begin(),world->mission.nodes.end(),[&](const auto& n){return n.id==node.id;});if(old!=world->mission.nodes.end()){node.position=old->position;node.floor=old->floor;}else node.position=(world->low+world->high)*.5;
    for(const auto& pin:s.settings.missionPins)if(pin.id==node.id){node.position=pin.position;node.floor=int(pin.position.z/256);node.pinned=true;}}
  for(const auto& e:graph.edges){const auto find=[&](uint64_t id)->const wa::MissionNode&{for(const auto& n:graph.nodes)if(n.id==id)return n;throw std::runtime_error("Missing mission node");};drawList->AddLine(point(find(e.a).position),point(find(e.b).position),e.optional?IM_COL32(140,150,230,255):IM_COL32(240,180,70,255),2);}
  for(const auto& node:graph.nodes){auto p=point(node.position);drawList->AddCircleFilled(p,node.id==s.missionNode?8.f:5.f,node.pinned?IM_COL32(100,220,180,255):IM_COL32(240,180,70,255));if(node.floor==s.planFloor)drawList->AddText({p.x+7,p.y-8},IM_COL32(240,240,225,255),node.name.c_str());}
  for(const auto& mask:s.settings.influences){if(mask.high.z<s.planFloor*256||mask.low.z>(s.planFloor+1)*256)continue;auto color=mask.kind=="preserve"?IM_COL32(80,220,160,180):mask.kind=="avoid"?IM_COL32(250,80,80,180):IM_COL32(220,140,60,180);drawList->AddRect(point(mask.low),point(mask.high),color,0,0,2);}
  if(!roads&&!s.settings.locked[0]&&!s.job.valid()){
    auto mouse=ImGui::GetIO().MousePos;
    if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(0)){double distance=15;for(const auto& node:graph.nodes){if(node.floor!=s.planFloor)continue;auto p=point(node.position);double d=std::hypot(mouse.x-p.x,mouse.y-p.y);if(d<distance){distance=d;s.dragMission=s.missionNode=node.id;}}}
    if(s.dragMission&&ImGui::IsMouseReleased(0)){
      auto id=s.dragMission;s.dragMission=0;wa::V target{world->low.x+(mouse.x-pos.x-16)/scale,world->low.y+(mouse.y-pos.y-16)/scale,double(s.planFloor*256)+1};
      const wa::Room* room=nullptr;for(const auto& b:world->spatial.buildings)for(const auto& r:b.rooms)if(r.floor==s.planFloor&&target.x>=r.low.x&&target.x<=r.high.x&&target.y>=r.low.y&&target.y<=r.high.y)room=&r;
      if(room&&ImGui::IsMouseDragPastThreshold(0))worldAction([&]{auto next=s.settings;if(!next.authoredMission)next.authoredMission=world->mission;wa::MissionNode pin;pin.id=id;pin.position={(room->low.x+room->high.x)/2,(room->low.y+room->high.y)/2,room->low.z+1};auto found=std::find_if(next.missionPins.begin(),next.missionPins.end(),[&](const auto& n){return n.id==id;});if(found==next.missionPins.end())next.missionPins.push_back(pin);else *found=pin;wa::validate(next,s.registry);auto before=wa::serialize(s.settings);s.settings=std::move(next);worldChanged(before);});
    }
  }
}
#include "ftebroom_world_authoring.h"
static void worldQa(){
  auto& s=worldArchitect;if(!s.qa)return;if(runUiBrowseOpen)for(auto it=runUiBrowseRects.begin();it!=runUiBrowseRects.end();++it)s.widgets[it.key()]=it.value();
  auto& io=ImGui::GetIO();auto& context=*ImGui::GetCurrentContext();static bool previousDown=false;
  if(io.MouseDown[0]!=previousDown||ImGui::IsMouseClicked(0)||ImGui::IsMouseReleased(0)){
    s.inputTrace.push_back({{"frame",ImGui::GetFrameCount()},{"down",io.MouseDown[0]},{"clicked",ImGui::IsMouseClicked(0)},{"released",ImGui::IsMouseReleased(0)},{"pos",{io.MousePos.x,io.MousePos.y}},{"capture",io.WantCaptureMouse},{"focusLost",io.AppFocusLost},{"active",context.ActiveId},{"nav",context.NavWindow?context.NavWindow->Name:""},{"hover",context.HoveredWindow?context.HoveredWindow->Name:""},{"tab",s.tab}});
    if(s.inputTrace.size()>128)s.inputTrace.erase(s.inputTrace.begin());previousDown=io.MouseDown[0];
  }
  char applyAck[128]={},applyStatus[128]={};cvars->GetString("ftebroom_world_apply_ack",applyAck,sizeof applyAck);cvars->GetString("ftebroom_world_apply_status",applyStatus,sizeof applyStatus);
  wa::Json j={{"display",{ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y}},{"applyAck",applyAck},{"applyStatus",applyStatus},{"mapEpoch",int(cvars->GetFloat("ftebroom_map_instance"))},{"open",s.open},{"busy",s.job.valid()},{"settings",wa::serialize(s.settings)},{"widgets",s.widgets},{"error",s.error},{"message",s.message},{"selected",s.selected},{"request",s.request},{"candidates",s.batch.worlds.size()}};
  j["projectId"]=s.project;j["detailRegion"]=s.detailRegion;
  j["legacySaved"]=s.legacySaved;if(s.legacy)j["legacy"]=s.legacy->report;
  j["inputTrace"]=s.inputTrace;
  j["candidateSeeds"]=wa::Json::array();for(const auto& world:s.batch.worlds)j["candidateSeeds"].push_back(world.seed);
  if(auto world=worldSelected())j["world"]=wa::diagnostics(*world);worldAction([&]{bb::atomicWrite(documentPath("world-architect-ui-live.json"),j.dump());});
}
static void worldPanel(){
  auto& s=worldArchitect;worldTick();if(!s.open){worldRelease();worldQa();return;}s.widgets=wa::Json::object();
  worldAction(worldRegistry);if(s.registry.presets.empty()){
    if(ImGui::Begin("World Architect registry error",&s.open)){ImGui::TextWrapped("%s",s.error.c_str());if(ImGui::Button("Reload registry"))worldAction(worldRegistry);}ImGui::End();worldQa();return;
  }
  ImGui::SetNextWindowSize({1500*uiScale(),900*uiScale()},ImGuiCond_FirstUseEver);
  if(s.focusRequest){ImGui::SetNextWindowFocus();s.focusRequest=false;}
  if(ImGui::Begin("World Architect — development preview",&s.open,ImGuiWindowFlags_NoDocking)){
    ImGui::TextWrapped("Development preview: mission / physical navigation foundation. AutoLevel remains available; final architecture and art acceptance is still pending.");
    auto before=wa::serialize(s.settings);bool restoredSettings=false;ImGui::BeginDisabled(s.job.valid());
    const int frozen=wa::lockedThrough(s.settings);
    ImGui::BeginDisabled(frozen>=0);ImGui::SetNextItemWidth(220*uiScale());bool presetsOpen=ImGui::BeginCombo("Preset",s.registry.preset(s.settings.preset).name.c_str());worldRect("preset");if(presetsOpen){for(const auto& p:s.registry.presets){if(ImGui::Selectable(p.name.c_str(),p.id==s.settings.preset)){s.settings.preset=p.id;s.settings.damage=p.damage;}worldRect("preset/"+p.id);}ImGui::EndCombo();}ImGui::EndDisabled();ImGui::SameLine();
    ImGui::SetNextItemWidth(150*uiScale());ImGui::InputScalar("Seed",ImGuiDataType_U64,&s.settings.seed);worldRect("seed");ImGui::SameLine();
    if(worldButton("Generate candidates","generate"))worldAction(worldGenerate);ImGui::SameLine();
    if(worldButton("Undo settings","undo-settings")){worldAction([]{worldUndo();});restoredSettings=true;}ImGui::SameLine();if(worldButton("Redo settings","redo-settings")){worldAction([]{worldUndo(true);});restoredSettings=true;}
    float scale=float(s.settings.scale),damage=float(s.settings.damage),density=float(s.settings.dressing);
    ImGui::BeginDisabled(frozen>=1);ImGui::SetNextItemWidth(150*uiScale());if(ImGui::SliderFloat("Scale",&scale,.6f,1.4f))s.settings.scale=scale;worldRect("scale");ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(frozen>=3);ImGui::SetNextItemWidth(150*uiScale());if(ImGui::SliderFloat("Damage",&damage,0,1))s.settings.damage=damage;worldRect("damage");ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(frozen>=4);ImGui::SetNextItemWidth(150*uiScale());if(ImGui::SliderFloat("Dressing",&density,0,1))s.settings.dressing=density;worldRect("dressing");ImGui::EndDisabled();ImGui::SameLine();
    const char* freezeNames[]={"None","Mission","Site","Architecture","Damage","Dressing"};ImGui::SetNextItemWidth(150*uiScale());bool freezeOpen=ImGui::BeginCombo("Freeze through",freezeNames[frozen+1]);worldRect("freeze");if(freezeOpen){
      for(int i=0;i<6;i++){ImGui::BeginDisabled((i&&!worldSelected())||(i<3&&!s.settings.pinnedRegions.empty()));if(ImGui::Selectable(freezeNames[i],frozen==i-1))worldAction([&]{s.settings=i?wa::freezeThrough(s.settings,*worldSelected(),s.registry,i-1):wa::cleanSettings(s.settings);});worldRect("freeze/"+std::to_string(i));ImGui::EndDisabled();}ImGui::EndCombo();
    }
    if(frozen>=0)ImGui::TextUnformatted("Frozen stages keep their selected candidate recipes; seed changes affect later stages. Navigation and export are always checked.");
    if(!restoredSettings)worldChanged(before);else s.gesture=0;ImGui::EndDisabled();
    if(s.job.valid()){ImGui::ProgressBar((s.work->stage.load()+.5f)/7,{-1,0},wa::stages[std::clamp(s.work->stage.load(),0,6)]);if(worldButton("Cancel generation","cancel"))s.work->cancel=true;}
    if(!s.batch.worlds.empty()){
      ImGui::TextUnformatted("Validated candidates:");ImGui::SameLine();for(size_t i=0;i<s.batch.worlds.size();i++){ImGui::PushID(int(i));auto label=std::to_string(i+1)+" / "+std::to_string(s.batch.worlds[i].seed);if(ImGui::RadioButton(label.c_str(),s.selected==int(i))){s.selected=int(i);s.dirty=true;}worldRect("candidate/"+std::to_string(i));ImGui::SameLine();ImGui::PopID();}ImGui::NewLine();
    }
    if(ImGui::BeginTabBar("World Architect panels")){
      for(int tab=0;tab<9;tab++){const char* names[]={"Overview","Mission Graph","Site & Roads","Buildings","Architecture","Destruction","Dressing","Quality","Export"};bool active=ImGui::BeginTabItem(names[tab]);worldRect(std::string("tab/")+names[tab]);if(!active)continue;s.tab=tab;
        ImGui::BeginChild("World content",{0,-70*uiScale()});
        if(tab==0){
          ImGui::Text("Project profile: %s / active map: %s",wa::capability(s.settings.profile).name,wa::capability(gameProfile).name);
          if(s.settings.profile!=gameProfile&&worldButton("Retarget to the active game profile","retarget")){auto previous=wa::serialize(s.settings);s.settings.profile=gameProfile;worldChanged(previous);}
          if(worldButton("Save editable project...","save-project"))runUiChoose((documentPath("world-architect")/"project.world.json").u8string(),[](const std::string& name){auto path=std::filesystem::u8path(name);std::filesystem::create_directories(path.parent_path());bb::atomicWrite(path,worldProject().dump(2));},true);
          if(worldButton("Open project...","open-project"))runUiChoose(documentPath("world-architect").u8string(),worldLoadProject);
          ImGui::TextWrapped("Legacy AutoLevel settings and maps are preserved. This preview does not overwrite or silently migrate them.");
          worldLegacyPanel();
          worldAction(worldPreview);
        }
        if(tab==1)worldAction(worldMissionPanel);
        if(tab==2)worldAction(worldRoadPanel);
        if(tab==4)worldAction(worldArchitecturePanel);
        if(tab==5)worldAction([]{worldInfluencePanel(false);});
        if(tab==6)worldAction([]{auto& s=worldArchitect;auto before=wa::serialize(s.settings);auto p=wa::stagePreset(s.settings,s.registry);ImGui::BeginDisabled(s.job.valid()||wa::lockedThrough(s.settings)>=2||!s.settings.pinnedRegions.empty());bool themesOpen=ImGui::BeginCombo("Art theme",p.theme.c_str());worldRect("art-theme");if(themesOpen){for(const auto& theme:s.registry.themes){if(ImGui::Selectable(theme.id.c_str(),p.theme==theme.id))s.settings.theme=theme.id;worldRect("art-theme/"+theme.id);}ImGui::EndCombo();}ImGui::EndDisabled();worldChanged(before);worldRoomDressingPanel();worldInfluencePanel(true);});
        if(tab==7){
          worldCandidateComparison();
          if(auto world=worldSelected()){ImGui::Text("%zu brushes / %.2f MiB MAP / %.0f unit route through required mission rooms",world->diagnostics.brushes,world->map.size()/1048576.,world->diagnostics.routeLength);ImGui::Text("%zu / %zu physical navigation nodes reachable",world->navigation.reachable,world->navigation.nodes.size());for(const auto& stage:world->diagnostics.cachedStages)ImGui::Text("Reused frozen stage: %s",stage.c_str());for(const auto& [name,value]:world->diagnostics.score)ImGui::Text("%s: %.3f",name.c_str(),value);for(const auto& [name,ms]:world->diagnostics.timing)ImGui::Text("%s: %.1f ms%s",name.c_str(),ms,std::find(world->diagnostics.cachedStages.begin(),world->diagnostics.cachedStages.end(),name)!=world->diagnostics.cachedStages.end()?" (original build time)":"");for(const auto& warning:world->diagnostics.warnings)ImGui::TextWrapped("%s",warning.c_str());}
          for(const auto& reject:s.batch.rejects)ImGui::TextWrapped("Rejected: %s",reject.c_str());
        }
        if(tab==3){
          if(auto world=worldSelected()){
            if(std::none_of(world->spatial.buildings.begin(),world->spatial.buildings.end(),[&](const auto& b){return b.id==s.detailRegion;}))s.detailRegion=world->spatial.buildings.front().id;
            ImGui::SeparatorText("Building details");ImGui::BeginDisabled(s.job.valid());
            ImGui::SetNextItemWidth(260*uiScale());const auto& current=*std::find_if(world->spatial.buildings.begin(),world->spatial.buildings.end(),[&](const auto& b){return b.id==s.detailRegion;});
            auto label=current.archetype+" / "+std::to_string(current.floors)+" floors / "+std::to_string(current.rooms.size())+" rooms";
            bool buildingsOpen=ImGui::BeginCombo("Building",label.c_str());worldRect("detail-building");if(buildingsOpen){for(size_t i=0;i<world->spatial.buildings.size();i++){const auto& b=world->spatial.buildings[i];auto text="Building "+std::to_string(i+1)+" / "+b.archetype;if(ImGui::Selectable(text.c_str(),s.detailRegion==b.id))s.detailRegion=b.id;worldRect("detail-building/"+std::to_string(b.id));}ImGui::EndCombo();}
            const bool pinned=s.settings.regionRecipes.count(s.detailRegion)!=0;
            ImGui::BeginDisabled(!pinned&&s.generated!=worldInputKey());if(worldButton(pinned?"Unpin selected building":"Pin selected building","pin-building"))worldAction([&]{auto previous=wa::serialize(s.settings);s.settings=pinned?wa::unpinBuilding(s.settings,s.detailRegion):wa::pinBuilding(s.settings,*world,s.registry,s.detailRegion);worldChanged(previous);});ImGui::EndDisabled();
            ImGui::TextWrapped("Pins preserve the accepted building's architecture, damage and dressing. Mission and Site remain frozen to retain its lot and routes. Unpin before changing those stages.");
            ImGui::SetNextItemWidth(180*uiScale());ImGui::InputScalar("Detail seed",ImGuiDataType_U64,&s.detailSeed);worldRect("detail-seed");
            ImGui::BeginDisabled(pinned||s.generated!=worldInputKey());if(worldButton("Preview selected building details","regenerate-region"))worldAction([&]{auto previous=wa::serialize(s.settings);s.settings=wa::regenerateDetails(s.settings,*world,s.registry,s.detailRegion,s.detailSeed);worldChanged(previous);worldGenerate();});ImGui::EndDisabled();ImGui::EndDisabled();
            ImGui::TextWrapped("This keeps the selected candidate's architecture and every other region's detail streams. Review the preview, then update only the selected building in Export.");
          }
        }
        if(tab==8){
          ImGui::BeginDisabled(!worldSelected()||s.job.valid()||!s.request.empty());
          if(worldButton("Export MAP and original assets...","export"))runUiChoose((documentPath("world-architect")/"world.map").u8string(),[](const std::string& path){worldExport(std::filesystem::u8path(path));},true);
          if(worldButton("Apply to current map","apply"))worldAction([]{worldApply();});
          if(worldButton("Update this project's regions","update-world"))worldAction([]{worldApply(true);});
          ImGui::BeginDisabled(!s.detailRegion);if(worldButton("Update selected building only","update-region"))worldAction([&]{worldApply(true,s.detailRegion);});ImGui::EndDisabled();ImGui::EndDisabled();
          ImGui::TextWrapped("Apply adds editable brush geometry at its generated coordinates in one Undo transaction. Existing authored map content is retained. Preview and candidate generation do not change the map.");
          ImGui::TextWrapped("Update replaces only this project's tracked regions. Hidden, locked or manually edited regions are preserved and report an error instead. Each update is one Undo/Redo transaction.");
        }
        ImGui::EndChild();ImGui::EndTabItem();
      }ImGui::EndTabBar();
    }
    if(!s.error.empty())ImGui::TextColored({1,.4f,.3f,1},"%s",s.error.c_str());else ImGui::TextWrapped("%s",s.message.c_str());
    if(!s.generated.empty()&&s.generated!=worldInputKey())ImGui::TextUnformatted("Settings changed: regenerate to apply them to a candidate.");
  }ImGui::End();if(!s.open){if(s.work&&s.job.valid())s.work->cancel=true;worldRelease();}worldQa();
}
static void worldOpen(){worldAction([]{worldRegistry();if(!worldArchitect.open)input->SetMenuFocus(qtrue,"",0,0,1);worldArchitect.open=true;worldArchitect.focusRequest=true;});}
static void QDECL worldCommand(){
  char operation[64]={};commands->Argv(1,operation,sizeof operation);
  if(std::string(operation)=="materials"){
    char request[64]={};commands->Argv(2,request,sizeof request);auto& s=worldArchitect;if(s.request!=request||s.request.empty())return;
    worldAction([&]{for(const auto& [name,body]:s.pendingShaders)if(!s.materialHandles.count(name)){
      auto handle=draw->LoadImageShader(name.c_str(),body.c_str());if(!handle){std::error_code ec;std::filesystem::remove(s.requestPath,ec);throw std::runtime_error("Material registration failed; generated map was not applied");}s.materialHandles[name]=handle;
    }s.pendingShaders.clear();});return;
  }
  worldAction([]{worldRegistry();auto& s=worldArchitect;if(!s.open)input->SetMenuFocus(qtrue,"",0,0,1);s.open=true;s.focusRequest=true;char action[256]={};commands->Argv(1,action,sizeof action);if(std::string(action)=="qa")s.qa=true;else if(std::string(action)=="generate")worldGenerate();else if(*action){s.registry.preset(action);s.settings.preset=action;worldGenerate();}});
}
#ifdef FTEBROOM_WORLD_RESTORE_NEAR
#pragma pop_macro("near")
#undef FTEBROOM_WORLD_RESTORE_NEAR
#endif
