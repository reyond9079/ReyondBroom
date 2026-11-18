// SPDX-License-Identifier: GPL-2.0-or-later
static void bspModelShow(bool mdl=false,bool worldOnly=false);
static void plantBrushShow();
namespace { static void characterCreateHere(); }
static bool sceneContextPending = false, sceneContextCandidate = false;
static int sceneContextSerial = 0,sceneContextEpoch=0;
static quintptr_t sceneContextTime, sceneContextDuration;
static float sceneContextDX = 0, sceneContextDY = 0, sceneContextX = 0, sceneContextY = 0;
static bool organizationNameRequest = false, organizationRename = false;
static void sceneContextBegin(float x, float y)
{
  sceneContextCandidate = !altHeld && !ctrlHeld && !shiftHeld;
  for(bool moving:moveHeld)if(moving)sceneContextCandidate=false;
  sceneContextTime = core->GetMilliseconds();
  sceneContextDuration = 0;
  sceneContextDX = sceneContextDY = 0;
  sceneContextX = x;
  sceneContextY = y;sceneContextEpoch=organizationMapEpoch;
  sceneContextSerial=int(cvars->GetFloat("ftebroom_context_serial"));
  command(("ftebroom_context "+std::to_string(x)+" "+std::to_string(y)+"\n").c_str());
}
static void sceneContextRelease()
{
  if(cameraButton==1)sceneContextDuration=core->GetMilliseconds()-sceneContextTime;
  if (
    !sceneLook || cameraButton != 1 || !sceneContextCandidate)
    return;
  // A stationary release is a context click even after a slow rendered frame.
  // Actual mouse/keyboard/wheel navigation invalidates the candidate instead.
  if (sceneContextDX * sceneContextDX + sceneContextDY * sceneContextDY >= 16)
    return;
  if(sceneContextEpoch!=organizationMapEpoch)return;
  sceneContextPending = true;
}
static void sceneContextEntityMenu(bool brush)
{
  const bool open=ImGui::BeginMenu(brush?"Create brush entity":"Create point entity");
  if(brush)placementRect("context/brush-entity");
  if(!open)return;

  std::map<std::string,std::vector<std::string>> categories;
  for(const auto& item:entityDefinitions)
    if(item.first!="worldspawn" && item.second.brush==brush)
      categories[item.first.substr(0,item.first.find('_'))].push_back(item.first);
  for(const auto& category:categories){
    const bool categoryOpen=ImGui::BeginMenu(category.first.c_str());
    if(brush&&placementQa())placementRect(("context/brush-category/"+category.first).c_str());
    if(!categoryOpen)continue;
    for(const auto& name:category.second){
      const bool clicked=ImGui::MenuItem(name.c_str());
      if(brush&&placementQa())placementRect(("context/brush-class/"+name).c_str());
      if(!clicked)continue;
      if(brush){
        float position[3]{};
        entityCreate(entityDefinitions.at(name),position);
      }else{
        command(("ftebroom_entity_drop "+organizationQuote(name)+" "+
          std::to_string(sceneContextX)+" "+std::to_string(sceneContextY)+"\n").c_str());
      }
    }
    ImGui::EndMenu();
  }
  ImGui::EndMenu();
}
static void sceneContextCreateMenu()
{
  if(jamEditing()&&gameProfile!=1)return;
  const bool create=ImGui::BeginMenu("Create");
  placementRect("context/create");
  if(!create)return;
  if(!jamEditing()){
    const bool model=ImGui::BeginMenu("Model");
    placementRect("context/model");
    if(model){
      if(ImGui::MenuItem("3d model"))placementCreate(false);
      placementRect("context/new-model");
      if(ImGui::MenuItem("Plant",nullptr,false,viewTypes[viewFocus]==0))plantBrushShow();
      placementRect("context/plant-brush");
      if(ImGui::MenuItem("New plant model..."))placementCreate(true);
      placementRect("context/new-plant");
      if(ImGui::MenuItem("Character..."))characterCreateHere();
      placementRect("context/new-character");
      ImGui::EndMenu();
    }
  }
  if(gameProfile==1 && ImGui::MenuItem("Create FTE Engine terrain here...")){
    terrainCreateWindow=true;
    command(("ftebroom_terrain_position "+std::to_string(sceneContextX)+" "+std::to_string(sceneContextY)+"\n").c_str());
  }
  ImGui::EndMenu();
}
static void sceneContextActions()
{
  const bool brushes=cvars->GetFloat("ftebroom_selection_count")>0;
  const bool selected=brushes||cvars->GetFloat("ftebroom_entity_selection_count")>0;
  const bool hit=cvars->GetFloat("ftebroom_context_hit")!=0;
  const bool worldBrushes=cvars->GetFloat("ftebroom_context_world_brushes")!=0;
  const int group=int(cvars->GetFloat("ftebroom_context_group"));
  const int selectedGroups=int(cvars->GetFloat("ftebroom_context_selected_groups"));
  const int owner=int(cvars->GetFloat("ftebroom_context_entity"));
  const int layer=int(cvars->GetFloat("ftebroom_context_layer"));
  char classname[256]{},material[1024]{};
  cvars->GetString("ftebroom_context_class",classname,sizeof classname);
  cvars->GetString("ftebroom_context_material",material,sizeof material);

  if(*material&&ImGui::MenuItem("Reveal material in browser")){
    cvars->SetString("ftebroom_reveal_material",material);
    selectMaterialTab=true;
  }
  if(brushes&&hit)sceneContextEntityMenu(true);
  sceneContextEntityMenu(false);
  if(brushes&&!hit)sceneContextEntityMenu(true);
  sceneContextCreateMenu();

  const bool canConvert=worldBrushes&&!jamEditing();
  if(selected||layer>=0||*classname){
    ImGui::Separator();
    if(selected&&ImGui::BeginMenu("Group")){
      if(ImGui::MenuItem("Group...")){organizationNameRequest=true;organizationRename=false;}
      if(selectedGroups&&ImGui::MenuItem("Ungroup"))organize("ungroup");
      if(selectedGroups>1&&ImGui::MenuItem("Merge groups"))organize("mergegroups");
      if(selectedGroups&&ImGui::MenuItem("Rename groups...")){organizationNameRequest=true;organizationRename=true;}
      if(group&&ImGui::MenuItem("Add objects to this group"))organize("move",group);
      if(cvars->GetFloat("ftebroom_context_open_group")&&ImGui::MenuItem("Remove objects from group"))
        organize("removefromgroup");
      ImGui::EndMenu();
    }
    if(selected&&ImGui::BeginMenu("Visibility")){
      if(ImGui::MenuItem("Hide selection"))command("ftebroom_visibility hide\n");
      if(ImGui::MenuItem("Isolate selection"))command("ftebroom_visibility isolate\n");
      ImGui::EndMenu();
    }
    if((selected||layer>=0)&&ImGui::BeginMenu("Layer")){
      if(selected&&ImGui::BeginMenu("Move to layer")){
        for(const auto& n:organizationNodes)
          if(n.kind==1&&!(n.flags&3)&&ImGui::MenuItem(n.name.c_str()))organize("move",n.id);
        ImGui::EndMenu();
      }
      if(layer>=0){
        if(layer!=int(cvars->GetFloat("ftebroom_context_active_layer"))&&ImGui::MenuItem("Make layer active"))
          organize("layeractive",layer);
        if(ImGui::MenuItem("Hide this layer"))organize("layerhidden",layer,"1");
        if(ImGui::MenuItem("Select all in this layer"))organize("layerselect",layer);
        if(ImGui::MenuItem("Isolate this layer")){
          std::vector<std::pair<int,int>> values;
          for(const auto& n:organizationNodes)
            if(n.kind==1)values.emplace_back(n.id,n.id!=layer);
          organizationBatch("layerhidden",values);
        }
      }
      ImGui::EndMenu();
    }
    if(*classname&&ImGui::MenuItem(("Select all "+std::string(classname)).c_str()))
      command(("ftebroom_entity_select_class "+organizationQuote(classname)+"\n").c_str());
    if(brushes&&owner&&cvars->GetFloat("ftebroom_context_brush_entity")&&ImGui::MenuItem("Move brushes to this entity"))
      command(("ftebroom_entity_move_to "+std::to_string(owner)+"\n").c_str());
  }
  if(brushes||canConvert){
    ImGui::Separator();
    if(ImGui::MenuItem("Make structural"))organize("structural");
    if(canConvert&&ImGui::BeginMenu("Convert into...")){
      if((gameProfile==2||gameProfile==4)&&ImGui::MenuItem("MDL model"))bspModelShow(true,true);
      if(ImGui::MenuItem("BSP model"))bspModelShow(false,true);
      ImGui::EndMenu();
    }
    if(!canConvert&&!jamEditing()&&ImGui::MenuItem("Export selected BSP model..."))bspModelShow();
  }
}
static void sceneContextPanel()
{
  if (
    sceneContextPending
    && sceneContextSerial != (int)cvars->GetFloat("ftebroom_context_serial"))
  {
    sceneContextPending = false;
    placementCapture();
    ImGui::OpenPopup("Scene context");
    ImGui::SetNextWindowPos({sceneContextX, sceneContextY});
  }
  if (ImGui::BeginPopup("Scene context"))
  {
    sceneContextActions();
    ImGui::EndPopup();
  }
  if (organizationNameRequest)
  {
    organizationNameRequest = false;
    ImGui::OpenPopup("Group name");
  }
  if (uiDialog("Group name"))
  {
    static char name[256] = "Group";
    ImGui::InputText("Name", name, sizeof name);
    if (ImGui::Button(organizationRename ? "Rename" : "Create group"))
    {
      if (organizationRename)
        organize("rename", 0, name);
      else
        command(("ftebroom_group " + organizationQuote(name) + "\n").c_str());
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}
