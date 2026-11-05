// SPDX-License-Identifier: GPL-3.0-or-later
static bool spawnMasterOpen=false;
static void spawnMasterShow(){spawnMasterOpen=true;command("ftebroom_spawn_master prepare\n");}
static void spawnMasterPanel(){
  if(!spawnMasterOpen)return;
  static int epoch=-1,source=-1;static std::set<std::string> targets;static std::string destination,classname;static char targetFilter[128]{},classFilter[128]{};static bool pending=false;
  int next=(int)cvars->GetFloat("ftebroom_spawn_epoch");
  if(next!=epoch){
    std::ifstream file(documentPath("data/ftebroom-spawn-master.txt"));if(file){targets.clear();destination.clear();classname.clear();source=-1;std::string line;
      while(std::getline(file,line)){std::istringstream row(line);char kind;std::string value;if(!(row>>kind))continue;if(kind=='s'){row>>source;continue;}if(!(row>>std::quoted(value)))continue;if(kind=='t')targets.insert(value);if(kind=='d')destination=value;if(kind=='c')classname=value;}
      if(destination.empty()&&!targets.empty())destination=*targets.begin();
      if(classname.empty())for(const auto& entry:entityDefinitions)if(!entry.second.brush&&entry.first.rfind("item_",0)==0){classname=entry.first;break;}
      epoch=next;pending=false;
    }
  }
  if(pending&&cvars->GetFloat("ftebroom_spawn_result")){pending=false;if(cvars->GetFloat("ftebroom_spawn_result")>0)spawnMasterOpen=false;}
  const ImGuiViewport* viewport=ImGui::GetMainViewport();const ImVec2 available=viewport->WorkSize;
  ImGui::SetNextWindowPos({viewport->WorkPos.x+available.x*.5f,viewport->WorkPos.y+available.y*.5f},ImGuiCond_FirstUseEver,{.5f,.5f});
  ImGui::SetNextWindowSize({std::min(760*uiScale(),available.x-20),std::min(520*uiScale(),available.y-40)},ImGuiCond_FirstUseEver);
  if(ImGui::Begin("Spawn Master",&spawnMasterOpen)){
    ImGui::TextWrapped("Select a trigger/button, an existing destination targetname and a point entity. Rebuilds the shared teleport closet in one Undo. Teleport behavior depends on the game/mod.");
    if(ImGui::Button("Refresh selection / targets"))command("ftebroom_spawn_master prepare\n");
    ImGui::BeginDisabled(pending);
    float height=std::max(80.f,ImGui::GetContentRegionAvail().y-90*uiScale());
    ImGui::BeginChild("Destinations",{std::min(220*uiScale(),ImGui::GetContentRegionAvail().x*.35f),height},ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Destination targetname");ImGui::InputTextWithHint("##targetsearch","Search targetname",targetFilter,sizeof targetFilter);
    std::string targetQuery=entityLower(targetFilter);for(const auto& name:targets)if(entityLower(name).find(targetQuery)!=std::string::npos)if(ImGui::Selectable(name.c_str(),name==destination))destination=name;
    ImGui::EndChild();ImGui::SameLine();ImGui::BeginChild("Spawn classes",{0,height},ImGuiChildFlags_Borders);
    ImGui::Text("Spawn: %s",classname.c_str());ImGui::InputTextWithHint("##classsearch","Search point entities",classFilter,sizeof classFilter);
    static int thumbnail=72;ImGui::SliderInt("Thumbnail size",&thumbnail,48,128);
    std::vector<const ftebroom::EntityDefinition*> classes;std::string query=entityLower(classFilter);
    for(const auto& entry:entityDefinitions)if(!entry.second.brush&&entry.first!="worldspawn"&&entityLower(entry.first).find(query)!=std::string::npos)classes.push_back(&entry.second);
    float size=thumbnail*uiScale(),cellWidth=size+10,cellHeight=size+2*ImGui::GetTextLineHeightWithSpacing()+8;int columns=std::max(1,int(ImGui::GetContentRegionAvail().x/cellWidth));
    ImGuiListClipper clipper;clipper.Begin((int(classes.size())+columns-1)/columns,cellHeight);
    while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;row++){
      float top=ImGui::GetCursorPosY();for(int col=0;col<columns;col++){
        int index=row*columns+col;if(index>=int(classes.size()))break;const auto& def=*classes[index];ImGui::SetCursorPos({8+col*cellWidth,top});ImGui::PushID(def.name.c_str());ImVec2 tile=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("entity",{size,cellHeight-4}))classname=def.name;
        auto draw=ImGui::GetWindowDrawList();draw->PushClipRect(tile,{tile.x+size,tile.y+size},true);draw->AddRectFilled(tile,{tile.x+size,tile.y+size},uiChrome(IM_COL32(55,58,62,255),UiTone::Inset));
        if(!entityModelTile(def,tile,size))draw->AddText({tile.x+8,tile.y+size*.4f},ImGui::GetColorU32(ImGuiCol_TextDisabled),"Entity");
        if(classname==def.name)draw->AddRect(tile,{tile.x+size,tile.y+size},uiChrome(IM_COL32(225,95,55,255),UiTone::Selection),0,0,2);draw->PopClipRect();
        draw->PushClipRect({tile.x,tile.y+size},{tile.x+size,tile.y+cellHeight},true);draw->AddText({tile.x,tile.y+size+2},ImGui::GetColorU32(ImGuiCol_Text),def.name.c_str());draw->PopClipRect();
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s",def.name.c_str(),def.description.c_str());ImGui::PopID();
      }ImGui::SetCursorPosY(top+cellHeight);ImGui::Dummy({0,0});
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(source<1||destination.empty()||classname.empty());
    if(ImGui::Button("Build / update closet")){cvars->SetFloat("ftebroom_spawn_result",0);command(("ftebroom_spawn_master apply "+organizationQuote(destination)+" "+organizationQuote(classname)+"\n").c_str());pending=true;}
    ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Cancel"))spawnMasterOpen=false;ImGui::EndDisabled();
    char error[512]{};cvars->GetString("ftebroom_spawn_error",error,sizeof error);if(*error)ImGui::TextWrapped("%s",error);
  }ImGui::End();
}
