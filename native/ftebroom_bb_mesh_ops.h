// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_geometry.h"
namespace ftebroom::bb {
inline std::string vertex(Json &m, Vec3 p, bool weld = false) {
  if (weld)
    for (auto it = m["vertices"].begin(); it != m["vertices"].end(); ++it)
      if (length(sub(vec(it.value()), p)) < 1e-7)
        return it.key();
  auto key = "v" + uuid().substr(0, 8);
  m["vertices"][key] = p;
  return key;
}
inline void face(Json &m, std::vector<std::string> keys, int texture = -1) {
  if (keys.size() < 3)
    return;
  if (keys.size() > 4) {
    for (size_t i = 1; i + 1 < keys.size(); i++)
      face(m, {keys[0], keys[i], keys[i + 1]}, texture);
    return;
  }
  Json uv = Json::object();
  for (auto &k : keys) {
    auto v = vec(m["vertices"][k]);
    uv[k] = {v[0], v[2]};
  }
  m["faces"]["f" + uuid().substr(0, 8)] = {
      {"vertices", keys}, {"uv", uv}, {"texture", texture < 0 ? Json(false) : Json(texture)}};
}
inline Json primitive(int kind, double diameter = 16, double height = 8, int sides = 12,
                      double minor = 4, int minorSides = 8, bool align = true) {
  sides = std::clamp(sides, 3, 48);
  minorSides = std::clamp(minorSides, 3, 32);
  if (diameter <= 0 || height <= 0 || minor <= 0)
    throw std::runtime_error("Primitive dimensions must be positive");
  if (kind == 0)
    return toMesh(cube("cuboid", {-diameter * .5, 0, -diameter * .5},
                       {diameter * .5, height, diameter * .5}));
  Json m = {{"type", "mesh"},         {"uuid", uuid()},        {"name", "mesh"},
            {"origin", {0, 0, 0}},    {"rotation", {0, 0, 0}}, {"vertices", Json::object()},
            {"faces", Json::object()}};
  auto ring = [&](double radius, double y, int count, double phase = 0.) {
    std::vector<std::string> keys;
    for (int i = 0; i < count; i++) {
      double a = (i + phase) * 2 * pi / count;
      keys.push_back(vertex(m, {radius * std::cos(a), y, radius * std::sin(a)}));
    }
    return keys;
  };
  if (kind == 2) {
    auto base = ring(diameter * .5, 0, 4, .5);
    std::reverse(base.begin(), base.end());
    face(m, base);
    auto top = vertex(m, {0, height, 0});
    for (int i = 0; i < 4; i++)
      face(m, {base[i], base[(i + 1) % 4], top});
  } else if (kind == 3) {
    auto a = vertex(m, {-diameter * .5, 0, -diameter * .5}),
         b = vertex(m, {-diameter * .5, 0, diameter * .5}),
         c = vertex(m, {diameter * .5, 0, diameter * .5}),
         d = vertex(m, {diameter * .5, 0, -diameter * .5});
    face(m, {a, b, c, d});
  } else if (kind == 4) {
    auto r = ring(diameter * .5, 0, sides, align ? .5 : 0);
    std::reverse(r.begin(), r.end());
    face(m, r);
  } else if (kind == 5 || kind == 6 || kind == 7) {
    auto low = ring(diameter * .5, 0, sides, align ? .5 : 0),
         high = kind == 7 ? std::vector<std::string>{}
                          : ring(diameter * .5, height, sides, align ? .5 : 0);
    if (kind == 7) {
      auto top = vertex(m, {0, height, 0});
      for (int i = 0; i < sides; i++)
        face(m, {low[i], top, low[(i + 1) % sides]});
    } else
      for (int i = 0; i < sides; i++)
        face(m, {low[i], high[i], high[(i + 1) % sides], low[(i + 1) % sides]});
    if (kind == 6) {
      double radius = std::min(diameter * .49, minor * .5);
      auto innerLow = ring(radius, 0, sides, align ? .5 : 0),
           innerHigh = ring(radius, height, sides, align ? .5 : 0);
      for (int i = 0; i < sides; i++) {
        int j = (i + 1) % sides;
        face(m, {innerLow[j], innerHigh[j], innerHigh[i], innerLow[i]});
        face(m, {high[i], innerHigh[i], innerHigh[j], high[j]});
        face(m, {low[j], innerLow[j], innerLow[i], low[i]});
      }
    } else {
      std::reverse(low.begin(), low.end());
      face(m, low);
      if (kind != 7)
        face(m, high);
    }
  } else if (kind == 10) {
    std::vector<std::vector<std::string>> rings;
    for (int i = 0; i < sides; i++) {
      double a = i * 2 * pi / sides;
      std::vector<std::string> r;
      for (int j = 0; j < minorSides; j++) {
        double b = j * 2 * pi / minorSides, R = diameter * .5 + minor * .5 * std::cos(b);
        r.push_back(vertex(m, {R * std::cos(a), minor * .5 * std::sin(b), R * std::sin(a)}));
      }
      rings.push_back(r);
    }
    for (int i = 0; i < sides; i++)
      for (int j = 0; j < minorSides; j++)
        face(m, {rings[i][j], rings[(i + 1) % sides][j],
                 rings[(i + 1) % sides][(j + 1) % minorSides], rings[i][(j + 1) % minorSides]});
  } else if (kind == 1) { // Beveled cuboid: clipped corner rings, connected top/bottom bevels.
    double r = diameter * .5, b = std::min({minor * .5, r * .45, height * .24});
    std::vector<std::vector<std::string>> rows;
    for (int row = 0; row < 4; row++) {
      double extent = (row == 0 || row == 3) ? r - b : r, y = row == 0   ? 0
                                                              : row == 1 ? b
                                                              : row == 2 ? height - b
                                                                         : height;
      std::vector<std::string> keys;
      for (auto xz :
           {Vec2{-extent + b, -extent}, Vec2{extent - b, -extent}, Vec2{extent, -extent + b},
            Vec2{extent, extent - b}, Vec2{extent - b, extent}, Vec2{-extent + b, extent},
            Vec2{-extent, extent - b}, Vec2{-extent, -extent + b}})
        keys.push_back(vertex(m, {xz[0], y, xz[1]}));
      rows.push_back(keys);
    }
    for (int row = 0; row < 3; row++)
      for (int i = 0; i < 8; i++)
        face(m,
             {rows[row][i], rows[row + 1][i], rows[row + 1][(i + 1) % 8], rows[row][(i + 1) % 8]});
    auto bottom = rows.front();
    std::reverse(bottom.begin(), bottom.end());
    face(m, bottom);
    face(m, rows.back());
  } else if (kind == 9) {
    double g = (1 + std::sqrt(5.0)) * .5;
    std::vector<Vec3> points = {{-1, g, 0}, {1, g, 0}, {-1, -g, 0}, {1, -g, 0},
                                {0, -1, g}, {0, 1, g}, {0, -1, -g}, {0, 1, -g},
                                {g, 0, -1}, {g, 0, 1}, {-g, 0, -1}, {-g, 0, 1}};
    std::vector<std::array<int, 3>> tris = {
        {0, 11, 5},  {0, 5, 1},  {0, 1, 7},  {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
        {11, 10, 2}, {10, 7, 6}, {7, 1, 8},  {3, 9, 4},  {3, 4, 2},   {3, 2, 6}, {3, 6, 8},
        {3, 8, 9},   {4, 9, 5},  {2, 4, 11}, {6, 2, 10}, {8, 6, 7},   {9, 8, 1}};
    for (auto &p : points)
      p = unit(p);
    int levels = std::clamp(minorSides / 8, 0, 2);
    for (int n = 0; n < levels; n++) {
      std::map<std::pair<int, int>, int> mids;
      auto mid = [&](int a, int b) {
        auto k = std::minmax(a, b);
        auto it = mids.find(k);
        if (it != mids.end())
          return it->second;
        int i = points.size();
        points.push_back(unit(add(points[a], points[b])));
        mids[k] = i;
        return i;
      };
      std::vector<std::array<int, 3>> next;
      for (auto t : tris) {
        int a = mid(t[0], t[1]), b = mid(t[1], t[2]), c = mid(t[2], t[0]);
        for (auto v : {std::array<int, 3>{t[0], a, c}, {t[1], b, a}, {t[2], c, b}, {a, b, c}})
          next.push_back(v);
      }
      tris = std::move(next);
    }
    std::vector<std::string> keys;
    for (auto p : points)
      keys.push_back(vertex(m, {p[0] * diameter * .5, p[1] * height * .5, p[2] * diameter * .5}));
    for (auto t : tris)
      face(m, {keys[t[0]], keys[t[1]], keys[t[2]]});
  } else { // Latitude sphere with shared pole vertices.
    int levels = std::max(3, minorSides);
    auto bottom = vertex(m, {0, -height * .5, 0}), top = vertex(m, {0, height * .5, 0});
    std::vector<std::vector<std::string>> rings;
    for (int j = 1; j < levels; j++) {
      double a = -pi * .5 + pi * j / levels;
      rings.push_back(ring(diameter * .5 * std::cos(a), height * .5 * std::sin(a), sides));
    }
    for (int i = 0; i < sides; i++) {
      int n = (i + 1) % sides;
      face(m, {bottom, rings[0][n], rings[0][i]});
      face(m, {top, rings.back()[i], rings.back()[n]});
      for (size_t j = 0; j + 1 < rings.size(); j++) {
        auto keys =
            std::vector<std::string>{rings[j][i], rings[j][n], rings[j + 1][n], rings[j + 1][i]};
        if (kind == 9) {
          face(m, {keys[0], keys[1], keys[2]});
          face(m, {keys[0], keys[2], keys[3]});
        } else
          face(m, keys);
      }
    }
  }
  // Orient closed primitive faces away from their center; open planes retain +Y.
  Vec3 center{};
  for (auto &v : m["vertices"])
    center = add(center, vec(v));
  center = mul(center, 1. / std::max(size_t(1), m["vertices"].size()));
  for (auto &f : m["faces"]) {
    auto keys = f["vertices"].get<std::vector<std::string>>();
    auto a = vec(m["vertices"][keys[0]]), b = vec(m["vertices"][keys[1]]),
         c = vec(m["vertices"][keys[2]]);
    if (kind == 10 || (kind != 6 && dot(cross(sub(b, a), sub(c, a)),
                                        sub(mul(add(add(a, b), c), 1. / 3), center)) < 0)) {
      std::reverse(keys.begin(), keys.end());
      f["vertices"] = keys;
    }
  }
  return m;
}
inline void extrude(Json &mesh, const std::set<std::string> &selected, double distance,
                    bool inset = false) {
  if (selected.empty())
    throw std::runtime_error("Select mesh faces first");
  for (auto &key : selected) {
    if (!mesh["faces"].contains(key))
      continue;
    auto original = mesh["faces"][key];
    auto keys = original["vertices"].get<std::vector<std::string>>();
    Vec3 center{};
    for (auto &k : keys)
      center = add(center, vec(mesh["vertices"][k]));
    center = mul(center, 1. / keys.size());
    auto normal = unit(cross(sub(vec(mesh["vertices"][keys[1]]), vec(mesh["vertices"][keys[0]])),
                             sub(vec(mesh["vertices"][keys[2]]), vec(mesh["vertices"][keys[0]]))));
    std::vector<std::string> moved;
    Json uv = Json::object();
    for (auto &k : keys) {
      auto p = vec(mesh["vertices"][k]);
      p = inset ? add(center, mul(sub(p, center), std::clamp(1 - distance, 0.001, .999)))
                : add(p, mul(normal, distance));
      auto n = vertex(mesh, p);
      moved.push_back(n);
      uv[n] = original["uv"][k];
    }
    for (size_t i = 0; i < keys.size(); i++)
      face(mesh, {keys[i], keys[(i + 1) % keys.size()], moved[(i + 1) % keys.size()], moved[i]},
           textureIndex(original));
    mesh["faces"][key]["vertices"] = moved;
    mesh["faces"][key]["uv"] = uv;
  }
}
inline void mergeVertices(Json &m, const std::set<std::string> &selected, int mode,
                          double threshold = .1, std::string last = "") {
  std::map<std::string, std::string> replace;
  std::vector<std::string> keys(selected.begin(), selected.end());
  if (keys.size() < 2)
    throw std::runtime_error("Select two or more mesh vertices");
  if (mode) {
    auto dest = mode == 2 && selected.count(last) ? last : keys.front();
    Vec3 p{};
    if (mode == 2)
      p = vec(m["vertices"][dest]);
    else {
      for (auto &k : keys)
        p = add(p, vec(m["vertices"][k]));
      p = mul(p, 1. / keys.size());
    }
    m["vertices"][dest] = p;
    for (auto &k : keys)
      if (k != dest)
        replace[k] = dest;
  } else
    for (size_t i = 0; i < keys.size(); i++)
      if (!replace.count(keys[i]))
        for (size_t j = i + 1; j < keys.size(); j++)
          if (!replace.count(keys[j]) &&
              length(sub(vec(m["vertices"][keys[i]]), vec(m["vertices"][keys[j]]))) <= threshold)
            replace[keys[j]] = keys[i];
  std::vector<std::string> remove;
  for (auto it = m["faces"].begin(); it != m["faces"].end(); ++it) {
    auto &f = it.value();
    std::vector<std::string> v;
    Json uv = Json::object();
    for (auto k : f["vertices"].get<std::vector<std::string>>()) {
      auto old = k;
      if (replace.count(k))
        k = replace[k];
      if (std::find(v.begin(), v.end(), k) == v.end()) {
        v.push_back(k);
        uv[k] = f["uv"][old];
      }
    }
    if (v.size() < 3)
      remove.push_back(it.key());
    else {
      f["vertices"] = v;
      f["uv"] = uv;
    }
  }
  for (auto &k : remove)
    m["faces"].erase(k);
  for (auto &[a, b] : replace)
    m["vertices"].erase(a);
}
inline void splitFaces(Json &m, const std::set<std::string> &selected) {
  std::map<std::string, std::string> duplicate;
  for (auto &key : selected) {
    auto &f = m["faces"][key];
    Json uv = Json::object();
    auto keys = f["vertices"].get<std::vector<std::string>>();
    for (auto &k : keys) {
      auto old = k;
      if (!duplicate.count(k))
        duplicate[k] = vertex(m, vec(m["vertices"][k]));
      k = duplicate[k];
      uv[k] = f["uv"][old];
    }
    f["vertices"] = keys;
    f["uv"] = uv;
  }
}
inline void loopCut(Json &m, int axis = 0, int cuts = 1, double position = .5) {
  if (axis < 0 || axis > 2)
    throw std::runtime_error("Invalid loop axis");
  double low = 1e30, high = -1e30;
  for (auto &v : m["vertices"]) {
    auto p = vec(v);
    low = std::min(low, p[axis]);
    high = std::max(high, p[axis]);
  }
  for (int cut = 0; cut < std::clamp(cuts, 1, 32); cut++) {
    double plane = low + (high - low) * (cuts == 1 ? std::clamp(position, .001, .999)
                                                   : double(cut + 1) / (cuts + 1));
    auto old = m["faces"];
    for (auto it = old.begin(); it != old.end(); ++it) {
      auto f = it.value();
      auto keys = f["vertices"].get<std::vector<std::string>>();
      std::vector<std::string> halves[2];
      Json uv = f["uv"];
      bool below = false, above = false;
      for (auto &k : keys) {
        double d = vec(m["vertices"][k])[axis] - plane;
        below |= d < -1e-8;
        above |= d > 1e-8;
      }
      if (!below || !above)
        continue;
      for (size_t i = 0; i < keys.size(); i++) {
        auto a = keys[i], b = keys[(i + 1) % keys.size()];
        auto pa = vec(m["vertices"][a]), pb = vec(m["vertices"][b]);
        double da = pa[axis] - plane, db = pb[axis] - plane;
        if (da <= 1e-8)
          halves[0].push_back(a);
        if (da >= -1e-8)
          halves[1].push_back(a);
        if (da * db < -1e-12) {
          double t = da / (da - db);
          auto k = vertex(m, add(pa, mul(sub(pb, pa), t)), true);
          Vec2 uva = uv[a], uvb = uv[b];
          uv[k] = {uva[0] + (uvb[0] - uva[0]) * t, uva[1] + (uvb[1] - uva[1]) * t};
          halves[0].push_back(k);
          halves[1].push_back(k);
        }
      }
      m["faces"].erase(it.key());
      for (auto &half : halves) {
        if (half.size() < 3)
          continue;
        if (half.size() <= 4) {
          auto next = f;
          next["vertices"] = half;
          next["uv"] = uv;
          m["faces"]["f" + uuid().substr(0, 8)] = next;
        } else
          for (size_t i = 1; i + 1 < half.size(); i++) {
            auto next = f;
            next["vertices"] = {half[0], half[i], half[i + 1]};
            next["uv"] = uv;
            m["faces"]["f" + uuid().substr(0, 8)] = next;
          }
      }
    }
  }
}
inline void solidify(Json &m, double distance) {
  auto old = m;
  std::map<std::string, Vec3> normals;
  std::map<std::pair<std::string, std::string>, int> edges;
  for (auto &f : old["faces"]) {
    auto keys = f["vertices"].get<std::vector<std::string>>();
    auto n = unit(cross(sub(vec(old["vertices"][keys[1]]), vec(old["vertices"][keys[0]])),
                        sub(vec(old["vertices"][keys[2]]), vec(old["vertices"][keys[0]]))));
    for (size_t i = 0; i < keys.size(); i++) {
      normals[keys[i]] = add(normals[keys[i]], n);
      edges[std::minmax(keys[i], keys[(i + 1) % keys.size()])]++;
    }
  }
  std::map<std::string, std::string> back;
  for (auto it = old["vertices"].begin(); it != old["vertices"].end(); ++it)
    back[it.key()] = vertex(m, sub(vec(it.value()), mul(unit(normals[it.key()]), distance)));
  for (auto &f : old["faces"]) {
    auto keys = f["vertices"].get<std::vector<std::string>>();
    for (size_t i = 0; i < keys.size(); i++) {
      auto a = keys[i], b = keys[(i + 1) % keys.size()];
      if (edges[std::minmax(a, b)] == 1)
        face(m, {a, back[a], back[b], b}, textureIndex(f));
    }
    for (auto &k : keys)
      k = back[k];
    std::reverse(keys.begin(), keys.end());
    face(m, keys, textureIndex(f));
  }
}
} // namespace ftebroom::bb
