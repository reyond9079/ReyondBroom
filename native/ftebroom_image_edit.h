// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_bb_image_ops.h"
#include "ftebroom_bb_io.h"
#include <deque>
#include <memory>

namespace ftebroom::imageEdit {
using bb::Texture;

inline Texture decode(const std::string& raster) {
  int width, height, channels;
  const auto data = reinterpret_cast<const stbi_uc*>(raster.data());
  if (!stbi_info_from_memory(data, int(raster.size()), &width, &height, &channels) ||
      width <= 0 || height <= 0 || size_t(width) * height > 16777216)
    throw std::runtime_error("Cannot read the image pixels.");
  std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
    stbi_load_from_memory(data, int(raster.size()), &width, &height, &channels, 4), stbi_image_free);
  if (!pixels) throw std::runtime_error("Cannot read the image pixels.");
  Texture image;
  image.width = width;
  image.height = height;
  image.rgba.assign(pixels.get(), pixels.get() + size_t(width) * height * 4);
  return image;
}

enum class Transform { Left, Right, FlipX, FlipY };
inline Texture transform(const Texture& input, Transform operation) {
  Texture image;
  const bool rotate = operation == Transform::Left || operation == Transform::Right;
  image.width = rotate ? input.height : input.width;
  image.height = rotate ? input.width : input.height;
  image.rgba.resize(input.rgba.size());
  for (int y = 0; y < image.height; ++y)
    for (int x = 0; x < image.width; ++x) {
      int sx = x, sy = y;
      switch (operation) {
      case Transform::Left: sx = input.width - 1 - y; sy = x; break;
      case Transform::Right: sx = y; sy = input.height - 1 - x; break;
      case Transform::FlipX: sx = input.width - 1 - x; break;
      case Transform::FlipY: sy = input.height - 1 - y; break;
      }
      std::copy_n(input.rgba.data() + (size_t(sy) * input.width + sx) * 4, 4,
                  image.rgba.data() + (size_t(y) * image.width + x) * 4);
    }
  return image;
}

inline Texture resizeNearest(const Texture& input, int width, int height) {
  if (width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
      size_t(width) * height > 16777216)
    throw std::runtime_error("Set positive dimensions with at most 16 million pixels.");
  Texture image;
  image.width = width;
  image.height = height;
  image.rgba.resize(size_t(width) * height * 4);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      std::copy_n(input.rgba.data() + (size_t(y * input.height / height) * input.width +
                  x * input.width / width) * 4, 4, image.rgba.data() + (size_t(y) * width + x) * 4);
  return image;
}

struct Document {
  struct State { Texture pixels; unsigned id = 0; };
  State current;
  std::deque<State> undo, redo;
  unsigned serial = 0, saved = 0;

  bool dirty() const { return current.id != saved; }
  void beginEdit() {
    redo.clear();
    size_t memory = current.pixels.rgba.size();
    for (const auto& state : undo) memory += state.pixels.rgba.size();
    while (!undo.empty() && (memory > 96 * 1024 * 1024 || undo.size() >= 24)) {
      memory -= undo.front().pixels.rgba.size();
      undo.pop_front();
    }
    undo.push_back(current);
    current.id = ++serial;
  }

  bool step(bool forward) {
    auto& from = forward ? redo : undo;
    auto& to = forward ? undo : redo;
    if (from.empty()) return false;
    to.push_back(std::move(current));
    current = std::move(from.back());
    from.pop_back();
    return true;
  }
};
} // namespace ftebroom::imageEdit
