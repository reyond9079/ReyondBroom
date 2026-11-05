// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_geometry.h"
#include <filesystem>
#include <fstream>
#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#endif
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#ifdef _WIN32
#include <windows.h>
#endif
namespace ftebroom::bb {
inline std::string base64(const std::string &in) {
  static const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  unsigned bits = 0;
  int n = 0;
  for (unsigned char c : in) {
    bits = (bits << 8) | c;
    n += 8;
    while (n >= 6) {
      n -= 6;
      out += chars[(bits >> n) & 63];
    }
  }
  if (n)
    out += chars[(bits << (6 - n)) & 63];
  while (out.size() % 4)
    out += '=';
  return out;
}
inline std::string unbase64(const std::string &in) {
  std::string out;
  unsigned bits = 0;
  int n = 0;
  bool pad = false;
  for (unsigned char c : in) {
    if (std::isspace(c))
      continue;
    if (c == '=') {
      pad = true;
      continue;
    }
    if (pad)
      throw std::runtime_error("Invalid base64 padding");
    const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *p = std::strchr(chars, c);
    if (!p || !c)
      throw std::runtime_error("Invalid base64 image");
    bits = (bits << 6) | unsigned(p - chars);
    n += 6;
    if (n >= 8) {
      n -= 8;
      out += char(bits >> n);
    }
  }
  if (n >= 6)
    throw std::runtime_error("Truncated base64 image");
  return out;
}
inline Texture decodeImage(const std::string &bytes) {
  int w = 0, h = 0, c = 0;
  if (bytes.size() > 32 * 1024 * 1024 ||
      !stbi_info_from_memory(reinterpret_cast<const stbi_uc *>(bytes.data()), bytes.size(), &w, &h,
                             &c) ||
      w < 1 || h < 1 || w > 2048 || h > 2048)
    throw std::runtime_error("Invalid image or dimensions exceed 2048");
  auto data = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(bytes.data()), bytes.size(),
                                    &w, &h, &c, 4);
  if (!data)
    throw std::runtime_error("Image decode failed");
  Texture t;
  t.id = uuid();
  t.width = t.uvWidth = w;
  t.height = t.uvHeight = h;
  t.rgba.assign(data, data + size_t(w) * h * 4);
  stbi_image_free(data);
  return t;
}
inline std::string png(const Texture &t) {
  std::string out;
  stbi_write_png_to_func(
      +[](void *ctx, void *data, int n) {
        static_cast<std::string *>(ctx)->append(static_cast<char *>(data), n);
      },
      &out, t.width, t.height, 4, t.rgba.data(), t.width * 4);
  return out;
}
inline std::string tga(const Texture &t) {
  std::string out;
  stbi_write_tga_to_func(
      +[](void *ctx, void *data, int n) {
        static_cast<std::string *>(ctx)->append(static_cast<char *>(data), n);
      },
      &out, t.width, t.height, 4, t.rgba.data());
  return out;
}
inline std::string readFile(const std::filesystem::path &path, size_t limit = 128 * 1024 * 1024) {
  if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > limit)
    throw std::runtime_error("Missing file or file exceeds budget");
  std::ifstream in(path, std::ios::binary);
  std::string out((std::istreambuf_iterator<char>(in)), {});
  if (!in.eof() && in.fail())
    throw std::runtime_error("Cannot read model file");
  return out;
}
inline void atomicWrite(const std::filesystem::path &path, const std::string &bytes) {
  auto tmp = path;
  tmp += "." + uuid() + ".tmp";
  try {
    std::ofstream out(tmp, std::ios::binary);
    out.write(bytes.data(), bytes.size());
    out.close();
    if (!out)
      throw std::runtime_error("Cannot write model file");
#ifdef _WIN32
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      throw std::runtime_error("Cannot replace model file");
#else
    std::filesystem::rename(tmp, path);
#endif
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    throw;
  }
}
using ResourceReader = std::function<std::string(const std::string &)>;
inline bool versionBefore(std::string a, std::string b) {
  std::replace(a.begin(), a.end(), '.', ' ');
  std::replace(b.begin(), b.end(), '.', ' ');
  std::istringstream x(a), y(b);
  for (int i = 0; i < 3; i++) {
    int av = 0, bv = 0;
    x >> av;
    y >> bv;
    if (av != bv)
      return av < bv;
  }
  return false;
}
inline Project parseProject(const std::string &text, ResourceReader resource = {}) {
  if (text.size() > 128 * 1024 * 1024)
    throw std::runtime_error("Project exceeds 128 MiB input budget");
  Json j;
  try {
    j = Json::parse(text, [](int depth, Json::parse_event_t, Json &) {
      if (depth > 512)
        throw std::runtime_error("Project nesting exceeds 512 levels");
      return true;
    });
  } catch (const std::exception &) {
    throw std::runtime_error("Invalid bbmodel JSON");
  }
  if (!j.is_object() || !j.contains("meta"))
    throw std::runtime_error("Missing bbmodel header");
  Project p;
  std::string version =
      j["meta"].value("format_version", j["meta"].value("format", std::string("4.0")));
  if (versionBefore("5.0", version))
    p.warnings.push_back("Newer project version: reading known fields");
  if (j["meta"].value("model_format", "free") != "free")
    p.warnings.push_back("Opened as Generic Model; original model_format retained");
  if (j.contains("cubes") && !j.contains("elements"))
    j["elements"] = j["cubes"];
  p.root.update(j);
  for (auto key : {"elements", "groups", "textures", "animations", "outliner"})
    if (!p.root[key].is_array())
      throw std::runtime_error("Invalid project collection");
  if (!p.root["reyondbroom"].is_object())
    p.root["reyondbroom"] = Project().root["reyondbroom"];
  std::set<std::string> groupIds;
  for (auto &g : p.root["groups"])
    groupIds.insert(g.value("uuid", ""));
  std::function<void(Json &)> legacy = [&](Json &list) {
    for (auto &n : list)
      if (n.is_object()) {
        if (!n.contains("uuid"))
          n["uuid"] = uuid();
        auto id = n["uuid"].get<std::string>();
        if (!groupIds.count(id)) {
          auto g = n;
          g.erase("children");
          p.root["groups"].push_back(g);
          groupIds.insert(id);
        }
        if (!n.contains("children"))
          n["children"] = Json::array();
        legacy(n["children"]);
      }
  };
  legacy(p.root["outliner"]);
  for (auto &e : p.root["elements"]) {
    if (!e.contains("type"))
      e["type"] = "cube";
    if (e.value("type", "") == "cube") {
      if (!e.contains("box_uv"))
        e["box_uv"] = p.root["meta"].value("box_uv", false);
      if (versionBefore(version, "4.5") && e.value("shade", true) == false)
        e["mirror_uv"] = true;
      normalizeCube(e);
    }
    if (e.value("type", "") == "mesh") {
      Json faces = Json::object();
      for (auto it = e["faces"].begin(); it != e["faces"].end(); ++it) {
        auto f = it.value();
        auto keys = f["vertices"].get<std::vector<std::string>>();
        if (keys.size() > 4) {
          for (size_t n = 1; n + 1 < keys.size(); n++) {
            auto tri = f;
            tri["vertices"] = {keys[0], keys[n], keys[n + 1]};
            faces[it.key() + "_" + std::to_string(n)] = tri;
          }
        } else
          faces[it.key()] = f;
      }
      e["faces"] = faces;
    }
  }
  if (versionBefore(version, "5.0"))
    for (auto &animation : p.root["animations"])
      for (auto &track : animation["animators"])
        for (auto &key : track["keyframes"]) {
          auto channel = key.value("channel", "");
          for (auto &point : key["data_points"])
            for (auto axis : {"x", "y"})
              if ((std::string(axis) == "x" && (channel == "rotation" || channel == "position")) ||
                  (std::string(axis) == "y" && channel == "rotation")) {
                if (!point.contains(axis))
                  continue;
                auto &value = point[axis];
                if (value.is_number())
                  value = -value.get<double>();
                else if (value.is_string()) {
                  auto text = value.get<std::string>();
                  try {
                    size_t end;
                    double n = std::stod(text, &end);
                    if (end == text.size())
                      value = std::to_string(-n);
                    else
                      value = "-(" + text + ")";
                  } catch (...) {
                    value = "-(" + text + ")";
                  }
                }
              }
          for (auto field : {"bezier_left_value", "bezier_right_value"})
            if (key.contains(field)) {
              if (channel == "rotation" || channel == "position")
                key[field][0] = -key[field][0].get<double>();
              if (channel == "rotation")
                key[field][1] = -key[field][1].get<double>();
            }
        }
  if (versionBefore(version, "3.2"))
    for (auto &group : p.root["groups"])
      if (group.contains("rotation"))
        group["rotation"][2] = -group["rotation"][2].get<double>();
  p.root["meta"]["format_version"] = "5.0";
  validate(p);
  size_t decodedBytes = 0;
  for (auto &meta : p.root["textures"]) {
    Texture t;
    auto source = meta.value("source", "");
    if (source.rfind("data:image/", 0) == 0) {
      auto comma = source.find(',');
      if (comma == std::string::npos)
        throw std::runtime_error("Invalid embedded image URL");
      t = decodeImage(unbase64(source.substr(comma + 1)));
    } else if (resource) {
      auto path = meta.value("relative_path", meta.value("path", ""));
      if (!path.empty())
        try {
          t = decodeImage(resource(path));
        } catch (...) {
          p.warnings.push_back("Missing texture: " + path);
        }
    }
    if (t.rgba.empty()) {
      t.width = meta.value("width", 16);
      t.height = meta.value("height", 16);
      t.rgba.resize(size_t(t.width) * t.height * 4);
      for (size_t n = 0; n < t.rgba.size(); n += 4) {
        t.rgba[n] = 255;
        t.rgba[n + 2] = 255;
        t.rgba[n + 3] = 255;
      }
    }
    t.id = meta.value("uuid", uuid());
    meta["uuid"] = t.id;
    t.name = meta.value("name", "texture.png");
    t.path = meta.value("relative_path", meta.value("path", ""));
    t.uvWidth = meta.value("uv_width", p.root["resolution"].value("width", 64));
    t.uvHeight = meta.value("uv_height", p.root["resolution"].value("height", 64));
    if (meta.contains("layers"))
      p.warnings.push_back(
          "Texture layers opened as flattened source; original layer metadata retained");
    decodedBytes += t.rgba.size();
    if (decodedBytes > 256 * 1024 * 1024)
      throw std::runtime_error("Decoded model textures exceed 256 MiB");
    p.textures.push_back(std::move(t));
  }
  bool expressionWarning = false;
  for (auto &animation : p.root["animations"])
    for (auto &track : animation["animators"])
      for (auto &key : track["keyframes"])
        for (auto &point : key["data_points"])
          for (auto axis : {"x", "y", "z"})
            if (point.contains(axis) && point[axis].is_string()) {
              auto value = point[axis].get<std::string>();
              try {
                size_t n;
                std::stod(value, &n);
                if (n != value.size())
                  expressionWarning = true;
              } catch (...) {
                expressionWarning = true;
              }
            }
  if (expressionWarning)
    p.warnings.push_back(
        "Molang expressions are retained; unsupported expressions preview as zero");
  return p;
}
inline std::string serialize(const Project &p, bool embed = true) {
  auto j = p.root;
  if (!j["editor_state"].contains("selected_elements"))
    j["editor_state"]["selected_elements"] = Json::array();
  if (!j["editor_state"].contains("selected_groups"))
    j["editor_state"]["selected_groups"] = Json::array();
  for (size_t i = 0; i < p.textures.size(); i++) {
    auto &t = p.textures[i];
    auto &m = j["textures"][i];
    m["uuid"] = t.id;
    m["name"] = t.name;
    m["width"] = t.width;
    m["height"] = t.height;
    m["uv_width"] = t.uvWidth;
    m["uv_height"] = t.uvHeight;
    m["path"] = t.path;
    if (!t.path.empty())
      m["relative_path"] = t.path;
    else
      m.erase("relative_path");
    if (embed || t.path.empty()) {
      m["source"] = "data:image/png;base64," + base64(png(t));
      m["internal"] = true;
    } else {
      m.erase("source");
      m["path"] = t.path;
    }
  }
  return j.dump(2);
}
inline void addTexture(Project &p, Texture t) {
  if (p.textures.size() >= 64)
    throw std::runtime_error("At most 64 textures per project");
  if (t.rgba.size() != size_t(t.width) * t.height * 4)
    throw std::runtime_error("Invalid texture pixel buffer");
  if (t.id.empty())
    t.id = uuid();
  p.root["textures"].push_back({{"uuid", t.id},
                                {"name", t.name},
                                {"width", t.width},
                                {"height", t.height},
                                {"uv_width", t.uvWidth},
                                {"uv_height", t.uvHeight}});
  p.textures.push_back(std::move(t));
  p.revision++;
}
} // namespace ftebroom::bb
