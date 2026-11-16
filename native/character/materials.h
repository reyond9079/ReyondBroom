// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "wardrobe.h"

namespace ftebroom::character {
inline constexpr const char* complexionNames[]{
  "Automatic", "Warm", "Neutral", "Deep", "Soft", "Mature", "Fair", "Freckled"
};

inline constexpr char surfaceProgramPath[] = "glsl/reyond_character_surface_v4.glsl";
inline constexpr char surfaceDetailPath[] = "textures/reyond_character_detail_v3.png";
inline constexpr char fabricDetailPath[] = "textures/reyond_character_fabric_v4.png";

inline const char* surfaceVariant(unsigned material) {
  switch (material) {
    case 0: return "SKIN";
    case 1: return "EYE";
    case 5: return "HAIR";
    case 3: case 4: case 6: return "CLOTH";
    default: return nullptr;
  }
}

inline double fabricVertexMask(unsigned material,const Document& doc) {
  if (material==4) return .875; // Leather.
  if (material==6) return .625; // Wool felt.
  const std::string_view garment = outfits[doc.outfit].clothes;
  if (garment.find("elegant")!=garment.npos || garment=="male_casualsuit06") return .625;
  if (garment=="male_worksuit01" || garment=="male_casualsuit02") return .375;
  return .125; // Cotton, with a denim tint mask for mixed atlases.
}

inline std::array<std::string,materialCount> surfaceMaps(const Document& doc,bool pbr=false) {
  const auto& outfit = outfits[doc.outfit];
  const auto& hair = hairstyles[doc.hairstyle];
  const auto fittedFootwear = !doc.footwear && outfit.footwearAtlas;
  constexpr const char* skin[]{"","skin-warm","skin-medium","skin-deep","skin-soft","skin-mature",
                                   "skin-fair","skin-freckled"};
  constexpr const char* shoes[]{"","","shoes02","shoes03","shoes04"};
  const auto complexion = doc.complexion ? skin[doc.complexion] :
      doc.skin[0] < .43 ? "skin-deep" : doc.skin[0] < .58 ? "skin-medium" :
      doc.skin[0] >= .73 ? "skin-fair" : "skin-warm";
  const auto eyeColor = doc.eyes[2] > doc.eyes[1] ? "blue" :
                        doc.eyes[1] > doc.eyes[0] ? "green" : "brown";
  const auto eye = std::string(pbr ? "eyes-pbr-" : "eyes-")+eyeColor;
  std::array<std::string,materialCount> maps{complexion,eye,"",outfit.atlas ? outfit.atlas : outfit.clothes,
      fittedFootwear ? outfit.footwearAtlas : doc.footwear ? shoes[doc.footwear] : outfit.shoes,
      hair.fringeAtlas ? hair.fringeAtlas : hair.mesh,
      "fedora01","",eyebrowMeshes[doc.eyebrow],"eyelashes01"};
  if (pbr)
    for (std::size_t i=3; i<maps.size(); ++i)
      if (!maps[i].empty() && !(i==3 && outfit.atlas) && !(i==4 && fittedFootwear) && !(i==5 && hair.fringeAtlas))
        maps[i].insert(0,"hd-");
  if (!doc.customWardrobe.empty()) maps[3].clear();
  return maps;
}

static_assert(std::size(complexionNames)==complexionCount);

inline Vec3 surfaceTint(unsigned material,const Document& doc,std::string_view map) {
  if (map.empty()) return materialColor(material,doc);
  if (material == 0) {
    const Vec3 reference = map=="skin-deep" ? Vec3{.37,.24,.19} :
        map=="skin-medium" ? Vec3{.54,.36,.28} : Vec3{.68,.45,.35};
    Vec3 tint{};
    for (unsigned i = 0; i < 3; ++i) {
      // The lighter atlases already carry their own pigmentation. Preserve it
      // as the picker moves into the upper end of the complexion range.
      const auto maxGain = map=="skin-deep" || map=="skin-medium" ? 1.42 : 1.18;
      tint[i] = std::clamp(std::pow((doc.skin[i]+.07)/(reference[i]+.07),.53),.46,maxGain);
    }
    return tint;
  }
  if (material == 5 || material == browMaterial) {
    constexpr Vec3 reference{.14,.09,.07};
    Vec3 tint{};
    for (unsigned i = 0; i < 3; ++i)
      tint[i] = std::clamp(std::pow((doc.hairColor[i]+.035)/(reference[i]+.035),.76),.45,2.4);
    return tint;
  }
  if (material==1) return {.96,.94,.93};
  if (material==lashMaterial) return {.88,.80,.73};
  return {1,1,1};
}

inline double displayToLinear(double value) {
  return value <= .04045 ? value/12.92 : std::pow((value+.055)/1.055,2.4);
}

inline double surfaceOpacity(unsigned material) {
  return material==lashMaterial ? .54 : 1.;
}

inline double skinVertexMask(const Vertex& vertex) {
  // Both regions are disjoint. Neutral skin sits at the middle of the byte.
  return .5+.5*(vertex.lipMask-vertex.scalpMask);
}
} // namespace ftebroom::character
