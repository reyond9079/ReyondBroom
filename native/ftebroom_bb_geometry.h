// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_model.h"
namespace ftebroom::bb {
struct Pose {
  Vec3 position{}, rotation{}, scale{1, 1, 1};
};
struct Triangle {
  std::string element, face;
  std::array<std::string, 3> keys;
  std::array<Vec3, 3> p, n;
  std::array<Vec2, 3> uv;
  int texture = -1, color = 0;
  bool shade = true;
};
inline std::map<std::string, Mat4> groupMatrices(const Project &p,
                                                 const std::map<std::string, Pose> &poses = {}) {
  auto parent = parents(p);
  std::map<std::string, Mat4> out;
  out[""] = Mat4::identity();
  std::function<Mat4(std::string, int)> resolve = [&](std::string id, int depth) {
    if (out.count(id))
      return out[id];
    if (depth > 512)
      throw std::runtime_error("Cyclic group hierarchy");
    auto g = p.find(id);
    if (!g)
      throw std::runtime_error("Missing parent group");
    auto origin = getv(*g, "origin"), r = getv(*g, "rotation");
    Pose pose;
    if (poses.count(id))
      pose = poses.at(id);
    return out[id] = resolve(parent[id], depth + 1) * translate(add(origin, pose.position)) *
                     rotation(add(r, pose.rotation)) * scale(pose.scale) *
                     translate(mul(origin, -1));
  };
  for (auto &g : p.root["groups"])
    resolve(g["uuid"], 0);
  return out;
}
inline bool enabled(const Project &p, const Json &e, bool exporting) {
  if (!(exporting ? e.value("export", true) : e.value("visibility", true)))
    return false;
  auto par = parents(p);
  auto id = e.value("uuid", "");
  int limit = 0;
  while (par.count(id) && !par[id].empty()) {
    id = par[id];
    auto g = p.find(id);
    if (!g || !(exporting ? g->value("export", true) : g->value("visibility", true)) ||
        ++limit > 512)
      return false;
  }
  return true;
}
inline std::array<Vec4, 6> cubeUV(const Json &c) {
  std::array<Vec4, 6> uv;
  for (int i = 0; i < 6; i++) {
    auto f = c["faces"].value(faceNames[i], Json::object());
    uv[i] = f.value("uv", Vec4{0, 0, 16, 16});
  }
  if (c.value("box_uv", false)) {
    auto f = getv(c, "from"), t = getv(c, "to");
    double w = std::floor(t[0] - f[0]), h = std::floor(t[1] - f[1]), d = std::floor(t[2] - f[2]);
    auto o = c.value("uv_offset", Vec2{});
    double u = o[0], v = o[1];
    uv = {Vec4{u + d, v + d, u + d + w, v + d + h},
          Vec4{u, v + d, u + d, v + d + h},
          Vec4{u + 2 * d + w, v + d, u + 2 * d + 2 * w, v + d + h},
          Vec4{u + d + w, v + d, u + 2 * d + w, v + d + h},
          Vec4{u + d + w, v + d, u + d, v},
          Vec4{u + d + 2 * w, v, u + d + w, v + d}};
    if (c.value("mirror_uv", false)) {
      std::swap(uv[1], uv[3]);
      for (auto &f : uv)
        std::swap(f[0], f[2]);
    }
  }
  return uv;
}
inline std::array<std::array<Vec3, 4>, 6> cubeCorners(const Json &c) {
  Vec3 f = getv(c, "from"), t = getv(c, "to"), center = mul(add(f, t), .5),
       stretch = getv(c, "stretch", {1, 1, 1});
  double inf = c.value("inflate", 0.0);
  for (int a = 0; a < 3; a++) {
    f[a] = center[a] + (f[a] - center[a]) * stretch[a] - inf;
    t[a] = center[a] + (t[a] - center[a]) * stretch[a] + inf;
  }
  return {{{{{t[0], t[1], f[2]}, {f[0], t[1], f[2]}, {f[0], f[1], f[2]}, {t[0], f[1], f[2]}}},
           {{{t[0], t[1], t[2]}, {t[0], t[1], f[2]}, {t[0], f[1], f[2]}, {t[0], f[1], t[2]}}},
           {{{f[0], t[1], t[2]}, {t[0], t[1], t[2]}, {t[0], f[1], t[2]}, {f[0], f[1], t[2]}}},
           {{{f[0], t[1], f[2]}, {f[0], t[1], t[2]}, {f[0], f[1], t[2]}, {f[0], f[1], f[2]}}},
           {{{f[0], t[1], f[2]}, {t[0], t[1], f[2]}, {t[0], t[1], t[2]}, {f[0], t[1], t[2]}}},
           {{{f[0], f[1], t[2]}, {t[0], f[1], t[2]}, {t[0], f[1], f[2]}, {f[0], f[1], f[2]}}}}};
}
inline int textureIndex(const Json &f) {
  return f.contains("texture") && f["texture"].is_number_integer() ? f["texture"].get<int>() : -1;
}
inline std::vector<Triangle> triangles(const Project &p, bool exporting = false,
                                       const std::map<std::string, Pose> &poses = {}) {
  auto matrices = groupMatrices(p, poses);
  auto par = parents(p);
  std::vector<Triangle> out;
  for (auto &e : p.root["elements"]) {
    if (!enabled(p, e, exporting))
      continue;
    auto id = e.value("uuid", "");
    auto origin = getv(e, "origin");
    Mat4 parent = matrices.count(par[id]) ? matrices.at(par[id]) : Mat4::identity();
    auto local = translate(origin) * rotation(getv(e, "rotation"));
    auto type = e.value("type", "cube");
    if (type == "cube") {
      auto corners = cubeCorners(e);
      auto uvs = cubeUV(e);
      auto m = parent * local * translate(mul(origin, -1));
      for (int side = 0; side < 6; side++) {
        const char *name = faceNames[side];
        auto f = e["faces"].value(name, Json::object());
        if (!f.value("enabled", true) || (f.contains("texture") && f["texture"].is_null()))
          continue;
        auto uv = uvs[side];
        std::array<Vec2, 4> q{{{uv[0], uv[1]}, {uv[2], uv[1]}, {uv[2], uv[3]}, {uv[0], uv[3]}}};
        int rot = ((f.value("rotation", 0) / 90) % 4 + 4) % 4;
        std::rotate(q.rbegin(), q.rbegin() + rot, q.rend());
        for (auto indices : {std::array<int, 3>{2, 1, 0}, std::array<int, 3>{3, 2, 0}}) {
          Triangle t;
          t.element = id;
          t.face = name;
          t.texture = textureIndex(f);
          t.color = e.value("color", 0);
          t.shade = e.value("shade", true);
          for (int i = 0; i < 3; i++) {
            t.p[i] = m.point(corners[side][indices[i]]);
            t.uv[i] = q[indices[i]];
            t.keys[i] = std::to_string(side) + "_" + std::to_string(indices[i]);
          }
          auto n = unit(cross(sub(t.p[1], t.p[0]), sub(t.p[2], t.p[0])));
          t.n.fill(n);
          out.push_back(t);
        }
      }
    } else if (type == "mesh") {
      auto m = parent * local;
      std::map<std::string, Vec3> normals;
      std::vector<Triangle> temp;
      for (auto it = e["faces"].begin(); it != e["faces"].end(); ++it) {
        auto &f = it.value();
        if (!f.value("enabled", true) || (f.contains("texture") && f["texture"].is_null()))
          continue;
        auto keys = f["vertices"].get<std::vector<std::string>>();
        std::vector<std::array<int, 3>> indices{{0, 1, 2}};
        if (keys.size() == 4) {
          auto a = vec(e["vertices"][keys[0]]), b = vec(e["vertices"][keys[1]]),
               c = vec(e["vertices"][keys[2]]), d = vec(e["vertices"][keys[3]]);
          auto n = cross(sub(b, a), sub(c, a));
          if (dot(n, cross(sub(c, a), sub(d, a))) < 0)
            indices = {{0, 1, 3}, {1, 2, 3}};
          else
            indices.push_back({0, 2, 3});
        }
        for (auto inds : indices) {
          Triangle t;
          t.element = id;
          t.face = it.key();
          t.texture = textureIndex(f);
          t.color = e.value("color", 0);
          t.shade = e.value("shade", true);
          for (int i = 0; i < 3; i++) {
            t.keys[i] = keys[inds[i]];
            t.p[i] = m.point(vec(e["vertices"][t.keys[i]]));
            t.uv[i] = f.contains("uv") && f["uv"].contains(t.keys[i])
                          ? f["uv"][t.keys[i]].get<Vec2>()
                          : Vec2{};
          }
          auto n = unit(cross(sub(t.p[1], t.p[0]), sub(t.p[2], t.p[0])));
          t.n.fill(n);
          for (int i = 0; i < 3; i++) {
            auto u = unit(sub(t.p[(i + 1) % 3], t.p[i])), v = unit(sub(t.p[(i + 2) % 3], t.p[i]));
            normals[t.keys[i]] =
                add(normals[t.keys[i]], mul(n, std::acos(std::clamp(dot(u, v), -1.0, 1.0))));
          }
          temp.push_back(t);
        }
      }
      if (e.value("shading", "flat") == "smooth")
        for (auto &t : temp)
          for (int i = 0; i < 3; i++)
            t.n[i] = unit(normals[t.keys[i]]);
      out.insert(out.end(), temp.begin(), temp.end());
    }
  }
  return out;
}
inline Json toMesh(const Json &c) {
  Project p;
  p.root["elements"].push_back(c);
  appendNode(p, c["uuid"]);
  auto ts = triangles(p);
  Json mesh = {{"type", "mesh"},
               {"uuid", c["uuid"]},
               {"name", c.value("name", "mesh")},
               {"origin", {0, 0, 0}},
               {"rotation", {0, 0, 0}},
               {"vertices", Json::object()},
               {"faces", Json::object()},
               {"visibility", c.value("visibility", true)},
               {"export", c.value("export", true)}};
  std::map<Vec3, std::string> keys;
  for (auto &t : ts) {
    auto key = t.face;
    auto &f = mesh["faces"][key];
    if (f.is_null())
      f = {{"vertices", Json::array()},
           {"uv", Json::object()},
           {"texture", t.texture < 0 ? Json(false) : Json(t.texture)}};
    for (int i = 0; i < 3; i++) {
      if (!keys.count(t.p[i])) {
        auto k = "v" + std::to_string(keys.size());
        keys[t.p[i]] = k;
        mesh["vertices"][k] = t.p[i];
      }
      auto k = keys[t.p[i]];
      if (std::find(f["vertices"].begin(), f["vertices"].end(), Json(k)) == f["vertices"].end())
        f["vertices"].push_back(k);
      f["uv"][k] = t.uv[i];
    }
  }
  return mesh;
}
} // namespace ftebroom::bb
