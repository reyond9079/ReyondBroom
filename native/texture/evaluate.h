// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "graph.h"
#include <atomic>
#include <future>
#include <list>
#include <memory>
#include "../ftebroom_sha256.h"

namespace ftebroom::textureStudio {

class Pcg32 {
  uint64_t state = 0;
  uint64_t increment;

public:
  Pcg32(uint64_t seed, uint64_t sequence) : increment((sequence << 1) | 1) {
    next();
    state += seed;
    next();
  }
  uint32_t next() {
    const uint64_t old = state;
    state = old * UINT64_C(6364136223846793005) + increment;
    const auto mixed = uint32_t(((old >> 18) ^ old) >> 27);
    const auto rotation = unsigned(old >> 59);
    return (mixed >> rotation) | (mixed << ((0u - rotation) & 31u));
  }
};

inline uint64_t stableSeed(uint32_t seed, std::string_view id, uint32_t salt = 0) {
  uint64_t value = UINT64_C(14695981039346656037);
  const auto add = [&](unsigned char c) {
    value = (value ^ c) * UINT64_C(1099511628211);
  };
  for (unsigned i = 0; i < 32; i += 8) add(static_cast<unsigned char>(seed >> i));
  for (unsigned char c : std::string_view(generatorVersion)) add(c);
  add(0);
  for (unsigned char c : id) add(c);
  add(0);
  for (unsigned i = 0; i < 32; i += 8) add(static_cast<unsigned char>(salt >> i));
  return value;
}

inline float unit(float n) { return std::clamp(n, 0.f, 1.f); }
inline float frac(float n) { return n - std::floor(n); }

inline float fromSrgb(float v) {
  v = unit(v);
  return v <= .04045f ? v / 12.92f : std::pow((v + .055f) / 1.055f, 2.4f);
}

inline float toSrgb(float v) {
  v = unit(v);
  return v <= .0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.f / 2.4f) - .055f;
}

inline float mix(float a, float b, float t) {
  const float delta = b - a;
  const float amount = delta * t;
  return a + amount;
}

inline Pixel lerpColor(const Pixel& a, const Pixel& b, float t) {
  const float alpha = mix(a[3], b[3], t);
  if (alpha <= 0) return {};
  Pixel out{0, 0, 0, alpha};
  for (size_t i = 0; i < 3; ++i)
    out[i] = unit(mix(a[i] * a[3], b[i] * b[3], t) / alpha);
  return out;
}

inline Pixel scalar(float v) { return {unit(v), 0, 0, 1}; }

inline int wrap(int p, int n) {
  const int result = p % n;
  return result >= 0 ? result : result + n;
}

inline float lattice(uint64_t seed, unsigned x, unsigned y) {
  Pcg32 random(seed, (uint64_t(x) << 32) | y);
  return float(random.next() >> 8) / 16777216.f;
}

inline float valueNoise(uint64_t seed, int width, int height, float u, float v) {
  const float x = frac(u) * width;
  const float y = frac(v) * height;
  const int ix = int(x), iy = int(y);
  const auto smooth = [](float p) { return (p * p) * (3.f - 2.f * p); };
  const float tx = smooth(frac(x)), ty = smooth(frac(y));
  const auto a = lattice(seed, wrap(ix, width), wrap(iy, height));
  const auto b = lattice(seed, wrap(ix + 1, width), wrap(iy, height));
  const auto c = lattice(seed, wrap(ix, width), wrap(iy + 1, height));
  const auto d = lattice(seed, wrap(ix + 1, width), wrap(iy + 1, height));
  return mix(mix(a, b, tx), mix(c, d, tx), ty);
}

inline float valueNoise(uint64_t seed, int period, float u, float v) {
  return valueNoise(seed, period, period, u, v);
}

inline float gradientNoise(uint64_t seed, int period, float u, float v) {
  const float x = frac(u) * period, y = frac(v) * period;
  const int ix = int(x), iy = int(y);
  const float a = frac(x), b = frac(y);
  const auto smooth = [](float t) { return t * t * t * ((6.f * t - 15.f) * t + 10.f); };
  static constexpr std::array<std::array<float, 2>, 8> directions{{
    {1,0}, {-1,0}, {0,1}, {0,-1}, {.70710678f,.70710678f}, {-.70710678f,.70710678f},
    {.70710678f,-.70710678f}, {-.70710678f,-.70710678f}
  }};
  const auto dot = [&](int dx, int dy) {
    const auto& g = directions[unsigned(lattice(seed, wrap(ix + dx, period), wrap(iy + dy, period)) * 8)];
    return g[0] * (a - dx) + g[1] * (b - dy);
  };
  return unit(.5f + .5f * mix(mix(dot(0,0), dot(1,0), smooth(a)), mix(dot(0,1), dot(1,1), smooth(a)), smooth(b)));
}

inline std::array<float, 2> cellularDistances(uint64_t seed, int period, float jitter, float u, float v) {
  const float x = frac(u) * period, y = frac(v) * period;
  const int ix = int(x), iy = int(y);
  float nearest = 8.f, second = 8.f;
  for (int dy = -2; dy <= 2; ++dy) {
    for (int dx = -2; dx <= 2; ++dx) {
      const int i = ix + dx, j = iy + dy;
      const auto a = lattice(seed, wrap(i, period), wrap(j, period));
      const auto b = lattice(seed ^ UINT64_C(0x49f7af0b31b3c2e5), wrap(i, period), wrap(j, period));
      const float px = float(i) + mix(.5f, a, jitter) - x, py = float(j) + mix(.5f, b, jitter) - y;
      const float distance = px * px + py * py;
      if (distance < nearest) {
        second = nearest;
        nearest = distance;
      } else second = std::min(second, distance);
    }
  }
  return {std::sqrt(nearest), std::sqrt(second)};
}

inline float cellularDistance(uint64_t seed, int period, float jitter, float u, float v) {
  return unit(cellularDistances(seed, period, jitter, u, v)[0]);
}

inline Pixel rasterPixel(const Raster& image, int x, int y) {
  const size_t offset = (size_t(wrap(y, int(image.size.height))) * image.size.width
    + size_t(wrap(x, int(image.size.width)))) * 4;
  return {fromSrgb(image.rgba[offset] / 255.f), fromSrgb(image.rgba[offset + 1] / 255.f),
    fromSrgb(image.rgba[offset + 2] / 255.f), image.rgba[offset + 3] / 255.f};
}

inline Pixel resample(const Raster& image, int x, int y, Resolution out, float u, float v) {
  if (image.size.width == out.width && image.size.height == out.height) return rasterPixel(image, x, y);
  const float sx = u * image.size.width - .5f, sy = v * image.size.height - .5f;
  const int ix = int(std::floor(sx)), iy = int(std::floor(sy));
  return lerpColor(lerpColor(rasterPixel(image, ix, iy), rasterPixel(image, ix + 1, iy), frac(sx)),
    lerpColor(rasterPixel(image, ix, iy + 1), rasterPixel(image, ix + 1, iy + 1), frac(sx)), frac(sy));
}

struct Image {
  Resolution size;
  Port type;
  std::vector<Pixel> pixels;
  int tileX = 0, tileY = 0;
  unsigned tileWidth = 0;

  const Pixel& sample(int x, int y) const {
    if (tileWidth) {
      const auto localX = unsigned(x - tileX), localY = unsigned(y - tileY);
      const auto index = size_t(localY) * tileWidth + localX;
      if (localX >= tileWidth || index >= pixels.size()) throw Error("tiles", "A filter exceeded its tile halo.");
      return pixels[index];
    }
    return pixels[size_t(wrap(y, int(size.height))) * size.width + size_t(wrap(x, int(size.width)))];
  }
};

inline Pixel sampleColor(const Image& image, float u, float v) {
  const float x = frac(u) * image.size.width - .5f, y = frac(v) * image.size.height - .5f;
  const int left = int(std::floor(x)), top = int(std::floor(y));
  const auto upper = lerpColor(image.sample(left, top), image.sample(left + 1, top), frac(x));
  const auto lower = lerpColor(image.sample(left, top + 1), image.sample(left + 1, top + 1), frac(x));
  return lerpColor(upper, lower, frac(y));
}

inline Pixel adjustHsv(Pixel color, float hue, float saturation, float value) {
  std::array<float, 3> rgb{toSrgb(color[0]), toSrgb(color[1]), toSrgb(color[2])};
  const float lowest = *std::min_element(rgb.begin(), rgb.end());
  const float highest = *std::max_element(rgb.begin(), rgb.end());
  const float chroma = highest - lowest;
  float direction = 0;
  if (chroma > 0) {
    if (highest == rgb[0]) direction = (rgb[1] - rgb[2]) / chroma;
    else if (highest == rgb[1]) direction = 2 + (rgb[2] - rgb[0]) / chroma;
    else direction = 4 + (rgb[0] - rgb[1]) / chroma;
  }
  const float h = frac(direction / 6 + hue / 360) * 6;
  const float v = unit(highest * value);
  const float accent = v * unit(highest ? saturation * chroma / highest : 0);
  const float intermediate = accent * (1 - std::abs((h - 2.f * std::floor(h * .5f)) - 1));
  switch (unsigned(h)) {
    case 0: rgb = {accent, intermediate, 0}; break;
    case 1: rgb = {intermediate, accent, 0}; break;
    case 2: rgb = {0, accent, intermediate}; break;
    case 3: rgb = {0, intermediate, accent}; break;
    case 4: rgb = {intermediate, 0, accent}; break;
    default: rgb = {accent, 0, intermediate}; break;
  }
  for (size_t c = 0; c < rgb.size(); ++c) color[c] = fromSrgb(rgb[c] + v - accent);
  return color;
}

struct Evaluation {
  Resolution size;
  std::map<std::string, std::shared_ptr<const Image>> outputs;
};

constexpr size_t memoryBudget = size_t(1024) * 1024 * 1024;
constexpr unsigned maxThreads = 16;

class NodeCache {
  struct Entry {
    std::string key;
    std::shared_ptr<const Image> image;
  };
  size_t capacity, used = 0, hits = 0;
  std::list<Entry> entries;
  std::map<std::string, std::list<Entry>::iterator> indices;

public:
  explicit NodeCache(size_t budget) : capacity(budget) {}
  NodeCache(const NodeCache&) = delete;
  NodeCache& operator=(const NodeCache&) = delete;
  size_t limit() const { return capacity; }
  size_t bytes() const { return used; }
  size_t hitCount() const { return hits; }

  std::shared_ptr<const Image> find(const std::string& key) {
    auto found = indices.find(key);
    if (found == indices.end()) return {};
    entries.splice(entries.begin(), entries, found->second);
    ++hits;
    return found->second->image;
  }

  void store(std::string key, std::shared_ptr<const Image> image) {
    const size_t bytes = image->pixels.size() * sizeof(Pixel);
    if (key.empty() || bytes > capacity) return;
    while (used > capacity - bytes) {
      used -= entries.back().image->pixels.size() * sizeof(Pixel);
      indices.erase(entries.back().key);
      entries.pop_back();
    }
    entries.push_front({std::move(key), std::move(image)});
    indices.emplace(entries.front().key, entries.begin());
    used += bytes;
  }
};

inline std::string evalKey(const Node& node, uint64_t seed, Resolution size, float time,
                           const std::vector<std::string>& ancestors) {
  if (node.spec->kernel == Kernel::image) return {}; // Imported originals are retained by the source, independently of cache eviction.
  Json inputs = Json::array();
  for (int slot : node.inputs) {
    if (slot < 0) continue;
    if (ancestors[slot].empty()) return {};
    inputs.push_back(ancestors[slot]);
  }
  const auto scope = node.spec->kernel == Kernel::time ? time : 0.f;
  return ftebroom::sha256(Json{{"algorithm", generatorVersion}, {"backend", "cpu-reference"},
    {"type", node.spec->type}, {"params", node.p}, {"first", node.first}, {"last", node.last}, {"palette", node.palette},
    {"resolution", {size.width, size.height}}, {"seed", seed}, {"time", scope}, {"inputs", inputs}}.dump());
}

inline Pixel evaluatePixel(const Node& node, uint64_t seed, int x, int y, Resolution size,
                           const std::array<const Image*, 3>& input, float time) {
  const auto wrappedX = unsigned(x) < size.width ? x : wrap(x, int(size.width));
  const auto wrappedY = unsigned(y) < size.height ? y : wrap(y, int(size.height));
  const float u = (float(wrappedX) + .5f) / float(size.width);
  const float v = (float(wrappedY) + .5f) / float(size.height);
  switch (node.spec->kernel) {
  case Kernel::constant: return scalar(node.p[0]);
  case Kernel::color: return node.first;
  case Kernel::time: return scalar(time);
  case Kernel::seed: return scalar(lattice(seed, 0, 0));
  case Kernel::uv: return {u, v, 0, 1};
  case Kernel::image: return resample(*node.raster, x, y, size, u, v);
  case Kernel::noise: return scalar(.5f + node.p[1] * (valueNoise(seed, int(node.p[0]), u, v) - .5f));
  case Kernel::gradient: return scalar(.5f + node.p[1] * (gradientNoise(seed, int(node.p[0]), u, v) - .5f));
  case Kernel::worley: return scalar(cellularDistance(seed, int(node.p[0]), node.p[1], u, v));
  case Kernel::cracks: {
    const auto distances = cellularDistances(seed, int(node.p[0]), node.p[1], u, v);
    return scalar((distances[1] - distances[0]) / node.p[2]);
  }
  case Kernel::streaks:
    return scalar(.5f + node.p[2] * (valueNoise(seed, int(node.p[0]), int(node.p[1]), u, v) - .5f));
  case Kernel::fibers: {
    const float bend = (gradientNoise(seed + 1, 4, u, v) - .5f) * .045f;
    const float grain = valueNoise(seed, int(node.p[0]), int(node.p[1]), frac(u + bend), v);
    return scalar(.5f + (grain - .5f) * node.p[2]);
  }
  case Kernel::spots: {
    const float d = cellularDistance(seed, int(node.p[0]), node.p[2], u, v);
    return scalar((node.p[1] - d) * 10 + .5f);
  }
  case Kernel::veins: {
    const float drift = gradientNoise(seed, 5, u, v) * node.p[2];
    const float phase = frac((u + v) * node.p[0] + drift);
    return scalar(1.f - std::abs(phase - .5f) / node.p[1]);
  }
  case Kernel::fbm:
  case Kernel::ridged: {
    float sum = 0.f, weight = 0.f, amplitude = 1.f;
    int period = int(node.p[0]);
    for (int octave = 0; octave < int(node.p[1]); ++octave) {
      const float noise = gradientNoise(seed + unsigned(octave), period, u, v);
      const float ridge = 1.f - std::abs(noise * 2 - 1);
      sum += amplitude * (node.spec->kernel == Kernel::ridged ? ridge * ridge : noise);
      weight += amplitude;
      amplitude *= node.p[2];
      period *= int(node.p[3]);
    }
    return scalar(sum / weight);
  }
  case Kernel::white: return scalar(lattice(seed, unsigned(wrappedX), unsigned(wrappedY)));
  case Kernel::flow: {
    constexpr float turn = 6.28318530718f;
    const float phase = input[0]->sample(x, y)[0] * node.p[1];
    const int patch = int(node.p[0]);
    const float cycle = turn * frac(phase);
    if (node.p[3] == 1) {
      const float position = frac(u - phase);
      const float large = gradientNoise(seed, patch, position, v);
      const float fine = gradientNoise(seed + 1, patch * 2, u, frac(v + phase));
      return scalar(1.f - std::abs(large * 2 - 1.f + (fine - .5f) * node.p[2]));
    }
    if (node.p[3] == 2) {
      const float sway = .11f * node.p[2] * std::sin(turn * v + cycle);
      const auto cells = cellularDistances(seed, patch, .78f, frac(u + sway), frac(v - phase));
      return scalar((cells[1] - cells[0]) * 2.1f);
    }
    if (node.p[3] == 3) {
      const float drift = gradientNoise(seed, 4, u, v) * node.p[2];
      const float signal = std::sin(turn * frac(u * patch + drift + phase));
      const float pulse = .72f + .28f * std::sin(cycle);
      return scalar(pulse * (.53f + .47f * signal));
    }
    const float perturbation = node.p[2] * std::sin(turn * frac(v * node.p[0] - phase));
    return scalar(.5f + .5f * std::sin(turn * frac(u * node.p[0] + phase + perturbation)));
  }
  case Kernel::bricks:
  case Kernel::flemish: {
    const float row = v * node.p[1];
    const float column = u * node.p[0] + (int(row) % 2 ? .5f : 0.f);
    const float a = frac(column), b = frac(row);
    float edge = std::min(a, 1.f - a);
    if (node.spec->kernel == Kernel::flemish) edge = std::min(edge, std::abs(a - 1.f / 3.f));
    const float side = std::min(edge, std::min(b, 1.f - b));
    if (side <= node.p[2] * .5f) return scalar(.08f);
    const float bevel = unit((side - node.p[2] * .5f) / node.p[3]);
    const float variation = input[0]->sample(x, y)[0];
    const float course = lattice(seed, wrap(int(column), int(node.p[0])), wrap(int(row), int(node.p[1])));
    return scalar(mix(.08f, .83f + .16f * course - node.p[4] * variation, bevel));
  }
  case Kernel::tiles: {
    const float xCell = frac(u * node.p[0]), yCell = frac(v * node.p[1]);
    const float d = std::min(std::min(xCell, 1 - xCell), std::min(yCell, 1 - yCell));
    return scalar(mix(.05f, .98f, unit((d - node.p[2] * .5f) / node.p[3])));
  }
  case Kernel::hex: {
    const float row = v * node.p[1], column = u * node.p[0];
    const int current = int(std::floor(row));
    float edge = 1.f;
    for (int cell = current - 1; cell <= current + 1; ++cell) {
      const float dx = std::abs(frac(column + ((cell & 1) ? .5f : 0.f)) - .5f);
      const float dy = std::abs(row - cell - .5f);
      edge = std::min(edge, std::max(dx, .5f * dx + .75f * dy));
    }
    return scalar((.5f - edge - node.p[2] * .5f) / node.p[3]);
  }
  case Kernel::panels: {
    const float a = std::abs(frac(u * node.p[0]) - .5f), b = std::abs(frac(v * node.p[1]) - .5f);
    const float edge = .5f - std::max(a, b) - node.p[2] * .25f;
    const float corner = .035f - std::abs(a - .39f) - std::abs(b - .39f);
    const float recess = .5f - std::abs(std::max(a, b) - .31f) / .035f;
    return scalar(.06f + .76f * unit(edge / node.p[3]) - .26f * unit(recess) + .15f * unit(corner * 65));
  }
  case Kernel::checker:
    return scalar(float((int(u * node.p[0]) + int(v * node.p[1])) & 1));
  case Kernel::stripes: {
    const float d = std::abs(frac(u * node.p[0] + v * node.p[3]) - .5f);
    return scalar(.5f + (.5f * node.p[1] - d) / node.p[2]);
  }
  case Kernel::grate: {
    const float xBar = node.p[2] - std::abs(frac(u * node.p[0]) - .5f);
    const float yBar = node.p[2] * .66f - std::abs(frac(v * node.p[1]) - .5f);
    return scalar(.5f + std::max(xBar, yBar) / node.p[3]);
  }
  case Kernel::rivets: {
    const float xRivet = frac(u * node.p[0]) - .5f, yRivet = frac(v * node.p[1]) - .5f;
    return scalar((node.p[2] - std::sqrt(xRivet * xRivet + yRivet * yRivet)) / node.p[3]);
  }
  case Kernel::diamond: {
    const float row = v * node.p[1];
    const float a = frac(u * node.p[0]) - .5f, b = frac(row) - .5f;
    const float diagonal = int(row) & 1 ? b - a : b + a;
    const float length = std::abs(a) * .8f;
    return scalar((node.p[2] - std::abs(diagonal) * 2 - length) / node.p[3]);
  }
  case Kernel::rectangle: {
    const float d = std::max(std::abs(u - .5f) - node.p[0] * .5f, std::abs(v - .5f) - node.p[1] * .5f);
    return scalar(.5f - d / node.p[2]);
  }
  case Kernel::circle: {
    const float a = u - .5f, b = v - .5f;
    return scalar(.5f + (node.p[0] - std::sqrt(a * a + b * b)) / node.p[1]);
  }
  case Kernel::polygon: {
    constexpr float turn = 6.28318530718f;
    const int sides = int(node.p[2]);
    const float a = u - .5f, b = v - .5f;
    const float sector = turn / sides;
    const float angle = std::atan2(b, a);
    const float nearest = std::round(angle / sector) * sector;
    const float edge = node.p[0] * std::cos(sector * .5f) -
      (a * std::cos(nearest) + b * std::sin(nearest));
    return scalar(.5f + edge / node.p[1]);
  }
  case Kernel::rings: {
    const float xRing = u - .5f, yRing = v - .5f;
    const float radius = std::sqrt(xRing * xRing + yRing * yRing);
    const float offset = gradientNoise(seed, 5, u, v) * node.p[1];
    const float ring = frac(radius * node.p[0] + offset);
    return scalar(1.f - std::abs(ring - .45f) * 2.f);
  }
  case Kernel::digit: {
    static constexpr unsigned glyphs[]{63, 6, 91, 79, 102, 109, 125, 7, 127, 111};
    const auto mask = glyphs[unsigned(node.p[0])];
    const float a = (u - .5f) / node.p[1], b = (v - .5f) / node.p[1];
    float value = 0;
    const auto segment = [&](unsigned bit, float x0, float y0, float w, float h) {
      if (!(mask & (1u << bit))) return;
      const auto d = std::max(std::abs(a - x0) - w, std::abs(b - y0) - h);
      value = std::max(value, unit(.5f - d * 110.f));
    };
    segment(0, 0, -.39f, .18f, .035f);
    segment(1, .20f, -.205f, .035f, .16f);
    segment(2, .20f, .205f, .035f, .16f);
    segment(3, 0, .39f, .18f, .035f);
    segment(4, -.20f, .205f, .035f, .16f);
    segment(5, -.20f, -.205f, .035f, .16f);
    segment(6, 0, 0, .18f, .035f);
    return scalar(value);
  }
  case Kernel::arrow: {
    const float horizontal = (u - .5f) / node.p[1] + .5f, vertical = std::abs(v - .5f);
    const float shaft = std::min(vertical - node.p[0] * .28f, 1.f);
    const float head = vertical - node.p[0] * 2.4f * (1.f - horizontal);
    const float outline = std::max(std::max(-horizontal, horizontal - 1.f), horizontal < .57f ? shaft : head);
    return scalar(.5f - outline * 100.f);
  }
  case Kernel::distance: {
    const int radius = int(node.p[0]);
    int closest = radius * radius;
    const auto inside = [&](int dx, int dy) { return input[0]->sample(x + dx, y + dy)[0] >= node.p[1]; };
    const bool sign = inside(0, 0);
    for (int dy = -radius; dy <= radius; ++dy)
      for (int dx = -radius; dx <= radius; ++dx) {
        const int square = dx * dx + dy * dy;
        if (square < closest && sign != inside(dx, dy)) closest = square;
      }
    const float distance = std::max(0.f, std::sqrt(float(closest)) - .5f) / std::max(.5f, node.p[0] - .5f);
    return scalar(.5f + (sign ? .5f : -.5f) * distance);
  }
  case Kernel::ramp:
    return lerpColor(node.first, node.last, input[0]->sample(x, y)[0]);
  case Kernel::blend: {
    if (!node.p[1]) return input[0]->sample(x, y);
    const auto base = input[0]->sample(x, y);
    auto top = input[1]->sample(x, y);
    if (node.p[2] && base[3] > 0) {
      for (unsigned c = 0; c < 3; ++c) {
        const float a = base[c], b = top[c];
        switch (int(node.p[2])) {
          case 1: top[c] = a * b; break;
          case 2: top[c] = 1 - (1 - a) * (1 - b); break;
          case 3: top[c] = unit(a + b); break;
          case 4: top[c] = a < .5f ? 2 * a * b : 1 - 2 * (1 - a) * (1 - b); break;
        }
      }
    }
    return lerpColor(base, top, node.p[0] * input[2]->sample(x, y)[0]);
  }
  case Kernel::multiply:
    return scalar(input[0]->sample(x, y)[0] * input[1]->sample(x, y)[0]);
  case Kernel::add:
    return scalar(input[0]->sample(x, y)[0] + input[1]->sample(x, y)[0]);
  case Kernel::scalarMix:
    return scalar(mix(input[0]->sample(x, y)[0], input[1]->sample(x, y)[0], node.p[0]));
  case Kernel::minimum:
    return scalar(std::min(input[0]->sample(x, y)[0], input[1]->sample(x, y)[0]));
  case Kernel::maximum:
    return scalar(std::max(input[0]->sample(x, y)[0], input[1]->sample(x, y)[0]));
  case Kernel::difference:
    return scalar(std::abs(input[0]->sample(x, y)[0] - input[1]->sample(x, y)[0]));
  case Kernel::invert:
    return scalar(1.f - input[0]->sample(x, y)[0]);
  case Kernel::clamp:
    return scalar(std::clamp(input[0]->sample(x, y)[0], node.p[0], node.p[1]));
  case Kernel::levels:
    return scalar(std::pow(unit((input[0]->sample(x, y)[0] - node.p[0]) / (node.p[1] - node.p[0])), 1.f / node.p[2]));
  case Kernel::histogram: {
    const auto sample = input[0]->sample(x, y)[0];
    return scalar(node.p[3] > node.p[2] ? (sample - node.p[2]) / (node.p[3] - node.p[2]) : sample);
  }
  case Kernel::transform: {
    constexpr float radians = .017453292519943295f;
    const float angle = node.p[4] * radians;
    const float a = (u - .5f) / node.p[0], b = (v - .5f) / node.p[1];
    const float c = std::cos(angle), s = std::sin(angle);
    return sampleColor(*input[0], .5f + a * c + b * s - node.p[2], .5f - a * s + b * c - node.p[3]);
  }
  case Kernel::tile: return sampleColor(*input[0], u * node.p[0], v * node.p[1]);
  case Kernel::mirror: return sampleColor(*input[0], node.p[0] != 1 ? 1 - u : u, node.p[0] != 0 ? 1 - v : v);
  case Kernel::rotate90:
    switch (int(node.p[0])) {
      case 1: return sampleColor(*input[0], v, 1 - u);
      case 2: return sampleColor(*input[0], 1 - u, 1 - v);
      case 3: return sampleColor(*input[0], 1 - v, u);
      default: return input[0]->sample(x, y);
    }
  case Kernel::uvSample: {
    const auto uv = input[1]->sample(x, y);
    return sampleColor(*input[0], uv[0], uv[1]);
  }
  case Kernel::warp:
  case Kernel::directionalWarp: {
    const auto uv = input[0]->sample(x, y);
    const float a = (input[1]->sample(x, y)[0] - .5f) * node.p[0];
    if (node.spec->kernel == Kernel::warp) {
      const float b = (input[2]->sample(x, y)[0] - .5f) * node.p[0];
      return {frac(uv[0] + a), frac(uv[1] + b), 0, 1};
    }
    constexpr float radians = .017453292519943295f;
    const float angle = node.p[1] * radians;
    return {frac(uv[0] + a * std::cos(angle)), frac(uv[1] + a * std::sin(angle)), 0, 1};
  }
  case Kernel::hsv: return adjustHsv(input[0]->sample(x, y), node.p[0], node.p[1], node.p[2]);
  case Kernel::colorize: {
    const auto source = input[0]->sample(x, y);
    const float luma = .2126f * source[0] + .7152f * source[1] + .0722f * source[2];
    auto tint = lerpColor(node.first, node.last, luma);
    tint[3] *= source[3];
    return lerpColor(source, tint, node.p[0]);
  }
  case Kernel::palette: {
    const auto source = input[0]->sample(x, y);
    Pixel nearest{};
    float distance = std::numeric_limits<float>::infinity();
    for (const auto& swatch : node.palette) {
      const float r = source[0] - swatch[0], g = source[1] - swatch[1], b = source[2] - swatch[2];
      const float square = .2126f * r * r + .7152f * g * g + .0722f * b * b;
      if (square < distance) { distance = square; nearest = swatch; }
    }
    nearest[3] *= source[3];
    return nearest;
  }
  case Kernel::quantize: {
    const float levels = node.p[0] - 1;
    return scalar(std::floor(input[0]->sample(x, y)[0] * levels + .5f) / levels);
  }
  case Kernel::posterize:
  case Kernel::dither: {
    constexpr int matrix[][4]{{0,8,2,10}, {12,4,14,6}, {3,11,1,9}, {15,7,13,5}};
    const float threshold = node.spec->kernel == Kernel::dither ? (matrix[wrappedY & 3][wrappedX & 3] + .5f) / 16 : .5f;
    const float levels = node.p[0] - 1;
    auto color = input[0]->sample(x, y);
    for (size_t c = 0; c < 3; ++c)
      color[c] = fromSrgb(std::floor(toSrgb(color[c]) * levels + threshold) / levels);
    return color;
  }
  case Kernel::pixelate:
    return sampleColor(*input[0], (std::floor(u * node.p[0]) + .5f) / node.p[0],
      (std::floor(v * node.p[1]) + .5f) / node.p[1]);
  case Kernel::channelPack:
    return {input[0]->sample(x, y)[0], input[1]->sample(x, y)[0], input[2]->sample(x, y)[0], 1};
  case Kernel::channelUnpack: return scalar(input[0]->sample(x, y)[unsigned(node.p[0])]);
  case Kernel::retroShade: {
    auto color = input[0]->sample(x, y);
    if (!node.p[0]) return color;
    const auto normal = input[1]->sample(x, y);
    const float light = unit(-.23f * normal[0] + .48f * normal[1] + .846f * normal[2]);
    const float steps = node.p[1] - 1;
    const float shade = mix(node.p[2], 1.f, std::floor(light * steps + .5f) / steps);
    for (unsigned c = 0; c < 3; ++c) color[c] *= shade;
    return color;
  }
  case Kernel::directionalBlur: {
    constexpr std::array<std::array<int, 2>, 4> axes{{{1, 0}, {0, 1}, {1, 1}, {1, -1}}};
    const auto [dx, dy] = axes[unsigned(node.p[1])];
    const int radius = int(node.p[0]);
    float sum = 0;
    for (int offset = -radius; offset <= radius; ++offset)
      sum += input[0]->sample(x + offset * dx, y + offset * dy)[0];
    return scalar(sum / (2 * radius + 1));
  }
  case Kernel::slopeBlur: {
    const auto slope = [&](int a, int b) { return input[1]->sample(a, b)[0]; };
    const float dx = slope(x + 1, y) - slope(x - 1, y);
    const float dy = slope(x, y + 1) - slope(x, y - 1);
    const int samples = int(node.p[1]);
    float sum = 0;
    for (int i = 0; i < samples; ++i) {
      const float step = node.p[0] * i / (samples - 1);
      const float sx = dx * step, sy = dy * step;
      const int a = int(std::floor(sx)), b = int(std::floor(sy));
      const auto value = [&](int x0, int y0) { return input[0]->sample(x + x0, y + y0)[0]; };
      sum += mix(mix(value(a, b), value(a + 1, b), frac(sx)),
                 mix(value(a, b + 1), value(a + 1, b + 1), frac(sx)), frac(sy));
    }
    return scalar(sum / samples);
  }
  case Kernel::normalBlend: {
    const auto base = input[0]->sample(x, y), detail = input[1]->sample(x, y);
    const float nx = base[0] + detail[0] * node.p[0];
    const float ny = base[1] + detail[1] * node.p[0];
    const float nz = base[2] * mix(1.f, detail[2], node.p[0]);
    const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (length < 1e-8f) return {0,0,1,1};
    return {nx / length, ny / length, nz / length, 1};
  }
  case Kernel::luminance: {
    const auto& color = input[0]->sample(x, y);
    return scalar((.2126f * color[0] + .7152f * color[1] + .0722f * color[2]) * color[3]);
  }
  case Kernel::blur:
  case Kernel::dilate:
  case Kernel::erode: {
    const int radius = int(node.p[0]), width = 2 * radius + 1;
    float value = node.spec->kernel == Kernel::erode ? 1.f : 0.f;
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        const auto sample = input[0]->sample(x + dx, y + dy)[0];
        if (node.spec->kernel == Kernel::erode) value = std::min(value, sample);
        else if (node.spec->kernel == Kernel::dilate) value = std::max(value, sample);
        else value += sample;
      }
    }
    return scalar(node.spec->kernel == Kernel::blur ? value / (width * width) : value);
  }
  case Kernel::edge: {
    const float dx = input[0]->sample(x + 1, y)[0] - input[0]->sample(x - 1, y)[0];
    const float dy = input[0]->sample(x, y + 1)[0] - input[0]->sample(x, y - 1)[0];
    return scalar(node.p[0] * std::sqrt(dx * dx + dy * dy));
  }
  case Kernel::normal: {
    const float dx = input[0]->sample(x - 1, y)[0] - input[0]->sample(x + 1, y)[0];
    const float dy = input[0]->sample(x, y - 1)[0] - input[0]->sample(x, y + 1)[0];
    const float nx = dx * node.p[0], ny = dy * node.p[0] * node.p[1];
    const float length = std::sqrt(nx * nx + ny * ny + 1.f);
    return {nx / length, ny / length, 1.f / length, 1.f};
  }
  case Kernel::ao: {
    const float height = input[0]->sample(x, y)[0];
    const float sides = input[0]->sample(x - 1, y)[0] + input[0]->sample(x + 1, y)[0];
    const float ends = input[0]->sample(x, y - 1)[0] + input[0]->sample(x, y + 1)[0];
    return scalar(1.f - std::max(0.f, (sides + ends) * .25f - height) * node.p[0]);
  }
  case Kernel::unknown: break;
  }
  throw Error("graph.nodes." + node.id, "The output has no evaluator.");
}

inline int samplingRadius(const Node& node) {
  switch (node.spec->kernel) {
    case Kernel::histogram:
    case Kernel::transform:
    case Kernel::tile:
    case Kernel::mirror:
    case Kernel::rotate90:
    case Kernel::uvSample:
    case Kernel::pixelate: return -1;
    case Kernel::normal:
    case Kernel::ao:
    case Kernel::edge: return 1;
    case Kernel::distance:
    case Kernel::blur:
    case Kernel::dilate:
    case Kernel::erode:
    case Kernel::directionalBlur: return int(node.p[0]);
    case Kernel::slopeBlur: return int(std::ceil(node.p[0])) + 1;
    default: return 0;
  }
}

inline std::optional<Evaluation> evaluateTiles(const Plan& plan, Resolution size, size_t budget, float time,
                                                unsigned threads, const std::atomic<bool>* cancelled) {
  constexpr unsigned extent = 128, maxHalo = 64;
  const auto check = [&] {
    if (cancelled && cancelled->load(std::memory_order_relaxed)) throw Error("render", "Rendering was cancelled.");
  };
  std::vector<unsigned> halo(plan.nodes.size()), uses(plan.nodes.size());
  std::vector<uint64_t> seeds(plan.nodes.size());
  for (auto it = plan.order.rbegin(); it != plan.order.rend(); ++it) {
    const auto index = *it;
    const auto& node = plan.nodes[index];
    const int radius = samplingRadius(node);
    if (radius < 0 || halo[index] + unsigned(radius) > maxHalo) return std::nullopt;
    seeds[index] = stableSeed(plan.seed, node.seedIdentity.empty() ? node.id : node.seedIdentity);
    for (int in : node.inputs) if (in >= 0) {
      halo[in] = std::max(halo[in], halo[index] + unsigned(radius));
      ++uses[in];
    }
  }
  const size_t bytesPerOutput = size_t(size.width) * size.height * sizeof(Pixel);
  std::set<int> requiredOutputs;
  for (const auto& [channel, node] : plan.outputs) requiredOutputs.insert(node);
  if (requiredOutputs.empty() || requiredOutputs.size() > budget / bytesPerOutput) return std::nullopt;
  budget -= requiredOutputs.size() * bytesPerOutput;
  const auto tileBytes = [&](int node) {
    const auto edge = size_t(extent + halo[node] * 2);
    return edge * edge * sizeof(Pixel);
  };
  auto remaining = uses;
  size_t peak = 0, current = 0;
  for (int node : plan.order) {
    current += tileBytes(node);
    peak = std::max(peak, current);
    for (int in : plan.nodes[node].inputs)
      if (in >= 0 && !--remaining[in]) current -= tileBytes(in);
    if (!remaining[node]) current -= tileBytes(node);
  }
  if (!peak || budget < peak) return std::nullopt;
  const unsigned columns = (size.width + extent - 1) / extent, rows = (size.height + extent - 1) / extent;
  const unsigned workers = std::min({threads, columns * rows, unsigned(std::min<size_t>(budget / peak, maxThreads))});
  std::vector<std::shared_ptr<Image>> output(plan.nodes.size());
  for (int node : requiredOutputs) {
    check();
    output[node] = std::make_shared<Image>(Image{size, plan.nodes[node].spec->output.type, {}});
    output[node]->pixels.resize(size_t(size.width) * size.height);
  }
  const auto worker = [&](unsigned slot) {
    for (unsigned tile = slot; tile < columns * rows; tile += workers) {
      check();
      const unsigned x = (tile % columns) * extent, y = (tile / columns) * extent;
      const unsigned width = std::min(extent, size.width - x), height = std::min(extent, size.height - y);
      std::vector<std::optional<Image>> local(plan.nodes.size());
      auto references = uses;
      for (int index : plan.order) {
        check();
        const auto& node = plan.nodes[index];
        const unsigned pad = halo[index];
        const auto span = width + 2 * pad;
        auto& image = local[index].emplace(Image{size, node.spec->output.type,
          std::vector<Pixel>(size_t(span) * (height + 2 * pad)), int(x) - int(pad), int(y) - int(pad), span});
        std::array<const Image*, 3> input{};
        for (size_t i = 0; i < input.size(); ++i)
          if (node.inputs[i] >= 0) input[i] = &*local[node.inputs[i]];
        for (unsigned row = 0; row < height + 2 * pad; ++row)
          for (unsigned col = 0; col < span; ++col)
            image.pixels[size_t(row) * span + col] =
              evaluatePixel(node, seeds[index], image.tileX + int(col), image.tileY + int(row), size, input, time);
        if (output[index]) {
          for (unsigned row = 0; row < height; ++row)
            std::copy_n(image.pixels.begin() + size_t(row + pad) * span + pad, width,
              output[index]->pixels.begin() + size_t(y + row) * size.width + x);
        }
        for (int in : node.inputs) if (in >= 0 && !--references[in]) local[in].reset();
        if (!references[index]) local[index].reset();
      }
    }
  };
  std::vector<std::future<void>> background;
  background.reserve(workers - 1);
  for (unsigned slot = 1; slot < workers; ++slot) background.push_back(std::async(std::launch::async, worker, slot));
  worker(0);
  for (auto& job : background) job.get();
  Evaluation result{size, {}};
  for (const auto& [channel, node] : plan.outputs) result.outputs.emplace(channel, output[node]);
  return result;
}

inline Evaluation evaluate(const Plan& plan, Resolution size,
                           const std::atomic<bool>* cancelled = nullptr, size_t budget = memoryBudget,
                           unsigned threads = 1, unsigned frame = 0, NodeCache* cache = nullptr) {
  require(size.width > 0 && size.width <= maxDimension && size.height > 0 && size.height <= maxDimension,
    "resolution", "The render size is outside the 1–8192 range.");
  require(threads > 0 && threads <= maxThreads, "threads", "Choose 1 to 16 CPU workers.");
  require(plan.frames > 0 && (plan.loop || frame < plan.frames), "frame", "Frame is outside the animation range.");
  const float time = plan.frames == 1 ? 0.f : plan.loop ? float(frame % plan.frames) / plan.frames
    : float(frame) / (plan.frames - 1);
  const size_t pixels = size_t(size.width) * size.height;
  const size_t imageBytes = pixels * sizeof(Pixel);
  require(!cache || cache->limit() < budget, "assets", "The preview cache exceeds the render budget.");
  if (cache) budget -= cache->limit();
  std::vector<unsigned> uses(plan.nodes.size());
  std::set<const Raster*> inputRasters;
  for (int n : plan.order) {
    const auto& raster = plan.nodes[n].raster;
    if (!raster || !inputRasters.insert(raster.get()).second) continue;
    require(raster->rgba.size() <= budget, "assets", "The decoded assets exceed the render budget.");
    budget -= raster->rgba.size();
  }
  if (!cache && std::max(size.width, size.height) >= 512) {
    auto tiles = evaluateTiles(plan, size, budget, time, threads, cancelled);
    if (tiles) return std::move(*tiles);
  }
  for (int n : plan.order)
    for (int in : plan.nodes[n].inputs) if (in >= 0) ++uses[in];
  for (const auto& [name, index] : plan.outputs) ++uses[index];
  unsigned resident = 0, peak = 0;
  auto estimated = uses;
  for (int n : plan.order) {
    peak = std::max(peak, ++resident);
    for (int in : plan.nodes[n].inputs) if (in >= 0 && !--estimated[in]) --resident;
  }
  require(peak > 0 && peak <= budget / imageBytes, "resolution",
    "Evaluation exceeds its RAM budget. Reduce the size or select fewer channels.");

  std::vector<std::shared_ptr<const Image>> images(plan.nodes.size());
  std::vector<std::string> keys(plan.nodes.size());
  const auto checkCancel = [&] {
    if (cancelled && cancelled->load(std::memory_order_relaxed))
      throw Error("render", "Rendering was cancelled.");
  };
  constexpr unsigned tileSize = 128;
  const unsigned columns = (size.width + tileSize - 1) / tileSize;
  const unsigned tileCount = columns * ((size.height + tileSize - 1) / tileSize);
  const unsigned workers = std::min(threads, tileCount);
  for (int n : plan.order) {
    checkCancel();
    const auto& node = plan.nodes[n];
    const auto seed = stableSeed(plan.seed, node.seedIdentity.empty() ? node.id : node.seedIdentity);
    const auto path = "graph.nodes." + node.id;
    if (cache) {
      keys[n] = evalKey(node, seed, size, time, keys);
      images[n] = cache->find(keys[n]);
    }
    if (!images[n]) {
      auto image = std::make_shared<Image>(Image{size, node.spec->output.type, {}});
      try {
        image->pixels.resize(pixels);
      } catch (const std::bad_alloc&) {
        throw Error(path, "Available memory is too low for this render.");
      }
      std::array<const Image*, 3> input{};
      for (size_t i = 0; i < input.size(); ++i)
        if (node.inputs[i] >= 0) input[i] = images[node.inputs[i]].get();
      std::optional<Node> prepared;
      if (node.spec->kernel == Kernel::histogram) {
        constexpr unsigned bins = 1024;
        std::array<size_t, bins> distribution{};
        for (unsigned y = 0; y < size.height; ++y) {
          checkCancel();
          for (unsigned x = 0; x < size.width; ++x) {
            const auto value = input[0]->pixels[size_t(y) * size.width + x][0];
            ++distribution[unsigned(value * (bins - 1))];
          }
        }
        const auto percentile = [&](float fraction) {
          const auto rank = size_t(double(fraction) * (pixels - 1));
          size_t examined = 0;
          for (unsigned i = 0; i < bins; ++i) {
            examined += distribution[i];
            if (examined > rank) return float(i) / (bins - 1);
          }
          return 1.f;
        };
        prepared = node;
        prepared->p[2] = percentile(node.p[0]);
        prepared->p[3] = percentile(node.p[1]);
      }
      const auto& evaluating = prepared ? *prepared : node;
      const auto renderTiles = [&](unsigned worker) {
        for (unsigned tile = worker; tile < tileCount; tile += workers) {
          checkCancel();
          const unsigned row = (tile / columns) * tileSize, column = (tile % columns) * tileSize;
          for (unsigned y = row; y < std::min(row + tileSize, size.height); ++y)
            for (unsigned x = column; x < std::min(column + tileSize, size.width); ++x)
              image->pixels[size_t(y) * size.width + x] = evaluatePixel(evaluating, seed, int(x), int(y), size, input, time);
        }
      };
      std::vector<std::future<void>> background;
      background.reserve(workers - 1);
      for (unsigned worker = 1; worker < workers; ++worker)
        background.push_back(std::async(std::launch::async, renderTiles, worker));
      renderTiles(0);
      for (auto& worker : background) worker.get();
      if (cache) cache->store(keys[n], image);
      images[n] = std::move(image);
    }
    for (int in : node.inputs) if (in >= 0 && !--uses[in]) images[in].reset();
  }
  checkCancel();
  Evaluation result{size, {}};
  for (const auto& [name, index] : plan.outputs) result.outputs.emplace(name, images[index]);
  return result;
}

} // namespace ftebroom::textureStudio
