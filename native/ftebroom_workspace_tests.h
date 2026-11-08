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
  auto frame = [](float x = 640, float y = 400) { event(0, 0, 0, x, y, 1280, 800); };
  auto& io = ImGui::GetIO();
  const bool trickle = io.ConfigInputTrickleEventQueue;
  io.ConfigInputTrickleEventQueue = false;
  if(std::string(mode)=="rb10tools"){
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
