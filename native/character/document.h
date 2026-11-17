// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "animation.h"
#include "../third_party/json.hpp"
#include <optional>
#include <random>

namespace ftebroom::character {
enum class MorphKind { Height, Width, Depth, Vertical, Radial, Tilt };
struct Morph {
  const char* id;
  const char* label;
  Vec3 center;
  Vec3 radii;
  double amount;
  MorphKind kind;
};

inline constexpr Morph morphs[] = {
  {"height","Height",{},{},.14,MorphKind::Height},
  {"fat","Body fat",{0,9.4,.25},{3.7,4.5,2.4},.20,MorphKind::Radial},
  {"muscle","Muscle",{1.85,12.1,.45},{1.45,2.3,1.25},.17,MorphKind::Radial},
  {"bulk","Body bulk",{0,11.5,.1},{3.9,3.6,2.05},.12,MorphKind::Radial},
  {"shoulders","Shoulder width",{1.58,13.45,.15},{1.4,1.0,1.2},.29,MorphKind::Width},
  {"chest","Chest depth",{0,12.12,1.05},{1.78,1.37,.93},.22,MorphKind::Depth},
  {"waist","Waist width",{0,10.25,.1},{1.76,.81,1.5},.27,MorphKind::Width},
  {"belly","Belly",{0,9.8,1.02},{1.2,1.2,1.15},.34,MorphKind::Depth},
  {"hips","Hip width",{1.3,8.35,.04},{1.1,1.38,1.5},.21,MorphKind::Width},
  {"neck","Neck width",{0,14.1,.25},{.74,.75,.86},.16,MorphKind::Width},
  {"upper_arm","Upper arm",{2.25,12.35,.2},{1.03,1.24,.91},.15,MorphKind::Radial},
  {"forearm","Forearm",{3.65,11.08,.97},{.95,1.04,1.29},.13,MorphKind::Radial},
  {"thigh","Thigh",{1.2,7.25,.34},{1.26,1.48,1.34},.18,MorphKind::Radial},
  {"calf","Calf",{1.88,3.0,.09},{.97,1.36,1.0},.16,MorphKind::Radial},
  {"head_width","Head width",{0,15.60,.58},{1.04,1.42,1.04},.10,MorphKind::Width},
  {"head_height","Head height",{0,15.92,.45},{.94,1.17,1.03},.16,MorphKind::Vertical},
  {"head_depth","Head depth",{0,15.70,-.07},{.95,1.13,.86},.13,MorphKind::Depth},
  {"forehead_height","Forehead height",{0,15.96,1.27},{.78,.55,.43},.15,MorphKind::Vertical},
  {"forehead_slope","Forehead slope",{0,16.14,1.06},{.73,.52,.57},.15,MorphKind::Depth},
  {"brow_height","Brow height",{.32,15.66,1.40},{.46,.19,.29},.075,MorphKind::Vertical},
  {"brow_width","Brow width",{.45,15.65,1.31},{.39,.21,.32},.065,MorphKind::Width},
  {"eye_size","Eye size",{.31,15.44,1.43},{.25,.22,.37},.082,MorphKind::Radial},
  {"eye_spacing","Eye spacing",{.31,15.47,1.39},{.28,.26,.42},.085,MorphKind::Width},
  {"eye_depth","Eye depth",{.32,15.46,1.46},{.29,.20,.38},.08,MorphKind::Depth},
  {"eye_tilt","Eye tilt",{.32,15.47,1.43},{.32,.25,.35},.14,MorphKind::Tilt},
  {"nose_bridge","Nose bridge",{0,15.48,1.56},{.21,.32,.34},.09,MorphKind::Depth},
  {"nose_width","Nose width",{0,15.24,1.55},{.25,.27,.35},.065,MorphKind::Width},
  {"nose_length","Nose length",{0,15.18,1.70},{.22,.39,.30},.09,MorphKind::Vertical},
  {"nose_tip","Nose tip",{0,15.25,1.75},{.15,.16,.21},.083,MorphKind::Depth},
  {"nostrils","Nostrils",{.12,15.15,1.58},{.13,.12,.21},.048,MorphKind::Width},
  {"cheek_height","Cheek height",{.55,15.34,1.1},{.37,.32,.40},.093,MorphKind::Vertical},
  {"cheek_fullness","Cheek fullness",{.54,15.18,1.17},{.38,.37,.47},.13,MorphKind::Depth},
  {"jaw_width","Jaw width",{.54,14.92,.82},{.45,.43,.74},.13,MorphKind::Width},
  {"jaw_length","Jaw length",{.35,14.79,1.11},{.58,.41,.56},.14,MorphKind::Vertical},
  {"jaw_angle","Jaw angle",{.58,14.91,.45},{.35,.46,.53},.17,MorphKind::Tilt},
  {"chin_height","Chin height",{0,14.78,1.23},{.39,.28,.46},.13,MorphKind::Vertical},
  {"chin_width","Chin width",{.14,14.81,1.31},{.26,.26,.35},.07,MorphKind::Width},
  {"chin_projection","Chin projection",{0,14.83,1.31},{.38,.31,.41},.14,MorphKind::Depth},
  {"mouth_width","Mouth width",{.21,14.99,1.45},{.24,.21,.35},.082,MorphKind::Width},
  {"mouth_height","Mouth height",{0,15.01,1.51},{.38,.26,.29},.07,MorphKind::Vertical},
  {"lip_fullness","Lip fullness",{0,15.01,1.54},{.36,.15,.22},.067,MorphKind::Depth},
  {"upper_lip","Upper lip",{0,15.074,1.52},{.34,.10,.23},.055,MorphKind::Vertical},
  {"lower_lip","Lower lip",{0,14.96,1.53},{.33,.09,.23},.055,MorphKind::Vertical},
  {"ear_size","Ear size",{.76,15.38,.29},{.35,.48,.46},.15,MorphKind::Radial},
  {"ear_angle","Ear angle",{.76,15.44,.23},{.35,.44,.47},.19,MorphKind::Tilt},
  {"eye_height","Eye height",{.31,15.46,1.37},{.32,.27,.44},.07,MorphKind::Vertical},
  {"cheek_width","Cheek width",{.55,15.27,1.04},{.34,.39,.48},.10,MorphKind::Width},
  {"ear_depth","Ear projection",{.76,15.39,.23},{.36,.49,.46},.13,MorphKind::Width}
};
inline constexpr std::size_t morphCount = sizeof(morphs) / sizeof(*morphs);
inline constexpr std::size_t legacyMorphCount = 45;
inline constexpr std::size_t bodyMorphCount = 14;
inline constexpr const char* expressions[]{"Neutral","Smile","Frown","Surprise","Blink","Anger"};
inline constexpr const char* beards[]{"None","Moustache","Goatee","Full beard"};

inline constexpr const char* exportProfileIds[]{"quake3","fte","quake1","quake2","halflife"};

struct WardrobeLicense {
  std::string license, author, source, sha256;
};

inline WardrobeLicense readWardrobeLicense(const nlohmann::json& value, std::string_view garmentHash) {
  const auto text = [&](const char* field, std::size_t limit) {
    const auto item = value.find(field);
    if (item == value.end() || !item->is_string())
      throw std::runtime_error(std::string("Garment license is missing the ")+field+" field.");
    const auto& content = item->get_ref<const std::string&>();
    if (content.empty() || content.size()>limit || content.find_first_not_of(' ')==content.npos ||
        std::any_of(content.begin(),content.end(),[](unsigned char c) { return c<32 || c==127; }))
      throw std::runtime_error(std::string("Invalid ")+field+" in garment license.");
    return content;
  };
  auto license = text("license",64);
  constexpr std::string_view supported[]{"CC0-1.0","CC-BY-4.0","CC-BY-SA-4.0",
                                        "LicenseRef-OwnWork","LicenseRef-Permission"};
  if (std::find(std::begin(supported),std::end(supported),license)==std::end(supported))
    throw std::runtime_error("Unsupported garment license. Use CC0-1.0, CC-BY-4.0, CC-BY-SA-4.0, LicenseRef-OwnWork or LicenseRef-Permission.");
  const auto hash = text("sha256",64);
  if (garmentHash.size()!=64 || hash!=garmentHash)
    throw std::runtime_error("Garment license SHA-256 does not match the .rbw file.");
  return {std::move(license),text("author",160),text("source",512),hash};
}

struct Document {
  std::string name = "Human";
  std::array<double, morphCount> values{};
  std::array<double, 3> skin{.68, .45, .35};
  std::array<double, 3> eyes{.24, .35, .31};
  std::array<double, 3> hairColor{.14,.09,.07};
  std::uint32_t seed = 0;
  std::string clip = "idle_neutral";
  std::string animationAsset;
  std::string customWardrobe;
  std::optional<WardrobeLicense> wardrobeLicense;
  unsigned outfit = 0, hairstyle = 1;
  unsigned expression = 0, beard = 0;
  unsigned footwear = 0, headwear = 0, complexion = 0;
  // Zero keeps documents produced before the face templates on their original geometry.
  double faceProfile = 0;
  double bodyProfile = 0;
  unsigned faceTemplate = 0, eyebrow = 0;
  double hairVolume = 0, hairLength = 0, hairline = 0;
  double faceAsymmetry = 0;
  bool hat = true;
  int exportProfile = -1; // Follow the map profile unless the document specifies a target.
};
inline constexpr unsigned outfitCount = 13, hairstyleCount = 18, complexionCount = 8;
inline constexpr unsigned selectableFaceTemplateCount = 4;
// Keep this ID readable for saved documents even though it is retired.
inline constexpr unsigned fieldTechnicianFaceTemplate = 4;
inline constexpr unsigned faceTemplateCount = fieldTechnicianFaceTemplate+1, eyebrowCount = 4;
inline constexpr const char* footwearLabels[]{"Outfit default","None","Low shoes","Dress shoes","Boots"};
inline constexpr const char* headwearLabels[]{"Outfit default","None","Fedora"};

inline double boundedNumber(const nlohmann::json& value, double low, double high) {
  if (!value.is_number()) throw std::runtime_error("Expected a number in the character document");
  const auto number = value.get<double>();
  if (!std::isfinite(number) || number < low || number > high)
    throw std::runtime_error("Character document value is outside its supported range");
  return number;
}

inline nlohmann::json writeDocument(const Document& doc, const Humanoid& rig) {
  nlohmann::json values = nlohmann::json::object();
  for (std::size_t i = 0; i < morphCount; ++i) values[morphs[i].id] = doc.values[i];
  nlohmann::json output = {{"format","rbchar"}, {"schemaVersion",1}, {"generatorVersion",1},
          {"rig","RB_Humanoid_v1"}, {"rigSha256",rig.rigHash}, {"name",doc.name},
          {"seed",doc.seed}, {"morphs",values}, {"skin",doc.skin}, {"eyes",doc.eyes},
          {"animationBank",animationBank}, {"clip",doc.clip},
          {"wardrobeVersion",1},
          {"outfit",doc.outfit}, {"hairstyle",doc.hairstyle}, {"hairColor",doc.hairColor},
          {"hat",doc.hat}, {"expression",doc.expression}, {"beard",doc.beard},
          {"footwear",doc.footwear}, {"headwear",doc.headwear}, {"complexion",doc.complexion},
          {"faceProfile",doc.faceProfile}, {"bodyProfile",doc.bodyProfile},
          {"faceTemplate",doc.faceTemplate}, {"eyebrow",doc.eyebrow},
          {"hairVolume",doc.hairVolume}, {"hairLength",doc.hairLength}, {"hairline",doc.hairline},
          {"faceAsymmetry",doc.faceAsymmetry}};
  if (!doc.animationAsset.empty()) output["animationAsset"]=doc.animationAsset;
  if (!doc.customWardrobe.empty()) output["customWardrobe"]=doc.customWardrobe;
  if (doc.wardrobeLicense) {
    const auto& license = *doc.wardrobeLicense;
    output["wardrobeLicense"]={{"license",license.license},{"author",license.author},
                               {"source",license.source},{"sha256",license.sha256}};
  }
  if (doc.exportProfile>=0 && std::size_t(doc.exportProfile)<std::size(exportProfileIds))
    output["exportProfile"]=exportProfileIds[doc.exportProfile];
  return output;
}

inline nlohmann::json parseDocument(std::string_view bytes) {
  if (bytes.size() > 65536) throw std::runtime_error("Character document is too large");
  std::array<std::set<std::string>,9> keys;
  return nlohmann::json::parse(bytes, [&](int depth, auto event, auto& token) {
    if (depth > 8) throw std::runtime_error("Character document is too deeply nested");
    if (event == nlohmann::json::parse_event_t::object_start && depth < 8) keys[depth+1].clear();
    if (event == nlohmann::json::parse_event_t::key &&
        !keys[depth].insert(token.template get<std::string>()).second)
      throw std::runtime_error("Character document contains a duplicate key");
    return true;
  });
}

inline Document decodeDocument(const nlohmann::json& value, const Humanoid& rig) {
  if (value.at("format") != "rbchar" || value.at("schemaVersion") != 1 ||
      value.at("generatorVersion") != 1 || value.at("rig") != "RB_Humanoid_v1" ||
      value.at("rigSha256") != rig.rigHash)
    throw std::runtime_error("Unsupported character document or incompatible rig");
  Document doc;
  doc.name = value.at("name").get<std::string>();
  if (doc.name.empty() || doc.name.size() > 100 ||
      std::any_of(doc.name.begin(),doc.name.end(),[](unsigned char c) { return c < 32 || c == 127; }))
    throw std::runtime_error("Invalid character name");
  const auto& seed = value.at("seed");
  if (!seed.is_number_integer()) throw std::runtime_error("Invalid character seed");
  doc.seed = static_cast<std::uint32_t>(boundedNumber(seed, 0, UINT32_MAX));
  if (value.value("wardrobeVersion",1) != 1)
    throw std::runtime_error("Unsupported character wardrobe version");
  if (value.at("animationBank") != animationBank)
    throw std::runtime_error("Unsupported character animation bank");
  const auto readAsset=[&](const char* field) {
    const auto id=value.value(field,std::string{});
    if (!id.empty() && (id.size()!=64 || id.find_first_not_of("0123456789abcdef")!=id.npos))
      throw std::runtime_error("Invalid imported character asset reference.");
    return id;
  };
  doc.animationAsset=readAsset("animationAsset");
  doc.customWardrobe=readAsset("customWardrobe");
  if (value.contains("wardrobeLicense"))
    doc.wardrobeLicense=readWardrobeLicense(value.at("wardrobeLicense"),doc.customWardrobe);
  if (value.contains("exportProfile")) {
    const auto profile = value.at("exportProfile").get<std::string>();
    const auto match = std::find(std::begin(exportProfileIds),std::end(exportProfileIds),profile);
    if (match==std::end(exportProfileIds)) throw std::runtime_error("Unknown character export profile");
    doc.exportProfile=int(match-std::begin(exportProfileIds));
  }
  doc.clip = value.at("clip").get<std::string>();
  if ((doc.clip!=importedClipId || doc.animationAsset.empty()) &&
      std::none_of(std::begin(clips), std::end(clips), [&](const Clip& clip) { return doc.clip == clip.id; }))
    throw std::runtime_error("Unknown character animation clip");
  const auto& settings = value.at("morphs");
  if (!settings.is_object() || (settings.size()!=morphCount && settings.size()!=legacyMorphCount))
    throw std::runtime_error("Invalid character morph table");
  for (std::size_t i = 0; i < settings.size(); ++i)
    doc.values[i] = boundedNumber(settings.at(morphs[i].id), -1, 1);
  auto readColor = [&](const char* name, std::array<double, 3>& output) {
    const auto& color = value.at(name);
    if (!color.is_array() || color.size() != 3)
      throw std::runtime_error("Invalid character color");
    for (std::size_t i = 0; i < color.size(); ++i)
      output[i] = boundedNumber(color[i], 0, 1);
  };
  readColor("skin", doc.skin);
  readColor("eyes", doc.eyes);
  readColor("hairColor", doc.hairColor);
  auto readIndex = [&](const char* key, unsigned limit) {
    const auto& input = value.at(key);
    if (!input.is_number_integer()) throw std::runtime_error("Invalid character asset selection");
    return unsigned(boundedNumber(input,0,limit-1));
  };
  doc.outfit = readIndex("outfit",outfitCount);
  doc.hairstyle = readIndex("hairstyle",hairstyleCount);
  doc.expression = readIndex("expression",std::size(expressions));
  doc.beard = readIndex("beard",std::size(beards));
  if (value.contains("footwear")) doc.footwear = readIndex("footwear",std::size(footwearLabels));
  if (value.contains("headwear")) doc.headwear = readIndex("headwear",std::size(headwearLabels));
  if (value.contains("complexion")) doc.complexion = readIndex("complexion",complexionCount);
  if (value.contains("faceProfile")) doc.faceProfile = boundedNumber(value.at("faceProfile"),-1,1);
  if (value.contains("bodyProfile")) doc.bodyProfile = boundedNumber(value.at("bodyProfile"),-1,1);
  if (value.contains("faceTemplate")) doc.faceTemplate = readIndex("faceTemplate",faceTemplateCount);
  if (value.contains("eyebrow")) doc.eyebrow = readIndex("eyebrow",eyebrowCount);
  if (value.contains("hairVolume")) doc.hairVolume = boundedNumber(value.at("hairVolume"),-1,1);
  if (value.contains("hairLength")) doc.hairLength = boundedNumber(value.at("hairLength"),-1,1);
  if (value.contains("hairline")) doc.hairline = boundedNumber(value.at("hairline"),-1,1);
  if (value.contains("faceAsymmetry")) doc.faceAsymmetry = boundedNumber(value.at("faceAsymmetry"),-1,1);
  doc.hat = value.at("hat").get<bool>();
  return doc;
}

inline Document readDocument(std::string_view bytes, const Humanoid& rig) {
  return decodeDocument(parseDocument(bytes),rig);
}

inline void randomize(Document& doc, std::uint32_t seed) {
  std::mt19937 rng(seed);
  doc.seed = seed;
  for (auto& value : doc.values) value = (2.0 * rng() / std::mt19937::max() - 1) * .56;
}

inline Vec3 morphDelta(const Morph& morph, Vec3 point) {
  const auto side = point[0] < 0 ? -1. : 1.;
  point[0] = std::abs(point[0]);
  const auto offset = sub(point, morph.center);
  double radius = 0;
  for (int i = 0; i < 3; ++i) {
    const auto axis = offset[i] / morph.radii[i];
    radius += axis * axis;
  }
  if (radius >= 1) return {};
  Vec3 direction{};
  switch (morph.kind) {
    case MorphKind::Width: direction[0] = std::min(1., point[0] * 5.); break;
    case MorphKind::Vertical: direction[1] = 1; break;
    case MorphKind::Depth: direction[2] = 1; break;
    case MorphKind::Radial:
      direction = {morph.center[0] ? offset[0] / morph.radii[0] : std::min(1., point[0]),
                   offset[1] * .4 / morph.radii[1], offset[2] / morph.radii[2]};
      break;
    case MorphKind::Tilt: direction[1] = offset[0] / morph.radii[0]; break;
    case MorphKind::Height: break;
  }
  direction[0] *= side;
  return mul(direction, morph.amount * (1-radius) * (1-radius));
}

inline Vec3 morphPoint(const Document& doc, const Vec3& reference) {
  auto point = reference;
  for (std::size_t i = 1; i < morphCount; ++i)
    if (doc.values[i] &&
        std::abs(std::abs(reference[0])-morphs[i].center[0]) < morphs[i].radii[0] &&
        std::abs(reference[1]-morphs[i].center[1]) < morphs[i].radii[1] &&
        std::abs(reference[2]-morphs[i].center[2]) < morphs[i].radii[2])
      point = add(point,mul(morphDelta(morphs[i],reference),doc.values[i]));
  // Resting lids sit slightly lower over the globe. Apply the same offset to
  // wardrobe anchors so the upper lashes follow the reshaped lid.
  if (reference[1]>15.43 && reference[1]<15.62 && reference[2]>1.37) {
    constexpr Morph restingLid{"","",{.31,15.512,1.422},{.18,.09,.09},.019,MorphKind::Vertical};
    point = sub(point,morphDelta(restingLid,reference));
  }
  if (doc.expression) {
    constexpr Morph corner{"","",{.25,15.00,1.45},{.20,.19,.33},.12,MorphKind::Vertical};
    constexpr Morph lowerLip{"","",{0,14.96,1.51},{.39,.18,.33},.14,MorphKind::Vertical};
    constexpr Morph brow{"","",{.34,15.65,1.40},{.47,.22,.30},.12,MorphKind::Vertical};
    constexpr Morph upperLid{"","",{.31,15.50,1.54},{.20,.115,.29},.11,MorphKind::Vertical};
    constexpr Morph lowerLid{"","",{.31,15.41,1.53},{.20,.095,.27},.08,MorphKind::Vertical};
    constexpr double layers[][5]{{0,0,0,0,0},{1,-.16,.12,-.12,.4},
        {-.68,.2,.32,0,0},{0,-1,.85,.38,-.26},{0,0,0,-1,1},{-.4,-.18,-.68,-.32,.44}};
    constexpr Morph controls[]{corner,lowerLip,brow,upperLid,lowerLid};
    for (std::size_t i = 0; i < std::size(controls); ++i)
      if (layers[doc.expression][i])
        point = add(point,mul(morphDelta(controls[i],reference),layers[doc.expression][i]));
  }
  if (doc.faceAsymmetry && reference[1]>14.7 && reference[1]<15.85 && reference[2]>1.) {
    const auto side = std::copysign(1.,reference[0]);
    // Sub-millimetre offsets in the canonical rig's decimetre units.
    constexpr Morph eye{"","",{.31,15.47,1.40},{.23,.21,.37},.0045,MorphKind::Vertical};
    constexpr Morph brow{"","",{.34,15.65,1.40},{.37,.22,.30},.003,MorphKind::Vertical};
    constexpr Morph mouth{"","",{.25,15.00,1.48},{.23,.18,.30},.005,MorphKind::Vertical};
    const auto asymmetric = add(add(morphDelta(eye,reference),morphDelta(brow,reference)),
                                mul(morphDelta(mouth,reference),-.76));
    point = add(point,mul(asymmetric,doc.faceAsymmetry*side));
  }
  return point;
}

enum MorphIndex : std::size_t {
  Height=0, BodyFat, Muscle, BodyBulk, ShoulderWidth, ChestDepth, WaistWidth, BellyDepth,
  HipWidth, NeckWidth, UpperArm, Forearm,
  HeadWidth=bodyMorphCount, HeadHeight, HeadDepth, ForeheadHeight, ForeheadSlope,
  BrowHeight, BrowWidth, EyeSize, EyeSpacing, EyeDepth, EyeTilt, NoseBridge,
  NoseWidth, NoseLength, NoseTip, Nostrils, CheekHeight, CheekFullness, JawWidth,
  JawLength, JawAngle, ChinHeight, ChinWidth, ChinProjection, MouthWidth, MouthHeight,
  LipFullness, UpperLip, LowerLip, EarSize, EarAngle, EyeHeight, CheekWidth, EarDepth
};
static_assert(EarAngle+1==legacyMorphCount && EarDepth+1==morphCount &&
              std::string_view(morphs[NoseBridge].id)=="nose_bridge");

struct FaceRegion {
  const char* label;
  Vec3 point;
  double radius;
  bool paired;
  MorphIndex across, vertical, depth, extra;
};

inline constexpr FaceRegion faceRegions[]{
  {"Forehead",{0,15.99,1.39},.23,false,HeadWidth,ForeheadHeight,ForeheadSlope,HeadHeight},
  {"Eyebrows",{.37,15.65,1.32},.133,true,BrowWidth,BrowHeight,ForeheadSlope,BrowWidth},
  {"Eyes",{.309,15.45,1.39},.116,true,EyeSpacing,EyeHeight,EyeDepth,EyeSize},
  {"Nose bridge",{0,15.42,1.60},.104,false,NoseWidth,NoseLength,NoseBridge,NoseWidth},
  {"Nose tip",{0,15.22,1.70},.113,false,Nostrils,NoseLength,NoseTip,NoseWidth},
  {"Cheeks",{.51,15.21,1.28},.145,true,CheekWidth,CheekHeight,CheekFullness,HeadWidth},
  {"Mouth",{0,15.008,1.52},.136,false,MouthWidth,MouthHeight,LipFullness,UpperLip},
  {"Jaw",{.53,14.94,.86},.135,true,JawWidth,JawLength,JawAngle,HeadDepth},
  {"Chin",{0,14.79,1.30},.137,false,ChinWidth,ChinHeight,ChinProjection,LowerLip},
  {"Ears",{.74,15.38,.25},.14,true,EarDepth,EarSize,EarAngle,EarDepth},
};

inline Vec3 faceShapeDelta(const Humanoid& rig,const Document& doc,unsigned source) {
  if (!rig.faceShapes || source>=rig.faceShapes->bySource.size()) return {};
  if (!doc.faceTemplate && doc.faceProfile==0) return {};
  const auto& deltas = rig.faceShapes->bySource[source];
  const double weight = .5+.5*doc.faceProfile;
  auto mix = [&](unsigned index) {
    return add(mul(deltas[index],1-weight),mul(deltas[index+1],weight));
  };
  if (doc.faceTemplate==fieldTechnicianFaceTemplate) return mix(0);
  if (doc.faceTemplate) return mix((doc.faceTemplate-1)*2);
  return mul(add(add(mix(0),mix(2)),mix(4)),1./3.);
}

inline std::vector<Vec3> morphBody(const Humanoid& rig, const Document& doc) {
  std::vector<Vec3> positions;
  positions.reserve(rig.vertices.size());
  for (const auto& vertex : rig.vertices)
    positions.push_back(add(morphPoint(doc,vertex.position),faceShapeDelta(rig,doc,vertex.source)));
  return positions;
}

inline double bodyScale(const Document& doc) {
  return 1 + doc.values[0] * morphs[0].amount;
}
} // namespace ftebroom::character
