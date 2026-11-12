// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Included after the shared World Architect state, history and plan canvas.
template<class F>static void worldEdit(F edit){
  auto& s=worldArchitect;worldAction([&]{auto next=s.settings;edit(next);wa::validate(next,s.registry);auto previous=wa::serialize(s.settings);s.settings=std::move(next);worldChanged(previous);});
}
static std::optional<wa::V> worldVectorInput(const char* label,wa::V value,int dimensions,const std::string& key){
  auto& draft=worldArchitect.vectorDrafts[key];if(!draft.second)draft.first={float(value.x),float(value.y),float(value.z)};
  if(dimensions==2)ImGui::InputFloat2(label,draft.first.data(),"%.0f");else ImGui::InputFloat3(label,draft.first.data(),"%.0f");worldRect(key);
  // This ImGui version forbids EnterReturnsTrue on numeric scalar/vector
  // widgets. Retain the edit draft and commit the group when editing ends.
  bool commit=ImGui::IsItemDeactivatedAfterEdit();draft.second=ImGui::IsItemActive();if(commit)return wa::V{draft.first[0],draft.first[1],dimensions==2?value.z:draft.first[2]};return {};
}
static void worldImportModules(const std::vector<wa::AuthoredModule>& modules){
  if(worldArchitect.job.valid()||worldArchitect.settings.locked[4])throw std::runtime_error("Finish generation and unlock Dressing before importing modules");
  worldEdit([&](auto& next){for(const auto& module:modules){auto found=std::find_if(next.modules.begin(),next.modules.end(),[&](const auto& m){return m.id==module.id;});if(found==next.modules.end())next.modules.push_back(module);else if(wa::moduleJson(*found)!=wa::moduleJson(module))throw std::runtime_error("An imported module with this ID already has different source bytes; use a distinct ID");}});
}
static void worldCandidateComparison(){
  auto& s=worldArchitect;if(s.batch.worlds.empty())return;
  ImGui::SeparatorText("Compare validated candidates");
  ImGui::TextWrapped("Each plan shows the candidate's actual building envelopes, room partitions, streets and validated player route. Select a plan to inspect its 3D geometry in Overview.");
  const int count=std::min(4,int(s.batch.worlds.size()));
  if(ImGui::BeginTable("Candidate comparison",count,ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_SizingStretchSame)){
    for(int i=0;i<count;i++){
      ImGui::TableNextColumn();ImGui::PushID(i);const auto& w=s.batch.worlds[size_t(i)];
      auto label="Select seed "+std::to_string(w.seed);if(worldButton(label.c_str(),("compare-select/"+std::to_string(i)).c_str())){s.selected=i;s.previewIdentity.clear();s.dirty=true;}
      if(i==s.selected)ImGui::TextUnformatted("Selected");else ImGui::TextUnformatted(" ");
      auto pos=ImGui::GetCursorScreenPos();ImVec2 size{std::max(48.f,ImGui::GetContentRegionAvail().x),180*uiScale()};ImGui::InvisibleButton("Candidate spatial plan",size);worldRect("compare-plan/"+std::to_string(i));
      auto* drawList=ImGui::GetWindowDrawList();drawList->AddRectFilled(pos,{pos.x+size.x,pos.y+size.y},IM_COL32(25,32,34,255));
      double scale=std::min((size.x-12)/(w.high.x-w.low.x),(size.y-12)/(w.high.y-w.low.y));
      auto point=[&](wa::V p){return ImVec2{pos.x+6+float((p.x-w.low.x)*scale),pos.y+6+float((p.y-w.low.y)*scale)};};
      for(const auto& road:w.spatial.roads){auto points=wa::roadPoints(road);for(size_t n=1;n<points.size();n++)drawList->AddLine(point(points[n-1]),point(points[n]),IM_COL32(100,104,98,255),std::max(1.f,float(road.width*scale)));}
      for(const auto& b:w.spatial.buildings){drawList->AddRectFilled(point(b.low),point(b.high),IM_COL32(102,112,94,220));for(const auto& room:b.rooms)if(!room.floor)drawList->AddRect(point(room.low),point(room.high),IM_COL32(180,180,160,255));}
      for(size_t n=1;n<w.navigation.mainRoute.size();n++)drawList->AddLine(point(w.navigation.nodes[w.navigation.mainRoute[n-1]].feet),point(w.navigation.nodes[w.navigation.mainRoute[n]].feet),IM_COL32(250,190,70,255),1.5f);
      ImGui::Text("%zu brushes / %.0f route units",w.diagnostics.brushes,w.diagnostics.routeLength);
      for(const auto* name:{"weighted","variety","route","circulation","visibility","cover","verticality"}){auto found=w.diagnostics.score.find(name);if(found!=w.diagnostics.score.end())ImGui::Text("%s: %.2f",name,found->second);}
      if(ImGui::TreeNode("All measurements")){for(const auto& [name,value]:w.diagnostics.score)ImGui::Text("%s: %.2f",name.c_str(),value);ImGui::TreePop();}
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}
static void worldQualityControls(){
  auto& s=worldArchitect;bool show=ImGui::CollapsingHeader("Candidate count and quality weights");worldRect("quality-controls");if(!show)return;
  auto before=wa::serialize(s.settings);ImGui::BeginDisabled(s.job.valid());ImGui::SetNextItemWidth(180*uiScale());ImGui::SliderInt("Candidates per batch",&s.settings.candidates,1,16,"%d",ImGuiSliderFlags_AlwaysClamp);worldRect("candidate-count");
  for(const auto* name:{"variety","route","circulation","visibility","cover","verticality"}){float value=float(s.settings.weights.count(name)?s.settings.weights.at(name):0);ImGui::SetNextItemWidth(220*uiScale());if(ImGui::SliderFloat(name,&value,0,10,"%.2f",ImGuiSliderFlags_AlwaysClamp))s.settings.weights[name]=value;worldRect(std::string("quality-weight/")+name);}
  ImGui::EndDisabled();worldChanged(before);
  ImGui::TextWrapped("Weights rank accepted candidates; they never relax collision, support, route or export validity. Existing projects retain their stored weights. Regenerate after changing weights.");
  ImGui::BeginDisabled(s.batch.worlds.empty()||s.generated!=worldInputKey());if(worldButton("Select best accepted candidate","select-best")){s.selected=0;s.previewIdentity.clear();s.dirty=true;}ImGui::EndDisabled();
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
  const auto entries=report.at("convertedModules");if(s.legacySaved&&!entries.empty()){
    s.legacyModule=std::clamp(s.legacyModule,0,int(entries.size()-1));auto entry=entries[size_t(s.legacyModule)];auto label=entry.value("name",std::string("Unnamed module"));
    if(ImGui::BeginCombo("Extracted module",label.c_str())){for(size_t i=0;i<entries.size();i++)if(ImGui::Selectable(entries[i].value("name",std::to_string(i)).c_str(),s.legacyModule==int(i)))s.legacyModule=int(i);ImGui::EndCombo();}worldRect("legacy-module");
    ImGui::SliderInt("Map original materials to theme slot",&s.legacyMaterial,0,5);worldRect("legacy-material");
    ImGui::BeginDisabled(s.job.valid()||s.settings.locked[4]||!entry.contains("file")||!entry.contains("materialDependencies"));
    if(worldButton("Import selected legacy brushes as a module","legacy-use-module"))worldAction([&]{wa::AuthoredModule module;module.id="legacy-"+report.at("contentIdentity").get<std::string>()+"-"+std::to_string(s.legacyModule);module.function="any";module.profile=s.settings.profile;module.source=report.at("sourceName");module.license="License unspecified in the original AutoLevel repository; source bytes retained";module.map=s.legacy->files.at(entry.at("file").get<std::string>());for(const auto& material:entry.at("materialDependencies"))module.materials[material.get<std::string>()]=s.legacyMaterial;worldImportModules(wa::readModulePack({{"schemaVersion",1},{"modules",wa::Json::array({wa::moduleJson(module)})}}));});
    ImGui::EndDisabled();ImGui::TextWrapped("This explicitly aliases the selected module's materials to the chosen portable theme slot. The original MAP and its material names remain in the saved source. Assign the imported module to a compatible room in Dressing.");
  }

}
static void worldRoomDressingPanel(){
  auto& s=worldArchitect;auto* world=worldSelected();if(!world)return;
  ImGui::SeparatorText("Local room furnishing");
  std::vector<const wa::Room*> rooms;for(const auto& b:world->spatial.buildings)for(const auto& room:b.rooms)rooms.push_back(&room);
  if(rooms.empty())return;auto current=std::find_if(rooms.begin(),rooms.end(),[&](auto r){return r->id==s.detailRoom;});if(current==rooms.end()){current=rooms.begin();s.detailRoom=(*current)->id;}
  auto label=std::to_string(s.detailRoom)+" / "+(*current)->function;
  if(ImGui::BeginCombo("Furnishing room",label.c_str())){for(auto r:rooms){auto name=std::to_string(r->id)+" / floor "+std::to_string(r->floor+1)+" / "+r->function;if(ImGui::Selectable(name.c_str(),r->id==s.detailRoom))s.detailRoom=r->id;worldRect("furnishing-room/"+std::to_string(r->id));}ImGui::EndCombo();}worldRect("furnishing-room");
  ImGui::SetNextItemWidth(180*uiScale());ImGui::InputScalar("Room furnishing seed",ImGuiDataType_U64,&s.detailSeed);worldRect("room-seed");
  ImGui::BeginDisabled(s.job.valid()||s.generated!=worldInputKey()||s.settings.regionRecipes.count(s.detailRoom/10000));
  if(worldButton("Preview this room's furnishing","regenerate-room"))worldAction([&]{auto before=wa::serialize(s.settings);s.settings=wa::regenerateRoomDressing(s.settings,*world,s.registry,s.detailRoom,s.detailSeed);s.detailRegion=s.detailRoom/10000;worldChanged(before);worldGenerate();});ImGui::EndDisabled();
  ImGui::TextWrapped("Keeps mission, architecture, damage and every other room unchanged. Review the preview, then Update selected building in Export. Settings Undo restores the previous room seed.");
}
static void worldModulePanel(){
  auto& s=worldArchitect;bool show=ImGui::CollapsingHeader("Authored brush modules");worldRect("module-panel");if(!show)return;
  ImGui::TextWrapped("Import a v1 module pack with source/license metadata and explicit theme material aliases. Assignment preserves brush shapes and UVs, checks room fit and reserved player routes, and embeds the original MAP in the project/export.");
  ImGui::BeginDisabled(s.job.valid()||s.settings.locked[4]);
  if(worldButton("Import authored module pack...","module-open"))runUiChoose(documentPath("world-architect").u8string(),[](const std::string& name){worldAction([&]{auto path=std::filesystem::u8path(name);if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("Module pack exceeds 1 MiB");worldImportModules(wa::readModulePack(wa::Json::parse(q1Read(path))));});});
  ImGui::BeginDisabled(!s.detailRoom||s.settings.regionRecipes.count(s.detailRoom/10000));
  auto found=s.settings.roomModules.find(s.detailRoom);auto selected=found==s.settings.roomModules.end()?std::string("Procedural furnishings"):found->second;
  if(ImGui::BeginCombo("Room module",selected.c_str())){
    if(ImGui::Selectable("Procedural furnishings",found==s.settings.roomModules.end()))worldEdit([&](auto& next){next.roomModules.erase(s.detailRoom);});worldRect("module/none");
    const auto modules=s.settings.modules;for(const auto& module:modules){ImGui::BeginDisabled(module.profile!=s.settings.profile);if(ImGui::Selectable(module.id.c_str(),selected==module.id))worldEdit([&](auto& next){next.roomModules[s.detailRoom]=module.id;});worldRect("module/"+module.id);ImGui::EndDisabled();}
    ImGui::EndCombo();
  }worldRect("module-choice");
  ImGui::EndDisabled();ImGui::EndDisabled();
  if(s.settings.locked[4])ImGui::TextUnformatted("Unlock Dressing before importing or assigning modules.");
  ImGui::Text("%zu imported modules / %zu room assignments",s.settings.modules.size(),s.settings.roomModules.size());
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
  bool experienceOpen=ImGui::CollapsingHeader("Mission experience controls");worldRect("experience-controls");if(experienceOpen){
    auto before=wa::serialize(s.settings);auto preset=wa::stagePreset(s.settings,s.registry);int beats=preset.experience.beats,branches=preset.experience.branches,loops=preset.experience.loops;
    ImGui::BeginDisabled(s.settings.authoredMission.has_value());if(ImGui::SliderInt("Main beats",&beats,5,12,"%d",ImGuiSliderFlags_AlwaysClamp))s.settings.missionBeats=beats;worldRect("mission-beats");if(ImGui::SliderInt("Optional branches",&branches,0,4,"%d",ImGuiSliderFlags_AlwaysClamp))s.settings.optionalBranches=branches;worldRect("mission-branches");if(ImGui::SliderInt("Return shortcuts",&loops,0,4,"%d",ImGuiSliderFlags_AlwaysClamp))s.settings.missionLoops=loops;worldRect("mission-loops");ImGui::EndDisabled();worldChanged(before);
    ImGui::TextWrapped("These counts generate mission topology before room allocation. Explicitly authored graphs retain their own topology. Regenerate to inspect the resulting physical plan; a graph shortcut does not by itself certify a useful physical loop.");
  }
  const auto graph=s.settings.authoredMission?*s.settings.authoredMission:world->mission;
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
    bool gatesOpen=ImGui::CollapsingHeader("Gate prerequisites");worldRect("gate-panel");if(gatesOpen){
    std::string activation="None";for(const auto& node:graph.nodes)if(node.id==edge.prerequisite)activation=std::to_string(node.id)+" / "+node.name;
    if(ImGui::BeginCombo("Destination activation beat",activation.c_str())){
      if(ImGui::Selectable("None",!edge.prerequisite))worldEdit([&](auto& next){if(!next.authoredMission)next.authoredMission=world->mission;for(auto& transition:next.authoredMission->edges)if(transition.b==edge.b)transition.prerequisite=0;});worldRect("gate-prerequisite/none");
      for(const auto& node:graph.nodes){ImGui::BeginDisabled(node.id==edge.b);auto name=std::to_string(node.id)+" / "+node.name;if(ImGui::Selectable(name.c_str(),node.id==edge.prerequisite))worldEdit([&](auto& next){if(!next.authoredMission)next.authoredMission=world->mission;for(auto& transition:next.authoredMission->edges)if(transition.b==edge.b)transition.prerequisite=node.id;});worldRect("gate-prerequisite/"+std::to_string(node.id));ImGui::EndDisabled();}ImGui::EndCombo();
    }worldRect("gate-prerequisite");
    ImGui::BeginDisabled(wa::lockedThrough(s.settings)>=2||!s.settings.pinnedRegions.empty());if(ImGui::BeginCombo("Gate realization",s.settings.gateMode.c_str())){for(const auto* mode:{"auto","semantic","standard"}){if(ImGui::Selectable(mode,s.settings.gateMode==mode))worldEdit([&](auto& next){next.gateMode=mode;});worldRect(std::string("gate-mode/")+mode);}ImGui::EndCombo();}worldRect("gate-mode");ImGui::EndDisabled();
    ImGui::TextWrapped("Activation locks the destination room across all incoming transitions. Auto uses standard doors/zones for Quake 1 and Quake III; FTE Custom defaults to semantic metadata. Choose standard only for compatible func_door / trigger_multiple game code. One-way flags constrain mission traversal. Invalid or multi-entrance gate layouts are rejected.");
    }
  }
  ImGui::EndDisabled();ImGui::SetNextItemWidth(120*uiScale());ImGui::SliderInt("Plan floor",&s.planFloor,0,4);worldRect("plan-floor");
  ImGui::TextWrapped("Drag a beat onto a functional room on this floor. It snaps to that room's player anchor; other unpinned beats are reassigned around it. Unlock Mission to edit.");
  worldPlanCanvas(false);
}
static void worldRoadPanel(){
  auto& s=worldArchitect;auto* world=worldSelected();if(!world)return;
  ImGui::TextWrapped("Drag in empty plan space to sketch a pedestrian road; drag an endpoint or blue Bezier control to edit a segment. Blue lines show edited recipes; Generate validates and builds their street/curb brushes and navigation. Building intersections, disconnected routes and edits outside the sealed site are rejected.");
  ImGui::BeginDisabled(s.job.valid()||wa::lockedThrough(s.settings)>=1);
  if(ImGui::BeginCombo("Road pattern",wa::stagePreset(s.settings,s.registry).roadPattern.c_str())){for(const auto* pattern:{"grid","organic","hybrid","radial"}){if(ImGui::Selectable(pattern,s.settings.roadPattern==pattern))worldEdit([&](auto& next){next.roadPattern=pattern;next.roadPins.clear();});worldRect(std::string("road-pattern/")+pattern);}ImGui::EndCombo();}worldRect("road-pattern");
  if(!world->spatial.roads.empty()){
    auto selected=std::find_if(world->spatial.roads.begin(),world->spatial.roads.end(),[&](const auto& r){return r.id==s.roadSelection;});if(selected==world->spatial.roads.end()){s.roadSelection=world->spatial.roads.front().id;selected=world->spatial.roads.begin();}
    auto label=std::to_string(selected->id)+" / "+selected->hierarchy;if(ImGui::BeginCombo("Road",label.c_str())){for(const auto& road:world->spatial.roads){auto text=std::to_string(road.id)+" / "+road.hierarchy;if(ImGui::Selectable(text.c_str(),road.id==s.roadSelection))s.roadSelection=road.id;}ImGui::EndCombo();}
    auto road=*selected;for(const auto& pin:s.settings.roadPins)if(pin.id==road.id)road=pin;
    float width=float(road.width);bool changed=false;
    if(auto value=worldVectorInput("Start XY",road.a,2,"road-start")){road.a=*value;changed=true;}
    if(auto value=worldVectorInput("End XY",road.b,2,"road-end")){road.b=*value;changed=true;}
    if(ImGui::SliderFloat("Road width",&width,48,384,"%.0f",ImGuiSliderFlags_AlwaysClamp)){road.width=width;changed=true;}worldRect("road-width");
    bool curved=bool(road.control);if(ImGui::Checkbox("Bezier road curve",&curved)){road.control=curved?std::optional<wa::V>{(road.a+road.b)*.5}:std::nullopt;changed=true;}worldRect("road-curved");
    if(road.control)if(auto value=worldVectorInput("Curve control XY",*road.control,2,"road-control")){road.control=*value;changed=true;}
    if(changed)worldEdit([&](auto& next){road.pinned=true;auto& pins=next.roadPins;auto found=std::find_if(pins.begin(),pins.end(),[&](const auto& p){return p.id==road.id;});if(found==pins.end())pins.push_back(road);else *found=road;});
    if(worldButton("Release road override","road-unpin"))worldEdit([&](auto& next){auto& pins=next.roadPins;pins.erase(std::remove_if(pins.begin(),pins.end(),[&](const auto& p){return p.id==s.roadSelection;}),pins.end());});
  }else ImGui::TextUnformatted("Choose a city or mixed preset for street authoring.");
  ImGui::EndDisabled();worldPlanCanvas(true);
}
static void worldInfluencePanel(bool dressing){
  auto& s=worldArchitect;auto before=wa::serialize(s.settings);const char* kinds[]={"damage","preserve","avoid","dressing","must-pass","vertical-link","no-generation"};
  int constraintStage=s.paintType>=4?1:(dressing?4:2);
  ImGui::BeginDisabled(s.job.valid());
  ImGui::TextWrapped("Drag a rectangle on the plan to paint a bounded influence volume for the selected floor. Preserve suppresses damage and props; avoid rejects overlapping building envelopes. Must-pass adds a required player stop. Vertical-link requires an existing continuous stair route spanning the exact XYZ bounds; set a height of at least 256. No-generation rejects content inside its bounds, retaining the shared ground and sky seal. These hard constraints are frozen with Site.");
  ImGui::Combo("Field type",&s.paintType,kinds,7);worldRect("mask-type");constraintStage=s.paintType>=4?1:(dressing?4:2);ImGui::BeginDisabled(wa::lockedThrough(s.settings)>=constraintStage);ImGui::BeginDisabled(s.paintType>=4);ImGui::SliderFloat("Field strength",&s.paintStrength,0,1);worldRect("mask-strength");ImGui::EndDisabled();ImGui::SliderInt("Field floor",&s.planFloor,0,4);
  if(worldButton("Preserve selected building","mask-preserve-building"))worldAction([&]{auto* world=worldSelected();if(!world)return;auto found=std::find_if(world->spatial.buildings.begin(),world->spatial.buildings.end(),[&](const auto& b){return b.id==s.detailRegion;});if(found==world->spatial.buildings.end())found=world->spatial.buildings.begin();uint64_t id=1;for(const auto& mask:s.settings.influences)id=std::max(id,mask.id+1);s.settings.influences.push_back({id,found->low-wa::V{8,8,8},found->high+wa::V{8,8,8},"preserve",1});});
  if(!s.settings.influences.empty()){
    s.maskSelection=std::clamp(s.maskSelection,0,int(s.settings.influences.size()-1));const auto mask=s.settings.influences[size_t(s.maskSelection)];auto label=std::to_string(mask.id)+" / "+mask.kind;
    if(ImGui::BeginCombo("Saved fields",label.c_str())){for(size_t i=0;i<s.settings.influences.size();i++){const auto& m=s.settings.influences[i];auto name=std::to_string(m.id)+" / "+m.kind;if(ImGui::Selectable(name.c_str(),s.maskSelection==int(i)))s.maskSelection=int(i);}ImGui::EndCombo();}
    const int savedStage=(mask.kind=="must-pass"||mask.kind=="vertical-link"||mask.kind=="no-generation")?1:(mask.kind=="dressing"?4:2);ImGui::BeginDisabled(wa::lockedThrough(s.settings)>=savedStage);
    if(auto value=worldVectorInput("Volume minimum XYZ",mask.low,3,"volume-min"))worldAction([&]{auto next=s.settings;next.influences[size_t(s.maskSelection)].low=*value;wa::validate(next,s.registry);s.settings=std::move(next);});
    if(auto value=worldVectorInput("Volume maximum XYZ",mask.high,3,"volume-max"))worldAction([&]{auto next=s.settings;next.influences[size_t(s.maskSelection)].high=*value;wa::validate(next,s.registry);s.settings=std::move(next);});
    if(worldButton("Remove selected field","mask-remove"))s.settings.influences.erase(s.settings.influences.begin()+s.maskSelection);ImGui::EndDisabled();
  }
  ImGui::EndDisabled();ImGui::EndDisabled();worldChanged(before);
  // Canvas input uses the same active ImGui item and actual pointer events.
  auto origin=ImGui::GetCursorScreenPos();worldPlanCanvas(true);auto rectMin=ImGui::GetItemRectMin(),rectMax=ImGui::GetItemRectMax();auto* world=worldSelected();if(!world)return;
  double scale=std::min((rectMax.x-rectMin.x-32)/(world->high.x-world->low.x),(rectMax.y-rectMin.y-32)/(world->high.y-world->low.y));
  auto worldPoint=[&](ImVec2 p){return wa::V{world->low.x+(p.x-origin.x-16)/scale,world->low.y+(p.y-origin.y-16)/scale,double(s.planFloor*256)};};
  bool allowed=!s.job.valid()&&wa::lockedThrough(s.settings)<constraintStage;
  if(allowed&&ImGui::IsItemHovered()&&ImGui::IsMouseClicked(0)){s.painting=true;s.paintStart=worldPoint(ImGui::GetIO().MousePos);}
  if(s.painting&&ImGui::IsMouseReleased(0)){s.painting=false;auto end=worldPoint(ImGui::GetIO().MousePos);auto low=wa::V{std::min(end.x,s.paintStart.x),std::min(end.y,s.paintStart.y),double(s.planFloor*256)-1},high=wa::V{std::max(end.x,s.paintStart.x),std::max(end.y,s.paintStart.y),double((s.planFloor+1)*256)};
    if(high.x-low.x>=16&&high.y-low.y>=16)worldEdit([&](auto& next){uint64_t id=1;for(const auto& m:next.influences)id=std::max(id,m.id+1);next.influences.push_back({id,low,high,kinds[s.paintType],s.paintStrength});});
  }
}
