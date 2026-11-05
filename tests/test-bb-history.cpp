// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_bb_anim.h"
#include "../native/ftebroom_bb_paint.h"
#include "../native/ftebroom_bb_tools.h"
#include <iostream>
using namespace ftebroom::bb;
int main() {
  try {
    auto p = makeTemplate(4, "history", 2);
    textureTemplate(p, "skin", 16);
    p.root["animations"] = {
        {{"uuid", uuid()}, {"name", "test"}, {"length", 5}, {"animators", Json::object()}}};
    Selection s;
    s.elements.insert(p.root["elements"][0]["uuid"]);
    History h;
    auto before = serialize(p);
    for (int i = 0; i < 50; i++) {
      h.begin(p, s, i % 8 == 5 ? std::vector<size_t>{0} : std::vector<size_t>{});
      switch (i % 8) {
      case 0: {
        auto c = cube("added");
        p.root["elements"].push_back(c);
        appendNode(p, c["uuid"]);
        s.elements.insert(c["uuid"]);
        break;
      }
      case 1:
        moveWorld(p, s, {1, .5, -2});
        break;
      case 2:
        groupSelection(p, s);
        break;
      case 3:
        p.root["groups"][0]["rotation"] = {i, 15, 20};
        break;
      case 4:
        for (auto &e : p.root["elements"])
          uvTransform(e, {}, {1, -1});
        break;
      case 5: {
        PaintOptions o;
        o.color = {static_cast<unsigned char>(i * 3), 100, 150, 255};
        dab(p.textures[0], i % 16, i / 16, o);
        break;
      }
      case 6:
        setKey(p.root["animations"][0], p.root["groups"][0]["uuid"], "rotation", i / 10.,
               {i, 0, 0});
        break;
      case 7:
        if (p.root["elements"].size() > 1) {
          Selection remove;
          remove.elements.insert(p.root["elements"].back()["uuid"]);
          eraseSelection(p, remove);
          s = {};
          s.groups.insert(p.root["groups"][0]["uuid"]);
        }
        break;
      }
      if (!h.finish(p, s, "Mixed edit"))
        throw std::runtime_error("Expected one meaningful history record");
    }
    auto after = serialize(p);
    for (int i = 0; i < 50; i++)
      if (!h.undo(p, s))
        throw std::runtime_error("Missing undo");
    if (serialize(p) != before)
      throw std::runtime_error("Mixed history failed to restore original geometry/images/keys");
    for (int i = 0; i < 50; i++)
      if (!h.redo(p, s))
        throw std::runtime_error("Missing redo");
    if (serialize(p) != after)
      throw std::runtime_error("Mixed history failed to reproduce final state");
    auto count = h.size();
    h.begin(p, s, {0});
    p.root["name"] = "cancelled";
    PaintOptions brush;
    dab(p.textures[0], 0, 0, brush);
    h.cancel(p, s);
    if (h.size() != count || serialize(p) != after)
      throw std::runtime_error("Escape altered history or pixels");
    std::cout << "PASS 50 geometry/group/UV/pixel/key/delete edits, exact Undo/Redo and Escape\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
