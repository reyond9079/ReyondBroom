// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "graph.h"
#include "../ftebroom_sha256.h"
#include "../ftebroom_legacy_images.h"
#ifndef STBI_INCLUDE_STB_IMAGE_H
#include "../third_party/stb_image.h"
#endif

#include <filesystem>
#include <fstream>

namespace ftebroom::textureStudio {

constexpr size_t assetByteLimit = 64 * 1024 * 1024;
constexpr size_t decodedAssetLimit = 256 * 1024 * 1024;

inline bool licenseId(std::string_view id) {
  return !id.empty() && id.size() <= 128 && id.find_first_not_of(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.:+") == id.npos;
}

inline std::string imageFileBytes(const std::filesystem::path& file) {
  require(std::filesystem::is_regular_file(file) && std::filesystem::file_size(file) <= assetByteLimit,
    "assets", "The original image is missing or exceeds 64 MiB.");
  std::ifstream in(file, std::ios::binary);
  std::string result;
  std::array<char, 32768> block{};
  while (in.read(block.data(), block.size()) || in.gcount()) {
    require(size_t(in.gcount()) <= assetByteLimit - result.size(), "assets", "The original image changed during reading.");
    result.append(block.data(), in.gcount());
  }
  require(in.eof(), "assets", "Cannot read the original image.");
  return result;
}

inline void checkTgaPayload(std::string_view bytes, int width, int height) {
  const auto byte = [&](size_t offset) { return static_cast<unsigned char>(bytes[offset]); };
  size_t offset = 18 + byte(0);
  const size_t pixelBytes = (byte(16) + 7) / 8;
  const size_t pixels = size_t(width) * height;
  const auto intact = [&](bool ok) { require(ok, "assets", "The TGA image has damaged pixel data."); };
  intact(offset <= bytes.size());
  if (byte(2) < 10) {
    intact(pixels * pixelBytes <= bytes.size() - offset);
    return;
  }
  size_t remaining = pixels;
  while (remaining) {
    intact(offset < bytes.size());
    const unsigned packet = byte(offset++);
    const size_t count = (packet & 0x7f) + 1;
    const size_t payload = (packet & 0x80 ? 1 : count) * pixelBytes;
    intact(count <= remaining && payload <= bytes.size() - offset);
    offset += payload;
    remaining -= count;
  }
}

inline std::shared_ptr<Raster> decodeRaster(std::string_view bytes, size_t budget = decodedAssetLimit) {
  require(!bytes.empty() && bytes.size() <= assetByteLimit, "assets", "The image is empty or exceeds 64 MiB.");
  const bool png = bytes.substr(0, 8) == "\x89PNG\r\n\x1a\n";
  const bool jpeg = bytes.substr(0, 2) == "\xff\xd8";
  const bool tga = bytes.size() >= 18 && bytes[1] == 0 &&
    (bytes[2] == 2 || bytes[2] == 3 || bytes[2] == 10 || bytes[2] == 11);
  require(png || jpeg || tga, "assets", "Choose a PNG, JPEG or TGA image.");
  const auto* raw = reinterpret_cast<const stbi_uc*>(bytes.data());
  const auto length = int(bytes.size());
  int width = 0, height = 0, format = 0;
  require(stbi_info_from_memory(raw, length, &width, &height, &format) && width > 0 && height > 0 &&
    width <= int(maxDimension) && height <= int(maxDimension), "assets", "Use a supported raster up to 8192 × 8192.");
  const auto needed = size_t(width) * height * 4;
  require(needed <= std::min(budget, decodedAssetLimit), "assets", "Decoded images exceed their memory budget.");
  if (tga) checkTgaPayload(bytes, width, height);
  const Resolution expected{unsigned(width), unsigned(height)};
  std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
    stbi_load_from_memory(raw, length, &width, &height, &format, 4), stbi_image_free);
  require(pixels && unsigned(width) == expected.width && unsigned(height) == expected.height,
    "assets", "The image is damaged or its dimensions changed while decoding.");
  auto result = std::make_shared<Raster>();
  result->size = expected;
  result->rgba.assign(pixels.get(), pixels.get() + needed);
  return result;
}

inline std::string writePalette(std::string_view bytes) {
  require(bytes.size() == 768, "assets.palette", "Provide the 768-byte palette from the source game.");
  static constexpr char digits[] = "0123456789abcdef";
  std::string encoded;
  encoded.reserve(bytes.size() * 2);
  for (unsigned char value : bytes) {
    encoded.push_back(digits[value >> 4]);
    encoded.push_back(digits[value & 15]);
  }
  return encoded;
}

inline std::string readPalette(const Json& reference) {
  const auto& encoded = member(reference, "palette", "assets");
  require(encoded.is_string() && encoded.get_ref<const std::string&>().size() == 1536, "assets.palette",
    "Provide the 768-byte palette from the source game.");
  const auto digit = [](char c) -> unsigned {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    throw Error("assets.palette", "The imported palette contains a non-hexadecimal value.");
  };
  const auto& value = encoded.get_ref<const std::string&>();
  std::string result(768, '\0');
  for (size_t i = 0; i < result.size(); ++i) result[i] = char(digit(value[i * 2]) * 16 + digit(value[i * 2 + 1]));
  return result;
}

inline std::shared_ptr<Raster> decodeOriginal(const std::string& bytes, const Json& reference,
                                              size_t budget = decodedAssetLimit) {
  const auto encoding = reference.value("encoding", "image");
  if (encoding == "image") return decodeRaster(bytes, budget);
  require(!bytes.empty() && bytes.size() <= assetByteLimit &&
    (encoding == "wal" || encoding == "wad2" || encoding == "wad3"),
    "assets", "The indexed source has an invalid type or size.");
  quake1::Texture texture;
  if (encoding == "wal") {
    texture = legacy::wal(bytes, readPalette(reference));
  } else {
    require(bytes.compare(0, 4, encoding == "wad2" ? "WAD2" : "WAD3") == 0, "assets", "The original WAD type changed.");
    auto all = quake1::wad(bytes);
    const auto entry = reference.value("texture", "");
    auto chosen = all.end();
    if (entry.empty()) {
      require(all.size() == 1, "assets.texture", "Choose a texture by name when importing a WAD containing multiple images.");
      chosen = all.begin();
    } else {
      for (auto it = all.begin(); it != all.end(); ++it) {
        if (it->name != entry) continue;
        require(chosen == all.end(), "assets.texture", "The original WAD contains duplicate texture names.");
        chosen = it;
      }
      require(chosen != all.end(), "assets.texture", "The specified texture was not found in the original WAD.");
    }
    texture = std::move(*chosen);
    if (encoding == "wad2") texture.palette = readPalette(reference);
  }
  require(texture.palette.size() == 768 && texture.pixels.size() <= std::min(budget, decodedAssetLimit) / 4,
    "assets", "The indexed image exceeds its memory budget or has an invalid palette.");
  auto raster = std::make_shared<Raster>();
  raster->size = {texture.width, texture.height};
  raster->rgba.resize(texture.pixels.size() * 4);
  const bool masked = encoding == "wal" || (encoding == "wad3" && texture.name.front() == '{');
  for (size_t i = 0; i < texture.pixels.size(); ++i) {
    const auto index = texture.pixels[i];
    for (size_t c = 0; c < 3; ++c) raster->rgba[i * 4 + c] = texture.palette[index * 3 + c];
    raster->rgba[i * 4 + 3] = masked && index == 255 ? 0 : 255;
  }
  return raster;
}

inline std::string sourceEncoding(std::string_view bytes, const std::filesystem::path& file) {
  if (bytes.substr(0, 4) == "WAD2") return "wad2";
  if (bytes.substr(0, 4) == "WAD3") return "wad3";
  auto extension = file.extension().u8string();
  for (char& c : extension) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  return extension == ".wal" ? "wal" : "image";
}

inline std::string imageRelativePath(std::string_view checksum) {
  require(checksum.size() == 64 && checksum.find_first_not_of("0123456789abcdef") == checksum.npos,
    "assets", "Expected the SHA-256 of the original image.");
  return "assets/" + std::string(checksum) + ".image";
}

inline std::string readOriginal(const std::filesystem::path& sourceFolder, const Json& reference) {
  const auto digest = idField(reference, "sha256", "assets");
  const auto name = imageRelativePath(digest);
  require(member(reference, "path", "assets") == name, "assets", "The original image path is unsafe.");
  const auto base = std::filesystem::canonical(sourceFolder.empty() ? "." : sourceFolder);
  const auto file = base / std::filesystem::u8path(name);
  require(std::filesystem::canonical(file) == file, "assets", "The original image path points through a link.");
  auto bytes = imageFileBytes(file);
  require(ftebroom::sha256(bytes) == digest, "assets", "The original image checksum differs from the saved source.");
  return bytes;
}

inline Assets loadAssets(const Document& document, const std::filesystem::path& sourceFolder) {
  Assets assets;
  const auto& references = document.source.at("assetReferences");
  if (references.empty()) return assets;
  require(references.size() <= 64, "assets", "The source references too many images.");
  size_t budget = decodedAssetLimit;
  std::set<std::string> digests;
  for (const auto& reference : references) {
    const auto id = idField(reference, "id", "assets");
    const auto digest = idField(reference, "sha256", "assets");
    const auto& license = member(reference, "license", "assets");
    require(license.is_string() && licenseId(license.get_ref<const std::string&>()),
      "assets", "The original image requires a license identifier.");
    require(digests.insert(digest).second && !assets.count(id), "assets", "The image reference is duplicated.");
    auto bytes = readOriginal(sourceFolder, reference);
    auto raster = decodeOriginal(bytes, reference, budget);
    const auto& width = member(reference, "width", "assets");
    const auto& height = member(reference, "height", "assets");
    require(integer(width, 1, maxDimension, "assets.width") == raster->size.width &&
      integer(height, 1, maxDimension, "assets.height") == raster->size.height,
      "assets", "The original image dimensions differ from the saved source.");
    budget -= raster->rgba.size();
    assets.emplace(id, std::move(raster));
  }
  return assets;
}

} // namespace ftebroom::textureStudio
