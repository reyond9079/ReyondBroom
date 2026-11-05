// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_uv.h"
#include <queue>
namespace ftebroom::bb {
inline std::string edgeKey(std::string a, std::string b) {
  if (b < a)
    std::swap(a, b);
  return a + "|" + b;
}
// A deterministic, length-preserving unfolding along non-seam edges. This is
// deliberately a hinged-face unfold, not an LSCM solver. Curved closed surfaces
// need explicit seams; conflicting loop closures remain separate UV corners.
inline size_t unfold(Json &mesh, double padding = 1) {
  if (mesh.value("type", "") != "mesh")
    throw std::runtime_error("Unwrap requires a mesh");
  using Edge = std::pair<std::string, std::string>;
  mesh["reyondbroom"]["uv_islands"] = Json::array();
  std::map<std::string, std::vector<std::string>> adjacent;
  for (auto it = mesh["faces"].begin(); it != mesh["faces"].end(); ++it) {
    auto v = it.value()["vertices"].get<std::vector<std::string>>();
    for (size_t n = 0; n < v.size(); n++)
      adjacent[edgeKey(v[n], v[(n + 1) % v.size()])].push_back(it.key());
  }
  auto seams = mesh.value("seams", Json::object());
  std::set<std::string> done;
  size_t islands = 0;
  double cursor = 0;
  for (auto seed = mesh["faces"].begin(); seed != mesh["faces"].end(); ++seed) {
    if (done.count(seed.key()))
      continue;
    islands++;
    std::queue<std::string> todo;
    std::vector<std::string> island;
    auto initial = seed.value()["vertices"].get<std::vector<std::string>>();
    auto a = vec(mesh["vertices"][initial[0]]), b = vec(mesh["vertices"][initial[1]]),
         c = vec(mesh["vertices"][initial[2]]);
    auto x = unit(sub(b, a)), n = unit(cross(x, sub(c, a))), y = cross(n, x);
    for (auto k : initial) {
      auto d = sub(vec(mesh["vertices"][k]), a);
      seed.value()["uv"][k] = {dot(d, x), dot(d, y)};
    }
    done.insert(seed.key());
    todo.push(seed.key());
    while (!todo.empty()) {
      auto id = todo.front();
      todo.pop();
      island.push_back(id);
      auto &face = mesh["faces"][id];
      auto keys = face["vertices"].get<std::vector<std::string>>();
      for (size_t i = 0; i < keys.size(); i++) {
        auto k0 = keys[i], k1 = keys[(i + 1) % keys.size()], edge = edgeKey(k0, k1);
        if (seams.value(edge, "") == "divide")
          continue;
        for (auto other : adjacent[edge]) {
          if (done.count(other))
            continue;
          auto &next = mesh["faces"][other];
          auto points = next["vertices"].get<std::vector<std::string>>();
          Vec2 u0 = face["uv"][k0], u1 = face["uv"][k1];
          auto p0 = vec(mesh["vertices"][k0]), p1 = vec(mesh["vertices"][k1]),
               direction = unit(sub(p1, p0));
          double length3 = length(sub(p1, p0));
          if (length3 < 1e-9)
            continue;
          Vec2 axis{(u1[0] - u0[0]) / length3, (u1[1] - u0[1]) / length3};
          double side = 1;
          for (auto k : keys)
            if (k != k0 && k != k1) {
              Vec2 u = face["uv"][k];
              double cross2 = axis[0] * (u[1] - u0[1]) - axis[1] * (u[0] - u0[0]);
              if (std::abs(cross2) > 1e-9) {
                side = cross2 > 0 ? -1 : 1;
                break;
              }
            }
          for (auto k : points) {
            auto d = sub(vec(mesh["vertices"][k]), p0);
            double along = dot(d, direction),
                   height = std::sqrt(std::max(0., dot(d, d) - along * along));
            next["uv"][k] = {u0[0] + axis[0] * along - axis[1] * height * side,
                             u0[1] + axis[1] * along + axis[0] * height * side};
          }
          done.insert(other);
          todo.push(other);
        }
      }
    }
    Vec2 low{1e30, 1e30}, high{-1e30, -1e30};
    for (auto id : island)
      for (auto &uv : mesh["faces"][id]["uv"])
        for (int d = 0; d < 2; d++) {
          low[d] = std::min(low[d], uv[d].get<double>());
          high[d] = std::max(high[d], uv[d].get<double>());
        }
    mesh["reyondbroom"]["uv_islands"].push_back(island);
    for (auto id : island)
      for (auto &uv : mesh["faces"][id]["uv"]) {
        uv[0] = uv[0].get<double>() - low[0] + cursor + padding;
        uv[1] = uv[1].get<double>() - low[1] + padding;
      }
    cursor += high[0] - low[0] + padding * 2;
  }
  return islands;
}
inline std::set<std::string> edgeLoop(const Json &mesh, std::string start) {
  std::set<std::string> found{start};
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &f : mesh["faces"]) {
      auto v = f["vertices"].get<std::vector<std::string>>();
      if (v.size() != 4)
        continue;
      for (int i = 0; i < 4; i++)
        if (found.count(edgeKey(v[i], v[(i + 1) % 4])) &&
            found.insert(edgeKey(v[(i + 2) % 4], v[(i + 3) % 4])).second)
          changed = true;
    }
  }
  return found;
}
} // namespace ftebroom::bb
