// SPDX-License-Identifier: GPL-2.0-or-later
static bool preferencesSave();
// Action contexts are OR alternatives within each group and AND between groups.
enum ActionContext : uint32_t
{
  CtxAny = 0,
  CtxView3D = 1 << 0,
  CtxView2D = 1 << 1,
  CtxAnyView = 3,
  CtxNoSelection = 1 << 2,
  CtxNodeSelection = 1 << 3,
  CtxFaceSelection = 1 << 4,
  CtxAnySelection = CtxNodeSelection | CtxFaceSelection,
  CtxAnyOrNoSelection = CtxNoSelection | CtxAnySelection,
  CtxNoTool = 1 << 5,
  CtxBrushTool = 1 << 6,
  CtxClipTool = 1 << 7,
  CtxRotateTool = 1 << 8,
  CtxScaleTool = 1 << 9,
  CtxShearTool = 1 << 10,
  CtxVertexTool = 1 << 11,
  CtxEdgeTool = 1 << 12,
  CtxFaceTool = 1 << 13,
  CtxControlPointTool = 1 << 14,
  CtxSweepTool = 1 << 15,
  CtxSculptTool = 1 << 16,
  CtxBendTool = 1 << 17,
  CtxPolygonCutTool = 1 << 18,
  CtxPolygonSpawnTool = 1 << 19,
  CtxSpinTool = 1 << 20,
  CtxModelEditor = 1 << 21,
  CtxAnyHandleTool = CtxVertexTool | CtxEdgeTool | CtxFaceTool | CtxControlPointTool,
  CtxAnyTool = 0x1fffc0,
  CtxAnyOrNoTool = 0x1fffe0
};
struct KeyChord
{
  int key;
  bool ctrl, alt, shift;
};
struct Action
{
  const char* id;
  const char* label;
  uint32_t context;
  std::vector<KeyChord> defaults;
  const char* command;
  bool (*enabled)();
  std::string shortcutText;
  const char* gesture;
};
static std::vector<Action> actions;
static bool actionReference = false, preferencesRequest = false, preferencesAppearanceRequest=false;
static void preferencesApplyActions();
static void initActions()
{
  static int definitionEpoch = -1, usageEpoch = -1;
  static size_t staticCount = 0;
  static std::map<std::string, std::array<std::string, 3>> dynamicText;
  if (!actions.empty())
  {
    if (definitionEpoch == entityDefinitionEpoch && usageEpoch == entitySelectionEpoch)
      return;
    actions.resize(staticCount);
    dynamicText.clear();
  }
  auto add = [](
               const char* id,
               const char* label,
               uint32_t ctx,
               const char* chords,
               const char* cmd,
               const char* gesture) {
    Action a{id, label, ctx, {}, cmd, nullptr, chords, gesture};
    std::string keys = chords;
    size_t start = 0;
    while (start < keys.size())
    {
      auto end = keys.find(';', start);
      auto chord = keys.substr(start, end == std::string::npos ? end : end - start);
      KeyChord k{0, false, false, false};
      for (auto modifier : {"Ctrl+", "Alt+", "Shift+"})
      {
        auto pos = chord.find(modifier);
        if (pos == std::string::npos)
          continue;
        if (modifier[0] == 'C')
          k.ctrl = true;
        else if (modifier[0] == 'A')
          k.alt = true;
        else
          k.shift = true;
        chord.erase(pos, std::char_traits<char>::length(modifier));
      }
      k.key = input->GetKeyCode(chord.c_str(), nullptr);
      a.defaults.push_back(k);
      if (end == std::string::npos)
        break;
      start = end + 1;
    }
    actions.push_back(std::move(a));
  };
  if (actions.empty())
  {
#include "ftebroom_actions.inc"
    staticCount = actions.size();
  }
  definitionEpoch = entityDefinitionEpoch;
  usageEpoch = entitySelectionEpoch;
  std::set<std::string> classes;
  for (const auto& item : entityDefinitions)
    classes.insert(item.first);
  for (const auto& item : entityUsage)
    classes.insert(item.first);
  for (const auto& name : classes)
  {
    if (name == "worldspawn")
      continue;
    auto id = "View Filter/Entity class/" + name;
    auto& text = dynamicText[id];
    text = {id, "Show/hide " + name, "ftebroom_filter_class " + organizationQuote(name)};
    add(text[0].c_str(), text[1].c_str(), CtxAny, "", text[2].c_str(), "");
    auto def = entityDefinitions.find(name);
    if (def == entityDefinitions.end() || def->second.brush)
      continue;
    id = "Create Entity/" + name;
    auto& create = dynamicText[id];
    create = {
      id, "Create " + name, "ftebroom_entity_at_cursor " + organizationQuote(name)};
    add(
      create[0].c_str(),
      create[1].c_str(),
      CtxAnyView | CtxNoTool,
      "",
      create[2].c_str(),
      "");
  }
  preferencesApplyActions();
}
static uint32_t currentActionContext()
{
  if(bbActive&&bbFocused)return CtxModelEditor;
  uint32_t ctx = cvars->GetFloat("ftebroom_ctx_view") == 0 ? CtxView3D : CtxView2D;
  int selection = (int)cvars->GetFloat("ftebroom_ctx_selection");
  ctx |= selection == 2   ? CtxFaceSelection
         : selection == 1 ? CtxNodeSelection
                          : CtxNoSelection;
  static const uint32_t contexts[] = {
    CtxNoTool,
    CtxBrushTool,
    CtxClipTool,
    CtxRotateTool,
    CtxSweepTool,
    CtxScaleTool,
    CtxShearTool,
    CtxVertexTool,
    CtxEdgeTool,
    CtxFaceTool,
    CtxControlPointTool, CtxSculptTool, CtxBendTool, CtxPolygonCutTool, CtxPolygonSpawnTool, CtxSpinTool, CtxRotateTool};
  int tool = std::clamp((int)cvars->GetFloat("ftebroom_tool"), 0, 16);
  ctx |= contexts[tool];
  return ctx;
}
static bool actionMatches(uint32_t required, uint32_t current)
{
  if(bool(required&CtxModelEditor)!=bool(current&CtxModelEditor))return false;
  for (uint32_t group :
       {uint32_t(CtxAnyView), uint32_t(CtxAnyOrNoSelection), uint32_t(CtxAnyOrNoTool)})
    if ((required & group) && !(required & current & group))
      return false;
  return true;
}
static bool jamActionAllowed(const Action& a)
{
  if (!jamEditing()) return true;
  const std::string_view id=a.id;
  if (id.rfind("Menu/File/",0)==0)
    return id=="Menu/File/Open..." || id=="Menu/File/Save" || id=="Menu/File/Save as..." ||
           id=="Menu/File/Close" || id=="Menu/File/Revert" || id=="Menu/File/Exit";
  if (id.rfind("Menu/Run/",0)==0)
    return id=="Menu/Run/Build" || id=="Menu/Run/Toggle BSP preview";
  return id!="Menu/View/Prefab collections" && id!="Menu/Tools/Unity Sync" &&
    id!="Menu/Tools/Control Point" && id!="Menu/Edit/Convert Selection to Patches" &&
    id.rfind("Menu/Model editor/",0)!=0;
}

static bool actionAvailable(const Action& a)
{
  if (!jamActionAllowed(a)) return false;
  if(gameClassic() && std::string(a.id)=="Menu/Tools/Control Point")return false;
  if((int)cvars->GetFloat("ftebroom_tool")==4)
  {
    const std::string id=a.id;
    if(id.rfind("Menu/Textures/",0)==0 ||
       (id.rfind("Menu/Edit/",0)==0 && id!="Menu/Edit/Undo" && id!="Menu/Edit/Redo"))return false;
  }
  return a.command && actionMatches(a.context, currentActionContext())
         && (!a.enabled || a.enabled());
}
static void executeAction(const Action& a, bool down = true)
{
  if (!jamActionAllowed(a)) return;
  if (!a.command)
  {
    if (down)
      core->Print(
        (std::string("ReyondBroom: ") + a.label + " is not implemented yet\n").c_str());
    return;
  }
  std::string cmd = a.command;
  if(cmd.rfind("bb ",0)==0){if(down)bbAction(cmd.substr(3));return;}
  if (cmd.rfind("ftebroom_hold ", 0) == 0)
  {
    const char* keys = "wsadqx";
    const char* found = std::strchr(keys, cmd.back());
    if (found){
      moveHeld[found - keys] = down;
      if(down)sceneContextCandidate=false;
    }
    return;
  }
  if (!down)
    return;
  if(modEmbedded && cmd!="ftebroom_ui exit"){if(cmd=="ftebroom_ui cancel"){if(modMenuOpen)modMenuOpen=false;else modOpenControls();}return;}
  if(generatorWindow&&(cmd=="ftebroom_undo"||cmd=="ftebroom_redo")){generatorHistoryStep(cmd=="ftebroom_redo");return;}
  if (cmd.rfind("ftebroom_ui ", 0) == 0)
  {
    auto name = cmd.substr(12);
    if (name == "cancel")
    {
      if (popupOpen || ImGui::GetIO().WantTextInput)
        return;
      if (sceneLook)
      {
        cameraEnd();
        return;
      }
      if(generatorWindow){generatorClose();return;}
      sceneDrag = false;
      command("ftebroom_cancel\n");
    }
    else if (name == "texturelock" || name == "uvlock")
    {
      const char* var =
        name == "texturelock" ? "ftebroom_texture_lock" : "ftebroom_uv_lock";
      cvars->SetFloat(var, !cvars->GetFloat(var));
    }
    else if (name == "cycle" || name == "maximize")
      viewAction(name);
    else if (
      name == "copy" || name == "cut" || name == "paste" || name == "pasteoriginal")
      clipboardAction(name);
    else if (name == "replacematerial")
      replaceMaterialRequest = true;
    else if (name == "moveobjects")
      moveObjectsRequest = true;
    else if (name == "selectline")
      selectLineRequest = true;
    else if (name == "preferences")
      preferencesRequest = true;
    else if (name == "issues")
      issuesWindow = true;
    else if (name == "build")buildWindow=true;
    else if (name == "buildnow")buildBegin();
    else if (name == "bsptoggle") {if(cvars->GetFloat("ftebroom_bsp_preview"))buildEdit();else buildShow();}
    else if (name == "compile" && jamEditing()) buildBegin();
    else if (name == "compile" || name == "launch" || name == "launchfte" || name == "rerun")
      runAction(name);
    else if (name == "clip")
    {
      cancelGesture();
      command("ftebroom_tool_toggle clip\n");
      selectMapTab = true;
    }
    else if (name == "close" && jamEditing()) jamLocal.open = true;
    else if (name == "new" || name == "close")
      documentRequest("__new");
    else if (name == "revert" && !documentFile.empty())
      documentRequest("@"+documentFile.u8string());
    else if (name == "exit")
      documentRequest("exit");
    else if (name == "save" && jamSaveCommand()) {}
    else if (name == "saveas" && jamEditing()) jamFileRequest(JamFileDialog::Save);
    else if (name == "save" && !documentUntitled)
      documentSave(documentName);
    else if (name == "save" || name == "saveas")
    {
      saveRequest = true;
      inspector = true;
    }
    else if (name == "open")
    {
      if (jamEditing()) jamFileRequest(JamFileDialog::Open);
      else recentOpen();
      inspector = true;
    }
    else if (name == "cameraposition")
      cameraPositionRequest = true;
    else if (name == "resources")
    {
      cancelGesture();
      setupNeeded = true;
    }
    else if (name == "grid")
      cvars->SetFloat("ftebroom_grid_visible", !cvars->GetFloat("ftebroom_grid_visible"));
    else if (name == "map")
      selectMapTab = true;
    else if (name == "exportmap")
      exportRequest = true;
    else if (name == "exporttrenchmap")
    {
      if(!gameClassic()&&!documentTerrain)trenchMapRequest=true;
      else documentError=documentTerrain?"TrenchBroom does not support FTE terrain sections.":"Use Save As. This profile already saves in TrenchBroom's MAP format.";
    }
    else if (name == "exportbspmodel")
      bspModelShow();
    else if (name == "exportobj")
      objRequest = true;
    else if (name == "loadpoints" || name == "loadportals")
      traceRequest = name == "loadpoints" ? 1 : 2;
    else if (name == "reloadpoints" || name == "reloadportals")
    {
      int kind = name == "reloadpoints" ? 1 : 2;
      if (!tracePaths[kind-1].empty()) traceChoose(kind, tracePaths[kind-1]);
    }
    else if (name == "group" || name == "renamegroups")
    {
      organizationNameRequest = true;
      organizationRename = name == "renamegroups";
    }
    else if (name == "entitylist")
      entityListWindow=true;
    else if (name == "prefabs")
      prefabShow();
    else if (name == "unity")
      unityShow();
    else if (name == "spawnmaster")
      spawnMasterShow();
    else if (name == "entity")
      selectEntityTab = true;
    else if (name == "material")
      selectMaterialTab = true;
    else if (name == "actions")
      actionReference = true;
    else if (name == "inspector")
      inspector = true;
    return;
  }
  if (cmd.rfind("ftebroom_tool_", 0) == 0)
  {
    cameraEnd();
    sceneDrag = sceneLook = false;
    selectMapTab = true;
  }
  if (cmd == "ftebroom_undo" || cmd == "ftebroom_redo" || cmd == "ftebroom_delete")
    cancelGesture();
  if(traceAction(cmd))return;
  command((cmd + "\n").c_str());
}
static bool dispatchAction(int key, bool down, bool inScene, int targetView=-1, bool lessonHistory=false)
{
  initActions();
  for(int f=1;f<=12;f++){auto name="f"+std::to_string(f);if(key==input->GetKeyCode(name.c_str(),nullptr))return false;}
  // Key release always clears held actions, even after modifiers or focus changed.
  if (!down)
  {
    for (auto& a : actions)
      if (a.command && std::string(a.command).rfind("ftebroom_hold ", 0) == 0)
        for (auto& k : a.defaults)
          if (k.key == key)
            executeAction(a, false);
    return false;
  }
  if (entityPropertyFocused && !inScene)
    for (const char* name :
         {"del",
          "tab",
          "enter",
          "leftarrow",
          "rightarrow",
          "uparrow",
          "downarrow",
          "backspace",
          "a"})
      if (key == input->GetKeyCode(name, nullptr))
        return false;
  const Action* chosen = nullptr;
  // Flight modifiers change speed instead of disabling held movement shortcuts.
  if (
    !ctrlHeld && inScene && !popupOpen && !ImGui::GetIO().WantTextInput && !sceneDrag
    && cvars->GetFloat("ca_brush_view") == 0)
  {
    const char* flight = "wsadqx";
    for (int i = 0; i < 6; i++)
    {
      char name[2] = {flight[i], 0};
      if (key == input->GetKeyCode(name, nullptr))
      {
        moveHeld[i] = true;
        sceneContextCandidate = false;
        return true;
      }
    }
  }
  if(tourActive&&!lessonHistory)return false;
  for (auto& a : actions)
  {
    if(lessonHistory&&std::string_view(a.id)!="Menu/Edit/Undo"&&std::string_view(a.id)!="Menu/Edit/Redo")
      continue;
    if (!actionMatches(a.context, currentActionContext()))
      continue;
    if (a.enabled && !a.enabled())
      continue;
    for (auto& k : a.defaults)
    {
      if (k.key != key || k.ctrl != ctrlHeld || k.alt != altHeld || k.shift != shiftHeld)
        continue;
      bool cancel = std::string(a.id) == "Menu/Tools/Cancel";
      if ((popupOpen || ImGui::GetIO().WantTextInput) && !cancel)
        continue;
      if ((a.context & CtxAnyView) && !inScene)
        continue;
      if (
        a.command && std::string(a.command).rfind("ftebroom_hold ", 0) == 0 && sceneDrag)
        continue;
      if (chosen)
        core->Print("ReyondBroom: ambiguous shortcut\n");
      else
        chosen = &a;
    }
  }
  if (chosen){
    if(targetView>=0&&chosen->command&&std::string_view(chosen->command)=="ftebroom_ui cycle")viewActivate(targetView);
    executeAction(*chosen);
  }
  return chosen != nullptr;
}
static std::string historyActionLabel(const Action& a);
enum class ToolbarEditAction { Undo, Redo, Delete, FlipHorizontal, FlipVertical };
static void actionToolButton(ToolbarEditAction tool,ImVec2 size)
{
  const char* ids[]={"Menu/Edit/Undo","Menu/Edit/Redo","Menu/Edit/Delete",
    "Menu/Transform/Flip horizontal","Menu/Transform/Flip vertical"};
  const char* id=ids[static_cast<int>(tool)];
  initActions();
  for (auto& a : actions)
    if (std::string_view(a.id) == id)
    {
      ImGui::BeginDisabled(!actionAvailable(a));
      ImGui::PushID(id);
      if(ImGui::Button("##icon",size))executeAction(a);
      const auto origin=ImGui::GetItemRectMin();
      const float extent=std::min(size.x,size.y);
      const auto point=[&](float x,float y){
        if(tool==ToolbarEditAction::Redo)x=1-x;
        if(tool==ToolbarEditAction::FlipVertical)std::swap(x,y);
        return ImVec2{origin.x+(size.x-extent)*.5f+x*extent,
                      origin.y+(size.y-extent)*.5f+y*extent};
      };
      auto* draw=ImGui::GetWindowDrawList();
      const auto color=ImGui::GetColorU32(ImGuiCol_Text);
      const float stroke=std::max(1.f,extent*.058f);
      if(tool==ToolbarEditAction::Delete){
        draw->AddLine(point(.24f,.34f),point(.76f,.34f),color,stroke);
        draw->AddLine(point(.42f,.24f),point(.58f,.24f),color,stroke);
        const ImVec2 body[]={point(.30f,.34f),point(.36f,.76f),point(.64f,.76f),point(.70f,.34f)};
        draw->AddPolyline(body,IM_ARRAYSIZE(body),color,0,stroke);
      }else if(tool==ToolbarEditAction::FlipHorizontal||tool==ToolbarEditAction::FlipVertical){
        if(ImGui::IsItemVisible())
          BrushIcons::drawTool(tool==ToolbarEditAction::FlipHorizontal?BrushIcons::Tool::FlipHorizontal:BrushIcons::Tool::FlipVertical,origin,size);
      }else{
        draw->PathLineTo(point(.25f,.42f));
        draw->PathBezierCubicCurveTo(point(.56f,.32f),point(.75f,.39f),point(.75f,.71f));
        draw->PathStroke(color,0,stroke);
        const ImVec2 arrow[]={point(.42f,.24f),point(.25f,.42f),point(.42f,.55f)};
        draw->AddPolyline(arrow,IM_ARRAYSIZE(arrow),color,0,stroke);
      }
      runUiRecordGlobal(std::string("tools/")+a.label);
      if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s%s%s",uiTr(historyActionLabel(a).c_str()),a.shortcutText.empty()?"":"  ",a.shortcutText.c_str());
      ImGui::PopID();
      ImGui::EndDisabled();
      return;
    }
}
static std::string historyActionLabel(const Action& a)
{
  const std::string id = a.id;
  if (id != "Menu/Edit/Undo" && id != "Menu/Edit/Redo")
    return a.label;
  if(generatorWindow)return std::string(a.label)+" settings";
  char name[160] = {};
  cvars->GetString(
    id == "Menu/Edit/Undo" ? "ftebroom_undo_name" : "ftebroom_redo_name",
    name,
    sizeof name);
  return std::string(a.label) + (*name ? " " + std::string(name) : "");
}
static bool visibilityAction(const Action& a)
{
  return std::string_view(a.id)=="Menu/View/Isolate"||std::string_view(a.id)=="Menu/View/Hide"||
         std::string_view(a.id)=="Menu/View/Show All";
}
static void actionMenuGroup(const char* group,bool visibility=false)
{
  std::string prefix = std::string("Menu/") + group + "/";
  for (auto& a : actions)
    if (
      std::string(a.id).rfind(prefix, 0) == 0
      && std::string(a.id).find('/', prefix.size()) == std::string::npos
      && visibilityAction(a)==visibility && jamActionAllowed(a))
    {
      if(gameClassic() && std::string(a.id)=="Menu/Tools/Control Point")continue;
      bool held = a.command && std::string(a.command).rfind("ftebroom_hold ", 0) == 0;
      if (
        ImGui::MenuItem(
          historyActionLabel(a).c_str(),
          a.shortcutText.c_str(),
          false,
          actionAvailable(a) && !held))
        executeAction(a);
      runUiRecordGlobal(a.id);
    }
}
static bool symmetryWindow=false;
static void symmetryPanel(){
  if(!symmetryWindow)return;
  ImGui::SetNextWindowSize(ImVec2(480,420),ImGuiCond_FirstUseEver);
  if(ImGui::Begin("Symmetry settings",&symmetryWindow)){
          int angle=(int)cvars->GetFloat("ftebroom_symmetry_angle");
          if(ImGui::SliderInt("Plane angle",&angle,-180,180,"%d degrees")){cvars->SetFloat("ftebroom_symmetry_angle",angle);preferencesSave();}
          bool custom=cvars->GetFloat("ftebroom_symmetry_custom_center")!=0;
          if(ImGui::Checkbox("Custom symmetry center",&custom)){cvars->SetFloat("ftebroom_symmetry_custom_center",custom);preferencesSave();}
          if(custom){float center[3]={cvars->GetFloat("ftebroom_symmetry_center_x"),cvars->GetFloat("ftebroom_symmetry_center_y"),cvars->GetFloat("ftebroom_symmetry_center_z")};
            if(ImGui::InputFloat3("Center XYZ",center,"%.1f")){cvars->SetFloat("ftebroom_symmetry_center_x",center[0]);cvars->SetFloat("ftebroom_symmetry_center_y",center[1]);cvars->SetFloat("ftebroom_symmetry_center_z",center[2]);preferencesSave();}}
          bool relink=cvars->GetFloat("ftebroom_symmetry_relink")!=0;
          if(ImGui::Checkbox("Relink matching existing groups",&relink)){cvars->SetFloat("ftebroom_symmetry_relink",relink);preferencesSave();}
          bool guides=cvars->GetFloat("ftebroom_symmetry_guides")!=0;
          if(ImGui::Checkbox("Show symmetry guides",&guides)){cvars->SetFloat("ftebroom_symmetry_guides",guides);preferencesSave();}
          ImGui::TextDisabled("Choose a mode below to apply these settings.");

    ImGui::Separator();actionMenuGroup("Edit/Symmetry");
    ImGui::TextWrapped("Existing groups are relinked only when every side matches. Otherwise new copies are created. Later linked edits synchronize geometry and unprotected entity properties.");
  }ImGui::End();
}
// Menu placement is independent of action IDs: saved shortcuts, command dispatch
// and UI automation retain the existing catalogue identifiers.
static void actionMenuEntry(const std::string& id,const char* label=nullptr,bool enabled=true)
{
  for(const auto& a:actions)if(id==a.id){
    if(!jamActionAllowed(a)||(gameClassic()&&id=="Menu/Tools/Control Point"))return;
    const bool held=a.command&&std::string_view(a.command).rfind("ftebroom_hold ",0)==0;
    bool checked=false;
    if(id=="Menu/Edit/Texture Lock")checked=cvars->GetFloat("ftebroom_texture_lock")!=0;
    if(id=="Menu/Edit/UV Lock")checked=cvars->GetFloat("ftebroom_uv_lock")!=0;
    if(ImGui::MenuItem(label?label:historyActionLabel(a).c_str(),a.shortcutText.c_str(),checked,enabled&&actionAvailable(a)&&!held))executeAction(a);
    runUiRecordGlobal(a.id);
    return;
  }
}
static void actionMenuEntries(const char* group,std::initializer_list<const char*> names)
{
  for(const char* name:names)actionMenuEntry(std::string("Menu/")+group+"/"+name);
}
static bool editSubmenu(const char* label)
{
  const bool opened=ImGui::BeginMenu(label);
  runUiRecordGlobal(std::string("Menu/Edit/")+label);
  return opened;
}
static void editGeometryMenu()
{
  actionMenuEntries("Edit",{"CSG Convex Merge","CSG Subtract","CSG Subtract 2","CSG Intersect","CSG Hollow"});
  ImGui::Separator();
  const auto generator=[](const char* label,int kind){
    if(ImGui::MenuItem(label)){command("ftebroom_generator cancel\n");generatorWindow=kind;generatorPreview=false;}
    runUiRecordGlobal(std::string("Menu/Edit/Geometry Tools/")+label);
  };
  generator("Bevel...",2);generator("Inset...",5);generator("Subdivision...",6);
  generator("Perforate...",4);generator("Fracture...",3);
  ImGui::Separator();
  generator("Rock...",8);
  if(ImGui::MenuItem("Spin...")){command("ftebroom_tool_toggle spin\n");generatorWindow=0;generatorPreview=false;}
  runUiRecordGlobal("Menu/Tools/Spin");
  generator("Terrain generator...",1);
  if(gameProfile==1)ImGui::MenuItem("Create FTE Engine terrain...",nullptr,&terrainCreateWindow);
  ImGui::Separator();
  actionMenuEntries("Edit",{"Convert Selection to Patches","Convert Triangulated Brushes"});
  ImGui::Separator();
  actionMenuEntries("Edit",{"Make Structural","Create func_detail"});
}
static void editToolsMenu()
{
  actionMenuEntries("Tools",{"Brush","Clip","Vertex","Edge","Face","Control Point"});
  ImGui::Separator();
  actionMenuEntries("Tools",{"Rotate","Scale","Shear","Sweep","Polygon Cut","Polygon Spawn","Bezier Bend","Sculpt"});
  ImGui::Separator();
  actionMenuEntries("Tools",{"Spawn Master","Unity Sync"});
  // Apply/cancel and tool-specific operations follow the currently active tool.
  bool separated=false;
  for(const auto& a:actions){
    const std::string_view id=a.id;
    if(id.rfind("Menu/Tools/",0)!=0||!actionAvailable(a))continue;
    const auto name=id.substr(11);
    if(name=="Brush"||name=="Clip"||name=="Vertex"||name=="Edge"||name=="Face"||name=="Control Point"||
       name=="Rotate"||name=="Scale"||name=="Shear"||name=="Sweep"||name=="Polygon Cut"||name=="Polygon Spawn"||
       name=="Bezier Bend"||name=="Sculpt"||name=="Spin"||name=="Spawn Master"||name=="Unity Sync")continue;
    if(!separated){ImGui::Separator();separated=true;}
    actionMenuEntry(a.id);
  }
}
static void editTransformMenu()
{
  actionMenuEntries("Edit",{"Move objects..."});
  for(const char* operation:{"Move","Rotate","Duplicate and move"}){
    if(ImGui::BeginMenu(operation)){
      for(const char* direction:{"Forward","Back","Left","Right","Up","Down"})
        actionMenuEntry(std::string("Menu/Transform/")+operation+" "+direction);
      ImGui::EndMenu();
    }
  }
  ImGui::Separator();
  actionMenuEntries("Transform",{"Flip horizontal","Flip vertical"});
  ImGui::Separator();
  actionMenuEntries("Edit",{"Snap Vertices to Grid","Snap Vertices to Integer"});
  ImGui::Separator();
  if(ImGui::MenuItem("Array..."))arrayOpen=true;
  runUiRecordGlobal("Menu/Edit/Transform/Array...");
  if(ImGui::MenuItem("Settle...")){command("ftebroom_generator cancel\n");generatorWindow=0;settleWindow=true;}
  runUiRecordGlobal("Menu/Edit/Transform/Settle...");
  if(ImGui::BeginMenu("Symmetry")){
    actionMenuGroup("Edit/Symmetry");ImGui::Separator();
    if(ImGui::MenuItem("Settings..."))symmetryWindow=true;
    ImGui::EndMenu();
  }
}
static void editTexturesMenu()
{
  actionMenuEntries("Edit",{"Replace Material..."});
  ImGui::Separator();
  actionMenuEntries("Edit",{"Texture Lock","UV Lock"});
  ImGui::Separator();
  actionMenuEntries("Textures",{"Reset alignment","Reset to world aligned","Flip horizontal","Flip vertical"});
  ImGui::Separator();
  for(const char* operation:{"Move","Rotate"})if(ImGui::BeginMenu(operation)){
    for(const auto& a:actions){
      const std::string prefix=std::string("Menu/Textures/")+operation+" ";
      if(std::string_view(a.id).rfind(prefix,0)==0)actionMenuEntry(a.id);
    }
    ImGui::EndMenu();
  }
}
static void editMenu()
{
  actionMenuEntries("Edit",{"Undo","Redo"});
  ImGui::Separator();
  actionMenuEntries("Edit",{"Cut","Copy","Paste","Paste at Original Position","Duplicate","Delete"});
  ImGui::Separator();
  if(editSubmenu("Selection")){
    actionMenuEntries("Edit",{"Select All","Deselect All","Invert Selection"});
    ImGui::Separator();
    actionMenuEntries("Edit",{"Select Touching","Select Inside","Select Tall","Select Siblings","Select Linked Groups"});
    ImGui::Separator();
    actionMenuEntries("Edit",{"Select by Line Number..."});
    ImGui::EndMenu();
  }
  if(editSubmenu("Tools")){editToolsMenu();ImGui::EndMenu();}
  if(editSubmenu("Geometry Tools")){editGeometryMenu();ImGui::EndMenu();}
  if(editSubmenu("Transform")){editTransformMenu();ImGui::EndMenu();}
  if(editSubmenu("Textures")){editTexturesMenu();ImGui::EndMenu();}
  if(editSubmenu("Groups")){
    actionMenuEntries("Edit",{"Group","Ungroup","Rename Groups","Create func_group"});
    ImGui::Separator();
    actionMenuEntries("Edit",{"Create Linked Duplicate","Separate Linked Groups","Extract Linked Group","Clear Protected Properties"});
    ImGui::EndMenu();
  }
  if(editSubmenu("Repeat")){
    actionMenuEntries("Edit",{"Repeat Last Commands","Clear Repeatable Commands"});
    ImGui::EndMenu();
  }
  ImGui::Separator();
  actionMenuEntries("Edit",{"Preferences..."});
}
static bool viewSubmenu(const char* label)
{
  const bool opened=ImGui::BeginMenu(label);
  runUiRecordGlobal(std::string("Menu/View/")+label);
  return opened;
}
static void viewMenu()
{
  actionMenuEntries("View",{"Focus Camera on Selection","Cycle map view","Maximize current view"});
  ImGui::Separator();
  if(viewSubmenu("Visibility")){
    actionMenuEntries("View",{"Isolate","Hide","Show All"});
    ImGui::Separator();
    bool outlines=cvars->GetFloat("ftebroom_show_edges")!=0;
    if(ImGui::MenuItem("Brush outlines",nullptr,outlines))cvars->SetFloat("ftebroom_show_edges",!outlines);
    runUiRecordGlobal("Menu/View/Brush outlines");
    if(ImGui::MenuItem("Viewport options..."))viewOptionsOpen=true;
    runUiRecordGlobal("Menu/View/Viewport options...");
    ImGui::EndMenu();
  }
  if(viewSubmenu("Grid")){
    actionMenuGroup("View/Grid");
    ImGui::Separator();
    float opacity=cvars->GetFloat("ftebroom_grid_alpha");
    if(ImGui::SliderFloat("Grid opacity",&opacity,0,1))cvars->SetFloat("ftebroom_grid_alpha",opacity);
    if(ImGui::IsItemDeactivatedAfterEdit())preferencesSave();
    int angle=(int)std::round(cvars->GetFloat("ftebroom_grid_rotation"));
    if(ImGui::SliderInt("Grid rotation",&angle,-180,180,"%d degrees")){cvars->SetFloat("ftebroom_grid_rotation",angle);preferencesSave();}
    if(ImGui::MenuItem("Reset grid rotation"))command("ftebroom_grid reset\n");
    ImGui::EndMenu();
  }
  if(viewSubmenu("Camera")){
    actionMenuEntries("View",{"Move Camera to...","Reset camera zoom","Reset 2D Cameras"});
    ImGui::EndMenu();
  }
  if(viewSubmenu("Panels")){
    actionMenuEntries("View",{"Map Inspector","Entity Inspector","Face Inspector"});
    ImGui::Separator();
    actionMenuEntries("View",{"Entity list"});
    ImGui::EndMenu();
  }
  if(viewSubmenu("Diagnostics")){
    actionMenuEntries("View",{"Issues"});
    ImGui::Separator();
    actionMenuEntries("View",{"Next Point","Previous Point"});
    if(!jamEditing()){
      ImGui::Separator();
      if(ImGui::BeginMenu("Point File")){
        actionMenuEntry("Menu/File/Load Point File...","Load...");
        actionMenuEntry("Menu/File/Reload Point File","Reload",!tracePaths[0].empty());
        actionMenuEntry("Menu/File/Unload Point File","Unload");
        ImGui::EndMenu();
      }
      if(ImGui::BeginMenu("Portal File")){
        actionMenuEntry("Menu/File/Load Portal File...","Load...");
        actionMenuEntry("Menu/File/Reload Portal File","Reload",!tracePaths[1].empty());
        actionMenuEntry("Menu/File/Unload Portal File","Unload");
        ImGui::EndMenu();
      }
    }
    ImGui::EndMenu();
  }
  if(viewSubmenu("Appearance")){
    if(ImGui::MenuItem("Appearance...")){preferencesRequest=true;preferencesAppearanceRequest=true;}
    runUiRecordGlobal("menu-preferences");
    runUiRecordGlobal("Menu/View/Appearance/Appearance...");
    ImGui::Separator();
    if(ImGui::BeginMenu("Interface theme")){
      for(int t=0;t<6;t++)if(ImGui::MenuItem(uiThemeNames[t],nullptr,uiTheme==t)){uiTheme=t;uiSaveAppearance();}
      ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Language")){
      for(int language=0;language<7;language++)if(ImGui::MenuItem(uiLanguageNames[language],nullptr,uiLanguage==language)){uiLanguage=language;uiSaveAppearance();}
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }
  ImGui::Separator();
  if(ImGui::MenuItem("Fullscreen",nullptr,uiFullscreen))uiFullscreenSet(!uiFullscreen);
  runUiRecordGlobal("Menu/View/Fullscreen");
  if(!uiFullscreenError.empty())ImGui::TextWrapped("%s",uiFullscreenError.c_str());
}
static void fileMenu()
{
  actionMenuEntry("Menu/File/New","New Map");
  actionMenuEntry("Menu/File/Open...","Open Map...");
  ImGui::Separator();
  actionMenuEntry("Menu/File/Save");
  actionMenuEntry("Menu/File/Save as...","Save As...");
  ImGui::Separator();
  if(!jamEditing()&&ImGui::BeginMenu("Export")){
    actionMenuEntries("File",{"Export Map...","Export TrenchBroom MAP...","Export OBJ..."});
    actionMenuEntry("Menu/File/Export selected BSP model...",nullptr,cvars->GetFloat("ftebroom_selection_count")>0);
    ImGui::EndMenu();
  }
  actionMenuEntry("Menu/File/Revert","Revert to Saved...",
    !documentUntitled&&!documentFile.empty()&&!savingDocument&&cvars->GetFloat("ftebroom_dirty")!=0);
  ImGui::Separator();
  actionMenuEntry("Menu/File/Close","Close Map");
  actionMenuEntry("Menu/File/Exit");
}
static void assetsMenu()
{
  if(!ImGui::BeginMenu("Assets"))return;
  actionMenuEntry("Menu/File/New model...","New Model...");
  if(ImGui::MenuItem("Texture Studio..."))textureStudioShow();
  runUiRecordGlobal("Menu/Assets/Texture Studio...");
  if(ImGui::MenuItem("Plant Editor..."))plantShow();
  runUiRecordGlobal("Menu/Assets/Plant Editor...");
  if(ImGui::MenuItem("Prefab collections"))prefabShow();
  runUiRecordGlobal("Menu/Assets/Prefab collections");
  ImGui::Separator();
  if(ImGui::BeginMenu("Reload Resources")){
    actionMenuEntries("File",{"Reload Material Collections","Reload Entity Definitions"});
    ImGui::EndMenu();
  }
  if(gameProfile==1&&ImGui::BeginMenu("Shaders")){
    if(ImGui::MenuItem("Reyond's GLSL Shader Pack..."))shaderPackOpen=true;
    ImGui::EndMenu();
  }
  ImGui::EndMenu();
}
static void actionMenus()
{
  initActions();
  if(runUiLive)runUiGlobalRects=nlohmann::json::object();
  for (auto group : {"File", "Edit", "View", "Run", "Help"}){
    const bool opened=ImGui::BeginMenu(group);runUiRecordGlobal(std::string("menu/")+group);
    if (std::string_view(group) == "File") placementRect("menu/file");
    if (opened)
    {
      if(std::string_view(group)=="File")fileMenu();
      else if(std::string_view(group)=="Edit")editMenu();
      else if(std::string_view(group)=="View")viewMenu();
      else actionMenuGroup(group);
      if(!jamEditing()&&std::string(group)=="Help"&&ImGui::MenuItem("Interactive onboarding..."))documentRequest("__tour");
      if(std::string_view(group)=="Help"){
        if(ImGui::MenuItem("Updates..."))recentOpenUpdates();
        runUiRecordGlobal("Menu/Help/Updates...");
      }
      ImGui::EndMenu();
    }
  }
}
static void actionReferenceWindow()
{
  if (actionReference)
  {
    ImGui::OpenPopup("Action reference");
    actionReference = false;
  }
  ImGui::SetNextWindowSize({760, 500}, ImGuiCond_FirstUseEver);
  if (uiDialog("Action reference",{760,540}))
  {
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape))
      ImGui::CloseCurrentPopup();
    ImGui::TextWrapped(
      "Reserved actions are not implemented. Mouse gestures are listed as roadmap "
      "references.");
    if (
      ImGui::BeginTable(
        "actions",
        3,
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg,
        {0, 400}))
    {
      ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 4);
      ImGui::TableSetupColumn(
        "Shortcut / gesture", ImGuiTableColumnFlags_WidthStretch, 3);
      ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 2);
      ImGui::TableHeadersRow();
      for (auto& a : actions)
      {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", a.label);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s", a.id);
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", *a.gesture ? a.gesture : a.shortcutText.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(
          *a.gesture  ? "Gesture reference"
          : a.command ? "Implemented"
                      : "Reserved");
      }
      ImGui::EndTable();
    }
    ImGui::EndPopup();
  }
}
static void dumpActions()
{
  initActions();
  core->Print("FTEBROOM_ACTIONS_BEGIN\n");
  for (auto& a : actions)
  {
    std::string codes;
    for (auto& k : a.defaults)
    {
      if (!codes.empty())
        codes += ";";
      codes += std::to_string(k.key) + ":" + std::to_string(k.ctrl)
               + std::to_string(k.alt) + std::to_string(k.shift);
    }
    core->Print((std::string("FTEBROOM_ACTION\t") + a.id + "\t" + a.label + "\t"
                 + std::to_string(a.context) + "\t" + a.shortcutText + "\t" + codes + "\t"
                 + (a.command ? "implemented" : "reserved") + "\t" + a.gesture + "\n")
                  .c_str());
  }
  core->Print("FTEBROOM_ACTIONS_END\n");
}
