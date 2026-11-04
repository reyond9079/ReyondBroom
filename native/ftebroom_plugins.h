// SPDX-License-Identifier: GPL-2.0-or-later
#include "../sdk/reyondbroom_plugin.h"
#ifndef _WIN32
#include <dlfcn.h>
#endif
struct EditorPlugin {
  std::filesystem::path folder,library;std::string id,name,description,error;
  void* module=nullptr;const RBPluginV1* api=nullptr;bool open=false,enabled=false;
};
static std::vector<EditorPlugin> editorPlugins;
static bool pluginManagerOpen=false,pluginsScanned=false;
static std::string pluginManagerError;
static std::map<std::string,ImVec2> pluginButtonPositions;
static double pluginNumber(const char* name){if(name&&!std::strcmp(name,"rb.profile"))return gameProfile;if(name&&!std::strcmp(name,"rb.document_generation"))return organizationMapEpoch;return name?cvars->GetFloat(name):0;}
static int pluginString(const char* name,char* out,size_t size){if(!name||!out||!size)return 0;out[0]=0;if(!std::strcmp(name,"rb.model_class")){try{return std::snprintf(out,size,"%s",modelDropClass(std::filesystem::path("ivy.obj")));}catch(...){return 0;}}return cvars->GetString(name,out,int(std::min<size_t>(size,1048576)));}
static void pluginSetNumber(const char* name,double value){if(name&&std::isfinite(value))cvars->SetFloat(name,float(value));}
static void pluginSetString(const char* name,const char* value){if(name&&value)cvars->SetString(name,value);}
static void pluginCommand(const char* value){if(value&&std::strlen(value)<=65536)command(value);}
static void pluginLog(const char* value){if(value)core->Print((std::string("Plugin: ")+value+"\n").c_str());}
static int pluginImportMap(const char* text,size_t length){
  if(!text||!length||length>16*1024*1024||modEmbedded||documentTerrain)return 0;
  std::string bytes(text,length);qhandle_t f;if(files->Open("data/import-in.map",&f,2)<0)return 0;int done=files->Write(f,bytes.data(),int(length));files->Close(f);if(done!=int(length))return 0;
  command("ftebroom_import_file 1 0 0\n");return 1;
}
static bool pluginPhysicsOwned=false;
static int pluginSettle(int operation,const RBSettleOptionsV1* options){
  if(operation==0){
    if(!options||settleWaiting||settleJob.valid()||modEmbedded)return -1;
    const double values[]={options->friction,options->bounce,options->impulse,options->seconds};for(double v:values)if(!std::isfinite(v))return -1;
    if(options->friction<0||options->friction>1||options->bounce<0||options->bounce>1||options->impulse<0||options->impulse>256||options->seconds<.1||options->seconds>20)return -1;
    settleOptions.friction=options->friction;settleOptions.restitution=options->bounce;settleOptions.kick=options->impulse;settleOptions.duration=options->seconds;settleOptions.seed=options->seed;
    cvars->SetString("ftebroom_settle_error","");cvars->SetString("ftebroom_plugin_physics_error","");cvars->SetFloat("ftebroom_settle_applied",0);pluginPhysicsOwned=true;settleStart();return 0;
  }
  if(!pluginPhysicsOwned)return -1;
  if(operation==3){settleCancel=true;settleWaiting=false;command("ftebroom_settle cancel\n");pluginPhysicsOwned=false;return 1;}
  settleTick();cvars->SetFloat("ftebroom_plugin_progress",settleProgress.load());char error[2048]={};cvars->GetString("ftebroom_settle_error",error,sizeof error);
  if(!settleError.empty()||*error){cvars->SetString("ftebroom_plugin_physics_error",settleError.empty()?error:settleError.c_str());pluginPhysicsOwned=false;return -1;}
  if(operation==1){cvars->SetFloat("ftebroom_settle_moving",settleMoving.load());return cvars->GetFloat("ftebroom_settle_valid")?1:0;}
  if(operation==2&&cvars->GetFloat("ftebroom_settle_valid")){command(cvars->GetFloat("ftebroom_plugin_group_after_settle")?"ftebroom_settle apply_grouped\n":"ftebroom_settle apply\n");pluginPhysicsOwned=false;return 1;}
  return -1;
}
static void pluginText(const char* text){ImGui::TextWrapped("%s",text?text:"");}
static void pluginSeparator(const char* text){ImGui::SeparatorText(text?text:"");}
static int pluginInt(const char* label,int* value){return label&&value&&ImGui::InputInt(label,value);}
static int pluginFloat(const char* label,float* value){return label&&value&&ImGui::InputFloat(label,value);}
static int pluginBool(const char* label,int* value){if(!label||!value)return 0;bool b=*value;bool changed=ImGui::Checkbox(label,&b);*value=b;return changed;}
static int pluginCombo(const char* label,int* value,const char* items){return label&&value&&items&&ImGui::Combo(label,value,items);}
static int pluginButton(const char* label){if(!label)return 0;bool clicked=ImGui::Button(label);if(cvars->GetFloat("ftebroom_skip_setup")){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();pluginButtonPositions[label]={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}return clicked;}
static void pluginSameLine(){ImGui::SameLine();}
static void pluginDisabled(int begin){if(begin)ImGui::BeginDisabled();else ImGui::EndDisabled();}
static void pluginProgress(float value,const char* label){ImGui::ProgressBar(std::clamp(value,0.f,1.f),{-1,0},label);}
static int pluginReadData(const char* path,char* out,size_t capacity){
  if(!path||!ftebroom::pak::zipName(path)||std::string(path).rfind("plugin-",0)!=0)return -1;
  try{auto p=documentPath(std::string("data/")+path);auto size=std::filesystem::file_size(p);if(size>16*1024*1024)return -1;if(!out)return int(size);
    if(capacity<=size)return -1;std::ifstream f(p,std::ios::binary);f.read(out,size);out[size]=0;return f?int(size):-1;
  }catch(...){return -1;}
}
static int pluginWriteAsset(const char* path,const char* bytes,size_t length){
  if(!path||!bytes||!length||length>16*1024*1024||!ftebroom::pak::zipName(path))return 0;
  std::string relative=path;auto ext=std::filesystem::path(relative).extension().string();
  if(relative.rfind("models/reyond-plugins/",0)!=0&&relative.rfind("textures/reyond-plugins/",0)!=0)return 0;
  if(ext!=".obj"&&ext!=".mtl"&&ext!=".tga")return 0;
  try{auto target=documentPath(relative);std::filesystem::create_directories(target.parent_path());
    if(std::filesystem::exists(target)){std::ifstream old(target,std::ios::binary);std::string data((std::istreambuf_iterator<char>(old)),{});if(data!=std::string(bytes,length))return 0;}
    else{std::ofstream out(target,std::ios::binary);out.write(bytes,length);out.close();if(!out)return 0;}
    // Copy generated resources beside the mod when one is configured.
    auto root=gameProfile==1?std::filesystem::u8path(gameMod):gameProfile==3?std::filesystem::u8path(q2Mod):gameWad()?std::filesystem::u8path(q1Mod):std::filesystem::u8path(baseq3);
    std::vector<std::filesystem::path> roots{root};if(gameProfile==1)roots.push_back(std::filesystem::u8path(baseq3));
    for(const auto& folder:roots)if(!folder.empty()&&std::filesystem::is_directory(folder)){auto destination=folder/std::filesystem::u8path(relative);std::filesystem::create_directories(destination.parent_path());if(!std::filesystem::exists(destination))std::filesystem::copy_file(target,destination);}
    return 1;
  }catch(const std::exception& e){pluginManagerError=e.what();return 0;}
}
static int pluginInputText(const char* label,char* value,size_t size){return label&&value&&size&&ImGui::InputText(label,value,size);}
static std::vector<float> pluginPreviewLines;
static void pluginPreview(const float* xyz,size_t segments){
  pluginPreviewLines.clear();if(!xyz||segments>16384)return;
  for(size_t i=0;i<segments*6;i++)if(!std::isfinite(xyz[i])||std::abs(xyz[i])>131072)return;
  pluginPreviewLines.assign(xyz,xyz+segments*6);
}
static void pluginPreviewDraw(){
  if(pluginPreviewLines.empty()||modEmbedded||cvars->GetFloat("ftebroom_bsp_preview"))return;
  char state[512]{};cvars->GetString("ftebroom_camera_state",state,sizeof state);float v[7];if(!cameraParse(state,v))return;
  const float pitch=v[3]*.01745329252f,yaw=v[4]*.01745329252f;
  const std::array<float,3> forward{std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),-std::sin(pitch)},right{std::sin(yaw),-std::cos(yaw),0},up{std::sin(pitch)*std::cos(yaw),std::sin(pitch)*std::sin(yaw),std::cos(pitch)};
  auto rect=viewRects[viewFocus];if(rect.type)return;auto draw=ImGui::GetForegroundDrawList();draw->PushClipRect({rect.x,rect.y},{rect.x+rect.w,rect.y+rect.h},true);
  auto project=[&](const float* p,ImVec2& pixel){float depth=0,x=0,y=0;for(int a=0;a<3;a++){float delta=p[a]-v[a];depth+=delta*forward[a];x+=delta*right[a];y+=delta*up[a];}if(depth<1)return false;float scale=rect.w*.5f/std::tan(v[6]*.00872664626f)/depth;pixel={rect.x+rect.w*.5f+x*scale,rect.y+rect.h*.5f-y*scale};return true;};
  for(size_t i=0;i<pluginPreviewLines.size();i+=6){ImVec2 a,b;if(project(&pluginPreviewLines[i],a)&&project(&pluginPreviewLines[i+3],b))draw->AddLine(a,b,IM_COL32(99,232,143,220),1.5f);}
  draw->PopClipRect();
}
static int pluginSection(const char* label,int open){return ImGui::CollapsingHeader(label,open?ImGuiTreeNodeFlags_DefaultOpen:0);}
static const RBHostV1 pluginHost{RB_PLUGIN_API_VERSION,sizeof(RBHostV1),pluginNumber,pluginString,pluginSetNumber,pluginSetString,pluginCommand,pluginLog,pluginImportMap,pluginSettle,pluginText,pluginSeparator,pluginInt,pluginFloat,pluginBool,pluginCombo,pluginButton,pluginSameLine,pluginDisabled,pluginProgress,pluginReadData,pluginWriteAsset,pluginInputText,pluginPreview,pluginSection};
static void pluginUnload(EditorPlugin& p){
  pluginPreviewLines.clear();
  if(p.api&&p.api->shutdown)try{p.api->shutdown();}catch(...){p.error="Plugin shutdown failed.";}
  p.api=nullptr;
#ifdef _WIN32
  if(p.module)FreeLibrary((HMODULE)p.module);
#else
  if(p.module)dlclose(p.module);
#endif
  p.module=nullptr;p.enabled=false;p.open=false;
}
static bool pluginBusy(const EditorPlugin& p){try{return p.api&&p.api->busy&&p.api->busy();}catch(...){return true;}}
static void pluginLoad(EditorPlugin& p){
  try{
#ifdef _WIN32
    p.module=(void*)LoadLibraryExW(p.library.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    auto init=p.module?(const RBPluginV1*(*)(const RBHostV1*))GetProcAddress((HMODULE)p.module,"ReyondBroomPluginInit"):nullptr;
#else
    p.module=dlopen(p.library.c_str(),RTLD_NOW|RTLD_LOCAL);
    auto init=p.module?(const RBPluginV1*(*)(const RBHostV1*))dlsym(p.module,"ReyondBroomPluginInit"):nullptr;
#endif
    if(!init)throw std::runtime_error("Cannot load plugin or find ReyondBroomPluginInit. Check its architecture and dependencies.");
    auto api=init(&pluginHost);
    if(!api||api->api_version!=RB_PLUGIN_API_VERSION||api->struct_size<sizeof(RBPluginV1)||!api->id||p.id!=api->id||!api->draw)throw std::runtime_error("Plugin API/version/id mismatch.");
    p.api=api;p.enabled=true;p.error.clear();
  }catch(const std::exception& e){pluginUnload(p);p.error=e.what();}catch(...){pluginUnload(p);p.error="Plugin initialization failed.";}
}
static void pluginsSave(){std::ofstream out(settingsPath().parent_path()/"ftebroom-plugins.cfg");for(auto& p:editorPlugins)out<<std::quoted(p.id)<<' '<<p.enabled<<'\n';}
static void pluginsScan(){
  for(auto& p:editorPlugins)if(pluginBusy(p)){pluginManagerError="Wait for running plugins before rescanning.";return;}
  for(auto& p:editorPlugins)pluginUnload(p);editorPlugins.clear();pluginsScanned=true;pluginManagerError.clear();
  try{
    auto root=settingsPath().parent_path()/"plugins";std::filesystem::create_directories(root);
    std::map<std::string,int> enabled;std::ifstream prefs(settingsPath().parent_path()/"ftebroom-plugins.cfg");std::string id;int on;
    while(prefs>>std::quoted(id)>>on)enabled[id]=on;
    for(auto& entry:std::filesystem::directory_iterator(root)){
      if(editorPlugins.size()>=128)break;if(!entry.is_directory()||entry.is_symlink())continue;
      auto manifest=entry.path()/"plugin.ini";if(!std::filesystem::is_regular_file(manifest)||std::filesystem::is_symlink(manifest)||std::filesystem::file_size(manifest)>16384)continue;
      EditorPlugin p;p.folder=entry.path();std::ifstream in(manifest);std::string key,value,library;
      while(in>>key>>std::quoted(value)){
        if(key=="id")p.id=value;else if(key=="name")p.name=value;else if(key=="description")p.description=value;
#ifdef _WIN32
        else if(key=="windows")library=value;
#else
        else if(key=="linux")library=value;
#endif
      }
      if(p.id.empty()||p.name.empty()||library.empty()||library.find_first_of("/\\:")!=std::string::npos||library=="."||library=="..")continue;
      bool duplicate=false;for(auto& old:editorPlugins)if(old.id==p.id)duplicate=true;if(duplicate)continue;
      p.library=std::filesystem::absolute(p.folder/std::filesystem::u8path(library));
      if(!std::filesystem::is_regular_file(p.library)||std::filesystem::is_symlink(p.library)){p.error="Plugin binary is missing.";}
      else if(enabled.count(p.id)?enabled[p.id]!=0:((p.id=="org.reyondbroom.rubble"&&entry.path().filename()=="rubble")||(p.id=="org.reyondbroom.levelgen"&&entry.path().filename()=="levelgen")||(p.id=="org.reyondbroom.ivy"&&entry.path().filename()=="ivy")))pluginLoad(p);
      editorPlugins.push_back(std::move(p));
    }
  }catch(const std::exception& e){pluginManagerError=e.what();}
}
static void pluginsTick(){
  static int previewDocument=organizationMapEpoch;if(previewDocument!=organizationMapEpoch){pluginPreviewLines.clear();previewDocument=organizationMapEpoch;}
  if(!pluginsScanned)pluginsScan();
  for(auto& p:editorPlugins)if(p.api&&p.api->tick)try{p.api->tick();}catch(...){p.error="Plugin tick failed; disabled.";pluginUnload(p);}
  cvars->SetFloat("ftebroom_plugins_loaded",std::count_if(editorPlugins.begin(),editorPlugins.end(),[](auto& p){return p.api!=nullptr;}));
}
static void pluginsMenu(){
  if(ImGui::BeginMenu("Plugins")){
    if(ImGui::MenuItem("Manage plugins..."))pluginManagerOpen=true;
    for(auto& p:editorPlugins)if(p.api)if(ImGui::MenuItem(p.name.c_str()))p.open=true;
    ImGui::EndMenu();
  }
}
static void pluginsPanel(){
  if(pluginManagerOpen){bool visible=ImGui::Begin("Plugins",&pluginManagerOpen);if(visible){
    ImGui::TextWrapped("Install plugin folders into: %s",(settingsPath().parent_path()/"plugins").u8string().c_str());
    ImGui::TextWrapped("Native plugins run with the editor's permissions. Enable only plugins you trust; mod/PAK resources never install or load plugins.");
    if(ImGui::Button("Rescan plugins"))pluginsScan();
    if(!pluginManagerError.empty())ImGui::TextWrapped("%s",pluginManagerError.c_str());
    for(auto& p:editorPlugins){ImGui::PushID(p.id.c_str());ImGui::SeparatorText(p.name.c_str());ImGui::TextWrapped("%s",p.description.c_str());
      bool on=p.enabled;ImGui::BeginDisabled(pluginBusy(p));if(ImGui::Checkbox("Enabled",&on)){if(on)pluginLoad(p);else pluginUnload(p);pluginsSave();}ImGui::EndDisabled();
      if(p.api){ImGui::SameLine();if(ImGui::Button("Open"))p.open=true;}if(!p.error.empty())ImGui::TextWrapped("%s",p.error.c_str());ImGui::PopID();}
  }ImGui::End();}
  for(auto& p:editorPlugins)if(p.api&&p.open){ImGui::SetNextWindowSize({480*uiScale(),650*uiScale()},ImGuiCond_FirstUseEver);if(ImGui::Begin(p.name.c_str(),&p.open)){ImGui::PushID(p.id.c_str());try{p.api->draw();}catch(...){p.error="Plugin drawing failed.";}ImGui::PopID();}ImGui::End();}
}
static void pluginsShutdown(){for(auto& p:editorPlugins)pluginUnload(p);editorPlugins.clear();pluginsScanned=false;}
