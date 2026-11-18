// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_jam_scene.h"
#include <iostream>
#include <stdexcept>
using Json=nlohmann::json;
namespace jam=ftebroom::jam;
void check(bool value,const char* name){if(!value)throw std::runtime_error(name);std::cout<<"PASS "<<name<<'\n';}
int main()
{
  try {
    Json state={{"plots",Json::array({{{"plot","A1"},{"mode","map"},{"revision","self"}},
      {{"plot","A2"},{"mode","map"},{"revision","old"}},{{"plot","A3"},{"mode","bsp"},{"revision","stable"}}})}};
    const Json face=Json::array({"wall_pink_b",Json::array({{0,0,1008,0,0},{64,0,1008,1,0},{64,64,1008,1,1}})});
    std::map<std::string,jam::WorldPreview> cache{{"A2",{"old",{},Json::array({face})}},
      {"A3",{"stable","maps/stable.bsp",Json::array()}}};
    const auto scene=[&]{return jam::worldScene(state,"A1","maps/hub.bsp",cache);};
    const auto key=[&]{return jam::worldSceneKey(state,"A1","maps/hub.bsp",cache);};
    const auto before=scene(),beforeKey=key();
    check(before.find("2048 0 1008")!=std::string::npos,"draft_coordinates_include_new_height_and_plot_offset");
    check(before.find("model maps/stable.bsp 4096 0")!=std::string::npos,"unchanged_bsp_and_hub_render_together");
    for (int revision=0;revision<20;++revision) {
      state["plots"][1]["revision"]="announced-"+std::to_string(revision);
      check(scene()==before && key()==beforeKey,"announcing_new_draft_keeps_complete_scene_while_download_is_pending");
    }
    cache["A2"].revision="announced-19";cache["A2"].faces[0][1][0][2]=1024;
    check(scene()!=before && key()!=beforeKey,"complete_draft_replaces_previous_preview");
    const auto updated=scene(),updatedKey=key();
    state["plots"][1]["mode"]="bsp";state["plots"][1]["revision"]="publication";
    check(scene()==updated && key()==updatedKey,"map_to_bsp_retains_draft_until_download_completes");
    cache["A2"]={"publication","maps/published.bsp",Json::array()};
    check(scene().find("model maps/published.bsp 2048 0")!=std::string::npos,"published_bsp_activates_when_ready");
    const auto published=scene();
    state["plots"][1]["mode"]="map";state["plots"][1]["revision"]=nullptr;
    check(scene()==published,"return_to_editing_keeps_bsp_before_first_draft");
    cache["A2"]={"empty-draft",{},Json::array()};
    check(scene().find("published.bsp")==std::string::npos && scene().find("face ")==std::string::npos,
          "complete_empty_draft_removes_previous_geometry");
    cache["A2"]={"new-draft",{},Json::array({face})};
    state["plots"][1]["mode"]="empty";
    check(scene().find("face ")==std::string::npos,"explicit_empty_or_moderated_plot_does_not_keep_stale_geometry");
    state["plots"].erase(state["plots"].begin()+1);
    check(scene().find("face ")==std::string::npos,"removed_plot_does_not_reappear_from_cache");
    check(scene().find("RBJAMSCENE 1 0\n")==0 && scene().find("RBJAMSCENE-END 0\n")!=std::string::npos,
          "scene_has_complete_transaction_markers");
    check(jam::worldHalfSize({{"bounds",{{"maxSize",{2048,2048,2048}}}}})==std::array<float,3>{1024,1024,1024},
          "cubic_rules_apply_to_all_three_axes");
    check(jam::worldHalfSize({{"bounds",{{"maxSize",{2048,2048,1024}}}}})==std::array<float,3>{1024,1024,512},
          "legacy_server_height_is_respected");
    for (const auto& bad:Json::array({nullptr,Json::array({2048,2048}),Json::array({2048,2048,999999}),
                                     Json::array({2048,2048,"invalid"}),Json::array({2048,2048,-1})}))
      check(jam::worldHalfSize({{"bounds",{{"maxSize",bad}}}})==std::array<float,3>{1024,1024,512},
            "invalid_rules_do_not_expand_editable_volume");
    return 0;
  } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
