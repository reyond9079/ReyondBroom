// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "evaluate.h"
#include <string_view>
#include <zlib.h>

namespace ftebroom::textureStudio {

inline float encodedComponent(const Pixel& p, Port type, unsigned c) {
  if (c == 3) return unit(p[3]);
  if (type == Port::scalar) return unit(p[0]);
  if (type == Port::normal) return unit(p[c] * .5f + .5f);
  return type == Port::color ? toSrgb(p[c]) : unit(p[c]);
}

inline std::vector<unsigned char> rgba8(const Image& image) {
  std::vector<unsigned char> bytes;
  bytes.reserve(image.pixels.size() * 4);
  for (const auto& p : image.pixels)
    for (unsigned c = 0; c < 4; ++c)
      bytes.push_back(static_cast<unsigned char>(encodedComponent(p, image.type, c) * 255.f + .5f));
  return bytes;
}

inline void putBig32(std::string& bytes, uint32_t n) {
  for (int i = 24; i >= 0; i -= 8) bytes += char(n >> i);
}

inline void pngChunk(std::string& file, const char tag[5], std::string_view data) {
  putBig32(file, uint32_t(data.size()));
  auto crc = crc32(0, reinterpret_cast<const Bytef*>(tag), 4);
  if (!data.empty()) crc = crc32(crc, reinterpret_cast<const Bytef*>(data.data()), uInt(data.size()));
  file.append(tag, 4);
  file.append(data);
  putBig32(file, uint32_t(crc));
}

inline std::string deflateStored(std::string_view data) {
  std::string result("\x78\x01", 2);
  do {
    unsigned length = unsigned(std::min<size_t>(65535, data.size()));
    result += length == data.size() ? char(1) : char(0);
    result += char(length);
    result += char(length >> 8);
    result += char(~length);
    result += char(~length >> 8);
    result.append(data.substr(0, length));
    if (length == data.size()) break;
    data.remove_prefix(length);
  } while (!data.empty());
  return result;
}

inline std::string png(const Image& image, unsigned bits = 8) {
  require(bits == 8 || bits == 16, "png", "Choose 8-bit or 16-bit channels.");
  const auto size = image.size;
  require(size.width > 0 && size.height > 0 && size.width <= maxDimension &&
    size.height <= maxDimension && image.pixels.size() == size_t(size.width) * size.height,
    "png", "Image dimensions or pixel data are invalid.");
  const unsigned count = image.type == Port::scalar ? 1 : image.type == Port::normal ? 3 : 4;
  const size_t rowBytes = size_t(size.width) * count * (bits / 8) + 1;
  require(rowBytes <= (memoryBudget * 3 / 8) / size.height, "png", "Encoding exceeds its RAM budget.");
  std::string data;
  data.reserve(rowBytes * size.height);
  for (unsigned y = 0; y < size.height; ++y) {
    data += char(0);
    for (unsigned x = 0; x < size.width; ++x) {
      const auto& pixel = image.pixels[size_t(y) * size.width + x];
      for (unsigned c = 0; c < count; ++c) {
        const auto value = encodedComponent(pixel, image.type, c);
        if (bits == 16) {
          const auto word = unsigned(value * 65535.f + .5f);
          data += char(word >> 8); data += char(word);
        } else data += char(unsigned(value * 255.f + .5f));
      }
    }
  }
  std::string compressed = deflateStored(data);
  putBig32(compressed, uint32_t(adler32(1, reinterpret_cast<const Bytef*>(data.data()), uInt(data.size()))));
  std::string file("\x89PNG\r\n\x1a\n", 8);
  std::string header;
  putBig32(header, size.width);
  putBig32(header, size.height);
  header += char(bits);
  header += count == 1 ? char(0) : count == 3 ? char(2) : char(6);
  header.append(3, '\0');
  pngChunk(file, "IHDR", header);
  if (image.type == Port::color) pngChunk(file, "sRGB", std::string(1, '\0'));
  pngChunk(file, "IDAT", compressed);
  pngChunk(file, "IEND", {});
  return file;
}

} // namespace ftebroom::textureStudio
