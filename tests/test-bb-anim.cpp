// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_bb_anim.h"
#include "../native/ftebroom_bb_io.h"
#include <iostream>
using namespace ftebroom::bb;
int main() {
  try {
    auto p = parseProject(readFile("tests/fixtures/bb/anim_all_interp.bbmodel"));
    auto samples = Json::parse(readFile("tests/fixtures/bb/anim_all_interp.samples.json"));
    for (auto &a : p.root["animations"]) {
      auto track = a["animators"].begin().value();
      std::string name = a["name"];
      for (auto &sample : samples[name]) {
        auto actual = channel(track, "rotation", sample["time"]);
        auto expected = vec(sample["value"]);
        if (length(sub(actual, expected)) > .01) {
          std::cerr << name << " at " << sample["time"] << " actual=" << actual[2]
                    << " expected=" << expected[2] << '\n';
          throw std::runtime_error("Blockbench interpolation mismatch");
        }
      }
      std::cout << "PASS real Blockbench interpolation " << name << '\n';
    }
    auto poses = Json::parse(readFile("tests/fixtures/bb/anim_all_interp.poses.json"));
    for (auto &animation : p.root["animations"]) {
      auto name = animation.value("name", "");
      for (auto &sample : poses[name]) {
        auto geometry = triangles(p, false, animationPose(animation, sample["time"]));
        for (auto &point : sample["vertices"]) {
          auto expected = vec(point);
          double nearest = 1e30;
          for (auto &t : geometry)
            for (auto v : t.p)
              nearest = std::min(nearest, length(sub(v, expected)));
          if (nearest > .001) {
            std::cerr << name << " t=" << sample["time"] << " error=" << nearest << '\n';
            throw std::runtime_error("Actual glTF world pose mismatch");
          }
        }
      }
      std::cout << "PASS actual glTF world poses " << name << '\n';
    }
    auto monster = makeTemplate(1, "monster", 2);
    if (bakeFrames(monster).size() != 40)
      throw std::runtime_error("Monster frame count");
    if (bakeFrames(makeTemplate(2, "v_weapon", 2)).size() != 8)
      throw std::runtime_error("Weapon frame count");
    std::cout << "PASS monster 40 and weapon 8 baked frames\n";
    return 0;
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
