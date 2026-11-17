// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../ftebroom_bb_math.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace ftebroom::character {
using bb::Mat4;
using bb::Vec2;
using bb::Vec3;
using bb::add;
using bb::cross;
using bb::dot;
using bb::mul;
using bb::sub;
using bb::unit;

struct Joint {
  std::string name;
  int parent;
  Vec3 bindPosition;
  Mat4 inverseBind;
};

struct GearSocket {
  const char* id;
  const char* joint;
  Vec3 offset;
};
inline constexpr unsigned gearSocketVersion=1;
inline constexpr GearSocket gearSockets[]{
  {"gear.hand.L","wrist.L",{.31,-.02,.03}},
  {"gear.hand.R","wrist.R",{-.31,-.02,.03}},
  {"gear.head","head",{0,.92,-.05}},
  {"gear.back","spine03",{0,.15,-.70}},
};

inline std::size_t socketJoint(const std::vector<Joint>& joints,const GearSocket& socket) {
  const auto parent=std::find_if(joints.begin(),joints.end(),[&](const auto& joint) {
    return joint.name==socket.joint;
  });
  if (parent==joints.end()) throw std::runtime_error("Missing gear socket joint: "+std::string(socket.joint));
  return std::size_t(parent-joints.begin());
}

struct Vertex {
  std::uint32_t source;
  Vec3 position;
  Vec2 uv;
  std::array<std::uint8_t, 4> joints;
  std::array<double, 4> weights;
  Vec3 color{1,1,1}; // Local albedo multiplier carried into textured previews and GLB.
  double lipMask = 0;
  double scalpMask = 0;
};

struct Triangle {
  std::array<std::uint32_t, 3> indices;
  std::uint32_t material;
};

struct FaceShapes {
  using Deltas = std::array<Vec3,6>;
  std::vector<Deltas> bySource;
  using Key = std::array<int,3>;
  std::map<Key,unsigned> anchors;
  static Key anchorKey(const Vec3& p) {
    return {int(std::lround(p[0]*1000)),int(std::lround(p[1]*1000)),int(std::lround(p[2]*1000))};
  }
};

struct BodyShapes {
  std::vector<Vec3> vertices, joints;
  std::map<FaceShapes::Key, unsigned> anchors;

  Vec3 at(const Vec3& point) const {
    const auto match = anchors.find(FaceShapes::anchorKey(point));
    return match==anchors.end() ? Vec3{} : vertices[match->second];
  }
};

struct TorsoProfile {
  struct Section { double width = 0, back = 0, front = 0; };
  static constexpr double low = 6.64, step = .44, hipHeight = 8.4;
  std::array<Section,17> sections{};
  double upperArmPadding = .34, forearmPadding = .30;

  double armClearance(const Vec3& point,double radius) const {
    const double level = (point[1]-low)/step;
    if (level<0 || level>=sections.size()-1) return 0;
    const auto i = std::size_t(level);
    const auto t = level-i;
    const auto mix = [t](double a,double b) { return a+(b-a)*t; };
    const auto& a = sections[i];
    const auto& b = sections[i+1];
    const double width = mix(a.width,b.width);
    const double back = mix(a.back,b.back), front = mix(a.front,b.front);
    if (width<=0 || front<=back) return 0;
    const double depth = .5*(front-back)+radius;
    const double z = std::abs(point[2]-.5*(front+back));
    if (z>=depth) return 0;
    return (width+radius)*std::sqrt(1-z*z/(depth*depth));
  }
};

struct Humanoid {
  std::string rigHash;
  std::vector<Joint> joints;
  std::vector<Vertex> vertices;
  std::vector<Triangle> triangles;
  std::shared_ptr<const FaceShapes> faceShapes;
  std::shared_ptr<const BodyShapes> bodyShapes;
  TorsoProfile torso;
};

inline Vec3 adultProportions(Vec3 point,double legRise = 0.) {
  const double y = point[1];
  const auto falloff = [](double t) {
    t = std::clamp(t,0.,1.);
    return t*t*(3-2*t);
  };
  // Lengthen the lower body relative to the torso. Blend through the
  // waist and leave the head and the existing facial controls at their scale.
  const auto legs = 1-falloff((y-8.5)/4.6);
  point[1] += .48*legs*std::clamp(y/8.5,0.,1.) + legRise*falloff(y/8.5);
  // Bring the canonical wide-legged bind silhouette closer to a relaxed stance.
  const auto stance = 1-falloff((y-1.)/8.);
  const auto upperBody = falloff((y-9.5)/2.)*(1-falloff((y-13.7)/.9));
  point[0] *= 1-.12*stance-.045*upperBody;
  return point;
}

class AssetReader {
  std::string_view rest;
public:
  explicit AssetReader(std::string_view data) : rest(data) {}
  std::string_view read(std::size_t size) {
    if (size > rest.size()) throw std::runtime_error("Truncated humanoid asset");
    auto value = rest.substr(0, size);
    rest.remove_prefix(size);
    return value;
  }
  std::size_t remaining() const { return rest.size(); }
  std::uint32_t word() {
    const auto value = read(4);
    return std::uint32_t(std::uint8_t(value[0])) | (std::uint32_t(std::uint8_t(value[1])) << 8) |
           (std::uint32_t(std::uint8_t(value[2])) << 16) | (std::uint32_t(std::uint8_t(value[3])) << 24);
  }
  double scalar(double bound) {
    const auto bits = word();
    float value;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value) || std::abs(value) > bound)
      throw std::runtime_error("Invalid humanoid asset coordinate or weight");
    return value;
  }
};

inline Humanoid readHumanoid(std::string_view bytes) {
  AssetReader reader(bytes);
  if (reader.read(4) != "RBH1" || reader.word() != 1)
    throw std::runtime_error("Unsupported humanoid asset version");
  const auto joints = reader.word(), vertices = reader.word(), triangles = reader.word();
  constexpr std::size_t jointSize = 48, vertexSize = 44, triangleSize = 16;
  if (joints < 65 || joints > 95 || vertices < 3 || vertices > 65536 ||
      !triangles || triangles > 131072 ||
      reader.remaining() != 32 + joints * jointSize + vertices * vertexSize + triangles * triangleSize)
    throw std::runtime_error("Invalid humanoid asset dimensions");
  Humanoid result;
  constexpr char hex[] = "0123456789abcdef";
  for (std::uint8_t byte : reader.read(32)) {
    result.rigHash += hex[byte >> 4];
    result.rigHash += hex[byte & 15];
  }
  std::set<std::string> names;
  result.joints.reserve(joints);
  for (std::uint32_t i = 0; i < joints; ++i) {
    auto field = reader.read(32);
    std::string name(field.substr(0, field.find('\0')));
    const auto parent = reader.word();
    if (name.empty() || name.size() == 32 ||
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") != name.npos ||
        !names.insert(name).second || (i ? parent >= i : parent != UINT32_MAX))
      throw std::runtime_error("Invalid humanoid joint order or identifier");
    Vec3 position{reader.scalar(100), reader.scalar(100), reader.scalar(100)};
    result.joints.push_back({std::move(name), i ? int(parent) : -1,
                             position, bb::translate(mul(position, -1))});
  }
  result.vertices.reserve(vertices);
  for (std::uint32_t i = 0; i < vertices; ++i) {
    Vertex vertex{};
    vertex.source = reader.word();
    if (vertex.source >= 65536) throw std::runtime_error("Invalid source topology index");
    vertex.position = {reader.scalar(100), reader.scalar(100), reader.scalar(100)};
    vertex.uv = {reader.scalar(10), reader.scalar(10)};
    const auto indices = reader.read(4);
    double sum = 0;
    for (unsigned k = 0; k < vertex.weights.size(); ++k) {
      vertex.joints[k] = std::uint8_t(indices[k]);
      vertex.weights[k] = reader.scalar(1);
      if (vertex.joints[k] >= joints || vertex.weights[k] < 0)
        throw std::runtime_error("Invalid humanoid skin influence");
      sum += vertex.weights[k];
    }
    if (std::abs(sum - 1) > .0001) throw std::runtime_error("Unnormalized humanoid weights");
    result.vertices.push_back(vertex);
  }
  result.triangles.reserve(triangles);
  for (std::uint32_t i = 0; i < triangles; ++i) {
    Triangle triangle{{reader.word(), reader.word(), reader.word()}, reader.word()};
    const auto [a, b, c] = triangle.indices;
    if (a >= vertices || b >= vertices || c >= vertices || a == b || b == c || c == a ||
        triangle.material > 2)
      throw std::runtime_error("Invalid humanoid mesh index");
    result.triangles.push_back(triangle);
  }
  return result;
}

inline void readFaceShapes(Humanoid& rig,std::string_view bytes) {
  AssetReader reader(bytes);
  if (reader.read(4)!="RBF1" || reader.word()!=1)
    throw std::runtime_error("Unsupported character face shape asset");
  const auto count = reader.word();
  constexpr char hex[] = "0123456789abcdef";
  std::string hash;
  for (const std::uint8_t byte : reader.read(32)) {
    hash += hex[byte>>4];
    hash += hex[byte&15];
  }
  if (!count || count>rig.vertices.size() || reader.remaining()!=std::size_t(count)*76 ||
      hash!=rig.rigHash)
    throw std::runtime_error("Invalid character face shape asset");
  auto result = std::make_shared<FaceShapes>();
  const auto maxSource = std::max_element(rig.vertices.begin(),rig.vertices.end(),
      [](const Vertex& a,const Vertex& b) { return a.source<b.source; })->source;
  result->bySource.resize(maxSource+1);
  std::uint32_t previous = 0;
  for (unsigned i=0; i<count; ++i) {
    const auto id = reader.word();
    if (id>=result->bySource.size() || (i && id<=previous))
      throw std::runtime_error("Invalid face shape vertex index");
    previous = id;
    for (auto& shape : result->bySource[id])
      for (auto& axis : shape) axis = reader.scalar(.75);
  }
  result->bySource.resize(previous+1);
  for (const auto& vertex : rig.vertices)
    if (vertex.position[1]>13.75 && vertex.source<result->bySource.size())
      result->anchors.emplace(FaceShapes::anchorKey(vertex.position),vertex.source);
  rig.faceShapes = std::move(result);
}

inline void readBodyShapes(Humanoid& rig, std::string_view bytes) {
  AssetReader reader(bytes);
  if (reader.read(4)!="RBS1" || reader.word()!=1)
    throw std::runtime_error("Unsupported character body shape asset");
  const auto vertices = reader.word(), joints = reader.word();
  constexpr char hex[] = "0123456789abcdef";
  std::string hash;
  for (const std::uint8_t byte : reader.read(32)) {
    hash += hex[byte>>4];
    hash += hex[byte&15];
  }
  if (vertices==0 || vertices>65536 || joints!=rig.joints.size() || hash!=rig.rigHash ||
      reader.remaining()!=std::size_t(vertices)*24+std::size_t(joints)*12 ||
      std::any_of(rig.vertices.begin(),rig.vertices.end(),
                  [vertices](const Vertex& vertex) { return vertex.source>=vertices; }))
    throw std::runtime_error("Invalid character body shape asset");
  auto shapes = std::make_shared<BodyShapes>();
  shapes->vertices.reserve(vertices);
  for (unsigned i=0; i<vertices; ++i) {
    const Vec3 point{reader.scalar(100),reader.scalar(100),reader.scalar(100)};
    const Vec3 offset{reader.scalar(2),reader.scalar(2),reader.scalar(2)};
    shapes->anchors.emplace(FaceShapes::anchorKey(point),i);
    shapes->vertices.push_back(offset);
  }
  shapes->joints.reserve(joints);
  for (unsigned i=0; i<joints; ++i)
    shapes->joints.push_back({reader.scalar(2),reader.scalar(2),reader.scalar(2)});
  rig.bodyShapes = std::move(shapes);
}

inline std::vector<Mat4> skinMatrices(const Humanoid& rig, const std::vector<Mat4>& rotations,
                                      Vec3 rootOffset = {}) {
  if (rotations.size() != rig.joints.size())
    throw std::runtime_error("The pose belongs to a different rig");
  std::vector<Mat4> globals, skin;
  globals.reserve(rig.joints.size());
  skin.reserve(rig.joints.size());
  for (std::size_t i = 0; i < rig.joints.size(); ++i) {
    const auto& joint = rig.joints[i];
    const auto parent = joint.parent;
    const auto offset = parent < 0 ? add(joint.bindPosition, rootOffset)
                                    : sub(joint.bindPosition, rig.joints[parent].bindPosition);
    auto transform = bb::translate(offset) * rotations[i];
    if (parent >= 0) transform = globals[parent] * transform;
    globals.push_back(transform);
    skin.push_back(transform * joint.inverseBind);
  }
  return skin;
}

inline Vec3 deform(const Vertex& vertex, Vec3 position, const std::vector<Mat4>& skin) {
  Vec3 result{};
  for (std::size_t i = 0; i < vertex.weights.size(); ++i)
    if (vertex.weights[i] > 0)
      result = add(result, mul(skin[vertex.joints[i]].point(position), vertex.weights[i]));
  return result;
}
} // namespace ftebroom::character
