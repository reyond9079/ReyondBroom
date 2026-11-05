// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_bb_io.h"
#include <iostream>
using namespace ftebroom::bb;
int main() {
  try {
    for (auto n :
         {"rotated_cubes", "box_uv_mirror", "nested_groups", "mesh_mixed", "anim_all_interp"}) {
      auto p = parseProject(
          readFile(std::filesystem::path("tests/fixtures/bb") / (std::string(n) + ".bbmodel")));
      p.root["unknown_field"] = {{"nested", 123}};
      p.root["elements"][0]["future_option"] = 42;
      auto q = parseProject(serialize(p));
      if (q.root["unknown_field"] != p.root["unknown_field"] ||
          q.root["elements"][0]["future_option"] != 42 ||
          q.textures[0].rgba != p.textures[0].rgba || serialize(q) != serialize(p))
        throw std::runtime_error("Roundtrip mismatch");
      std::cout << "PASS project PNG unknown fields roundtrip " << n << '\n';
    }
    for (auto text :
         {std::string("{"), std::string("{\"meta\":{},\"elements\":[],\"outliner\":[\"missing\"]}"),
          std::string("{\"meta\":{},\"textures\":[{\"width\":999999999}]} ")}) {
      bool rejected = false;
      try {
        parseProject(text);
      } catch (...) {
        rejected = true;
      }
      if (!rejected)
        throw std::runtime_error("Invalid project accepted");
    }
    std::cout << "PASS malformed JSON, UUID and oversized image rejected\n";
    Project blank;
    auto encoded = Json::parse(serialize(blank));
    if (!encoded["editor_state"]["selected_elements"].is_array())
      throw std::runtime_error("Reference codec requires selected_elements");
    std::cout << "PASS reference codec editor_state schema\n";
    auto external = parseProject(readFile("tests/fixtures/bb/rotated_cubes.bbmodel"));
    external.textures[0].path = "textures/new-source.png";
    auto refreshed = parseProject(serialize(external));
    if (refreshed.textures[0].path != "textures/new-source.png")
      throw std::runtime_error("Stale refresh path after Save as");
    std::cout << "PASS texture source path survives Save as and project reload\n";
    return 0;
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
