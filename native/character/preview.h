// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "legacy.h"

namespace ftebroom::character {
inline Humanoid previewLod(const Humanoid& rig,std::size_t target) {
  if (rig.triangles.size()<=target) return rig;
  for (double grid=.20; grid<2.5; grid*=1.27) {
    auto clusters = clusterCharacter(rig,grid,false,true);
    std::size_t faces=0;
    for (const auto& material : clusters) faces+=material.triangles.size();
    if (faces>target) continue;
    Humanoid result;
    result.joints=rig.joints;
    result.rigHash=rig.rigHash;
    for (const auto& material : clusters) {
      const auto offset=std::uint32_t(result.vertices.size());
      result.vertices.insert(result.vertices.end(),material.vertices.begin(),material.vertices.end());
      for (auto triangle : material.triangles) {
        for (auto& index : triangle) index+=offset;
        result.triangles.push_back({triangle,material.material});
      }
    }
    return result;
  }
  throw std::runtime_error("Preview LOD cannot preserve the current materials at this triangle limit.");
}

enum class PreviewShading { Materials, Normals, SkinWeights };

inline std::string previewMesh(const Humanoid& rig, const Document& doc,
                               const std::vector<Vec3>& rest, const AnimationPose& pose,
                               const std::array<std::string,materialCount>& textures,
                               const std::array<Vec3,materialCount>& tints, Vec3 view, bool pbr=false,
                               PreviewShading shading=PreviewShading::Materials, std::size_t joint=0) {
  const auto skin = skinMatrices(rig, pose.rotations, pose.root);
  std::vector<Vec3> points;
  points.reserve(rest.size());
  for (std::size_t i = 0; i < rest.size(); ++i)
    points.push_back(deform(rig.vertices[i], rest[i], skin));
  const auto normals = surfaceNormals(rig, points);
  constexpr std::size_t batchFaces = 100000;
  std::string buffer = pbr ? "RBB2" : "RBB1";
  buffer.reserve(8 + rig.vertices.size()*(pbr ? 48 : 36) + rig.triangles.size()*12 + materialCount*136);
  std::array<std::vector<std::array<std::uint32_t,3>>,materialCount> surfaces;
  for (const auto& face : rig.triangles) surfaces[face.material].push_back(face.indices);
  unsigned batches = 0;
  for (const auto& surface : surfaces) batches += (surface.size()+batchFaces-1)/batchFaces;
  glbWord(buffer,batches);
  const auto key = unit({-.45,.51,.73});
  const auto fill = unit({.68,.15,.70});
  const auto rim = unit({.35,.62,-.68});
  const auto half = unit(add(key,view));
  for (unsigned material = 0; material < materialCount; ++material) {
    const auto& surface = surfaces[material];
    if (surface.empty()) continue;
    const bool useMaterials = shading == PreviewShading::Materials;
    const std::string_view name = !useMaterials || textures[material].empty()
        ? std::string_view("$whiteimage") : textures[material];
    const double shine[]{.105,.13,.06,.08,.12,.045,.07,.055,.05,.04};
    const double gloss[]{24,40,28,13,29,14,12,13,14,18};
    for (std::size_t first = 0; first < surface.size(); first += batchFaces) {
      const auto end = std::min(surface.size(),first+batchFaces);
      const auto count = unsigned((end-first)*3);
      std::vector<unsigned> ids, indices;
      std::vector<int> remap(rig.vertices.size(),-1);
      indices.reserve(count);
      for (std::size_t face = first; face < end; ++face)
        for (const auto index : surface[face]) {
          if (remap[index] < 0) {
            remap[index] = int(ids.size());
            ids.push_back(index);
          }
          indices.push_back(unsigned(remap[index]));
        }
      if (ids.size() > 65536) throw std::runtime_error("Character exceeds the native preview index limit.");
      glbWord(buffer,unsigned(ids.size()));
      glbWord(buffer,count);
      buffer += name;
      buffer.append(128-name.size(),0);
      for (const auto index : ids) {
        const auto point = bb::bbToQuake(points[index],0,4*bodyScale(doc));
        const auto& uv = rig.vertices[index].uv;
        const auto& normal = normals[index];
        const double warm = std::clamp((dot(normal,key)+.27)/1.27,0.,1.);
        const double cool = std::max(0.,dot(normal,fill));
        const double back = std::max(0.,dot(normal,rim));
        const auto& vertexColor = rig.vertices[index].color;
        const bool physical = pbr && surfaceVariant(material);
        const bool glSkin = material==0 && (physical || name.rfind("bbtex/rbchar-skin-",0)==0);
        const auto lip = rig.vertices[index].lipMask;
        const double sheen = glSkin ? 0 : (shine[material]+.022*lip) *
            std::pow(std::max(0.,dot(normal,half)),gloss[material]+5*lip);
        Vec3 color{.55+.48*warm+.125*cool+.13*back+sheen,
                   .54+.44*warm+.155*cool+.15*back+sheen,
                   .54+.40*warm+.20*cool+.17*back+sheen};
        if (physical) {
          color = {1,1,1};
        } else if (material==1) {
          const double lidShade = 1-.12*smoothBand(.59,.81,uv[1]);
          color = mul(color,lidShade);
        }
        for (unsigned channel = 0; channel < 3; ++channel)
          color[channel] = std::clamp(color[channel]*tints[material][channel]*vertexColor[channel],0.,1.);
        if (shading == PreviewShading::Normals) {
          color = mul(add(bb::bbToQuake(normal),{1,1,1}),.5);
        } else if (shading == PreviewShading::SkinWeights) {
          const auto& skinVertex = rig.vertices[index];
          double weight = 0;
          for (std::size_t i=0; i<skinVertex.joints.size(); ++i)
            if (skinVertex.joints[i] == joint) weight += skinVertex.weights[i];
          const Vec3 unweighted{.12,.16,.24}, weighted{1,.67,.13};
          color = add(unweighted,mul(sub(weighted,unweighted),std::clamp(weight,0.,1.)));
        }
        const auto alpha = !useMaterials ? 1. : glSkin ? skinVertexMask(rig.vertices[index]) :
            pbr && (material==3 || material==4 || material==6) ? fabricVertexMask(material,doc) :
            surfaceOpacity(material);
        const float vertex[]{float(point[0]),float(point[1]),float(point[2]),float(uv[0]),
                            float(1-uv[1]),float(color[0]),float(color[1]),float(color[2]),
                            float(alpha)};
        buffer.append(reinterpret_cast<const char*>(vertex),sizeof(vertex));
        if (pbr) {
          const auto orientation = bb::bbToQuake(normal);
          const float xyz[]{float(orientation[0]),float(orientation[1]),float(orientation[2])};
          buffer.append(reinterpret_cast<const char*>(xyz),sizeof(xyz));
        }
      }
      for (const auto index : indices) glbWord(buffer,index);
    }
  }
  return buffer;
}
} // namespace ftebroom::character
