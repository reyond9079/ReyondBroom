// SPDX-License-Identifier: GPL-2.0-or-later
#include <thread>
#ifndef _WIN32
#include <spawn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
#include "ftebroom_run_profile.h"
#include "ftebroom_run_variables.h"
#include "ftebroom_run_import_data.h"
#include "ftebroom_engine_launch.h"
#include "profile/files.h"
#include "profile/export.h"
static std::vector<RunProfile> runProfiles;
static std::map<std::string,std::string> runVariables;
static RunProfile runningProfile,lastRunProfile;
static std::map<std::string,std::string> activeVariables;
static bool runExclusive=false;
static bool runStrict=false,runTesting=false;
static nlohmann::json activeTypedVariables,activeLaunchVariables,runCameraEntity;
static std::string runActiveGame;
static bool runWindow,runActive,runExportWaiting,runHadError,runHaveLast;
static int runSelected,runStep;
static std::string runLog,runError;
static uint64_t runControlAcks;
static bool (*runReadyHook)()=nullptr;
static void (*runExportHook)(const std::filesystem::path&)=nullptr;
static void (*runStageHook)()=nullptr;
#ifdef _WIN32
static HANDLE runProcess,runPipe,runJob,runInput;
static std::vector<HANDLE> runLaunchedEngines;
#else
static pid_t runProcess=-1;static int runPipe=-1;
static std::vector<pid_t> runLaunchedEngines;
#endif
static void (*runPreviewSettings)(const std::string&)=nullptr;
static void runAppend(const std::string& text)
{
  auto previous=runLog.size();runLog+=text;
  if(runPreviewSettings){
    size_t at=previous>512?previous-512:0;
    while((at=runLog.find("reyond_preview_settings ",at))!=std::string::npos){
      auto end=runLog.find('\n',at);if(end==std::string::npos)break;
      if(end>=previous&&end-at<512)runPreviewSettings(runLog.substr(at+24,end-at-24));at=end+1;
    }
  }
  for(size_t at=previous>32?previous-32:0;(at=runLog.find("ftebroom_camera_ack",at))!=std::string::npos;at+=18)
    if(at+18>previous)++runControlAcks;

}
#include "ftebroom_build_textures.h"
static std::filesystem::path runBundledEngine()
{
  auto root=documentPath("ftebroom.cfg").parent_path().parent_path();
#ifdef _WIN32
  auto engine=root/"bin/fteqw64.exe";
  return std::filesystem::is_regular_file(engine)?engine:root/"fteqw64.exe";
#else
  auto preview=root/"tools/runtimes/fteqw-preview64";
  if(std::filesystem::is_regular_file(preview))return preview;
  return root/"fteqw-gl64";
#endif
}
static void runLoadLegacy()
{
  static bool loaded=false;if(loaded)return;loaded=true;
  if(runProfiles.empty())
  {
    runProfiles.push_back({"Q3: Export + q3map2 -meta","${MAP_DIR_PATH}/_ftebroom-build",false,{{0,"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map",""},{1,"${q3map2}","-game quake3 -fs_basepath \"${GAME_DIR_PATH}/..\" -meta \"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map\""}}});
#ifdef _WIN32
    const char* launchEngine="${APP_DIR_PATH}/fteqw64.exe";
#else
    const char* launchEngine="${APP_DIR_PATH}/fteqw-gl64";
#endif
    runProfiles.push_back({"Launch in FTE","${APP_DIR_PATH}",true,{{1,launchEngine,"-basedir \"${GAME_DIR_PATH}/..\" -game baseq3 +map ${MAP_BASE_NAME}"}}});
    runVariables["q3map2"]="";runVariables["bspc"]="";
  }
  auto preset=[&](RunProfile profile){if(std::none_of(runProfiles.begin(),runProfiles.end(),[&](const RunProfile& p){return p.name==profile.name;}))runProfiles.push_back(std::move(profile));};
  const std::string map="\"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map\"",bsp="\"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\"";
  const std::string q3="-game quake3 -fs_basepath \"${GAME_DIR_PATH}/..\" -fs_pakpath \"${WORK_DIR_PATH}\" ";
  preset({"Q3: BSP + fast VIS + LIGHT","${MAP_DIR_PATH}/_ftebroom-build",false,{
    {0,"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map",""},{1,"${q3map2}",q3+"-meta "+map},
    {1,"${q3map2}",q3+"-vis -fast "+bsp},{1,"${q3map2}",q3+"-light -fast "+bsp}}});
  preset({"Q3: BSP + VIS + final LIGHT","${MAP_DIR_PATH}/_ftebroom-build",false,{
    {0,"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map",""},{1,"${q3map2}",q3+"-meta "+map},
    {1,"${q3map2}",q3+"-vis "+bsp},{1,"${q3map2}",q3+"-light -samples 2 "+bsp}}});
  preset({"Q1: BSP + VIS + LIGHT","${MAP_DIR_PATH}/_ftebroom-build",false,{
    {0,"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map",""},{1,"${qbsp}",map},{1,"${vis}",bsp},{1,"${light}","-extra "+bsp}}});
  const std::string q2Paths="-basedir \"${BASE_GAME_DIR_PATH}\" -gamedir \"${GAME_DIR_PATH}\" -path \"${WORK_DIR_PATH}\" ";
  preset({"Q2: BSP + VIS + LIGHT","${MAP_DIR_PATH}/_ftebroom-build",false,{{0,"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map",""},{1,"${q2bsp}","-q2bsp "+q2Paths+map},{1,"${q2vis}",bsp},{1,"${q2light}",q2Paths+bsp}}});
  preset({"HL: CSG + BSP + VIS + RAD","${MAP_DIR_PATH}/_ftebroom-build",false,{{0,"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map",""},{1,"${hlcsg}","-nowadtextures "+map},{1,"${hlbsp}",map},{1,"${hlvis}",map},{1,"${hlrad}",map}}});
  auto root=documentPath("ftebroom.cfg").parent_path().parent_path();
  for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"q2bsp","tools/ericw-q2/qbsp"},{"q2vis","tools/ericw-q2/vis"},{"q2light","tools/ericw-q2/light"},{"hlcsg","tools/sdhlt/sdHLCSG"},{"hlbsp","tools/sdhlt/sdHLBSP"},{"hlvis","tools/sdhlt/sdHLVIS"},{"hlrad","tools/sdhlt/sdHLRAD"}}){
    auto path=root/pair.second;
#ifdef _WIN32
    path+=".exe";
#endif
    if(runVariables[pair.first].empty()&&std::filesystem::is_regular_file(path))runVariables[pair.first]=std::filesystem::absolute(path).u8string();
  }
  std::error_code compilerError;
  if(std::none_of(runProfiles.begin(),runProfiles.end(),[](const RunProfile& p){return p.name=="Q1: Export + qbsp";}))
    runProfiles.push_back({"Q1: Export + qbsp","${MAP_DIR_PATH}/_ftebroom-build",false,{{0,"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map",""},{1,"${qbsp}","\"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map\""}}});
  if(runVariables["q3map2"].empty() || !std::filesystem::is_regular_file(std::filesystem::u8path(runVariables["q3map2"]),compilerError)) {
    auto bundled=documentPath("ftebroom.cfg").parent_path().parent_path()/"tools/q3map2/q3map2.exe";
#ifndef _WIN32
    bundled.replace_extension();
#endif
    if(std::filesystem::exists(bundled))runVariables["q3map2"]=std::filesystem::absolute(bundled).lexically_normal().u8string();
  }
  for(const char* name:{"qbsp","vis","light"})if(runVariables[name].empty()) {
    auto bundled=documentPath("ftebroom.cfg").parent_path().parent_path()/"tools/ericw"/(std::string(name)+".exe");
#ifndef _WIN32
    bundled.replace_extension();
#endif
    if(std::filesystem::exists(bundled))runVariables[name]=std::filesystem::absolute(bundled).lexically_normal().u8string();
  }

}
#include "ftebroom_run_store.h"
static std::string runExpand(const std::string& text)
{
  if(runStrict)return ftebroom::profile::interpolate(text,activeTypedVariables);
  auto out=ftebroom::expandRunVariables(text,activeVariables);
  // Preserve saved launch profiles from distributions with the engine at root.
#ifdef _WIN32
  auto legacy=documentPath("ftebroom.cfg").parent_path().parent_path()/"fteqw64.exe";
  if(std::filesystem::u8path(out).lexically_normal()==legacy.lexically_normal())
    return runBundledEngine().u8string();
#endif
  return out;
}
static void runStop()
{
#ifdef _WIN32
  if(runInput){CloseHandle(runInput);runInput=nullptr;}
  if(runJob){TerminateJobObject(runJob,1);CloseHandle(runJob);runJob=nullptr;}
  else if(runProcess)TerminateProcess(runProcess,1);
  if(runProcess){CloseHandle(runProcess);runProcess=nullptr;}
  if(runPipe){CloseHandle(runPipe);runPipe=nullptr;}
#else
  if(runProcess>0){kill(-runProcess,SIGKILL);waitpid(runProcess,nullptr,0);runProcess=-1;}
  if(runPipe>=0){close(runPipe);runPipe=-1;}
#endif
  runMissingAwaiting=false;runMissingDecision=-1;runExportPrepared=false;
  runActive=runExportWaiting=false;runAppend("\nStopped.\n");
}
#ifdef _WIN32
static std::wstring runWide(const std::string& text)
{
  int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),nullptr,0);
  if(!count&&!text.empty())throw std::runtime_error("Invalid UTF-8 process argument.");
  std::wstring out(count,0);MultiByteToWideChar(CP_UTF8,0,text.data(),(int)text.size(),out.data(),count);return out;
}
#endif
static void runStartProcess(std::string executable,const std::string& arguments,const std::filesystem::path& working,bool launchEngine=false)
{
  if(executable.empty())throw std::runtime_error("Set the executable path in the profile's tool variables.");
  if(runStrict&&!launchEngine)executable=ftebroom::profile::parsePath(executable).u8string();
  auto configuredExecutable=std::filesystem::u8path(executable);
  if(configuredExecutable.is_relative()&&configuredExecutable.has_parent_path()&&
#ifdef _WIN32
 true
#else
 !runStrict
#endif
 )executable=std::filesystem::absolute(configuredExecutable).lexically_normal().u8string();
#ifdef _WIN32
  auto exe=runWide(executable);
  if(runStrict&&!configuredExecutable.has_parent_path()){
    wchar_t found[32768]={};auto n=SearchPathW(nullptr,exe.c_str(),L".exe",32768,found,nullptr);if(n&&n<32768)exe=found;
  }
  auto line=L"\""+exe+L"\" ";
  if(runStrict){for(const auto& a:ftebroom::profile::parseArguments(arguments)){
    auto w=runWide(a);line+=L'"';size_t slashes=0;
    for(wchar_t c:w){if(c==L'\\'){++slashes;continue;}if(c==L'"'){line.append(slashes*2+1,L'\\');line+=c;}else{line.append(slashes,L'\\');line+=c;}slashes=0;}
    line.append(slashes*2,L'\\');line+=L"\" ";
  }}else line+=runWide(arguments);
  if(launchEngine){
    STARTUPINFOW startup{};startup.cb=sizeof startup;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(exe.c_str(),line.data(),nullptr,nullptr,FALSE,0,nullptr,working.empty()?nullptr:working.c_str(),&startup,&process))
      throw std::runtime_error("Cannot launch engine (Windows error "+std::to_string(GetLastError())+").");
    CloseHandle(process.hThread);runLaunchedEngines.push_back(process.hProcess);
    return;
  }
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};HANDLE writer=nullptr;
  if(!CreatePipe(&runPipe,&writer,&security,0))throw std::runtime_error("Cannot create compiler output pipe.");
  SetHandleInformation(runPipe,HANDLE_FLAG_INHERIT,0);
  STARTUPINFOW startup{};startup.cb=sizeof startup;startup.dwFlags=STARTF_USESTDHANDLES;
  HANDLE reader=nullptr;
  if(arguments.find("-ftebroom-preview")!=std::string::npos){
    if(!CreatePipe(&reader,&runInput,&security,65536)){CloseHandle(writer);CloseHandle(runPipe);runPipe=nullptr;throw std::runtime_error("Cannot create preview control pipe.");}
    SetHandleInformation(runInput,HANDLE_FLAG_INHERIT,0);
  }
  startup.hStdOutput=startup.hStdError=writer;startup.hStdInput=reader?reader:GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION process{};
  BOOL started=CreateProcessW(exe.c_str(),line.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,working.empty()?nullptr:working.c_str(),&startup,&process);
  CloseHandle(writer);if(reader)CloseHandle(reader);
  if(!started && runInput){CloseHandle(runInput);runInput=nullptr;}
  if(!started){CloseHandle(runPipe);runPipe=nullptr;throw std::runtime_error("Cannot launch tool (Windows error "+std::to_string(GetLastError())+").");}
  runProcess=process.hProcess;runJob=CreateJobObjectW(nullptr,nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if(!runJob || !SetInformationJobObject(runJob,JobObjectExtendedLimitInformation,&limits,sizeof limits) || !AssignProcessToJobObject(runJob,runProcess))
  {
    TerminateProcess(runProcess,1);CloseHandle(process.hThread);CloseHandle(runProcess);runProcess=nullptr;
    if(runJob){CloseHandle(runJob);runJob=nullptr;}CloseHandle(runPipe);runPipe=nullptr;
    if(runInput){CloseHandle(runInput);runInput=nullptr;}
    throw std::runtime_error("Cannot create a stoppable process group.");
  }
  ResumeThread(process.hThread);CloseHandle(process.hThread);
#else
  int descriptors[2];if(!launchEngine&&pipe(descriptors))throw std::runtime_error("Cannot create compiler output pipe.");
  std::vector<std::string> words{executable};std::istringstream args(arguments);std::string word;
  if(runStrict){auto parsed=ftebroom::profile::parseArguments(arguments);words.insert(words.end(),parsed.begin(),parsed.end());}
  else while(args>>std::quoted(word))words.push_back(word);std::vector<char*> argv;
  for(auto& value:words)argv.push_back(value.data());argv.push_back(nullptr);
  posix_spawn_file_actions_t fileActions;posix_spawn_file_actions_init(&fileActions);
  if(!launchEngine){
    posix_spawn_file_actions_adddup2(&fileActions,descriptors[1],STDOUT_FILENO);posix_spawn_file_actions_adddup2(&fileActions,descriptors[1],STDERR_FILENO);
    posix_spawn_file_actions_addclose(&fileActions,descriptors[0]);posix_spawn_file_actions_addclose(&fileActions,descriptors[1]);
  }
  if(!working.empty())posix_spawn_file_actions_addchdir_np(&fileActions,working.c_str());
  posix_spawnattr_t attributes;posix_spawnattr_init(&attributes);posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);posix_spawnattr_setpgroup(&attributes,0);
  pid_t process=-1;
  int result=posix_spawnp(&process,executable.c_str(),&fileActions,&attributes,argv.data(),environ);
  posix_spawnattr_destroy(&attributes);posix_spawn_file_actions_destroy(&fileActions);
  if(launchEngine){
    if(result)throw std::runtime_error("Cannot launch engine: "+std::to_string(result));
    runLaunchedEngines.push_back(process);
    return;
  }
  close(descriptors[1]);
  if(result){close(descriptors[0]);runProcess=-1;throw std::runtime_error("Cannot launch tool: "+std::to_string(result));}
  runProcess=process;
  runPipe=descriptors[0];fcntl(runPipe,F_SETFL,O_NONBLOCK);
#endif
}
static void runBegin(const RunProfile& profile,bool test=false)
{
  if(runActive||runExclusive)return;
  runWindow=true;runError.clear();runHadError=false;runLog.clear();runStep=0;
  runMissingAwaiting=false;runMissingDecision=0;runExportPrepared=false;runMissingNames.clear();runPlaceholderNames.clear();
  if(!profile.trenchBroom&&gameClassic() && std::any_of(profile.tasks.begin(),profile.tasks.end(),[](const RunTask& task){return task.kind==1 && task.source.find("q3map2")!=std::string::npos;})){
    runError="q3map2 cannot compile this game format. Use Compile / Setup compile for the active game profile.";runHadError=true;return;
  }
  if(documentFile.empty() && !profile.launch && profile.name!="Export selected BSP model"){runError="Save the map before running a profile.";return;}
  try
  {
    if(gameProfile==2&&!gameMapModsError.empty())throw std::runtime_error(gameMapModsError);
    runningProfile=lastRunProfile=profile;runStrict=profile.trenchBroom;runTesting=test;
    activeVariables=runVariables;
    if(runStrict){
      runActiveGame=runStoreGame;activeLaunchVariables=runCommonVariables();activeTypedVariables=runWorkdirVariables();auto working=ftebroom::profile::interpolate(profile.work,activeTypedVariables);
      activeTypedVariables["WORK_DIR_PATH"]=working;activeTypedVariables["CPU_COUNT"]=std::max(1u,std::thread::hardware_concurrency());
      activeVariables["WORK_DIR_PATH"]=working;
      char camera[512]={};float values[7]={};cvars->GetString("ftebroom_camera_state",camera,sizeof camera);cameraParse(camera,values);
      std::ostringstream origin,yaw;origin<<values[0]<<' '<<values[1]<<' '<<values[2];yaw<<values[4];
      runCameraEntity={{"origin",origin.str()},{"angle",yaw.str()}};
      runReadyHook=nullptr;runExportHook=nullptr;runStageHook=nullptr;
      runHaveLast=true;runActive=true;runAppend((working.empty()||std::filesystem::is_directory(std::filesystem::u8path(working))?"#### Using working directory '":"#### Error: working directory '")+working+(working.empty()||std::filesystem::is_directory(std::filesystem::u8path(working))?"'\n":"' does not exist\n"));return;
    }
    activeVariables["MAP_BASE_NAME"]=documentFile.stem().u8string();activeVariables["MAP_FULL_NAME"]=documentFile.filename().u8string();
    activeVariables["MAP_DIR_PATH"]=ftebroom::pak::diskFolder(documentFile.parent_path()).u8string();activeVariables["GAME_DIR_PATH"]=baseq3;
    auto base=std::filesystem::u8path(baseq3);if(gameProfile==3){auto sibling=base.parent_path()/"baseq2";if(std::filesystem::is_directory(sibling))base=sibling;}activeVariables["BASE_GAME_DIR_PATH"]=base.u8string();
    activeVariables["GAME_DIR"]=baseq3;activeVariables["MODS"]="baseq3";
    auto runtime=gameProfile==0?std::string(baseq3):gameRuntime();
    if(runtime.empty())runtime=baseq3;
    auto modFolder=std::filesystem::u8path(runtime).lexically_normal();
    if(!modFolder.empty()&&modFolder.filename().empty())modFolder=modFolder.parent_path();
    activeVariables["TB_GAME_ROOT"]=modFolder.has_filename()&&!modFolder.has_parent_path()?".":modFolder.parent_path().generic_u8string();
    activeVariables["TB_ACTIVE_MOD"]=modFolder.filename().generic_u8string();
    activeVariables["CPU_COUNT"]=std::to_string(std::max(1u,std::thread::hardware_concurrency()));
    activeVariables["APP_DIR_PATH"]=documentPath("ftebroom.cfg").parent_path().parent_path().u8string();
    activeVariables["APP_DIR"]=activeVariables["APP_DIR_PATH"];
    activeVariables["WORK_DIR_PATH"]=ftebroom::pak::diskFolder(documentFile.parent_path()).u8string();activeVariables["WORK_DIR"]=activeVariables["WORK_DIR_PATH"];
    auto working=runExpand(profile.work);activeVariables["WORK_DIR_PATH"]=working;activeVariables["WORK_DIR"]=working;
    if(!runTesting)std::filesystem::create_directories(std::filesystem::u8path(working));
    if(!std::filesystem::is_directory(std::filesystem::u8path(working)))throw std::runtime_error("Working directory does not exist.");
    runningProfile=lastRunProfile=profile;runHaveLast=true;runActive=true;runAppend("Running "+profile.name+"\n");
  }
  catch(const std::exception& error){runError=error.what();runHadError=true;}
}
static std::filesystem::path runFilePath(const std::string& text)
{
  auto path=runStrict?ftebroom::profile::parsePath(text):std::filesystem::u8path(text);
  if(path.is_relative())path=std::filesystem::u8path(activeVariables.at("WORK_DIR_PATH"))/path;
  return std::filesystem::weakly_canonical(path);
}
static bool runIsWorkingMap(const std::filesystem::path& path)
{
  std::error_code error;
  if(std::filesystem::equivalent(path,documentFile,error)&&!error)return true;
  auto left=std::filesystem::weakly_canonical(path).u8string();
  auto right=std::filesystem::weakly_canonical(documentFile).u8string();
#ifdef _WIN32
  return CompareStringOrdinal(runWide(left).c_str(),-1,runWide(right).c_str(),-1,TRUE)==CSTR_EQUAL;
#else
  return left==right;
#endif
}
static bool runGlobMatch(const std::string& pattern,const std::string& name)
{
  size_t p=0,n=0,star=std::string::npos,retry=0;
  while(n<name.size())
  {
    if(p<pattern.size()&&(pattern[p]=='?'||pattern[p]==name[n])){++p;++n;}
    else if(p<pattern.size()&&pattern[p]=='*'){star=p++;retry=n;}
    else if(star!=std::string::npos){p=star+1;n=++retry;}
    else return false;
  }
  while(p<pattern.size()&&pattern[p]=='*')++p;
  return p==pattern.size();
}
static void runFileTask(int kind,const std::string& source,const std::string& destination,bool targetDirectory=false)
{
  auto from=runFilePath(source),to=runFilePath(destination);
  const auto pattern=from.filename().u8string();
  const bool wildcard=pattern.find_first_of("*?")!=std::string::npos;
  if(wildcard&&kind==3)throw std::runtime_error("Rename File requires one exact source path.");
  std::vector<std::filesystem::path> inputs;
  if(wildcard)
  {
    for(const auto& entry:std::filesystem::directory_iterator(from.parent_path()))
      if(entry.is_regular_file()&&runGlobMatch(pattern,entry.path().filename().u8string()))inputs.push_back(entry.path());
    std::sort(inputs.begin(),inputs.end());
    if(inputs.empty())throw std::runtime_error("No files match: "+source);
  }
  else inputs.push_back(from);
  // Validate the complete batch before any file is changed.
  std::vector<std::filesystem::path> outputs;
  for(const auto& path:inputs)
  {
    auto output=(wildcard||targetDirectory||std::filesystem::is_directory(to))?to/path.filename():to;
    if(!std::filesystem::is_regular_file(path))throw std::runtime_error("File tasks accept regular files only.");
    if(runIsWorkingMap(path)||(kind!=4&&runIsWorkingMap(output)))throw std::runtime_error("File tasks must not replace or delete the working map.");
    outputs.push_back(output);
  }
  for(size_t i=0;i<inputs.size();++i)
  {
    if(kind==4)std::filesystem::remove(inputs[i]);
    else
    {
      std::filesystem::create_directories(outputs[i].parent_path());
      if(kind==2){
        std::error_code ignored;
        if(!targetDirectory||!std::filesystem::equivalent(inputs[i],outputs[i],ignored))
          std::filesystem::copy_file(inputs[i],outputs[i],std::filesystem::copy_options::overwrite_existing);
      }
      else std::filesystem::rename(inputs[i],outputs[i]);
    }
  }
}
static bool runSnapshotHasWad(const std::string& text){
  // QC serializes each entity property on its own line. Inspect worldspawn
  // only, and retain both WAD key spellings accepted by Quake compilers.
  std::istringstream rows(text);std::string line;int depth=0;
  while(std::getline(rows,line)){
    auto first=line.find_first_not_of(" \t\r"),last=line.find_last_not_of(" \t\r");
    if(first==line.npos)continue;auto row=line.substr(first,last-first+1);
    if(row=="{"){++depth;continue;}
    if(row=="}"){if(--depth==0)break;continue;}
    if(depth==1){std::istringstream property(row);std::string key,value;
      if(property>>std::quoted(key)>>std::quoted(value))
        if((key=="wad"||key=="_wad")&&!value.empty())return true;
    }
  }
  return false;
}
static void runExportTrenchSnapshot(const RunTask& task,const std::filesystem::path& target){
  auto text=q1Read(documentPath("data/export-map.map"));
  // Save supplies the indexed WAD list for a newly authored map, but does not
  // rewrite its live QC worldspawn. Give immediate Export/Compile the same
  // default resources without changing any explicitly authored WAD references.
  // Texture staging/model/contents transformations stay in the legacy adapter.
  if(gameWad())text=ftebroom::quake1::map(text,gameProfile==2&&q1MapFormat==0,
    gameProfile==2&&!runSnapshotHasWad(text)?q1WadList():std::string{},false,false);
  auto entity=task.entityToAdd;
  if(!entity.is_null())for(auto it=runCameraEntity.begin();it!=runCameraEntity.end();++it){bool found=false;
    for(auto& p:entity)if(p.at("key")==it.key()){p["value"]=it.value();found=true;}
    if(!found)entity.push_back({{"key",it.key()},{"value",it.value()}});
  }
  text=ftebroom::profile::exportEntities(text,task.stripTbProperties,task.stripEntityPattern,entity);
  std::filesystem::create_directories(target.parent_path());buildWriteTexture(target,text);
}
static void runTrenchTask(){
  const auto& task=runningProfile.tasks[runStep];
  if(task.kind==5){
    try{
      auto engines=runEngines;
      if(runActiveGame!=runStoreGame){engines.clear();for(const auto& e:runStore["games"][runActiveGame].value("engines",nlohmann::json::array()))engines.push_back(runEngineFromJson(e));}
      auto e=std::find_if(engines.begin(),engines.end(),[&](const auto& p){return p.id==task.engineProfileId;});
      if(e==engines.end())throw std::runtime_error("Engine profile '"+task.engineProfileId+"' was not found");
      runAppend("#### Launching engine profile '"+e->name+"' at '"+e->path+"'\n");
      if(runTesting){ftebroom::profile::interpolate(e->path,activeTypedVariables);ftebroom::profile::parseArguments(ftebroom::profile::interpolate(e->parameters,activeTypedVariables));++runStep;return;}
      auto vars=activeTypedVariables;
      // Compilation launch uses the frozen document/workdir, including WORK_DIR_PATH.
      auto plan=ftebroom::engineLaunchPlan(*e,vars,runApplicationDirectory());
      runAppend("#### Engine command: "+plan.executable.u8string()+" "+plan.arguments+"\n#### Engine working directory: "+plan.working.u8string()+"\n");
      if(!plan.target.empty())runAppend("#### Map: "+plan.source.u8string()+" -> "+plan.target.u8string()+"\n");
      if(!runTesting){ftebroom::stageEngineMap(plan);runStartProcess(plan.executable.u8string(),plan.arguments,plan.working,true);runAppend("#### Engine process started (BSP compilation and game startup are separate results).\n");}
    }catch(const std::exception& e){runAppend("#### Launch failed: "+std::string(e.what())+"\n");if(!(task.flags&RunAllowLaunchFailure))throw;runAppend("#### Continuing despite launch failure\n");}
    ++runStep;return;
  }
  auto source=runExpand(task.source),destination=runExpand(task.destination);
  auto working=std::filesystem::u8path(activeVariables.at("WORK_DIR_PATH"));
  if(task.kind==0){
    auto p=ftebroom::profile::parsePath(source);if(p.is_relative())p=working/p;
    runAppend("#### Exporting map file '"+p.u8string()+"'\n");
    if(runTesting){++runStep;return;}
    cvars->SetFloat("ftebroom_export_status",0);command("ftebroom_export_map\n");runExportWaiting=true;return;
  }
  if(task.kind==1){
    runAppend("#### Executing '"+source+" "+destination+"'\n");
    if(runTesting){ftebroom::profile::parseArguments(destination);++runStep;return;}
    runStartProcess(source,destination,working);return;
  }
  auto plan=ftebroom::profile::planFiles(task.kind,source,destination,working);
  runAppend(task.kind==2?"#### Copying to '"+plan.target.u8string()+"/': ":task.kind==3?"#### Renaming to '"+plan.target.u8string()+"': ":"#### Deleting: ");
  for(const auto& p:plan.sources)runAppend(p.u8string()+", ");runAppend("\n");
  if(!runTesting)ftebroom::profile::executeFiles(plan);++runStep;
}
static void runTick()
{
  // Keep launched games independent of compiler cancellation, but observe their
  // exit status rather than reporting a failed startup as a successful compile.
  runLaunchedEngines.erase(std::remove_if(runLaunchedEngines.begin(),runLaunchedEngines.end(),[](auto process){
#ifdef _WIN32
    if(WaitForSingleObject(process,0)==WAIT_TIMEOUT)return false;
    DWORD code=1;GetExitCodeProcess(process,&code);CloseHandle(process);
    const bool failed=code!=0;auto message="Engine exited with code "+std::to_string(code);
#else
    int status=0;const auto result=waitpid(process,&status,WNOHANG);
    if(result==0||(result<0&&errno==EINTR))return false;
    if(result<0)return errno==ECHILD;
    const bool failed=!WIFEXITED(status)||WEXITSTATUS(status)!=0;
    auto message=WIFEXITED(status)?"Engine exited with code "+std::to_string(WEXITSTATUS(status)):"Engine crashed with signal "+std::to_string(WTERMSIG(status));
#endif
    runAppend("#### "+message+"\n");if(failed){runError=message+". Check the engine's game data and console log.";runHadError=true;}
    return true;
  }),runLaunchedEngines.end());
  if(!runActive||runMissingAwaiting)return;
  try
  {
    if(runReadyHook&&!runReadyHook())return;
#ifdef _WIN32
    if(runProcess)
    {
      DWORD available=0;char buffer[4096];int batches=0;
      while(batches++<64 && PeekNamedPipe(runPipe,nullptr,0,nullptr,&available,nullptr)&&available){DWORD received=0;
        if(!ReadFile(runPipe,buffer,std::min<DWORD>(available,sizeof buffer),&received,nullptr)||!received)break;runAppend(std::string(buffer,received));}
      if(WaitForSingleObject(runProcess,0)==WAIT_TIMEOUT)return;
      if(PeekNamedPipe(runPipe,nullptr,0,nullptr,&available,nullptr)&&available)return;
      DWORD code=1;GetExitCodeProcess(runProcess,&code);CloseHandle(runProcess);runProcess=nullptr;
      CloseHandle(runPipe);runPipe=nullptr;if(runInput){CloseHandle(runInput);runInput=nullptr;}if(runJob){CloseHandle(runJob);runJob=nullptr;}
      if(code){
        auto message="Tool exited with code "+std::to_string(code);
        // Match the reference process status mapping, including breakpoint /
        // guard-page exceptions and its reserved forced-termination code.
        if(code==0xf291UL||(code>=0x80000000UL&&code<0xD0000000UL)||!(runningProfile.tasks[runStep].flags&RunAllowNonzero))throw std::runtime_error(message);
        runAppend(message+"; continuing as configured.\n");
      }
      if(runStageHook)runStageHook();
      ++runStep;
    }
#else
    if(runProcess>0){char buffer[4096];ssize_t count;while((count=read(runPipe,buffer,sizeof buffer))>0)runAppend(std::string(buffer,count));
      int status=0;auto waited=waitpid(runProcess,&status,WNOHANG);if(waited==0 || (waited<0&&errno==EINTR))return;runProcess=-1;close(runPipe);runPipe=-1;
      if(waited<0)throw std::runtime_error("Cannot read tool exit status. Start with Launch-ReyondBroom.sh so the editor owns compiler processes.");
      if(!WIFEXITED(status)||WEXITSTATUS(status)){
        if(!WIFEXITED(status))throw std::runtime_error("Tool crashed with signal "+std::to_string(WTERMSIG(status)));
        if(!(runningProfile.tasks[runStep].flags&RunAllowNonzero))throw std::runtime_error("Tool exited with code "+std::to_string(WEXITSTATUS(status)));
        runAppend("Tool exited with code "+std::to_string(WEXITSTATUS(status))+"; continuing as configured.\n");
      }
      if(runStageHook)runStageHook();++runStep;}
#endif
    if(runExportWaiting)
    {
      int status=(int)cvars->GetFloat("ftebroom_export_status");if(!status)return;
      if(status!=1)throw std::runtime_error("Map export failed.");
      auto target=runFilePath(runExpand(runningProfile.tasks[runStep].source));
      if(!runStrict&&runIsWorkingMap(target))throw std::runtime_error("Export Map must use a build copy, not the working map.");
      if(runStrict)runExportTrenchSnapshot(runningProfile.tasks[runStep],target);
      else if(!runPrepareExport(target))return;
      if(runExportHook)runExportHook(target);
      runExportWaiting=false;runExportPrepared=false;runMissingDecision=0;++runStep;
      if(runReadyHook)return;
    }
    if(runStep>=(int)runningProfile.tasks.size()){runActive=false;runAppend("Completed successfully.\n");return;}
    if(!runningProfile.tasks[runStep].enabled){runAppend("Skipped disabled step "+std::to_string(++runStep)+"\n");return;}
    if(runStrict){runTrenchTask();return;}
    const auto& task=runningProfile.tasks[runStep];auto source=runExpand(task.source),destination=runExpand(task.destination);
    runAppend("Step "+std::to_string(runStep+1)+": "+source+" "+destination+"\n");
    if(runTesting){++runStep;return;}
    if(task.kind==0){cvars->SetFloat("ftebroom_export_status",0);command(task.destination=="@selected-brushes"?"ftebroom_export_selected_bsp\n":"ftebroom_export_map\n");runExportWaiting=true;return;}
    if(task.kind==1){
      auto working=std::filesystem::u8path(activeVariables["WORK_DIR_PATH"]);
      const bool launchEngine=(task.flags&RunLaunchEngine)!=0;
      if(launchEngine){
        if(source.empty())throw std::runtime_error("Set the engine executable path.");
        auto executable=runFilePath(source);
        source=executable.u8string();
        working=executable.parent_path();
      }
      try{
        runStartProcess(source,destination,working,launchEngine);
        if(!launchEngine)return;
      }
      catch(const std::exception& error){
        if(!(task.flags&RunAllowLaunchFailure))throw;
        runAppend(std::string(error.what())+" Continuing as configured.\n");
      }
    }else runFileTask(task.kind,source,destination,(task.flags&RunCopyToDirectory)!=0);
    ++runStep;
  }
  catch(const std::exception& error){runError=error.what();runHadError=true;runActive=runExportWaiting=false;runAppend("ERROR: "+runError+"\n");}
}
static bool runText(const char* label,std::string& value)
{
  std::vector<char> buffer(std::max<size_t>(4096,value.size()+256));std::copy(value.begin(),value.end(),buffer.begin());
  if(ImGui::InputText(label,buffer.data(),buffer.size())){value=buffer.data();return true;}return false;
}
static void runUiRecord(const std::string& key);
#include "ftebroom_run_import.h"
#include "ftebroom_run_commands.h"
#include "ftebroom_run_ui.h"
static void runPanel(){runImGuiPanel();}
static void runAction(const std::string& operation)
{
  runLoad();
  if(operation=="launch"){runUiShow(2);return;}
  if(operation=="rerun"){
    if(runHaveLast&&!lastRunProfile.trenchBroom){runBegin(lastRunProfile);return;}
    if(runHaveLast){auto p=std::find_if(runProfiles.begin(),runProfiles.end(),[](const auto& p){return p.name==lastRunProfile.name;});if(p!=runProfiles.end())runBegin(*p);}
    return;
  }
  if(operation=="launchfte"){for(const auto& profile:runBundledProfiles)if(profile.name=="Launch in FTE"){runBegin(profile);return;}}
  runWindow=true;
}
#include "ftebroom_run_tests.h"
static void testRunProfiles()
{
  if(!cvars->GetFloat("ftebroom_skip_setup"))return;
  char argument[16]={};commands->Argv(1,argument,sizeof argument);int stage=std::atoi(argument);
  auto expect=[](bool ok,const char* name){core->Print((std::string(ok?"FTEBROOM_RUN_PASS ":"FTEBROOM_RUN_FAIL ")+name+"\n").c_str());};
  if(stage>=20){testRunParity(stage);return;}
  auto root=documentPath("maps/run-test");std::filesystem::create_directories(root);
#ifdef _WIN32
  wchar_t system[MAX_PATH]={};GetSystemDirectoryW(system,MAX_PATH);auto shell=(std::filesystem::path(system)/"cmd.exe").u8string();
#else
  std::string shell="/bin/sh";
#endif
  if(stage==0)
  {
    std::ofstream(root/"source.txt")<<"pipeline fixture";
    RunProfile profile{"Pipeline test",root.u8string(),false,{{0,"${WORK_DIR_PATH}/build.map",""},
#ifdef _WIN32
      {1,shell,"/d /c echo FTEBROOM_TOOL_OUTPUT"},
#else
      {1,"/bin/echo","FTEBROOM_TOOL_OUTPUT"},
#endif
      {2,"source.txt","copy.txt"},
      {3,"copy.txt","renamed.txt"},
      {4,"renamed.txt",""},{1,"missing-disabled-tool","",false}}};
    runLoad();runBegin(profile);runMissingDecision=1;
  }
  else if(stage==1)
  {
    expect(!runActive&&!runHadError&&runStep==6 && runLog.find("Skipped disabled step")!=std::string::npos,"all_five_tasks");
    expect(runLog.find("FTEBROOM_TOOL_OUTPUT")!=std::string::npos,"streamed_output");
    expect(std::filesystem::exists(root/"build.map")&&!std::filesystem::exists(root/"copy.txt")&&!std::filesystem::exists(root/"renamed.txt"),"export_copy_rename_delete");
    expect(activeVariables["MAP_FULL_NAME"]==documentFile.filename().u8string()&&!activeVariables["CPU_COUNT"].empty(),"variables");
    expect(runIsWorkingMap(documentFile.parent_path()/"."/documentFile.filename()),"working_map_path_alias_protected");
    expect(runFilePath("../run-test/source.txt")==std::filesystem::weakly_canonical(root/"source.txt"),"relative_paths_use_profile_working_directory");
    runFileTask(2,"*.txt","batch");
    expect(std::filesystem::exists(root/"batch/source.txt"),"copy_file_pattern");
    runFileTask(4,"batch/*.txt","");
    expect(!std::filesystem::exists(root/"batch/source.txt"),"delete_file_pattern");
  }
  else if(stage==2)
  {
#ifdef _WIN32
    runBegin({"Failing tool",root.u8string(),false,{{1,shell,"/d /c exit /b 7"}}});
#else
    runBegin({"Failing tool",root.u8string(),false,{{1,"/bin/false",""}}});
#endif
  }
  else if(stage==3)expect(!runActive&&runHadError,"nonzero_exit_stops_pipeline");
  else if(stage==4)
  {
#ifdef _WIN32
    runBegin({"Stop test",root.u8string(),false,{{1,shell,"/d /c ping -n 30 127.0.0.1"}}});
#else
    runBegin({"Stop test",root.u8string(),false,{{1,"/bin/sleep","30"}}});
#endif
  }
  else if(stage==5){runStop();expect(!runActive,"stop_process_tree");}
  else if(stage==6)
  {
    char compiler[4096]={};cvars->GetString("ftebroom_test_compiler",compiler,sizeof compiler);
    runBegin({"Actual q3map2 test",root.u8string(),false,{{0,"build.map",""},
      {1,compiler,"-game quake3 -fs_basepath \"${WORK_DIR_PATH}\" -meta \"${WORK_DIR_PATH}/build.map\""}}});
  }
  else if(stage==7)
  {
    expect(!runActive&&!runHadError&&runStep==2,"real_q3map2_profile");
    expect(std::filesystem::exists(root/"build.bsp")&&std::filesystem::file_size(root/"build.bsp")>100,"real_q3map2_bsp");
  }
  else if(stage==8)
  {
#ifdef _WIN32
    // cmd.exe parses /c itself, unlike the native argv contract exercised here.
    // Use the real test executable so engine exit monitoring observes code 0.
    shell=(documentPath("ftebroom.cfg").parent_path().parent_path()/"tools/compilation-profile-probe.exe").u8string();
#endif
    const auto file=root/"CompilationProfiles.cfg";
    const auto engines=root/"GameEngineProfiles.cfg";
    std::ofstream(file)<<R"({
      // TrenchBroom compilation profile fixture
      "version":2,"profiles":[{"name":"Pipeline test","workdir":"","tasks":[
        {"type":"export","target":"${MAP_BASE_NAME}-compile.map"},
        {"type":"tool","tool":"${rb_test_tool}","parameters":"\"a b.bsp\""},
        {"type":"copy","source":"source.txt","target":"${GAME_DIR_PATH}/${MODS[-1]}/maps","enabled":false},
        {"type":"launchEngine","engineProfileId":"external","treatLaunchFailureAsError":true},
        {"type":"rename","source":"source.txt","target":"renamed.txt"},
        {"type":"delete","target":"renamed.txt","enabled":false}
      ]},{"name":"Unsupported profile","workdir":"","tasks":[
        {"type":"export","target":"map.map","stripEntityPattern":"light*"}
      ]}]})";
    nlohmann::json launch={{"id","external"},{"name","Engine"},{"path",shell},{"parameters",""}};
    std::ofstream(engines)<<nlohmann::json{{"version",1},{"profiles",nlohmann::json::array({launch})}};
    runLoad();
    try{
      auto imported=runReadTrenchBroom(file);
      if(imported.profiles.size()!=2||imported.profiles.front().tasks.size()!=6)
        throw std::runtime_error("Unexpected imported profile count");
      const auto& profile=imported.profiles.front();
      expect(profile.work.empty()&&profile.tasks[0].source=="${MAP_BASE_NAME}-compile.map"&&
             profile.tasks[1].destination=="\"a b.bsp\""&&imported.missingVariables.count("rb_test_tool")==1&&
             imported.notices.empty(),"trenchbroom_profiles_parsed");
      expect(!profile.tasks[2].enabled&&profile.tasks[2].destination=="${GAME_DIR_PATH}/${MODS[-1]}/maps"&&
             (profile.tasks[2].flags&RunCopyToDirectory)&&profile.tasks[3].engineProfileId=="external"&&profile.tasks[3].kind==5&&
             imported.engines.size()==1&&imported.engines[0].path==shell,"trenchbroom_variables_copy_and_engine");
      std::filesystem::remove(engines);
      auto missing=runReadTrenchBroom(file);
      expect(missing.profiles.front().tasks[3].enabled&&missing.notices.empty(),"trenchbroom_missing_engine");
      if(runProfiles.empty())runProfiles.push_back({"Pipeline test",root.u8string(),false,{}});
      const auto count=runProfiles.size();
      imported.profiles.front().name=runProfiles.front().name;
      imported.missingVariables["rb_test_tool"]=shell;
      bool saved=runSaveImported(imported);
      expect(saved&&runProfiles.size()==count+2&&runProfiles[count].name==runProfiles.front().name&&
             runVariables.at("rb_test_tool")==shell,"trenchbroom_saved_alongside_existing");
      auto test=imported.profiles.front();
      test.work=root.u8string();
      test.launch=true;
      test.tasks={test.tasks[1],test.tasks[3],{2,"source.txt","new maps",true,RunCopyToDirectory}};
      test.tasks[0].destination=
#ifdef _WIN32
        "exit 7";
      test.tasks[1].destination="exit 0";
#else
        "-c \"exit 7\"";
      test.tasks[1].destination="-c \"exit 0\"";
#endif
      for(auto& engine:runEngines)if(engine.id==test.tasks[1].engineProfileId)engine.parameters=test.tasks[1].destination;
      runBegin(test);
    }catch(const std::exception& error){core->Print((std::string(error.what())+"\n").c_str());expect(false,"trenchbroom_import_exception");}
  }
  else if(stage==9)
  {
    if(runActive||runHadError||runStep!=3)core->Print(("FTEBROOM_RUN_DIAGNOSTIC step="+std::to_string(runStep)+" active="+std::to_string(runActive)+" error="+runError+"\n"+runLog).c_str());
    expect(!runActive&&!runHadError&&runStep==3&&runLog.find("continuing as configured")!=std::string::npos&&
           std::filesystem::is_regular_file(root/"new maps/source.txt"),"trenchbroom_imported_pipeline_ran");
  }
  else if(stage==12){runLoad();runWindow=true;}
  else if(stage==10){
    auto imported=runReadTrenchBroom(documentPath("CompilationProfiles-regression.cfg"));
    expect(imported.profiles.size()==5&&imported.notices.empty()&&imported.missingVariables.empty(),"customer_five_profiles_imported");
    bool tasks=imported.profiles.size()==5;for(const auto& profile:imported.profiles)tasks&=profile.tasks.size()==14;
    expect(tasks,"customer_seventy_tasks_preserved");
    runLoad();auto count=runProfiles.size();bool saved=runSaveImported(imported);
    expect(saved&&runProfiles.size()==count+5,"customer_profiles_saved");
    runWindow=true;
  }
  else if(stage==11){
    runLoad();int found=0;bool arguments=true;
    auto old=activeVariables;activeVariables={{"CPU_COUNT","8"},{"MAP_BASE_NAME","example"}};
    for(const auto& profile:runProfiles)if(profile.name=="VKQuake_Full"||profile.name=="VK-Quake_Disturb (schnell)"||profile.name=="VKQuake_Full_with_Music"||profile.name=="IronWail_Full"||profile.name=="IronWail_Disturb (schnell)"){
      found++;arguments&=profile.tasks.size()==14;
      for(const auto& task:profile.tasks)if(task.destination.find("${CPU_COUNT - 1}")!=std::string::npos)arguments&=ftebroom::profile::interpolate(task.destination,{{"CPU_COUNT",8},{"MAP_BASE_NAME","example"}}).find("-threads 7 ")==0;
    }
    expect(found==5,"customer_profiles_restored");expect(arguments,"customer_expression_expanded_after_restore");
    expect(ftebroom::profile::interpolate("${CPU_COUNT - 1}",{{"CPU_COUNT",1}})=="0","customer_single_core_matches_trenchbroom");activeVariables=old;
  }

}
