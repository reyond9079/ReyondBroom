// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_edit.h"
#include "ftebroom_bb_unwrap.h"
#include <cstdlib>
namespace ftebroom::bb {
inline bool locked(const Project &p, std::string id) {
  auto parent = parents(p);
  for (int guard = 0; !id.empty() && guard < 513; guard++) {
    auto e = p.find(id);
    if (e && e->value("locked", false))
      return true;
    id = parent[id];
  }
  return false;
}
inline Mat4 objectMatrix(const Project &p, const std::string &id) {
  auto e = p.find(id);
  if (!e)
    return Mat4::identity();
  auto par = parents(p);
  auto groups = groupMatrices(p);
  auto parent = groups.count(par[id]) ? groups[par[id]] : Mat4::identity();
  auto origin = getv(*e, "origin");
  auto m = parent * translate(origin) * rotation(getv(*e, "rotation"));
  return e->value("type", "") == "cube" ? m * translate(mul(origin, -1)) : m;
}
inline void moveWorld(Project &p, const Selection &selection, Vec3 delta) {
  auto top = selectedTop(p, selection);
  auto par = parents(p);
  auto matrices = groupMatrices(p);
  for (auto id : top)
    if (auto e = p.find(id)) {
      if (locked(p, id))
        continue;
      auto parent = matrices.count(par[id]) ? matrices[par[id]] : Mat4::identity();
      auto vertices = selection.vertices.find(id);
      if (e->value("type", "") == "mesh" && vertices != selection.vertices.end() &&
          !vertices->second.empty()) {
        auto local = inverse(parent * rotation(getv(*e, "rotation"))).vector(delta);
        for (auto k : vertices->second)
          if ((*e)["vertices"].contains(k))
            (*e)["vertices"][k] = add(vec((*e)["vertices"][k]), local);
      } else {
        auto local = inverse(parent).vector(delta);
        for (auto child : descendants(p, {id}))
          if (auto item = p.find(child))
            translateObject(*item, local);
      }
    }
  p.revision++;
}
inline Mat4 axisRotation(Vec3 axis, double degrees) {
  axis = unit(axis);
  double a = degrees * pi / 180, c = std::cos(a), s = std::sin(a), t = 1 - c;
  auto m = Mat4::identity();
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      m.m[i][j] = t * axis[i] * axis[j] + (i == j ? c : 0);
  m.m[0][1] -= s * axis[2];
  m.m[0][2] += s * axis[1];
  m.m[1][0] += s * axis[2];
  m.m[1][2] -= s * axis[0];
  m.m[2][0] -= s * axis[1];
  m.m[2][1] += s * axis[0];
  return m;
}
inline Mat4 axisScale(Vec3 axis, double factor) {
  axis = unit(axis);
  auto m = Mat4::identity();
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      m.m[i][j] += (factor - 1) * axis[i] * axis[j];
  return m;
}

inline void rotateWorld(Project &p, const Selection &selection, Vec3 axis, double angle) {
  auto par = parents(p);
  auto groups = groupMatrices(p);
  auto turn = axisRotation(axis, angle);
  for (auto id : selectedTop(p, selection))
    if (auto e = p.find(id)) {
      if (locked(p, id))
        continue;
      auto parent = groups.count(par[id]) ? groups[par[id]] : Mat4::identity();
      (*e)["rotation"] =
          eulerAngles(inverse(parent) * turn * parent * rotation(getv(*e, "rotation")));
    }
  p.revision++;
}
inline void transformVertices(Project &p, const Selection &selection, const Mat4 &world) {
  for (auto id : selection.elements)
    if (auto e = p.find(id)) {
      if (locked(p, id) || e->value("type", "") != "mesh")
        continue;
      auto matrix = objectMatrix(p, id), local = inverse(matrix) * world * matrix;
      auto keys = selection.vertices.find(id);
      for (auto it = (*e)["vertices"].begin(); it != (*e)["vertices"].end(); ++it)
        if (keys == selection.vertices.end() || keys->second.empty() ||
            keys->second.count(it.key()))
          it.value() = local.point(vec(it.value()));
    }
  p.revision++;
}
inline void deleteComponents(Project &p, Selection &selection, int mode) {
  if (mode == 0) {
    eraseSelection(p, selection);
    return;
  }
  for (auto id : selection.elements)
    if (auto e = p.find(id)) {
      if (locked(p, id))
        continue;
      if (mode == 1) {
        for (auto f : selection.faces[id])
          if (e->value("type", "") == "cube")
            (*e)["faces"][f]["texture"] = nullptr;
          else
            (*e)["faces"].erase(f);
      } else if (e->value("type", "") == "mesh") {
        std::set<std::string> vertices = selection.vertices[id];
        if (mode == 2)
          for (auto edge : selection.edges[id]) {
            auto at = edge.find('|');
            vertices.insert(edge.substr(0, at));
            vertices.insert(edge.substr(at + 1));
          }
        std::vector<std::string> remove;
        for (auto it = (*e)["faces"].begin(); it != (*e)["faces"].end(); ++it)
          for (auto &v : it.value()["vertices"])
            if (vertices.count(v.get<std::string>())) {
              remove.push_back(it.key());
              break;
            }
        for (auto f : remove)
          (*e)["faces"].erase(f);
        for (auto v : vertices)
          (*e)["vertices"].erase(v);
      }
    }
  selection.faces.clear();
  selection.vertices.clear();
  selection.edges.clear();
  p.revision++;
}
// Small numeric expression parser. Leading +,-,*,/ are relative to the initial
// value; parentheses and ordinary arithmetic are evaluated without script eval.
inline double expression(std::string text, double initial) {
  size_t at = 0;
  std::function<double()> sum, product, atom;
  auto space = [&] {
    while (at < text.size() && std::isspace((unsigned char)text[at]))
      at++;
  };
  atom = [&]() {
    space();
    if (at >= text.size())
      throw std::runtime_error("Incomplete numeric expression");
    char c = text[at];
    if (c == '+' || c == '-') {
      at++;
      double v = atom();
      return c == '-' ? -v : v;
    }
    if (c == '(') {
      at++;
      double v = sum();
      space();
      if (at >= text.size() || text[at++] != ')')
        throw std::runtime_error("Missing closing parenthesis");
      return v;
    }
    char *end = nullptr;
    double v = std::strtod(text.c_str() + at, &end);
    if (end == text.c_str() + at)
      throw std::runtime_error("Invalid numeric expression");
    at = end - text.c_str();
    return v;
  };
  product = [&]() {
    double v = atom();
    for (;;) {
      space();
      if (at >= text.size() || (text[at] != '*' && text[at] != '/'))
        return v;
      char op = text[at++];
      double n = atom();
      if (op == '/' && std::abs(n) < 1e-12)
        throw std::runtime_error("Division by zero");
      v = op == '*' ? v * n : v / n;
    }
  };
  sum = [&]() {
    double v = product();
    for (;;) {
      space();
      if (at >= text.size() || (text[at] != '+' && text[at] != '-'))
        return v;
      char op = text[at++];
      double n = product();
      v = op == '+' ? v + n : v - n;
    }
  };
  space();
  char relative = at < text.size() ? text[at] : 0;
  if (relative == '+' || relative == '*' || relative == '/') {
    text = "(" + std::to_string(initial) + ")" + text;
    at = 0;
  }
  double value = sum();
  space();
  if (at != text.size() || !std::isfinite(value) || std::abs(value) > 1e7)
    throw std::runtime_error("Invalid or excessive numeric result");
  return value;
}
} // namespace ftebroom::bb
