// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_io.h"
namespace ftebroom::bb {
inline void autoUV(Json &e, bool relative = false) {
  if (e.value("type", "cube") == "cube") {
    auto f = getv(e, "from"), t = getv(e, "to");
    for (int i = 0; i < 6; i++) {
      auto &face = e["faces"][faceNames[i]];
      auto uv = face.value("uv", Vec4{});
      int x = i == 1 || i == 3 ? 2 : 0, y = i >= 4 ? 2 : 1;
      if (relative)
        uv = {f[x], -t[y], t[x], -f[y]};
      else {
        uv[2] = uv[0] + t[x] - f[x];
        uv[3] = uv[1] + t[y] - f[y];
      }
      face["uv"] = uv;
    }
  } else if (e.value("type", "") == "mesh")
    for (auto &face : e["faces"]) {
      auto keys = face["vertices"].get<std::vector<std::string>>();
      Vec3 a = vec(e["vertices"][keys[0]]), b = vec(e["vertices"][keys[1]]),
           c = vec(e["vertices"][keys[2]]), normal = unit(cross(sub(b, a), sub(c, a)));
      int drop = 0;
      for (int n = 1; n < 3; n++)
        if (std::abs(normal[n]) > std::abs(normal[drop]))
          drop = n;
      int x = (drop + 1) % 3, y = (drop + 2) % 3;
      for (auto &key : keys) {
        auto p = vec(e["vertices"][key]);
        face["uv"][key] = {p[x], -p[y]};
      }
    }
}
inline void uvTransform(Json &e, const std::set<std::string> &selected, Vec2 offset,
                        Vec2 stretch = {1, 1}, Vec2 pivot = {}) {
  for (auto it = e["faces"].begin(); it != e["faces"].end(); ++it) {
    if (!selected.empty() && !selected.count(it.key()))
      continue;
    auto &f = it.value();
    if (f["uv"].is_array()) {
      auto uv = f["uv"].get<Vec4>();
      for (int i = 0; i < 4; i++)
        uv[i] = pivot[i % 2] + (uv[i] - pivot[i % 2]) * stretch[i % 2] + offset[i % 2];
      f["uv"] = uv;
    } else
      for (auto &uv : f["uv"]) {
        Vec2 v = uv;
        for (int i = 0; i < 2; i++)
          v[i] = pivot[i] + (v[i] - pivot[i]) * stretch[i] + offset[i];
        uv = v;
      }
  }
}
inline void uvAction(Json &e, const std::set<std::string> &selected, std::string action, int width,
                     int height, const Texture *texture = nullptr) {
  if (action == "Auto UV" || action == "Relative Auto UV") {
    autoUV(e, action == "Relative Auto UV");
    return;
  }
  if (action == "Mirror X" || action == "Mirror Y") {
    Vec2 min{1e30, 1e30}, max{-1e30, -1e30};
    for (auto it = e["faces"].begin(); it != e["faces"].end(); ++it)
      if (selected.empty() || selected.count(it.key())) {
        auto &uv = it.value()["uv"];
        if (uv.is_array()) {
          for (int n = 0; n < 4; n++) {
            min[n % 2] = std::min(min[n % 2], uv[n].get<double>());
            max[n % 2] = std::max(max[n % 2], uv[n].get<double>());
          }
        } else
          for (auto &v : uv)
            for (int a = 0; a < 2; a++) {
              min[a] = std::min(min[a], v[a].get<double>());
              max[a] = std::max(max[a], v[a].get<double>());
            }
      }
    uvTransform(e, selected, {}, action == "Mirror X" ? Vec2{-1, 1} : Vec2{1, -1},
                {(min[0] + max[0]) * .5, (min[1] + max[1]) * .5});
    return;
  }
  if (action == "Apply to All Faces" && !selected.empty()) {
    auto f = e["faces"][*selected.begin()];
    if (e.value("type", "cube") == "cube")
      for (auto &dest : e["faces"])
        dest = f;
    return;
  }
  for (auto it = e["faces"].begin(); it != e["faces"].end(); ++it) {
    if (!selected.empty() && !selected.count(it.key()))
      continue;
    auto &f = it.value();
    if (f["uv"].is_array()) {
      if (action == "Maximize" || action == "Reset")
        f["uv"] = {0, 0, width, height};
      else if (action == "Rotate Left" || action == "Rotate Right" || action == "Turn Mapping" ||
               action == "Cycle")
        f["rotation"] = (f.value("rotation", 0) + (action == "Rotate Left" ? 270 : 90)) % 360;
      else if (action == "Transparent" && texture) {
        bool done = false;
        for (int y = 0; y < texture->height && !done; y++)
          for (int x = 0; x < texture->width && !done; x++)
            if (texture->rgba[(size_t(y) * texture->width + x) * 4 + 3] == 0) {
              double sx = double(texture->uvWidth) / texture->width,
                     sy = double(texture->uvHeight) / texture->height;
              f["uv"] = {x * sx, y * sy, (x + 1) * sx, (y + 1) * sy};
              done = true;
            }
        if (!done)
          throw std::runtime_error("No transparent texel in this texture");
      } else if (action == "Snap UV to pixels")
        for (int n = 0; n < 4; n++)
          f["uv"][n] =
              snap(f["uv"][n], texture ? (n % 2 ? double(texture->uvHeight) / texture->height
                                                : double(texture->uvWidth) / texture->width)
                                       : 1);
    } else {
      if (action == "Maximize" || action == "Reset") {
        Vec2 low{1e30, 1e30}, high{-1e30, -1e30};
        for (auto &uv : f["uv"])
          for (int a = 0; a < 2; a++) {
            low[a] = std::min(low[a], uv[a].get<double>());
            high[a] = std::max(high[a], uv[a].get<double>());
          }
        for (auto &uv : f["uv"]) {
          uv[0] = (uv[0].get<double>() - low[0]) / std::max(.001, high[0] - low[0]) * width;
          uv[1] = (uv[1].get<double>() - low[1]) / std::max(.001, high[1] - low[1]) * height;
        }
      } else if (action == "Snap UV to pixels")
        for (auto &v : f["uv"]) {
          v[0] = snap(v[0], texture ? double(texture->uvWidth) / texture->width : 1);
          v[1] = snap(v[1], texture ? double(texture->uvHeight) / texture->height : 1);
        }
      else if (action == "Rotate Left" || action == "Rotate Right" || action == "Cycle" ||
               action == "Turn Mapping") {
        if (action == "Cycle" || action == "Turn Mapping") {
          auto keys = f["vertices"].get<std::vector<std::string>>();
          auto old = f["uv"];
          for (size_t i = 0; i < keys.size(); i++)
            f["uv"][keys[i]] = old[keys[(i + 1) % keys.size()]];
        } else {
          Vec2 center{};
          for (auto &uv : f["uv"]) {
            center[0] += uv[0].get<double>();
            center[1] += uv[1].get<double>();
          }
          center[0] /= f["uv"].size();
          center[1] /= f["uv"].size();
          double sign = action == "Rotate Right" ? 1 : -1;
          for (auto &uv : f["uv"]) {
            double x = uv[0].get<double>() - center[0], y = uv[1].get<double>() - center[1];
            uv = {center[0] - sign * y, center[1] + sign * x};
          }
        }
      }
    }
  }
  if (action == "Merge UV vertices" || action == "Connect UV faces") {
    std::map<std::string, std::pair<Vec2, int>> points;
    for (auto it = e["faces"].begin(); it != e["faces"].end(); ++it)
      if ((selected.empty() || selected.count(it.key())) && it.value()["uv"].is_object())
        for (auto uv = it.value()["uv"].begin(); uv != it.value()["uv"].end(); ++uv) {
          auto &v = points[uv.key()];
          Vec2 p = uv.value();
          for (int n = 0; n < 2; n++)
            v.first[n] += p[n];
          v.second++;
        }
    for (auto &f : e["faces"])
      if (f["uv"].is_object())
        for (auto uv = f["uv"].begin(); uv != f["uv"].end(); ++uv)
          if (points.count(uv.key())) {
            auto p = points[uv.key()];
            uv.value() = {p.first[0] / p.second, p.first[1] / p.second};
          }
  }
}
inline size_t unfold(Json &mesh, double padding);
struct UVRect {
  std::string element, face;
  double w = 1, h = 1;
  int x = 0, y = 0;
};
inline Texture textureTemplate(Project &p, std::string name, int resolution = 64,
                               bool compress = true, int fill = 0, bool outline = true,
                               bool boxes = false) {
  double density = resolution / 16.;
  std::vector<UVRect> rects;
  for (auto &e : p.root["elements"]) {
    auto id = e["uuid"].get<std::string>();
    if (e.value("type", "cube") == "cube" && boxes) {
      auto f = getv(e, "from"), t = getv(e, "to");
      double w = t[0] - f[0], h = t[1] - f[1], d = t[2] - f[2];
      rects.push_back({id, "", std::ceil((2 * w + 2 * d) * density), std::ceil((h + d) * density)});
    } else if (e.value("type", "") == "mesh") {
      unfold(e, 1);
      auto islands = e["reyondbroom"]["uv_islands"];
      for (size_t n = 0; n < islands.size(); n++) {
        Vec2 low{1e30, 1e30}, high{-1e30, -1e30};
        for (auto &face : islands[n])
          for (auto &uv : e["faces"][face.get<std::string>()]["uv"])
            for (int a = 0; a < 2; a++) {
              low[a] = std::min(low[a], uv[a].get<double>());
              high[a] = std::max(high[a], uv[a].get<double>());
            }
        rects.push_back({id, "@" + std::to_string(n),
                         std::max(1., std::ceil((high[0] - low[0]) * density)),
                         std::max(1., std::ceil((high[1] - low[1]) * density))});
      }
    } else if (e.contains("faces")) {
      autoUV(e);
      for (auto it = e["faces"].begin(); it != e["faces"].end(); ++it) {
        if (!it.value().value("enabled", true) ||
            (it.value().contains("texture") && it.value()["texture"].is_null()))
          continue;
        auto &uv = it.value()["uv"];
        double low[2] = {1e30, 1e30}, high[2] = {-1e30, -1e30};
        if (uv.is_array())
          for (int n = 0; n < 4; n++) {
            double v = uv[n];
            low[n % 2] = std::min(low[n % 2], v);
            high[n % 2] = std::max(high[n % 2], v);
          }
        else
          for (auto &q : uv)
            for (int n = 0; n < 2; n++) {
              double v = q[n];
              low[n] = std::min(low[n], v);
              high[n] = std::max(high[n], v);
            }
        rects.push_back({id, it.key(), std::max(1., std::ceil((high[0] - low[0]) * density)),
                         std::max(1., std::ceil((high[1] - low[1]) * density))});
      }
    }
  }
  if (compress)
    std::stable_sort(rects.begin(), rects.end(), [](auto &a, auto &b) { return a.h > b.h; });
  int size = 16, padding = outline ? 1 : 0;
  for (;;) {
    int x = padding, y = padding, row = 0;
    bool fit = true;
    for (auto &r : rects) {
      int w = r.w + padding * 2, h = r.h + padding * 2;
      if (w > size || h > size) {
        fit = false;
        break;
      }
      if (x + w > size) {
        x = padding;
        y += row;
        row = 0;
      }
      if (y + h > size) {
        fit = false;
        break;
      }
      r.x = x + padding;
      r.y = y + padding;
      x += w;
      row = std::max(row, h);
    }
    if (fit)
      break;
    size *= 2;
    if (size > 2048)
      throw std::runtime_error("Texture template exceeds 2048 pixels; lower texel density");
  }
  Texture t;
  t.id = uuid();
  t.name = name;
  t.width = t.height = size;
  t.uvWidth = t.uvHeight = boxes ? int(size / density) : size;
  t.rgba.resize(size_t(size) * size * 4);
  const unsigned char colors[8][3] = {{130, 156, 180}, {174, 106, 80},  {190, 167, 83},
                                      {93, 160, 124},  {139, 112, 167}, {155, 155, 150},
                                      {171, 110, 136}, {81, 135, 161}};
  int index = p.textures.size();
  for (auto &r : rects) {
    auto e = p.find(r.element);
    int color = (e->value("color", 0) % 8 + 8) % 8;
    for (int y = r.y - padding; y < r.y + r.h + padding; y++)
      for (int x = r.x - padding; x < r.x + r.w + padding; x++) {
        auto at = (size_t(y) * size + x) * 4;
        bool edge = x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + r.h;
        for (int c = 0; c < 3; c++)
          t.rgba[at + c] = edge ? 35 : fill == 2 ? 180 : colors[color][c];
        t.rgba[at + 3] = fill == 1 && !edge ? 0 : 255;
      }
    if (r.face.empty()) {
      (*e)["box_uv"] = true;
      (*e)["uv_offset"] = {r.x / density, r.y / density};
      t.uvWidth = t.uvHeight = int(size / density);
      for (auto &f : (*e)["faces"])
        f["texture"] = index;
    } else if (r.face[0] == '@') {
      auto island = (*e)["reyondbroom"]["uv_islands"][std::stoul(r.face.substr(1))];
      Vec2 low{1e30, 1e30};
      for (auto face : island)
        for (auto &uv : (*e)["faces"][face.get<std::string>()]["uv"])
          for (int a = 0; a < 2; a++)
            low[a] = std::min(low[a], uv[a].get<double>());
      for (auto face : island) {
        auto &f = (*e)["faces"][face.get<std::string>()];
        f["texture"] = index;
        for (auto &uv : f["uv"]) {
          Vec2 q = uv;
          uv = {(r.x + (q[0] - low[0]) * density) / (boxes ? density : 1),
                (r.y + (q[1] - low[1]) * density) / (boxes ? density : 1)};
        }
      }
    } else {
      (*e)["box_uv"] = false;
      auto &f = (*e)["faces"][r.face];
      f["texture"] = index;
      if (f["uv"].is_array())
        f["uv"] = {r.x / (boxes ? density : 1), r.y / (boxes ? density : 1),
                   (r.x + r.w) / (boxes ? density : 1), (r.y + r.h) / (boxes ? density : 1)};
      else {
        Vec2 low{1e30, 1e30};
        for (auto &v : f["uv"])
          for (int a = 0; a < 2; a++)
            low[a] = std::min(low[a], v[a].get<double>());
        for (auto &v : f["uv"]) {
          Vec2 q = v;
          v = {r.x + (q[0] - low[0]) * density, r.y + (q[1] - low[1]) * density};
        }
      }
    }
  }
  addTexture(p, t);
  return t;
}
} // namespace ftebroom::bb

#include "ftebroom_bb_unwrap.h"
