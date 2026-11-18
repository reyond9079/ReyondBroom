// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "png.h"
#ifndef INCLUDE_STB_IMAGE_WRITE_H
#include "../third_party/stb_image_write.h"
#endif

namespace ftebroom::textureStudio {

inline std::string jpeg(const Image& image, unsigned quality) {
  require(quality >= 1 && quality <= 100, "jpeg", "Choose a quality between 1 and 100.");
  const auto width = image.size.width, height = image.size.height;
  require(width && height && width <= maxDimension && height <= maxDimension &&
    image.type == Port::color && image.pixels.size() == size_t(width) * height,
    "jpeg", "JPEG export requires a complete Base Color image.");
  require(std::all_of(image.pixels.begin(), image.pixels.end(), [](const Pixel& pixel) {
    return pixel[3] == 1.f;
  }), "jpeg", "JPEG requires opaque Base Color. Choose a lossless format for alpha.");
  std::string result;
  const auto encoded = rgba8(image);
  const auto written = stbi_write_jpg_to_func([](void* file, void* bytes, int count) {
    static_cast<std::string*>(file)->append(static_cast<const char*>(bytes), size_t(count));
  }, &result, int(width), int(height), 4, encoded.data(), int(quality));
  require(written, "jpeg", "Cannot encode the JPEG.");
  return result;
}

} // namespace ftebroom::textureStudio
