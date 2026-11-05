// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_io.h"
namespace ftebroom::bb {
// OBJ indices are local to their own stream, can be negative, and UV corners
// are separate from shared position vertices. Quads are retained; larger faces
// are triangulated without discarding their material or corner UVs.
inline Project importObj(const std::string &bytes, ResourceReader reader = {},
                         std::string name = "imported") {
  if (bytes.size() > 64 * 1024 * 1024)
    throw std::runtime_error("OBJ exceeds 64 MiB");
  Project p;
  p.root["name"] = name;
  std::vector<Vec3> positions;
  std::vector<Vec2> uvs;
  std::map<std::string, int> materials;
  std::map<std::string, std::string> images;
  std::string active = "default", library;
  Json *mesh = nullptr;
  auto create = [&](std::string label) {
    Json m = {{"uuid", uuid()},         {"type", "mesh"},        {"name", label},
              {"origin", {0, 0, 0}},    {"rotation", {0, 0, 0}}, {"vertices", Json::object()},
              {"faces", Json::object()}};
    p.root["elements"].push_back(m);
    appendNode(p, m["uuid"]);
    mesh = &p.root["elements"].back();
  };
  auto material = [&](const std::string &id) {
    if (materials.count(id))
      return materials[id];
    Texture t;
    t.id = uuid();
    t.name = id;
    t.path = images[id];
    if (reader && !t.path.empty())
      try {
        t = decodeImage(reader(t.path));
        t.id = uuid();
        t.name = id;
        t.path = images[id];
      } catch (...) {
        p.warnings.push_back("Missing OBJ image: " + images[id]);
      }
    if (t.rgba.empty()) {
      t.width = t.height = 16;
      t.rgba.assign(16 * 16 * 4, 200);
    }
    t.uvWidth = t.width;
    t.uvHeight = t.height;
    int n = p.textures.size();
    addTexture(p, t);
    return materials[id] = n;
  };
  auto index = [](std::string text, size_t count) {
    int n = std::stoi(text);
    long i = n < 0 ? long(count) + n : n - 1;
    if (n == 0 || i < 0 || i >= long(count))
      throw std::runtime_error("Invalid OBJ index");
    return size_t(i);
  };
  std::istringstream lines(bytes);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream row(line);
    std::string op;
    row >> op;
    if (op.empty() || op[0] == '#')
      continue;
    if (op == "v") {
      Vec3 v{};
      if (!(row >> v[0] >> v[1] >> v[2]))
        throw std::runtime_error("Invalid OBJ position");
      if (positions.size() >= 65536)
        throw std::runtime_error("OBJ exceeds 65536 positions");
      positions.push_back(v);
    } else if (op == "vt") {
      Vec2 v{};
      row >> v[0] >> v[1];
      uvs.push_back(v);
    } else if (op == "o" || op == "g") {
      std::string n;
      std::getline(row, n);
      create(n.empty() ? "mesh" : n.substr(1));
    } else if (op == "usemtl")
      row >> active;
    else if (op == "mtllib" && reader) {
      row >> library;
      try {
        std::istringstream mtl(reader(library));
        std::string l, id;
        while (std::getline(mtl, l)) {
          std::istringstream r(l);
          std::string cmd;
          r >> cmd;
          if (cmd == "newmtl")
            r >> id;
          else if (cmd == "map_Kd") {
            std::string path;
            std::getline(r, path);
            if (!path.empty())
              images[id] = path.substr(1);
          }
        }
      } catch (...) {
        p.warnings.push_back("Missing OBJ material library: " + library);
      }
    } else if (op == "f") {
      if (!mesh)
        create("mesh");
      std::vector<std::string> keys;
      Json corner = Json::object();
      std::string token;
      int tex = material(active);
      while (row >> token) {
        auto slash = token.find('/'),
             next = slash == std::string::npos ? slash : token.find('/', slash + 1);
        size_t position = index(token.substr(0, slash), positions.size());
        auto key = "v" + std::to_string(position);
        (*mesh)["vertices"][key] = positions[position];
        keys.push_back(key);
        Vec2 uv{};
        if (slash != std::string::npos && next != slash + 1 && slash + 1 < token.size()) {
          auto at =
              index(token.substr(slash + 1, next == std::string::npos ? next : next - slash - 1),
                    uvs.size());
          uv = uvs[at];
        }
        corner[key] = {uv[0] * p.textures[tex].uvWidth, (1 - uv[1]) * p.textures[tex].uvHeight};
      }
      if (keys.size() < 3)
        throw std::runtime_error("OBJ face needs three corners");
      auto emit = [&](std::vector<std::string> v) {
        auto f = "f" + std::to_string((*mesh)["faces"].size());
        (*mesh)["faces"][f] = {{"vertices", v}, {"uv", corner}, {"texture", tex}};
      };
      if (keys.size() <= 4)
        emit(keys);
      else
        for (size_t i = 1; i + 1 < keys.size(); i++)
          emit({keys[0], keys[i], keys[i + 1]});
    }
  }
  validate(p);
  return p;
}
} // namespace ftebroom::bb
