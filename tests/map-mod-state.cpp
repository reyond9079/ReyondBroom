// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_map_mods.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace ftebroom;namespace fs=std::filesystem;
  const std::string vanilla="// Game: Quake\n{\n\"classname\" \"worldspawn\"\n{\n( 0 0 0 ) ( 0 1 0 ) ( 0 0 1 ) wall 0 0 0 1 1\n}\n}\n{\n\"classname\" \"info_player_start\"\n\"_tb_mod\" \"must-not-read-entity\"\n}\n";
  assert(mapModsFromSource(vanilla).empty());
  auto ad=mapSourceWithMods(vanilla,{"AD19b5"});assert(mapModsFromSource(ad)==std::vector<std::string>{"AD19b5"});
  assert(ad.find("\"_tb_mod\" \"AD19b5\"")!=ad.npos);assert(ad.find("( 0 0 0 )")!=ad.npos);assert(mapSourceWithMods(vanilla,{})==vanilla);
  assert((mapModsFromSource("\xef\xbb\xbf/*header*/ { \"classname\" \"worldspawn\" /*a*/ \"_tb_mod\" \"copper;AD19b5\" }")==std::vector<std::string>{"copper","AD19b5"}));
  assert((mapModsFromSource("{\"message\" \"{ not a brush }\" \"_tb_mod\" \"AD19b5\"}")==std::vector<std::string>{"AD19b5"}));
  assert((mapModList("copper;;AD19b5;")==std::vector<std::string>{"copper","AD19b5"}));
  assert(mapModString({"copper","AD19b5"})=="copper;AD19b5");
  auto paths=mapModResourcePaths(fs::u8path("/games/quake"),"id1",{"copper","AD19b5"});
  assert((paths==std::vector<fs::path>{"/games/quake/id1","/games/quake/copper","/games/quake/AD19b5"}));
  assert(mapModResourcePaths(fs::u8path("/games/quake"),"id1",{}).back()==fs::path("/games/quake/id1"));
  std::swap(paths[1],paths[2]);assert(paths.back()==fs::path("/games/quake/copper"));
  for(const auto& invalid:{"","bad;name","bad\nname","bad\tname"}){
    bool failed=false;try{validateMapMod(invalid);}catch(const std::exception&){failed=true;}assert(failed);
  }
  assert((mapModsFromSource("{\"_tb_mod\" \"mods/AD19b5;../shared\"}")==std::vector<std::string>{"mods/AD19b5","../shared"}));
  auto nested=mapModResourcePaths(fs::u8path("/games/quake"),"id1",{"mods/AD19b5"});assert(nested.back()==fs::path("/games/quake/mods/AD19b5"));
  // Parsing is frame-safe even for a hand-edited path which cannot be mounted;
  // mount validation provides the diagnostic without changing the list.
  assert(mapModList("bad\nname").front()=="bad\nname");
  // A map's explicit empty state must remain vanilla, even if the resource
  // defaults used for a *different new document* are a custom mod.
  assert(mapModsFromSource(vanilla).empty());assert(mapModsFromSource(mapSourceWithMods(vanilla,{"AD19b5"})).back()=="AD19b5");
  std::cout<<"PASS: per-map mod metadata, empty vanilla, new-document defaults, ordered resource paths and invalid names\n";
}
