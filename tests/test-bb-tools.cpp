// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_bb_anim.h"
#include "../native/ftebroom_bb_image_ops.h"
#include "../native/ftebroom_bb_obj.h"
#include "../native/ftebroom_bb_tools.h"
#include <iostream>
using namespace ftebroom::bb;
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
int main() {
  try {
    check(std::abs(expression("8/3", 0) - 8. / 3) < 1e-8 && expression("*2", 3) == 6 &&
              expression("+2", 3) == 5,
          "numeric expressions");
    auto p = makeTemplate(4, "prop", 2);
    auto id = p.root["elements"][0]["uuid"].get<std::string>();
    p.root["elements"][0] = toMesh(p.root["elements"][0]);
    auto &m = p.root["elements"][0];
    std::set<std::string> edges;
    for (auto &f : m["faces"]) {
      auto v = f["vertices"].get<std::vector<std::string>>();
      for (size_t i = 0; i < v.size(); i++)
        edges.insert(edgeKey(v[i], v[(i + 1) % v.size()]));
    }
    for (auto e : edges)
      m["seams"][e] = "divide";
    check(unfold(m) == 6, "six explicitly divided cube UV islands");
    for (auto &f : m["faces"]) {
      auto v = f["vertices"].get<std::vector<std::string>>();
      for (size_t i = 0; i < v.size(); i++) {
        Vec2 a = f["uv"][v[i]], b = f["uv"][v[(i + 1) % v.size()]];
        double uv = std::hypot(a[0] - b[0], a[1] - b[1]),
               distance =
                   length(sub(vec(m["vertices"][v[i]]), vec(m["vertices"][v[(i + 1) % v.size()]])));
        check(std::abs(uv - distance) < 1e-6, "unfold preserves edge lengths");
      }
    }
    auto parent = parents(p)[id];
    p.find(parent)->at("rotation") = {10, 35, 15};
    Selection s;
    s.elements.insert(id);
    s.vertices[id].insert(m["vertices"].begin().key());
    auto key = *s.vertices[id].begin();
    auto before = objectMatrix(p, id).point(vec(m["vertices"][key]));
    moveWorld(p, s, {2, 3, -1});
    auto after = objectMatrix(p, id).point(vec(p.find(id)->at("vertices")[key]));
    check(length(sub(sub(after, before), {2, 3, -1})) < 1e-6,
          "world vertex motion through rotated parent");
    (*p.find(parent))["locked"] = true;
    auto unchanged = p.root;
    moveWorld(p, s, {1, 2, 3});
    check(p.root == unchanged, "ancestor lock prevents transforms");
    auto chain = parseProject(readFile("tests/fixtures/bb/nested_groups.bbmodel"));
    Selection groupSelection_;
    groupSelection_.groups.insert(chain.root["groups"][0]["uuid"]);
    auto initial = triangles(chain);
    moveWorld(chain, groupSelection_, {2, -3, 4});
    auto moved = triangles(chain);
    double maximum = 0;
    for (size_t i = 0; i < initial.size(); i++)
      for (int n = 0; n < 3; n++)
        maximum = std::max(maximum, length(sub(sub(moved[i].p[n], initial[i].p[n]), {2, -3, 4})));
    check(maximum < 1e-6, "world group move preserves rotated descendant offsets");
    auto pivot = getv(chain.root["groups"][0], "origin");
    initial = triangles(chain);
    pivotObject(chain, chain.root["groups"][0]["uuid"], add(pivot, {1, 2, 3}));
    moved = triangles(chain);
    maximum = 0;
    for (size_t i = 0; i < initial.size(); i++)
      for (int n = 0; n < 3; n++)
        maximum = std::max(maximum, length(sub(moved[i].p[n], initial[i].p[n])));
    check(maximum < 1e-6, "group pivot preserves nested geometry");
    auto target = group("target", {10, 5, -3});
    target["rotation"] = {30, 10, 15};
    std::string targetId = target["uuid"];
    chain.root["groups"].push_back(target);
    appendNode(chain, targetId, "", true);
    initial = triangles(chain);
    std::string movedGroup = chain.root["groups"][0]["uuid"];
    reparent(chain, {movedGroup}, targetId);
    moved = triangles(chain);
    maximum = 0;
    for (size_t i = 0; i < initial.size(); i++)
      for (int n = 0; n < 3; n++)
        maximum = std::max(maximum, length(sub(moved[i].p[n], initial[i].p[n])));
    check(maximum < 1e-6, "reparent rotated subtree preserves world geometry");
    auto obj = importObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nf "
                         "-4/1 -3/2 -2/3 -1/4\n");
    check(obj.root["elements"][0]["faces"].size() == 1 && triangles(obj).size() == 2,
          "OBJ negative indices and quad corners");
    auto legacy = parseProject(readFile("tests/fixtures/bb/legacy_4_0.bbmodel"));
    auto expected = parseProject(readFile("tests/fixtures/bb/legacy_4_0.expected.json"));
    for (size_t i = 0; i < legacy.root["animations"].size(); i++) {
      auto &a = legacy.root["animations"][i];
      auto &b = expected.root["animations"][i];
      for (auto it = a["animators"].begin(); it != a["animators"].end(); ++it)
        for (double time : {0., .1, .25, .5, .8, 1.})
          check(length(sub(channel(it.value(), "rotation", time),
                           channel(b["animators"][it.key()], "rotation", time))) < .001,
                "legacy migration matches actual Blockbench");
    }
    check(serialize(parseProject(serialize(legacy))) == serialize(legacy),
          "legacy migration happens exactly once");
    Texture image;
    image.width = image.height = 4;
    image.uvWidth = image.uvHeight = 4;
    image.rgba.assign(4 * 4 * 4, 255);
    auto cropped = cropImage(image, {1, 1, 3, 4});
    check(cropped.width == 2 && cropped.height == 3, "pixel selection crop");
    adjustImage(image, -.2, 1, 0, 1, .5);
    check(pixel(image, 0, 0) == Pixel{204, 204, 204, 127}, "brightness and opacity adjustment");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
