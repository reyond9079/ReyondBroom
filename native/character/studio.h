// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "legacy.h"
#include "../ftebroom_model_studio_export.h"
#include <unordered_map>

namespace ftebroom::character {
inline std::vector<Vec3> studioSkinTint(const Humanoid& rig,unsigned dimension) {
  std::vector<Vec3> texels(std::size_t(dimension)*dimension,{1,1,1});
  const auto size = double(dimension-1);
  for (const auto& face : rig.triangles) {
    if (face.material!=0) continue;
    const auto& a = rig.vertices[face.indices[0]];
    const auto& b = rig.vertices[face.indices[1]];
    const auto& c = rig.vertices[face.indices[2]];
    constexpr Vec3 neutral{1,1,1};
    if (a.color==neutral && b.color==neutral && c.color==neutral) continue;
    const Vec3 pa{a.uv[0]*size,(1-a.uv[1])*size,0};
    const Vec3 pb{b.uv[0]*size,(1-b.uv[1])*size,0};
    const Vec3 pc{c.uv[0]*size,(1-c.uv[1])*size,0};
    const auto ab = sub(pb,pa), ac = sub(pc,pa);
    const auto area = cross(ab,ac)[2];
    if (std::abs(area)<1e-7) continue;
    const int xMin = std::max(0,int(std::floor(std::min({pa[0],pb[0],pc[0]}))));
    const int yMin = std::max(0,int(std::floor(std::min({pa[1],pb[1],pc[1]}))));
    const int xMax = std::min(int(size),int(std::ceil(std::max({pa[0],pb[0],pc[0]}))));
    const int yMax = std::min(int(size),int(std::ceil(std::max({pa[1],pb[1],pc[1]}))));
    for (int y=yMin; y<=yMax; ++y) for (int x=xMin; x<=xMax; ++x) {
      const auto relative = sub(Vec3{double(x),double(y),0},pa);
      const auto v = cross(relative,ac)[2]/area, w = cross(ab,relative)[2]/area;
      const auto u = 1-v-w;
      if (std::min({u,v,w})<-.002) continue;
      texels[std::size_t(y)*dimension+x] = add(mul(a.color,u),add(mul(b.color,v),mul(c.color,w)));
    }
  }
  return texels;
}

inline unsigned studioTextureDimension(unsigned material,bool textured) {
  return textured ? (material==0 ? 512 : 256) : 16;
}

inline std::string exportStudio(const Humanoid& rig, const Document& doc,
                                const std::array<modelExport::Skin,materialCount>* maps = nullptr,
                                const ImportedMotion* imported = nullptr) {
  using studioExport::Data;
  using studioExport::V;
  const auto surfaces = legacyTopology(rig,8600,15000,true);
  const auto topology = splitLegacySurfaces(surfaces,1800,3600);
  if (topology.size()>32) throw std::runtime_error("Character exceeds the Studio body part limit.");
  const auto scale = bodyScale(doc)*4;
  const auto boneCount = unsigned(rig.joints.size());
  const auto definitions=characterClips(imported);
  const auto clipCount = unsigned(definitions.size());
  const auto count = unsigned(surfaces.size());
  Data data{std::string(244,0)};
  data.b.replace(0,4,"IDST");
  data.u(4,10);
  data.name(8,64,"ReyondBroom Character Creator");
  auto table = [&](std::size_t header, unsigned n, unsigned stride) {
    const auto at = data.add(std::size_t(n)*stride);
    data.u(header,n);
    data.u(header+4,unsigned(at));
    return at;
  };
  const auto bones = table(140,boneCount,112);
  const auto sequences = table(164,clipCount,176);
  const auto groups = table(172,1,104);
  data.name(groups,32,"default");
  const auto bodies = table(204,unsigned(topology.size()),76);
  const auto models = data.add(topology.size()*112);
  const auto meshes = data.add(topology.size()*20);
  std::array<unsigned,materialCount> materialToTexture{};
  for (unsigned i=0; i<count; ++i) materialToTexture[surfaces[i].material] = i;

  std::vector<Vec3> relatives, global;
  for (unsigned i = 0; i < boneCount; ++i) {
    const auto& joint = rig.joints[i];
    const auto bind = bb::bbToQuake(joint.bindPosition,0,scale);
    const auto origin = joint.parent < 0 ? bind : sub(bind,global[joint.parent]);
    global.push_back(bind);
    relatives.push_back(origin);
    const auto at = bones + i*112;
    data.name(at,32,joint.name);
    data.u(at+32,unsigned(joint.parent));
    for (int channel = 0; channel < 6; ++channel) {
      data.u(at+40+channel*4,UINT32_MAX);
      data.f(at+64+channel*4,channel < 3 ? float(origin[channel]) : 0.f);
      data.f(at+88+channel*4,channel < 3 ? 1.f/256.f : float(bb::pi/32767));
    }
  }
  Mat4 basis = Mat4::identity();
  basis.m[0][0] = basis.m[1][1] = basis.m[2][2] = 0;
  basis.m[0][2] = basis.m[1][0] = -1;
  basis.m[2][1] = 1;
  const auto inverseBasis = bb::inverse(basis);
  const Animator animator(rig);
  auto vector = [](const Vec3& a) { return V{float(a[0]),float(a[1]),float(a[2])}; };
  const V low{-90,-90,-8}, high{90,90,95};
  for (unsigned sequence = 0; sequence < clipCount; ++sequence) {
    const auto& clip = definitions[sequence];
    const auto steps = unsigned(std::ceil(24*clip.seconds));
    const auto frames = steps+1;
    const auto at = sequences+sequence*176;
    data.name(at,32,clip.id);
    data.f(at+32,float(steps/clip.seconds));
    data.u(at+36,clip.loop ? 1 : 0);
    data.u(at+56,frames);
    data.v(at+96,low);
    data.v(at+108,high);
    data.u(at+120,1);
    const auto anim = data.add(boneCount*12);
    data.u(at+124,unsigned(anim));
    std::vector<std::array<std::vector<float>,6>> keys(boneCount);
    for (unsigned f = 0; f < frames; ++f) {
      const auto pose = sampleCharacterClip(animator,clip,f==steps ? clip.seconds : f*clip.seconds/steps,imported);
      for (unsigned b = 0; b < boneCount; ++b) {
        const auto translation = b ? relatives[b] : add(relatives[b],bb::bbToQuake(pose.root,0,scale));
        const auto angles = mul(bb::eulerAngles(basis*pose.rotations[b]*inverseBasis),bb::pi/180);
        for (unsigned axis = 0; axis < 3; ++axis) {
          keys[b][axis].push_back(float(translation[axis]));
          keys[b][3+axis].push_back(float(angles[axis]));
        }
      }
    }
    for (unsigned b = 0; b < boneCount; ++b)
      for (unsigned c = 0; c < 6; ++c) {
        const auto bone = bones+b*112;
        studioExport::channel(data,anim+b*12,c,keys[b][c],data.f(bone+64+c*4),data.f(bone+88+c*4));
      }
  }
  std::vector<Vec3> bindPoints;
  bindPoints.reserve(rig.vertices.size());
  for (const auto& vertex : rig.vertices) bindPoints.push_back(vertex.position);
  const auto bindNormals = surfaceNormals(rig,bindPoints);
  std::unordered_map<unsigned,Vec3> normalsBySource;
  normalsBySource.reserve(bindNormals.size());
  for (std::size_t v=0; v<rig.vertices.size(); ++v)
    normalsBySource.emplace(rig.vertices[v].source,bb::bbToQuake(bindNormals[v]));
  for (unsigned i = 0; i < topology.size(); ++i) {
    const auto& surface = topology[i];
    const auto dimension = studioTextureDimension(surface.material,maps!=nullptr);
    const auto n = unsigned(surface.vertices.size());
    const auto body = bodies+i*76, model = models+i*112, mesh = meshes+i*20;
    data.name(body,64,"character_"+std::to_string(i));
    data.u(body+64,1);
    data.u(body+68,1);
    data.u(body+72,unsigned(model));
    data.name(model,64,"surface_"+std::to_string(i));
    data.u(model+72,1);
    data.u(model+76,unsigned(mesh));
    const auto vertexBones = data.add(n);
    const auto positions = data.add(n*12);
    const auto normalBones = data.add(n);
    const auto normals = data.add(n*12);
    data.u(model+80,n);
    data.u(model+84,unsigned(vertexBones));
    data.u(model+88,unsigned(positions));
    data.u(model+92,n);
    data.u(model+96,unsigned(normalBones));
    data.u(model+100,unsigned(normals));
    for (unsigned v = 0; v < n; ++v) {
      const auto& vertex = surface.vertices[v];
      const auto influence = std::max_element(vertex.weights.begin(),vertex.weights.end())-vertex.weights.begin();
      const auto joint = vertex.joints[influence];
      const auto point = bb::bbToQuake(vertex.position,0,scale);
      data.b[vertexBones+v] = char(joint);
      data.b[normalBones+v] = char(joint);
      data.v(positions+v*12,vector(sub(point,global[joint])));
      data.v(normals+v*12,vector(normalsBySource.at(vertex.source)));
    }
    auto cmd = data.add(surface.triangles.size()*26+2);
    data.u(mesh,unsigned(surface.triangles.size()));
    data.u(mesh+4,unsigned(cmd));
    data.u(mesh+8,materialToTexture[surface.material]);
    data.u(mesh+12,n);
    auto shortValue = [&](int value) {
      const auto bits = std::uint16_t(value);
      data.b[cmd++] = char(bits);
      data.b[cmd++] = char(bits>>8);
    };
    for (std::size_t t=0; t<surface.triangles.size(); ++t) {
      shortValue(3);
      for (const auto corner : {0u,2u,1u}) {
        const auto id = surface.triangles[t][corner];
        const auto& uv = surface.cornerUv[t][corner];
        shortValue(int(id));
        shortValue(int(id));
        shortValue(int(std::lround(uv[0]*(dimension-1))));
        shortValue(int(std::lround((1-uv[1])*(dimension-1))));
      }
    }
  }
  const auto textures = table(180,count,80);
  const auto refs = data.add(count*2);
  data.u(192,count);
  data.u(196,1);
  data.u(200,unsigned(refs));
  for (unsigned i=0; i<count; ++i) data.b[refs+i*2] = char(i);

  // GoldSrc keeps the model data before the texture payload. All mesh and
  // animation offsets must refer to bytes before this boundary.
  data.u(188,unsigned(data.add(0)));
  for (unsigned i=0; i<count; ++i) {
    const auto& surface = surfaces[i];
    const auto dimension = studioTextureDimension(surface.material,maps!=nullptr);
    const auto texture = textures+i*80;
    const auto color = materialColor(surface.material,doc);
    const auto* image = maps ? &maps->at(surface.material) : nullptr;
    const bool masked = image && cutoutMaterial(surface.material);
    constexpr unsigned studioMasked = 0x0040;
    const unsigned pixelCount = dimension*dimension;
    const auto pixels = data.add(pixelCount+768);
    data.name(texture,64,"material"+std::to_string(surface.material));
    if (masked) data.u(texture+64,studioMasked);
    data.u(texture+68,dimension);
    data.u(texture+72,dimension);
    data.u(texture+76,unsigned(pixels));
    if (image) {
      if (!image->width || !image->height ||
          image->rgba.size()!=std::size_t(image->width)*image->height*4)
        throw std::runtime_error("Missing Studio character texture.");
      std::string palette;
      palette.reserve(768);
      // Build a per-material RGB palette from the populated 5-bit RGB bins.
      struct Bin { unsigned count = 0; std::array<unsigned,3> sum{}; };
      constexpr unsigned bins = 32*32*32;
      std::vector<Bin> histogram(bins);
      std::vector<std::array<unsigned,4>> samples;
      samples.reserve(pixelCount);
      const auto tint = surface.material==0 ? studioSkinTint(rig,dimension) : std::vector<Vec3>{};
      for (unsigned y=0; y<dimension; ++y) for (unsigned x=0; x<dimension; ++x) {
        const auto sx = std::min(image->width-1,(2*x+1)*image->width/(2*dimension));
        const auto sy = std::min(image->height-1,(2*y+1)*image->height/(2*dimension));
        const auto pixel = &image->rgba[(std::size_t(sy)*image->width+sx)*4];
        std::array<unsigned,4> rgba{pixel[0],pixel[1],pixel[2],pixel[3]};
        if (!tint.empty())
          for (unsigned channel=0; channel<3; ++channel)
            rgba[channel] = unsigned(std::clamp(std::lround(rgba[channel]*tint[y*dimension+x][channel]),0l,255l));
        samples.push_back(rgba);
        if (masked && rgba[3]<128) continue;
        const auto& rgb = rgba;
        auto& bin = histogram[(rgb[0]>>3)*1024 + (rgb[1]>>3)*32 + (rgb[2]>>3)];
        ++bin.count;
        for (unsigned c=0; c<3; ++c) bin.sum[c] += rgb[c];
      }
      std::stable_sort(histogram.begin(),histogram.end(),[](const Bin& a,const Bin& b) {
        return a.count>b.count;
      });
      const auto colors = std::clamp<unsigned>(std::count_if(histogram.begin(),histogram.end(),
                         [](const Bin& bin) { return bin.count!=0; }),1,masked ? 255 : 256);
      for (unsigned entry=0; entry<colors; ++entry)
        for (const auto sum : histogram[entry].sum)
          palette.push_back(char(sum/std::max(1u,histogram[entry].count)));
      palette.resize(768,0);
      ftebroom::PaletteQuantizer quantizer(palette,colors);
      for (unsigned p=0; p<pixelCount; ++p) {
        const auto& rgba = samples[p];
        data.b[pixels+p] = char(masked && rgba[3]<128 ? 255 : quantizer.nearest(rgba[0],rgba[1],rgba[2]));
      }
      data.b.replace(pixels+pixelCount,768,palette);
    } else {
      for (unsigned c=0; c<3; ++c)
        data.b[pixels+pixelCount+c] = char(std::lround(color[c]*255));
    }
  }
  data.v(88,low);
  data.v(100,high);
  data.v(112,low);
  data.v(124,high);
  data.u(72,unsigned(data.b.size()));
  return std::move(data.b);
}
} // namespace ftebroom::character
