// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
static nlohmann::json runTestInventory(const std::filesystem::path& root){
 auto j=nlohmann::json::object();for(const auto& p:std::filesystem::recursive_directory_iterator(root)){auto name=p.path().lexically_relative(root).generic_u8string();j[name]=p.is_directory()?nlohmann::json{{"type","directory"}}:nlohmann::json{{"type","file"},{"data",q1Read(p.path())}};}return j;
}
static void testRunParity(int stage){
 auto expect=[](bool value,const char* name){core->Print((std::string(value?"FTEBROOM_PARITY_PASS ":"FTEBROOM_PARITY_FAIL ")+name+"\n").c_str());};
 auto root=documentPath("maps/compilation-parity");auto app=documentPath("ftebroom.cfg").parent_path().parent_path();
#ifdef _WIN32
 auto probe=(app/"tools/compilation-profile-probe.exe").u8string();
#else
 auto probe=(app/"tools/compilation-profile-probe").u8string();
#endif
 static RunProfile pipeline;static nlohmann::json before;static std::string previousMods;
 auto finishProfile=[&](std::vector<RunTask> tasks){RunProfile p{"Compatibility probe",root.u8string(),false,std::move(tasks)};p.trenchBroom=true;runBegin(p);};
 if(stage>=60&&stage<=64){uiScaleChoice=stage-60;uiStyle();}
 else if(stage>=40&&stage<=43){
  runWindow=true;runUiAtlasWidth=stage>=42?1200:800;runUiAtlasHeight=stage>=42?800:600;runUiAtlasTheme=stage%2?2:1;runUiManagerHeight=442;
  runUiAtlasDirectory=documentPath("compile-ui-atlas")/(std::to_string(runUiAtlasWidth)+"x"+std::to_string(runUiAtlasHeight)+(stage%2?"-dark":"-light"));runUiAtlasFrames=3;
  expect(true,"imgui_compile_ui_capture_requested");
 }else if(stage==20){
  std::filesystem::remove_all(root);std::filesystem::create_directories(root/"cleanup/nested");
  buildWriteTexture(root/std::filesystem::u8path(u8"seed α.txt"),"seed data");buildWriteTexture(root/"cleanup/top.tmp","remove");buildWriteTexture(root/"cleanup/nested/deep.TMP","remove");buildWriteTexture(root/"cleanup/nested/keep.bsp","keep");
  pipeline={"TrenchBroom compatibility",root.u8string(),false,{
   {0,"snapshot.map",""},{2,u8"seed α.txt","copydir",true,RunCopyToDirectory},{3,u8"copydir/seed α.txt",u8"moved/renamed α.txt"},{4,"cleanup/*.tmp",""},
   {2,"missing.bsp","no-matches",true,RunCopyToDirectory},{4,"cleanup/missing-file.bsp",""},
   {1,probe,R"(args "C:\Tools\map dir\test.map" "" 'single words')",true,RunAllowNonzero},{1,probe,"exit 7",true,RunAllowNonzero},{1,probe,"done",true,RunAllowNonzero},{1,"${broken; expression}","",false}}};
  pipeline.trenchBroom=true;pipeline.tasks[0].stripTbProperties=true;pipeline.tasks[0].stripEntityPattern="light*";
  pipeline.tasks[0].entityToAdd=nlohmann::json::array({{{"key","classname"},{"value","info_player_start"}},{{"key","rb_compile_marker"},{"value","extra property"}}});
  runLoad();runProfiles={pipeline};runSelected=0;runSaveProfiles();before=runTestInventory(root);runBegin(pipeline,true);
 }else if(stage==21){
  expect(!runActive&&!runHadError&&runStep==10,"test_completed");expect(before==runTestInventory(root),"test_no_file_or_directory_changes");expect(runLog.find("PROBE_RAN")==std::string::npos,"test_did_not_start_processes");
 }else if(stage==22)runBegin(pipeline);
 else if(stage==23){
  expect(!runActive&&!runHadError&&runStep==10,"all_six_task_pipeline_completed");
  expect(q1Read(root/std::filesystem::u8path(u8"moved/renamed α.txt"))=="seed data","copy_and_rename_unicode");
  expect(!std::filesystem::exists(root/"cleanup/top.tmp")&&!std::filesystem::exists(root/"cleanup/nested/deep.TMP")&&std::filesystem::exists(root/"cleanup/nested/keep.bsp"),"delete_recursive_case_insensitive");
  expect(std::filesystem::is_directory(root/"no-matches"),"empty_copy_continues_and_creates_target");
  expect(runLog.find("code 7")!=std::string::npos&&runLog.find("PROBE_RAN")!=std::string::npos,"nonzero_policy_continues");
  expect(runLog.find(R"(C:\\Tools\\map dir\\test.map)")!=std::string::npos,"argv_preserves_quoted_backslashes");
  auto map=q1Read(root/"snapshot.map");expect(map.find("rb_compile_marker")!=std::string::npos&&map.find("extra property")!=std::string::npos&&map.find("\"origin\"")!=std::string::npos&&map.find("\"angle\"")!=std::string::npos,"export_adds_camera_entity_and_extra_properties");
  expect(runProfileToJson(runProfileFromJson(runProfileToJson(pipeline)))==runProfileToJson(pipeline),"all_fields_round_trip");
 }else if(stage==24){
  int game=gameProfile;auto original=runProfileToJson(runProfiles.front());runSaveProfiles();gameProfile=2;runLoad();runProfiles.clear();runVariables["game_scope_probe"]="quake1";runSaveProfiles();
  gameProfile=game;runLoad();expect(!runProfiles.empty()&&runProfileToJson(runProfiles.front())==original&&runVariables.count("game_scope_probe")==0,"per_game_profiles_and_tools");
  gameProfile=2;runLoad();expect(runProfiles.empty(),"empty_profile_list_does_not_resurrect_presets");gameProfile=game;runLoad();
 }else if(stage==25){
  std::string query;qcQuery(7,0,query);std::istringstream in(query);std::string version;in>>version>>std::quoted(previousMods);command("ftebroom_compile_mods \"modA;modB\"\n");
 }else if(stage==26){
  auto vars=runCommonVariables();expect(vars["MODS"]==nlohmann::json::array({runBaseMod,"modA","modB"}),"mods_are_per_map_ordered_array");
  expect(ftebroom::profile::interpolate("${MODS[0]}/${MODS[-1]}",vars)==runBaseMod+"/modB","mods_indices_keep_meaning");
  auto w=runWorkdirVariables();expect(!w.contains("CPU_COUNT")&&!w.contains("WORK_DIR_PATH"),"workdir_variable_scope");command("ftebroom_undo\n");
 }else if(stage==27){
  auto actual=runCommonVariables()["MODS"];auto expected=nlohmann::json::array({runBaseMod});for(const auto& name:ftebroom::profile::splitMods(previousMods))expected.push_back(name);expect(actual==expected,"mod_changes_are_undoable");
 }else if(stage==28)finishProfile({{1,probe,"sleep 10000"}});
 else if(stage==29){runStop();expect(!runActive,"stop_terminates_probe");}
 else if(stage==30)finishProfile({{1,"/this-tool-does-not-exist","",true,RunAllowNonzero},{1,probe,"must-not-run"}});
 else if(stage==31)expect(!runActive&&runHadError&&runStep==0,"spawn_error_not_masked_by_nonzero_policy");
 else if(stage==32){
  auto file=app/"tools/rb-parity-engine-launch.txt";std::filesystem::remove(file);
  runEngines={{"engine-A","First",probe,"write rb-parity-engine-launch.txt A"},{"engine-B","Second",probe,"write rb-parity-engine-launch.txt B"}};
  RunTask launch;launch.kind=5;launch.engineProfileId="engine-B";finishProfile({launch,{1,probe,"after-detached-engine"}});
 }else if(stage==33){expect(!runActive&&!runHadError&&q1Read(app/"tools/rb-parity-engine-launch.txt")=="B","engine_id_and_executable_working_directory");}
 else if(stage==34){RunTask launch;launch.kind=5;launch.engineProfileId="missing-id";launch.flags=RunAllowLaunchFailure;finishProfile({launch,{1,probe,"after-missing-engine"}});}
 else if(stage==35)expect(!runActive&&!runHadError&&runLog.find("PROBE_RAN")!=std::string::npos,"missing_engine_continues_when_configured");
 else if(stage==36){RunTask launch;launch.kind=5;launch.engineProfileId="missing-id";finishProfile({launch,{1,probe,"must-not-run"}});}
 else if(stage==37)expect(!runActive&&runHadError&&runStep==0,"missing_engine_stops_when_configured");
 else if(stage==38)finishProfile({{2,documentFile.u8string(),"source-map-copy",true,RunCopyToDirectory}});
 else if(stage==39)expect(!runActive&&!runHadError&&q1Read(root/"source-map-copy"/documentFile.filename())==q1Read(documentFile),"copy_source_map_is_allowed");
}
