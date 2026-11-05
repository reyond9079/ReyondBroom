// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_bb_tools.h"
#include "../native/ftebroom_bb_uv.h"
#include <iostream>
using namespace ftebroom::bb;
void check(bool ok, const char *name) {
  if (!ok)
    throw std::runtime_error(name);
  std::cout << "PASS " << name << '\n';
}
void valid(const Json &mesh) {
  Project p;
  p.root["elements"].push_back(mesh);
  appendNode(p, mesh["uuid"]);
  validate(p);
}
bool closed(const Json &mesh) {
  std::map<std::pair<std::string, std::string>, std::pair<int, int>> edges;
  for (auto &f : mesh["faces"]) {
    auto v = f["vertices"].get<std::vector<std::string>>();
    for (size_t i = 0; i < v.size(); i++) {
      auto a = v[i], b = v[(i + 1) % v.size()];
      auto &edge = edges[std::minmax(a, b)];
      edge.first++;
      edge.second += a < b ? 1 : -1;
    }
  }
  for (auto &edge : edges)
    if (edge.second.first != 2 || edge.second.second)
      return false;
  return true;
}
int main() {
  try {
    for (bool inset : {false, true}) {
      auto mesh = primitive(0);
      auto faceId = mesh["faces"].begin().key();
      extrude(mesh, {faceId}, inset ? .2 : 2, inset);
      valid(mesh);
      check(closed(mesh),
            inset ? "inset retains closed manifold" : "extrude retains closed manifold");
    }
    for (int axis = 0; axis < 3; axis++) {
      auto mesh = primitive(0);
      loopCut(mesh, axis);
      valid(mesh);
      check(mesh["vertices"].size() == 12 && closed(mesh),
            "loop cut keeps shared manifold on each axis");
    }
    auto mesh = primitive(0);
    auto first = mesh["vertices"].begin().key();
    auto duplicate = vertex(mesh, vec(mesh["vertices"][first]));
    mergeVertices(mesh, {first, duplicate}, 0, .001);
    valid(mesh);
    check(mesh["vertices"].size() == 8, "merge by distance welds coincident vertices");
    for (int mode : {1, 2}) {
      mesh = primitive(0);
      auto a = mesh["vertices"].begin().key(), b = std::next(mesh["vertices"].begin()).key();
      auto pa = vec(mesh["vertices"][a]), pb = vec(mesh["vertices"][b]);
      mergeVertices(mesh, {a, b}, mode, .1, b);
      valid(mesh);
      auto target = mode == 2 ? pb : mul(add(pa, pb), .5);
      bool found = false;
      for (auto &v : mesh["vertices"])
        found |= length(sub(vec(v), target)) < 1e-8;
      check(mesh["vertices"].size() == 7 && found,
            mode == 2 ? "merge to last preserves target"
                      : "merge to center averages selected vertices");
    }
    mesh = primitive(0);
    auto faceId = mesh["faces"].begin().key();
    splitFaces(mesh, {faceId});
    valid(mesh);
    check(mesh["vertices"].size() == 12 && mesh["faces"].size() == 6,
          "split duplicates only face vertices");
    mesh = primitive(3);
    solidify(mesh, 2);
    valid(mesh);
    check(mesh["vertices"].size() == 8 && mesh["faces"].size() == 6 && closed(mesh),
          "solidify open plane makes closed six-sided shell");
    mesh = primitive(3);
    auto v = mesh["faces"].begin().value()["vertices"].get<std::vector<std::string>>();
    mesh["faces"] = Json::object();
    face(mesh, {v[0], v[1], v[2]});
    face(mesh, {v[0], v[2], v[3]});
    dissolveEdges(mesh, {edgeKey(v[0], v[2])});
    valid(mesh);
    check(mesh["faces"].size() == 1 && mesh["faces"].begin().value()["vertices"].size() == 4,
          "dissolve selected diagonal reconstructs quad");
    mesh["faces"] = Json::object();
    createFace(mesh, {v[0], v[1], v[2]});
    valid(mesh);
    check(mesh["faces"].size() == 1, "create face from three selected vertices");
    Project p;
    mesh = primitive(0);
    p.root["elements"].push_back(mesh);
    appendNode(p, mesh["uuid"]);
    Selection selected;
    std::string id = mesh["uuid"];
    selected.elements.insert(id);
    selected.faces[id].insert(mesh["faces"].begin().key());
    deleteComponents(p, selected, 1);
    check(p.root["elements"][0]["faces"].size() == 5, "delete face retains remaining mesh");
    validate(p);
    mesh = primitive(3);
    auto before = mesh["faces"];
    for (int i = 0; i < 4; i++)
      uvAction(mesh, {}, "Rotate Right", 16, 16);
    double error = 0;
    for (auto it = mesh["faces"].begin(); it != mesh["faces"].end(); ++it)
      for (auto uv = it.value()["uv"].begin(); uv != it.value()["uv"].end(); ++uv) {
        Vec2 a = uv.value(), b = before[it.key()]["uv"][uv.key()];
        error = std::max(error, std::hypot(a[0] - b[0], a[1] - b[1]));
      }
    check(error < 1e-8, "four mesh UV quarter-turns preserve original mapping");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
