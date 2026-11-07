// SPDX-License-Identifier: GPL-2.0-or-later
static bool uiLoaded = false, uiReset = false, uiLocked = false;
static bool preferencesSave();
static constexpr float uiDefaultSkyColor[3]={.16f,.39f,.70f};
static float uiSkyColor[3]={.16f,.39f,.70f};
static void uiApplySky(){
  cvars->SetFloat("ftebroom_clear_sky",gameClearSky);
  char color[128];std::snprintf(color,sizeof color,"%.9g %.9g %.9g",uiSkyColor[0],uiSkyColor[1],uiSkyColor[2]);
  cvars->SetString("ftebroom_sky_color",color);
}

static bool uiHasScene = false;
static ImVec2 uiScenePos, uiSceneSize;
static ImGuiID uiDockId;
static bool uiExplorerDockChecked=false;
static bool uiExplorerMarkerChecked=false;
static float uiLayerFraction=.48f;
static float uiAppliedScale = 0;
static void uiScaleDockNode(ImGuiDockNode* node, float ratio)
{
  if (!node)
    return;
  node->SizeRef.x *= ratio;
  node->SizeRef.y *= ratio;
  uiScaleDockNode(node->ChildNodes[0], ratio);
  uiScaleDockNode(node->ChildNodes[1], ratio);
}
static void uiApplyScale(float w, float h)
{
  const float scale = uiScale();
  if (uiAppliedScale != 0 && uiAppliedScale != scale)
  {
    const float ratio = scale / uiAppliedScale;
    uiScaleDockNode(ImGui::DockBuilderGetNode(uiDockId), ratio);
    for (auto window : ImGui::GetCurrentContext()->Windows)
    {
      if (window->DockIsActive || (window->Flags & ImGuiWindowFlags_ChildWindow))
        continue;
      const std::string name = window->Name;
      if (
        name != "Map" && name != "Entity" && name != "Face" && name != "Tools"
        && name != "Tool options" && !(window->Flags & ImGuiWindowFlags_Modal))
        continue;
      const ImVec2 size{
        std::min(w - 24, window->SizeFull.x * ratio),
        std::min(h - 64, window->SizeFull.y * ratio)};
      ImGui::SetWindowSize(window, size, ImGuiCond_Always);
      ImGui::SetWindowPos(
        window,
        {std::clamp(window->Pos.x, 0.f, std::max(0.f, w - size.x)),
         std::clamp(window->Pos.y, 24.f, std::max(24.f, h - size.y))},
        ImGuiCond_Always);
    }
  }
  uiAppliedScale = scale;
  uiStyle();
}
static void uiSaveAppearance()
{
  if (cvars->GetFloat("ftebroom_skip_setup"))
    return;
  std::ofstream out(documentPath("appearance.cfg"));
  out << uiScaleChoice << ' ' << uiLocked << ' ' << uiTheme << ' ' << uiLanguage << ' ' << uiCursorPercent << " 1 " << gameClearSky << ' ' << uiSkyColor[0] << ' ' << uiSkyColor[1] << ' ' << uiSkyColor[2] << ' ' << uiLayerFraction << '\n';
}
static void uiLoad()
{
  if (uiLoaded)
    return;
  uiLoaded = true;
  // Entity creation always instantiates the definition defaults; the old
  // optional switch is no longer exposed by the compact entity inspector.
  cvars->SetFloat("ftebroom_instantiate_defaults",1);
  gameProfileLoad(); // Migrate the former custom-profile sky preference once.
  if (!cvars->GetFloat("ftebroom_skip_setup"))
  {
    int scale = 0, locked = 0;
    std::ifstream appearance(documentPath("appearance.cfg"));
    if (appearance >> scale >> locked)
    {
      uiScaleChoice = std::clamp(scale, 0, 4);
      uiLocked = locked != 0;
      int theme=0;if(appearance>>theme)uiTheme=std::clamp(theme,0,5);
      int language=0;if(appearance>>language)uiLanguage=std::clamp(language,0,6);
      int cursor=uiDefaultCursorPercent;
      uiCursorPercent=(appearance>>cursor)?std::clamp(cursor,50,250):uiDefaultCursorPercent;
      int skyVersion=0,skyEnabled=1;float color[3];
      if((appearance>>skyVersion>>skyEnabled>>color[0]>>color[1]>>color[2])&&skyVersion==1&&(skyEnabled==0||skyEnabled==1)){
        bool valid=true;for(float value:color)valid&=std::isfinite(value);
        if(valid){gameClearSky=skyEnabled!=0;for(int a=0;a<3;a++)uiSkyColor[a]=std::clamp(color[a],0.f,1.f);}
      }
      float fraction=0;if(appearance>>fraction&&std::isfinite(fraction))uiLayerFraction=std::clamp(fraction,.22f,.68f);
    }
    std::ifstream layout(documentPath("workspace.ini"), std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(layout)), {});
    if (!data.empty() && data.size() <= 1024 * 1024){
      const auto old=data.find("[Window][Material]");
      if(old!=data.npos){
        data.replace(old,18,"[Window][Face]");
        char from[32],to[32];
        std::snprintf(from,sizeof from,"Selected=0x%08X",ImHashStr("#TAB",0,ImHashStr("Material")));
        std::snprintf(to,sizeof to,"Selected=0x%08X",ImHashStr("#TAB",0,ImHashStr("Face")));
        if(auto at=data.find(from);at!=data.npos)data.replace(at,std::strlen(from),to);
      }
      ImGui::LoadIniSettingsFromMemory(data.data(), data.size());
      if(old!=data.npos){
        auto* entity=ImGui::FindWindowSettingsByID(ImHashStr("Entity"));
        auto* face=ImGui::FindWindowSettingsByID(ImHashStr("Face"));
        if(entity&&face&&entity->DockId==face->DockId&&entity->DockOrder>face->DockOrder)
          std::swap(entity->DockOrder,face->DockOrder);
      }
    }
  }
  uiApplySky();
}
static void uiSaveLayout()
{
  auto& io = ImGui::GetIO();
  if (!io.WantSaveIniSettings)
    return;
  if (!cvars->GetFloat("ftebroom_skip_setup"))
  {
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    std::ofstream out(documentPath("workspace.ini"), std::ios::binary);
    out.write(data, size);
  }
  io.WantSaveIniSettings = false;
}
static void uiWorkspace(float w, float h, float top, float bottom, float margin)
{
  const ImVec2 workspaceSize{std::max(1.f,w-2*margin),std::max(1.f,h-top-bottom)};
  ImGui::SetNextWindowPos({margin, top});
  ImGui::SetNextWindowSize(workspaceSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
  ImGui::Begin(
    "##workspace",
    nullptr,
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
      | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
      | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking
      | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar(3);
  uiDockId = ImGui::GetID("WorkspaceDock");
  const auto flags = ImGuiDockNodeFlags_PassthruCentralNode
                     | ImGuiDockNodeFlags_NoDockingOverCentralNode
                     | (uiLocked ? ImGuiDockNodeFlags_NoResize : 0);
  if (uiReset || !ImGui::DockBuilderGetNode(uiDockId))
  {
    ImGui::DockBuilderRemoveNode(uiDockId);
    ImGui::DockBuilderAddNode(uiDockId, ImGuiDockNodeFlags_DockSpace | flags);
    ImGui::DockBuilderSetNodeSize(uiDockId, workspaceSize);
    ImGuiID center = uiDockId, right, info, explorer;
    ImGui::DockBuilderSplitNode(
      center,
      ImGuiDir_Right,
      std::min(.48f, 370 * uiScale() / std::max(1.f, workspaceSize.x - 64 * uiScale())),
      &right,
      &center);
    const bool rail=uiModern() && workspaceSize.x>=740*uiScale();
    const auto* workNode=ImGui::DockBuilderGetNode(center);
    const float workWidth=workNode?workNode->Size.x:workspaceSize.x;
    ImGui::DockBuilderSplitNode(center,rail?ImGuiDir_Left:ImGuiDir_Up,
      rail?std::min(.24f,ModernChrome::railWidth*uiScale()/std::max(1.f,workWidth))
          :std::min(.3f,120*uiScale()/workspaceSize.y),&info,&center);
    ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.34f,&explorer,&center);
    ImGui::DockBuilderDockWindow("Mod file manager",explorer);
    uiExplorerDockChecked=true;
    ImGui::DockBuilderDockWindow("Tools", info);
    ImGui::DockBuilderDockWindow("Map", right);
    ImGui::DockBuilderDockWindow("Entity", right);
    ImGui::DockBuilderDockWindow("Face", right);
    ImGui::DockBuilderDockWindow("FTE Engine terrain", right);
    ImGui::DockBuilderFinish(uiDockId);
    uiReset = false;
  }
  if(!uiExplorerDockChecked) {
    uiExplorerDockChecked=true;
    // Upgrade older layouts once; subsequent runs preserve the user's docking choice.
    if(!std::filesystem::exists(documentPath("explorer-layout-v1.cfg"))) {
      if(auto central=ImGui::DockBuilderGetCentralNode(uiDockId)){ImGuiID center=central->ID,explorer;ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.34f,&explorer,&center);ImGui::DockBuilderDockWindow("Mod file manager",explorer);ImGui::DockBuilderFinish(uiDockId);}
    }
  }
  if(!uiExplorerMarkerChecked) {
    uiExplorerMarkerChecked=true;
    if(!cvars->GetFloat("ftebroom_skip_setup")&&!std::filesystem::exists(documentPath("explorer-layout-v1.cfg")))std::ofstream(documentPath("explorer-layout-v1.cfg"))<<"1\n";
  }
  const bool soft=uiModern();
  // Size references in the dock tree can exceed the resized window. Clamp the
  // shared inspector on resize or scale changes, without overriding sash drags.
  static float lastWidth=0,lastHeight=0,lastScale=0;
  const float scale=uiScale();
  if(soft&&workspaceSize.x<1200*scale&&(lastWidth!=workspaceSize.x||lastScale!=scale)){
    const auto* map=ImGui::FindWindowByName("Map");
    auto* node=map?ImGui::DockBuilderGetNode(map->DockId):nullptr;
    if(node&&node->ParentNode&&node->ParentNode->SplitAxis==ImGuiAxis_X&&
       node==node->ParentNode->ChildNodes[1]&&ImGui::DockNodeGetRootNode(node)->ID==uiDockId){
      node->SizeRef.x=std::min(node->SizeRef.x,workspaceSize.x*.48f);
    }
  }
  if(soft&&workspaceSize.y<740*scale&&(lastHeight!=workspaceSize.y||lastScale!=scale)){
    const auto* explorer=ImGui::FindWindowByName("Mod file manager");
    auto* node=explorer?ImGui::DockBuilderGetNode(explorer->DockId):nullptr;
    if(node&&node->ParentNode&&node->ParentNode->SplitAxis==ImGuiAxis_Y&&
       node==node->ParentNode->ChildNodes[1]&&ImGui::DockNodeGetRootNode(node)->ID==uiDockId){
      node->SizeRef.y=std::min(node->SizeRef.y,node->ParentNode->Size.y*.40f);
    }
  }
  lastWidth=workspaceSize.x;lastHeight=workspaceSize.y;lastScale=scale;
  const ImGuiCol dockColors[]={ImGuiCol_WindowBg,ImGuiCol_Border,ImGuiCol_TitleBg,
    ImGuiCol_TitleBgActive,ImGuiCol_TitleBgCollapsed};
  // PassthruCentralNode normally paints WindowBg over the rest of the dock.
  // Leave that layer transparent so the workspace shows through the dividers.
  if(soft)for(auto slot:dockColors)ImGui::PushStyleColor(slot,ModernChrome::transparent);
  ImGui::DockSpace(uiDockId, {0, 0}, flags);
  if(soft)ImGui::PopStyleColor(IM_ARRAYSIZE(dockColors));
  auto* node=ImGui::DockBuilderGetCentralNode(uiDockId);
  if(node)
  {
    uiScenePos = node->Pos;
    uiSceneSize = node->Size;
    uiHasScene = true;
  }
  ImGui::End();
  if(!soft)return;

  const ImRect screen{{0,0},{w,h}};
  ImRect viewport=screen;
  if(node) {
    const float inset=std::min(5*uiScale(),std::max(0.f,(std::min(node->Size.x,node->Size.y)-1)*.5f));
    viewport={node->Pos,{node->Pos.x+node->Size.x,node->Pos.y+node->Size.y}};
    uiScenePos={viewport.Min.x+inset,viewport.Min.y+inset};
    uiSceneSize={node->Size.x-2*inset,node->Size.y-2*inset};
  }
  ImRect scene{uiScenePos,{uiScenePos.x+uiSceneSize.x,uiScenePos.y+uiSceneSize.y}};
  scene.ClipWithFull(screen);
  auto* draw=ImGui::GetBackgroundDrawList();
  auto colorAt=[&](float y){
    const auto base=uiModernPalette().workspace;
    const float shade=.018f*y/std::max(1.f,h);
    return ImGui::GetColorU32(ImVec4{base.x-shade,base.y-shade,base.z-shade,1});
  };
  auto fill=[&](const ImRect& bounds){
    const auto light=colorAt(bounds.Min.y),shade=colorAt(bounds.Max.y);
    draw->AddRectFilledMultiColor(bounds.Min,bounds.Max,light,light,shade,shade);
  };
  if(node) {
    uiOutsideScene(screen,scene,fill);
    uiSoftSurface(draw,viewport,ModernChrome::panelRadius*uiScale(),false,&scene);
  } else {
    fill(screen);
  }
}
static bool uiBlocksScene(float x, float y, bool acceptEntityDrop = false)
{
  if (!uiHasScene)
    return false;
  ImGuiWindow* hovered = nullptr;
  ImGuiWindow* underMoving = nullptr;
  ImGui::FindHoveredWindowEx({x, y}, false, &hovered, &underMoving);
  // The empty central node is a hole in the dock host. Floating panels,
  // splitters, popup windows and title-bar drags retain their input.
  auto payload = ImGui::GetDragDropPayload();
  bool entityDrop = acceptEntityDrop && payload && (payload->IsDataType("FTE_ENTITY")||payload->IsDataType("FTEBROOM_MOD_FILE")||payload->IsDataType("FTEBROOM_PREFAB"));
  return hovered != nullptr || ImGui::GetCurrentContext()->MovingWindow != nullptr
         || ImGui::GetIO().WantTextInput
         || (ImGui::GetCurrentContext()->ActiveId != 0 && !entityDrop);
}
static ImGuiWindowFlags uiPanelFlags()
{
  return ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar
         | (uiLocked ? ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize : 0);
}
static void uiPreferences()
{
  uiSectionHeading("Appearance and workspace");
  ImGui::TextDisabled("Brush edges and 2D background: Colors tab.");
  if(ImGui::Combo("Interface theme",&uiTheme,uiThemeNames,6))uiSaveAppearance();
  float labelSize=cvars->GetFloat("ftebroom_label_size"),coordinateSize=cvars->GetFloat("ftebroom_coordinate_size");
  if(labelSize<8)labelSize=13;if(coordinateSize<8)coordinateSize=8;
  if(ImGui::SliderFloat("Entity / group label size",&labelSize,8,40,"%.0f px",ImGuiSliderFlags_AlwaysClamp))cvars->SetFloat("ftebroom_label_size",std::round(labelSize));
  if(ImGui::IsItemDeactivatedAfterEdit())preferencesSave();
  if(ImGui::SliderFloat("Coordinate label size",&coordinateSize,8,40,"%.0f px",ImGuiSliderFlags_AlwaysClamp))cvars->SetFloat("ftebroom_coordinate_size",std::round(coordinateSize));
  if(ImGui::IsItemDeactivatedAfterEdit())preferencesSave();
  uiSectionHeading("Viewport sky background");
  if(ImGui::Checkbox("Show sky background",&gameClearSky)){uiApplySky();uiSaveAppearance();}
  ImGui::BeginDisabled(!gameClearSky);
  if(ImGui::ColorEdit3("Sky color",uiSkyColor,ImGuiColorEditFlags_NoInputs)){uiApplySky();uiSaveAppearance();}
  ImGui::SameLine();if(ImGui::Button("Reset sky color")){std::copy(std::begin(uiDefaultSkyColor),std::end(uiDefaultSkyColor),uiSkyColor);uiApplySky();uiSaveAppearance();}
  ImGui::EndDisabled();
  ImGui::TextWrapped("Applies to the 3D editor background in every game profile. Game sky materials and mod preview keep their own rendering.");
  ImGui::Separator();
  if (
    ImGui::Combo(
      "UI / font scale",
      &uiScaleChoice,
      "100% (17 px)\0 125% (21 px)\0 150% (26 px)\0 175% (30 px)\0 200% (34 px)\0"))
    uiSaveAppearance();
  ImGui::SetNextItemWidth(140*uiScale());
  if(ImGui::SliderInt("Cursor size",&uiCursorPercent,50,250,"%d%%",ImGuiSliderFlags_AlwaysClamp))uiSaveAppearance();
  ImGui::SameLine();
  if(ImGui::Button("Reset cursor size")){uiCursorPercent=uiDefaultCursorPercent;uiSaveAppearance();}
  ImGui::TextWrapped("Cursor size is independent of UI / font scale and applies immediately.");
  ImGui::TextWrapped(
    "Dark panels use the TrenchBroom dark palette. Drag a panel tab to undock it or join "
    "another tab group. Drag dividers to resize.");
  if (ImGui::Checkbox("Lock panel layout", &uiLocked))
    uiSaveAppearance();
  if (ImGui::Button("Reset panel layout"))
    uiReset = true;
  ImGui::SameLine();
  if (ImGui::Button("Reset UI scale"))
  {
    uiScaleChoice = uiDefaultScaleChoice;
    uiSaveAppearance();
  }
  ImGui::TextWrapped(
    "Layout and scale are saved locally. Panels stay inside the editor window; no "
    "separate OS windows are created.");
}
