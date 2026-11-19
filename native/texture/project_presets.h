// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "catalog.h"
#include "legacy_codec.h"
#include "../ftebroom_sha256.h"

namespace ftebroom::textureStudio {

struct ProjectPreset {
  std::string key;
  const char* label;
  int family;
  unsigned index;
  std::string name;

  Document document() const {
    return family < 0 ? builtinPreset(index, "project-" + key)
                      : catalogPreset(size_t(family), index, "project-" + key);
  }
};

inline std::string projectMaterialWord(std::string label, std::string fallback = {}) {
  for (auto& c : label) {
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    else if (c < 'a' || c > 'z') c = ' ';
  }
  const auto surrounded = " " + label + " ";
  for (const char* kind : {"brick", "plaster", "concrete", "ceramic", "porcelain", "tile", "gravel", "basalt",
                          "rubber", "water", "foam", "lava", "oil", "metal", "steel", "iron", "wood",
                          "stone", "sand", "mud", "snow", "ice", "grass", "moss", "paint", "energy", "glass"}) {
    if (surrounded.find(" " + std::string(kind) + " ") != std::string::npos) return kind;
  }
  if (!fallback.empty()) return fallback;
  const auto start = label.find_first_not_of(' ');
  return start == std::string::npos ? "material" : label.substr(start, std::min(size_t(10), label.find(' ', start) - start));
}

inline const std::vector<ProjectPreset>& projectPresets() {
  static const auto presets = [] {
    std::vector<ProjectPreset> result;
    std::map<std::string, unsigned> counts;
    const auto name = [&](const char* label, const char* fallback) {
      const auto word = projectMaterialWord(label, fallback);
      return word + "_" + std::to_string(++counts[word]);
    };
    const char* builtinKinds[]{"brick", "ceramic", "hazard", "concrete", "gravel", "panel", "basalt",
      "rubber", "water", "foam", "lava", "oil", "energy", "display"};
    for (unsigned index = 0; index < presetNames.size(); ++index)
      result.push_back({"builtin-" + std::to_string(index), presetNames[index], -1, index, name(presetNames[index], builtinKinds[index])});
    for (unsigned family = 0; family < std::size(catalogRecipes); ++family)
      for (unsigned index = 0; index < std::size(catalogRecipes[family]); ++index) {
        const auto* label = catalogRecipes[family][index].name;
        result.push_back({recipeId(family, index), label, int(family), index, name(label, recipeCategoryIds[family])});
      }
    return result;
  }();
  return presets;
}

inline bool projectUsesWad(int profile) { return profile == 2 || profile == 4; }

inline std::string projectProfileId(int profile) {
  static const char* const ids[]{"q3", "fte", "q1", "q2", "hl"};
  require(profile >= 0 && profile < int(std::size(ids)), "project", "Unsupported project texture profile.");
  return ids[profile];
}

inline std::string projectMaterialPath(int profile, const std::string& name) {
  return projectUsesWad(profile) ? name : "reyond-studio/" + name;
}

inline bool projectNumberedName(std::string_view name) {
  const auto separator = name.find('_');
  return name.size() <= 15 && separator != name.npos && separator > 0 && separator + 1 < name.size() &&
    name.substr(0, separator).find_first_not_of("abcdefghijklmnopqrstuvwxyz") == name.npos &&
    name.substr(separator + 1).find_first_not_of("0123456789") == name.npos;
}

inline std::string projectPresetMaterial(int profile, const ProjectPreset& preset) {
  return projectMaterialPath(profile, preset.name);
}

inline std::string legacyProjectPresetMaterial(int profile, const ProjectPreset& preset) {
  const auto hash = ftebroom::sha256(projectProfileId(profile) + "/" + preset.key);
  return projectUsesWad(profile) ? "rbs_" + hash.substr(0, 11) : "reyond-studio/p" + hash.substr(0, 14);
}

inline std::string projectPresetWad(int profile) {
  return "texture-wads/reyond-studio-presets-" + projectProfileId(profile) + ".wad";
}

inline std::string projectPresetShaders(int profile) {
  return "reyond-studio-presets-" + projectProfileId(profile);
}

inline std::string projectPresetStorage(int profile) {
  return "texture-studio-presets/" + projectProfileId(profile) + "/";
}

inline std::string projectPresetImagePath(int profile, const std::string& material) {
  return "textures/" + material + (profile == 3 ? ".wal" : ".tga");
}

using ProjectPresetFiles = std::map<std::string, std::string>;

inline ProjectPresetFiles projectWadLumps(const std::string& archive, int profile) {
  const bool hl = profile == 4;
  require(archive.size() >= 12 && archive.compare(0, 4, hl ? "WAD3" : "WAD2") == 0,
    "project", "The preset WAD has the wrong format.");
  const auto count = quake1::word(archive, 4), directory = quake1::word(archive, 8);
  require(count <= 4096, "project", "The preset WAD directory is too large.");
  quake1::range(archive, directory, uint64_t(count) * 32);
  ProjectPresetFiles lumps;
  for (size_t i = 0; i < count; ++i) {
    const auto entry = directory + i * 32;
    const auto name = quake1::name(archive, entry + 16, 16);
    const auto offset = quake1::word(archive, entry), size = quake1::word(archive, entry + 4);
    quake1::range(archive, offset, size);
    require(size >= 40 && archive[entry + 12] == (hl ? 67 : 68) && !archive[entry + 13] &&
      name == quake1::name(archive, offset, 16) &&
      lumps.emplace(name, archive.substr(offset, size)).second,
      "project", "The preset WAD has an invalid or duplicate texture.");
  }
  return lumps;
}

inline void projectEncodeMaterial(ProjectPresetFiles& files, ProjectPresetFiles& lumps, int profile,
    const std::string& material, const Evaluation& evaluation, const std::string& palette) {
  const auto& base = *evaluation.outputs.at("baseColor");
  if (profile == 2 || profile == 3 || profile == 4) {
    auto opaque = base;
    for (auto& pixel : opaque.pixels) pixel[3] = 1;
    const auto format = profile == 2 ? IndexedFormat::wad2 : profile == 4 ? IndexedFormat::wad3 : IndexedFormat::wal;
    auto bytes = indexed(opaque, material, palette, format);
    if (profile == 3) files[projectPresetImagePath(profile, material)] = std::move(bytes);
    else {
      auto entry = projectWadLumps(bytes, profile);
      lumps[material] = std::move(entry.at(material));
    }
    return;
  }
  const auto texture = "textures/" + material;
  files[projectPresetImagePath(profile, material)] = tga(base, TgaOrigin::lowerLeft);
  if (profile == 1) {
    const auto bake = [&](const char* channel, Port type, Pixel value) {
      const auto found = evaluation.outputs.find(channel);
      if (found != evaluation.outputs.end()) return tga(*found->second);
      return tga(Image{base.size, type, std::vector<Pixel>(base.pixels.size(), value)});
    };
    files[texture + "_norm.tga"] = bake("normal", Port::normal, {0, 0, 1, 1});
    files[texture + "_luma.tga"] = bake("emissive", Port::color, {0, 0, 0, 1});
  }
}

inline void projectEncodePreset(ProjectPresetFiles& files, ProjectPresetFiles& lumps, int profile,
    const ProjectPreset& preset, const Evaluation& evaluation, const std::string& palette) {
  projectEncodeMaterial(files, lumps, profile, projectPresetMaterial(profile, preset), evaluation, palette);
}

inline std::string projectShaderEntry(int profile, const std::string& name) {
    const auto material = "textures/" + name;
    std::string shaders;
    shaders += material + "\n{\n qer_editorimage " + material + ".tga\n";
    if (profile == 1) {
      shaders += " fte_program defaultwall\n diffusemap " + material + ".tga\n normalmap " + material
        + "_norm.tga\n fullbrightmap " + material + "_luma.tga\n {\n  map $diffuse\n }\n}\n";
    } else {
      // Alpha-tested surfaces still receive the BSP lightmap.
      shaders += " {\n  map " + material + ".tga\n  alphaFunc GE128\n  depthWrite\n }\n"
        " {\n  map $lightmap\n  blendFunc filter\n  depthFunc equal\n }\n}\n";
    }
    return shaders;
}

inline std::string projectShaderText(int profile) {
  std::string shaders;
  for (const auto& preset : projectPresets()) shaders += projectShaderEntry(profile, projectPresetMaterial(profile, preset));
  return shaders;
}

} // namespace ftebroom::textureStudio
