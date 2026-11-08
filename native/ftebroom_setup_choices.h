// SPDX-License-Identifier: GPL-2.0-or-later
static ImVec2 setupGameCenters[5],setupModCenters[2];
static void setupPathField(const char* label,char* value,size_t capacity,bool multiline=false){
  ImGui::TextWrapped("%s",uiTr(label));ImGui::PushID(label);ImGui::SetNextItemWidth(-1);
  bool changed=multiline?ImGui::InputTextMultiline("##path",value,capacity,{-1,60*uiScale()}):ImGui::InputText("##path",value,capacity);if(changed)assetError.clear();
  ImGui::PopID();
}
static bool setupChoice(const char* id,const char* title,const char* subtitle,bool selected,const char* logo=nullptr,ImVec2* center=nullptr,float progress=-1,const char* progressLabel=nullptr) {
  ImGui::PushID(id);
  title=uiTr(title);subtitle=uiTr(subtitle);
  auto start=ImGui::GetCursorScreenPos();
  const float inset=(logo?80.f:16.f)*uiScale(),padding=16*uiScale();
  const float textWidth=std::max(1.f,ImGui::GetContentRegionAvail().x-inset-padding);
  const float titleHeight=ImGui::CalcTextSize(title,nullptr,false,textWidth).y;
  const float subtitleY=14*uiScale()+titleHeight+6*uiScale();
  const float contentHeight=std::max(76*uiScale(),subtitleY+ImGui::CalcTextSize(subtitle,nullptr,false,textWidth).y+14*uiScale());
  ImVec2 size{ImGui::GetContentRegionAvail().x,contentHeight+(progress>=0?36*uiScale():0)};
  bool clicked=ImGui::InvisibleButton("choice",size);bool hover=ImGui::IsItemHovered();
  if(center)*center={start.x+size.x*.5f,start.y+size.y*.5f};
  auto list=ImGui::GetWindowDrawList();ImVec2 end{start.x+size.x,start.y+size.y};
  list->AddRectFilled(start,end,ImGui::GetColorU32(selected?ImGuiCol_HeaderActive:hover?ImGuiCol_HeaderHovered:ImGuiCol_FrameBg),4*uiScale());
  list->AddRect(start,end,ImGui::GetColorU32(selected?ImGuiCol_CheckMark:ImGuiCol_Border),4*uiScale(),0,selected?2:1);
  if(logo){auto handle=draw->LoadImage(logo);if(handle)list->AddImage((ImTextureID)handle,{start.x+padding,start.y+14*uiScale()},{start.x+padding+48*uiScale(),start.y+62*uiScale()});}
  list->PushClipRect(start,end,true);
  list->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{start.x+inset,start.y+14*uiScale()},ImGui::GetColorU32(ImGuiCol_Text),title,nullptr,textWidth);
  list->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{start.x+inset,start.y+subtitleY},ImGui::GetColorU32(ImGuiCol_Text),subtitle,nullptr,textWidth);
  if(progress>=0){
    ImVec2 a{start.x+padding,start.y+contentHeight},b{end.x-padding,end.y-10*uiScale()};
    list->AddRectFilled(a,b,ImGui::GetColorU32(ImGuiCol_FrameBg),3);
    list->AddRectFilled(a,{a.x+(b.x-a.x)*std::clamp(progress,0.f,1.f),b.y},ImGui::GetColorU32(ImGuiCol_CheckMark),3);
    if(progressLabel)list->AddText({a.x+6,a.y+2},ImGui::GetColorU32(ImGuiCol_Text),uiTr(progressLabel));
  }
  list->PopClipRect();
  ImGui::PopID();return clicked;
}
static void setupGameChoices() {
  const int order[]={2,3,0,4,1};
  const char* ids[]={"Quake 1","Quake II","Quake III Arena","Half-Life","FTE Custom game"};
  const char* titles[]={"Quake 1 (recommended)","Quake II","Quake III Arena","Half-Life","FTE Custom game"};
  const char* captions[]={"Classic Quake maps, WAD textures and mods","Quake II maps, WAL textures and PAK archives","Quake III maps, patches and shaders","GoldSrc maps, WAD3 textures, FreeHL / FreeCS","Custom FTE games and engine features"};
  const char* logos[]={"ui/games/quake1.png","ui/games/quake2.png","ui/games/quake3.png","ui/games/halflife.png","ui/games/fte.png"};
  for(int i=0;i<5;i++)if(setupChoice(ids[i],titles[i],captions[i],gameProfile==order[i],logos[i],&setupGameCenters[i])){
    assetError.clear();
    if(gameProfile<2)q3Root=baseq3;
    gameProfile=order[i];
    std::snprintf(baseq3,sizeof baseq3,"%s",gameProfile==3?q2Mod:gameWad()?q1Authoring().c_str():q3Root.c_str());
  }
}
static bool setupModBrowseOpen=false;
static int setupModBrowseProfile=2;
static ImVec2 setupModBrowseUseCenter;
static char setupModBrowsePath[4096]{};
static std::string setupModBrowseScanned,setupModBrowseError;
static std::vector<std::filesystem::path> setupModDirectories;
static void setupModBrowseStart(int profile) {
  setupModBrowseProfile=profile;
  const char* value=profile==3?q2Mod:profile==4?hlMod:q1Mod;
  auto path=*value?std::filesystem::u8path(value):settingsPath().parent_path();
  std::snprintf(setupModBrowsePath,sizeof setupModBrowsePath,"%s",path.u8string().c_str());
  setupModBrowseOpen=true;setupModBrowseScanned.clear();
}
static void setupModBrowse() {
  if(!setupModBrowseOpen)return;
  if(!ImGui::IsPopupOpen("Choose mod directory"))ImGui::OpenPopup("Choose mod directory");
  ImGui::SetNextWindowSize({650*uiScale(),420*uiScale()},ImGuiCond_FirstUseEver);
  if(ImGui::BeginPopupModal("Choose mod directory",&setupModBrowseOpen)){
    ImGui::InputText("Directory",setupModBrowsePath,sizeof setupModBrowsePath);
    if(ImGui::Button("Up##mod")){auto parent=std::filesystem::u8path(setupModBrowsePath).parent_path();if(!parent.empty())std::snprintf(setupModBrowsePath,sizeof setupModBrowsePath,"%s",parent.u8string().c_str());}
    ImGui::SameLine();if(ImGui::Button("Refresh##mod"))setupModBrowseScanned.clear();
    if(setupModBrowseScanned!=setupModBrowsePath){setupModBrowseScanned=setupModBrowsePath;setupModDirectories.clear();setupModBrowseError.clear();try{
      for(const auto& e:std::filesystem::directory_iterator(std::filesystem::u8path(setupModBrowsePath)))if(e.is_directory())setupModDirectories.push_back(e.path());
      std::sort(setupModDirectories.begin(),setupModDirectories.end());
    }catch(const std::exception& e){setupModBrowseError=e.what();}}
    ImGui::BeginChild("Mod folders",{0,-85*uiScale()},ImGuiChildFlags_Borders);
    ImGuiListClipper clip;clip.Begin(setupModDirectories.size());while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;i++)if(ImGui::Selectable(setupModDirectories[i].filename().u8string().c_str(),false,ImGuiSelectableFlags_NoAutoClosePopups))std::snprintf(setupModBrowsePath,sizeof setupModBrowsePath,"%s",setupModDirectories[i].u8string().c_str());
    ImGui::EndChild();
    if(ImGui::Button("Use this folder")){try{auto path=std::filesystem::absolute(std::filesystem::u8path(setupModBrowsePath));if(!std::filesystem::is_directory(path))throw std::runtime_error("Choose an existing directory.");auto text=path.u8string();if(text.size()>=sizeof q1Mod)throw std::runtime_error("Directory path is too long.");auto target=setupModBrowseProfile==3?q2Mod:setupModBrowseProfile==4?hlMod:q1Mod;std::snprintf(target,1024,"%s",text.c_str());if(setupModBrowseProfile==2)q1UseLibre=false;assetError.clear();setupModBrowseOpen=false;}catch(const std::exception& e){setupModBrowseError=e.what();}}
    {auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();setupModBrowseUseCenter={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}
    ImGui::SameLine();if(ImGui::Button("Cancel##mod"))setupModBrowseOpen=false;
    if(!setupModBrowseError.empty())ImGui::TextWrapped("%s",setupModBrowseError.c_str());
    if(!setupModBrowseOpen)ImGui::CloseCurrentPopup();ImGui::EndPopup();
  }
}
static int setupModChoice=-1;
static void setupQuakeModFields() {
  // Recommend the download on a fresh install, retaining an existing custom mod.
  // Selection does not enable q1UseLibre until the download is ready to mount.
  if(setupModChoice<0)setupModChoice=(q1UseLibre||!*q1Mod)?0:1;
  uint64_t done=0,total=0;std::string phase;char progressLabel[128]={};
  if(libreBusy()){std::ifstream(libreInstall()/"progress.txt")>>done>>total>>phase;std::snprintf(progressLabel,sizeof progressLabel,"%s  %.1f / %.1f MiB",uiTr(phase.empty()?"Connecting...":phase.c_str()),done/1048576.,total/1048576.);}
  ImGui::BeginDisabled(libreBusy());
  if(setupChoice("libre","Download LibreQuake (recommended)",libreReady()?"Installed and ready to use":"Downloads automatically when you start",setupModChoice==0,nullptr,&setupModCenters[0],libreBusy()?(total?float(double(done)/total):0.f):-1,progressLabel)){setupModChoice=0;q1UseLibre=libreReady();assetError.clear();}
  if(setupChoice("own","Choose my own mod","Select a directory with your mod resources",setupModChoice==1,nullptr,&setupModCenters[1])){setupModChoice=1;q1UseLibre=false;assetError.clear();}
  ImGui::EndDisabled();
  if(setupModChoice==0 && !libreError.empty())ImGui::TextWrapped("%s",libreError.c_str());
  if(setupModChoice==1) {
    ImGui::SetNextItemWidth(-110*uiScale());if(ImGui::InputText("##moddirectory",q1Mod,sizeof q1Mod))assetError.clear();ImGui::SameLine();
    if(ImGui::Button("Browse...##mod"))setupModBrowseStart(2);
    ImGui::TextWrapped("Choose the game or mod folder. If it contains id1, that folder is selected automatically. Otherwise the selected folder is used as the mod. A sibling id1 supplies base assets for a custom mod, with mod overrides. Mapping uses the standard Quake palette when the mod has no palette. Playing an expansion such as AD still requires the base game.");
  }
  if(ImGui::CollapsingHeader("Advanced settings")){
    setupPathField("Additional WAD2 archives (optional, one path per line)",q1Wads,sizeof q1Wads,true);
    setupPathField("Entity definitions (optional .def / .fgd / .ent)",q1Definitions,sizeof q1Definitions);
    ImGui::TextWrapped("Leave entity definitions empty to discover the mod's definitions automatically, including PAK archives. Built-in definitions are used only if none are found.");
    ImGui::Combo("Save MAP format",&q1MapFormat,"Quake standard\0Valve 220\0");
    ImGui::Combo("Target BSP",&q1BspFormat,"BSP29 (classic limits)\0BSP2 (modern engines)\0");
  }
  setupModBrowse();
}

// A clear community invitation; joining never advances or blocks setup.
static ImVec2 setupDiscordCenter;
static ImVec2 setupDiscordSize;
static bool setupDiscordOpenFailed=false;
static void setupChatMark(ImDrawList* list,ImVec2 center,float size,ImU32 color){
  const float unit=size/32.f;
  list->AddRectFilled({center.x-14*unit,center.y-10*unit},{center.x+14*unit,center.y+8*unit},color,7*unit);
  list->AddTriangleFilled({center.x-8*unit,center.y+5*unit},{center.x-8*unit,center.y+13*unit},{center.x+unit,center.y+5*unit},color);
  const auto eye=uiChrome(IM_COL32(88,101,242,255),UiTone::Selection);
  list->AddCircleFilled({center.x-5*unit,center.y-unit},1.8f*unit,eye,12);
  list->AddCircleFilled({center.x+5*unit,center.y-unit},1.8f*unit,eye,12);
}
static void setupDiscordInvitation(){
  const float scale=uiScale(),available=ImGui::GetContentRegionAvail().x;
  auto centeredText=[&](const char* text,bool disabled=false){
    const char* end=nullptr;FteBroomTranslate(text,end);
    const float width=ImGui::CalcTextSize(text,end).x;
    ImGui::SetCursorPosX(std::max(0.f,(available-width)*.5f));
    if(width>available)ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+available);
    if(disabled)ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextUnformatted(text,end);
    if(disabled)ImGui::PopStyleColor();
    if(width>available)ImGui::PopTextWrapPos();
  };
  ImGui::Dummy({0,18*scale});
  auto p=ImGui::GetCursorScreenPos();
  auto list=ImGui::GetWindowDrawList();ImVec2 center{p.x+available*.5f,p.y+38*scale};
  list->AddCircleFilled(center,38*scale,uiChrome(IM_COL32(88,101,242,255),UiTone::Selection),48);
  setupChatMark(list,center,44*scale,IM_COL32_WHITE);
  ImGui::Dummy({0,90*scale});
  if(uiBoldFonts[uiScaleChoice])ImGui::PushFont(uiBoldFonts[uiScaleChoice]);
  centeredText("Join the ReyondBroom community");
  if(uiBoldFonts[uiScaleChoice])ImGui::PopFont();
  ImGui::Dummy({0,8*scale});
  centeredText("Share your maps. Get help. Follow new releases.");
  ImGui::Dummy({0,22*scale});
  const char* label="Join Discord";const char* end=nullptr;FteBroomTranslate(label,end);
  const float width=std::min(available,std::max(300*scale,ImGui::CalcTextSize(label,end).x+112*scale));
  ImGui::SetCursorPosX(std::max(0.f,(available-width)*.5f));
  ImGui::PushStyleColor(ImGuiCol_Button,uiModern()?uiModernPalette().action:ImVec4(88/255.f,101/255.f,242/255.f,1));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,uiModern()?uiModernPalette().actionHover:ImVec4(71/255.f,82/255.f,196/255.f,1));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,uiModern()?uiModernPalette().action:ImVec4(60/255.f,69/255.f,165/255.f,1));
  ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,1,1,1));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,10*scale);
  ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,ImVec2(.6f,.5f));
  bool clicked=ImGui::Button("Join Discord##setupInvite",{width,58*scale});
  auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();setupDiscordCenter={(a.x+b.x)*.5f,(a.y+b.y)*.5f};setupDiscordSize={b.x-a.x,b.y-a.y};
  setupChatMark(list,{a.x+32*scale,setupDiscordCenter.y},26*scale,IM_COL32_WHITE);
  ImGui::PopStyleVar(2);ImGui::PopStyleColor(4);
  if(ImGui::IsItemHovered())ImGui::SetTooltip("https://discord.gg/eDMf53Kny");
  if(clicked){auto open=ImGui::GetPlatformIO().Platform_OpenInShellFn;setupDiscordOpenFailed=!open||!open(ImGui::GetCurrentContext(),"https://discord.gg/eDMf53Kny");}
  ImGui::Dummy({0,12*scale});
  centeredText("Joining is optional. Start editing whenever you are ready.",true);
  if(setupDiscordOpenFailed){ImGui::Spacing();ImGui::TextWrapped("Open this invite in your browser:");ImGui::TextLinkOpenURL("https://discord.gg/eDMf53Kny","https://discord.gg/eDMf53Kny");}
}
