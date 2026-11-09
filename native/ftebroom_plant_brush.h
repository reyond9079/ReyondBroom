// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

static constexpr int plantBrushTool=17;
struct PlantBrushSceneEntry {
  std::string name;
  std::optional<plants::Project> project;
  std::set<std::string> models;
  int count=0;
};
struct PlantBrushBake {
  uint64_t generation=0;
  PlantRuntimeAsset asset;
  std::string error;
};
static struct PlantBrushState {
  bool open=false,focus=false,erase=false,filtered=false,published=false,failed=false;
  float radius=96;
  int category=-1,epoch=-1,profile=-1,geometry=-1,resourceRevision=-1;
  std::string root,selection,error;
  std::array<char,128> search{};
  std::optional<plants::Project> custom,plant;
  std::optional<PlantRuntimeAsset> prepared;
  std::future<PlantBrushBake> job;
  std::shared_ptr<std::atomic<bool>> cancel;
  uint64_t generation=0;
  std::map<std::string,PlantBrushSceneEntry> scene;
  std::map<std::string,std::optional<plants::Project>> sceneSources;
  std::set<std::string> filter;
} plantBrush;

static bool plantBrushActive(){return int(cvars->GetFloat("ftebroom_tool"))==plantBrushTool;}
static void plantBrushInvalidateAsset()
{
  auto& s=plantBrush;
  if(s.cancel)*s.cancel=true;
  ++s.generation;
  s.prepared.reset();
  s.published=s.failed=false;
  s.error.clear();
}
static void plantBrushShutdown()
{
  if(plantBrush.cancel)*plantBrush.cancel=true;
  if(plantBrush.job.valid())plantBrush.job.wait();
}
static void plantBrushActivate()
{
  auto& s=plantBrush;
  if(!plantBrushActive())action("ftebroom_tool_toggle plantbrush\n");
  cvars->SetFloat("ftebroom_plant_brush_radius",s.radius);
  cvars->SetFloat("ftebroom_plant_brush_erase",s.erase);
}
static void plantBrushShow()
{
  auto& s=plantBrush;
  if(!s.open)plantLibraryLoad();
  s.open=s.focus=true;
  plantBrushActivate();
}
static void plantBrushSelect(const plants::Project& project,std::string key)
{
  plantBrushInvalidateAsset();
  plantBrush.plant=project;
  plantBrush.selection=std::move(key);
}
static void plantBrushFromEditor()
{
  auto& s=plantBrush;
  s.custom=plantState.doc;
  s.erase=false;
  plantBrushSelect(*s.custom,"@current");
  plantState.hidden=true;
  plantReleasePreview();
  plantBrushShow();
}
static void plantBrushNewPlant()
{
  plantRequest([](){
    if(plantBrushActive())action("ftebroom_tool_deactivate\n");
    placementCancel(plantPlacement);
    plantShow();
    auto draft=plants::presets().front();
    draft.name="Untitled oak";
    draft.seed=uint32_t(plants::mix(draft.seed,plantState.revision+1));
    plantSelect(draft);
  });
}
static bool plantBrushContextValid()
{
  const auto& s=plantBrush;
  return s.epoch==organizationMapEpoch&&s.profile==gameProfile&&s.root==baseq3;
}
static void plantBrushBakeTick()
{
  auto& s=plantBrush;
  if(!plantBrushContextValid()){
    const bool mapChanged=s.epoch>=0&&s.epoch!=organizationMapEpoch;
    plantBrushInvalidateAsset();
    s.epoch=organizationMapEpoch;s.profile=gameProfile;s.root=baseq3;
    s.geometry=s.resourceRevision=-1;
    s.scene.clear();s.sceneSources.clear();s.filter.clear();s.filtered=false;
    if(mapChanged&&plantBrushActive())action("ftebroom_tool_deactivate\n");
  }
  if(s.job.valid()&&s.job.wait_for(std::chrono::seconds(0))==std::future_status::ready){
    auto result=s.job.get();
    if(result.generation==s.generation){
      if(result.error.empty())s.prepared=std::move(result.asset);
      else{s.failed=true;s.error=std::move(result.error);}
    }
  }
  if(s.erase||!s.plant||s.failed||s.published||s.prepared||s.job.valid())return;
  const auto project=*s.plant;
  const int profile=gameProfile;
  const bool libre=q1UseLibre;
  const auto palette=profile==2?q1Palette():profile==3?q2Palette():std::string{};
  const auto* photos=project.generator>=3?plantPhotos():nullptr;
  const auto generation=s.generation;
  auto cancel=std::make_shared<std::atomic<bool>>(false);
  s.cancel=cancel;
  s.job=std::async(std::launch::async,[project,profile,libre,palette,photos,generation,cancel](){
    PlantBrushBake output;output.generation=generation;
    try{
      PlantResult result;
      result.lods[0]=plants::generate(project,0,cancel.get());
      plants::validate(result.lods[0]);
      result.maps=std::make_shared<plants::Textures>(
        plantTextureMaps(project,plantReadTextures(project),cancel.get(),256,photos));
      if(!*cancel)output.asset=plantBakeRuntime(project,result,profile,libre,palette,photos);
    }catch(const std::exception& e){output.error=e.what();}
    return output;
  });
}
static bool plantBrushPath(const std::string& path)
{
  constexpr std::string_view prefix="models/reyond-plugins/plant-";
  return path.rfind(prefix,0)==0&&path.size()>prefix.size()&&
    path.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-_",prefix.size())==path.npos;
}
static std::optional<plants::Project> plantBrushSceneSource(const std::string& model,std::string source)
{
  if(!plantBrushPath(source)||std::filesystem::u8path(source).extension()!=".json")
    source=std::filesystem::u8path(model).replace_extension(".plant.json").generic_u8string();
  try{
    auto local=documentPath(source);
    constexpr int maxBytes=4*1024*1024;
    std::string text;
    if(std::filesystem::is_regular_file(local)){
      if(std::filesystem::file_size(local)>maxBytes)return {};
      text=q1Read(local);
    }else{
      qhandle_t handle;
      const auto size=files->Open(source.c_str(),&handle,1);
      if(size<0)return {};
      files->Close(handle);
      if(size>maxBytes)return {};
      text=modelResource(source);
    }
    if(text.empty())return {};
    auto project=plants::deserialize(plants::Json::parse(text));
    for(const auto& slot:plantTextureFiles(project))if(!slot.file->empty()&&std::filesystem::u8path(*slot.file).is_relative())
      *slot.file=(local.parent_path()/std::filesystem::u8path(*slot.file)).u8string();
    return project;
  }catch(const std::exception&){return {};}
}
static bool plantBrushRefreshScene()
{
  auto& s=plantBrush;
  std::string header,chunk,version;
  int epoch,entities,nodes,geometry,revision;
  if(!qcQuery(0,0,header))return false;
  std::istringstream meta(header);
  if(!(meta>>version>>epoch>>entities>>nodes>>geometry>>revision)||version!="RBUI1"||nodes<2||nodes>4000000)return false;
  if(geometry==s.geometry&&revision==s.resourceRevision)return true;
  std::map<std::string,PlantBrushSceneEntry> updated;
  for(int cursor=0;cursor<nodes;cursor+=128){
    if(!qcQuery(8,cursor,chunk))return false;
    std::istringstream rows(chunk);
    std::string model,source;
    while(rows>>std::quoted(model)>>std::quoted(source)){
      if(!plantBrushPath(model))continue;
      auto [it,added]=s.sceneSources.try_emplace(model);
      if(added)it->second=plantBrushSceneSource(model,source);
      const auto& project=it->second;
      auto key=project?project->preset+"\n"+project->name:model;
      auto& item=updated[key];
      if(item.count++==0){
        item.name=project?project->name:std::filesystem::u8path(model).stem().u8string();
        item.project=project;
      }
      item.models.insert(model);
    }
  }
  s.scene=std::move(updated);
  s.geometry=geometry;s.resourceRevision=revision;
  return true;
}
static bool plantBrushBeforeStroke()
{
  auto& s=plantBrush;
  if(!plantBrushContextValid()||modEmbedded||cvars->GetFloat("ftebroom_bsp_preview"))return false;
  if(s.erase){
    if(s.filtered&&!plantBrushRefreshScene())return false;
    command(s.filtered?"ftebroom_plant_brush_filter reset 1\n":"ftebroom_plant_brush_filter reset 0\n");
    if(s.filtered)for(const auto& key:s.filter){
      auto entry=s.scene.find(key);
      if(entry!=s.scene.end())for(const auto& model:entry->second.models)
        command(("ftebroom_plant_brush_filter "+organizationQuote(model)+"\n").c_str());
    }
    return true;
  }
  if(s.published)return true;
  if(!s.prepared)return false;
  try{
    auto& asset=*s.prepared;
    placementPublish(asset.bundle);
    if(!asset.shader.empty()){
      plantState.pendingShaders[asset.id]={asset.material,asset.shader.substr(asset.shader.find('{'))};
      command(("ftebroom_plant shader "+asset.id+"\n").c_str());
    }
    auto config="ftebroom_plant_brush_asset "+organizationQuote(asset.model)+" "+organizationQuote(asset.source)
      +" "+plants::targetClass(gameProfile)+" "+(plantState.allowFteQ1?"1":"0")
      +" "+organizationQuote(placementVector(asset.low))+" "+organizationQuote(placementVector(asset.high))
      +" "+std::to_string(asset.scale)+" "+std::to_string(s.epoch)+"\n";
    command(config.c_str());
    s.prepared.reset();s.published=true;s.error.clear();
    return true;
  }catch(const std::exception& e){s.error=e.what();return false;}
}
static bool plantBrushMatches(const plants::Project& project,const std::string& search)
{
  if(plantBrush.category>=0&&int(project.family)!=plantBrush.category)return false;
  if(search.empty())return true;
  std::string text=project.name+" "+project.species+" "+plants::familyName(project.family);
  for(const auto& tag:project.tags)text+=" "+tag;
  return entityLower(text).find(search)!=std::string::npos;
}
static bool plantBrushRow(const char* id,const std::string& label,const char* detail,
    const plants::Project* project,bool selected,const std::string& qa)
{
  ImGui::PushID(id);
  const float height=46*uiScale(),padding=5*uiScale();
  auto start=ImGui::GetCursorScreenPos();
  int image=project&&ImGui::IsRectVisible({height,height})?plantThumbnail(*project):0;
  bool clicked=ImGui::Selectable("##plant",selected,ImGuiSelectableFlags_None,{0,height});
  if(placementQa())placementRect(qa.c_str());
  auto end=ImGui::GetItemRectMax();
  auto* list=ImGui::GetWindowDrawList();
  if(image)list->AddImage((ImTextureID)image,start,{start.x+height,start.y+height});
  auto left=start.x+height+padding,right=end.x-padding;
  if(right>left){
    list->PushClipRect({left,start.y},{right,end.y},true);
    list->AddText({left,start.y+padding},ImGui::GetColorU32(ImGuiCol_Text),label.c_str());
    list->AddText({left,start.y+height*.54f},ImGui::GetColorU32(ImGuiCol_TextDisabled),detail);
    list->PopClipRect();
  }
  if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))ImGui::SetTooltip("%s",label.c_str());
  ImGui::PopID();
  return clicked;
}
static void plantBrushPanel()
{
  auto& s=plantBrush;
  if(!s.open)return;
  try{plantBrushBakeTick();}
  catch(const std::exception& e){s.failed=true;s.error=e.what();}
  if(s.focus){
    for(const char* name:{"Entity","Map","Face"}){
      auto* inspector=ImGui::FindWindowByName(name);
      if(inspector&&inspector->DockId){ImGui::SetNextWindowDockID(inspector->DockId,ImGuiCond_FirstUseEver);break;}
    }
    ImGui::SetNextWindowFocus();
  }
  const bool visible=uiSoftPanelBegin("Plant Brush",&s.open,uiPanelFlags());
  if(s.focus){
    auto* window=ImGui::GetCurrentWindow();
    if(window->DockNode&&window->DockNode->TabBar)
      window->DockNode->TabBar->NextSelectedTabId=window->TabId;
    if(visible)s.focus=false;
  }
  if(visible){
    if(ImGui::RadioButton("Paint",!s.erase)){s.erase=false;cvars->SetFloat("ftebroom_plant_brush_erase",0);}
    placementRect("plant-brush/paint");
    ImGui::SameLine();
    if(ImGui::RadioButton("Erase",s.erase)){s.erase=true;cvars->SetFloat("ftebroom_plant_brush_erase",1);}
    placementRect("plant-brush/erase");
    ImGui::TextUnformatted(uiTr("Radius"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if(ImGui::SliderFloat("##plant-brush-radius",&s.radius,8,1024,"%.0f",ImGuiSliderFlags_Logarithmic|ImGuiSliderFlags_AlwaysClamp))
      cvars->SetFloat("ftebroom_plant_brush_radius",s.radius);
    placementRect("plant-brush/radius");
    if(!plantBrushActive()&&placementButton("Activate Plant Brush","plant-brush/activate"))plantBrushActivate();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##plant-brush-search","Search species...",s.search.data(),s.search.size());
    placementRect("plant-brush/search");
    const auto search=entityLower(s.search.data());
    if(s.erase){
      plantBrushRefreshScene();
      bool all=!s.filtered;
      if(ImGui::Checkbox("Erase all plants",&all)){s.filtered=!all;s.filter.clear();}
      placementRect("plant-brush/all");
      if(s.filtered&&s.filter.empty())ImGui::TextDisabled("Choose plants to erase.");
    }else{
      ImGui::SetNextItemWidth(-1);
      if(ImGui::BeginCombo("##plant-brush-category",s.category<0?"All plants":plants::familyName(plants::Family(s.category)))){
        if(ImGui::Selectable("All plants",s.category<0))s.category=-1;
        for(int i=0;i<13;i++)if(ImGui::Selectable(plants::familyName(plants::Family(i)),s.category==i))s.category=i;
        ImGui::EndCombo();
      }
    }
    float footer=2*(ImGui::GetFrameHeightWithSpacing());
    if(ImGui::BeginChild("Plant brush catalogue",{0,-footer},ImGuiChildFlags_Borders)){
      if(s.erase){
        if(s.scene.empty())ImGui::TextDisabled("No plants on this map.");
        for(const auto& [key,item]:s.scene){
          if(!search.empty()&&entityLower(item.name).find(search)==std::string::npos)continue;
          bool selected=s.filter.count(key)!=0;
          auto detail=std::to_string(item.count)+" "+uiTr("on map");
          if(plantBrushRow(key.c_str(),item.name,detail.c_str(),item.project?&*item.project:nullptr,selected,
              "plant-brush/scene/"+(item.project?item.project->preset:key))){
            s.filtered=true;
            if(selected)s.filter.erase(key);
            else s.filter.insert(key);
          }
        }
      }else{
        if(s.custom&&plantBrushMatches(*s.custom,search)&&plantBrushRow("@current",s.custom->name,"From Plant Editor",
            &*s.custom,s.selection=="@current","plant-brush/current"))plantBrushSelect(*s.custom,"@current");
        for(const auto& project:plantState.library){
          if(!plantBrushMatches(project,search))continue;
          const char* category=plants::familyName(project.family);
          if(plantBrushRow(project.preset.c_str(),project.name,category,&project,s.selection==project.preset,
              "plant-brush/preset/"+project.preset))plantBrushSelect(project,project.preset);
        }
      }
    }
    ImGui::EndChild();
    if(placementButton("+  Create new plant...","plant-brush/new"))plantBrushNewPlant();
    if(!s.error.empty())ImGui::TextWrapped("%s",s.error.c_str());
    else if(!s.erase&&s.plant&&!s.published&&!s.prepared)ImGui::TextDisabled("Preparing plant...");
    else if(!s.erase&&!s.plant)ImGui::TextDisabled("Choose a plant to paint.");
    else ImGui::TextDisabled("Drag on a surface in the 3D view.");
  }
  ImGui::End();
  if(!s.open&&plantBrushActive())action("ftebroom_tool_deactivate\n");
}
static void plantBrushHover()
{
  static bool wasHovering=false;
  auto mouse=ImGui::GetIO().MousePos;
  int view=viewHit(mouse.x,mouse.y);
  bool hovering=plantBrushActive()&&!modEmbedded&&!sceneLook&&!popupOpen&&view>=0&&
    viewRects[view].type==0&&!uiBlocksScene(mouse.x,mouse.y)&&!cvars->GetFloat("ftebroom_bsp_preview");
  if(hovering!=wasHovering)cvars->SetFloat("ftebroom_plant_brush_hover",hovering);
  wasHovering=hovering;
}
static nlohmann::json plantBrushQa()
{
  const auto& s=plantBrush;
  auto dock=[](const char* title){auto* w=ImGui::FindWindowByName(title);return w?w->DockId:0;};
  return {{"open",s.open},{"active",plantBrushActive()},{"erase",s.erase},{"filtered",s.filtered},
    {"radius",s.radius},{"ready",s.published||s.prepared.has_value()},{"selection",s.selection},
    {"error",s.error},{"editorVisible",plantState.open&&!plantState.hidden},
    {"dock",dock("Plant Brush")},{"inspectorDock",dock("Entity")}};
}
