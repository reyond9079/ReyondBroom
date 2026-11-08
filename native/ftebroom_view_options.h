// SPDX-License-Identifier: GPL-2.0-or-later
static void publishMaterialTags()
{
  std::ostringstream out;
  for (const auto& item : materials)
  {
    auto name = item.first;
    if (name.rfind("textures/", 0) == 0)
      name = name.substr(9);
    out << std::quoted(name) << ' ' << item.second.tags << ' '
        << std::quoted(item.second.preview) << ' ' << item.second.opacity << ' ' << item.second.masked << ' ' << std::quoted(item.second.blend) << ' ' << std::quoted(item.second.alphaTest) << ' ' << item.second.looseWal << '\n';
  }
  const auto content=out.str();
  const auto scope=std::to_string(gameProfile)+":"+std::string(baseq3)+":"+gameRuntime();
  static std::string published, publishedScope;
  // Applying an unrelated profile option must not rebuild every MAP face cache.
  if(content==published && scope==publishedScope)return;
  std::ofstream file(documentPath("ftebroom-material-tags.txt"),std::ios::binary);
  file<<content;file.close();
  if (file) {
    published=content;publishedScope=scope;
    cvars->SetFloat(
      "ftebroom_material_tag_epoch", cvars->GetFloat("ftebroom_material_tag_epoch") + 1);
  }
}
static void runUiRecordGlobal(const std::string& key);
static void viewOptionsRecord(const std::string& key){
  runUiRecordGlobal(key);if(runUiLive)runUiGlobalRects[key]["visible"]=ImGui::IsItemVisible();
}
static void viewOption(const char* label, const char* cvar)
{
  bool value = cvars->GetFloat(cvar) != 0;
  if (ImGui::Checkbox(label, &value))
    cvars->SetFloat(cvar, value);
  viewOptionsRecord(std::string("view-option-")+cvar);
}
static void displayBrightnessControls(){
  float gamma=cvars->GetFloat("v_gamma"),contrast=cvars->GetFloat("v_contrast");
  if(ImGui::SliderFloat("Display brightness (gamma)",&gamma,.5f,2.f,"%.2f"))cvars->SetFloat("v_gamma",gamma);
  if(ImGui::SliderFloat("Display contrast",&contrast,.5f,2.f,"%.2f"))cvars->SetFloat("v_contrast",contrast);
  if(ImGui::Button("Reset display brightness")){cvars->SetFloat("v_gamma",1);cvars->SetFloat("v_contrast",1);}
  ImGui::TextDisabled("Display only; saved textures, compiled lightmaps and GPU bake values are unchanged.");
}
static void serviceOpacityControls(){
  struct Field{float value=.35f;bool active=false;};static std::map<ImGuiID,Field> fields;
  auto& field=fields[ImGui::GetID("Service texture opacity")];if(!field.active)field.value=cvars->GetFloat("ftebroom_tool_opacity");
  ImGui::SliderFloat("Service texture opacity",&field.value,0,1,"%.2f");
  if(ImGui::IsItemDeactivatedAfterEdit())cvars->SetFloat("ftebroom_tool_opacity",field.value);
  field.active=ImGui::IsItemActive();
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Clip, trigger, hint, origin, caulk and portal surfaces. 0 hides their fill, 1 is opaque. Release to apply. Game previews are unchanged.");
}
static bool viewOptionsOpen=false;
static std::string viewOptionsSettings();
static bool viewOptionsPosition=false;
static ImVec2 viewOptionsAnchor;
static void viewOptionsRestoreDefaults(){
  for(auto name:{"ftebroom_show_entity_names","ftebroom_show_group_bounds","ftebroom_show_brush_entity_bounds","ftebroom_show_entity_bounds","ftebroom_show_models","ftebroom_show_points","ftebroom_show_brushes","ftebroom_show_patches","ftebroom_shade_faces","ftebroom_show_edges"})cvars->SetFloat(name,1);
  cvars->SetFloat("ftebroom_render_faces",0);cvars->SetFloat("r_fog_enabled",0);
  cvars->SetFloat("ftebroom_bounds_visible",1);cvars->SetFloat("ftebroom_entity_links",1);
}
static void viewOptionsToolbar(){
  if(modEmbedded||!uiHasScene)return;
  if(runUiLive){
    nlohmann::json settings=nlohmann::json::object();std::istringstream lines(viewOptionsSettings());
    for(std::string line;std::getline(lines,line);){auto at=line.find('=');if(at!=line.npos)settings[line.substr(0,at)]=line.substr(at+1);}
    runUiGlobalRects["view-settings"]=std::move(settings);
  }
  const float scale=uiScale();
  for(int i=0;i<viewLayout;i++){
    const auto r=viewRects[i];if(r.type!=0||r.w<145*scale||r.h<80*scale)continue;
    ImGui::PushID(i);
    ImGui::SetNextWindowPos({r.x+r.w-5*scale,r.y+4*scale},ImGuiCond_Always,{1,0});
    ImGui::SetNextWindowBgAlpha(.98f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{3*scale,3*scale});
    ImGui::Begin(("##view-options-toolbar"+std::to_string(i)).c_str(),nullptr,
      ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoDocking|
      ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoFocusOnAppearing);
    if(ImGui::Button("View Options")){
      viewOptionsOpen=!viewOptionsOpen;viewOptionsPosition=true;
      viewOptionsAnchor={r.x+r.w-5*scale,ImGui::GetItemRectMax().y+4*scale};
    }
    runUiRecordGlobal("view-options-button-"+std::to_string(i));
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Click to edit view settings");
    ImGui::End();ImGui::PopStyleVar();ImGui::PopID();
  }
}
static void viewOptionsClasses(){
  uiSectionHeading("Entity Definitions");
  static char query[128]{};
  ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##classvisibility","Filter entity classes",query,sizeof query);
  char buffer[16384]{};cvars->GetString("ftebroom_hidden_classes",buffer,sizeof buffer);
  std::string hidden=buffer;std::map<std::string,std::vector<std::string>> groups;
  std::set<std::string> names;
  for(const auto& item:entityDefinitions)names.insert(item.first);
  for(const auto& item:entityUsage)names.insert(item.first);
  for(const auto& name:names)if(name!="worldspawn")groups[name.substr(0,name.find('_'))].push_back(name);
  auto visible=[&](const std::string& name){return hidden.find("|"+name+"|")==std::string::npos;};
  auto set=[&](const std::string& name,bool show){auto token="|"+name+"|";auto at=hidden.find(token);if(show&&at!=std::string::npos)hidden.erase(at,token.size());else if(!show&&at==std::string::npos)hidden+=token;};
  const float footer=ImGui::GetFrameHeightWithSpacing()+4*uiScale();
  ImGui::BeginChild("Class visibility",{0,-footer},ImGuiChildFlags_Borders);
  if(runUiLive){auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();runUiGlobalRects["view-options-classes-region"]={{"x",p.x},{"y",p.y},{"width",s.x},{"height",s.y}};}
  for(const auto& group:groups){
    std::vector<std::string> filtered;for(auto& name:group.second)if(entityLower(name).find(entityLower(query))!=std::string::npos)filtered.push_back(name);
    if(filtered.empty())continue;
    ImGui::PushID(group.first.c_str());
    int shown=0;for(auto& name:group.second)shown+=visible(name);bool all=shown==int(group.second.size());
    if(shown&&!all)ImGui::PushItemFlag(ImGuiItemFlags_MixedValue,true);
    if(ImGui::Checkbox((group.first+"##group").c_str(),&all))for(auto& name:group.second)set(name,all);
    if(shown&&shown!=int(group.second.size()))ImGui::PopItemFlag();
    ImGui::Indent(13*uiScale());
    for(auto& name:filtered){bool show=visible(name);if(ImGui::Checkbox((name+"##definition").c_str(),&show))set(name,show);viewOptionsRecord("view-definition-"+name);}
    ImGui::Unindent(13*uiScale());ImGui::PopID();
  }
  ImGui::EndChild();
  if(ImGui::Button("Show all"))hidden.clear();viewOptionsRecord("view-options-showall");ImGui::SameLine();
  if(ImGui::Button("Hide all")){hidden.clear();for(auto& name:names)if(name!="worldspawn")set(name,false);}viewOptionsRecord("view-options-hideall");
  if(hidden!=buffer)cvars->SetString("ftebroom_hidden_classes",hidden.c_str());
}
static void viewOptionsRenderer(){
  uiSectionHeading("Entities");
  viewOption("Show entity classnames","ftebroom_show_entity_names");
  viewOption("Show group bounds and names","ftebroom_show_group_bounds");
  viewOption("Show brush entity bounds","ftebroom_show_brush_entity_bounds");
  viewOption("Show point entity bounds","ftebroom_show_entity_bounds");
  viewOption("Show point entities","ftebroom_show_points");
  viewOption("Show point entity models","ftebroom_show_models");
  uiSectionHeading("Brushes");viewOption("Show brushes","ftebroom_show_brushes");
  int hidden=int(cvars->GetFloat("ftebroom_hidden_tags"));
  const char* tags[]={"trigger","clip","caulk","hint","detail","liquid","translucent","areaportal","clusterportal"};
  for(int i=0;i<9;i++){
    bool show=!(hidden&(1<<i));
    if(ImGui::Checkbox((std::string("Show ")+tags[i]).c_str(),&show)){hidden=show?hidden&~(1<<i):hidden|(1<<i);cvars->SetFloat("ftebroom_hidden_tags",float(hidden));}
    viewOptionsRecord(std::string("view-tag-")+tags[i]);
    if(i%2==0&&i!=8)ImGui::SameLine(160*uiScale());
  }
  if(gameProfile!=2&&gameProfile!=3&&gameProfile!=4){uiSectionHeading("Patches");viewOption("Show Patches","ftebroom_show_patches");}
  uiSectionHeading("Renderer");
  const char* faces[]={"Show materials","Hide materials","Hide faces"};
  for(int i=0;i<3;i++){if(ImGui::RadioButton(faces[i],int(cvars->GetFloat("ftebroom_render_faces"))==i))cvars->SetFloat("ftebroom_render_faces",float(i));viewOptionsRecord("view-render-mode-"+std::to_string(i));}
  viewOption("Shade faces","ftebroom_shade_faces");viewOption("Use fog","r_fog_enabled");viewOption("Show edges","ftebroom_show_edges");
  const char* links[]={"Hide entity links","Show directly selected entity links","Show transitively selected entity links","Show all entity links"};
  for(int i=3;i>=0;i--){if(ImGui::RadioButton(links[i],int(cvars->GetFloat("ftebroom_entity_links"))==i))cvars->SetFloat("ftebroom_entity_links",float(i));viewOptionsRecord("view-entity-links-"+std::to_string(i));}
  viewOption("Show soft bounds","ftebroom_bounds_visible");
  if(ImGui::Button("Restore Defaults"))viewOptionsRestoreDefaults();
  viewOptionsRecord("view-options-restore");
}
static std::string viewOptionsSettings(){
  std::string result;char value[16384]{};
  for(auto name:{"ftebroom_show_brushes","ftebroom_show_patches","ftebroom_show_points","ftebroom_show_models","ftebroom_show_entity_names","ftebroom_show_group_bounds","ftebroom_show_brush_entity_bounds","ftebroom_show_entity_bounds","ftebroom_hidden_tags","ftebroom_hidden_classes","ftebroom_render_faces","ftebroom_shade_faces","ftebroom_show_edges","r_fog_enabled","ftebroom_entity_links","ftebroom_bounds_visible","ftebroom_grid_plane","ftebroom_sky_overlays","ftebroom_tool_opacity","ftebroom_label_distance","v_gamma","v_contrast","vid_multisample","ftebroom_grid_aa","ftebroom_grid_fade_start","ftebroom_grid_fade_end","gl_texture_anisotropy","gl_texturemode"}){cvars->GetString(name,value,sizeof value);result+=name;result+='=';result+=value;result+='\n';}
  return result;
}
static void viewOptionsPanel(){
  if(!viewOptionsOpen)return;
  const auto before=viewOptionsSettings();
  const auto screen=ImGui::GetMainViewport()->WorkSize;const float scale=uiScale();
  ImGui::SetNextWindowSize({std::min(700*scale,screen.x-20),std::min(790*scale,screen.y-45)},ImGuiCond_FirstUseEver);
  if(viewOptionsPosition){viewOptionsAnchor.y=std::max(8.f,std::min(viewOptionsAnchor.y,screen.y-std::min(790*scale,screen.y-45)-8));ImGui::SetNextWindowPos(viewOptionsAnchor,ImGuiCond_Always,{1,0});viewOptionsPosition=false;}
  if(ImGui::Begin("View Options",&viewOptionsOpen,ImGuiWindowFlags_NoDocking)){
    if(runUiLive){auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();float side=ImGui::GetFrameHeight();runUiGlobalRects["view-options-close"]={{"x",p.x+s.x-side},{"y",p.y},{"width",side},{"height",side}};}
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{6*scale,2*scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{6*scale,2*scale});
    if(ImGui::BeginTable("View settings",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV)){
      ImGui::TableSetupColumn("Definitions",ImGuiTableColumnFlags_WidthStretch,.43f);
      ImGui::TableSetupColumn("Display",ImGuiTableColumnFlags_WidthStretch,.57f);
      ImGui::TableNextRow();ImGui::TableNextColumn();viewOptionsClasses();ImGui::TableNextColumn();
      ImGui::BeginChild("Display options");
      ImGui::PushItemWidth(std::max(60.f,ImGui::GetContentRegionAvail().x*.42f));
      ImGui::PushTextWrapPos(0);
      viewOptionsRenderer();
      if(runUiLive){auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();runUiGlobalRects["view-options-display-region"]={{"x",p.x},{"y",p.y},{"width",s.x},{"height",s.y}};}
      const bool additional=ImGui::CollapsingHeader("Additional ReyondBroom settings");viewOptionsRecord("view-options-additional");
      if(additional){
        viewOption("Horizontal grid plane","ftebroom_grid_plane");viewOption("Sky surface overlays","ftebroom_sky_overlays");
        serviceOpacityControls();
        float distance=cvars->GetFloat("ftebroom_label_distance");if(ImGui::InputFloat("Entity label distance",&distance,128,512,"%.0f"))cvars->SetFloat("ftebroom_label_distance",std::max(0.f,distance));
        displayBrightnessControls();
        int samples=int(cvars->GetFloat("vid_multisample"));int choice=samples>=8?3:samples>=4?2:samples>=2?1:0;
        if(ImGui::Combo("Edge MSAA",&choice,"Off\0 2x\0 4x\0 8x\0"))cvars->SetFloat("vid_multisample",choice?float(1<<choice):1);
        if(ImGui::Button("Apply MSAA (restart renderer)")){if(documentTerrain||modEmbedded)ImGui::OpenPopup("Apply MSAA");else command("ftebroom_renderer_restart\n");}
        if(ImGui::BeginPopup("Apply MSAA")){ImGui::TextWrapped("MSAA is saved. Restart the application to apply it when using engine terrain or mod preview.");ImGui::EndPopup();}
        float aa=cvars->GetFloat("ftebroom_grid_aa");if(ImGui::SliderFloat("Grid smoothing",&aa,.05f,2))cvars->SetFloat("ftebroom_grid_aa",aa);
        float start=cvars->GetFloat("ftebroom_grid_fade_start"),end=cvars->GetFloat("ftebroom_grid_fade_end");
        if(ImGui::InputFloat("Grid fade start",&start,64,256,"%.0f"))cvars->SetFloat("ftebroom_grid_fade_start",std::max(0.f,start));
        if(ImGui::InputFloat("Grid draw distance",&end,64,256,"%.0f"))cvars->SetFloat("ftebroom_grid_fade_end",std::max(0.f,end));
        ImGui::TextWrapped("World units; distance 0 is unlimited. Grid size changes snapping, not the draw distance.");
        int anisotropy=std::clamp(int(cvars->GetFloat("gl_texture_anisotropy")),1,16);if(ImGui::SliderInt("Texture anisotropy",&anisotropy,1,16))cvars->SetFloat("gl_texture_anisotropy",float(anisotropy));
        char mode[80]{};cvars->GetString("gl_texturemode",mode,sizeof mode);bool smooth=std::string(mode)=="GL_LINEAR_MIPMAP_LINEAR";
        if(ImGui::Checkbox("Trilinear texture filtering",&smooth))cvars->SetString("gl_texturemode",smooth?"GL_LINEAR_MIPMAP_LINEAR":"GL_NEAREST_MIPMAP_NEAREST");
        if(ImGui::TreeNode("Selection tags")){for(auto type:{"trigger","clip","caulk","hint","detail"}){ImGui::PushID(type);ImGui::TextUnformatted(type);ImGui::SameLine();if(ImGui::SmallButton("Set"))command(("ftebroom_tag "+std::string(type)+" 1\n").c_str());ImGui::SameLine();if(ImGui::SmallButton("Remove"))command(("ftebroom_tag "+std::string(type)+" 0\n").c_str());ImGui::PopID();}ImGui::TreePop();}
      }
      ImGui::PopTextWrapPos();ImGui::PopItemWidth();
      ImGui::EndChild();ImGui::EndTable();
    }
    ImGui::PopStyleVar(2);
  }ImGui::End();
  if(before!=viewOptionsSettings())preferencesSave();
}
