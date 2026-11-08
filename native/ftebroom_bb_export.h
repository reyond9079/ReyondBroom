// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_anim.h"
#include <atomic>
#include "ftebroom_model_export.h"
namespace ftebroom::bb {
struct Baked {
  modelExport::Mesh mesh;
  std::vector<int> textures;
  std::vector<std::string> warnings;
};
inline Baked bake(const Project &p, bool animated = true,const std::atomic<bool>* cancel=nullptr) {
  Baked result;
  auto frames = animated ? bakeFrames(p) : std::vector<BakeFrame>{{}};
  std::map<int, size_t> surfaces;
  std::vector<std::pair<std::string, std::string>> topology;
  for (size_t frame = 0; frame < frames.size(); frame++) {
    if(cancel&&cancel->load())throw std::runtime_error("Model export cancelled");
    auto &f = frames[frame];
    auto poses = f.animation < 0 ? std::map<std::string, Pose>{}
                                 : animationPose(p.root["animations"][f.animation], f.time);
    auto ts = triangles(p, true, poses);
    std::map<int, std::vector<const Triangle *>> grouped;
    for (auto &t : ts)
      grouped[t.texture].push_back(&t);
    if (!frame) {
      for (auto &[texture, triangles] : grouped) {
        surfaces[texture] = result.mesh.surfaces.size();
        result.mesh.surfaces.emplace_back();
        result.textures.push_back(texture);
      }
      if (grouped.empty())
        throw std::runtime_error("The project has no exportable faces");
    }
    if (grouped.size() != surfaces.size())
      throw std::runtime_error("Animation changed model topology");
    for (auto &[texture, tris] : grouped) {
      auto &surface = result.mesh.surfaces.at(surfaces.at(texture));
      surface.material = texture >= 0 && size_t(texture) < p.textures.size()
                             ? p.textures[texture].path
                             : "bb-missing-texture";
      std::vector<modelExport::Vertex> vertices;
      for (auto t : tris) {
        if(cancel&&cancel->load())throw std::runtime_error("Model export cancelled");
        for (int i = 0; i < 3; i++) {
          auto pos = bbToQuake(t->p[i], p.front(), p.exportScale()),
               normal = unit(bbToQuake(t->n[i], p.front()));
          auto uv = t->uv[i];
          if (texture >= 0 && size_t(texture) < p.textures.size()) {
            uv[0] /= p.textures[texture].uvWidth;
            uv[1] /= p.textures[texture].uvHeight;
          }
          modelExport::Vertex v;
          for (int a = 0; a < 3; a++) {
            v.position[a] = pos[a];
            v.normal[a] = normal[a];
          }
          v.uv = {float(uv[0]), float(uv[1])};
          vertices.push_back(v);
        }
        if (!frame) {
          unsigned n = vertices.size();
          surface.triangles.push_back({n - 3, n - 2, n - 1});
        }
      }
      if (frame && vertices.size() != surface.frames[0].size())
        throw std::runtime_error("Animated topology changed");
      surface.frames.push_back(std::move(vertices));
    }
    result.mesh.frameNames.push_back(f.name);
  }
  result.mesh.frames = frames.size();
  for (auto index : result.textures)
    if (index < 0)
      result.warnings.push_back("Untextured faces use a neutral placeholder");
  for (auto &e : p.root["elements"])
    if (e.value("type", "") == "locator")
      result.warnings.push_back("This model writer does not retain locator tags");
  return result;
}
inline std::string frameList(const Project &p) {
  std::string out = "$frame";
  for (auto &f : bakeFrames(p))
    out += " " + f.name;
  return out;
}
inline Project importMesh(const modelExport::Mesh &source, std::string name = "imported") {
  Project p;
  p.root["name"] = name;
  for (size_t index = 0; index < source.surfaces.size(); index++) {
    auto &surface = source.surfaces[index];
    if (surface.frames.empty())
      continue;
    auto g = group("surface" + std::to_string(index));
    p.root["groups"].push_back(g);
    appendNode(p, g["uuid"], "", true);
    Json m = {{"uuid", uuid()},
              {"type", "mesh"},
              {"name", "surface" + std::to_string(index)},
              {"origin", {0, 0, 0}},
              {"rotation", {0, 0, 0}},
              {"vertices", Json::object()},
              {"faces", Json::object()}};
    for (size_t i = 0; i < surface.frames[0].size(); i++) {
      auto v = surface.frames[0][i];
      m["vertices"]["v" + std::to_string(i)] =
          quakeToBb({v.position[0], v.position[1], v.position[2]});
    }
    for (size_t i = 0; i < surface.triangles.size(); i++) {
      Json f = {{"vertices", Json::array()}, {"uv", Json::object()}, {"texture", int(index)}};
      for (auto n : surface.triangles[i]) {
        auto k = "v" + std::to_string(n);
        f["vertices"].push_back(k);
        auto uv = surface.frames[0][n].uv;
        f["uv"][k] = {uv[0] * 64, uv[1] * 64};
      }
      m["faces"]["f" + std::to_string(i)] = f;
    }
    p.root["elements"].push_back(m);
    appendNode(p, m["uuid"], g["uuid"]);
    p.root["textures"].push_back({{"uuid", uuid()},
                                  {"name", surface.material},
                                  {"path", surface.material},
                                  {"width", 64},
                                  {"height", 64},
                                  {"uv_width", 64},
                                  {"uv_height", 64}});
  }
  if (source.frames > 1)
    p.warnings.push_back("Vertex animation is not converted; only the first pose is imported");
  return p;
}
} // namespace ftebroom::bb
