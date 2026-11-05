// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_bb_geometry.h"
#include "../native/ftebroom_bb_undo.h"
#include <fstream>
#include <iostream>
using namespace ftebroom::bb;
static void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}

static void fixture(const std::string &name) {
  Project p;
  std::ifstream jf("tests/fixtures/bb/" + name + ".bbmodel");
  Json j;
  jf >> j;
  p.root.update(j);
  auto ts = triangles(p);
  std::vector<Vec3> vs;
  std::vector<Vec2> uv;
  std::vector<std::array<std::pair<Vec3, Vec2>, 3>> expected;
  std::ifstream obj("tests/fixtures/bb/" + name + ".obj");
  std::string line;
  while (std::getline(obj, line)) {
    std::istringstream in(line);
    std::string type;
    in >> type;
    if (type == "v") {
      Vec3 v;
      in >> v[0] >> v[1] >> v[2];
      vs.push_back(v);
    } else if (type == "vt") {
      Vec2 v;
      in >> v[0] >> v[1];
      v[1] = 1 - v[1];
      uv.push_back(v);
    } else if (type == "f") {
      std::array<std::pair<Vec3, Vec2>, 3> t;
      for (auto &v : t) {
        std::string word;
        in >> word;
        int a = std::stoi(word) - 1, b = std::stoi(word.substr(word.find('/') + 1)) - 1;
        v = {vs.at(a), uv.at(b)};
      }
      expected.push_back(t);
    }
  }
  check(ts.size() == expected.size(), ("Blockbench fixture triangle count " + name).c_str());
  std::vector<bool> used(expected.size());
  for (auto &t : ts) {
    bool found = false;
    for (size_t n = 0; n < expected.size() && !found; n++)
      if (!used[n])
        for (int offset = 0; offset < 3 && !found; offset++) {
          bool same = true;
          for (int k = 0; k < 3; k++) {
            auto &v = expected[n][(k + offset) % 3];
            auto tex = t.texture >= 0 ? p.root["textures"][t.texture] : Json::object();
            double w = tex.value("uv_width", p.root["resolution"].value("width", 64)),
                   h = tex.value("uv_height", p.root["resolution"].value("height", 64));
            if (length(sub(t.p[k], v.first)) > 1e-4 ||
                std::abs(t.uv[k][0] / w - v.second[0]) > 1e-4 ||
                std::abs(t.uv[k][1] / h - v.second[1]) > 1e-4)
              same = false;
          }
          if (same) {
            found = true;
            used[n] = true;
          }
        }
    if (!found) {
      std::cerr << "Mismatch " << name << " face " << t.face << " point " << t.p[0][0] << ","
                << t.p[0][1] << "," << t.p[0][2] << " uv " << t.uv[0][0] << "," << t.uv[0][1]
                << '\n';
      throw std::runtime_error("Blockbench OBJ mismatch");
    }
  }
  check(true, ("Blockbench vertices UV winding " + name).c_str());
}
int main() {
  try {
    for (auto n : {"rotated_cubes", "box_uv_mirror", "nested_groups", "mesh_mixed"})
      fixture(n);
    Project p;
    auto c = cube("cube", {-4, 0, -2}, {4, 12, 2});
    auto id = c["uuid"].get<std::string>();
    p.root["elements"].push_back(c);
    appendNode(p, id);
    validate(p);
    auto ts = triangles(p);
    check(ts.size() == 12, "cube twelve triangles");
    for (auto &t : ts) {
      auto center = mul(add(add(t.p[0], t.p[1]), t.p[2]), 1. / 3);
      check(dot(t.n[0], sub(center, {0, 6, 0})) > 0, "cube outward normal");
    }
    auto a = bbToQuake({0, 0, -1});
    check(length(sub(a, {1, 0, 0})) < 1e-9, "north to Quake positive X");
    check(dot(cross(bbToQuake({1, 0, 0}), bbToQuake({0, 1, 0})), bbToQuake({0, 0, 1})) > 0,
          "coordinate conversion determinant positive");
    check(length(sub(quakeToBb(bbToQuake({1, 2, 3})), {1, 2, 3})) < 1e-9, "coordinate roundtrip");
    auto m = toMesh(c);
    p.root["elements"][0] = m;
    check(triangles(p).size() == 12, "convert cube preserves triangles");
    Selection s;
    History h;
    auto initial = p.root;
    for (int i = 0; i < 50; i++) {
      h.begin(p, s);
      p.root["elements"][0]["origin"] = {i + 1, 0, 0};
      h.finish(p, s, "Move elements");
    }
    auto end = p.root;
    for (int i = 0; i < 50; i++)
      check(h.undo(p, s), "undo operation");
    check(p.root == initial, "fifty undo restores original");
    for (int i = 0; i < 50; i++)
      h.redo(p, s);
    check(p.root == end, "fifty redo restores final");
    h.begin(p, s);
    p.root["name"] = "temporary";
    h.cancel(p, s);
    check(p.root == end, "Escape cancels without history");
    return 0;
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
