// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "assets.h"
#include "../ftebroom_pak.h"
#include <sstream>

namespace ftebroom::textureStudio {

constexpr uint64_t packageByteLimit = UINT64_C(1024) * 1024 * 1024;
constexpr unsigned packageEntryLimit = 65;
inline constexpr const char* packageSourceName = "source.rbtex.json";

inline std::string pack(const Document& document, const std::filesystem::path& sourceFolder) {
  require(!document.readOnly, "package", "This source requires a newer package writer.");
  std::map<std::string, std::string> objects{{packageSourceName, document.save()}};
  const auto& references = document.source.at("assetReferences");
  require(references.size() < packageEntryLimit, "package", "The source references too many images.");
  size_t bytes = objects.at(packageSourceName).size();
  for (const auto& reference : references) {
    const auto name = imageRelativePath(idField(reference, "sha256", "assets"));
    auto image = readOriginal(sourceFolder, reference);
    require(bytes + image.size() <= packageByteLimit - 65536, "package", "The texture package exceeds 1 GiB.");
    bytes += image.size();
    require(objects.emplace(name, std::move(image)).second, "package", "A source image is listed twice.");
  }
  std::map<std::string, std::string_view> entries;
  for (const auto& [name, item] : objects) entries.emplace(name, item);
  std::ostringstream archive(std::ios::binary | std::ios::out);
  ftebroom::pak::zipStore(archive, entries);
  return archive.str();
}

struct Package {
  Document document;
  Assets assets;
  std::map<std::string, std::string> originals;
};

inline Package unpack(std::istream& stream, uint64_t size) {
  require(size <= packageByteLimit, "package", "The texture package exceeds 1 GiB.");
  const auto entries = ftebroom::pak::zipDirectory(stream, size, packageEntryLimit);
  std::map<std::string, ftebroom::pak::ZipEntry> byName;
  std::vector<std::pair<uint64_t, uint64_t>> ranges;
  uint64_t total = 0;
  for (const auto& item : entries) {
    const bool source = item.name == packageSourceName;
    require(item.size > 0 && item.size <= (source ? maxSourceBytes : assetByteLimit) &&
      uint64_t(item.size) <= uint64_t(item.compressed) * 100,
      "package", "A package entry exceeds its size or compression budget.");
    const unsigned mode = (item.attributes >> 16) & 0170000;
    require((mode == 0 || mode == 0100000) && !(item.attributes & 0x410),
      "package", "The package contains a link or directory.");
    const auto begin = ftebroom::pak::zipData(stream, item, size);
    ranges.emplace_back(item.offset, begin + item.compressed);
    total += item.size;
    require(total <= packageByteLimit, "package", "The unpacked texture package exceeds 1 GiB.");
    require(byName.emplace(item.name, item).second, "package", "A package path occurs twice.");
  }
  std::sort(ranges.begin(), ranges.end());
  for (size_t i = 1; i < ranges.size(); ++i)
    require(ranges[i - 1].second <= ranges[i].first, "package", "The ZIP entries overlap.");
  auto source = byName.find(packageSourceName);
  require(source != byName.end(), "package", "The texture package does not contain its source.");
  auto doc = open(ftebroom::pak::zipRead(stream, size, source->second));
  require(!doc.readOnly, "package", "This package requires a newer version of Texture Studio.");
  byName.erase(source);
  Package result{std::move(doc), {}, {}};
  size_t budget = decodedAssetLimit;
  for (const auto& reference : result.document.source.at("assetReferences")) {
    const auto id = idField(reference, "id", "assets");
    const auto digest = idField(reference, "sha256", "assets");
    const auto name = imageRelativePath(digest);
    const auto& license = member(reference, "license", "assets");
    require(license.is_string() && licenseId(license.get_ref<const std::string&>()) &&
      member(reference, "path", "assets") == name && !result.assets.count(id), "assets",
      "A package image has an unsafe path, a duplicate ID or no license.");
    auto asset = byName.find(name);
    require(asset != byName.end(), "package", "A referenced image is missing from the package.");
    auto original = ftebroom::pak::zipRead(stream, size, asset->second);
    require(ftebroom::sha256(original) == digest, "package", "An original image failed its checksum.");
    auto raster = decodeOriginal(original, reference, budget);
    require(integer(member(reference, "width", "assets"), 1, maxDimension, "assets.width") == raster->size.width &&
      integer(member(reference, "height", "assets"), 1, maxDimension, "assets.height") == raster->size.height,
      "package", "An original image has the wrong dimensions.");
    budget -= raster->rgba.size();
    result.originals.emplace(name, std::move(original));
    result.assets.emplace(id, std::move(raster));
    byName.erase(asset);
  }
  require(byName.empty(), "package", "The package contains files not referenced by the source.");
  compile(result.document, {}, result.assets);
  return result;
}

inline Package unpack(const std::filesystem::path& file) {
  require(std::filesystem::is_regular_file(file), "package", "Choose an existing Texture Studio package.");
  const auto size = std::filesystem::file_size(file);
  std::ifstream stream(file, std::ios::binary);
  require(bool(stream), "package", "Cannot open the Texture Studio package.");
  return unpack(stream, size);
}

} // namespace ftebroom::textureStudio
