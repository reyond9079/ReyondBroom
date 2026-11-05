// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_geometry.h"
namespace ftebroom::bb {
struct Ray {
  Vec3 origin{}, direction{0, 0, -1};
};
struct Hit {
  bool found = false;
  double distance = 1e30;
  std::string element, face;
  Vec3 point{}, normal{}, barycentric{};
  Vec2 uv{};
  int texture = -1, x = 0, y = 0;
  size_t triangle = 0;
};
inline bool rayBox(Ray r, Vec3 low, Vec3 high, double maxDistance) {
  double a = 0, b = maxDistance;
  for (int i = 0; i < 3; i++) {
    if (std::abs(r.direction[i]) < 1e-12) {
      if (r.origin[i] < low[i] || r.origin[i] > high[i])
        return false;
    } else {
      double x = (low[i] - r.origin[i]) / r.direction[i],
             y = (high[i] - r.origin[i]) / r.direction[i];
      if (x > y)
        std::swap(x, y);
      a = std::max(a, x);
      b = std::min(b, y);
      if (a > b)
        return false;
    }
  }
  return true;
}
// Acceleration for overlay visibility. Locked objects still occlude, while
// transparent texels do not. Rebuild when the source triangle vector changes.
class VisibilityTree {
  struct Node {
    Vec3 low{1e30, 1e30, 1e30}, high{-1e30, -1e30, -1e30};
    size_t begin = 0, count = 0;
    int left = -1, right = -1;
  };
  const std::vector<Triangle> *source = nullptr;
  std::vector<size_t> order;
  std::vector<Node> nodes;
  int build(size_t begin, size_t end) {
    int index = int(nodes.size());
    nodes.emplace_back();
    for (size_t n = begin; n < end; n++)
      for (auto p : (*source)[order[n]].p)
        for (int a = 0; a < 3; a++) {
          nodes[index].low[a] = std::min(nodes[index].low[a], p[a]);
          nodes[index].high[a] = std::max(nodes[index].high[a], p[a]);
        }
    if (end - begin <= 8) {
      nodes[index].begin = begin;
      nodes[index].count = end - begin;
    } else {
      auto extent = sub(nodes[index].high, nodes[index].low);
      int axis = int(std::max_element(extent.begin(), extent.end()) - extent.begin());
      size_t middle = (begin + end) / 2;
      std::nth_element(order.begin() + begin, order.begin() + middle, order.begin() + end,
                       [&](size_t a, size_t b) {
                         auto &x = (*source)[a].p;
                         auto &y = (*source)[b].p;
                         return x[0][axis] + x[1][axis] + x[2][axis] <
                                y[0][axis] + y[1][axis] + y[2][axis];
                       });
      int left = build(begin, middle), right = build(middle, end);
      nodes[index].left = left;
      nodes[index].right = right;
    }
    return index;
  }
  bool blockedNode(int index, const Project &project, Ray ray, double limit) const {
    const auto &node = nodes[index];
    if (!rayBox(ray, node.low, node.high, limit))
      return false;
    if (node.left >= 0)
      return blockedNode(node.left, project, ray, limit) ||
             blockedNode(node.right, project, ray, limit);
    for (size_t n = node.begin; n < node.begin + node.count; n++) {
      const auto &t = (*source)[order[n]];
      auto a = sub(t.p[1], t.p[0]), b = sub(t.p[2], t.p[0]), c = cross(ray.direction, b);
      double determinant = dot(a, c);
      if (std::abs(determinant) < 1e-10)
        continue;
      auto s = sub(ray.origin, t.p[0]), q = cross(s, a);
      double u = dot(s, c) / determinant, v = dot(ray.direction, q) / determinant,
             distance = dot(b, q) / determinant;
      if (u < -1e-9 || v < -1e-9 || u + v > 1 + 1e-9 || distance <= .00001 || distance >= limit)
        continue;
      if (t.texture >= 0 && size_t(t.texture) < project.textures.size()) {
        const auto &texture = project.textures[t.texture];
        auto uv = Vec2{t.uv[0][0] * (1 - u - v) + t.uv[1][0] * u + t.uv[2][0] * v,
                       t.uv[0][1] * (1 - u - v) + t.uv[1][1] * u + t.uv[2][1] * v};
        int x = std::clamp(int(std::floor(uv[0] * texture.width / texture.uvWidth)), 0,
                           texture.width - 1),
            y = std::clamp(int(std::floor(uv[1] * texture.height / texture.uvHeight)), 0,
                           texture.height - 1);
        if (!texture.rgba.empty() && texture.rgba[(size_t(y) * texture.width + x) * 4 + 3] < 128)
          continue;
      }
      return true;
    }
    return false;
  }

public:
  void rebuild(const std::vector<Triangle> &triangles) {
    source = &triangles;
    nodes.clear();
    order.resize(triangles.size());
    for (size_t n = 0; n < order.size(); n++)
      order[n] = n;
    if (!order.empty())
      build(0, order.size());
  }
  bool blocked(const Project &project, Ray ray, double limit) const {
    return !nodes.empty() && limit > 0 && blockedNode(0, project, ray, limit);
  }
};
inline Hit pick(const Project &p, const std::vector<Triangle> &tris, Ray ray, bool cull = false) {
  Hit h;
  auto hierarchy = parents(p);
  std::map<std::string, std::vector<size_t>> byElement;
  std::map<std::string, std::pair<Vec3, Vec3>> boxes;
  for (size_t i = 0; i < tris.size(); i++) {
    auto &t = tris[i];
    byElement[t.element].push_back(i);
    if (!boxes.count(t.element))
      boxes[t.element] = {{1e30, 1e30, 1e30}, {-1e30, -1e30, -1e30}};
    auto &box = boxes[t.element];
    for (auto v : t.p)
      for (int a = 0; a < 3; a++) {
        box.first[a] = std::min(box.first[a], v[a]);
        box.second[a] = std::max(box.second[a], v[a]);
      }
  }
  for (auto &[id, indices] : byElement) {
    auto &box = boxes[id];
    if (!rayBox(ray, box.first, box.second, h.distance))
      continue;
    auto e = p.find(id);
    bool locked = e && e->value("locked", false);
    auto ancestor = hierarchy[id];
    for (int guard = 0; !ancestor.empty() && guard < 513; guard++) {
      auto group = p.find(ancestor);
      if (group && group->value("locked", false))
        locked = true;
      ancestor = hierarchy[ancestor];
    }
    if (locked)
      continue;
    for (auto index : indices) {
      auto &t = tris[index];
      auto e1 = sub(t.p[1], t.p[0]), e2 = sub(t.p[2], t.p[0]);
      auto crossD = cross(ray.direction, e2);
      double determinant = dot(e1, crossD);
      if ((cull && determinant <= 1e-9) || std::abs(determinant) < 1e-9)
        continue;
      double inv = 1 / determinant;
      auto s = sub(ray.origin, t.p[0]);
      double u = dot(s, crossD) * inv;
      if (u < 0 || u > 1)
        continue;
      auto q = cross(s, e1);
      double v = dot(ray.direction, q) * inv;
      if (v < 0 || u + v > 1)
        continue;
      double d = dot(e2, q) * inv;
      if (d <= .00001 || d >= h.distance)
        continue;
      h.found = true;
      h.distance = d;
      h.element = id;
      h.face = t.face;
      h.triangle = index;
      h.barycentric = {1 - u - v, u, v};
      h.point = add(ray.origin, mul(ray.direction, d));
      h.normal = t.n[0];
      h.texture = t.texture;
      for (int a = 0; a < 2; a++)
        h.uv[a] = t.uv[0][a] * (1 - u - v) + t.uv[1][a] * u + t.uv[2][a] * v;
      if (h.texture >= 0 && size_t(h.texture) < p.textures.size()) {
        auto &tex = p.textures[h.texture];
        h.x = std::clamp(int(std::floor(h.uv[0] * tex.width / tex.uvWidth)), 0, tex.width - 1);
        h.y = std::clamp(int(std::floor(h.uv[1] * tex.height / tex.uvHeight)), 0, tex.height - 1);
      }
    }
  }
  return h;
}
} // namespace ftebroom::bb
