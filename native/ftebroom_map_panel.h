// SPDX-License-Identifier: GPL-2.0-or-later
static void mapLayerOrder(const std::vector<int>& order)
{
  std::vector<std::pair<int,int>> changes;
  for(size_t i=0;i<order.size();++i){
    const auto node=std::find_if(organizationNodes.begin(),organizationNodes.end(),[&](const auto& n){return n.kind==1&&n.id==order[i];});
    if(node!=organizationNodes.end()&&node->order!=int(i+1))changes.emplace_back(order[i],int(i+1));
  }
  organizationBatch("layersort",changes);
}

static void mapLayerPanel(float height)
{
  organizationLoad();
  uiSectionHeading("Layers");
  const float controls=ImGui::GetFrameHeightWithSpacing();
  static std::set<int> selected;
  static int mapEpoch=-1,renameId=0;
  static char name[256]{};
  if(mapEpoch!=organizationMapEpoch){mapEpoch=organizationMapEpoch;selected.clear();}
  const int active=(int)cvars->GetFloat("ftebroom_active_layer");
  std::vector<int> order;
  for(const auto& layer:organizationNodes)if(layer.kind==1&&layer.id)order.push_back(layer.id);
  for(auto it=selected.begin();it!=selected.end();){
    if(*it&&std::find(order.begin(),order.end(),*it)==order.end())it=selected.erase(it);
    else ++it;
  }
  const int focused=selected.empty()?active:selected.size()==1?*selected.begin():-1;
  if(ImGui::BeginChild("Layer list",{0,std::max(45*uiScale(),height-controls)},ImGuiChildFlags_Borders)){
    for(const auto& layer:organizationNodes)if(layer.kind==1){
      ImGui::PushID(layer.id);
      const auto row=ImGui::GetCursorScreenPos();
      const float side=ImGui::GetFrameHeight(),gap=ImGui::GetStyle().ItemSpacing.x;
      const float rowHeight=ImGui::GetTextLineHeight()*2+5*uiScale();
      const float width=ImGui::GetContentRegionAvail().x;
      if(ImGui::Selectable("##layer",active==layer.id||selected.count(layer.id),
                          ImGuiSelectableFlags_AllowDoubleClick|ImGuiSelectableFlags_AllowOverlap,{width,rowHeight})){
        if(!ImGui::GetIO().KeyCtrl)selected={layer.id};
        else if(!selected.erase(layer.id))selected.insert(layer.id);
        if(ImGui::IsMouseDoubleClicked(0))organize("layeractive",layer.id);
      }
      const ImVec2 next=ImGui::GetCursorScreenPos();
      if(ImGui::IsItemHovered())ImGui::SetTooltip("%s%s",layer.name.c_str(),layer.omit?" (omitted from export)":"");
      if(layer.id&&ImGui::BeginDragDropSource()){
        ImGui::SetDragDropPayload("FTE_LAYER",&layer.id,sizeof layer.id);
        ImGui::TextUnformatted(layer.name.c_str());ImGui::EndDragDropSource();
      }
      if(layer.id&&ImGui::BeginDragDropTarget()){
        if(auto payload=ImGui::AcceptDragDropPayload("FTE_LAYER")){
          const int moved=*static_cast<const int*>(payload->Data);
          if(moved!=layer.id&&std::find(order.begin(),order.end(),moved)!=order.end()){
            auto reordered=order;
            reordered.erase(std::find(reordered.begin(),reordered.end(),moved));
            reordered.insert(std::find(reordered.begin(),reordered.end(),layer.id),moved);
            mapLayerOrder(reordered);
          }
        }
        ImGui::EndDragDropTarget();
      }
      const bool hidden=layer.flags&1,locked=layer.flags&2;
      if(ImGui::BeginPopupContextItem("Layer actions")){
        if(ImGui::MenuItem("Make active"))organize("layeractive",layer.id);
        if(ImGui::MenuItem("Move selection here"))organize("move",layer.id);
        if(ImGui::MenuItem("Select contents"))organize("layerselect",layer.id);
        if(ImGui::MenuItem("Hide layer",nullptr,hidden))organize("layerhidden",layer.id,hidden?"0":"1");
        if(ImGui::MenuItem("Lock layer",nullptr,locked))organize("layerlocked",layer.id,locked?"0":"1");
        if(ImGui::MenuItem("Omit from export",nullptr,layer.omit))organize("layeromit",layer.id,layer.omit?"0":"1");
        if(ImGui::MenuItem("Isolate layer")){
          std::vector<std::pair<int,int>> states;
          for(const auto& node:organizationNodes)if(node.kind==1)states.emplace_back(node.id,node.id!=layer.id);
          organizationBatch("layerhidden",states);
        }
        if(ImGui::MenuItem("Rename layer...",nullptr,false,layer.id!=0)){
          renameId=layer.id;std::snprintf(name,sizeof name,"%s",layer.name.c_str());
        }
        if(ImGui::MenuItem("Remove layer (keep objects)",nullptr,false,layer.id!=0))organize("layerremove",layer.id);
        ImGui::Separator();
        for(int mode=0;mode<4;++mode)
          if(ImGui::MenuItem(mode==0?"Show all layers":mode==1?"Hide all layers":mode==2?"Unlock all layers":"Lock all layers")){
            std::vector<std::pair<int,int>> states;
            for(const auto& node:organizationNodes)if(node.kind==1)states.emplace_back(node.id,mode&1);
            organizationBatch(mode<2?"layerhidden":"layerlocked",states);
          }
        ImGui::EndPopup();
      }
      const float line=ImGui::GetTextLineHeightWithSpacing();
      auto* draw=ImGui::GetWindowDrawList();
      const float labelStart=row.x+side+gap,labelEnd=row.x+std::max(side+gap,width-2*(side+gap));
      if(labelEnd>labelStart){
        draw->PushClipRect({labelStart,row.y},{labelEnd,row.y+rowHeight},true);
        draw->AddText({labelStart,row.y+2*uiScale()},ImGui::GetColorU32(ImGuiCol_Text),layer.name.c_str());
        if(layer.objectCount>=0){
          const auto count=std::to_string(layer.objectCount)+(layer.objectCount==1?" object":" objects");
          draw->AddText({labelStart,row.y+line},ImGui::GetColorU32(ImGuiCol_TextDisabled),count.c_str());
        }
        draw->PopClipRect();
      }
      ImGui::SetCursorScreenPos({row.x,row.y+(rowHeight-side)*.5f});
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{3*uiScale(),1*uiScale()});
      if(ImGui::RadioButton("##active",active==layer.id))organize("layeractive",layer.id);
      ImGui::PopStyleVar();
      if(ImGui::IsItemHovered())ImGui::SetTooltip("Make active");
      if(width>3*side+2*gap){
        ImGui::SetCursorScreenPos({row.x+width-2*side-gap,row.y+(rowHeight-side)*.5f});
        if(inspectorIcon("##visible",hidden?"Show layer":"Hide layer",hidden?'h':'e'))organize("layerhidden",layer.id,hidden?"0":"1");
        ImGui::SameLine();
        if(inspectorIcon("##locked",locked?"Unlock layer":"Lock layer",locked?'l':'u',locked))organize("layerlocked",layer.id,locked?"0":"1");
      }
      ImGui::SetCursorScreenPos(next);
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
  if(renameId>0){ImGui::OpenPopup("Layer name");renameId=-renameId;}
  if(inspectorIcon("##new-layer","Create layer",'+')){
    renameId=0;std::snprintf(name,sizeof name,"New layer");ImGui::OpenPopup("Layer name");
  }
  fileBrowserTestItem("map_create_layer");
  if(ImGui::BeginPopup("Layer name")){
    if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
    bool submit=ImGui::InputText("Layer name",name,sizeof name,ImGuiInputTextFlags_EnterReturnsTrue);
    submit|=ImGui::Button(renameId?"Rename":"Create");
    if(submit&&*name){organize(renameId?"layerrename":"layernew",-renameId,name);ImGui::CloseCurrentPopup();}
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  const bool removable=selected.empty()?focused>0:std::any_of(selected.begin(),selected.end(),[](int id){return id!=0;});
  ImGui::BeginDisabled(!removable);
  if(inspectorIcon("##remove-layer","Remove selected layers (keep objects)",'-')){
    std::vector<std::pair<int,int>> removed;
    for(int id:selected)if(id)removed.emplace_back(id,0);
    if(selected.empty())removed.emplace_back(focused,0);
    organizationBatch("layerremove",removed);selected.clear();
  }
  ImGui::EndDisabled();
  const auto pos=std::find(order.begin(),order.end(),focused);
  for(int direction:{-1,1}){
    ImGui::SameLine();
    const bool canMove=pos!=order.end()&&(direction<0?pos!=order.begin():pos+1!=order.end());
    ImGui::BeginDisabled(!canMove);
    if(inspectorIcon(direction<0?"##layer-up":"##layer-down",direction<0?"Move layer up":"Move layer down",direction<0?'^':'v')&&canMove){
      std::iter_swap(pos,pos+direction);mapLayerOrder(order);
    }
    ImGui::EndDisabled();
  }
  ImGui::SameLine();
  if(ImGui::Button("Groups..."))ImGui::OpenPopup("Group management");
  fileBrowserTestItem("map_groups");
  if(ImGui::BeginPopup("Group management")){organizationGroupControls();ImGui::EndPopup();}
}

static void mapBoundsPanel()
{
  if(!uiInspectorFold("Map Properties",ImGuiTreeNodeFlags_DefaultOpen))return;
  const float world=std::max(1.f,cvars->GetFloat("ftebroom_world_bound"));
  std::array<float,6> limits={-world,-world,-world,world,world,world},values{};
  static std::array<float,6> custom{};
  static std::string lastBounds="\n";
  static bool editCustom=false;
  char current[256]{};
  cvars->GetString("ftebroom_map_bounds",current,sizeof current);
  std::istringstream source(current);source.imbue(std::locale::classic());
  const bool hasBounds=bool(source>>values[0]>>values[1]>>values[2]>>values[3]>>values[4]>>values[5]);
  if(lastBounds!=current){custom=hasBounds?values:limits;lastBounds=current;editCustom=false;}
  auto save=[&](const std::array<float,6>* box){
    std::ostringstream out;out.imbue(std::locale::classic());
    if(box)for(size_t i=0;i<box->size();++i){if(i)out<<' ';out<<(*box)[i];}
    lastBounds=out.str();cvars->SetString("ftebroom_map_bounds",lastBounds.c_str());preferencesSave();
  };
  const int mode=editCustom?2:!hasBounds?0:values==limits?1:2;
  if(ImGui::RadioButton("Soft bounds disabled",mode==0)){editCustom=false;save(nullptr);}
  fileBrowserTestItem("map_bounds_disabled");
  if(ImGui::RadioButton("Use game default",mode==1)){editCustom=false;save(&limits);}
  fileBrowserTestItem("map_bounds_default");
  ImGui::Indent();ImGui::TextDisabled("Min: %.0f    Max: %.0f",-world,world);ImGui::Unindent();
  if(ImGui::RadioButton("Use custom bounds",mode==2))editCustom=true;
  const bool customSelected=mode==2||editCustom;
  ImGui::BeginDisabled(!customSelected);
  if(ImGui::BeginTable("Bounds fields",2,ImGuiTableFlags_SizingStretchSame)){
    for(int i=0;i<2;++i){
      ImGui::TableNextColumn();ImGui::TextUnformatted(i==0?"Minimum":"Maximum");
      ImGui::SetNextItemWidth(-FLT_MIN);ImGui::InputFloat3(i==0?"##minimum":"##maximum",custom.data()+3*i,"%.6g");
    }
    ImGui::EndTable();
  }
  bool valid=true;
  for(int i=0;i<3;++i)valid&=std::isfinite(custom[i])&&std::isfinite(custom[i+3])&&
    custom[i]<custom[i+3]&&custom[i]>=-world&&custom[i+3]<=world;
  ImGui::BeginDisabled(!valid);
  if(customSelected&&ImGui::SmallButton("Apply bounds"))save(&custom);
  ImGui::EndDisabled();ImGui::EndDisabled();
}

static bool mapModHasResources(const std::filesystem::path& path)
{
  std::error_code error;
  for(std::filesystem::directory_iterator it(path,error),end;!error&&it!=end;it.increment(error)){
    const auto name=entityLower(it->path().filename().u8string());
    const auto ext=entityLower(it->path().extension().u8string());
    if(name=="progs.dat"||name=="gameinfo.txt"||name=="liblist.gam")return true;
    if(name=="textures"||name=="scripts"||name=="maps"||ext==".pak"||ext==".pk3"||ext==".wad")return true;
  }
  return false;
}
static void mapModsLegacyPanel()
{
  if(!uiInspectorFold("Mods",ImGuiTreeNodeFlags_DefaultOpen))return;
  namespace fs=std::filesystem;
  const auto runtime=fs::u8path(gameProfile==0?std::string(baseq3):gameRuntime()).lexically_normal();
  auto authoring=fs::u8path(baseq3).lexically_normal();
  const auto primary=runtime.empty()?authoring:runtime;
  fs::path base;
  if(gameWad())base=fs::u8path(q1BaseDirectory());
  else if(gameProfile==3)base=ftebroom::quake2FallbackDirectory(runtime);
  else if(gameProfile==1&&runtime!=authoring)base=authoring;
  static fs::path scanRoot,available,enabled,pending;
  static std::vector<fs::path> candidates;
  static bool pendingRemove=false;
  static std::string message;
  static char query[128]{};
  auto root=primary.parent_path();
  if(scanRoot!=root||ImGui::IsWindowAppearing()){
    scanRoot=root;available.clear();enabled.clear();candidates.clear();
    std::error_code error;
    if(!root.empty())for(fs::directory_iterator it(root,error),end;!error&&it!=end;it.increment(error)){
      std::error_code status;
      if(it->is_directory(status)&&mapModHasResources(it->path()))candidates.push_back(it->path().lexically_normal());
    }
    std::sort(candidates.begin(),candidates.end());
  }
  const bool canSwitch=gameProfile!=0&&!gameProfileLocked&&!assetsLoading&&!savingDocument&&!runActive&&!modEmbedded;
  const bool canRemove=canSwitch&&!runtime.empty()&&enabled==runtime&&
    (gameProfile==1||(!base.empty()&&base!=runtime));
  const float height=std::max(78*uiScale(),ImGui::GetContentRegionAvail().y-2.5f*ImGui::GetFrameHeightWithSpacing());
  auto row=[&](const fs::path& path,fs::path& selection,const char* suffix=""){
    if(path.empty())return;
    auto label=path.filename().u8string()+suffix;
    ImGui::PushID(path.u8string().c_str());
    if(ImGui::Selectable(label.c_str(),selection==path))selection=path;
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",path.u8string().c_str());
    ImGui::PopID();
  };
  if(ImGui::BeginTable("Mod columns",2,ImGuiTableFlags_SizingStretchSame)){
    ImGui::TableNextColumn();ImGui::TextUnformatted("Available");
    if(ImGui::BeginChild("Available mods",{0,height},ImGuiChildFlags_Borders)){
      for(const auto& path:candidates)if(path!=primary&&path!=base&&
        entityLower(path.filename().u8string()).find(entityLower(query))!=std::string::npos)row(path,available);
      if(candidates.empty())ImGui::TextDisabled("No mod folders found");
    }
    ImGui::EndChild();
    ImGui::SetNextItemWidth(-FLT_MIN);uiBrowserSearch("##mod-search","Search mods",query,sizeof query);
    ImGui::TableNextColumn();ImGui::TextUnformatted("Enabled");
    if(ImGui::BeginChild("Enabled mods",{0,height},ImGuiChildFlags_Borders)){
      row(primary,enabled,primary==authoring&&runtime.empty()?" (source)":"");
      if(base!=primary)row(base,enabled," (base)");
      if(primary.empty())ImGui::TextDisabled("Configure game resources");
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(!canSwitch||available.empty());
    if(inspectorIcon("##enable-mod","Use this mod",'+')){pending=available;pendingRemove=false;ImGui::OpenPopup("Change game resources");}
    ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(!canRemove);
    if(inspectorIcon("##disable-mod","Return to base resources",'-')){pending=gameProfile==1?fs::path{}:base;pendingRemove=true;ImGui::OpenPopup("Change game resources");}
    ImGui::EndDisabled();
    if(ImGui::IsPopupOpen("Change game resources"))
      ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),ImGuiCond_Appearing,{.5f,.5f});
    if(ImGui::BeginPopupModal("Change game resources",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
      const bool dirty=cvars->GetFloat("ftebroom_dirty")!=0;
      if(dirty)ImGui::TextUnformatted("Save the map before switching resources.");
      else ImGui::TextWrapped("%s",pendingRemove?"Return to the base resources?":"Reload game resources with the selected mod?");
      ImGui::BeginDisabled(dirty||!canSwitch);
      if(ImGui::Button("Reload resources")){
        const auto path=pending.u8string();
        char* field=gameProfile==1?gameMod:gameProfile==2?q1Mod:gameProfile==3?q2Mod:hlMod;
        const std::string old=field,oldBase=baseq3;const bool oldLibre=q1UseLibre;
        if(path.size()>=1024)message="Mod folder path is too long.";
        else{
          std::snprintf(field,1024,"%s",path.c_str());
          if(gameProfile==2)q1UseLibre=false;
          if(gameProfileSave()&&mountAssets()){message.clear();scanRoot.clear();}
          else{
            message=assetError.empty()?"Could not save game resources.":assetError;
            std::snprintf(field,1024,"%s",old.c_str());std::snprintf(baseq3,sizeof baseq3,"%s",oldBase.c_str());
            q1UseLibre=oldLibre;gameProfileSave();
          }
        }
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndDisabled();ImGui::SameLine();
      if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    ImGui::EndTable();
  }
  if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
}


static void mapModsPanel()
{
  if(gameProfile!=2){mapModsLegacyPanel();return;}
  const bool open=uiInspectorFold("Mods",ImGuiTreeNodeFlags_DefaultOpen);fileBrowserTestItem("map_mod_header");if(!open)return;
  namespace fs=std::filesystem;runLoad();
  const auto root=mapModsRoot();
  static std::string available,enabled,message;static char query[128]{};
  std::vector<std::string> mods;
  try{mods=mapModsForDocument();}catch(const std::exception& error){message=error.what();}
  static fs::path scanRoot;static std::vector<std::string> candidates;
  if(scanRoot!=root||ImGui::IsWindowAppearing()){
    scanRoot=root;candidates.clear();std::error_code error;
    if(!root.empty())for(fs::directory_iterator it(root,error),end;!error&&it!=end;it.increment(error)){
      std::error_code status;if(it->is_directory(status)&&mapModHasResources(it->path()))candidates.push_back(it->path().filename().u8string());
    }
    std::sort(candidates.begin(),candidates.end());
  }
  const bool canSwitch=gameClassic()&&!gameProfileLocked&&!assetsLoading&&!savingDocument&&!runActive&&!modEmbedded;
  const float height=std::max(78*uiScale(),ImGui::GetContentRegionAvail().y-3.6f*ImGui::GetFrameHeightWithSpacing());
  auto row=[&](const std::string& name,std::string& selected,const char* group){
    ImGui::PushID(group);ImGui::PushID(name.c_str());
    if(ImGui::Selectable(name.c_str(),selected==name))selected=name;
    fileBrowserTestItem(std::string("map_mod_")+group+"/"+name);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",(root/fs::u8path(name)).u8string().c_str());
    ImGui::PopID();ImGui::PopID();
  };
  auto apply=[&](std::vector<std::string> next){if(mapModsSet(next))message.clear();else message=gameMapModsError;};
  if(ImGui::BeginTable("Mod columns",2,ImGuiTableFlags_SizingStretchSame)){
    ImGui::TableNextColumn();ImGui::TextUnformatted("Available");
    if(ImGui::BeginChild("Available mods",{0,height},ImGuiChildFlags_Borders)){
      for(const auto& name:candidates)if(std::find_if(mods.begin(),mods.end(),[&](const auto& value){return ftebroom::mapModSame(value,name);})==mods.end()&&entityLower(name).find(entityLower(query))!=std::string::npos)row(name,available,"available");
      if(candidates.empty())ImGui::TextDisabled("No mod folders found");
    }
    ImGui::EndChild();ImGui::SetNextItemWidth(-FLT_MIN);uiBrowserSearch("##mod-search","Search mods",query,sizeof query);
    ImGui::TableNextColumn();ImGui::TextUnformatted("Enabled");
    if(ImGui::BeginChild("Enabled mods",{0,height},ImGuiChildFlags_Borders))for(const auto& name:mods)row(name,enabled,"enabled");
    ImGui::EndChild();
    ImGui::BeginDisabled(!canSwitch||available.empty()||std::find_if(mods.begin(),mods.end(),[&](const auto& value){return ftebroom::mapModSame(value,available);})!=mods.end());
    if(inspectorIcon("##enable-mod","Enable selected mod",'+')){auto next=mods;next.push_back(available);apply(std::move(next));}
    fileBrowserTestItem("map_mod_add");ImGui::EndDisabled();ImGui::SameLine();
    auto selected=std::find(mods.begin(),mods.end(),enabled);const bool hasSelection=selected!=mods.end();
    ImGui::BeginDisabled(!canSwitch||!hasSelection);
    if(inspectorIcon("##disable-mod","Disable selected mod",'-')){auto next=mods;next.erase(next.begin()+(selected-mods.begin()));apply(std::move(next));}
    fileBrowserTestItem("map_mod_remove");ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(!canSwitch||!hasSelection||selected==mods.begin());
    if(ImGui::SmallButton("Up##mod-up")){auto next=mods;auto index=selected-mods.begin();std::swap(next[index-1],next[index]);apply(std::move(next));}
    fileBrowserTestItem("map_mod_up");ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(!canSwitch||!hasSelection||(hasSelection&&std::next(selected)==mods.end()));
    if(ImGui::SmallButton("Down##mod-down")){auto next=mods;auto index=selected-mods.begin();std::swap(next[index+1],next[index]);apply(std::move(next));}
    fileBrowserTestItem("map_mod_down");ImGui::EndDisabled();ImGui::EndTable();
  }
  ImGui::TextDisabled("Base resources: %s (inherited)",runBaseMod.c_str());
  if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
  if(!gameMapModsError.empty()&&gameMapModsError!=message)ImGui::TextWrapped("%s",gameMapModsError.c_str());
}

static void mapInspector()
{
  const float height=std::max(100*uiScale(),ImGui::GetContentRegionAvail().y*uiLayerFraction);
  mapLayerPanel(height);
  ImGui::InvisibleButton("Resize Layers",{ImGui::GetContentRegionAvail().x,6*uiScale()});
  fileBrowserTestItem("map_layer_splitter");
  const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
  ImGui::GetWindowDrawList()->AddLine({a.x,(a.y+b.y)*.5f},{b.x,(a.y+b.y)*.5f},ImGui::GetColorU32(ImGuiCol_Separator));
  if(ImGui::IsItemActive()||ImGui::IsItemHovered())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  if(ImGui::IsItemActive())uiLayerFraction=std::clamp(uiLayerFraction+ImGui::GetIO().MouseDelta.y/std::max(1.f,ImGui::GetWindowHeight()),.22f,.68f);
  if(ImGui::IsItemDeactivated())uiSaveAppearance();
  mapBoundsPanel();
  ImGui::Spacing();
  mapModsPanel();
}
