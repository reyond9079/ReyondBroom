// SPDX-License-Identifier: GPL-3.0-or-later
// ImGui implementation of the TrenchBroom 2026.2 Compile layout. Geometry and
// interactions follow the reference; fonts and control rendering use ImGui.
#pragma once
#include <functional>
#include <cfloat>
#include "ftebroom_profile_order.h"
static bool runSettingsWindow=false,runLaunchWindow=false,runEngineWindow=false;
static std::string runUiPendingDocument;
static int runUiTaskSelected=-1,runUiEngineSelected=-1,runUiCloseRequest=0;
static float runUiManagerHeight=442.f;
static nlohmann::json runUiRects=nlohmann::json::object(),runUiGlobalRects=nlohmann::json::object();
static ImVec2 runUiOrigin;
static bool runUiLive=false;
static nlohmann::json runUiInputTrace=nlohmann::json::array();
static std::string runUiRecordPrefix;
static int runUiAtlasWidth=0,runUiAtlasHeight=0,runUiAtlasTheme=-1,runUiAtlasFrames=0;
static std::filesystem::path runUiAtlasDirectory;
static bool runUiImportRequested=false;
static void runUiShow(int mode){if(mode==1)runSettingsWindow=true;else if(mode==2)runLaunchWindow=true;else if(mode==3)runUiCloseRequest=2;else runWindow=true;}
static void runOpenSettings(){runUiShow(1);}
static bool runBeforeDocument(const std::string& next){if(!runActive)return false;runUiPendingDocument=next;runUiCloseRequest=3;return true;}
static bool runUiNavigation(){
 auto* context=ImGui::GetCurrentContext();if(!context)return false;
 for(auto* window=context->NavWindow;window;window=window->ParentWindowInBeginStack){std::string name=window->Name;
  for(const char* prefix:{"Compile","Compilation settings","Launch Engine","Game Engines","Choose file","Import TrenchBroom profiles","Stop compilation?"})if(name==prefix||name.rfind(std::string(prefix)+"/",0)==0)return true;
 }return false;
}
static void runUiConsumeEnter(){auto& g=*ImGui::GetCurrentContext();g.NavActivateId=g.NavActivateDownId=g.NavActivatePressedId=0;}
static bool runUiSave(){return runSaveProfiles();}
static void runUiHint(const char* text){if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",text);}
static void runUiRecord(const std::string& key){auto p=ImGui::GetItemRectMin(),q=ImGui::GetItemRectMax();runUiRects[(key.rfind("##",0)==0?runUiRecordPrefix+key.substr(2):key)]={{"x",p.x-runUiOrigin.x},{"y",p.y-runUiOrigin.y},{"width",q.x-p.x},{"height",q.y-p.y},{"active",ImGui::IsItemActive()}};}
static void runUiRecordGlobal(const std::string& key){if(!runUiLive)return;auto p=ImGui::GetItemRectMin(),q=ImGui::GetItemRectMax();runUiGlobalRects[key]={{"x",p.x},{"y",p.y},{"width",q.x-p.x},{"height",q.y-p.y}};}
struct RunUiStyle {
 RunUiStyle(){
  const bool dark=uiTheme!=1&&uiTheme!=3;
  ImGui::PushStyleColor(ImGuiCol_WindowBg,dark?uiColor(50,50,50):uiColor(240,240,240));
  ImGui::PushStyleColor(ImGuiCol_ChildBg,dark?uiColor(27,27,27):uiColor(255,255,255));
  ImGui::PushStyleColor(ImGuiCol_FrameBg,dark?uiColor(35,35,35):uiColor(255,255,255));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,dark?uiColor(48,48,48):uiColor(247,247,247));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive,dark?uiColor(55,55,55):uiColor(250,250,250));
  ImGui::PushStyleColor(ImGuiCol_Text,dark?uiColor(207,207,207):uiColor(20,20,20));
  ImGui::PushStyleColor(ImGuiCol_TextDisabled,dark?uiColor(110,110,110):uiColor(150,150,150));
  ImGui::PushStyleColor(ImGuiCol_Button,dark?uiColor(53,53,53):uiColor(238,238,238));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,dark?uiColor(65,65,65):uiColor(220,230,240));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,dark?uiColor(62,90,135):uiColor(180,210,240));
  ImGui::PushStyleColor(ImGuiCol_Header,dark?uiColor(62,112,205):uiColor(48,140,198));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered,dark?uiColor(48,65,85):uiColor(220,235,248));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive,dark?uiColor(62,112,205):uiColor(48,140,198));
  ImGui::PushStyleColor(ImGuiCol_Border,dark?uiColor(67,67,67):uiColor(175,175,175));
  ImGui::PushStyleColor(ImGuiCol_Separator,dark?uiColor(67,67,67):uiColor(175,175,175));
  ImGui::PushStyleColor(ImGuiCol_CheckMark,dark?uiColor(207,207,207):uiColor(30,30,30));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4*uiScale(),2*uiScale()});
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{3*uiScale(),2*uiScale()});
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,2*uiScale());
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,uiScale());
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0);
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,14*uiScale());
 }
 ~RunUiStyle(){ImGui::PopStyleVar(8);ImGui::PopStyleColor(16);}
};
static void runUiTitle(const char* title,float width=0){
 const auto p=ImGui::GetCursorScreenPos();const float h=20*uiScale();if(width<=0)width=ImGui::GetContentRegionAvail().x;
 ImGui::GetWindowDrawList()->AddRectFilled(p,{p.x+width,p.y+h},ImGui::GetColorU32(ImGuiCol_WindowBg));
 auto* font=uiBoldFonts[std::clamp(uiScaleChoice,0,4)];if(font)ImGui::PushFont(font);
 ImGui::SetCursorScreenPos({p.x+2*uiScale(),p.y+(h-ImGui::GetFontSize())*.5f});ImGui::TextUnformatted(title);if(font)ImGui::PopFont();
 ImGui::SetCursorScreenPos(p);ImGui::Dummy({width,h});
}
static bool runUiIcon(const char* id,char glyph,const char* hint,bool enabled=true){
 ImGui::BeginDisabled(!enabled);const float u=uiScale();bool clicked=ImGui::InvisibleButton(id,{20*u,20*u},ImGuiButtonFlags_EnableNav);auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
 if(ImGui::IsItemHovered()||ImGui::IsItemActive())ImGui::GetWindowDrawList()->AddRectFilled(a,b,ImGui::GetColorU32(ImGui::IsItemActive()?ImGuiCol_ButtonActive:ImGuiCol_ButtonHovered));
 auto c=ImGui::GetColorU32(enabled?ImGuiCol_Text:ImGuiCol_TextDisabled);auto* draw=ImGui::GetWindowDrawList();float x=(a.x+b.x)*.5f,y=(a.y+b.y)*.5f,r=4*u;
 if(glyph=='+'||glyph=='-'){draw->AddLine({x-r,y},{x+r,y},c,1.5f*u);if(glyph=='+')draw->AddLine({x,y-r},{x,y+r},c,1.5f*u);}
 else{float d=glyph=='^'?1.f:-1.f;draw->AddTriangleFilled({x-r,y+d*2*u},{x+r,y+d*2*u},{x,y-d*3*u},c);}
 runUiRecord(id);runUiHint(hint);ImGui::EndDisabled();return clicked;
}
static nlohmann::json runUiVariables(bool workdir=false,bool engine=false){
 auto vars=engine?runCommonVariables():runWorkdirVariables();if(!workdir&&!engine){vars["CPU_COUNT"]=std::max(1u,std::thread::hardware_concurrency());vars["WORK_DIR_PATH"]="";
  if(runSelected>=0&&runSelected<int(runProfiles.size()))try{vars["WORK_DIR_PATH"]=ftebroom::profile::interpolate(runProfiles[runSelected].work,runWorkdirVariables());}catch(...){}
 }return vars;
}
static ImGuiID runUiCaretId=0;static int runUiCaret=0;
struct RunUiCompletion {int cursor=0;bool tab=false;ImGuiID id=0;std::vector<char>* buffer=nullptr;};
static int runUiCompletionCallback(ImGuiInputTextCallbackData* data){auto& c=*static_cast<RunUiCompletion*>(data->UserData);if(data->EventFlag==ImGuiInputTextFlags_CallbackResize){c.buffer->resize(data->BufSize);data->Buf=c.buffer->data();return 0;}if(data->EventFlag==ImGuiInputTextFlags_CallbackAlways&&runUiCaretId==c.id){data->CursorPos=data->SelectionStart=data->SelectionEnd=runUiCaret;runUiCaretId=0;}c.cursor=data->CursorPos;if(data->EventFlag==ImGuiInputTextFlags_CallbackCompletion)c.tab=true;return 0;}
static std::string runUiRefocus;
struct RunUiCompletionRange {size_t begin,end;std::string prefix;};
static std::map<ImGuiID,RunUiCompletionRange> runUiCompletions;
static bool runUiEdit(const char* id,std::string& value,const char* hint="",int scope=-1,const char* placeholder=""){
 std::vector<char> buffer(std::max<size_t>(4096,value.size()+1024),0);std::copy(value.begin(),value.end(),buffer.begin());RunUiCompletion completion;completion.buffer=&buffer;
 const auto inputId=ImGui::GetID(id);completion.id=inputId;auto key=std::to_string(inputId);if(runUiRefocus==key){ImGui::SetKeyboardFocusHere();runUiRefocus.clear();}
 int caret=int(value.size());if(auto* state=ImGui::GetInputTextState(inputId))caret=state->GetCursorPos();auto prefixAt=value.rfind("${",size_t(std::max(0,caret)));bool completing=scope>=0&&prefixAt!=value.npos&&prefixAt<size_t(caret)&&value.find('}',prefixAt)>=size_t(caret);
 bool changed=ImGui::InputTextWithHint(id,placeholder,buffer.data(),buffer.size(),ImGuiInputTextFlags_CallbackResize|(scope<0?ImGuiInputTextFlags_None:(completing?ImGuiInputTextFlags_CallbackCompletion:0)|ImGuiInputTextFlags_CallbackAlways),runUiCompletionCallback,&completion);
 if(changed)value=buffer.data();runUiHint(hint);runUiRecord(id);
 if(scope>=0){
  auto prefixEnd=std::min(size_t(std::max(0,completion.cursor)),value.size());auto start=value.rfind("${",prefixEnd);
  bool valid=start!=value.npos&&start<prefixEnd&&value.find('}',start)>=prefixEnd;
  if(completion.tab&&valid){runUiCompletions[inputId]={start,prefixEnd,value.substr(start+2,prefixEnd-start-2)};ImGui::OpenPopup("Variables");}
  if(ImGui::BeginPopup("Variables")){
   auto vars=runUiVariables(scope==1,scope==2);auto range=runUiCompletions[inputId];start=range.begin;prefixEnd=range.end;const auto& prefix=range.prefix;bool found=false;
   for(auto it=vars.begin();it!=vars.end();++it)if(it.key().compare(0,prefix.size(),prefix)==0){found=true;
    if(ImGui::Selectable(it.key().c_str())){auto end=value.find('}',start);if(end==value.npos)end=prefixEnd;else ++end;value.replace(start,end-start,"${"+it.key()+"}");changed=true;runUiRefocus=key;runUiCaretId=inputId;runUiCaret=int(start+it.key().size()+3);ImGui::ClearActiveID();ImGui::CloseCurrentPopup();}
    runUiRecord("completion/"+it.key());
    auto text=it.value().is_string()?it.value().get<std::string>():it.value().dump();runUiHint(text.c_str());
   }
   if(!found)ImGui::TextDisabled("No matching variables");ImGui::EndPopup();
  }
 }
 return changed;
}
// A file chooser drawn with the editor's existing ImGui backend on every OS.
static bool runUiBrowseRequested=false,runUiBrowseOpen=false,runUiBrowseAllowNew=false;
static nlohmann::json runUiBrowseRects=nlohmann::json::object();
static std::filesystem::path runUiBrowseFolder;
static std::string runUiBrowsePath,runUiBrowseError;
static std::function<void(const std::string&)> runUiBrowseAccept;
static void runUiChoose(std::string initial,std::function<void(const std::string&)> accept,bool allowNew=false){
 auto p=std::filesystem::u8path(initial);std::error_code error;runUiBrowseFolder=std::filesystem::is_directory(p,error)?p:p.parent_path();
 if(runUiBrowseFolder.empty()||!std::filesystem::is_directory(runUiBrowseFolder,error))runUiBrowseFolder=documentDefaultFolder();
 runUiBrowsePath=std::move(initial);runUiBrowseError.clear();runUiBrowseAccept=std::move(accept);runUiBrowseRequested=runUiBrowseOpen=true;runUiBrowseAllowNew=allowNew;
}
static void runUiBrowsePanel(){
 if(runUiBrowseRequested){runUiBrowseRequested=false;ImGui::OpenPopup("Choose file");}
 ImGui::SetNextWindowSize({640*uiScale(),440*uiScale()},ImGuiCond_FirstUseEver);
 if(ImGui::BeginPopupModal("Choose file",nullptr,ImGuiWindowFlags_NoDocking)){
  auto record=[](const char* key){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();runUiBrowseRects[key]={{"x",a.x},{"y",a.y},{"width",b.x-a.x},{"height",b.y-a.y},{"active",ImGui::IsItemActive()}};};
  if(ImGui::IsKeyPressed(ImGuiKey_Escape)){ImGui::SetKeyOwner(ImGuiKey_Escape,ImGui::GetCurrentWindow()->ID);runUiBrowseOpen=false;ImGui::CloseCurrentPopup();ImGui::EndPopup();return;}
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{8*uiScale(),8*uiScale()});
  if(ImGui::Button("Up"))runUiBrowseFolder=runUiBrowseFolder.parent_path();ImGui::SameLine();ImGui::TextUnformatted(runUiBrowseFolder.u8string().c_str());
  ImGui::BeginChild("Files",{0,-70*uiScale()},ImGuiChildFlags_Borders);
  std::vector<std::filesystem::path> entries;std::error_code error;for(std::filesystem::directory_iterator i(runUiBrowseFolder,error),end;!error&&i!=end;i.increment(error))entries.push_back(i->path());std::sort(entries.begin(),entries.end());
  if(error)ImGui::TextWrapped("%s",error.message().c_str());
  for(const auto& p:entries){bool folder=std::filesystem::is_directory(p,error);auto label=(folder?"[Folder] ":"")+p.filename().u8string();
   if(ImGui::Selectable(label.c_str(),runUiBrowsePath==p.u8string(),ImGuiSelectableFlags_AllowDoubleClick)){if(folder){runUiBrowseFolder=p;break;}runUiBrowsePath=p.u8string();}
  }
  ImGui::EndChild();ImGui::SetNextItemWidth(-1);runUiEdit("##Selected file",runUiBrowsePath);record("file-path");
  if(ImGui::Button(runUiBrowseAllowNew?"Save":"Choose",{80*uiScale(),0})){
   auto path=std::filesystem::u8path(runUiBrowsePath);
   if(!runUiBrowsePath.empty()&&(runUiBrowseAllowNew?!std::filesystem::is_directory(path,error):std::filesystem::is_regular_file(path,error))){
    try{if(runUiBrowseAccept)runUiBrowseAccept(runUiBrowsePath);runUiBrowseOpen=false;ImGui::CloseCurrentPopup();}catch(const std::exception& e){runUiBrowseError=e.what();}
   }else runUiBrowseError=runUiBrowseAllowNew?"Choose a file path.":"Choose an existing file.";
  }record("file-choose");
  ImGui::SameLine();if(ImGui::Button("Cancel",{80*uiScale(),0})){runUiBrowseOpen=false;ImGui::CloseCurrentPopup();}record("file-cancel");if(!runUiBrowseError.empty())ImGui::TextWrapped("%s",runUiBrowseError.c_str());
  ImGui::PopStyleVar();ImGui::EndPopup();
 }
}
static void runUiProfileMutation(const std::string& action,int row){
 if(action=="add"){RunProfile p;p.name="unnamed";p.trenchBroom=true;runProfiles.push_back(std::move(p));runSelected=int(runProfiles.size()-1);}
 else if(row>=0&&row<int(runProfiles.size())){if(action=="duplicate"){auto p=runProfiles[row];runProfiles.push_back(std::move(p));runSelected=int(runProfiles.size()-1);}else if(action=="remove"){runProfiles.erase(runProfiles.begin()+row);runSelected=std::min(row,int(runProfiles.size())-1);}}
 runUiTaskSelected=-1;runUiSave();
}
static void runUiTaskMutation(const std::string& action,int row){
 if(runSelected<0||runSelected>=int(runProfiles.size()))return;auto& tasks=runProfiles[runSelected].tasks;
 if(row<0||row>=int(tasks.size()))return;
 if(action=="up"&&row>0){std::swap(tasks[row],tasks[row-1]);--row;}
 else if(action=="down"&&row+1<int(tasks.size())){std::swap(tasks[row],tasks[row+1]);++row;}
 else if(action=="duplicate"){auto copy=tasks[row];tasks.insert(tasks.begin()+row+1,std::move(copy));++row;}
 else if(action=="remove"){tasks.erase(tasks.begin()+row);row=std::min(row,int(tasks.size())-1);}
 runUiTaskSelected=row;runUiSave();
}
static void runUiAddTask(int kind){
 if(runSelected<0||runSelected>=int(runProfiles.size()))return;RunTask t;t.kind=kind;
 if(kind==0)t.source="${WORK_DIR_PATH}/${MAP_BASE_NAME}-compile.map";if(kind==2)t.flags=RunCopyToDirectory;if(kind==1)t.flags=RunAllowNonzero;
 if(kind==5){t.flags=RunAllowLaunchFailure;if(!runEngines.empty())t.engineProfileId=runEngines.front().id;}
 auto& a=runProfiles[runSelected].tasks;int row=runUiTaskSelected<0?int(a.size()):std::min(runUiTaskSelected+1,int(a.size()));a.insert(a.begin()+row,std::move(t));runUiTaskSelected=row;runUiSave();
}
static void runUiFormRow(const char* label){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);ImGui::TableNextColumn();ImGui::SetNextItemWidth(-FLT_MIN);}
static bool runUiTaskCard(RunTask& t,int index,std::string& mutation){
 const char* names[]={"Export Map","Run Tool","Copy Files","Rename File","Delete Files","Launch Engine"};bool changed=false;const float u=uiScale(),w=ImGui::GetContentRegionAvail().x;
 const int rows=t.kind==0?4:t.kind==1?3:t.kind==4?1:2;const float row=std::max(22*u,ImGui::GetFrameHeight()),body=16*u+rows*row+(rows-1)*4*u,height=20*u+body+u;
 auto p=ImGui::GetCursorScreenPos();runUiRecordPrefix="task/"+std::to_string(index)+"/";ImGui::PushID(index);ImGui::SetNextItemAllowOverlap();
 if(ImGui::Selectable("##card",runUiTaskSelected==index,ImGuiSelectableFlags_AllowOverlap,{w,height}))runUiTaskSelected=index;
 runUiRecord("card/"+std::to_string(index));
 if(ImGui::BeginPopupContextItem("Task context")){if(ImGui::MenuItem("Move Up",nullptr,false,index>0))mutation="up";if(ImGui::MenuItem("Move Down",nullptr,false,runSelected>=0&&index+1<int(runProfiles[runSelected].tasks.size())))mutation="down";ImGui::Separator();if(ImGui::MenuItem("Duplicate"))mutation="duplicate";runUiRecord("task-context/duplicate");if(ImGui::MenuItem("Remove"))mutation="remove";runUiRecord("task-context/remove");if(!mutation.empty())runUiTaskSelected=index;ImGui::EndPopup();}
 ImGui::SetCursorScreenPos(p);runUiTitle(names[std::clamp(t.kind,0,5)],w);
 ImGui::SetCursorScreenPos({p.x+2*u,p.y+20*u+(body-ImGui::GetFrameHeight())*.5f});changed|=ImGui::Checkbox("##Enabled",&t.enabled);runUiRecord("task/"+std::to_string(index)+"/enabled");runUiHint("Whether to include this task when running the compile profile");
 ImGui::SetCursorScreenPos({p.x+28*u,p.y+28*u});ImGui::BeginGroup();
 ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,{0,2*u});
 if(ImGui::BeginTable("Form",2,ImGuiTableFlags_SizingFixedFit,{w-36*u,0})){
  float label=t.kind==0?72*u:t.kind==2?124*u:t.kind==3?99*u:t.kind==1?64*u:t.kind==5?81*u:48*u;ImGui::TableSetupColumn("Label",ImGuiTableColumnFlags_WidthFixed,label);ImGui::TableSetupColumn("Field",ImGuiTableColumnFlags_WidthStretch);
  auto field=[&](const char* label,const char* id,std::string& value,const char* tooltip){runUiFormRow(label);changed|=runUiEdit(id,value,tooltip,0);};
  if(t.kind==0){
   field("File Path","##Export path",t.source,"The path of the exported file.\nVariables are allowed. Use Tab for completion.");
   runUiFormRow("Strip Entities");changed|=runUiEdit("##Strip entities",t.stripEntityPattern,"Set a GLOB pattern to strip any entities with a matching classname. Example: info_player_*",-1,"enter GLOB pattern");
   runUiFormRow("Add Entity");std::string classname;if(t.entityToAdd.is_array())for(const auto& prop:t.entityToAdd)if(prop.at("key")=="classname")classname=prop.at("value").get<std::string>();
   if(runUiEdit("##Add entity",classname,"Set the classname of an entity added at the camera position and view direction.",-1,"enter classname")){
    changed=true;if(classname.empty())t.entityToAdd=nullptr;else{if(!t.entityToAdd.is_array())t.entityToAdd=nlohmann::json::array();bool found=false;for(auto& prop:t.entityToAdd)if(prop.at("key")=="classname"){prop["value"]=classname;found=true;}if(!found)t.entityToAdd.push_back({{"key","classname"},{"value",classname}});}
   }
   runUiFormRow("");changed|=ImGui::Checkbox("Strip TrenchBroom specific entity properties",&t.stripTbProperties);runUiHint("Strip entity properties starting with _tb_ from the exported map file. Some compilers cannot handle these properties.");
  }else if(t.kind==1){
   runUiFormRow("Tool Path");float available=ImGui::GetContentRegionAvail().x;ImGui::SetNextItemWidth(std::max(20*u,available-82*u));changed|=runUiEdit("##Tool path",t.source,"The absolute path to the executable of the tool that should be run. Variables are allowed.",0);ImGui::SameLine(0,2*u);
   if(ImGui::Button("...",{80*u,0})){auto game=runStoreGame;int profile=runSelected;runUiChoose(t.source,[game,profile,index](const std::string& path){if(runStoreGame==game&&profile<int(runProfiles.size())&&index<int(runProfiles[profile].tasks.size())){runProfiles[profile].tasks[index].source=path;runUiSave();}});}runUiHint("Click to browse");
   field("Parameters","##Parameters",t.destination,"The parameters passed to the tool. Variables are allowed. Use Tab for completion.");
   runUiFormRow("");bool stop=(t.flags&RunAllowNonzero)==0;if(ImGui::Checkbox("Stop on nonzero error code",&stop)){changed=true;t.flags=stop?t.flags&~RunAllowNonzero:t.flags|RunAllowNonzero;}runUiHint("Stop compilation if the tool exits with a nonzero result code.");
  }else if(t.kind==2||t.kind==3){
   field("Source File Path","##Source path",t.source,t.kind==2?"The file(s) to copy. Wildcards (*, ?) are allowed in the filename. Variables are allowed.":"The file to rename or move. Wildcards are not supported. Variables are allowed.");
   field(t.kind==2?"Target Directory Path":"Target File Path","##Target path",t.destination,t.kind==2?"The directory to copy files to. It is created recursively. Existing files are overwritten. Variables are allowed.":"The new path for the file, including filename. Parent directories are created recursively. Existing files are overwritten. Variables are allowed.");
  }else if(t.kind==4)field("File Path","##Delete path",t.source,"The file(s) to delete. Wildcards (*, ?) are allowed in the filename. Searches subdirectories recursively. Variables are allowed.");
  else{
   runUiFormRow("Engine Profile");std::string name;for(const auto& engine:runEngines)if(engine.id==t.engineProfileId)name=engine.name;
   if(ImGui::BeginCombo("##Engine profile",name.c_str())){for(const auto& engine:runEngines)if(ImGui::Selectable((engine.name+"###"+engine.id).c_str(),engine.id==t.engineProfileId)){t.engineProfileId=engine.id;changed=true;}ImGui::EndCombo();}runUiHint("The Launch Engine profile to start when this task runs");
   runUiFormRow("");bool stop=(t.flags&RunAllowLaunchFailure)==0;if(ImGui::Checkbox("Stop on launch failure",&stop)){changed=true;t.flags=stop?t.flags&~RunAllowLaunchFailure:t.flags|RunAllowLaunchFailure;}runUiHint("Stop compilation if the engine cannot be launched");
  }
  ImGui::EndTable();
 }
 ImGui::PopStyleVar();ImGui::EndGroup();ImGui::SetCursorScreenPos({p.x,p.y+height-u});ImGui::Separator();ImGui::SetCursorScreenPos(p);ImGui::Dummy({w,height});ImGui::PopID();runUiRecordPrefix.clear();return changed;
}
static std::string runUiElide(std::string text,float width){if(ImGui::CalcTextSize(text.c_str()).x<=width)return text;while(!text.empty()&&ImGui::CalcTextSize((text+"...").c_str()).x>width){size_t at=text.size()-1;while(at>0&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)--at;text.resize(at);}return text+"...";}
static bool runUiListItem(const std::string& id,const std::string& title,const std::string& subtitle,bool selected){
 const float u=uiScale(),w=ImGui::GetContentRegionAvail().x,h=34*u;auto p=ImGui::GetCursorScreenPos();
 bool clicked=ImGui::Selectable(("##"+id).c_str(),selected,ImGuiSelectableFlags_AllowDoubleClick|ImGuiSelectableFlags_SelectOnNav,{w,h});runUiRecord(id);if(runUiLive)runUiRects[id]["selected"]=selected;
 auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(p,{p.x+w,p.y+h},true);
 auto* font=uiBoldFonts[std::clamp(uiScaleChoice,0,4)];if(font)ImGui::PushFont(font);auto name=runUiElide(title,w-8*u);draw->AddText({p.x+4*u,p.y+2*u},ImGui::GetColorU32(ImGuiCol_Text),name.c_str());if(font)ImGui::PopFont();
 draw->AddText({p.x+4*u,p.y+17*u},ImGui::GetColorU32(ImGuiCol_Text),runUiElide(subtitle,w-8*u).c_str());draw->PopClipRect();
 draw->AddLine({p.x,p.y+h},{p.x+w,p.y+h},ImGui::GetColorU32(ImGuiCol_Border));runUiHint(title.c_str());return clicked;
}
static void runUiProfiles(float height){
 const float u=uiScale();runUiTitle("Profiles");std::string action;int actionRow=-1;
 ImGui::BeginChild("Profile list",{0,std::max(1.f,height-64*u)},ImGuiChildFlags_None);
 for(int i=0;i<int(runProfiles.size());++i){const auto& p=runProfiles[i];ImGui::PushID(i);
  if(runUiListItem("profile/"+std::to_string(i),p.name,std::to_string(p.tasks.size())+" tasks",i==runSelected)){runSelected=i;runUiTaskSelected=-1;}
  if(ImGui::BeginPopupContextItem("Profile context")){if(ImGui::MenuItem("Duplicate")){action="duplicate";actionRow=i;}runUiRecord("profile-context/duplicate");if(ImGui::MenuItem("Remove")){action="remove";actionRow=i;}runUiRecord("profile-context/remove");ImGui::EndPopup();}ImGui::PopID();
 }
 if(runProfiles.empty())ImGui::TextWrapped("Click the '+' button to create a compilation profile.");ImGui::EndChild();
 ImGui::SetCursorPosY(height-44*u);if(ImGui::Button("Import TrenchBroom profiles...",{ImGui::GetContentRegionAvail().x,22*u}))runUiImportRequested=true;runUiRecord("import");
 ImGui::SetCursorPosY(height-20*u);if(runUiIcon("profile-add",'+',"Add profile"))action="add";ImGui::SameLine(0,0);if(runUiIcon("profile-remove",'-',"Remove the selected profile",runSelected>=0&&runSelected<int(runProfiles.size()))){action="remove";actionRow=runSelected;}
 ImGui::SameLine(0,0);if(runUiIcon("profile-up",'^',"Move profile up",runSelected>0))if(ftebroom::reorderProfiles(runProfiles,runSelected,"up"))runUiSave();
 ImGui::SameLine(0,0);if(runUiIcon("profile-down",'v',"Move profile down",runSelected>=0&&runSelected+1<int(runProfiles.size())))if(ftebroom::reorderProfiles(runProfiles,runSelected,"down"))runUiSave();
 ImGui::SameLine(0,2*u);if(ImGui::SmallButton("A-Z##profiles"))if(ftebroom::reorderProfiles(runProfiles,runSelected,"sort"))runUiSave();runUiRecord("profile-sort");runUiHint("Sort profiles by name; keeps the selected profile");
 if(!action.empty())runUiProfileMutation(action,actionRow);
}
static void runUiDetails(float height){
 const float u=uiScale();runUiTitle("Details");if(runSelected<0||runSelected>=int(runProfiles.size())){ImGui::TextWrapped("Select a compilation profile");return;}
 auto& p=runProfiles[runSelected];bool changed=false;ImGui::SetCursorPos({4*u,24*u});
 ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,{0,0});
 if(ImGui::BeginTable("Profile fields",2,ImGuiTableFlags_SizingFixedFit,{ImGui::GetContentRegionAvail().x-4*u,0})){
  ImGui::TableSetupColumn("Label",ImGuiTableColumnFlags_WidthFixed,100*u);ImGui::TableSetupColumn("Field",ImGuiTableColumnFlags_WidthStretch);
  runUiFormRow("Name");changed|=runUiEdit("##Profile name",p.name);
  runUiFormRow("Working Directory");changed|=runUiEdit("##Working directory",p.work,"A working directory for the compilation profile. Variables are allowed. Use Tab for completion.",1);ImGui::EndTable();
 }
 ImGui::PopStyleVar();const float head=std::max(80*u,ImGui::GetCursorPosY()+8*u);ImGui::SetCursorPos({0,head-u});ImGui::Separator();ImGui::SetCursorPos({0,head});
 std::string mutation;ImGui::BeginChild("Tasks",{0,std::max(1.f,height-head-21*u)},ImGuiChildFlags_None);
 if(p.tasks.empty())ImGui::TextWrapped("Click the '+' button to create a task.");
 for(int i=0;i<int(p.tasks.size());++i){changed|=runUiTaskCard(p.tasks[i],i,mutation);if(!mutation.empty())break;}
 ImGui::EndChild();if(changed)runUiSave();if(!mutation.empty())runUiTaskMutation(mutation,runUiTaskSelected);
 ImGui::SetCursorPos({0,height-20*u});if(runUiIcon("task-add",'+',"Add task"))ImGui::OpenPopup("Add task");
 if(ImGui::BeginPopup("Add task")){const char* titles[]={"Export Map","Copy Files","Rename File","Delete Files","Run Tool","Launch Engine"};int kinds[]={0,2,3,4,1,5};for(int i=0;i<6;++i){if(ImGui::MenuItem(titles[i]))runUiAddTask(kinds[i]);runUiRecord("add-task/"+std::to_string(kinds[i]));}ImGui::EndPopup();}
 ImGui::SameLine(0,0);if(runUiIcon("task-remove",'-',"Remove the selected task",runUiTaskSelected>=0&&runUiTaskSelected<int(p.tasks.size())))runUiTaskMutation("remove",runUiTaskSelected);
 ImGui::SameLine(0,0);if(runUiIcon("task-up",'^',"Move the selected task up",runUiTaskSelected>0&&runUiTaskSelected<int(p.tasks.size())))runUiTaskMutation("up",runUiTaskSelected);
 ImGui::SameLine(0,0);if(runUiIcon("task-down",'v',"Move the selected task down",runUiTaskSelected>=0&&runUiTaskSelected+1<int(p.tasks.size())))runUiTaskMutation("down",runUiTaskSelected);
}
static void runUiCloseCompile(){if(runActive)runUiCloseRequest=1;else{runWindow=false;runUiSave();}}
static void runUiCompile(){
 const float u=uiScale();if(runUiAtlasWidth){ImGui::SetNextWindowSize({runUiAtlasWidth*u,runUiAtlasHeight*u},ImGuiCond_Always);ImGui::SetNextWindowPos({8*u,32*u},ImGuiCond_Always);}
 else{ImGui::SetNextWindowSize({800*u,600*u},ImGuiCond_FirstUseEver);ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),ImGuiCond_FirstUseEver,{.5f,.5f});}
 ImGui::SetNextWindowSizeConstraints({600*u,300*u},ImGui::GetIO().DisplaySize);
 bool open=true;bool visible=ImGui::Begin("Compile",&open,ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
 runUiOrigin=ImGui::GetWindowPos();runUiRects=nlohmann::json::object();auto windowSize=ImGui::GetWindowSize();runUiRects["window"]={{"x",runUiOrigin.x},{"y",runUiOrigin.y},{"width",windowSize.x},{"height",windowSize.y}};
 if(!open)runUiCloseCompile();
 bool defaultEnter=false;
 if(visible){
  if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)){
   if(ImGui::IsKeyPressed(ImGuiKey_Escape)){ImGui::SetKeyOwner(ImGuiKey_Escape,ImGui::GetCurrentWindow()->ID);runUiCloseCompile();}
   if(ImGui::IsKeyPressed(ImGuiKey_Enter)){auto id=ImGui::GetCurrentContext()->NavId;bool footer=false;for(const char* label:{"Launch...","Stop","Test","Compile","Close"})footer|=id==ImGui::GetID(label);if(!footer){defaultEnter=true;runUiConsumeEnter();}}
  }
  const auto available=ImGui::GetContentRegionAvail();const float footer=44*u,splitter=6*u;float top=ImGui::GetCursorPosY();float usable=available.y-footer-splitter;
  runUiManagerHeight=std::clamp(runUiManagerHeight,140.f,std::max(140.f,usable/u-40.f));float manager=std::min(runUiManagerHeight*u,usable-30*u);
  ImGui::BeginChild("Manager",{0,manager},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
  float side=std::min(256*u,ImGui::GetContentRegionAvail().x*.43f);ImGui::BeginChild("Profiles",{side,manager},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);runUiProfiles(manager);ImGui::EndChild();ImGui::SameLine(0,u);
  ImGui::BeginChild("Details",{0,manager},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);runUiDetails(manager);ImGui::EndChild();ImGui::EndChild();
  ImGui::SetCursorPosY(top+manager);ImGui::InvisibleButton("Output splitter",{available.x,splitter});runUiRecord("splitter");auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();ImGui::GetWindowDrawList()->AddRectFilled(a,b,ImGui::GetColorU32(ImGuiCol_Border));ImGui::GetWindowDrawList()->AddRectFilled({(a.x+b.x)*.5f-20*u,a.y+u},{(a.x+b.x)*.5f+20*u,b.y-u},ImGui::GetColorU32(uiTheme==1||uiTheme==3?uiColor(210,210,210):uiColor(90,90,90)));
  if(ImGui::IsItemHovered()||ImGui::IsItemActive())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);if(ImGui::IsItemActive())runUiManagerHeight+=ImGui::GetIO().MouseDelta.y/u;
  const float output=std::max(20*u,available.y-manager-splitter-footer);ImGui::SetCursorPosY(top+manager+splitter);
  static std::string cached,selection;static std::vector<char> buffer(1,0);static bool selectAll=false;bool newLog=cached!=runLog;
  if(newLog){cached=runLog;buffer.assign(cached.begin(),cached.end());buffer.push_back(0);}
  if(selectAll)ImGui::SetKeyboardFocusHere();
  ImGui::InputTextMultiline("##Output",buffer.data(),buffer.size(),{available.x,output},ImGuiInputTextFlags_ReadOnly);auto outputId=ImGui::GetItemID();runUiRecord("output");
  if(auto* state=ImGui::GetInputTextState(outputId)){if(selectAll){state->SelectAll();selectAll=false;}if(state->HasSelection()){int begin=state->GetSelectionStart(),end=state->GetSelectionEnd();if(begin>end)std::swap(begin,end);begin=std::clamp(begin,0,int(runLog.size()));end=std::clamp(end,begin,int(runLog.size()));selection=runLog.substr(size_t(begin),size_t(end-begin));}else selection.clear();}
  auto* window=ImGui::GetCurrentWindow();if(!window->DC.ChildWindows.empty()){auto* child=window->DC.ChildWindows.back();static bool followed=true;bool userScroll=(ImGui::IsItemHovered()&&ImGui::GetIO().MouseWheel!=0)||ImGui::GetActiveID()==ImGui::GetWindowScrollbarID(child,ImGuiAxis_Y);if(userScroll)followed=child->Scroll.y>=child->ScrollMax.y-2*u;if(newLog&&followed)ImGui::SetScrollY(child,1e20f);else if(!newLog)followed=child->Scroll.y>=child->ScrollMax.y-2*u;}
  if(ImGui::BeginPopupContextItem("Output context")){if(ImGui::MenuItem("Copy",nullptr,false,!selection.empty()))ImGui::SetClipboardText(selection.c_str());if(ImGui::MenuItem("Select All",nullptr,false,!runLog.empty()))selectAll=true;ImGui::Separator();if(ImGui::MenuItem("Clear")){runLog.clear();selection.clear();}ImGui::EndPopup();}
  ImGui::SetCursorPosY(top+available.y-footer+10*u);float button=80*u,gap=6*u,total=5*button+4*gap;ImGui::SetCursorPosX(std::max(8*u,available.x-total-8*u));
  if(ImGui::Button("Launch...",{button,24*u}))runUiShow(2);runUiRecord("launch");ImGui::SameLine(0,gap);
  ImGui::BeginDisabled(!runActive);if(ImGui::Button("Stop",{button,24*u}))runStop();runUiRecord("stop");ImGui::EndDisabled();ImGui::SameLine(0,gap);
  bool canRun=!runActive&&!runExclusive&&runSelected>=0&&runSelected<int(runProfiles.size())&&!runProfiles[runSelected].tasks.empty();
  ImGui::BeginDisabled(!canRun);if(ImGui::Button("Test",{button,24*u})&&runUiSave())runBegin(runProfiles[runSelected],true);runUiRecord("test");ImGui::SameLine(0,gap);
  if(ImGui::Button("Compile",{button,24*u})&&runUiSave())runBegin(runProfiles[runSelected]);runUiRecord("compile");ImGui::EndDisabled();ImGui::SameLine(0,gap);
  if(ImGui::Button("Close",{button,24*u}))runUiCloseCompile();runUiRecord("close");
  if(defaultEnter&&canRun&&runWindow&&!runUiCloseRequest&&runUiSave())runBegin(runProfiles[runSelected]);
  if(runActive){ImGui::SetCursorPos({8*u,top+available.y-footer+12*u});ImGui::TextUnformatted(("Running "+runningProfile.name).c_str());}
 }
 if(runUiImportRequested){runUiImportRequested=false;runImportOpen();}runImportPanel();
 if(runUiAtlasFrames>0&&--runUiAtlasFrames==0){
  try{std::filesystem::create_directories(runUiAtlasDirectory);std::ofstream(runUiAtlasDirectory/"geometry.json")<<nlohmann::json{{"toolkit","ImGui"},{"scale",u},{"theme",uiTheme},{"widgets",runUiRects}}.dump(2);auto relative=(runUiAtlasDirectory.lexically_relative(documentPath("ftebroom.cfg").parent_path())/"compile.png").generic_u8string();command(("screenshot "+organizationQuote(relative)+"\n").c_str());}catch(const std::exception& e){runError=e.what();}
  runUiAtlasWidth=runUiAtlasHeight=0;
 }
 ImGui::End();
}
static nlohmann::json runUiSettingsDraft;
static std::string runUiSettingsGame,runUiModsText,runUiNewVariable;
static void runUiSettings(){
 static bool wasOpen=false;if(!runSettingsWindow){wasOpen=false;return;}
 if(!wasOpen||runUiSettingsGame!=runStoreGame){runUiSettingsDraft=runUiState();runUiSettingsGame=runStoreGame;runUiModsText.clear();for(const auto& m:runUiSettingsDraft["enabledMods"]){if(!runUiModsText.empty())runUiModsText+='\n';runUiModsText+=m.get<std::string>();}wasOpen=true;}
 ImGui::SetNextWindowSize({760*uiScale(),560*uiScale()},ImGuiCond_FirstUseEver);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{8*uiScale(),8*uiScale()});
 if(ImGui::Begin("Compilation settings",&runSettingsWindow,ImGuiWindowFlags_NoDocking)){
  ImGui::TextUnformatted(gameProfileName());ImGui::Separator();
  if(ImGui::BeginTabBar("Settings tabs")){
   if(ImGui::BeginTabItem("Paths and tools")){
    ImGui::BeginChild("Paths",{0,-40*uiScale()});
    std::string root=runUiSettingsDraft["gameRoot"],base=runUiSettingsDraft["baseMod"];
    if(runText("Game directory",root))runUiSettingsDraft["gameRoot"]=root;if(runText("Default mod",base))runUiSettingsDraft["baseMod"]=base;
    ImGui::TextUnformatted("Enabled mods (one per line, in map load order)");std::vector<char> buffer(std::max<size_t>(4096,runUiModsText.size()+1024),0);std::copy(runUiModsText.begin(),runUiModsText.end(),buffer.begin());if(ImGui::InputTextMultiline("##Mods",buffer.data(),buffer.size(),{-1,70*uiScale()}))runUiModsText=buffer.data();
    ImGui::TextUnformatted("Compilation tools / variables");
    std::string remove;
    if(ImGui::BeginTable("Tools",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerH)){
     ImGui::TableSetupColumn("Tool / variable",ImGuiTableColumnFlags_WidthFixed,150*uiScale());ImGui::TableSetupColumn("Path / value",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,44*uiScale());ImGui::TableHeadersRow();
     auto& vars=runUiSettingsDraft["variables"];for(auto it=vars.begin();it!=vars.end();++it){ImGui::PushID(it.key().c_str());ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(it.key().c_str());ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);auto value=it.value().get<std::string>();if(runUiEdit("##Tool value",value))it.value()=value;ImGui::TableNextColumn();if(ImGui::SmallButton("...")){auto key=it.key();runUiChoose(value,[key](const std::string& p){runUiSettingsDraft["variables"][key]=p;});}ImGui::SameLine();if(ImGui::SmallButton("-"))remove=it.key();ImGui::PopID();}ImGui::EndTable();
    }
    if(!remove.empty())runUiSettingsDraft["variables"].erase(remove);
    ImGui::SetNextItemWidth(160*uiScale());runUiEdit("##New variable",runUiNewVariable);ImGui::SameLine();if(ImGui::Button("Add tool / variable")&&!runUiNewVariable.empty()){runUiSettingsDraft["variables"].emplace(runUiNewVariable,"");runUiNewVariable.clear();}
    ImGui::EndChild();ImGui::EndTabItem();
   }
   if(ImGui::BeginTabItem("Engine profiles")){ImGui::TextWrapped("Configure named game engines for Launch Engine tasks and the Launch dialog.");if(ImGui::Button("Configure engines..."))runEngineWindow=true;ImGui::EndTabItem();}
   if(ImGui::BeginTabItem("Bundled presets")){
    static int selected=-1;ImGui::BeginChild("Presets",{0,-80*uiScale()});for(int i=0;i<int(runBundledProfiles.size());++i)if(ImGui::Selectable(runBundledProfiles[i].name.c_str(),selected==i))selected=i;ImGui::EndChild();
    ImGui::BeginDisabled(selected<0||selected>=int(runBundledProfiles.size()));if(ImGui::Button("Add selected bundled preset")){runProfiles.push_back(runBundledProfiles[selected]);runSelected=int(runProfiles.size()-1);runUiSave();}ImGui::EndDisabled();ImGui::EndTabItem();
   }ImGui::EndTabBar();
  }
  if(ImGui::Button("Save",{80*uiScale(),0})){
   auto mods=nlohmann::json::array();std::istringstream lines(runUiModsText);std::string line;while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(!line.empty())mods.push_back(line);}
   auto current=runUiState();auto q=runUiSettingsDraft;q["action"]="settings";q["enabledMods"]=mods;q["engines"]=current["engines"];auto r=runUiExecute(q);if(!r.contains("error"))runSettingsWindow=false;
  }
  ImGui::SameLine();if(ImGui::Button("Cancel",{80*uiScale(),0}))runSettingsWindow=false;if(!runError.empty())ImGui::TextWrapped("%s",runError.c_str());
 }
 ImGui::End();ImGui::PopStyleVar();
}
static std::vector<RunEngineProfile> runUiEngineDraft;
static std::string runUiEngineGame;
static void runUiEngines(){
 static bool wasOpen=false;if(!runEngineWindow){wasOpen=false;return;}
 if(!wasOpen||runUiEngineGame!=runStoreGame){runUiEngineDraft=runEngines;runUiEngineGame=runStoreGame;runUiEngineSelected=-1;wasOpen=true;}
 ImGui::SetNextWindowSize({820*uiScale(),520*uiScale()},ImGuiCond_FirstUseEver);bool open=true;
 if(ImGui::Begin("Game Engines",&open,ImGuiWindowFlags_NoDocking)){
  if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Escape)&&!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)){ImGui::SetKeyOwner(ImGuiKey_Escape,ImGui::GetCurrentWindow()->ID);open=false;}
  float u=uiScale();runUiTitle(gameProfileName());auto available=ImGui::GetContentRegionAvail();float body=available.y-44*u;
  ImGui::BeginChild("Engine profiles",{std::min(250*u,available.x*.45f),body});runUiTitle("Profiles");ImGui::BeginChild("Engine list",{0,gameProfile==2?-50*u:-20*u});
  for(int i=0;i<int(runUiEngineDraft.size());++i)if(runUiListItem("engine/"+std::to_string(i),runUiEngineDraft[i].name,runUiEngineDraft[i].path,i==runUiEngineSelected))runUiEngineSelected=i;
  if(runUiEngineDraft.empty())ImGui::TextWrapped("Click the '+' button to create a game engine profile.");ImGui::EndChild();
  if(runUiIcon("engine-add",'+',"Add profile")){runUiEngineDraft.push_back({"engine-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()),"","",""});runUiEngineSelected=int(runUiEngineDraft.size()-1);}
  ImGui::SameLine(0,0);if(runUiIcon("engine-remove",'-',"Remove the selected profile",runUiEngineSelected>=0&&runUiEngineSelected<int(runUiEngineDraft.size()))){runUiEngineDraft.erase(runUiEngineDraft.begin()+runUiEngineSelected);runUiEngineSelected=std::min(runUiEngineSelected,int(runUiEngineDraft.size())-1);}
  ImGui::SameLine(0,0);if(runUiIcon("engine-up",'^',"Move engine up",runUiEngineSelected>0))ftebroom::reorderProfiles(runUiEngineDraft,runUiEngineSelected,"up");
  ImGui::SameLine(0,0);if(runUiIcon("engine-down",'v',"Move engine down",runUiEngineSelected>=0&&runUiEngineSelected+1<int(runUiEngineDraft.size())))ftebroom::reorderProfiles(runUiEngineDraft,runUiEngineSelected,"down");
  ImGui::SameLine(0,2*u);if(ImGui::SmallButton("A-Z##engines"))ftebroom::reorderProfiles(runUiEngineDraft,runUiEngineSelected,"sort");runUiRecord("engine-sort");runUiHint("Sort engines by name; keeps the selected engine");
  if(gameProfile==2){for(const char* name:{"VKQuake","Ironwail"}){if(ImGui::SmallButton(name)){RunEngineProfile e;e.id="engine-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());e.name=name;ftebroom::quakeEngineDefaults(e);runUiEngineDraft.push_back(std::move(e));runUiEngineSelected=int(runUiEngineDraft.size()-1);}ImGui::SameLine();}ImGui::NewLine();}
  ImGui::EndChild();ImGui::SameLine(0,u);
  ImGui::BeginChild("Engine details",{0,body});runUiTitle("Details");
  if(runUiEngineSelected>=0&&runUiEngineSelected<int(runUiEngineDraft.size())){
   auto& e=runUiEngineDraft[runUiEngineSelected];ImGui::SetCursorPos({8*u,28*u});ImGui::BeginGroup();
   if(ImGui::BeginTable("Engine fields",2,ImGuiTableFlags_SizingFixedFit,{ImGui::GetContentRegionAvail().x-8*u,0})){
    ImGui::TableSetupColumn("Labels",ImGuiTableColumnFlags_WidthFixed,86*u);ImGui::TableSetupColumn("Fields",ImGuiTableColumnFlags_WidthStretch);runUiFormRow("Name");runUiEdit("##Engine name",e.name,"Choose a name");
    runUiFormRow("Path");ImGui::SetNextItemWidth(std::max(10*u,ImGui::GetContentRegionAvail().x-58*u));runUiEdit("##Engine path",e.path,"Click on the button to choose...");ImGui::SameLine(0,2*u);if(ImGui::Button("...",{56*u,0})){int selected=runUiEngineSelected;runUiChoose(e.path,[selected](const std::string& path){if(selected<int(runUiEngineDraft.size())){auto& e=runUiEngineDraft[selected];e.path=path;auto stem=std::filesystem::u8path(path).stem().u8string();if(e.name.empty())e.name=stem;for(auto& c:stem)c=char(std::tolower(static_cast<unsigned char>(c)));if(e.parameters.empty()&&(stem=="vkquake"||stem=="ironwail"))ftebroom::quakeEngineDefaults(e);}});}
    runUiFormRow("Working dir");runUiEdit("##Engine workdir",e.work,"Blank uses the executable directory; variables allowed.",1);
    runUiFormRow("Arguments");runUiEdit("##Engine arguments",e.parameters,"Editable command line; quote paths with spaces.",1);
    runUiFormRow("Compiled map");runUiEdit("##Engine map file",e.mapFile,"Optional Quake BSP to copy to -basedir/-game/maps before launch.",1);
    ImGui::EndTable();
    ImGui::TextWrapped("Compiled map enables Quake 1 BSP validation and automatic deployment using -basedir, -game and +map. Leave blank for existing engines or your own Copy tasks.");
    if(gameProfile==2&&ImGui::Button("Use Quake launch template"))ftebroom::quakeEngineDefaults(e);
    ImGui::TextWrapped("Variables include GAME_DIR_PATH (game root), MODS[-1] (active mod), MAP_BASE_NAME, WORK_DIR_PATH and ENGINE_DIR_PATH.");
   }ImGui::EndGroup();
  }else ImGui::TextWrapped("Select a game engine profile");ImGui::EndChild();
  ImGui::SetCursorPosX(std::max(8*u,ImGui::GetContentRegionAvail().x-88*u));if(ImGui::Button("Close",{80*u,24*u}))open=false;runUiRecord("engine-close");runUiRecord("engine-manager-close");
 }
 ImGui::End();if(!open){nlohmann::json engines=nlohmann::json::array();for(const auto& e:runUiEngineDraft)engines.push_back(runEngineToJson(e));if(!runUiExecute({{"action","engineProfiles"},{"engines",engines}}).contains("error"))runEngineWindow=false;}
}
static void runUiLaunch(){
 if(!runLaunchWindow)return;float u=uiScale();ImGui::SetNextWindowSize({660*u,360*u},ImGuiCond_FirstUseEver);bool open=true;
 if(ImGui::Begin("Launch Engine",&open,ImGuiWindowFlags_NoDocking)){
  if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Escape)&&!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)){ImGui::SetKeyOwner(ImGuiKey_Escape,ImGui::GetCurrentWindow()->ID);open=false;}
  runUiTitle(gameProfileName());float available=ImGui::GetContentRegionAvail().x,body=ImGui::GetContentRegionAvail().y-44*u;float side=250*u;
  ImGui::BeginChild("Launch description",{std::max(120*u,available-side-u),body});ImGui::SetCursorPos({20*u,20*u});ImGui::TextUnformatted("Launch Engine");ImGui::SetCursorPos({20*u,60*u});
  ImGui::PushTextWrapPos(ImGui::GetWindowWidth()-20*u);ImGui::TextUnformatted("Select a game engine from the list on the right and edit the commandline parameters in the text box below. You can use variables to refer to the map name and other values.");ImGui::PopTextWrapPos();
  ImGui::SetCursorPosX(20*u);if(ImGui::Button("Configure engines..."))runEngineWindow=true;runUiRecord("launch-configure");
  static std::string selectedId;static int selection=-1;
  auto selectedEngine=std::find_if(runEngines.begin(),runEngines.end(),[&](const auto& e){return e.id==selectedId;});selection=selectedEngine==runEngines.end()?-1:int(selectedEngine-runEngines.begin());
  ImGui::SetCursorPos({20*u,std::max(ImGui::GetCursorPosY()+4*u,body-70*u)});ImGui::TextUnformatted("Parameters");ImGui::SetCursorPosX(20*u);ImGui::SetNextItemWidth(std::max(20*u,ImGui::GetContentRegionAvail().x-20*u));ImGui::BeginDisabled(selection<0);
  if(selection>=0){if(runUiEdit("##Launch parameters",runEngines[selection].parameters,"Parameters passed to the game engine. Variables are allowed.",2))runUiSave();}else{std::string empty;runUiEdit("##Launch parameters",empty);}ImGui::EndDisabled();ImGui::EndChild();ImGui::SameLine(0,u);
  ImGui::BeginChild("Launch engine profiles",{0,body});ImGui::BeginChild("Launch engine list",{0,-24*u});for(int i=0;i<int(runEngines.size());++i)if(runUiListItem("launch-engine/"+std::to_string(i),runEngines[i].name,runEngines[i].path,i==selection)){selection=i;selectedId=runEngines[i].id;if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))try{runLaunchConfiguredEngine(runEngines[i]);}catch(const std::exception& e){runError=e.what();}}
  if(runEngines.empty())ImGui::TextWrapped("Click the 'Configure engines...' button to create a game engine profile.");ImGui::EndChild();
  std::string reorder;if(runUiIcon("launch-engine-up",'^',"Move engine up",selection>0))reorder="up";ImGui::SameLine(0,0);
  if(runUiIcon("launch-engine-down",'v',"Move engine down",selection>=0&&selection+1<int(runEngines.size())))reorder="down";ImGui::SameLine(0,2*u);
  if(ImGui::SmallButton("A-Z##launch-engines"))reorder="sort";runUiRecord("launch-engine-sort");runUiHint("Sort engines by name; keeps the selected engine");
  if(!reorder.empty()&&ftebroom::reorderProfiles(runEngines,selection,reorder))runUiSave();ImGui::EndChild();
  ImGui::SetCursorPosX(std::max(8*u,available-174*u));ImGui::BeginDisabled(selection<0);bool launch=ImGui::Button("Launch",{80*u,24*u});runUiRecord("engine-launch");if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Enter)&&ImGui::GetCurrentContext()->NavId!=ImGui::GetID("Close"))launch=true;
  if(launch&&selection>=0)try{runUiSave();runLaunchConfiguredEngine(runEngines[selection]);}catch(const std::exception& e){runError=e.what();}ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Close",{80*u,24*u}))open=false;runUiRecord("launch-close");
 }
 ImGui::End();if(!open){runLaunchWindow=false;runUiSave();}
}
static void runUiConfirmClose(){
 static int reason=0;if(runUiCloseRequest){reason=runUiCloseRequest;runUiCloseRequest=0;ImGui::OpenPopup("Stop compilation?");}
 if(ImGui::BeginPopupModal("Stop compilation?",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDocking)){
  ImGui::TextUnformatted((reason==2||(reason==3&&documentIsExit(runUiPendingDocument)))?"Closing the editor will stop the running compilation. Are you sure?":reason==3?"Changing the map will stop the running compilation. Are you sure?":"Closing this dialog will stop the running compilation. Are you sure?");
  if(ImGui::Button("Yes",{80*uiScale(),0})){if(runActive)runStop();runWindow=false;runUiSave();ImGui::CloseCurrentPopup();if(reason==2)documentRequest("exit");else if(reason==3){auto next=std::move(runUiPendingDocument);runUiPendingDocument.clear();documentRequest(next);}}ImGui::SetItemDefaultFocus();runUiRecord("close-confirm-yes");
  ImGui::SameLine();if(ImGui::Button("No",{80*uiScale(),0})||(!ImGui::IsWindowAppearing()&&ImGui::IsKeyPressed(ImGuiKey_Escape)))ImGui::CloseCurrentPopup();runUiRecord("close-confirm-no");ImGui::EndPopup();
 }
}
static void runUiWriteLive(){
 if(runUiLive){static double last=0;if(ImGui::GetTime()-last>.1){last=ImGui::GetTime();auto path=documentPath("compilation-ui-live.json");auto temp=path;temp+=".tmp";nlohmann::json views=nlohmann::json::array(),colors=nlohmann::json::object();for(int i=0;i<viewLayout;i++){auto& v=viewRects[i];views.push_back({{"x",v.x},{"y",v.y},{"width",v.w},{"height",v.h},{"type",v.type}});}for(const char* name:{"ftebroom_color_brush_edges","ftebroom_color_2d_background","ftebroom_color_3d_background","ftebroom_grid_color","ftebroom_clear_sky","ftebroom_color_selected","ftebroom_color_locked"}){char text[128]{};cvars->GetString(name,text,sizeof text);colors[name]=text;}std::ofstream(temp)<<nlohmann::json{{"views",views},{"viewportColors",colors},{"log",runLog},{"widgets",runUiRects},{"globalWidgets",runUiGlobalRects},{"display",{ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y}},{"running",runActive},{"compileOpen",runWindow},{"launchOpen",runLaunchWindow},{"engineOpen",runEngineWindow},{"selected",runSelected},{"game",runStoreGame},{"document",documentFile.u8string()},{"error",runError},{"inputTrace",runUiInputTrace}}.dump();
#ifdef _WIN32
  MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);
#else
  std::filesystem::rename(temp,path);
#endif
 }}
 }
static void runImGuiPanel(){
 if(runUiBrowseOpen&&!runUiLive&&!runWindow&&!runSettingsWindow&&!runLaunchWindow&&!runEngineWindow&&!runUiCloseRequest){runUiBrowsePanel();return;}
 if(!runUiLive&&!runWindow&&!runSettingsWindow&&!runLaunchWindow&&!runEngineWindow&&!runUiCloseRequest&&!runUiBrowseOpen&&!ImGui::IsPopupOpen("Stop compilation?"))return;runLoad();
 if(runUiAtlasWidth&&runUiAtlasTheme>=0){uiTheme=runUiAtlasTheme;uiStyle();}
 if(!runWindow){runUiOrigin={0,0};runUiRects=nlohmann::json::object();runUiRects["window"]={{"x",0},{"y",0},{"width",ImGui::GetIO().DisplaySize.x},{"height",ImGui::GetIO().DisplaySize.y}};}
 RunUiStyle style;if(runWindow)runUiCompile();runUiSettings();runUiEngines();runUiLaunch();runUiBrowsePanel();runUiConfirmClose();
}
