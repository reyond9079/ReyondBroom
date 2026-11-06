// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_run_profile.h"
#include "ftebroom_run_variables.h"
#include "profile/el.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
struct RunImport {
 std::vector<RunProfile> profiles;
 std::vector<std::string> notices;
 std::map<std::string,std::string> missingVariables;
 std::vector<RunEngineProfile> engines;
};
static nlohmann::json runImportRead(const std::filesystem::path& path) {
 if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>4*1024*1024)
  throw std::runtime_error("Choose a readable TrenchBroom profile file up to 4 MiB.");
 std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Could not read "+path.filename().u8string());
 auto config=ftebroom::profile::parseConfig(std::string(std::istreambuf_iterator<char>(input),{}));
 if(config.at("version")!=1&&config.at("version")!=2)throw std::runtime_error("Unsupported TrenchBroom profile version.");
 if(!config.at("profiles").is_array())throw std::runtime_error("Expected a list of profiles.");return config;
}
static std::string runImportText(std::string value,std::map<std::string,std::string>& required,const std::map<std::string,std::string>& existing) {
 // Import preserves expressions verbatim. Only identify bare custom variables for
 // the optional tool-path review; evaluation (including errors) belongs to Run/Test.
 static const std::set<std::string> builtin={"GAME_DIR_PATH","MODS","MAP_BASE_NAME","MAP_FULL_NAME","MAP_DIR_PATH","CPU_COUNT","WORK_DIR_PATH","APP_DIR_PATH"};
 for(size_t start=0;(start=value.find("${",start))!=value.npos;) {
  auto end=value.find('}',start+2);if(end==value.npos)break;
  auto name=value.substr(start+2,end-start-2);
  if(!name.empty()&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")==name.npos&&!builtin.count(name)){
   auto old=existing.find(name);if(old==existing.end()||old->second.empty())required.try_emplace(name);
  }
  start=end+1;
 }
 return value;
}
static RunProfile runProfileFromJson(const nlohmann::json& entry) {
 RunProfile p;p.name=entry.at("name").get<std::string>();p.work=entry.at("workdir").get<std::string>();p.trenchBroom=entry.value("trenchBroom",true);p.launch=entry.value("legacyLaunch",false);
 for(const auto& s:entry.at("tasks")) {
  RunTask t;t.enabled=s.value("enabled",true);auto type=s.at("type").get<std::string>();
  if(type=="export"){t.source=s.at("target");t.stripTbProperties=s.value("stripTbProperties",false);t.stripEntityPattern=s.value("stripEntityPattern",std::string{});if(s.contains("entityToAdd"))t.entityToAdd=s.at("entityToAdd");}
  else if(type=="copy"||type=="rename"){t.kind=type=="copy"?2:3;t.source=s.at("source");t.destination=s.at("target");if(t.kind==2)t.flags|=RunCopyToDirectory;}
  else if(type=="delete"){t.kind=4;t.source=s.at("target");}
  else if(type=="tool"){t.kind=1;t.source=s.at("tool");t.destination=s.at("parameters");if(!s.value("treatNonZeroResultCodeAsError",false))t.flags|=RunAllowNonzero;}
  else if(type=="launchEngine"){t.kind=5;t.engineProfileId=s.at("engineProfileId");if(!s.value("treatLaunchFailureAsError",false))t.flags|=RunAllowLaunchFailure;}
  else throw std::runtime_error("Unsupported compilation task: "+type);
  if(s.contains("legacyFlags"))t.flags=s.at("legacyFlags").get<int>();
  if(s.contains("legacyDestination"))t.destination=s.at("legacyDestination").get<std::string>();
  p.tasks.push_back(std::move(t));
 }
 return p;
}
static nlohmann::json runProfileToJson(const RunProfile& p) {
 using J=nlohmann::json;J out={{"name",p.name},{"workdir",p.work},{"trenchBroom",p.trenchBroom},{"legacyLaunch",p.launch},{"tasks",J::array()}};
 for(const auto& t:p.tasks){J s={{"enabled",t.enabled}};
  if(t.kind==0){s["type"]="export";s["target"]=t.source;s["stripTbProperties"]=t.stripTbProperties;if(!t.stripEntityPattern.empty())s["stripEntityPattern"]=t.stripEntityPattern;if(!t.entityToAdd.is_null())s["entityToAdd"]=t.entityToAdd;}
  else if(t.kind==1){s["type"]="tool";s["tool"]=t.source;s["parameters"]=t.destination;s["treatNonZeroResultCodeAsError"]=!(t.flags&RunAllowNonzero);}
  else if(t.kind==2||t.kind==3){s["type"]=t.kind==2?"copy":"rename";s["source"]=t.source;s["target"]=t.destination;}
  else if(t.kind==4){s["type"]="delete";s["target"]=t.source;}
  else{s["type"]="launchEngine";s["engineProfileId"]=t.engineProfileId;s["treatLaunchFailureAsError"]=!(t.flags&RunAllowLaunchFailure);}
  if(!p.trenchBroom){s["legacyFlags"]=t.flags;s["legacyDestination"]=t.destination;}
  out["tasks"].push_back(std::move(s));
 }return out;
}
static RunImport runReadTrenchBroom(const std::filesystem::path& path,const std::map<std::string,std::string>& existing) {
 auto config=runImportRead(path);RunImport out;
 auto engineFile=path.parent_path()/"GameEngineProfiles.cfg";
 if(std::filesystem::exists(engineFile))try{
  auto engines=runImportRead(engineFile);size_t i=0;
  for(const auto& e:engines.at("profiles"))out.engines.push_back({e.value("id","imported-engine-"+std::to_string(++i)),e.at("name"),e.at("path"),e.at("parameters")});
 }catch(const std::exception& e){out.notices.push_back("GameEngineProfiles.cfg: "+std::string(e.what()));}
 for(const auto& entry:config.at("profiles"))try{
  auto p=runProfileFromJson(entry);
  runImportText(p.work,out.missingVariables,existing);
  for(const auto& t:p.tasks){runImportText(t.source,out.missingVariables,existing);runImportText(t.destination,out.missingVariables,existing);}
  out.profiles.push_back(std::move(p));
 }catch(const std::exception& e){out.notices.push_back(entry.value("name",std::string{})+": "+e.what());}
 return out;
}
