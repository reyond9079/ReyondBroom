// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_math.h"
#include "third_party/json.hpp"
#include <functional>
#include <iomanip>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <vector>
namespace ftebroom::bb {
using Json = nlohmann::json;
inline std::string uuid() {
  static std::mt19937_64 rng(std::random_device{}());
  unsigned char b[16];
  for (int i = 0; i < 16; i++)
    b[i] = rng();
  b[6] = (b[6] & 15) | 64;
  b[8] = (b[8] & 63) | 128;
  std::ostringstream s;
  s << std::hex << std::setfill('0');
  for (int i = 0; i < 16; i++) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      s << '-';
    s << std::setw(2) << unsigned(b[i]);
  }
  return s.str();
}
inline Vec3 vec(const Json &j, Vec3 fallback = {}) {
  if (!j.is_array() || j.size() < 3)
    return fallback;
  for (int a = 0; a < 3; a++) {
    if (j[a].is_number())
      fallback[a] = j[a].get<double>();
    else if (j[a].is_string())
      try {
        size_t n;
        auto text = j[a].get<std::string>();
        double v = std::stod(text, &n);
        fallback[a] = n == text.size() ? v : 0;
      } catch (...) {
        fallback[a] = 0;
      }
  }
  return fallback;
}
inline Vec3 getv(const Json &j, const char *key, Vec3 fallback = {}) {
  return j.contains(key) ? vec(j[key], fallback) : fallback;
}
inline const std::array<const char *, 6> faceNames{"north", "east", "south", "west", "up", "down"};
struct Texture {
  std::string id, name, path;
  int width = 16, height = 16, uvWidth = 16, uvHeight = 16;
  std::vector<unsigned char> rgba;
  bool saved = true;
  uint64_t revision = 1;
};
// Schema-preserving JSON objects retain fields unknown to this version, including
// unsupported node/animation types. Geometry and image buffers remain typed.
struct Selection {
  std::set<std::string> elements, groups, keyframes;
  std::string activeGroup, last;
  std::map<std::string, std::set<std::string>> vertices, faces, edges;
};
struct Project {
  Json root;
  std::vector<Texture> textures;
  uint64_t revision = 1;
  std::string path;
  std::vector<std::string> warnings;
  Project() {
    root = {{"meta", {{"format_version", "5.0"}, {"model_format", "free"}, {"box_uv", false}}},
            {"name", "untitled"},
            {"resolution", {{"width", 64}, {"height", 64}}},
            {"elements", Json::array()},
            {"groups", Json::array()},
            {"outliner", Json::array()},
            {"textures", Json::array()},
            {"animations", Json::array()},
            {"reyondbroom",
             {{"export_scale", 1.0},
              {"front", "north"},
              {"target_format", "md3"},
              {"include_base_frame", true}}},
            {"editor_state",
             {{"mode", "edit"},
              {"selected_elements", Json::array()},
              {"selected_groups", Json::array()}}}};
  }
  Json *find(const std::string &id) {
    for (auto key : {"elements", "groups"})
      for (auto &e : root[key])
        if (e.value("uuid", "") == id)
          return &e;
    return nullptr;
  }
  const Json *find(const std::string &id) const {
    return const_cast<Project *>(this)->find(id);
  }
  Texture *texture(int i) {
    return i >= 0 && size_t(i) < textures.size() ? &textures[i] : nullptr;
  }
  int front() const {
    auto f = root["reyondbroom"].value("front", "north");
    return f == "south" ? 1 : f == "east" ? 2 : f == "west" ? 3 : 0;
  }
  double exportScale() const {
    return root["reyondbroom"].value("export_scale", 1.0);
  }
};
inline Json cube(std::string name = "cube", Vec3 from = {}, Vec3 to = {2, 2, 2}) {
  Json faces = Json::object();
  for (auto n : faceNames)
    faces[n] = {{"uv", {0, 0, 16, 16}}, {"texture", false}, {"rotation", 0}};
  return {{"type", "cube"},        {"uuid", uuid()},     {"name", name},
          {"from", from},          {"to", to},           {"origin", {0, 0, 0}},
          {"rotation", {0, 0, 0}}, {"inflate", 0},       {"stretch", {1, 1, 1}},
          {"box_uv", false},       {"mirror_uv", false}, {"uv_offset", {0, 0}},
          {"faces", faces},        {"visibility", true}, {"export", true},
          {"locked", false},       {"color", 0}};
}
inline Json group(std::string name = "group", Vec3 origin = {}) {
  return {{"uuid", uuid()},     {"name", name},   {"origin", origin}, {"rotation", {0, 0, 0}},
          {"visibility", true}, {"export", true}, {"isOpen", true}};
}
inline void appendNode(Project &p, const std::string &id, const std::string &parent = "",
                       bool isGroup = false) {
  Json node =
      isGroup ? Json{{"uuid", id}, {"isOpen", true}, {"children", Json::array()}} : Json(id);
  std::function<bool(Json &)> visit = [&](Json &list) {
    for (auto &n : list)
      if (n.is_object()) {
        if (n.value("uuid", "") == parent) {
          n["children"].push_back(node);
          return true;
        }
        if (visit(n["children"]))
          return true;
      }
    return false;
  };
  if (parent.empty() || !visit(p.root["outliner"]))
    p.root["outliner"].push_back(node);
}
inline std::map<std::string, std::string> parents(const Project &p) {
  std::map<std::string, std::string> out;
  std::function<void(const Json &, std::string)> walk = [&](const Json &list, std::string parent) {
    for (auto &n : list) {
      std::string id = n.is_string() ? n.get<std::string>() : n.value("uuid", "");
      out[id] = parent;
      if (n.is_object() && n.contains("children"))
        walk(n["children"], id);
    }
  };
  walk(p.root["outliner"], "");
  return out;
}
inline void normalizeCube(Json &c) {
  auto a = getv(c, "from"), b = getv(c, "to");
  for (int i = 0; i < 3; i++)
    if (a[i] > b[i]) {
      std::swap(a[i], b[i]);
      for (auto &f : c["faces"]) {
        if (f.contains("uv")) {
          std::swap(f["uv"][0], f["uv"][2]);
        }
      }
    }
  c["from"] = a;
  c["to"] = b;
}
inline void validate(const Project &p) {
  if (!p.root.is_object() || !p.root["elements"].is_array() || !p.root["groups"].is_array() ||
      !p.root["outliner"].is_array())
    throw std::runtime_error("Invalid model document collections");
  if (p.root["elements"].size() > 4096 || p.root["groups"].size() > 512 ||
      p.root["textures"].size() > 64 || p.root["animations"].size() > 256)
    throw std::runtime_error("Model document budget exceeded");
  std::set<std::string> all, seen;
  for (auto key : {"elements", "groups"})
    for (auto &e : p.root[key]) {
      auto id = e.value("uuid", "");
      if (id.empty() || !all.insert(id).second)
        throw std::runtime_error("Missing or duplicate object UUID");
      if (e.value("type", "") == "mesh") {
        for (auto &point : e["vertices"])
          for (double n : vec(point))
            if (!std::isfinite(n) || std::abs(n) > 1e7)
              throw std::runtime_error("Invalid mesh vertex");
        if (e["faces"].size() > 131072)
          throw std::runtime_error("Mesh face budget exceeded");
        if (e["vertices"].size() > 65536)
          throw std::runtime_error("Mesh exceeds 65536 vertices");
        for (auto &f : e["faces"]) {
          auto keys = f["vertices"].get<std::vector<std::string>>();
          if (keys.size() < 3 || keys.size() > 4 ||
              std::set<std::string>(keys.begin(), keys.end()).size() != keys.size())
            throw std::runtime_error("Mesh face requires 3 or 4 distinct vertices");
          for (auto &k : keys)
            if (!e["vertices"].contains(k))
              throw std::runtime_error("Face references missing vertex");
        }
      }
      for (auto number : {"inflate", "light_emission"})
        if (e.contains(number) && e[number].is_number() &&
            (!std::isfinite(e[number].get<double>()) || std::abs(e[number].get<double>()) > 1e7))
          throw std::runtime_error("Invalid numeric element property");
      for (auto key2 : {"from", "to", "origin", "rotation", "position", "stretch"})
        if (e.contains(key2))
          for (double v : vec(e[key2]))
            if (!std::isfinite(v) || std::abs(v) > 1e7)
              throw std::runtime_error("Invalid model coordinate");
      if (e.contains("faces"))
        for (auto &f : e["faces"])
          if (f.contains("uv")) {
            auto validUV = [](const Json &uv, int expected) {
              if (!uv.is_array() || uv.size() != size_t(expected))
                throw std::runtime_error("Invalid face UV coordinates");
              for (auto &number : uv)
                if (!number.is_number() || !std::isfinite(number.get<double>()) ||
                    std::abs(number.get<double>()) > 1e7)
                  throw std::runtime_error("Invalid face UV coordinate");
            };
            if (e.value("type", "") == "cube")
              validUV(f["uv"], 4);
            else if (e.value("type", "") == "mesh")
              for (auto &uv : f["uv"])
                validUV(uv, 2);
          }
      if (e.contains("faces"))
        for (auto &f : e["faces"])
          if (f.contains("texture") && f["texture"].is_number_integer() &&
              (f["texture"].get<int>() < -1 ||
               f["texture"].get<int>() >= int(p.root["textures"].size())))
            throw std::runtime_error("Invalid face texture index");
    }
  std::function<void(const Json &, int)> walk = [&](const Json &list, int depth) {
    if (depth > 512)
      throw std::runtime_error("Outliner depth exceeded");
    for (auto &n : list) {
      auto id = n.is_string() ? n.get<std::string>() : n.value("uuid", "");
      if (!all.count(id) || !seen.insert(id).second)
        throw std::runtime_error("Unknown, repeated or cyclic outliner UUID");
      if (n.is_object()) {
        bool isGroup = false;
        for (auto &g : p.root["groups"])
          if (g.value("uuid", "") == id)
            isGroup = true;
        if (!isGroup || !n.contains("children") || !n["children"].is_array())
          throw std::runtime_error("Only groups may have children");
        walk(n["children"], depth + 1);
      }
    }
  };
  walk(p.root["outliner"], 0);
  if (all != seen)
    throw std::runtime_error("Every element and group must occur once in the outliner");
  size_t keyCount = 0;
  for (auto &animation : p.root["animations"])
    for (auto &track : animation["animators"])
      for (auto &key : track["keyframes"]) {
        if ((key.value("channel", "") == "position" || key.value("channel", "") == "rotation" ||
             key.value("channel", "") == "scale") &&
            (!key.contains("data_points") || !key["data_points"].is_array() ||
             key["data_points"].empty() || key["data_points"].size() > 2))
          throw std::runtime_error("Transform keys need one or two data points");
        if (++keyCount > 262144)
          throw std::runtime_error("Animation keyframe budget exceeded");
        double time = key.value("time", 0.);
        if (!std::isfinite(time) || time < 0 || time > 86400)
          throw std::runtime_error("Invalid keyframe time");
      }
  for (auto &t : p.root["textures"])
    for (auto k : {"width", "height", "uv_width", "uv_height"})
      if (t.contains(k)) {
        double n = t[k].get<double>();
        if (!std::isfinite(n) || n < 1 || n > 2048)
          throw std::runtime_error("Texture dimensions must be 1 to 2048");
      }
  if (!std::isfinite(p.exportScale()) || p.exportScale() <= 0 || p.exportScale() > 10000)
    throw std::runtime_error("Invalid export scale");
}
inline void deleteTexture(Project &p, int index) {
  if (index < 0 || index >= int(p.textures.size()))
    return;
  p.textures.erase(p.textures.begin() + index);
  p.root["textures"].erase(index);
  for (auto &e : p.root["elements"])
    if (e.contains("faces"))
      for (auto &f : e["faces"])
        if (f.contains("texture") && f["texture"].is_number_integer()) {
          int v = f["texture"];
          f["texture"] = v == index ? Json(false) : Json(v > index ? v - 1 : v);
        }
  p.revision++;
}
} // namespace ftebroom::bb
