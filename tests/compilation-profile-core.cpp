// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/profile/el.h"
#include "../native/profile/files.h"
#include "../native/profile/export.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace ftebroom::profile;
namespace fs=std::filesystem;
int main(){
 Json vars={{"CPU_COUNT",1},{"MODS",{"id1","modA","modB"}},{"GAME_DIR_PATH","D:/Game Root"}};
 assert(interpolate("${CPU_COUNT - 1}",vars)=="0");
 assert(interpolate("${GAME_DIR_PATH}/${MODS[0]}/${MODS[-1]}",vars)=="D:/Game Root/id1/modB");
 assert(interpolate("${(2 + 3) * 4}",vars)=="20");
 assert(interpolate("${[10, 20, 30][1]}",vars)=="20");
 bool unknown=false;try{interpolate("${missing}",vars);}catch(...){unknown=true;}assert(unknown);
 assert(parseConfig("// EL config\n{version:1,profiles:[]}").at("version")==1);
 auto args=parseArguments(R"("C:\Tools\map dir\test.map" tail)");assert(args.size()==2&&args[0]==R"(C:\Tools\map dir\test.map)");
 assert(filenameMatches("FILE.TMP","*.tmp"));
 auto root=fs::temp_directory_path()/("reyond-profile-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root/"cleanup/nested");
 std::ofstream(root/"seed.txt")<<"seed";std::ofstream(root/"cleanup/top.tmp")<<"remove";std::ofstream(root/"cleanup/nested/deep.TMP")<<"remove";std::ofstream(root/"cleanup/nested/keep.bsp")<<"keep";
 auto copy=planFiles(2,"seed.txt","copydir",root);auto remove=planFiles(4,"cleanup/*.tmp","",root);
 assert(copy.sources.size()==1&&remove.sources.size()==2);assert(!fs::exists(root/"copydir")); // Planning/Test does not write.
 executeFiles(copy);executeFiles(planFiles(3,"copydir/seed.txt","moved/renamed.txt",root));executeFiles(remove);
 auto missing=planFiles(2,"missing.bsp","no-matches",root);assert(missing.sources.empty());executeFiles(missing);assert(fs::is_directory(root/"no-matches"));
 executeFiles(planFiles(4,"cleanup/missing-file.bsp","",root));
 assert(fs::exists(root/"moved/renamed.txt")&&fs::exists(root/"cleanup/nested/keep.bsp")&&!fs::exists(root/"cleanup/nested/deep.TMP"));
 assert(planFiles(2,"cleanup/*.bsp","out",root).sources.empty()); // Copy is flat.
 bool absentParent=false;try{planFiles(4,"missing-dir/*.tmp","",root);}catch(...){absentParent=true;}assert(absentParent);
 std::string map="// quoted braces must not change nesting\n{\n\"classname\" \"worldspawn\"\n\"_tb_test\" \"{ value }\"\n\"_TB_keep\" \"keep\"\n}\n{\"classname\" \"Light_Spot\"}\n";
 auto exported=exportEntities(map,true,"light*",Json::array({{{"key","classname"},{"value","info_player_start"}},{{"key","origin"},{"value","1 2 3"}},{{"key","_tb_extra"},{"value","remove"}}}));
 assert(exported.find("_tb_test")==std::string::npos&&exported.find("Light_Spot")==std::string::npos&&exported.find("_tb_extra")==std::string::npos);
 assert(exported.find("_TB_keep")!=std::string::npos&&exported.find("info_player_start")!=std::string::npos&&exported.find("1 2 3")!=std::string::npos);
 assert(exportEntities(map,false,"",nullptr)==map);assert(exportEntities(map,false,"info*",Json::array({{{"key","classname"},{"value","info_player_start"}}})).find("info_player_start")==std::string::npos);
 fs::remove_all(root);std::cout<<"PASS typed EL/scopes, argv, original TB disk traversal and export-only options\n";
}
