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

  Document document() const {
    return family < 0 ? builtinPreset(index, "project-" + key)
                      : catalogPreset(size_t(family), index, "project-" + key);
  }
};

inline const std::vector<ProjectPreset>& projectPresets() {
  static const auto presets = [] {
    std::vector<ProjectPreset> result;
    for (unsigned index = 0; index < presetNames.size(); ++index)
      result.push_back({"builtin-" + std::to_string(index), presetNames[index], -1, index});
    for (unsigned family = 0; family < std::size(catalogRecipes); ++family)
      for (unsigned index = 0; index < std::size(catalogRecipes[family]); ++index)
        result.push_back({recipeId(family, index), catalogRecipes[family][index].name, int(family), index});
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

inline std::string projectPresetMaterial(int profile, const ProjectPreset& preset) {
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

inline void projectEncodePreset(ProjectPresetFiles& files, ProjectPresetFiles& lumps, int profile,
    const ProjectPreset& preset, const Evaluation& evaluation, const std::string& palette) {
  const auto material = projectPresetMaterial(profile, preset);
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

inline std::string projectShaderText(int profile) {
  std::string shaders;
  for (const auto& preset : projectPresets()) {
    const auto material = "textures/" + projectPresetMaterial(profile, preset);
    shaders += material + "\n{\n qer_editorimage " + material + ".tga\n";
    if (profile == 1) {
      shaders += " fte_program defaultwall\n diffusemap " + material + ".tga\n normalmap " + material
        + "_norm.tga\n fullbrightmap " + material + "_luma.tga\n {\n  map $diffuse\n }\n}\n";
    } else {
      // Alpha-tested surfaces still receive the BSP lightmap.
      shaders += " {\n  map " + material + ".tga\n  alphaFunc GE128\n  depthWrite\n }\n"
        " {\n  map $lightmap\n  blendFunc filter\n  depthFunc equal\n }\n}\n";
    }
  }
  return shaders;
}

} // namespace ftebroom::textureStudio
