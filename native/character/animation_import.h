// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "document.h"

namespace ftebroom::character {
class MotionGlb {
  nlohmann::json document;
  std::string_view binary;

  static std::uint32_t word(std::string_view bytes,std::size_t offset) {
    if (offset>bytes.size() || bytes.size()-offset<4)
      throw std::runtime_error("Truncated binary glTF animation.");
    std::uint32_t result=0;
    for (unsigned i=0;i<4;++i) result|=std::uint32_t(std::uint8_t(bytes[offset+i]))<<(8*i);
    return result;
  }

  template <std::size_t Width>
  std::vector<std::array<double,Width>> floats(std::size_t index,const char* type) const {
    const auto& attribute=document.at("accessors").at(index);
    if (attribute.at("type")!=type || attribute.at("componentType")!=5126 ||
        attribute.contains("sparse") || attribute.value("normalized",false))
      throw std::runtime_error("Animation channels must use standard floating-point glTF accessors.");
    const auto& view=document.at("bufferViews").at(attribute.at("bufferView").get<std::size_t>());
    const auto count=attribute.at("count").get<std::size_t>();
    const auto offset=view.value("byteOffset",std::size_t(0));
    const auto length=view.at("byteLength").get<std::size_t>();
    const auto relative=attribute.value("byteOffset",std::size_t(0));
    constexpr auto elementBytes=Width*4;
    if (view.at("buffer")!=0 || view.contains("byteStride") || offset>binary.size() ||
        length>binary.size()-offset || relative>length || count<2 || count>1024 ||
        count>(length-relative)/elementBytes || (offset+relative)%4)
      throw std::runtime_error("Invalid or oversized animation data view.");
    std::vector<std::array<double,Width>> result(count);
    for (std::size_t i=0;i<count;++i)
      for (std::size_t j=0;j<Width;++j) {
        auto bits=word(binary,offset+relative+i*elementBytes+j*4);
        float value;
        std::memcpy(&value,&bits,4);
        if (!std::isfinite(value)) throw std::runtime_error("An animation key is not finite.");
        result[i][j]=value;
      }
    return result;
  }

  static Mat4 restRotation(const nlohmann::json& value) {
    auto q=value.get<bb::Vec4>();
    double magnitude=0;
    for (auto component : q) magnitude+=component*component;
    if (!std::isfinite(magnitude) || std::abs(magnitude-1)>.002)
      throw std::runtime_error("A glTF rotation is not a unit quaternion.");
    for (auto& component : q) component/=std::sqrt(magnitude);
    return rotationMatrix(q);
  }

public:
  explicit MotionGlb(std::string_view bytes) {
    if (bytes.size()<28 || bytes.substr(0,4)!="glTF" || word(bytes,4)!=2 || word(bytes,8)!=bytes.size())
      throw std::runtime_error("Choose a binary glTF 2.0 (.glb) animation.");
    const auto manifest=word(bytes,12);
    if (manifest>4*1024*1024 || manifest>bytes.size()-28 || manifest%4 || bytes.substr(16,4)!="JSON" ||
        bytes.substr(24+manifest,4)!=std::string_view("BIN\0",4) ||
        word(bytes,20+manifest)!=bytes.size()-28-manifest)
      throw std::runtime_error("Invalid binary glTF animation chunks.");
    document=nlohmann::json::parse(bytes.substr(20,manifest),[](int depth,auto,auto&) {
      if (depth>32) throw std::runtime_error("Animation metadata is too deeply nested.");
      return true;
    });
    if (document.at("asset").at("version")!="2.0" || document.at("buffers").size()!=1 ||
        document.at("buffers").at(0).contains("uri"))
      throw std::runtime_error("Animation must use one embedded GLB 2.0 buffer.");
    binary=bytes.substr(28+manifest);
  }

  ImportedMotion retarget(const Humanoid& rig) const {
    const auto& skin=document.at("skins").at(0).at("joints");
    const auto& nodes=document.at("nodes");
    if (skin.size()!=rig.joints.size() || nodes.size()>1000)
      throw std::runtime_error("Animation skeleton must match the RB_Humanoid_v1 joint inventory.");
    std::vector<int> parents(nodes.size(),-1), canonical(nodes.size(),-1);
    for (std::size_t parent=0;parent<nodes.size();++parent)
      if (nodes[parent].contains("children"))
        for (const auto& child : nodes[parent].at("children")) {
          auto& entry=parents.at(child.get<std::size_t>());
          if (entry!=-1) throw std::runtime_error("Animation has multiple parents for a node.");
          entry=int(parent);
        }
    std::vector<Mat4> rest, world;
    std::vector<Vec3> positions;
    for (std::size_t i=0;i<rig.joints.size();++i) {
      const auto index=skin.at(i).get<std::size_t>();
      const auto& node=nodes.at(index);
      const auto parent=rig.joints[i].parent;
      const auto expected=parent<0 ? -1 : skin.at(parent).get<int>();
      if (node.at("name")!=rig.joints[i].name || canonical[index]!=-1 || parents[index]!=expected ||
          node.contains("matrix") || node.value("scale",Vec3{1,1,1})!=Vec3{1,1,1})
        throw std::runtime_error("Incompatible animation joint order, parent or rest transform.");
      canonical[index]=int(i);
      rest.push_back(restRotation(node.value("rotation",bb::Vec4{0,0,0,1})));
      world.push_back(parent<0 ? rest.back() : world[parent]*rest.back());
      const auto local=node.value("translation",Vec3{});
      if (std::any_of(local.begin(),local.end(),[](double x){return !std::isfinite(x) || std::abs(x)>10.;}))
        throw std::runtime_error("The source rig has an invalid joint offset.");
      positions.push_back(parent<0 ? local : add(positions[parent],world[parent].vector(local)));
    }
    auto height=[](const auto& items,const auto& y) {
      const auto order=[&](const auto& a,const auto& b){return y(a)<y(b);};
      const auto [low,high]=std::minmax_element(items.begin(),items.end(),order);
      return y(*high)-y(*low);
    };
    const auto sourceHeight=height(positions,[](const Vec3& p){return p[1];});
    if (sourceHeight<.45 || sourceHeight>3.1)
      throw std::runtime_error("The imported skeleton must be upright and measured in metres.");
    const auto rootScale=height(rig.joints,[](const Joint& j){return j.bindPosition[1];})/sourceHeight;

    struct Track {
      unsigned joint;
      bool translation, hold;
      std::vector<std::array<double,1>> times;
      std::vector<Vec3> offsets;
      std::vector<Mat4> rotations;
    };
    const auto& animation=document.at("animations").at(0);
    const auto& samplers=animation.at("samplers");
    const auto& channels=animation.at("channels");
    if (channels.empty() || channels.size()>rig.joints.size()+1 || samplers.size()>rig.joints.size()+1)
      throw std::runtime_error("Invalid animation track count.");
    std::vector<Track> tracks;
    std::set<std::pair<unsigned,bool>> occupied;
    ImportedMotion motion;
    motion.loop=animation.value("extras",nlohmann::json::object()).value("loop",false);
    for (const auto& channel : channels) {
      const auto& target=channel.at("target");
      const auto index=canonical.at(target.at("node").get<std::size_t>());
      const auto path=target.at("path").get<std::string>();
      if (index<0 || (path!="rotation" && !(index==0 && path=="translation")))
        throw std::runtime_error("Use joint rotations and optional root translation in the imported clip.");
      const auto& input=samplers.at(channel.at("sampler").get<std::size_t>());
      const auto interpolation=input.value("interpolation","LINEAR");
      if (interpolation!="LINEAR" && interpolation!="STEP")
        throw std::runtime_error("Bake imported animation interpolation to LINEAR or STEP.");
      Track track{unsigned(index),path=="translation",interpolation=="STEP",
                  floats<1>(input.at("input").get<std::size_t>(),"SCALAR"),{},{}};
      if (!occupied.emplace(track.joint,track.translation).second ||
          track.times.front()[0]<0 || track.times.back()[0]>8 ||
          std::adjacent_find(track.times.begin(),track.times.end(),std::greater_equal<>{})!=track.times.end())
        throw std::runtime_error("Animation times must be unique, ordered and within 0–8 seconds.");
      const auto output=input.at("output").get<std::size_t>();
      if (track.translation) {
        track.offsets=floats<3>(output,"VEC3");
        for (auto& offset : track.offsets) {
          offset=mul(sub(offset,positions[0]),rootScale);
          if (dot(offset,offset)>125.) throw std::runtime_error("Imported root motion exceeds the supported range.");
        }
      } else {
        const auto inverseRest=bb::inverse(rest[index]);
        const auto inverseWorld=bb::inverse(world[index]);
        for (const auto& key : floats<4>(output,"VEC4"))
          track.rotations.push_back(world[index]*inverseRest*restRotation(key)*inverseWorld);
      }
      if (track.times.size()!=(track.translation ? track.offsets.size() : track.rotations.size()))
        throw std::runtime_error("Animation keys and timestamps do not match.");
      motion.seconds=std::max(motion.seconds,track.times.back()[0]);
      tracks.push_back(std::move(track));
    }
    if (motion.seconds<.05) throw std::runtime_error("The imported clip is shorter than 0.05 seconds.");
    const auto frames=unsigned(std::ceil(motion.seconds*60));
    motion.samples.reserve(frames+1);
    for (unsigned frame=0;frame<=frames;++frame) {
      AnimationPose pose{{},std::vector<Mat4>(rig.joints.size(),Mat4::identity())};
      const std::array<double,1> time{motion.seconds*frame/frames};
      for (const auto& track : tracks) {
        const auto next=std::upper_bound(track.times.begin(),track.times.end(),time);
        const auto b=std::min(std::size_t(next-track.times.begin()),track.times.size()-1);
        const auto a=b && next!=track.times.end() ? b-1 : b;
        const auto mix=a==b || track.hold ? 0. : (time[0]-track.times[a][0])/(track.times[b][0]-track.times[a][0]);
        if (track.translation) pose.root=add(mul(track.offsets[a],1-mix),mul(track.offsets[b],mix));
        else pose.rotations[track.joint]=blendRotation(track.rotations[a],track.rotations[b],mix);
      }
      motion.samples.push_back(std::move(pose));
    }
    return motion;
  }
};
} // namespace ftebroom::character
