// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_geometry.h"
namespace ftebroom::bb {
inline Vec3 keyValue(const Json &k, bool post = false) {
  Vec3 out{};
  auto points = k.value("data_points", Json::array());
  if (points.empty())
    return out;
  auto &p = points[post && points.size() > 1 ? 1 : 0];
  for (int a = 0; a < 3; a++) {
    auto v = p.value(a == 0 ? "x" : a == 1 ? "y" : "z", Json(0));
    if (v.is_number())
      out[a] = v.get<double>();
    else if (v.is_string())
      try {
        size_t n = 0;
        auto s = v.get<std::string>();
        double d = std::stod(s, &n);
        out[a] = n == s.size() && std::isfinite(d) ? d : 0;
      } catch (...) {
      }
  }
  return out;
}
inline double animationLength(const Json &a) {
  double length = a.value("length", 0.0);
  if (length <= 0 && a.contains("animators"))
    for (auto &track : a["animators"])
      for (auto &k : track.value("keyframes", Json::array()))
        length = std::max(length, k.value("time", 0.0));
  return length;
}
inline double cubic(double a, double b, double c, double d, double t) {
  double u = 1 - t;
  return u * u * u * a + 3 * u * u * t * b + 3 * u * t * t * c + t * t * t * d;
}
inline Vec3 channel(const Json &animator, std::string name, double time) {
  std::vector<Json> keys;
  for (auto &k : animator.value("keyframes", Json::array()))
    if (k.value("channel", "") == name)
      keys.push_back(k);
  if (keys.empty())
    return name == "scale" ? Vec3{1, 1, 1} : Vec3{};
  std::stable_sort(keys.begin(), keys.end(),
                   [](auto &a, auto &b) { return a.value("time", 0.0) < b.value("time", 0.0); });
  if (time < keys.front().value("time", 0.0))
    return keyValue(keys.front());
  if (time >= keys.back().value("time", 0.0))
    return keyValue(keys.back(), true);
  size_t i = 0;
  while (i + 1 < keys.size() && keys[i + 1].value("time", 0.0) <= time)
    i++;
  auto &a = keys[i];
  auto &b = keys[i + 1];
  double t0 = a.value("time", 0.0), t1 = b.value("time", 0.0),
         alpha = (time - t0) / std::max(1e-12, t1 - t0);
  auto va = keyValue(a, true), vb = keyValue(b);
  auto interp = a.value("interpolation", "linear");
  if (interp == "step")
    return va;
  Vec3 out{};
  for (int n = 0; n < 3; n++)
    if (interp == "catmullrom") {
      auto pre = i && keys[i - 1].value("interpolation", "") == "catmullrom"
                     ? keyValue(keys[i - 1], true)
                     : va;
      auto post = i + 2 < keys.size() && keys[i + 2].value("interpolation", "") == "catmullrom"
                      ? keyValue(keys[i + 2])
                      : vb;
      double t = alpha, t2 = t * t, t3 = t2 * t;
      out[n] = .5 * ((2 * va[n]) + (-pre[n] + vb[n]) * t +
                     (2 * pre[n] - 5 * va[n] + 4 * vb[n] - post[n]) * t2 +
                     (-pre[n] + 3 * va[n] - 3 * vb[n] + post[n]) * t3);
    } else if (interp == "bezier") {
      auto at = getv(a, "bezier_right_time", {.1, .1, .1}), av = getv(a, "bezier_right_value"),
           bt = getv(b, "bezier_left_time", {-.1, -.1, -.1}), bv = getv(b, "bezier_left_value");
      if (b.value("interpolation", "") != "bezier") {
        bt = {};
        bv = {};
      }
      double low = 0, high = 1;
      for (int j = 0; j < 30; j++) {
        double t = (low + high) * .5;
        if (cubic(t0, t0 + at[n], t1 + bt[n], t1, t) < time)
          low = t;
        else
          high = t;
      }
      out[n] = cubic(va[n], va[n] + av[n], vb[n] + bv[n], vb[n], (low + high) * .5);
    } else
      out[n] = va[n] + (vb[n] - va[n]) * alpha;
  return out;
}
inline std::map<std::string, Pose> animationPose(const Json &animation, double time) {
  double length = animationLength(animation);
  if (animation.value("loop", "once") == "loop" && length > 0)
    time = std::fmod(std::max(0.0, time), length);
  else
    time = std::clamp(time, 0.0, std::max(0.0, length));
  std::map<std::string, Pose> poses;
  if (animation.contains("animators"))
    for (auto it = animation["animators"].begin(); it != animation["animators"].end(); ++it) {
      if (it.value().value("type", "bone") != "bone")
        continue;
      poses[it.key()] = {channel(it.value(), "position", time),
                         channel(it.value(), "rotation", time), channel(it.value(), "scale", time)};
    }
  return poses;
}
inline Json keyframe(std::string channelName, double time, Vec3 value,
                     std::string interp = "linear") {
  return {{"uuid", uuid()},
          {"channel", channelName},
          {"time", time},
          {"interpolation", interp},
          {"data_points", Json::array({{{"x", std::to_string(value[0])},
                                        {"y", std::to_string(value[1])},
                                        {"z", std::to_string(value[2])}}})}};
}
inline void setKey(Json &animation, const std::string &groupId, std::string name, double time,
                   Vec3 value) {
  auto &track = animation["animators"][groupId];
  if (track.is_null())
    track = {{"type", "bone"}, {"name", groupId}, {"keyframes", Json::array()}};
  for (auto &k : track["keyframes"])
    if (k.value("channel", "") == name && std::abs(k.value("time", 0.0) - time) < 1e-6) {
      auto replacement = keyframe(name, time, value, k.value("interpolation", "linear"));
      replacement["uuid"] = k["uuid"];
      k = replacement;
      return;
    }
  track["keyframes"].push_back(keyframe(name, time, value));
}
struct BakeFrame {
  int animation = -1;
  double time = 0;
  std::string name = "base";
};
inline std::vector<BakeFrame> bakeFrames(const Project &p) {
  std::vector<BakeFrame> frames;
  if (p.root["reyondbroom"].value("include_base_frame", true))
    frames.push_back({});
  for (size_t i = 0; i < p.root["animations"].size(); i++) {
    auto &a = p.root["animations"][i];
    auto ext = a.value("reyondbroom", Json::object());
    if (!ext.value("export", true))
      continue;
    double fps = std::clamp(ext.value("export_fps", 10.0), 1.0, 120.0), len = animationLength(a);
    size_t count = std::ceil(len * fps - 1e-8) + (a.value("loop", "once") == "loop" ? 0 : 1);
    count = std::max(size_t(1), count);
    if (frames.size() + count > 1024)
      throw std::runtime_error("Export exceeds 1024 vertex-animation frames");
    auto name = ext.value("export_name", a.value("name", "animation"));
    for (size_t n = 0; n < count; n++) {
      auto suffix = std::to_string(n + 1);
      frames.push_back(
          {int(i), std::min(len, n / fps), name.substr(0, 15 - suffix.size()) + suffix});
    }
  }
  if (frames.empty())
    frames.push_back({});
  return frames;
}
inline Project makeTemplate(int type, std::string name, int profile) {
  Project p;
  p.root["name"] = name;
  auto &settings = p.root["reyondbroom"];
  settings["target_format"] = profile == 2   ? "mdl"
                              : profile == 3 ? "md2"
                              : profile == 4 ? "studio"
                                             : "md3";
  settings["export_path"] = profile == 2   ? "progs/" + name + ".mdl"
                            : profile == 3 ? "models/" + name + "/tris.md2"
                            : profile == 4 ? "models/" + name + ".mdl"
                            : profile == 0 ? "models/mapobjects/" + name + "/" + name + ".md3"
                                           : "models/" + name + ".md3";
  p.root["meta"]["box_uv"] = type == 1 || type == 2;
  std::map<std::string, std::string> ids;
  auto addGroup = [&](std::string n, Vec3 origin, std::string parent = "") {
    auto g = group(n, origin);
    std::string id = g["uuid"];
    p.root["groups"].push_back(g);
    appendNode(p, id, parent.empty() ? "" : ids.at(parent), true);
    ids[n] = id;
  };
  auto addCube = [&](std::string n, Vec3 from, Vec3 to, int color) {
    auto c = cube(n, from, to);
    c["color"] = color;
    c["box_uv"] = p.root["meta"]["box_uv"];
    c["origin"] = getv(*p.find(ids[n]), "origin");
    std::string id = c["uuid"];
    p.root["elements"].push_back(c);
    appendNode(p, id, ids[n]);
  };
  if (type == 0)
    addGroup("root", {});
  if (type == 1) {
    addGroup("body", {0, 4, 0});
    addGroup("torso", {0, 4, 0}, "body");
    addGroup("head", {0, 22, 0}, "torso");
    addGroup("arm_left", {9, 20, 0}, "torso");
    addGroup("arm_right", {-9, 20, 0}, "torso");
    addGroup("leg_left", {3.5, 4, 0}, "body");
    addGroup("leg_right", {-3.5, 4, 0}, "body");
    addCube("torso", {-7, 4, -4}, {7, 22, 4}, 1);
    addCube("head", {-5, 22, -5}, {5, 32, 5}, 2);
    addCube("arm_left", {7, 0, -2.5}, {12, 22, 2.5}, 3);
    addCube("arm_right", {-12, 0, -2.5}, {-7, 22, 2.5}, 3);
    addCube("leg_left", {1, -24, -2.5}, {6, 4, 2.5}, 4);
    addCube("leg_right", {-6, -24, -2.5}, {-1, 4, 2.5}, 4);
    for (auto &e : p.root["elements"])
      if (e.value("name", "").find("right") != std::string::npos)
        e["mirror_uv"] = true;
  } else if (type == 2) {
    addGroup("gun", {5, -8, -10});
    for (auto bounds : {std::pair<Vec3, Vec3>{{3, -10, -22}, {7, -6, -6}},
                        {{4, -8.5, -30}, {6, -6.5, -22}},
                        {{4, -15, -12}, {6, -10, -9}}})
      addCube("gun", bounds.first, bounds.second, 3);
  } else if (type == 3) {
    addGroup("item", {0, 8, 0});
    addCube("item", {-8, 0, -8}, {8, 16, 8}, 4);
    settings["rotate_pickup"] = true;
  } else if (type == 4) {
    addGroup("prop", {});
    addCube("prop", {-16, 0, -16}, {16, 32, 16}, 1);
  }
  auto anim = [&](std::string n, double len, std::string loop, double fps = 10) {
    return Json{{"uuid", uuid()},
                {"name", n},
                {"length", len},
                {"loop", loop},
                {"snapping", 24},
                {"animators", Json::object()},
                {"reyondbroom", {{"export_fps", fps}, {"export_name", n}}}};
  };
  if (type == 1) {
    auto stand = anim("stand", 2, "loop"), walk = anim("walk", 1, "loop"),
         death = anim("death", .8, "once");
    for (int i = 0; i < 3; i++) {
      setKey(stand, ids["torso"], "rotation", i, {i == 1 ? 3. : 0, 0, 0});
      setKey(stand, ids["head"], "rotation", i, {i == 1 ? -3. : 0, 0, 0});
      for (auto part : {"leg_left", "leg_right", "arm_left", "arm_right"}) {
        double v = std::string(part).find("leg") == 0 ? 30 : 25;
        if (std::string(part).find("right") != std::string::npos)
          v = -v;
        if (std::string(part).find("arm") == 0)
          v = -v;
        setKey(walk, ids[part], "rotation", i * .5, {i == 1 ? -v : v, 0, 0});
      }
    }
    for (auto &track : stand["animators"])
      for (auto &k : track["keyframes"])
        k["interpolation"] = "catmullrom";
    for (int i = 0; i < 5; i++)
      setKey(walk, ids["body"], "position", i * .25, {0, i % 2 ? 1. : 0, 0});
    setKey(death, ids["body"], "rotation", 0, {});
    setKey(death, ids["body"], "rotation", .8, {-90, 0, 0});
    setKey(death, ids["body"], "position", 0, {});
    setKey(death, ids["body"], "position", .8, {0, -20, 0});
    for (auto &track : death["animators"])
      for (auto &k : track["keyframes"])
        k["interpolation"] = "bezier";
    p.root["animations"] = {stand, walk, death};
  }
  if (type == 2) {
    auto shot = anim("shot", .4, "once", 15);
    for (auto t : {0., .05, .4}) {
      setKey(shot, ids["gun"], "position", t, {0, 0, t == .05 ? 3. : 0});
      setKey(shot, ids["gun"], "rotation", t, {t == .05 ? 8. : 0, 0, 0});
    }
    p.root["animations"] = {shot};
  }
  validate(p);
  return p;
}
} // namespace ftebroom::bb
