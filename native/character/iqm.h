// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "glb.h"
#include "../ftebroom_model_export.h"

namespace ftebroom::character {
inline std::string exportIqm(const Humanoid& rig, const Document& doc,
                             const std::array<std::string,materialCount>& materialPaths, bool fitted = false,
                             bool skinDetails = false, bool useCapture = false,
                             const ImportedMotion* imported = nullptr) {
  using modelExport::Binary;
  Binary text, meshes, vertexArrays, triangles, jointData, poseData, animations, frameData;
  Binary positions, normals, uv, jointIndices, jointWeights, colors;
  auto intern = [&](std::string_view name) {
    const auto offset = unsigned(text.bytes.size());
    text.bytes.append(name);
    text.bytes.push_back(0);
    return offset;
  };
  intern("");
  const auto scale = 4 * bodyScale(doc);
  std::vector<Vec3> points;
  if (fitted) {
    points.reserve(rig.vertices.size());
    for (const auto& vertex : rig.vertices) points.push_back(vertex.position);
  } else points = morphBody(rig, doc);
  const auto surface = surfaceNormals(rig, points);
  unsigned nextVertex = 0, nextTriangle = 0, meshCount = 0;
  for (unsigned material = 0; material < materialPaths.size(); ++material) {
    const auto firstVertex = nextVertex, firstTriangle = nextTriangle;
    std::vector<int> remap(rig.vertices.size(),-1);
    for (const auto& face : rig.triangles) {
      if (face.material != material) continue;
      ++nextTriangle;
      for (const auto id : {face.indices[0],face.indices[2],face.indices[1]}) {
        if (remap[id] < 0) {
          remap[id] = int(nextVertex++);
          const auto& vertex = rig.vertices[id];
          for (const auto value : bb::bbToQuake(points[id],0,scale)) positions.real(float(value));
          for (const auto value : bb::bbToQuake(surface[id])) normals.real(float(value));
          uv.real(float(vertex.uv[0]));
          uv.real(float(1 - vertex.uv[1]));
          unsigned remaining = 255;
          unsigned written = 0;
          const auto last = std::find_if(vertex.weights.rbegin(),vertex.weights.rend(),[](double weight) {
            return weight > 0;
          }).base()-vertex.weights.begin()-1;
          for (unsigned i = 0; i < 4; ++i) {
            const auto weight = i == last ? remaining :
                                std::min(remaining,unsigned(std::lround(vertex.weights[i] * 255)));
            if (!weight) continue;
            jointIndices.bytes.push_back(char(vertex.joints[i]));
            jointWeights.bytes.push_back(char(weight));
            remaining -= weight;
            ++written;
          }
          jointIndices.bytes.append(4-written,'\0');
          jointWeights.bytes.append(4-written,'\0');
          const auto tint = materialPaths[material]=="$whiteimage" ? materialColor(material,doc) : Vec3{1,1,1};
          for (unsigned channel=0; channel<tint.size(); ++channel)
            colors.bytes.push_back(char(std::lround(
                std::clamp(tint[channel]*vertex.color[channel],0.,1.)*255)));
            // The FTE programs read the lip and scalp mask or the fabric type.
            const auto alpha = !skinDetails ? 1. : material==0 ? skinVertexMask(vertex) :
                material==3 || material==4 || material==6 ? fabricVertexMask(material,doc) : 1.;
          colors.bytes.push_back(char(std::lround(std::clamp(alpha,0.,1.)*255)));
        }
        triangles.u32(unsigned(remap[id]));
      }
    }
    if (nextTriangle == firstTriangle) continue;
    ++meshCount;
    for (unsigned value : {intern("body" + std::to_string(material)),intern(materialPaths[material]),
                           firstVertex,nextVertex-firstVertex,firstTriangle,nextTriangle-firstTriangle})
      meshes.u32(value);
  }

  using Channels = std::array<double, 7>;
  std::vector<Vec3> rest;
  for (const auto& joint : rig.joints) {
    const auto relative = joint.parent < 0 ? joint.bindPosition
                                           : sub(joint.bindPosition,rig.joints[joint.parent].bindPosition);
    rest.push_back(bb::bbToQuake(relative,0,scale));
    jointData.u32(intern(joint.name));
    jointData.u32(unsigned(joint.parent));
    for (auto v : rest.back()) jointData.real(float(v));
    for (float v : {0.f,0.f,0.f,1.f,1.f,1.f,1.f}) jointData.real(v);
  }

  Mat4 basis = Mat4::identity();
  basis.m[0][0] = basis.m[1][1] = basis.m[2][2] = 0;
  basis.m[0][2] = basis.m[1][0] = -1;
  basis.m[2][1] = 1;
  const auto inverseBasis = bb::inverse(basis);
  const Animator animator(rig,useCapture);
  std::vector<std::vector<Channels>> poses;
  auto minima = std::vector<Channels>(rig.joints.size()), maxima = minima;
  for (auto& channels : minima) channels.fill(1e30);
  for (auto& channels : maxima) channels.fill(-1e30);
  const auto definitions=characterClips(imported);
  for (const auto& definition : definitions) {
    const auto clip = captureTiming(definition,useCapture);
    const auto steps = unsigned(std::ceil(30 * clip.seconds));
    const auto count = steps + !clip.loop;
    animations.u32(intern(clip.id));
    animations.u32(unsigned(poses.size()));
    animations.u32(count);
    animations.real(float(steps / clip.seconds));
    animations.u32(clip.loop ? 1 : 0);
    auto previous = std::vector<bb::Vec4>(rig.joints.size(),{0,0,0,1});
    for (unsigned frame = 0; frame < count; ++frame) {
      const auto pose = sampleCharacterClip(animator,clip,frame*clip.seconds/steps,imported);
      auto& frameValues = poses.emplace_back();
      frameValues.reserve(rig.joints.size());
      for (std::size_t i = 0; i < rig.joints.size(); ++i) {
        auto q = rotationQuaternion(basis * pose.rotations[i] * inverseBasis);
        double alignment = 0;
        for (int axis = 0; axis < 4; ++axis) alignment += q[axis] * previous[i][axis];
        if (alignment < 0) for (auto& value : q) value = -value;
        previous[i] = q;
        const auto p = i ? rest[i] : add(rest[0],bb::bbToQuake(pose.root,0,scale));
        const Channels channels{p[0],p[1],p[2],q[0],q[1],q[2],q[3]};
        frameValues.push_back(channels);
        for (int j = 0; j < 7; ++j) {
          minima[i][j] = std::min(minima[i][j],channels[j]);
          maxima[i][j] = std::max(maxima[i][j],channels[j]);
        }
      }
    }
  }
  unsigned activeChannels = 0;
  std::vector<Channels> steps(rig.joints.size());
  for (std::size_t i = 0; i < rig.joints.size(); ++i) {
    unsigned mask = 0;
    for (unsigned channel = 0; channel < 7; ++channel) {
      if (maxima[i][channel] - minima[i][channel] <= 1e-7) continue;
      mask |= 1u << channel;
      ++activeChannels;
      steps[i][channel] = (maxima[i][channel] - minima[i][channel]) / 65535;
    }
    poseData.u32(unsigned(rig.joints[i].parent));
    poseData.u32(mask);
    for (auto value : minima[i]) poseData.real(float(value));
    for (int axis = 0; axis < 3; ++axis) poseData.real(1);
    for (auto step : steps[i]) poseData.real(float(step));
    for (int axis = 0; axis < 3; ++axis) poseData.real(0);
  }
  for (const auto& frame : poses)
    for (std::size_t i = 0; i < frame.size(); ++i)
      for (std::size_t j = 0; j < frame[i].size(); ++j)
        if (steps[i][j] > 0)
          frameData.u16(unsigned(std::clamp(std::lround((frame[i][j] - minima[i][j]) / steps[i][j]),
                                            0l,65535l)));

  unsigned next = 124;
  auto place = [&](Binary& section) {
    while (section.bytes.size() % 4) section.bytes.push_back(0);
    const auto offset = next;
    next += unsigned(section.bytes.size());
    return offset;
  };
  const auto textOffset = place(text), meshOffset = place(meshes);
  const auto triangleOffset = place(triangles), jointOffset = place(jointData);
  const auto poseOffset = place(poseData), animOffset = place(animations), frameOffset = place(frameData);
  constexpr unsigned vertexArrayCount = 6;
  const auto arraysOffset = next;
  next += vertexArrayCount * 20;
  const auto positionOffset = place(positions), uvOffset = place(uv), normalOffset = place(normals);
  const auto indicesOffset = place(jointIndices), weightsOffset = place(jointWeights);
  const auto colorsOffset = place(colors);
  Binary comment;
  unsigned commentOffset = 0;
  if (useCapture) {
    comment.bytes =
        "Locomotion adapted from 100STYLE by Ian Mason, Sebastian Starke and Taku Komura. "
        "https://www.ianxmason.com/100style/ | CC BY 4.0 https://creativecommons.org/licenses/by/4.0/ | "
        "Trimmed, resampled and retargeted to the ReyondBroom rig.";
    comment.bytes.push_back(0);
    commentOffset = place(comment);
  }
  const std::array<unsigned,5> arrays[]{{0,0,7,3,positionOffset},{1,0,7,2,uvOffset},
      {2,0,7,3,normalOffset},{4,0,1,4,indicesOffset},{5,0,1,4,weightsOffset},{6,0,1,4,colorsOffset}};
  for (const auto& row : arrays) for (const auto n : row) vertexArrays.u32(n);

  Binary output;
  output.bytes.assign("INTERQUAKEMODEL\0",16);
  const unsigned boneCount = unsigned(rig.joints.size());
  const unsigned header[]{2,next,0,unsigned(text.bytes.size()),textOffset,meshCount,meshOffset,
      vertexArrayCount,nextVertex,arraysOffset,nextTriangle,triangleOffset,0,boneCount,jointOffset,
      boneCount,poseOffset,unsigned(definitions.size()),animOffset,unsigned(poses.size()),activeChannels,
      frameOffset,0,unsigned(comment.bytes.size()),commentOffset,0,0};
  for (const auto value : header) output.u32(value);
  for (const auto* section : {&text,&meshes,&triangles,&jointData,&poseData,&animations,&frameData,
                              &vertexArrays,&positions,&uv,&normals,&jointIndices,&jointWeights,&colors,&comment})
    output.bytes += section->bytes;
  if (output.bytes.size() != next) throw std::runtime_error("Inconsistent IQM offsets");
  return std::move(output.bytes);
}
} // namespace ftebroom::character
