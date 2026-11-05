// SPDX-License-Identifier: GPL-2.0-or-later

static void terrainPanel()
{
  static bool shown=false;
  if(gameProfile!=1){shown=false;return;}
  if(terrainCreateWindow) {
    ImGui::SetNextWindowSize({520*uiScale(),320*uiScale()},ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Create FTE Engine terrain",&terrainCreateWindow)) {
      static char texture[256]="textures/ftebroom/grid";float origin[3]={};char value[128]={};cvars->GetString("ftebroom_terrain_origin",value,sizeof value);std::sscanf(value,"%f %f %f",&origin[0],&origin[1],&origin[2]);
      if(ImGui::InputFloat3("Origin",origin)){std::snprintf(value,sizeof value,"%g %g %g",origin[0],origin[1],origin[2]);cvars->SetString("ftebroom_terrain_origin",value);}
      ImGui::InputText("Ground material",texture,sizeof texture);
      ImGui::TextWrapped("Adds a 2 x 2 section terrain area to this map. XY snaps to section boundaries. Existing terrain, brushes, patches and entities are preserved. Terrain sections are saved beside the MAP.");
      if(ImGui::Button("Add FTE Engine terrain")) {
        if(std::isfinite(origin[0])&&std::isfinite(origin[1])&&std::isfinite(origin[2])&&*texture) {
          command(("ftebroom_terrain_create "+organizationQuote(value)+" "+organizationQuote(texture)+"\n").c_str());terrainCreateWindow=false;
        }
      }
      if(!documentError.empty())ImGui::TextWrapped("%s",documentError.c_str());
    }ImGui::End();
  }
  documentTerrain=cvars->GetFloat("ftebroom_terrain_document")!=0;
  if(!documentTerrain){shown=false;return;}
  if(!shown){ImGui::SetNextWindowFocus();shown=true;}
  ImGui::SetNextWindowSize({370*uiScale(),600*uiScale()},ImGuiCond_FirstUseEver);
  if(auto inspector=ImGui::FindWindowByName("Map");inspector&&inspector->DockId)ImGui::SetNextWindowDockID(inspector->DockId,ImGuiCond_FirstUseEver);
  bool visible=uiSoftPanelBegin("FTE Engine terrain",nullptr,uiPanelFlags());
  static bool wasVisible=false;
  auto leave=[](){cvars->SetFloat("ftebroom_terrain_paint",0);cvars->SetFloat("ftebroom_terrain_edit",0);command("ftebroom_tool_deactivate\n");};
  if(!visible&&wasVisible&&cvars->GetFloat("ftebroom_terrain_edit"))leave();
  wasVisible=visible;
  if(visible) {
    bool editing=cvars->GetFloat("ftebroom_terrain_edit")!=0;if(ImGui::Checkbox("Sculpt / paint terrain",&editing)){if(editing)command("ftebroom_tool_deactivate\nset ftebroom_terrain_edit 1\n");else leave();}
    if(editing&&ImGui::Button("Exit terrain editing"))leave();
    static const int operations[]={7,8,3,4,5,6,11,12,13,14,15,16,2};static int selected=0;
    ImGui::Combo("Operation",&selected,"Raise\0Lower\0Set height\0Smooth\0Spread\0Flatten\0Paint blend\0Paint solid\0Concentrate layers\0Layer noise\0Blur layers\0Water height\0Hole\0");cvars->SetFloat("ftebroom_terrain_operation",operations[selected]);
    static float radius=256,quantity=8,strength=40;static bool square=false;static char texture[256]="textures/ftebroom/grid";
    ImGui::SliderFloat("Radius",&radius,8,2048);ImGui::InputFloat("Height / amount",&quantity);ImGui::SliderFloat("Strength",&strength,0,100);ImGui::Checkbox("Square brush",&square);ImGui::InputText("Paint material",texture,sizeof texture);
    if(ImGui::Button("Use selected material")){auto name=selectedMaterial;std::snprintf(texture,sizeof texture,"%s",name.c_str());}
    cvars->SetFloat("ftebroom_terrain_radius",radius);cvars->SetFloat("ftebroom_terrain_quantity",std::isfinite(quantity)?quantity:0);cvars->SetFloat("ftebroom_terrain_strength",strength);cvars->SetFloat("ftebroom_terrain_square",square);cvars->SetString("ftebroom_terrain_texture",texture);
    if(ImGui::Button("Save terrain and map"))documentSave(documentName);
    ImGui::TextWrapped("Drag in the perspective viewport to paint. Disable sculpting to edit brushes/entities. Save writes the MAP and its terrain sections. Each stroke is one Undo / Redo step, shared with brush and entity edits.");
  }ImGui::End();
}
