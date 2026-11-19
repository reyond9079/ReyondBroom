// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "png.h"
#include "../ftebroom_compiler_wad.h"
#include <numeric>

namespace ftebroom::textureStudio {

enum class IndexedFormat { wad2, wad3, wal };

inline void little32(std::string& out, uint32_t n) {
  for (unsigned i = 0; i < 32; i += 8) out += char(n >> i);
}

inline bool powerOfTwo(unsigned n) { return n && !(n & (n - 1)); }

inline bool indexedName(std::string_view name, bool longer, bool masked) {
  const size_t max = longer ? 31 : 15;
  if (name.empty() || name.size() > max) return false;
  if (masked) {
    if (name.front() != '{') return false;
    name.remove_prefix(1);
  }
  if (name.empty() || name.front() == '/' || name.back() == '/') return false;
  bool previousSlash = false;
  for (char c : name) {
    const bool slash = c == '/' && longer;
    if (!(slash || c == '_' || c == '-' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
      return false;
    if (slash && previousSlash) return false;
    previousSlash = slash;
  }
  for (size_t begin = 0; begin < name.size();) {
    const auto separator = name.find('/', begin);
    const auto segment = name.substr(begin, separator == name.npos ? name.size() - begin : separator - begin);
    const bool device = segment == "con" || segment == "prn" || segment == "aux" || segment == "nul"
      || (segment.size() == 4 && (segment.substr(0, 3) == "com" || segment.substr(0, 3) == "lpt")
          && segment[3] >= '0' && segment[3] <= '9');
    if (device) return false;
    if (separator == name.npos) break;
    begin = separator + 1;
  }
  return true;
}

inline std::vector<Pixel> half(const std::vector<Pixel>& input, unsigned width, unsigned height) {
  std::vector<Pixel> pixels;
  pixels.reserve(size_t(width / 2) * (height / 2));
  for (unsigned y = 0; y < height; y += 2) {
    for (unsigned x = 0; x < width; x += 2) {
      Pixel sum{};
      for (unsigned dy = 0; dy < 2; ++dy) {
        for (unsigned dx = 0; dx < 2; ++dx) {
          const auto& p = input[size_t(y + dy) * width + x + dx];
          for (size_t c = 0; c < 3; ++c) sum[c] += p[c] * p[3];
          sum[3] += p[3];
        }
      }
      if (sum[3] > 0) for (size_t c = 0; c < 3; ++c) sum[c] /= sum[3];
      sum[3] /= 4.f;
      pixels.push_back(sum);
    }
  }
  return pixels;
}

inline std::array<std::string, 4> indexedMips(const Image& image, std::string_view palette,
                                            unsigned paletteCount, bool masked) {
  require(palette.size() == 768 && paletteCount > 0 && paletteCount <= 256, "palette",
    "Supply the palette from the selected game profile.");
  std::array<Pixel, 256> linear{};
  for (unsigned i = 0; i < paletteCount; ++i)
    for (unsigned c = 0; c < 3; ++c)
      linear[i][c] = fromSrgb(static_cast<unsigned char>(palette[i * 3 + c]) / 255.f);
  const auto quantize = [&](const Pixel& p) {
    float nearest = std::numeric_limits<float>::max();
    unsigned found = 0;
    for (unsigned i = 0; i < paletteCount; ++i) {
      const float r = p[0] - linear[i][0], g = p[1] - linear[i][1], b = p[2] - linear[i][2];
      const float d = r * r + g * g + b * b;
      if (d < nearest) {
        nearest = d;
        found = i;
      }
    }
    return char(found);
  };

  const size_t covered = std::count_if(image.pixels.begin(), image.pixels.end(),
    [](const auto& p) { return p[3] >= .5f; });
  std::array<std::string, 4> levels;
  auto pixels = image.pixels;
  unsigned width = image.size.width, height = image.size.height;
  for (unsigned mip = 0; mip < levels.size(); ++mip) {
    auto& result = levels[mip];
    result.resize(pixels.size());
    std::vector<size_t> opaque;
    if (masked) {
      result.assign(pixels.size(), char(255));
      opaque.resize(pixels.size());
      std::iota(opaque.begin(), opaque.end(), size_t(0));
      const size_t quota = (covered * pixels.size() + image.pixels.size() / 2) / image.pixels.size();
      std::partial_sort(opaque.begin(), opaque.begin() + quota, opaque.end(),
        [&](size_t a, size_t b) { return pixels[a][3] != pixels[b][3] ? pixels[a][3] > pixels[b][3] : a < b; });
      opaque.resize(quota);
      for (size_t i : opaque) result[i] = quantize(pixels[i]);
    } else {
      for (size_t i = 0; i < pixels.size(); ++i) result[i] = quantize(pixels[i]);
    }
    if (mip + 1 < levels.size()) {
      pixels = half(pixels, width, height);
      width /= 2;
      height /= 2;
    }
  }
  return levels;
}

inline std::string indexed(const Image& image, std::string_view name, std::string palette,
                           IndexedFormat format, bool masked = false, std::string_view next = {}) {
  const unsigned w = image.size.width, h = image.size.height;
  const bool hl = format == IndexedFormat::wad3, wal = format == IndexedFormat::wal;
  require(image.type == Port::color && image.pixels.size() == size_t(w) * h,
    "export", "Indexed textures require a complete Base Color image.");
  require((hl ? w >= 16 && h >= 16 : w >= 32 && h >= 32 && powerOfTwo(w) && powerOfTwo(h)) &&
    w <= 512 && h <= 512 && w % 16 == 0 && h % 16 == 0,
    "resolution", "Choose a profile-compatible indexed size between 16 and 512.");
  const bool animation = !wal && !masked && name.size() > 2 && name.size() <= 15 && name.front() == '+' &&
    name[1] >= '0' && name[1] <= '9' && indexedName(name.substr(2), false, false);
  require((!masked || hl) && (animation || indexedName(name, wal, masked)), "name",
    "Invalid profile texture name. Use lowercase ASCII and the profile's length limit.");
  require(next.empty() || (wal && indexedName(next, true, false)), "name", "The next WAL frame name is invalid.");
  require(palette.size() == 768, "palette", "Supply a full 256-color game palette.");
  if (!masked) require(std::all_of(image.pixels.begin(), image.pixels.end(),
    [](const Pixel& p) { return p[3] == 1; }), "alpha",
    "This texture export is opaque. Choose an explicit alpha fallback.");
  if (masked) palette.replace(765, 3, "\0\0\xff", 3);

  // Quake II's OpenGL renderer reserves index 255 for transparent texels.
  const auto levels = indexedMips(image, palette, format == IndexedFormat::wad2 ? 224 : (masked || wal ? 255 : 256), masked);
  std::string lump(name);
  lump.resize(wal ? 32 : 16, '\0');
  little32(lump,w); little32(lump,h);
  uint32_t offset = wal ? 100 : 40;
  for (const auto& level : levels) {
    little32(lump, offset);
    offset += uint32_t(level.size());
  }
  if (wal) {
    lump.append(next);
    lump.resize(100, '\0');
  }
  for (const auto& level : levels) lump += level;
  if (wal) return lump;
  if (hl) {
    lump.append("\0\1", 2);
    lump += palette;
  }
  while (lump.size() % 4) lump += '\0';
  std::string archive(hl ? "WAD3" : "WAD2", 4);
  little32(archive, 1);
  little32(archive, 12 + uint32_t(lump.size()));
  archive += lump;
  little32(archive, 12);
  little32(archive, uint32_t(lump.size()));
  little32(archive, uint32_t(lump.size()));
  archive += hl ? '\x43' : '\x44';
  archive.append(3, '\0');
  archive.append(name);
  archive.append(16 - name.size(), '\0');
  return archive;
}

inline std::string combineAnimationWads(const std::vector<std::string>& frames, IndexedFormat format) {
  require(format != IndexedFormat::wal && frames.size() >= 2 && frames.size() <= 10,
    "animation", "A WAD animation needs 2 to 10 frames.");
  const std::string_view signature = format == IndexedFormat::wad3 ? "WAD3" : "WAD2";
  std::map<std::string, std::string> lumps;
  for (const auto& frame : frames) {
    require(frame.size() >= 84 && frame.compare(0, 4, signature) == 0,
      "animation", "Cannot combine incompatible indexed frames.");
    const auto name = quake1::name(frame, 12, 16);
    require(lumps.emplace(name, frame.substr(12, frame.size() - 12 - 32)).second,
      "animation", "The indexed animation contains duplicate frames.");
  }
  return quake1::mipWad(lumps, format == IndexedFormat::wad3);
}

enum class TgaOrigin { upperLeft, lowerLeft };

inline std::string tga(const Image& image, TgaOrigin origin = TgaOrigin::upperLeft) {
  const unsigned width = image.size.width, height = image.size.height;
  require(width > 0 && width <= maxDimension && height > 0 && height <= maxDimension &&
    image.pixels.size() == size_t(width) * height, "tga", "Invalid image dimensions.");
  const auto pixels = rgba8(image);
  std::string bytes(18, '\0');
  bytes[2] = 2;
  bytes[12] = char(width); bytes[13] = char(width >> 8);
  bytes[14] = char(height); bytes[15] = char(height >> 8);
  const bool lowerLeft = origin == TgaOrigin::lowerLeft;
  bytes[16] = 32; bytes[17] = lowerLeft ? 0x08 : 0x28;
  bytes.reserve(18 + pixels.size());
  const size_t stride = size_t(width) * 4;
  for (unsigned row = 0; row < height; ++row) {
    const size_t start = (lowerLeft ? height - 1 - row : row) * stride;
    for (size_t i = start; i < start + stride; i += 4) {
      bytes += char(pixels[i + 2]); bytes += char(pixels[i + 1]);
      bytes += char(pixels[i]); bytes += char(pixels[i + 3]);
    }
  }
  return bytes;
}

inline std::map<std::string, std::string> fteWallMaterial(const Evaluation& images, const std::string& name) {
  require(indexedName(name, true, false), "name", "Choose a portable texture name.");
  const auto base = images.outputs.find("baseColor");
  const auto normal = images.outputs.find("normal");
  const auto emissive = images.outputs.find("emissive");
  require(base != images.outputs.end() && normal != images.outputs.end() && emissive != images.outputs.end(),
    "profile", "The FTE material requires Base Color, Normal and Emissive outputs.");
  const auto material = "textures/" + name;
  const auto diffusePath = material + ".tga";
  const auto normalPath = material + "_norm.tga";
  const auto emissionPath = material + "_luma.tga";
  return {{diffusePath, tga(*base->second)}, {normalPath, tga(*normal->second)}, {emissionPath, tga(*emissive->second)},
    {"scripts/reyond-texture.shader", material + "\n{\n  fte_program defaultwall\n  diffusemap " + diffusePath +
      "\n  normalmap " + normalPath + "\n  fullbrightmap " + emissionPath + "\n  {\n    map $diffuse\n  }\n}\n"}};
}

} // namespace ftebroom::textureStudio
