// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/texture/legacy_codec.h"
#include "../native/texture/presets.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../native/texture/package.h"
#undef STB_IMAGE_IMPLEMENTATION

#include <chrono>
#include <iostream>
#include <random>

namespace ts = ftebroom::textureStudio;

int main(int argc, char** argv) try {
  if (argc != 3) return 2;
  const std::string mode = argv[1];
  if (mode != "graph" && mode != "image" && mode != "zip") return 2;
  const long seconds = std::stol(argv[2]);
  if (seconds < 1 || seconds > 7200) return 2;

  const auto brick = ts::runningBrick("mutation-fuzz");
  const auto image = ts::evaluate(ts::compile(brick), {32,32}).outputs.at("baseColor");
  std::string palette;
  for (unsigned index = 0; index < 256; ++index) palette.append(3, char(index));
  std::vector<std::string> seedFiles;
  std::vector<ts::Json> references;
  if (mode == "graph") {
    seedFiles.push_back(brick.save());
    auto grouped = brick;
    ts::groupNodes(grouped, {"wear", "bond", "clay"}, "fuzz-masonry", "Grouped masonry");
    seedFiles.push_back(grouped.save());
  }
  else if (mode == "zip") seedFiles.push_back(ts::pack(brick, {}));
  else {
    seedFiles = {ts::png(*image), ts::tga(*image), ts::tga(*image, ts::TgaOrigin::lowerLeft)};
    references.resize(seedFiles.size(), ts::Json::object());
    for (const auto& [format, encoding] : std::vector<std::pair<ts::IndexedFormat, std::string>>{
      {ts::IndexedFormat::wad2, "wad2"}, {ts::IndexedFormat::wad3, "wad3"}, {ts::IndexedFormat::wal, "wal"}}) {
      seedFiles.push_back(ts::indexed(*image, "probe", palette, format));
      references.push_back({{"encoding", encoding}, {"palette", ts::writePalette(palette)}});
    }
  }
  std::mt19937 random(24058);
  size_t attempts = 0, accepted = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  do {
    const auto input = random() % seedFiles.size();
    auto bytes = seedFiles[input];
    const auto changes = random() % 7;
    for (unsigned change = 0; change < changes && !bytes.empty(); ++change) {
      const auto offset = random() % bytes.size();
      switch (random() % 4) {
      case 0: bytes.resize(offset); break;
      case 1: bytes.erase(offset, std::min<size_t>(16, bytes.size() - offset)); break;
      case 2: bytes.insert(offset, 1, char(random() & 255)); break;
      case 3: bytes[offset] ^= char(1 << (random() & 7)); break;
      }
    }
    ++attempts;
    try {
      if (mode == "graph") {
        const auto parsed = ts::open(bytes);
        if (!parsed.readOnly) ts::compile(parsed);
      } else if (mode == "image") {
        ts::decodeOriginal(bytes, references[input], 4 * 1024 * 1024);
      } else {
        std::istringstream stream(bytes, std::ios::binary | std::ios::in);
        ts::unpack(stream, bytes.size());
      }
      ++accepted;
    } catch (const std::exception&) {
      // Malformed sources are expected to fail validation.
    }
  } while (std::chrono::steady_clock::now() < deadline);
  std::cout << ts::Json{{"mode", mode}, {"durationSeconds", seconds}, {"seed", 24058},
    {"iterations", attempts}, {"accepted", accepted}}.dump() << '\n';
  return 0;
} catch (const std::exception& e) {
  std::cerr << e.what() << '\n';
  return 1;
}
