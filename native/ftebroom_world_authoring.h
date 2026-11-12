// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Included after the shared World Architect state, history and plan canvas.
template<class F>static void worldEdit(F edit){
  auto& s=worldArchitect;worldAction([&]{auto next=s.settings;edit(next);wa::validate(next,s.registry);auto previous=wa::serialize(s.settings);s.settings=std::move(next);worldChanged(previous);});
}
static void worldLegacyPanel(){
  auto& s=worldArchitect;bool show=ImGui::CollapsingHeader("Legacy AutoLevel migration");worldRect("legacy-panel");if(!show)return;
  ImGui::TextWrapped("Choose an AutoLevel repository, builder or settings file. Inspect the report, then save its backup and extracted module MAPs. Originals and the active map are retained. Solver settings without an equivalent are reported explicitly.");
  if(worldButton("Inspect legacy file...","legacy-open"))runUiChoose(documentPath("data/plugin-autolevel-settings.txt").u8string(),[](const std::string& name){worldAction([&]{auto path=std::filesystem::u8path(name);if(std::filesystem::file_size(path)>8*1024*1024)throw std::runtime_error("Legacy source exceeds 8 MiB");auto result=wa::migrateLegacy(q1Read(path),path.filename().u8string());worldArchitect.legacy=std::move(result);worldArchitect.legacySaved=false;worldArchitect.error.clear();});});
  if(!s.legacy)return;const auto& report=s.legacy->report;ImGui::Text("Result: %s / %zu source bytes",report.at("status").get<std::string>().c_str(),size_t(report.at("sourceBytes").get<uint64_t>()));
  if(report.contains("diagnostic"))ImGui::TextWrapped("%s",report.at("diagnostic").get<std::string>().c_str());
  for(const auto& warning:report.at("unsupported"))ImGui::BulletText("%s",warning.get<std::string>().c_str());
  ImGui::Text("Module records: %zu",report.at("convertedModules").size());
  if(worldButton("Save migration report and backup...","legacy-save"))runUiChoose((documentPath("world-architect")/("legacy-"+s.legacy->report.at("contentIdentity").get<std::string>())/"report.json").u8string(),[](const std::string& name){worldAction([&]{auto& state=worldArchitect;if(!state.legacy)throw std::runtime_error("No inspected legacy source");auto path=std::filesystem::u8path(name);path.replace_extension(".json");auto bundle=state.legacy->files;if(path.filename()!=std::filesystem::path("report.json")){auto text=bundle.at("report.json");bundle.erase("report.json");bundle[path.filename().u8string()]=std::move(text);}worldPublish(path.parent_path(),bundle);for(const auto& [relative,bytes]:bundle)if(q1Read(path.parent_path()/std::filesystem::u8path(relative))!=bytes)throw std::runtime_error("Migration bundle verification failed");state.legacySaved=true;state.message="Legacy backup and module MAPs saved and verified. Original files and map geometry were retained.";});},true);
  ImGui::BeginDisabled(!s.legacySaved||!s.legacy->seed||s.job.valid());if(worldButton("Copy suggested builder seed","legacy-seed"))worldEdit([&](auto& next){next.seed=*s.legacy->seed;});ImGui::EndDisabled();
}
static void worldArchitecturePanel(){
  auto& s=worldArchitect;const auto p=wa::stagePreset(s.settings,s.registry);auto before=wa::serialize(s.settings);
  ImGui::TextWrapped("These values change the functional floor plan and the generated brushes. Zero-valued dimensions inherit the preset. Unlock Site before editing the architectural program.");
  ImGui::BeginDisabled(s.job.valid()||wa::lockedThrough(s.settings)>=1);
  const char* layouts[]={"spine","ring","atrium","parallel","courtyard","offset","branched"};
  ImGui::SetNextItemWidth(220*uiScale());if(ImGui::BeginCombo("Circulation grammar",p.layout.c_str())){for(auto layout:layouts)if(ImGui::Selectable(layout,s.settings.layout==layout))s.settings.layout=layout;ImGui::EndCombo();}worldRect("architecture-layout");
  int floors=p.floors,rooms=p.rooms;float width=float(p.width),depth=float(p.depth),hall=float(p.hallHalfWidth);
  if(ImGui::SliderInt("Floors",&floors,1,4,"%d",ImGuiSliderFlags_AlwaysClamp))s.settings.floors=floors;worldRect("architecture-floors");
  if(ImGui::SliderInt("Minimum rooms",&rooms,2,16,"%d",ImGuiSliderFlags_AlwaysClamp))s.settings.rooms=rooms;
  if(ImGui::SliderFloat("Building width",&width,768,2560,"%.0f",ImGuiSliderFlags_AlwaysClamp))s.settings.width=width;worldRect("architecture-width");
  if(ImGui::SliderFloat("Building depth",&depth,448,1536,"%.0f",ImGuiSliderFlags_AlwaysClamp))s.settings.depth=depth;
  if(ImGui::SliderFloat("Gallery half-width",&hall,64,224,"%.0f",ImGuiSliderFlags_AlwaysClamp))s.settings.hall=hall;
  if(worldButton("Restore preset architecture","reset-architecture")){s.settings.layout.clear();s.settings.width=s.settings.depth=s.settings.hall=0;s.settings.floors=s.settings.rooms=0;}
  ImGui::EndDisabled();worldChanged(before);
  if(auto world=worldSelected()){ImGui::SeparatorText("Generated functional program");for(const auto& b:world->spatial.buildings){ImGui::Text("Building %llu: %d floors, %zu rooms",static_cast<unsigned long long>(b.id),b.floors,b.rooms.size());for(const auto& room:b.rooms)ImGui::BulletText("Floor %d / %s%s",room.floor+1,room.function.c_str(),room.mission?" / mission room":"");}}
}
static void worldMissionPanel(){
  auto& s=worldArchitect;auto* world=worldSelected();if(!world){ImGui::TextUnformatted("Generate a candidate to edit its mission.");return;}
  ImGui::BeginDisabled(s.job.valid()||s.settings.locked[0]);
  const auto& graph=s.settings.authoredMission?*s.settings.authoredMission:world->mission;
  auto selected=std::find_if(graph.nodes.begin(),graph.nodes.end(),[&](const auto& n){return n.id==s.missionNode;});if(selected==graph.nodes.end()){s.missionNode=graph.nodes.front().id;selected=graph.nodes.begin();}
  if(ImGui::BeginCombo("Mission beat",selected->name.c_str())){for(const auto& n:graph.nodes){auto label=std::to_string(n.id)+" / "+n.name;if(ImGui::Selectable(label.c_str(),s.missionNode==n.id))s.missionNode=n.id;}ImGui::EndCombo();}worldRect("mission-node");
  bool required=selected->required;ImGui::BeginDisabled(selected->role==wa::Role::Start||selected->role==wa::Role::Exit);
  if(ImGui::Checkbox("Required beat",&required))worldEdit([&](auto& next){if(!next.authoredMission)next.authoredMission=world->mission;for(auto& n:next.authoredMission->nodes)if(n.id==s.missionNode)n.required=required;});worldRect("mission-required");ImGui::EndDisabled();
  if(worldButton("Add optional branch from selected beat","mission-add"))worldEdit([&](auto& next){if(!next.authoredMission)next.authoredMission=world->mission;auto& g=*next.authoredMission;uint64_t id=0,exit=0;for(const auto& n:g.nodes){id=std::max(id,n.id);if(n.role==wa::Role::Exit)exit=n.id;}wa::MissionNode n;n.id=id+1;n.name="Designer reward "+std::to_string(n.id);n.role=wa::Role::Reward;n.required=false;g.nodes.push_back(n);g.edges.push_back({s.missionNode,n.id,true,false,"designer branch",0});if(exit!=s.missionNode)g.edges.push_back({n.id,exit,true,false,"designer return",0});});
  ImGui::SameLine();if(worldButton("Unpin beat's room","mission-unpin"))worldEdit([&](auto& next){auto& pins=next.missionPins;pins.erase(std::remove_if(pins.begin(),pins.end(),[&](const auto& n){return n.id==s.missionNode;}),pins.end());});
  // A commit replaces Settings and invalidates references into authoredMission.
  const auto edges=s.settings.authoredMission?s.settings.authoredMission->edges:world->mission.edges;
  if(!edges.empty()){
    s.missionEdge=std::clamp(s.missionEdge,0,int(edges.size()-1));const auto edge=edges[size_t(s.missionEdge)];auto label=std::to_string(edge.a)+" -> "+std::to_string(edge.b)+" / "+edge.purpose;
    if(ImGui::BeginCombo("Transition",label.c_str())){for(size_t i=0;i<edges.size();i++){auto text=std::to_string(i+1)+": "+std::to_string(edges[i].a)+" -> "+std::to_string(edges[i].b);if(ImGui::Selectable(text.c_str(),s.missionEdge==int(i)))s.missionEdge=int(i);}ImGui::EndCombo();}
    bool oneWay=edge.oneWay;if(ImGui::Checkbox("One-way mission transition",&oneWay))worldEdit([&](auto& next){if(!next.authoredMission)next.authoredMission=world->mission;next.authoredMission->edges[size_t(s.missionEdge)].oneWay=oneWay;});worldRect("mission-one-way");
    if(worldButton("Remove transition","mission-remove-edge"))worldEdit([&](auto& next){if(!next.authoredMission)next.authoredMission=world->mission;auto& e=next.authoredMission->edges;e.erase(e.begin()+s.missionEdge);});
    ImGui::TextWrapped("Transitions constrain the validated mission traversal. They do not yet create physical engine gate entities. Edits that disconnect required beats are rejected.");
  }
  ImGui::EndDisabled();ImGui::SetNextItemWidth(120*uiScale());ImGui::SliderInt("Plan floor",&s.planFloor,0,3);worldRect("plan-floor");
  ImGui::TextWrapped("Drag a beat onto a functional room on this floor. It snaps to that room's player anchor; other unpinned beats are reassigned around it. Unlock Mission to edit.");
  worldPlanCanvas(false);
}
static void worldRoadPanel(){
  auto& s=worldArchitect;auto* world=worldSelected();if(!world)return;
  ImGui::TextWrapped("Road segments create real street/curb brushes and navigation links. The generator rejects intersections with building envelopes, disconnected routes and roads outside the sealed site.");
  ImGui::BeginDisabled(s.job.valid()||wa::lockedThrough(s.settings)>=1);
  if(!world->spatial.roads.empty()){
    auto selected=std::find_if(world->spatial.roads.begin(),world->spatial.roads.end(),[&](const auto& r){return r.id==s.roadSelection;});if(selected==world->spatial.roads.end()){s.roadSelection=world->spatial.roads.front().id;selected=world->spatial.roads.begin();}
    auto label=std::to_string(selected->id)+" / "+selected->hierarchy;if(ImGui::BeginCombo("Road",label.c_str())){for(const auto& road:world->spatial.roads){auto text=std::to_string(road.id)+" / "+road.hierarchy;if(ImGui::Selectable(text.c_str(),road.id==s.roadSelection))s.roadSelection=road.id;}ImGui::EndCombo();}
    auto road=*selected;for(const auto& pin:s.settings.roadPins)if(pin.id==road.id)road=pin;
    float a[]={float(road.a.x),float(road.a.y)},b[]={float(road.b.x),float(road.b.y)},width=float(road.width);bool changed=false;
    if(ImGui::InputFloat2("Start XY",a,"%.0f",ImGuiInputTextFlags_EnterReturnsTrue)){road.a={a[0],a[1],0};changed=true;}worldRect("road-start");
    if(ImGui::InputFloat2("End XY",b,"%.0f",ImGuiInputTextFlags_EnterReturnsTrue)){road.b={b[0],b[1],0};changed=true;}
    if(ImGui::SliderFloat("Road width",&width,48,384,"%.0f",ImGuiSliderFlags_AlwaysClamp)){road.width=width;changed=true;}worldRect("road-width");
    if(changed)worldEdit([&](auto& next){road.pinned=true;auto& pins=next.roadPins;auto found=std::find_if(pins.begin(),pins.end(),[&](const auto& p){return p.id==road.id;});if(found==pins.end())pins.push_back(road);else *found=road;});
    if(worldButton("Release road override","road-unpin"))worldEdit([&](auto& next){auto& pins=next.roadPins;pins.erase(std::remove_if(pins.begin(),pins.end(),[&](const auto& p){return p.id==s.roadSelection;}),pins.end());});
  }else ImGui::TextUnformatted("Choose a city or mixed preset for street authoring.");
  ImGui::EndDisabled();worldPlanCanvas(true);
}
static void worldInfluencePanel(bool dressing){
  auto& s=worldArchitect;auto before=wa::serialize(s.settings);const char* kinds[]={"damage","preserve","avoid","dressing"};
  ImGui::BeginDisabled(s.job.valid()||wa::lockedThrough(s.settings)>=(dressing?4:2));
  ImGui::TextWrapped("Drag a rectangle on the plan to paint a bounded influence volume for the selected floor. Preserve suppresses damage and props; avoid rejects overlapping building envelopes. These fields are part of the saved project.");
  ImGui::Combo("Field type",&s.paintType,kinds,4);worldRect("mask-type");ImGui::SliderFloat("Field strength",&s.paintStrength,0,1);worldRect("mask-strength");ImGui::SliderInt("Field floor",&s.planFloor,0,3);
  if(worldButton("Preserve selected building","mask-preserve-building"))worldAction([&]{auto* world=worldSelected();if(!world)return;auto found=std::find_if(world->spatial.buildings.begin(),world->spatial.buildings.end(),[&](const auto& b){return b.id==s.detailRegion;});if(found==world->spatial.buildings.end())found=world->spatial.buildings.begin();uint64_t id=1;for(const auto& mask:s.settings.influences)id=std::max(id,mask.id+1);s.settings.influences.push_back({id,found->low-wa::V{8,8,8},found->high+wa::V{8,8,8},"preserve",1});});
  if(!s.settings.influences.empty()){
    s.maskSelection=std::clamp(s.maskSelection,0,int(s.settings.influences.size()-1));const auto mask=s.settings.influences[size_t(s.maskSelection)];auto label=std::to_string(mask.id)+" / "+mask.kind;
    if(ImGui::BeginCombo("Saved fields",label.c_str())){for(size_t i=0;i<s.settings.influences.size();i++){const auto& m=s.settings.influences[i];auto name=std::to_string(m.id)+" / "+m.kind;if(ImGui::Selectable(name.c_str(),s.maskSelection==int(i)))s.maskSelection=int(i);}ImGui::EndCombo();}
    if(worldButton("Remove selected field","mask-remove"))s.settings.influences.erase(s.settings.influences.begin()+s.maskSelection);
  }
  ImGui::EndDisabled();worldChanged(before);
  // Canvas input uses the same active ImGui item and actual pointer events.
  auto origin=ImGui::GetCursorScreenPos();worldPlanCanvas(true);auto rectMin=ImGui::GetItemRectMin(),rectMax=ImGui::GetItemRectMax();auto* world=worldSelected();if(!world)return;
  double scale=std::min((rectMax.x-rectMin.x-32)/(world->high.x-world->low.x),(rectMax.y-rectMin.y-32)/(world->high.y-world->low.y));
  auto worldPoint=[&](ImVec2 p){return wa::V{world->low.x+(p.x-origin.x-16)/scale,world->low.y+(p.y-origin.y-16)/scale,double(s.planFloor*256)};};
  bool allowed=!s.job.valid()&&wa::lockedThrough(s.settings)<(dressing?4:2);
  if(allowed&&ImGui::IsItemHovered()&&ImGui::IsMouseClicked(0)){s.painting=true;s.paintStart=worldPoint(ImGui::GetIO().MousePos);}
  if(s.painting&&ImGui::IsMouseReleased(0)){s.painting=false;auto end=worldPoint(ImGui::GetIO().MousePos);auto low=wa::V{std::min(end.x,s.paintStart.x),std::min(end.y,s.paintStart.y),double(s.planFloor*256)-1},high=wa::V{std::max(end.x,s.paintStart.x),std::max(end.y,s.paintStart.y),double((s.planFloor+1)*256)};
    if(high.x-low.x>=16&&high.y-low.y>=16)worldEdit([&](auto& next){uint64_t id=1;for(const auto& m:next.influences)id=std::max(id,m.id+1);next.influences.push_back({id,low,high,kinds[s.paintType],s.paintStrength});});
  }
}
