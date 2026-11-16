// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "materials.h"

namespace ftebroom::character {
using Json = nlohmann::json;

inline std::vector<Vec3> surfaceNormals(const Humanoid& rig, const std::vector<Vec3>& positions) {
  std::uint32_t last = 0;
  for (const auto& vertex : rig.vertices) last = std::max(last, vertex.source);
  std::vector<Vec3> bySource(last + 1);
  for (const auto& face : rig.triangles) {
    const auto [a, b, c] = face.indices;
    const auto normal = cross(sub(positions[b], positions[a]), sub(positions[c], positions[a]));
    for (const auto index : face.indices) {
      auto& accumulated = bySource[rig.vertices[index].source];
      accumulated = add(accumulated, normal);
    }
  }
  std::vector<Vec3> normals;
  normals.reserve(positions.size());
  for (const auto& vertex : rig.vertices) normals.push_back(unit(bySource[vertex.source]));
  return normals;
}

inline void glbWord(std::string& bytes, std::uint32_t word) {
  for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<char>((word >> (8 * i)) & 255));
}

class GlbBuffer {
  std::string data;
  Json views = Json::array(), accessors = Json::array();
public:
  template <typename T>
  int add(const std::vector<T>& values, std::size_t width, const char* type, unsigned target = 0) {
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, std::uint32_t> ||
                  std::is_same_v<T, std::uint8_t>);
    if (values.empty() || values.size() % width)
      throw std::runtime_error("Invalid character GLB accessor dimensions");
    while (data.size() % 4) data.push_back(0);
    const auto offset = data.size();
    constexpr int component = std::is_same_v<T, float> ? 5126 :
                              std::is_same_v<T, std::uint32_t> ? 5125 : 5121;
    for (const auto value : values) {
      if constexpr (std::is_same_v<T, std::uint8_t>) {
        data.push_back(static_cast<char>(value));
      } else {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        glbWord(data, bits);
      }
    }
    Json view{{"buffer",0},{"byteOffset",offset},{"byteLength",data.size() - offset}};
    if (target) view["target"] = target;
    accessors.push_back({{"bufferView",views.size()},{"componentType",component},
                         {"count",values.size() / width},{"type",type}});
    views.push_back(std::move(view));
    return int(accessors.size() - 1);
  }
  std::size_t image(std::string_view bytes) {
    while (data.size() % 4) data.push_back(0);
    views.push_back({{"buffer",0},{"byteOffset",data.size()},{"byteLength",bytes.size()}});
    data.append(bytes);
    return views.size()-1;
  }
  void bounds(int accessor, Json minimum, Json maximum) {
    accessors.at(accessor)["min"] = std::move(minimum);
    accessors.at(accessor)["max"] = std::move(maximum);
  }
  std::string finish(Json document) {
    document["accessors"] = std::move(accessors);
    document["bufferViews"] = std::move(views);
    document["buffers"] = Json::array({{{"byteLength",data.size()}}});
    auto json = document.dump();
    while (json.size() % 4) json.push_back(' ');
    while (data.size() % 4) data.push_back(0);
    std::string out = "glTF";
    glbWord(out, 2);
    glbWord(out, std::uint32_t(json.size() + data.size() + 28));
    glbWord(out, std::uint32_t(json.size()));
    out += "JSON";
    out += json;
    glbWord(out, std::uint32_t(data.size()));
    out.append("BIN\0", 4);
    out += data;
    return out;
  }
};

template <std::size_t N>
inline void appendFloats(std::vector<float>& out, const std::array<double, N>& values) {
  out.insert(out.end(), values.begin(), values.end());
}

// Empty entries use the documented solid-color fallback.
using MaterialImages = std::array<std::string,materialCount>;

struct GlbExpression {
  std::vector<Vec3> positions, normals;
};
using GlbExpressions = std::array<GlbExpression,std::size(expressions)-1>;

inline GlbExpressions glbExpressionTargets(const Humanoid& rig,const Humanoid& neutral,
                                          Document doc,const WardrobeReader& assets,bool fittedBody) {
  std::vector<Vec3> base;
  for (const auto& vertex : neutral.vertices) base.push_back(vertex.position);
  const auto normals=surfaceNormals(neutral,base);
  GlbExpressions targets;
  for (unsigned expression=1;expression<std::size(expressions);++expression) {
    doc.expression=expression;
    const auto shaped=assemble(rig,doc,assets,fittedBody);
    if (shaped.vertices.size()!=base.size() || shaped.triangles.size()!=neutral.triangles.size())
      throw std::runtime_error("Expression changes the exported character topology.");
    for (unsigned i=0;i<shaped.triangles.size();++i) {
      if (shaped.triangles[i].indices!=neutral.triangles[i].indices ||
          shaped.triangles[i].material!=neutral.triangles[i].material)
        throw std::runtime_error("Expression changes the exported character surface order.");
    }
    std::vector<Vec3> points;
    for (const auto& vertex : shaped.vertices) points.push_back(vertex.position);
    const auto deformedNormals=surfaceNormals(shaped,points);
    auto& target=targets[expression-1];
    for (unsigned i=0;i<base.size();++i) {
      target.positions.push_back(sub(points[i],base[i]));
      target.normals.push_back(sub(deformedNormals[i],normals[i]));
    }
  }
  return targets;
}

inline std::string exportGlb(const Humanoid& rig, const Document& doc, bool fitted = false,
                             const MaterialImages& images = {}, bool hd = false,
                             const ImportedMotion* imported = nullptr,
                             const GlbExpressions* expressionMorphs = nullptr) {
  GlbBuffer buffer;
  const auto scale = bodyScale(doc) * .1; // The source uses decimetres, glTF uses metres.
  std::vector<Vec3> points;
  if (fitted) {
    points.reserve(rig.vertices.size());
    for (const auto& vertex : rig.vertices) points.push_back(vertex.position);
  } else points = morphBody(rig, doc);
  const auto normals = surfaceNormals(rig, points);
  std::vector<float> positions, normalValues, uv, weights, inverseBinds, colors;
  std::vector<std::uint8_t> joints;
  Vec3 low = mul(points.front(), scale), high = low;
  for (std::size_t i = 0; i < points.size(); ++i) {
    const auto p = mul(points[i], scale);
    for (int axis = 0; axis < 3; ++axis) {
      low[axis] = std::min(low[axis], p[axis]);
      high[axis] = std::max(high[axis], p[axis]);
    }
    appendFloats(positions, p);
    appendFloats(normalValues, normals[i]);
    const auto& vertex = rig.vertices[i];
    appendFloats(uv, Vec2{vertex.uv[0], 1 - vertex.uv[1]});
    appendFloats(weights, vertex.weights);
    Vec3 albedo{};
    for (unsigned channel=0; channel<albedo.size(); ++channel)
      albedo[channel] = displayToLinear(std::clamp(vertex.color[channel],0.,1.));
    appendFloats(colors,albedo);
    joints.insert(joints.end(), vertex.joints.begin(), vertex.joints.end());
  }
  const auto p = buffer.add(positions, 3, "VEC3", 34962);
  for (int axis = 0; axis < 3; ++axis) {
    low[axis] = float(low[axis]);
    high[axis] = float(high[axis]);
  }
  buffer.bounds(p, low, high);
  Json attributes{{"POSITION",p},{"NORMAL",buffer.add(normalValues,3,"VEC3",34962)},
                  {"JOINTS_0",buffer.add(joints,4,"VEC4",34962)},
                  {"WEIGHTS_0",buffer.add(weights,4,"VEC4",34962)}};
  if (fitted) attributes["COLOR_0"] = buffer.add(colors,3,"VEC3",34962);
  if (std::any_of(images.begin(),images.end(),[](const auto& image){ return !image.empty(); }))
    attributes["TEXCOORD_0"] = buffer.add(uv,2,"VEC2",34962);
  std::array<std::vector<std::uint32_t>, materialCount> indices;
  for (const auto& face : rig.triangles) {
    auto& group = indices[face.material];
    group.insert(group.end(), face.indices.begin(), face.indices.end());
  }
  Json primitives = Json::array(), materials = Json::array();
  Json packedImages = Json::array(), textures = Json::array();
  const auto maps = surfaceMaps(doc,hd);
  Json targets=Json::array();
  if (expressionMorphs) for (const auto& morph : *expressionMorphs) {
    if (morph.positions.size()!=points.size() || morph.normals.size()!=points.size())
      throw std::runtime_error("Expression data does not match the exported base mesh.");
    std::vector<float> delta, normalDelta;
    std::array<float,3> lower{}, upper{};
    for (unsigned i=0;i<points.size();++i) {
      const auto offset=mul(morph.positions[i],scale);
      appendFloats(delta,offset);
      appendFloats(normalDelta,morph.normals[i]);
      for (unsigned axis=0;axis<3;++axis) {
        const auto value=float(offset[axis]);
        lower[axis]=i ? std::min(lower[axis],value) : value;
        upper[axis]=i ? std::max(upper[axis],value) : value;
      }
    }
    const auto accessor=buffer.add(delta,3,"VEC3",34962);
    buffer.bounds(accessor,lower,upper);
    targets.push_back({{"POSITION",accessor},{"NORMAL",buffer.add(normalDelta,3,"VEC3",34962)}});
  }
  const char* names[]{"Skin","Eyes","Teeth","Clothing","Footwear","Hair","Headwear","Facial hair","Eyebrows","Eyelashes"};
  for (unsigned material = 0; material < materialCount; ++material) {
    if (indices[material].empty()) continue;
    const bool textured = !images[material].empty();
    auto usedAttributes = attributes;
    if (!textured) usedAttributes.erase("TEXCOORD_0");
    primitives.push_back({{"attributes",std::move(usedAttributes)},
                          {"indices",buffer.add(indices[material],1,"SCALAR",34963)},
                          {"material",materials.size()}});
    if (expressionMorphs) primitives.back()["targets"]=targets;
    const auto c = textured ? surfaceTint(material,doc,maps[material]) : materialColor(material,doc);
    Json definition{{"name",names[material]},{"pbrMetallicRoughness",
          {{"baseColorFactor",{displayToLinear(std::min(c[0],1.)),displayToLinear(std::min(c[1],1.)),
                               displayToLinear(std::min(c[2],1.)),surfaceOpacity(material)}},
           {"metallicFactor",0},{"roughnessFactor",material==1 ? .29 : material==5 ? .55 : .76}}}};
    if (textured) {
      const bool png = cutoutMaterial(material);
      definition["pbrMetallicRoughness"]["baseColorTexture"] = {{"index",textures.size()}};
      textures.push_back({{"source",packedImages.size()}});
      packedImages.push_back({{"name",maps[material]},{"bufferView",buffer.image(images[material])},
                              {"mimeType",png ? "image/png" : "image/jpeg"}});
      if (png) {
        const bool fine = material==browMaterial || material==lashMaterial;
        definition["alphaMode"] = fine ? "BLEND" : "MASK";
        if (!fine) definition["alphaCutoff"] = .5;
        definition["doubleSided"] = true;
      }
    }
    materials.push_back(std::move(definition));
  }
  Json nodes = Json::array(), boneNodes = Json::array();
  for (std::size_t i = 0; i < rig.joints.size(); ++i) {
    const auto& joint = rig.joints[i];
    const auto relative = joint.parent < 0 ? joint.bindPosition
                                          : sub(joint.bindPosition, rig.joints[joint.parent].bindPosition);
    nodes.push_back({{"name",joint.name},{"translation",mul(relative,scale)}});
    if (joint.parent >= 0) nodes.at(joint.parent)["children"].push_back(i);
    boneNodes.push_back(i);
    const auto inverse = bb::translate(mul(joint.bindPosition, -scale));
    for (int column = 0; column < 4; ++column)
      for (int row = 0; row < 4; ++row) inverseBinds.push_back(float(inverse.m[row][column]));
  }
  const auto meshNode = nodes.size();
  nodes.push_back({{"name",doc.name},{"mesh",0},{"skin",0}});
  for (const auto& socket : gearSockets) {
    const auto parent=socketJoint(rig.joints,socket);
    nodes.at(parent)["children"].push_back(nodes.size());
    nodes.push_back({{"name",socket.id},{"translation",mul(socket.offset,scale)},
                     {"extras",{{"reyondGearSocket",gearSocketVersion}}}});
  }
  const auto bindAccessor = buffer.add(inverseBinds,16,"MAT4");

  const Animator animator(rig);
  Json animations = Json::array();
  for (const auto& clip : characterClips(imported)) {
    const auto steps = unsigned(std::ceil(30 * clip.seconds));
    std::vector<float> times, root;
    std::vector<std::vector<float>> rotations(rig.joints.size());
    std::vector<bb::Vec4> previous(rig.joints.size(), {0,0,0,1});
    for (unsigned i = 0; i <= steps; ++i) {
      const double seconds = i == steps ? clip.seconds : clip.seconds * i / steps;
      times.push_back(float(seconds));
      const auto pose = sampleCharacterClip(animator,clip,seconds,imported);
      appendFloats(root, mul(add(rig.joints[0].bindPosition, pose.root), scale));
      for (std::size_t joint = 0; joint < rig.joints.size(); ++joint) {
        auto q = rotationQuaternion(pose.rotations[joint]);
        double dotProduct = 0;
        for (int axis = 0; axis < 4; ++axis) dotProduct += previous[joint][axis] * q[axis];
        if (dotProduct < 0) for (auto& axis : q) axis = -axis;
        previous[joint] = q;
        appendFloats(rotations[joint], q);
      }
    }
    const auto timeAccessor = buffer.add(times,1,"SCALAR");
    buffer.bounds(timeAccessor, {times.front()}, {times.back()});
    Json samplers = Json::array(), channels = Json::array();
    auto channel = [&](std::size_t node, const char* path, int output) {
      channels.push_back({{"sampler",samplers.size()},{"target",{{"node",node},{"path",path}}}});
      samplers.push_back({{"input",timeAccessor},{"output",output},{"interpolation","LINEAR"}});
    };
    channel(0, "translation", buffer.add(root,3,"VEC3"));
    for (std::size_t joint = 0; joint < rotations.size(); ++joint) {
      const auto& keys = rotations[joint];
      bool identity = true;
      for (std::size_t i = 0; i < keys.size(); i += 4)
        identity &= std::abs(keys[i]) + std::abs(keys[i+1]) + std::abs(keys[i+2]) < 1e-6f;
      if (!identity) channel(joint, "rotation", buffer.add(keys,4,"VEC4"));
    }
    animations.push_back({{"name",clip.id},{"samplers",std::move(samplers)},
                          {"channels",std::move(channels)},{"extras",{{"loop",clip.loop}}}});
  }
  Json gltf{{"asset",{{"version","2.0"},{"generator","ReyondBroom Character Creator"}}},
      {"scene",0},{"scenes",Json::array({{{"nodes",{0,meshNode}}}})},{"nodes",std::move(nodes)},
      {"meshes",Json::array({{{"primitives",std::move(primitives)}}})},{"materials",std::move(materials)},
      {"skins",Json::array({{{"skeleton",0},{"joints",std::move(boneNodes)},{"inverseBindMatrices",bindAccessor}}})},
      {"animations",std::move(animations)},{"extras",{{"rigSha256",rig.rigHash},{"animationBank",animationBank}}}};
  if (expressionMorphs) {
    auto& mesh=gltf["meshes"][0];
    auto& defaults=mesh["weights"]=Json::array();
    auto& names=mesh["extras"]["targetNames"]=Json::array();
    for (unsigned i=1;i<std::size(expressions);++i) {
      defaults.push_back(i==doc.expression ? 1. : 0.);
      names.push_back(expressions[i]);
    }
  }
  if (!packedImages.empty()) {
    gltf["images"] = std::move(packedImages);
    gltf["textures"] = std::move(textures);
  }
  return buffer.finish(std::move(gltf));
}
} // namespace ftebroom::character
