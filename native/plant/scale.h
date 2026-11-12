// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "presets.h"
namespace ftebroom::plant {
// Standing collision hulls: Quake/Quake II/Quake III [-24,32]; GoldSrc
// [-36,36]. FTE-specific content deliberately follows Half-Life (RB-30).
inline double humanHeightUnits(int profile) {
  switch(profile) {
    case 0: case 2: case 3:return 56.;
    case 1: case 4:return 72.;
    default:throw std::runtime_error("Unknown plant scale game profile");
  }
}
inline std::vector<Project> presetsForProfile(int profile) {
  const double unitsPerMetre=humanHeightUnits(profile)/1.8;
  auto library=presets();
  const auto& entries=catalogManifest().at("presets");
  for(size_t i=0;i<library.size();i++) {
    auto& d=library[i];
    const double scale=entries.at(i).at("defaultHeightMetres").get<double>()*unitsPerMetre/d.p.height;
    // Scale dimensional traits together; angles, ratios and normalized curves
    // retain their botanical shape. Applied only when instantiating defaults,
    // never when opening projects, reusing a scene plant or baking an export.
    d.p.height*=scale;d.p.radius*=scale;d.p.leafSize*=scale;d.p.flowerSize*=scale;
    for(auto& node:d.nodes)node.offset=mul(node.offset,scale);
    // Keep bark density within the existing authoring range.
    d.barkVUnits=std::clamp(d.barkVUnits*scale,16.,512.);
    validate(d);
  }
  return library;
}
}
