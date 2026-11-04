// SPDX-License-Identifier: GPL-2.0-or-later
// Optional namespaced MIT assets. Downloading never replaces game VM binaries.
static bool shaderPackOpen=false,shaderPackLoaded=false;
static std::string shaderPackError;
#ifdef _WIN32
static HANDLE shaderPackProcess=nullptr;
#else
static pid_t shaderPackProcess=-1;
#endif
struct ShaderPackSetting {const char* key;const char* label;float value,low,high;};
static ShaderPackSetting shaderPackSettings[]={
 {"ryn_time_hour","Time of day",12,0,24},{"ryn_day_animate","Animate day/night",0,0,1},{"ryn_day_minutes","Day duration (minutes)",20,1,120},
 {"ryn_latitude","Latitude",0,-90,90},{"ryn_cloud_cover","Cloud coverage",.5,0,1},{"ryn_cloud_density","Cloud density",1,0,3},
 {"ryn_cloud_base","Cloud base (km)",1.2,.3,5},{"ryn_cloud_depth","Cloud thickness (km)",1.8,.2,4},{"ryn_cloud_scale","Cloud scale",1.5,.4,4},
 {"ryn_cloud_steps","Cloud samples",40,24,96},{"ryn_cloud_wind","Wind (km/h)",18,0,120},{"ryn_cloud_heading","Wind direction",35,0,360},
 {"ryn_cloud_type","Cloud type",.8,0,1},{"ryn_cloud_cirrus","Cirrus",.25,0,1},{"ryn_cloud_erosion","Cloud erosion",.65,0,1},
 {"ryn_cloud_region_heading","Cloud region direction",0,0,360},{"ryn_cloud_region_width","Cloud region width",360,0,360},
 {"ryn_cloud_region_feather","Cloud boundary softness",15,1,60},{"ryn_cloud_region_min","Cloud minimum elevation",0,0,89},{"ryn_cloud_region_max","Cloud maximum elevation",90,1,90},
 {"ryn_aurora_strength","Polar aurora",1,0,3},{"ryn_star_brightness","Stars",1,0,3},{"ryn_sky_exposure","Sky exposure",1,.2,3},
 {"ryn_sunrise","Clock sunrise anchor",6,0,11.5},{"ryn_sunset","Clock sunset anchor",18,12,23.5},
 {"ryn_weather_mode","Weather: 0 clear / 1 cloudy / 2 rain or snow",0,0,2},{"ryn_weather_transition","Weather transition (seconds)",8,1,60},
 {"ryn_weather_auto","Automatic weather",0,0,1},{"ryn_weather_interval","Weather interval (seconds)",120,15,900},
 {"ryn_season","Season: 0 spring / 1 summer / 2 autumn / 3 winter",1,0,3},{"ryn_season_auto","Automatic seasons",0,0,1},{"ryn_year_minutes","Year duration (minutes)",20,1,120},
 {"ryn_moon_phase","Moon phase: 0 new / 0.5 full",.5,0,1},{"ryn_sun_radius","Sun angular radius",.266,.15,2},{"ryn_moon_radius","Moon angular radius",.272,.15,2},
 {"ryn_lens_strength","Lens effects",.6,0,2},{"ryn_ray_strength","Optical rays",.7,0,2},
 {"rsp_wave_height","Wave normal amplitude",4,0,24},{"rsp_wave_length","Wave length",180,8,800},{"rsp_wave_speed","Wave speed",1,0,4},
 {"rsp_water_alpha","Water opacity",.8,.1,1},{"rsp_water_level","Water level for ice",0,-8192,8192},
 {"rsp_fog","Atmospheric fog",0,0,1},{"rsp_fog_density","Fog density",.0005,0,.01},{"rsp_effect_strength","Material fog strength",1,0,3}
};
static std::filesystem::path shaderPackRoot(){return settingsPath().parent_path()/"downloads/reyond-shaders-1.0.0";}
static bool shaderPackBusy(){
#ifdef _WIN32
 return shaderPackProcess!=nullptr;
#else
 return shaderPackProcess>0;
#endif
}
static bool shaderPackReady(){return std::filesystem::is_regular_file(shaderPackRoot()/"ready.txt");}
static void shaderPackLoad(){
 if(shaderPackLoaded)return;shaderPackLoaded=true;
 for(auto& p:shaderPackSettings)cvars->SetFloat(p.key,p.value);
 std::ifstream in(settingsPath().parent_path()/"reyond-shaders-settings.txt");std::string key;float value;
 while(in>>key>>value)for(auto& p:shaderPackSettings)if(key==p.key&&std::isfinite(value))cvars->SetFloat(p.key,std::clamp(value,p.low,p.high));
}
static void shaderPackSave(){
 std::ofstream out(settingsPath().parent_path()/"reyond-shaders-settings.txt");
 for(auto& p:shaderPackSettings)out<<p.key<<' '<<cvars->GetFloat(p.key)<<'\n';
 if(!out)throw std::runtime_error("Cannot save shader settings.");
}
static bool shaderPackResource(const std::string& name){
 return name.rfind("glsl/reyondpack/",0)==0||name.rfind("textures/reyondpack/",0)==0;
}
static void shaderPackCopy(const std::filesystem::path& destination,bool exporting){
 std::ifstream list(shaderPackRoot()/"files.txt");std::string name;bool any=false;
 while(std::getline(list,name)){
  if(!name.empty()&&name.back()=='\r')name.pop_back();
  if(!ftebroom::pak::zipName(name))throw std::runtime_error("Invalid shader pack path.");
  if(!shaderPackResource(name)&&name!="scripts/reyond-pack.shader"&&!(exporting&&(name=="LICENSE.txt"||name=="README.md"||name=="PROVENANCE.md"||name=="sdk/reyond-pack.qc")))continue;
  auto target=destination/std::filesystem::u8path(name);
  if(name=="scripts/reyond-pack.shader"&&!exporting)target=destination/"scripts/reyond-pack-active.shader";
  if(exporting&&(name=="LICENSE.txt"||name=="README.md"||name=="PROVENANCE.md"))target=destination/"reyond-shader-pack"/name;
  auto source=shaderPackRoot()/"content"/name;
  // Ship portal-coordinate fixes for already-downloaded 1.0.0 packs as well.
  if(name=="glsl/reyondpack/water.glsl"&&std::filesystem::is_regular_file(documentPath("glsl/ftebroom_pack_water.glsl")))source=documentPath("glsl/ftebroom_pack_water.glsl");
  if(name=="glsl/reyondpack/waves.glsl"&&std::filesystem::is_regular_file(documentPath("glsl/ftebroom_pack_waves.glsl")))source=documentPath("glsl/ftebroom_pack_waves.glsl");
  std::filesystem::create_directories(target.parent_path());if(source!=target)std::filesystem::copy_file(source,target,std::filesystem::copy_options::overwrite_existing);any=true;
 }
 if(!any)throw std::runtime_error("The shader pack is incomplete. Download it again.");
 if(exporting){std::ofstream config(destination/"reyond-pack.cfg");for(auto& p:shaderPackSettings)config<<"set "<<p.key<<' '<<cvars->GetFloat(p.key)<<'\n';
  for(const char* key:{"ryn_declination","ryn_weather_cloud","ryn_weather_rain","ryn_snow_cover","ryn_ice_amount"})config<<"set "<<key<<' '<<cvars->GetFloat(key)<<'\n';
  if(!config)throw std::runtime_error("Cannot export shader settings.");}
}
static void shaderPackActivate(bool reload){
 if(gameProfile!=1||!shaderPackReady())return;
 cvars->SetFloat("ftebroom_game_profile",gameProfile);
 shaderPackLoad();shaderPackCopy(documentPath("scripts/reyond-pack-active.shader").parent_path().parent_path(),false);
 cvars->SetFloat("ftebroom_shader_pack_enabled",1);
 if(reload)command("fs_restart\nftebroom_assets_ready\n");
}
static void shaderPackStage(const std::filesystem::path& directory){
 if(gameProfile!=1||!cvars->GetFloat("ftebroom_shader_pack_enabled")||!shaderPackReady())return;
 shaderPackCopy(directory,true);
 size_t length=0;auto bytes=static_cast<char*>(files->LoadFile("scripts/shaderlist.txt",&length));
 std::string list=bytes?std::string(bytes,length):std::string();if(bytes)core->Free(bytes);
 if(list.find("reyond-pack")==list.npos)list+="\nreyond-pack\n";
 std::ofstream(directory/"scripts/shaderlist.txt")<<list;
}
static void shaderPackPrepareProfile(){
 cvars->SetFloat("ftebroom_shader_pack_enabled",0);
 if(gameProfile==1&&shaderPackReady())shaderPackActivate(false);
 else {auto script=documentPath("scripts/reyond-pack-active.shader");if(std::filesystem::exists(script))std::ofstream(script)<<"// Optional FTE shader pack inactive in this profile.\n";}
}
static void shaderPackStart(){
 if(gameProfile!=1||shaderPackBusy())return;shaderPackError.clear();
 try{
  auto root=shaderPackRoot();std::filesystem::create_directories(root);
  std::ofstream(root/"progress.txt")<<"0 0 Connecting";
#ifdef _WIN32
  auto script=documentPath("download-shader-pack.ps1"),manifest=documentPath("reyond-shader-pack.json");
  std::wstring args=L"powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File \""+script.wstring()+L"\" -Destination \""+root.wstring()+L"\" -Manifest \""+manifest.wstring()+L"\"";
  STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
  if(!CreateProcessW(nullptr,args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))throw std::runtime_error("Cannot start shader download.");
  CloseHandle(pi.hThread);shaderPackProcess=pi.hProcess;
#else
  std::vector<std::string> args={"python3",documentPath("download-shader-pack.py").u8string(),"--destination",root.u8string(),"--manifest",documentPath("reyond-shader-pack.json").u8string()};
  std::vector<char*> argv;for(auto& s:args)argv.push_back(s.data());argv.push_back(nullptr);
  int status=posix_spawnp(&shaderPackProcess,"python3",nullptr,nullptr,argv.data(),environ);
  if(status){shaderPackProcess=-1;throw std::runtime_error("Cannot start downloader. Python 3 is required on Linux.");}
#endif
 }catch(const std::exception& e){shaderPackError=e.what();}
}
static void shaderPackTick(){
 bool done=false,failed=false;
#ifdef _WIN32
 if(shaderPackProcess){DWORD code=STILL_ACTIVE;if(GetExitCodeProcess(shaderPackProcess,&code)&&code!=STILL_ACTIVE){CloseHandle(shaderPackProcess);shaderPackProcess=nullptr;done=true;failed=code!=0;}}
#else
 if(shaderPackProcess>0){int code=0;auto result=waitpid(shaderPackProcess,&code,WNOHANG);if(result>0||(result<0&&errno!=EINTR)){shaderPackProcess=-1;done=true;failed=result<0||!WIFEXITED(code)||WEXITSTATUS(code)!=0;}}
#endif
 if(done){try{if(failed||!shaderPackReady()){std::ifstream in(shaderPackRoot()/"error.txt");std::getline(in,shaderPackError);if(shaderPackError.empty())shaderPackError="Shader download failed. Retry when connected.";}else shaderPackActivate(true);}catch(const std::exception& e){shaderPackError=e.what();}}
}
static void shaderPackMenu(){
 if(gameProfile==1&&ImGui::BeginMenu("Assets")){if(ImGui::BeginMenu("Shaders")){if(ImGui::MenuItem("Reyond's GLSL Shader Pack..."))shaderPackOpen=true;ImGui::EndMenu();}ImGui::EndMenu();}
}
static void shaderPackPanel(){
 if(!shaderPackOpen)return;
 ImGui::SetNextWindowSize({650*uiScale(),670*uiScale()},ImGuiCond_FirstUseEver);
 if(ImGui::Begin("Reyond's GLSL Shader Pack",&shaderPackOpen)){
  if(gameProfile!=1){ImGui::TextWrapped("This pack is available only in the FTE Custom game profile.");ImGui::End();return;}
  ImGui::TextWrapped("Original shaders under MIT, including commercial use with the license notice. No restricted reference shaders are included.");
  if(!shaderPackError.empty())ImGui::TextWrapped("%s",shaderPackError.c_str());
  if(shaderPackBusy()){
   uint64_t done=0,total=0;std::ifstream progress(shaderPackRoot()/"progress.txt");progress>>done>>total;
   ImGui::ProgressBar(total?std::clamp(float(done)/total,0.f,1.f):-float(ImGui::GetTime()),{-1,0},"Downloading / verifying shader pack...");
  }else if(!shaderPackReady()){if(ImGui::Button("Download Reyond's GLSL Shader Pack"))shaderPackStart();}
  else {
   shaderPackLoad();ImGui::TextUnformatted("Installed: 1.0.0 | Material collection: textures/reyondpack");
   if(ImGui::Button("Open sample island")){shaderPackActivate(false);documentRequest("reyond-island-sample");shaderPackOpen=false;}
   ImGui::TextWrapped("A small island surrounded by ocean and a complete sky. Change time, clouds, weather and seasons here to explore the pack.");
   if(!shaderPackError.empty()&&ImGui::Button("Download again"))shaderPackStart();
   if(!cvars->GetFloat("ftebroom_shader_pack_enabled")){if(ImGui::Button("Enable shader pack"))try{shaderPackActivate(true);}catch(const std::exception& e){shaderPackError=e.what();}}
   ImGui::TextWrapped("Water, sky, puddles, fog, fire, explosion, ambient GI, snow and ice can be applied in Materials. Snow/ice are visual; collision and baked lighting are unchanged.");
   if(ImGui::Button("Save settings"))try{shaderPackSave();}catch(const std::exception& e){shaderPackError=e.what();}
   ImGui::SameLine();if(ImGui::Button("Export pack to mod"))try{auto mod=std::filesystem::u8path(gameMod);if(mod.empty()||!std::filesystem::is_directory(mod))throw std::runtime_error("Set an existing runtime mod directory in the FTE Custom game profile first.");shaderPackCopy(mod,true);}catch(const std::exception& e){shaderPackError=e.what();}
   ImGui::TextWrapped("Export copies namespaced materials and an optional QC SDK. It never replaces progs.dat or csprogs.dat. Integrate the SDK to use precipitation and optics in your game.");
   for(auto& p:shaderPackSettings){float value=cvars->GetFloat(p.key);bool changed=false;
    const bool boolean=!std::strcmp(p.key,"ryn_day_animate")||!std::strcmp(p.key,"ryn_weather_auto")||!std::strcmp(p.key,"ryn_season_auto")||!std::strcmp(p.key,"rsp_fog");
    if(boolean){bool checked=value>.5f;changed=ImGui::Checkbox(p.label,&checked);value=checked?1.f:0.f;}
    else if(!std::strcmp(p.key,"ryn_season")){int chosen=int(value+.5f);changed=ImGui::Combo("Season",&chosen,"Spring\0Summer\0Autumn\0Winter\0");value=float(chosen);}
    else if(!std::strcmp(p.key,"ryn_weather_mode")){int chosen=int(value+.5f);changed=ImGui::Combo("Weather",&chosen,"Clear\0Cloudy\0Rain / winter snow\0");value=float(chosen);}
    else changed=ImGui::SliderFloat(p.label,&value,p.low,p.high,p.high<=1?"%.2f":p.high>100?"%.0f":"%.2f");
    if(changed){
    if(!std::strcmp(p.key,"ryn_cloud_region_min"))value=std::min(value,cvars->GetFloat("ryn_cloud_region_max")-1);
    if(!std::strcmp(p.key,"ryn_cloud_region_max"))value=std::max(value,cvars->GetFloat("ryn_cloud_region_min")+1);
    if(!std::strcmp(p.key,"ryn_weather_mode")||!std::strcmp(p.key,"ryn_season"))value=std::round(value);
    if(!std::strcmp(p.key,"ryn_season")){cvars->SetFloat("ryn_season_auto",0);cvars->SetFloat("ryn_weather_mode",value>=2?2:value==1?0:1);}
    if(!std::strcmp(p.key,"ryn_time_hour"))cvars->SetFloat("ryn_day_animate",0);
    if(!std::strcmp(p.key,"ryn_weather_mode"))cvars->SetFloat("ryn_weather_auto",0);
    cvars->SetFloat(p.key,value);
   }}
  }
 }ImGui::End();
}
static void shaderPackCommand(){
 if(commands->IsInsecure())return;
 char op[64]{};commands->Argv(1,op,sizeof op);
 try{
  if(gameProfile!=1)throw std::runtime_error("Shader pack is available only in FTE Custom game.");
  if(!std::strcmp(op,"download"))shaderPackStart();
  else if(!std::strcmp(op,"activate"))shaderPackActivate(true);
  else if(!std::strcmp(op,"save"))shaderPackSave();
  else if(!std::strcmp(op,"sample")){if(!shaderPackReady())throw std::runtime_error("Download the shader pack first.");shaderPackActivate(false);documentRequest("reyond-island-sample");}
  else if(!std::strcmp(op,"status")){
   unsigned count=0;for(auto& pair:materials)if(pair.first.rfind("textures/reyondpack/",0)==0)++count;
   core->Print(("REYOND_SHADER_PACK ready="+std::to_string(shaderPackReady())+" enabled="+std::to_string(cvars->GetFloat("ftebroom_shader_pack_enabled"))+" materials="+std::to_string(count)+"\n").c_str());
  }else shaderPackOpen=true;
 }catch(const std::exception& e){shaderPackError=e.what();core->Print((shaderPackError+"\n").c_str());}
}
