// SPDX-License-Identifier: GPL-2.0-or-later
static void testWorkspace()
{
  if (!cvars->GetFloat("ftebroom_skip_setup") || commands->IsInsecure())
    return;
  auto expect = [](bool ok, const char* name) {
    core->Print((std::string(ok ? "FTEBROOM_WORKSPACE_PASS " : "FTEBROOM_WORKSPACE_FAIL ")
                 + name + "\n")
                  .c_str());
  };
  char mode[64] = {};
  commands->Argv(1, mode, sizeof mode);
  const std::string_view testMode=mode;
  const bool flipHorizontal=testMode=="workspace-flip-horizontal";
  const bool flipVertical=testMode=="workspace-flip-vertical";
  auto frame = [](float x = 640, float y = 400) { event(0, 0, 0, x, y, 1280, 800); };
  auto& io = ImGui::GetIO();
  const bool trickle = io.ConfigInputTrickleEventQueue;
  io.ConfigInputTrickleEventQueue = false;
  if(std::string(mode)=="entity-inspector"){
    const int width=std::max(640,int(io.DisplaySize.x)), height=std::max(480,int(io.DisplaySize.y));
    auto refresh=[&](ImVec2 pointer={0,0}){event(0,0,0,pointer.x,pointer.y,width,height);};
    modFilesOpen=true;
    selectEntityTab=true;
    for(int i=0;i<5;i++)refresh();
    const auto* inspector=ImGui::FindWindowByName("Entity");
    const auto* tools=ImGui::FindWindowByName("Tools");
    const auto* files=ImGui::FindWindowByName(uiModFilesName);
    expect(inspector&&tools&&files&&tools->DockId&&files->DockId&&uiStatusInFiles&&
      tools->Rect().Max.x<=inspector->Pos.x&&files->Rect().Max.x<=inspector->Pos.x&&
      std::abs(inspector->Pos.y-tools->Pos.y)<2&&std::abs(inspector->Rect().Max.y-files->Rect().Max.y)<2,
      "entity_inspector_spans_history_and_status");
    if(auto item=entityTestItems.find("value/targetname");item!=entityTestItems.end()){
      const auto point=item->second;
      auto click=[&]{
        refresh(point);
        event(1,mouseKeys[0],0,point.x,point.y,width,height);
        refresh(point);
        event(2,mouseKeys[0],0,point.x,point.y,width,height);
        refresh(point);
      };
      click();click();refresh(point);
      char editing[160]{};
      cvars->GetString("ftebroom_property_editing",editing,sizeof editing);
      expect(std::string(editing)=="targetname","entity_inspector_existing_value_edit");
    }else expect(false,"entity_inspector_existing_value_edit");
  }
  else if(testMode=="entity-class-defaults"||testMode=="entity-class-create"){
    const bool create=testMode=="entity-class-create";
    const float doubleClickTime=io.MouseDoubleClickTime;
    if(!create)io.MouseDoubleClickTime=0;
    cvars->SetString("ftebroom_test_entity_filter","qa_fgd_");
    selectEntityTab=true;
    auto refresh=[&](ImVec2 p={0,0}){event(0,0,0,p.x,p.y,1920,1080);};
    auto findTile=[&](const char* name){
      entityTestItems.clear();
      refresh();
      refresh();
      auto tile=entityTestItems.find(std::string("class:")+name);
      return tile==entityTestItems.end()?ImVec2{-1,-1}:tile->second;
    };
    auto click=[&](ImVec2 p){
      if(p.x<0)return false;
      refresh(p);
      event(1,mouseKeys[0],0,p.x,p.y,1920,1080);
      refresh(p);
      event(2,mouseKeys[0],0,p.x,p.y,1920,1080);
      refresh(p);
      refresh(p);
      return true;
    };
    const auto first=findTile("qa_fgd_alpha");
    if(create){
      expect(click(first)&&click(first),"entity_browser_double_click");
    }else{
      expect(click(first)&&entityTestItems.count("class-default/shared=17")&&
        entityTestItems.count("class-default/alpha_only=amber")&&
        entityTestItems.count("class-default/optional=")&&
        !entityTestItems.count("class-default/beta_only=blue"),
        "entity_browser_first_class_defaults");
      const auto second=findTile("qa_fgd_beta");
      const bool clicked=click(second);
      entityTestItems.clear();
      refresh(second);
      expect(clicked&&entityTestItems.count("class-default/shared=29")&&
        entityTestItems.count("class-default/beta_only=blue")&&
        !entityTestItems.count("class-default/alpha_only=amber")&&
        cvars->GetFloat("ftebroom_entity_selection_count")==0,
        "entity_browser_second_class_defaults_without_creating");
    }
    io.MouseDoubleClickTime=doubleClickTime;
  }
  else if(testMode=="workspace-controls"||testMode=="workspace-controls-default-scale"||flipHorizontal||flipVertical){
    const bool flip=flipHorizontal||flipVertical;
    if(!flip){
      uiReset=true;
      uiLocked=false;
      uiScaleChoice=testMode=="workspace-controls"?0:uiDefaultScaleChoice;
      modFilesOpen=false;
      cvars->SetFloat("ftebroom_browser132_qa",1);
    }
    extraToolsExpanded=false;
    runUiLive=true;
    cvars->SetFloat("ftebroom_tool",0);
    auto refresh=[&](ImVec2 pointer={0,0}){event(0,0,0,pointer.x,pointer.y,1920,1080);};
    auto frames=[&]{for(int i=0;i<6;i++)refresh();};
    auto clickAt=[&](ImVec2 p,int button){
      refresh(p);
      event(1,mouseKeys[button],0,p.x,p.y,1920,1080);
      refresh(p);
      event(2,mouseKeys[button],0,p.x,p.y,1920,1080);
      refresh(p);
    };
    auto click=[&](const char* id){
      const auto item=browserQaWidgets.find(id);
      if(item==browserQaWidgets.end()||!item->second.visible)return false;
      const auto& rect=item->second;
      clickAt({(rect.min.x+rect.max.x)*.5f,(rect.min.y+rect.max.y)*.5f},0);
      return true;
    };
    auto clickTool=[&](const char* id,int button){
      if(!runUiGlobalRects.contains(id))return false;
      const auto& rect=runUiGlobalRects[id];
      clickAt({rect.value("x",0.f)+rect.value("width",0.f)*.5f,
               rect.value("y",0.f)+rect.value("height",0.f)*.5f},button);
      return true;
    };
    frames();
    if(flip){
      const bool selected=cvars->GetFloat("ftebroom_ctx_selection")==1;
      expect(selected&&clickTool(flipHorizontal?"tools/Flip horizontal":"tools/Flip vertical",0),mode);
      io.ConfigInputTrickleEventQueue=trickle;
      return;
    }
    const auto* tools=ImGui::FindWindowByName("Tools");
    float top=0,bottom=FLT_MAX;
    bool fit=tools&&uiToolsHorizontal(tools);
    bool uniform=true;
    const auto& first=runUiGlobalRects.value("tools/Undo",nlohmann::json::object());
    const float width=first.value("width",0.f), height=first.value("height",0.f);
    for(const auto* key:{"tools/Undo","tools/Redo","tools/Delete","tools/default-move","tools/Brush",
                        "tools/Clip","tools/Vertex","tools/Edge","tools/Face","tools/Control Point","tools/Rotate",
                        "tools/Sweep","tools/Scale","tools/Shear","tools/Flip horizontal","tools/Flip vertical",
                        "tools-extra-toggle","tools/Integer snap","tools/Texture Lock","tools/UV Lock",
                        "tools/Grid","tools/View","tools/Layout","tools/Compile","tools/Setup compile",
                        "tools/Show BSP","tools/Play mod","tools-shape-toggle"}){
      if(gameClassic()&&std::string_view(key)=="tools/Control Point")continue;
      if((gameProfile==0||gameProfile==3)&&std::string_view(key)=="tools/Play mod")continue;
      if(!runUiGlobalRects.contains(key)){fit=false;uniform=false;continue;}
      const auto& rect=runUiGlobalRects[key];
      top=std::max(top,rect.value("y",0.f));
      bottom=std::min(bottom,rect.value("y",0.f)+rect.value("height",0.f));
      if(tools&&rect.value("x",0.f)+rect.value("width",0.f)>tools->InnerRect.Max.x)fit=false;
      uniform=uniform&&std::abs(rect.value("width",0.f)-width)<.5f&&
        std::abs(rect.value("height",0.f)-height)<.5f;
    }
    float previousEnd=0;
    for(const auto* key:{"tools/UV Lock","tools/Grid","tools/View","tools/Layout","tools/Compile"}){
      if(!runUiGlobalRects.contains(key)){fit=false;continue;}
      const auto& rect=runUiGlobalRects[key];
      const float x=rect.value("x",0.f);
      fit=fit&&x>=previousEnd;
      previousEnd=x+rect.value("width",0.f);
    }
    expect(fit&&top<bottom,"workspace_tools_single_row_1920");
    expect(uniform&&!extraToolsExpanded,"workspace_tools_equal_size_with_flip_buttons");
    expect(clickTool("tools/default-move",1)&&runUiGlobalRects.contains("tools/shape-choice"),
      "workspace_default_move_settings_open_from_icon");
    ImGui::ClosePopupsOverWindow(nullptr,false);
    frames();
    const bool expanded=clickTool("tools-shape-toggle",0)&&runUiGlobalRects.contains("tools/shape-choice");
    frames();
    const bool collapsed=clickTool("tools-shape-toggle",0)&&!runUiGlobalRects.contains("tools/shape-choice");
    expect(expanded&&collapsed,"workspace_default_move_arrow_expands_and_collapses");
    frames();
    for(const auto* key:{"tools/Grid","tools/View","tools/Layout"}){
      const bool open=clickTool(key,0)&&ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
      expect(open,(std::string("workspace_dropdown_opens_")+std::string(key).substr(6)).c_str());
      ImGui::ClosePopupsOverWindow(nullptr,false);
      frames();
    }
    expect(browserQaWidgets.count("files/standalone-status")&&click("files/show"),"workspace_show_from_status");
    frames();
    auto footerOwnsStatus=[&]{
      const auto files=browserQaWidgets.find("files/panel");
      const auto document=browserQaWidgets.find("files/status-document");
      return files!=browserQaWidgets.end()&&document!=browserQaWidgets.end()&&
        document->second.min.y>=files->second.min.y&&document->second.max.y<=files->second.max.y&&
        browserQaWidgets.count("files/hide")&&!browserQaWidgets.count("files/standalone-status");
    };
    expect(uiStatusInFiles&&footerOwnsStatus(),"workspace_bottom_files_owns_status");
    if(const auto* entity=ImGui::FindWindowByName("Entity")){
      ImGui::DockBuilderDockWindow(uiModFilesName,entity->DockId);
      ImGui::DockBuilderFinish(uiDockId);
      modFilesRequestFocus=true;
    }
    frames();
    expect(!uiStatusInFiles&&browserQaWidgets.count("files/standalone-status")&&browserQaWidgets.count("files/hide"),
      "workspace_side_files_restores_status");
    expect(click("files/hide"),"workspace_side_files_hide_button");
    frames();
    expect(!modFilesOpen&&browserQaWidgets.count("files/show")&&browserQaWidgets.count("files/standalone-status"),
      "workspace_status_returns_after_hide");
  }
  else if(std::string(mode)=="rb10tools"){
    uiScaleChoice=0;uiLocked=false;uiReset=true;extraToolsExpanded=false;
    cvars->SetFloat("ftebroom_tool",0);
    auto toolsFrame=[](){event(0,0,0,300,350,640,800);};
    for(int i=0;i<5;i++)toolsFrame();
    auto* tools=ImGui::FindWindowByName("Tools");
    expect(tools&&tools->DockNode&&tools->DockNode->ParentNode&&tools->DockNode->ParentNode->SplitAxis==ImGuiAxis_Y&&tools->DockNode==tools->DockNode->ParentNode->ChildNodes[0],"rb10_tools_default_top");
    expect(uiToolsHorizontal(tools)&&tools->ScrollMax.x<1&&tools->ScrollMax.y<1,"rb10_tools_compact_content_fits");
    const float collapsed=tools->Size.y;
    extraToolsExpanded=true;for(int i=0;i<5;i++)toolsFrame();
    expect(tools->Size.y>collapsed&&tools->ScrollMax.x<1&&tools->ScrollMax.y<1,"rb10_tools_expansion_grows_height");
    extraToolsExpanded=false;for(int i=0;i<5;i++)toolsFrame();
    expect(std::abs(tools->Size.y-collapsed)<2,"rb10_tools_collapse_reclaims_height");
    auto* central=ImGui::DockBuilderGetCentralNode(uiDockId);ImGuiID side,rest;
    ImGui::DockBuilderSplitNode(central->ID,ImGuiDir_Left,.25f,&side,&rest);
    ImGui::DockBuilderDockWindow("Tools",side);ImGui::DockBuilderFinish(uiDockId);
    for(int i=0;i<4;i++)toolsFrame();
    expect(tools->DockId==side&&!uiToolsHorizontal(tools),"rb10_tools_custom_side_dock_preserved");
    uiReset=true;
  }
  else if (std::string(mode).rfind("tools65",0)==0) {
    extraToolsExpanded=std::string(mode).find("expanded")!=std::string::npos;
    uiScaleChoice=0;uiLocked=false;uiReset=true;frame();frame();
    if(std::string(mode).find("left")!=std::string::npos || std::string(mode).find("right")!=std::string::npos) {
      auto central=ImGui::DockBuilderGetCentralNode(uiDockId);
      ImGuiID side,rest;
      ImGui::DockBuilderSplitNode(central->ID,std::string(mode).find("left")!=std::string::npos?ImGuiDir_Left:ImGuiDir_Right,.35f,&side,&rest);
      ImGui::DockBuilderDockWindow("Tools",side);ImGui::DockBuilderFinish(uiDockId);
      frame();frame();
      auto tools=ImGui::FindWindowByName("Tools");
      ImGui::SetScrollY(tools,std::string(mode).find("params")!=std::string::npos?tools->ScrollMax.y:0);frame();frame();
      expect(tools->ScrollMax.x<1,"tools65_vertical_no_horizontal_overflow");
    }
  }
  else if (std::string(mode) == "load")
  {
    uiLoaded = false;
    uiAppliedScale = 0;
    cvars->SetFloat("ftebroom_skip_setup", 0);
    uiLoad();
    cvars->SetFloat("ftebroom_skip_setup", 1);
    frame();
    frame();
    expect(uiScaleChoice == 1 && uiLocked, "preferences_restored");
    auto map = ImGui::FindWindowByName("Map");
    expect(
      map && !map->DockId && std::abs(map->Pos.x - 180) < 4, "floating_layout_restored");
    uiLocked = false;
    uiReset = true;
    uiScaleChoice = 0;
    frame();
    frame();
    expect(ImGui::FindWindowByName("Map")->DockId != 0, "reset_redocks_panels");
  }
  else if (std::string(mode) == "large")
  {
    uiScaleChoice = 4;
    preferencesRequest = true;
    frame();
    frame();
    expect(std::abs(ImGui::GetFontSize() - 34) < 1, "large_font");
    expect(viewArea.w > 200 && viewArea.h > 100, "large_font_scene_survives");
  }
  else
  {
    uiReset = true;
    frame();
    frame();
    expect(uiHasScene && viewArea.w > 300 && viewArea.h > 200, "central_scene");
    const float sx = viewArea.x + viewArea.w * .5f;
    const float sy = viewArea.y + viewArea.h * .5f;
    expect(!uiBlocksScene(sx, sy), "central_mouse_passthrough");
    auto map = ImGui::FindWindowByName("Map");
    auto material = ImGui::FindWindowByName("Face");
    expect(map->DockId && map->DockId == material->DockId, "default_tab_group");
    auto right = ImGui::DockBuilderGetNode(map->DockId);
    const float divider = right->Pos.x - 2;
    const float dividerY = right->Pos.y + right->Size.y * .5f;
    const float oldWidth = viewArea.w;
    frame(divider, dividerY);
    event(1, mouseKeys[0], 0, divider, dividerY, 1280, 800);
    frame(divider, dividerY);
    frame(divider - 80, dividerY);
    event(2, mouseKeys[0], 0, divider - 80, dividerY, 1280, 800);
    frame(divider - 80, dividerY);
    frame();
    expect(!sceneDrag && viewArea.w < oldWidth - 40, "dock_splitter_resizes_scene");
    ImGui::SetWindowDock(map, 0, ImGuiCond_Always);
    ImGui::SetWindowPos(map, {180, 240}, ImGuiCond_Always);
    ImGui::SetWindowSize(map, {390, 410}, ImGuiCond_Always);
    selectMapTab = true;
    frame();
    frame();
    expect(!map->DockId, "floating_panel");
    expect(uiBlocksScene(200, 280), "floating_panel_blocks_scene");
    event(1, mouseKeys[0], 0, 200, 250, 1280, 800);
    expect(!sceneDrag, "title_click_does_not_start_brush");
    frame(200, 250);
    event(2, mouseKeys[0], 0, 200, 250, 1280, 800);
    frame();
    frame();
    uiScaleChoice = 1;
    uiLocked = true;
    frame();
    frame();
    expect(std::abs(ImGui::GetFontSize() - 21.25f) < 1, "scaled_font_raster");
    // Exercise the real local persistence paths in the disposable test bundle.
    cvars->SetFloat("ftebroom_skip_setup", 0);
    uiSaveAppearance();
    io.WantSaveIniSettings = true;
    uiSaveLayout();
    cvars->SetFloat("ftebroom_skip_setup", 1);
    expect(std::filesystem::exists(documentPath("workspace.ini")), "layout_written");
  }
  io.ConfigInputTrickleEventQueue = trickle;
}
