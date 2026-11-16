// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "glb.h"
#include "../ftebroom_model_md2_export.h"

namespace ftebroom::character {
struct LegacySurface {
  unsigned material;
  std::vector<Vertex> vertices;
  std::vector<std::array<unsigned,3>> triangles;
  std::vector<std::array<std::array<double,2>,3>> cornerUv;
};
using LegacySurfaces = std::vector<LegacySurface>;

inline LegacySurfaces clusterCharacter(const Humanoid& rig, double cell,bool studio=false,bool preview=false) {
  using Key = std::array<int,6>;
  std::array<std::map<Key,unsigned>,materialCount> clusters;
  std::array<std::vector<unsigned>,materialCount> remaps;
  LegacySurfaces results;
  for (unsigned m = 0; m < materialCount; ++m) {
    results.push_back({m,{},{},{}});
    remaps[m].assign(rig.vertices.size(),UINT32_MAX);
  }
  for (const auto& face : rig.triangles) {
    // Opaque legacy skins cannot use the eye-area hair cards.
    if (!studio && !preview && (face.material == browMaterial || face.material == lashMaterial)) continue;
    auto& surface = results[face.material];
    std::array<unsigned,3> triangle{};
    for (int corner = 0; corner < 3; ++corner) {
      const auto id = face.indices[corner];
      auto& remap = remaps[face.material][id];
      if (remap == UINT32_MAX) {
        const auto& vertex = rig.vertices[id];
        const auto head = (studio || preview) && vertex.position[1]>14.55;
        const auto neck = (studio || preview) && face.material==0 && !head && vertex.position[1]>13.55;
        const auto grid = cell*(head ? (face.material==0 ? .19 : .30) : neck ? .40 : 1.);
        Key key{};
        for (unsigned axis = 0; axis < 3; ++axis) key[axis] = int(std::floor(vertex.position[axis]/grid));
        key[5] = head ? 1 : neck ? 2 : 0;
        if (studio && cutoutMaterial(face.material))
          for (unsigned axis = 0; axis < 2; ++axis)
            key[3+axis] = int(std::floor(vertex.uv[axis]/(head ? .025 : .075)));
        if (preview) {
          const auto dominant = std::max_element(vertex.weights.begin(),vertex.weights.end())-vertex.weights.begin();
          key[5] += 3*vertex.joints[dominant];
          for (unsigned axis=0; axis<2; ++axis)
            key[3+axis] = int(std::floor(vertex.uv[axis]/(head || cutoutMaterial(face.material) ? .035 : .11)));
        }
        const auto [it,inserted] = clusters[face.material].emplace(key,unsigned(surface.vertices.size()));
        if (inserted) surface.vertices.push_back(vertex);
        remap = it->second;
      }
      triangle[corner] = remap;
    }
    if (triangle[0] != triangle[1] && triangle[0] != triangle[2] && triangle[1] != triangle[2]) {
      surface.triangles.push_back(triangle);
      if (studio)
        surface.cornerUv.push_back({rig.vertices[face.indices[0]].uv,rig.vertices[face.indices[1]].uv,
                                    rig.vertices[face.indices[2]].uv});
    }
  }
  results.erase(std::remove_if(results.begin(),results.end(),[](const auto& s) {
    return s.triangles.empty();
  }),results.end());
  for (auto& surface : results) {
    std::set<std::array<unsigned,3>> seen;
    std::size_t unique = 0;
    for (std::size_t i=0; i<surface.triangles.size(); ++i) {
      auto key = surface.triangles[i];
      std::sort(key.begin(),key.end());
      if (!seen.insert(key).second) continue;
      surface.triangles[unique] = surface.triangles[i];
      if (studio) surface.cornerUv[unique] = surface.cornerUv[i];
      ++unique;
    }
    surface.triangles.resize(unique);
    if (studio) surface.cornerUv.resize(unique);
    std::vector<bool> used(surface.vertices.size());
    for (const auto& face : surface.triangles) for (const auto index : face) used[index] = true;
    std::vector<unsigned> remap(used.size());
    unsigned n = 0;
    for (unsigned i = 0; i < used.size(); ++i) if (used[i]) {
      remap[i] = n;
      surface.vertices[n++] = surface.vertices[i];
    }
    surface.vertices.resize(n);
    for (auto& face : surface.triangles) for (auto& index : face) index = remap[index];
  }
  return results;
}

inline LegacySurfaces legacyTopology(const Humanoid& rig, unsigned vertexLimit, unsigned triangleLimit,
                                     bool studio=false) {
  for (double cell = .06; cell <= 1.25; cell *= 1.12) {
    auto result = clusterCharacter(rig,cell,studio);
    std::size_t vertices = 0, faces = 0;
    for (const auto& surface : result) {
      vertices += surface.vertices.size();
      faces += surface.triangles.size();
    }
    if (vertices <= vertexLimit && faces <= triangleLimit && faces >= 300) return result;
  }
  throw std::runtime_error("The character cannot be reduced to the selected game limits.");
}

inline LegacySurfaces splitLegacySurfaces(const LegacySurfaces& input,unsigned vertexLimit,unsigned triangleLimit) {
  LegacySurfaces result;
  for (const auto& source : input) {
    std::size_t first = 0;
    while (first < source.triangles.size()) {
      LegacySurface surface{source.material,{},{},{}};
      std::vector<int> local(source.vertices.size(),-1);
      for (; first < source.triangles.size() && surface.triangles.size() < triangleLimit; ++first) {
        auto face = source.triangles[first];
        unsigned needed = 0;
        for (const auto id : face) needed += local[id] < 0;
        if (surface.vertices.size() + needed > vertexLimit) break;
        for (auto& id : face) {
          if (local[id] < 0) {
            local[id] = int(surface.vertices.size());
            surface.vertices.push_back(source.vertices[id]);
          }
          id = unsigned(local[id]);
        }
        surface.triangles.push_back(face);
        if (!source.cornerUv.empty()) surface.cornerUv.push_back(source.cornerUv[first]);
      }
      result.push_back(std::move(surface));
    }
  }
  return result;
}

struct LegacyCharacter {
  modelExport::Mesh mesh;
  std::vector<modelExport::Skin> skins;
  Json frameGroups = Json::array();
};

inline Json engineFrameGroups(const Json& groups) {
  Json result = Json::array();
  for (const auto& group : groups)
    result.push_back({{"name",group.at("name")},{"firstpose",group.at("first")},
                      {"numposes",group.at("count")},{"fps",group.at("fps")},
                      {"loop",group.at("loop").get<bool>() ? 1 : 0}});
  return {{"framegroups",std::move(result)}};
}

inline LegacyCharacter bakeLegacy(const Humanoid& rig, const Document& doc, int profile,
                                   const std::array<std::string,materialCount>& materials,
                                   const std::array<modelExport::Skin,materialCount>* maps = nullptr,
                                   const ImportedMotion* imported = nullptr) {
  const auto topology = profile == 0 ? splitLegacySurfaces(legacyTopology(rig,6000,10000),999,1999) :
                        legacyTopology(rig,profile == 2 ? 1024 : 2048,profile == 2 ? 2048 : 4096);
  if (profile==0 && topology.size()>32) throw std::runtime_error("Character exceeds the MD3 surface limit.");
  LegacyCharacter out;
  for (const auto& part : topology) {
    modelExport::Surface surface;
    surface.material = materials[part.material];
    for (auto triangle : part.triangles) {
      std::swap(triangle[1],triangle[2]);
      surface.triangles.push_back(triangle);
    }
    out.mesh.surfaces.push_back(std::move(surface));
    if (maps) {
      out.skins.push_back(maps->at(part.material));
      continue;
    }
    modelExport::Skin skin{4,4,{}};
    for (unsigned pixel = 0; pixel < 16; ++pixel) {
      for (const auto color : materialColor(part.material,doc))
        skin.rgba.push_back(std::uint8_t(std::lround(color*255)));
      skin.rgba.push_back(255);
    }
    out.skins.push_back(std::move(skin));
  }
  const Animator animator(rig);
  const auto scale = bodyScale(doc)*4;
  for (const auto& clip : characterClips(imported)) {
    const auto steps = std::max(2u,unsigned(std::ceil(8*clip.seconds)));
    const auto count = steps + !clip.loop;
    const auto start = out.mesh.frames;
    for (unsigned i = 0; i < count; ++i) {
      const auto pose = sampleCharacterClip(animator,clip,i*clip.seconds/steps,imported);
      const auto skin = skinMatrices(rig,pose.rotations,pose.root);
      for (std::size_t p = 0; p < topology.size(); ++p) {
        const auto& part = topology[p];
        std::vector<Vec3> points(part.vertices.size()), normals(part.vertices.size());
        for (std::size_t v = 0; v < points.size(); ++v)
          points[v] = bb::bbToQuake(deform(part.vertices[v],part.vertices[v].position,skin),0,scale);
        for (const auto& face : part.triangles) {
          const auto n = cross(sub(points[face[1]],points[face[0]]),sub(points[face[2]],points[face[0]]));
          for (const auto v : face) normals[v] = add(normals[v],n);
        }
        auto& frame = out.mesh.surfaces[p].frames.emplace_back();
        frame.reserve(points.size());
        for (std::size_t v = 0; v < points.size(); ++v) {
          const auto& t = part.vertices[v].uv;
          const auto n = unit(normals[v]), point = points[v];
          frame.push_back({{float(point[0]),float(point[1]),float(point[2])},
                           {float(n[0]),float(n[1]),float(n[2])},{float(t[0]),float(1-t[1])}});
        }
      }
      out.mesh.frameNames.push_back("a"+std::to_string(out.frameGroups.size())+"_"+std::to_string(i));
      ++out.mesh.frames;
    }
    out.frameGroups.push_back({{"name",clip.id},{"first",start},{"count",count},
                               {"fps",steps/clip.seconds},{"loop",clip.loop}});
  }
  return out;
}
} // namespace ftebroom::character
