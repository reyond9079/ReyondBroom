"""Exercise actual compilation workspace read/write functions across restarts."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
store = (root / "native/ftebroom_run_store.h").read_text()
profiles = (root / "native/ftebroom_run_import_data.h").read_text()


def function(source, signature):
    start = source.index(signature)
    begin = source.index("{", start)
    end, depth = begin + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


source = r'''
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
'''
source += '#include "' + str(root / "native/ftebroom_run_profile.h") + '"\n'
source += r'''
static std::vector<RunProfile> runProfiles;
static std::map<std::string,std::string> runVariables;
static int runSelected=-1,gameProfile=2;
static std::string runError;
static std::filesystem::path testDirectory;
static std::filesystem::path documentPath(const char* name){return testDirectory/name;}
'''
source += store[store.index("static std::vector<RunEngineProfile>"):store.index("static nlohmann::json runWorkspaceJson()")]
for signature in ("static RunProfile runProfileFromJson(", "static nlohmann::json runProfileToJson("):
    source += function(profiles, signature) + "\n"
for signature in ("static nlohmann::json runWorkspaceJson(", "static void runReadWorkspace(", "static bool runWriteStore("):
    source += function(store, signature) + "\n"
source += r'''
int main(int argc,char** argv){
 assert(argc==2);testDirectory=argv[1];
 const std::string first="${MAP_DIR_PATH}/first compile",second="D:/maps/custom compile folder";
 runProfiles={{"Duplicate name",first,false,{{0,"${WORK_DIR_PATH}/${MAP_BASE_NAME}.map",""}}},
              {"Duplicate name",second,false,{{1,"${qbsp}","${MAP_BASE_NAME}.map"}}}};
 runProfiles[0].trenchBroom=runProfiles[1].trenchBroom=true;
 runSelected=1;runStoreGame="quake1";runStore={{"version",1},{"games",nlohmann::json::object()}};
 runVariables={{"qbsp","D:/tools/compiler with spaces.exe"}};
 runEngines={{"vk","VKQuake","D:/games/vkquake.exe","+map ${MAP_BASE_NAME}","",""}};
 assert(runWriteStore());assert(runStoreRevision==1);
 nlohmann::json disk;{std::ifstream input(documentPath("compilation-profiles.json"));input>>disk;}
 assert(disk["games"]["quake1"]["selected"]==1);
 // Simulate a fresh process: preserve only the on-disk workspace.
 runProfiles.clear();runEngines.clear();runVariables.clear();runSelected=-1;
 runReadWorkspace(disk["games"]["quake1"]);
 assert(runSelected==1&&runProfiles.at(runSelected).work==second);
 assert(runProfiles.at(0).work==first&&runProfiles.at(1).tasks.at(0).source=="${qbsp}");
 assert(runVariables.at("qbsp")=="D:/tools/compiler with spaces.exe");
 assert(runEngines.at(0).parameters=="+map ${MAP_BASE_NAME}");
 auto workspace=disk["games"]["quake1"];
 auto legacy=workspace;legacy.erase("selected");runSelected=1;runReadWorkspace(legacy);assert(runSelected==0);
 for(const auto& invalid:nlohmann::json::array({-1,99,nullptr,"1",true,1.5,UINT64_MAX})){
   auto value=workspace;value["selected"]=invalid;runReadWorkspace(value);assert(runSelected==0);
 }
 auto empty=workspace;empty["profiles"]=nlohmann::json::array();runReadWorkspace(empty);assert(runSelected==-1);
 // Each game's selection stays with that game's profiles and exact workdir.
 gameProfile=3;runStoreGame="quake2";runReadWorkspace(workspace);runSelected=0;
 runProfiles[0].work="${MAP_DIR_PATH}/Q2 custom output";assert(runWriteStore());
 {std::ifstream input(documentPath("compilation-profiles.json"));input>>disk;}
 assert(disk["games"]["quake1"]["selected"]==1&&disk["games"]["quake2"]["selected"]==0);
 runReadWorkspace(disk["games"]["quake2"]);assert(runSelected==0&&runProfiles[0].work=="${MAP_DIR_PATH}/Q2 custom output");
 gameProfile=2;runReadWorkspace(disk["games"]["quake1"]);assert(runSelected==1&&runProfiles[1].work==second);
 std::cout<<"PASS actual workspace file restart preserves selected profile/custom workdir, duplicate names and per-game selections; legacy/invalid/empty selections remain compatible\n";
}
'''
with tempfile.TemporaryDirectory(prefix="rb-workspace-selection-") as directory:
    src = Path(directory) / "selection.cpp"
    exe = Path(directory) / ("selection.exe" if os.name == "nt" else "selection")
    src.write_text(source)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe), directory], check=True)
