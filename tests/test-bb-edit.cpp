// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_bb_anim.h"
#include "../native/ftebroom_bb_edit.h"
#include "../native/ftebroom_bb_pick.h"
#include "../native/ftebroom_bb_uv.h"
#include <iostream>
using namespace ftebroom::bb;
static void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}
int main() {
  try {
    Project p;
    Selection s;
    auto c = cube();
    p.root["elements"].push_back(c);
    appendNode(p, c["uuid"]);
    s.elements.insert(c["uuid"]);
    History h;
    auto initial = serialize(p);
    for (int i = 0; i < 50; i++) {
      h.begin(p, s);
      if (i % 5 == 0)
        duplicate(p, s);
      else if (i % 5 == 1)
        moveSelection(p, s, {1, 2, 3});
      else if (i % 5 == 2)
        groupSelection(p, s);
      else if (i % 5 == 3) {
        auto all = descendants(p, selectedTop(p, s));
        for (auto id : all)
          if (auto e = p.find(id))
            if (e->value("type", "") == "cube")
              autoUV(*e);
      } else {
        for (auto id : s.groups)
          if (auto e = p.find(id))
            (*e)["rotation"] = {0, i * 3, 0};
      }
      p.root["revision_note"] = i;
      h.finish(p, s, "Edit elements");
    }
    auto final = serialize(p);
    for (int i = 0; i < 50; i++)
      check(h.undo(p, s), "mixed operation undo");
    check(serialize(p) == initial, "50 mixed edits return to original");
    for (int i = 0; i < 50; i++)
      h.redo(p, s);
    if (serialize(p) != final)
      std::cerr << Json::diff(Json::parse(final), Json::parse(serialize(p))).dump(2) << "\n";
    check(serialize(p) == final, "50 mixed edits redo exact document");
    auto m = primitive(0);
    size_t vertices = m["vertices"].size(), faces = m["faces"].size();
    std::string selected = m["faces"].begin().key();
    extrude(m, {selected}, 1);
    check(m["vertices"].size() == vertices + 4 && m["faces"].size() == faces + 4,
          "quad extrude adds four vertices and faces");
    m = primitive(0);
    loopCut(m);
    check(m["vertices"].size() == 12, "cube loop cut adds four shared vertices");
    for (int kind = 0; kind <= 10; kind++) {
      Project q;
      auto e = primitive(kind);
      q.root["elements"].push_back(e);
      appendNode(q, e["uuid"]);
      validate(q);
      check(!triangles(q).empty(), "primitive produces valid visible geometry");
      auto tris = triangles(q);
      for (auto &triangle : tris)
        check(length(cross(sub(triangle.p[1], triangle.p[0]), sub(triangle.p[2], triangle.p[0]))) >
                  1e-8,
              "primitive non-degenerate triangle");
      if (kind != 3 && kind != 4) {
        std::map<std::pair<std::string, std::string>, std::pair<int, int>> edges;
        for (auto &f : e["faces"]) {
          auto vertices = f["vertices"].get<std::vector<std::string>>();
          for (size_t i = 0; i < vertices.size(); i++) {
            auto a = vertices[i], b = vertices[(i + 1) % vertices.size()];
            auto &counts = edges[std::minmax(a, b)];
            counts.first++;
            counts.second += a < b ? 1 : -1;
          }
        }
        bool closed = true;
        for (auto &item : edges)
          closed &= item.second.first == 2 && item.second.second == 0;
        check(closed, "closed primitive oriented manifold");
        double volume = 0;
        for (auto &triangle : tris)
          volume += dot(triangle.p[0], cross(triangle.p[1], triangle.p[2])) / 6;
        check(volume > 0, "closed primitive outward signed volume");
      }
    }
    p = Project();
    auto box = cube("box", {-1, -1, -1}, {1, 1, 1});
    p.root["elements"].push_back(box);
    appendNode(p, box["uuid"]);
    auto hit = pick(p, triangles(p), {{0, 0, -5}, {0, 0, 1}});
    check(hit.found && hit.face == "north" && std::abs(hit.uv[0] - 8) < 1e-8 &&
              std::abs(hit.uv[1] - 8) < 1e-8,
          "north ray picks center UV");
    auto t = textureTemplate(p, "skin.png", 16);
    check(t.width <= 2048 && p.textures.size() == 1, "texture template packed and assigned");
    return 0;
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
