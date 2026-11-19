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

// Fold the cycle into a quarter turn before evaluating the sine polynomial.
// Keeping this calculation in double avoids platform-specific sinf rounding
// differences in 16-bit height maps.
inline float cycleSine(float turns) {
  const double cycle = frac(turns);
  const double centered = cycle > .5 ? cycle - 1 : cycle;
  const double folded = std::copysign(std::min(std::abs(centered), .5 - std::abs(centered)), centered);
  const double angle = folded * 6.283185307179586;
  const double squared = angle * angle;
  const double polynomial = 1 + squared * (-1.0 / 6 + squared * (1.0 / 120 +
    squared * (-1.0 / 5040 + squared * (1.0 / 362880 + squared *
    (-1.0 / 39916800 + squared / 6227020800.0)))));
  return float(angle * polynomial);
}

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

inline float softThreshold(float low, float high, float value) {
  const float t = unit((value - low) / (high - low));
  return t * t * (3.f - 2.f * t);
}

inline float materialGrain(GrainKind kind, int frequency, uint64_t seed, float u, float v) {
  u = frac(u);
  v = frac(v);
  const float broad = gradientNoise(seed, 5, u, v);
  const float grain = valueNoise(seed + 1, frequency * 3, u, v);
  switch (kind) {
  case GrainKind::mineral:
    return .46f + .42f * (broad - .5f) + .36f * (grain - .5f) +
      .18f * (valueNoise(seed + 2, frequency, u, v) - .5f);
  case GrainKind::stone: {
    const float coarse = valueNoise(seed + 2, frequency, u, v);
    const float chips = softThreshold(.56f, .79f, coarse * .74f + grain * .26f);
    const float inclusions = softThreshold(.66f, .86f, valueNoise(seed + 3, frequency * 2, u, v));
    return .34f + .22f * broad + .30f * chips - .16f * inclusions + .16f * (grain - .5f);
  }
  case GrainKind::wood: {
    // Growth lines bend around the knots and remain continuous across the tile boundary.
    const float bend = (valueNoise(seed + 2, 5, 3, u, v) - .5f) * .085f;
    const float knot = valueNoise(seed + 3, 9, 4, u, v);
    const float drift = (knot - .5f) * .036f + bend;
    const float growth = valueNoise(seed + 4, frequency, 3, u + drift, v);
    const float fine = valueNoise(seed + 5, frequency * 3, 11, u + drift, v);
    const float latewood = softThreshold(.58f, .87f, growth * .80f + fine * .20f);
    return .35f + .28f * broad + .25f * growth - .38f * latewood + .25f * (fine - .5f)
      - .19f * softThreshold(.80f, .94f, knot);
  }
  case GrainKind::brushed:
    return .50f + .20f * (broad - .5f) + .22f * (grain - .5f) +
      .38f * (valueNoise(seed + 6, 4, frequency * 3, u, v) - .5f);
  case GrainKind::sand: {
    const float ridge = cycleSine(v * frequency + .78f * broad + .4f * valueNoise(seed + 2, 4, u, v));
    return .48f + .14f * ridge + .26f * (grain - .5f) + .28f * (broad - .5f);
  }
  case GrainKind::fibers: {
    const float bend = (broad - .5f) * .065f;
    const float strand = valueNoise(seed + 2, frequency * 3, std::max(3, frequency / 5), u + bend, v);
    return .39f + .17f * broad + .28f * (strand - .5f) + .20f * (grain - .5f);
  }
  case GrainKind::polished:
    return .50f + .25f * (broad - .5f) + .13f * (grain - .5f);
  case GrainKind::endGrain: {
    const float x = cycleSine(frac(u - .17f) * .5f);
    const float y = .78f * cycleSine(frac(v - .86f) * .5f);
    const float radius = std::sqrt(x * x + y * y);
    const float growth = cycleSine(radius * frequency * .54f + 1.9f * broad +
      .65f * valueNoise(seed + 2, 7, u, v));
    const float latewood = softThreshold(.25f, .91f, growth);
    return .45f + .17f * broad - .21f * latewood + .19f * (grain - .5f);
  }
  case GrainKind::bedded: {
    const float bedding = v + .03f * cycleSine(u + .2f) + .14f * (broad - .5f);
    const float layers = valueNoise(seed + 2, 3, frequency, u, bedding);
    const float loose = valueNoise(seed + 3, std::max(2, frequency / 4), frequency * 3, u, bedding);
    return .32f + .30f * layers + .16f * loose + .25f * (grain - .5f);
  }
  case GrainKind::marble: {
    const float fracture = valueNoise(seed + 2, 3, 4, u, v);
    const float ripple = valueNoise(seed + 3, frequency, u, v);
    const float fault = u + v + .48f * (fracture - .5f) + .08f * (ripple - .5f);
    const float root = std::abs(frac(fault) - .49f);
    const float branch = std::abs(frac(fault + .25f * (broad - .42f) + .018f) - .49f);
    const float vein = 1.f - softThreshold(.005f, mix(.025f, .092f, fracture), root);
    const float hairline = 1.f - softThreshold(.002f, mix(.009f, .025f, ripple), branch);
    return .74f - .38f * vein - .32f * hairline + .14f * (grain - .5f);
  }
  case GrainKind::count: break;
  }
  return .5f;
}

inline float surfaceWear(WearKind kind, int frequency, float density, uint64_t seed, float u, float v) {
  u = frac(u);
  v = frac(v);
  const float field = valueNoise(seed, 4, u, v);
  if (kind == WearKind::pits || kind == WearKind::chips || kind == WearKind::deposits) {
    const float detail = valueNoise(seed + 1, frequency, u, v);
    const float edge = valueNoise(seed + 2, frequency * 3, u, v);
    const float threshold = 1.f - density * .39f;
    if (kind == WearKind::pits)
      return softThreshold(threshold, threshold + .15f, .15f * field + .71f * detail + .14f * edge);
    if (kind == WearKind::chips)
      return softThreshold(threshold, threshold + .11f, .50f * field + .34f * detail + .16f * edge);
    return softThreshold(threshold, threshold + .25f, .61f * field + .29f * detail + .10f * edge);
  }

  // Periodic cells contain sparse marks with tapered edges and independent position and length.
  const bool runoff = kind == WearKind::runoff;
  const int columns = runoff ? frequency : std::max(2, frequency / 2);
  const int rows = runoff ? std::max(2, frequency / 7) : columns;
  const float x = frac(u) * columns, y = frac(v) * rows;
  const int ix = int(x), iy = int(y);
  const float pick = lattice(seed + 3, ix, iy);
  if (pick >= density * (.22f + .78f * field)) return 0.f;
  const float dx = frac(x) - mix(.37f, .63f, lattice(seed + 4, ix, iy));
  const float dy = frac(y) - mix(.36f, .64f, lattice(seed + 5, ix, iy));
  const float angle = lattice(seed + 6, ix, iy) - .5f;
  const float along = runoff ? dy : dx + dy * angle;
  const float across = runoff ? dx + .065f * cycleSine(dy + angle)
                              : dy - dx * angle;
  const float length = mix(.13f, .32f, lattice(seed + 7, ix, iy));
  const float taper = runoff ? unit((length - along) / (2.f * length)) : 1.f;
  const float width = runoff ? .12f : .028f;
  const float footprint = 1.f - softThreshold(length * .48f, length, std::abs(along));
  return footprint * taper * (1.f - softThreshold(width * .21f, width, std::abs(across))) *
    (.46f + .54f * valueNoise(seed + 8, frequency * 2, u, v));
}

// Keep features inside their cells and sample the surrounding field periodically. Marks
// of two size classes can meet without lining up on the same lattice.
inline float mineralMarks(uint64_t seed, int frequency, float density, float u, float v,
                          float minRadius, float maxRadius, float stretch) {
  const float x = frac(u) * frequency, y = frac(v) * frequency;
  const unsigned i = unsigned(x), j = unsigned(y);
  const float pick = lattice(seed, i, j);
  if (pick >= density) return 0.f;
  const float px = frac(x), py = frac(y);
  const float dx = px - mix(.36f, .64f, lattice(seed + 1, i, j));
  const float dy = py - mix(.36f, .64f, lattice(seed + 2, i, j));
  const float size = lattice(seed + 3, i, j);
  const float radius = mix(minRadius, maxRadius, size * size);
  const float turn = lattice(seed + 4, i, j) - .5f;
  const float ax = dx + dy * turn, ay = (dy - dx * turn) * stretch;
  const float distance = ax * ax + ay * ay;
  if (distance >= radius * radius * 1.6f) return 0.f;
  const float edge = valueNoise(seed + 5, frequency * 5, u, v);
  const float outline = radius * mix(.48f, 1.31f, edge);
  const float border = std::min({px, 1.f - px, py, 1.f - py});
  return (1.f - softThreshold(outline * outline * .59f, outline * outline, distance)) *
    softThreshold(0.f, .055f, border) * mix(.55f, 1.f, lattice(seed + 6, i, j));
}

inline float sawnGrain(int frequency, uint64_t seed, float u, float v) {
  const float broad = valueNoise(seed, 5, 3, u, v);
  const float warp = .055f * (valueNoise(seed + 1, 5, 2, u, v) - .5f);
  const int columns = 5, rows = 4;
  const float x = frac(u) * columns, y = frac(v) * rows;
  float bend = 0.f, split = 0.f;
  for (int j = int(y) - 1; j <= int(y) + 1; ++j) {
    const unsigned col = unsigned(x), row = wrap(j, rows);
    if (lattice(seed + 2, col, row) < .59f) continue;
    const float dx = frac(x) - mix(.36f, .64f, lattice(seed + 3, col, row));
    const float dy = y - j - mix(.27f, .72f, lattice(seed + 4, col, row));
    const float influence = (1.f - softThreshold(.04f, .35f, std::abs(dx))) *
      (1.f - softThreshold(.05f, .65f, std::abs(dy)));
    const float knotWidth = mix(.047f, .105f, lattice(seed + 5, col, row));
    bend += .032f * influence * std::clamp(dx * 14.f, -1.f, 1.f);
    const float knot = (1.f - softThreshold(.25f, 1.f,
      dx * dx / (knotWidth * knotWidth) + dy * dy * 75.f));
    const float check = (1.f - softThreshold(.006f, .024f, std::abs(dx + .028f * dy))) *
      (1.f - softThreshold(.05f, .38f, std::abs(dy)));
    split = std::max(split, std::max(knot, .64f * check));
  }
  const float grainU = u + warp + bend;
  const float drift = .46f * valueNoise(seed + 6, 8, 3, grainU, v);
  const float rings = .5f + .5f * cycleSine(grainU * frequency * 2.f + drift);
  const float bands = valueNoise(seed + 7, frequency, 5, grainU, v);
  const float fibers = valueNoise(seed + 8, frequency * 4, 17, grainU, v);
  const float latewood = softThreshold(.62f, .96f, rings) * mix(.35f, 1.f, bands);
  return unit(.57f + .29f * (broad - .5f) + .33f * (bands - .5f) +
    .32f * (fibers - .5f) - .27f * latewood - .43f * split);
}

inline float naturalGrain(GrainKind kind, int frequency, uint64_t seed, float u, float v) {
  u = frac(u);
  v = frac(v);
  if (kind == GrainKind::wood) return sawnGrain(frequency, seed, u, v);
  if (kind == GrainKind::marble) {
    const float a = valueNoise(seed, 3, 4, u, v);
    const float b = valueNoise(seed + 1, 8, 9, u, v);
    const float c = valueNoise(seed + 2, 23, 17, u, v);
    const float strata = u + v + .78f * (a - .5f) + .29f * (b - .5f) + .058f * (c - .5f);
    const float branch = strata + .27f * (b - .33f);
    const float main = 1.f - softThreshold(.008f, mix(.030f, .095f, b), std::abs(frac(strata) - .5f));
    const float hairline = 1.f - softThreshold(.002f, .023f, std::abs(frac(branch + .055f * a) - .5f));
    const float cloud = valueNoise(seed + 3, 6, 8, u, v);
    return unit(.67f + .34f * (cloud - .5f) - .57f * main - .29f * hairline);
  }
  if (kind != GrainKind::mineral && kind != GrainKind::stone &&
      kind != GrainKind::brushed && kind != GrainKind::fibers) {
    return materialGrain(kind, frequency, seed, u, v);
  }
  const int coarse = std::max(3, frequency / 2);
  const float broad = valueNoise(seed, 4, 5, u, v);
  const float mids = gradientNoise(seed + 1, coarse, u, v);
  const float fine = valueNoise(seed + 2, frequency * 4, u, v);
  const float specks = mineralMarks(seed + 3, frequency * 2, .76f, u, v, .11f, .28f, 1.14f);
  if (kind == GrainKind::mineral || kind == GrainKind::stone) {
    const float aggregate = mineralMarks(seed + 9, frequency, .44f, u, v, .09f, .26f, 1.07f);
    const float stone = kind == GrainKind::stone ? 1.f : .62f;
    return unit(.49f + .46f * (broad - .5f) + .36f * (mids - .5f) +
      .27f * (fine - .5f) + stone * (.23f * aggregate - .32f * specks));
  }
  if (kind == GrainKind::brushed) {
    const float mill = valueNoise(seed + 12, 5, frequency * 4, u, v);
    return unit(.51f + .36f * (broad - .5f) + .26f * (mill - .5f) + .11f * (fine - .5f) - .15f * specks);
  }
  const float imprint = valueNoise(seed + 13, frequency * 2, 4, u + .023f * broad, v);
  return unit(.48f + .46f * (broad - .5f) + .32f * (imprint - .5f) + .16f * (fine - .5f) - .20f * specks);
}

inline float naturalWear(WearKind kind, int frequency, float density, uint64_t seed, float u, float v) {
  u = frac(u);
  v = frac(v);
  const float exposure = valueNoise(seed, 3, 5, u, v);
  if (kind == WearKind::pits || kind == WearKind::chips) {
    const bool chipped = kind == WearKind::chips;
    const float p = density * mix(.38f, 1.f, exposure);
    const float large = mineralMarks(seed + 9, frequency, p * .57f, u, v, .08f, .30f, 1.25f);
    const float fine = mineralMarks(seed + 17, frequency * 3, p, u, v, .06f, .24f, 1.45f);
    if (!chipped) return std::max(.76f * large, .58f * fine);

    const int zones = std::max(3, frequency / 3);
    const float wiggle = .62f / frequency;
    const float x = u + wiggle * (valueNoise(seed + 24, frequency * 2, u, v) - .5f);
    const float y = v + wiggle * (valueNoise(seed + 25, frequency * 2, u, v) - .5f);
    const float adhesion = .59f * valueNoise(seed + 26, zones, x, y) +
      .26f * valueNoise(seed + 27, frequency * 2, x, y) +
      .15f * valueNoise(seed + 28, frequency * 6, x, y);
    const float threshold = 1.f - .387f * density;
    const float flake = softThreshold(threshold, threshold + .068f, adhesion);
    return unit(std::max(.64f * large, .69f * fine) + flake);
  }
  if (kind == WearKind::deposits) {
    const float winding = .027f * (valueNoise(seed + 30, frequency, u, v) - .5f);
    const float stain = .62f * valueNoise(seed + 31, std::max(3, frequency / 2), u + winding, v) +
      .25f * valueNoise(seed + 32, frequency * 2, u, v + winding) +
      .13f * valueNoise(seed + 33, frequency * 6, u, v);
    return softThreshold(.93f - .48f * density, 1.12f - .41f * density, stain) * density;
  }
  if (kind == WearKind::runoff) {
    const float mineral = valueNoise(seed + 8, frequency * 2, 4, u + .012f * exposure, v);
    const float rivulet = valueNoise(seed + 10, frequency * 5, 3, u + .009f * exposure, v);
    const float leading = softThreshold(.76f - .29f * density, .91f - .29f * density,
      .47f * exposure + .53f * mineral);
    return leading * mix(.29f, 1.f, rivulet) * density;
  }
  return surfaceWear(kind, frequency, density, seed, u, v);
}

inline float roundedStone(uint64_t seed, int period, float mortar, float roundness, float irregularity,
                          float u, float v) {
  u = frac(u);
  v = frac(v);
  const float drift = (.08f + .17f * irregularity) / period;
  const float x = frac(u + drift * (valueNoise(seed + 1, period * 3, u, v) - .5f)) * period;
  const float y = frac(v + drift * (valueNoise(seed + 2, period * 3, u, v) - .5f)) * period;
  const int ix = int(x), iy = int(y);
  float closest = 8.f, second = 8.f, center = 0.f, shade = 0.f;
  for (int j = iy - 2; j <= iy + 2; ++j) {
    for (int i = ix - 2; i <= ix + 2; ++i) {
      const unsigned a = wrap(i, period), b = wrap(j, period);
      const float dx = i + mix(.14f, .86f, lattice(seed, a, b)) - x;
      const float dy = j + mix(.19f, .81f, lattice(seed + 3, a, b)) - y;
      const float distance = dx * dx + dy * dy;
      const float score = distance - .17f * irregularity * lattice(seed + 4, a, b);
      if (score < closest) {
        second = closest;
        closest = score;
        center = distance;
        shade = lattice(seed + 5, a, b);
      } else second = std::min(second, score);
    }
  }
  const float shoulders = mix(.075f, .18f, roundness);
  const float body = softThreshold(mortar * .20f, mortar + shoulders, second - closest);
  const float silhouette = 1.f - roundness * .52f * softThreshold(.32f, .82f, center);
  return .07f + body * silhouette * (.58f + .30f * shade - .19f * roundness * unit(center));
}

inline float handLaidBrick(const Node& node, uint64_t seed, float u, float v, float variation) {
  u = frac(u);
  v = frac(v);
  const int columns = int(node.p[0]), rows = int(node.p[1]);
  const bool flemish = node.spec->kernel == Kernel::flemish;
  const float uneven = node.p[5];
  const float warpY = valueNoise(seed + 1, std::max(3, columns), std::max(2, rows / 2), u, v);
  const float warpX = valueNoise(seed + 2, columns, rows * 2, u, v);
  const float row = v * rows + .24f * uneven * (warpY - .5f);
  const int course = int(std::floor(row)), rowId = wrap(course, rows);
  const float rowOffset = .21f * (lattice(seed + 3, rowId, 0) - .5f);
  const float column = u * columns + (rowId % 2 ? .5f : 0.f) +
    uneven * (.10f * (warpX - .5f) + rowOffset);
  const int columnId = wrap(int(std::floor(column)), columns);
  const float a = frac(column), b = frac(row);
  const float header = 1.f / 3.f + uneven * .18f * (lattice(seed + 4, columnId, rowId) - .5f);
  const float side = flemish ? std::min({a, 1.f - a, std::abs(a - header)}) : std::min(a, 1.f - a);
  const float bed = std::min(b, 1.f - b);
  const float edge = std::min(side * 1.7f, bed);
  const float grit = valueNoise(seed + 5, columns * 12, rows * 7, u, v);
  const float rim = valueNoise(seed + 7, columns * 5, rows * 3, u, v);
  const float erosion = uneven * (.085f * softThreshold(.63f, .94f, rim) +
    std::max(0.f, grit - .63f) * .045f + .023f * (warpX - .5f));
  const float width = node.p[2] * mix(.68f, 1.37f, warpX);
  const float bevel = softThreshold(width * .43f, width * .43f + node.p[3], edge - erosion);
  const unsigned brick = unsigned(columnId) * 2 + unsigned(flemish && a > header);
  const float tint = lattice(seed + 6, brick, rowId);
  const float clay = .63f + .33f * tint + .065f * (grit - .5f) - node.p[4] * .31f * variation;
  return mix(.065f + .07f * grit, clay, bevel);
}

struct JointSpan {
  int index;
  float start, end;
};

inline JointSpan straightSpan(float position, int count, uint64_t seed, unsigned lane, float offset) {
  const auto boundary = [&](int index) {
    return float(index) + offset * (lattice(seed, wrap(index, count), lane) - .5f);
  };
  int index = int(std::floor(position));
  if (position < boundary(index)) --index;
  else if (position >= boundary(index + 1)) ++index;
  return {index, boundary(index), boundary(index + 1)};
}

inline Pixel brickLayout(const Node& node, uint64_t seed, float u, float v, float variation) {
  const int columns = int(node.p[0]), rows = int(node.p[1]);
  const float uneven = node.p[5];
  const float y = frac(v) * rows;
  // A joint has one position along the entire course and is shared by both courses.
  const auto course = straightSpan(y, rows, seed + 1, 0, .075f * uneven);
  const unsigned row = wrap(course.index, rows);
  const float x = frac(u) * columns + (row % 2 ? .5f : 0.f);
  const auto brick = straightSpan(x, columns, seed + 2, row, .18f * uneven);
  const unsigned column = wrap(brick.index, columns);
  float start = brick.start, end = brick.end;
  bool stretcher = false;
  if (node.spec->kernel == Kernel::flemishLayout) {
    const float header = mix(start, end, 1.f / 3.f + .064f * uneven * (lattice(seed + 3, column, row) - .5f));
    stretcher = x >= header;
    if (stretcher) start = header;
    else end = header;
  }
  const float side = std::min(x - start, end - x) * float(rows) / columns;
  const float bed = std::min(y - course.start, course.end - y);
  const float edge = std::min(side, bed);
  const float grit = valueNoise(seed + 4, columns * 16, rows * 9, u, v);
  const float chip = valueNoise(seed + 5, columns * 9, rows * 6, u, v);
  const float corner = 1.f - softThreshold(.025f, .14f, std::max(side, bed));
  const float worn = uneven * (.034f * corner + .042f * softThreshold(.68f, .95f, chip) + .019f * (grit - .5f));
  const float joint = node.p[2] * .5f;
  const float lip = edge - joint - worn;
  const float face = softThreshold(-.009f, .009f, lip);
  const float raised = softThreshold(-.006f, node.p[3], lip);
  const float pigment = lattice(seed + 6, column * 2 + unsigned(stretcher), row);
  const float height = mix(.07f + .042f * grit, .88f + .055f * (grit - .5f) +
    .060f * (pigment - .5f) - .12f * node.p[4] * variation, raised);
  return {height, face, pigment, 1.f};
}

struct StoneFace {
  float depth;
  float grain;
  float erosion;
};

inline float stoneFissure(uint64_t seed, float u, float v) {
  float fracture = 0.f;
  for (unsigned strike = 0; strike < 2; ++strike) {
    if (lattice(seed, strike, 0) < .23f) continue;
    const float x = u - mix(.20f, .80f, lattice(seed, strike, 1));
    const float y = v - mix(.23f, .77f, lattice(seed, strike, 2));
    const float length = mix(.11f, .24f, lattice(seed, strike, 3));
    const float slope = mix(.65f, 1.95f, lattice(seed, strike, 4)) *
      (lattice(seed, strike, 5) < .5f ? -1.f : 1.f);
    const float across = y - x * slope - .28f * std::max(0.f, x);
    const float taper = 1.f - softThreshold(length * .65f, length, std::abs(x));
    const float line = 1.f - softThreshold(.003f, .013f, std::abs(across));
    const float chippedSide = softThreshold(.004f, .025f, across) *
      (1.f - softThreshold(.032f, .091f, across));
    fracture = std::max(fracture, taper * (line * .68f + chippedSide * .32f));
  }
  return fracture;
}

inline StoneFace stoneBrickFace(uint64_t seed, float u, float v, float aspect) {
  const int across = std::clamp(int(aspect * 4.f + .5f), 4, 24);
  const float warp = .087f * (valueNoise(seed, across, 5, u, v) - .5f);
  const float x = u + warp / aspect, y = v - .69f * warp;
  const float coarse = valueNoise(seed + 1, across, 5, x, y);
  const float bed = valueNoise(seed + 2, across, 13, x, y + .085f * coarse);
  const float broken = valueNoise(seed + 3, across * 2, 9, x, y);
  const float sand = valueNoise(seed + 4, across * 7, 34, u, v);
  const float spall = softThreshold(.58f, .85f, .58f * coarse + .42f * broken);
  const float pores = mineralMarks(seed + 5, 8, .64f, 1.5f * x, y, .10f, .31f, .97f);
  const float pinholes = mineralMarks(seed + 12, 21, .80f, 1.5f * x, y, .095f, .29f, 1.20f);
  const float embedded = mineralMarks(seed + 19, 14, .56f, x, y, .09f, .28f, 1.08f);
  const float cleft = stoneFissure(seed + 27, x, y);
  const float mottling = softThreshold(.31f, .72f, .72f * coarse + .28f * broken);
  const float depth = .38f * (coarse - .5f) + .23f * (bed - .5f) + .20f * (sand - .5f) -
    .47f * spall - .39f * pores - .20f * pinholes + .19f * embedded - .21f * cleft;
  const float grain = unit(.24f + .67f * mottling + .24f * (bed - .5f) + .31f * (sand - .5f) -
    .37f * spall - .48f * pores - .27f * pinholes + .25f * embedded - .29f * cleft);
  return {depth, grain, .58f * spall + .27f * pores + .12f * pinholes + .13f * (broken - .5f)};
}

inline Pixel stoneBrickLayout(const Node& node, uint64_t seed, float u, float v, float variation) {
  const int columns = int(node.p[0]), rows = int(node.p[1]);
  const float irregularity = node.p[5], wear = node.p[4];
  const float y = frac(v) * rows;
  const auto course = straightSpan(y, rows, seed + 1, 0, .27f * irregularity);
  const unsigned row = wrap(course.index, rows);
  const float offset = (row % 2 ? .5f : 0.f) + .29f * irregularity * (lattice(seed + 2, 0, row) - .5f);
  const float x = frac(u) * columns + offset;
  const auto block = straightSpan(x, columns, seed + 3, row, .69f * irregularity);
  const unsigned column = wrap(block.index, columns);
  const uint64_t faceSeed = seed ^ (UINT64_C(0x94d049bb133111eb) * (1 + column + columns * row));

  const float localX = (x - block.start) / (block.end - block.start);
  const float localY = (y - course.start) / (course.end - course.start);
  const float width = float(rows) / columns;
  const float side = std::min(x - block.start, block.end - x) * width;
  const float bed = std::min(y - course.start, course.end - y);
  const float radius = node.p[3] * mix(.45f, 1.21f, lattice(faceSeed + 1, 0, 0));
  const float cornerX = std::max(0.f, radius - side), cornerY = std::max(0.f, radius - bed);
  const float rounded = std::min(side, bed) -
    (std::sqrt(cornerX * cornerX + cornerY * cornerY) - std::max(cornerX, cornerY));
  const float aspect = (block.end - block.start) * width / (course.end - course.start);
  const auto surface = stoneBrickFace(faceSeed + 20, localX, localY, std::max(.5f, aspect));
  const bool upright = side < bed;
  const unsigned edge = upright ? unsigned(localX > .5f) : 2u + unsigned(localY > .5f);
  const float along = upright ? localY : localX;
  const float chipCenter = mix(.14f, .86f, lattice(faceSeed + 8, edge, 0));
  const float chipWidth = mix(.037f, .134f, lattice(faceSeed + 8, edge, 1));
  const float chipDepth = .31f * softThreshold(.18f, .89f, lattice(faceSeed + 8, edge, 2));
  const float chip = chipDepth * (1.f - softThreshold(.08f, 1.f, std::abs(along - chipCenter) / chipWidth));
  const float chips = wear * (chip + .13f * surface.erosion + .046f * (variation - .5f));
  const float lip = rounded - node.p[2] * .5f - chips;
  const float coverage = softThreshold(-.011f, .012f, lip);
  const float bevel = node.p[3] * mix(.68f, 1.35f, lattice(faceSeed + 2, 0, 0));
  const float shoulder = softThreshold(-.017f, bevel, lip);
  const float piece = lattice(faceSeed + 3, 0, 0);
  const float tilt = (localX - .5f) * (lattice(faceSeed + 4, 0, 0) - .5f) +
    (localY - .5f) * (lattice(faceSeed + 5, 0, 0) - .5f);
  const float top = .79f + .16f * (piece - .5f) + .20f * tilt + (.38f + .18f * wear) * surface.depth;
  const float mortar = .11f + .043f * valueNoise(seed + 7, columns * 22, rows * 15, u, v);
  return {unit(mix(mortar, top, shoulder)), coverage, piece, surface.grain};
}

inline float plankFiber(uint64_t seed, float u, float v) {
  const float knotX = mix(.20f, .80f, lattice(seed, 0, 0));
  const float knotY = mix(.14f, .86f, lattice(seed + 1, 0, 0));
  const float dx = u - knotX, dy = v - knotY;
  const float d = dx * dx * 22.f + dy * dy * 180.f;
  const float knot = softThreshold(.23f, .64f, lattice(seed + 2, 0, 0));
  const float bend = knot * .078f * (1.f - softThreshold(.12f, 1.1f, d)) *
    std::clamp(dx * 16.f, -1.f, 1.f);
  const float run = valueNoise(seed + 3, 6, 2, u, v);
  const float fiberU = u + bend + .025f * (run - .5f);
  const float seasonal = valueNoise(seed + 4, 17, 3, fiberU, v);
  const float latewood = valueNoise(seed + 5, 42, 5, fiberU, v);
  const float etched = softThreshold(.58f, .91f, .70f * latewood + .30f * seasonal);
  const float fine = valueNoise(seed + 6, 97, 14, fiberU, v);
  const float heart = (1.f - softThreshold(.05f, .40f, d)) * knot;
  const float split = (1.f - softThreshold(.004f, .018f, std::abs(dx + .10f * dy))) *
    (1.f - softThreshold(.026f, .16f, std::abs(dy))) * knot;
  return unit(.48f + .43f * (seasonal - .5f) + .25f * (fine - .5f) -
    .24f * etched - .38f * heart - .22f * split);
}

inline Pixel boardLayout(const Node& node, uint64_t seed, float u, float v) {
  const int columns = int(node.p[0]), seams = int(node.p[1]);
  const float x = frac(u) * columns;
  const auto plank = straightSpan(x, columns, seed + 1, 0, .20f * node.p[3]);
  const unsigned board = wrap(plank.index, columns);
  const float width = node.p[2];
  const float edge = std::min(x - plank.start, plank.end - x);
  const float fibers = valueNoise(seed + 2, columns * 12, 17, u, v);
  const float abrasion = node.p[3] * width * .18f * (fibers - .5f);
  const float lip = edge - width * .25f - abrasion;
  float face = softThreshold(-width * .08f, width * .12f, lip);
  float raised = softThreshold(-width * .08f, width * .62f, lip);
  unsigned length = 0;
  float along = frac(v);
  if (seams > 0) {
    const float y = along * seams + lattice(seed + 3, board, 0);
    length = wrap(int(std::floor(y)), seams);
    along = frac(y);
    const float endLip = std::min(along, 1.f - along) - width * .049f;
    face *= softThreshold(-width * .027f, width * .032f, endLip);
    raised *= softThreshold(-width * .027f, width * .16f, endLip);
  }
  const float pigment = lattice(seed + 4, board, length);
  const float localU = (x - plank.start) / (plank.end - plank.start);
  const uint64_t grainSeed = seed ^ (UINT64_C(0xa24baed4963ee407) * (1 + board + columns * length));
  const float grain = seams == 0 ? fibers : plankFiber(grainSeed, localU, along);
  const float height = mix(.09f, .88f + .09f * (pigment - .5f) + .039f * (grain - .5f), raised);
  return {height, face, pigment, grain};
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
    if (const auto existing = indices.find(key); existing != indices.end()) {
      entries.splice(entries.begin(), entries, existing->second);
      return;
    }
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
  case Kernel::materialGrain: {
    const auto kind = GrainKind(int(node.p[0]));
    const int frequency = int(node.p[1]);
    if (node.p[2] == 1.f) return scalar(naturalGrain(kind, frequency, seed, u, v));
    const float original = materialGrain(kind, frequency, seed, u, v);
    return scalar(node.p[2] > 0.f ? mix(original, naturalGrain(kind, frequency, seed, u, v), node.p[2]) : original);
  }
  case Kernel::surfaceWear: {
    const auto kind = WearKind(int(node.p[0]));
    const int frequency = int(node.p[1]);
    if (node.p[3] == 1.f) return scalar(naturalWear(kind, frequency, node.p[2], seed, u, v));
    const float original = surfaceWear(kind, frequency, node.p[2], seed, u, v);
    return scalar(node.p[3] > 0.f ? mix(original, naturalWear(kind, frequency, node.p[2], seed, u, v), node.p[3]) : original);
  }
  case Kernel::brickLayout:
  case Kernel::flemishLayout: return brickLayout(node, seed, u, v, input[0]->sample(x, y)[0]);
  case Kernel::stoneBrickLayout: return stoneBrickLayout(node, seed, u, v, input[0]->sample(x, y)[0]);
  case Kernel::boardLayout: return boardLayout(node, seed, u, v);
  case Kernel::boardJoints: {
    if (node.p[3] > 0.f) {
      const int columns = int(node.p[0]);
      const float wander = valueNoise(seed + 4, columns, 5, u, v);
      const float column = u * columns + node.p[3] * .37f * (wander - .5f);
      const unsigned board = wrap(int(std::floor(column)), columns);
      const float edge = std::min(frac(column), 1.f - frac(column));
      const float knots = valueNoise(seed + 5, columns * 4, 15, u, v);
      const float width = node.p[2] * mix(.58f, 1.59f, knots);
      float height = softThreshold(width * .23f, width, edge);
      if (node.p[1] > 0.f) {
        const float seam = std::abs(frac(v * node.p[1] + lattice(seed, board, 0) +
          .055f * node.p[3] * (wander - .5f)) - .5f);
        height *= softThreshold(width * .065f, width * .21f, seam);
      }
      const float grain = valueNoise(seed + 6, columns * 9, 4, u, v);
      return scalar(.08f + height * (.76f + .16f * lattice(seed + 2, board, 0) + .10f * (grain - .5f)));
    }
    const float column = u * node.p[0];
    const unsigned board = unsigned(wrap(int(column), int(node.p[0])));
    const float edge = std::min(frac(column), 1.f - frac(column));
    const float width = node.p[2] * mix(.74f, 1.34f, valueNoise(seed + 1, 11, u, v));
    float height = softThreshold(width * .32f, width, edge);
    if (node.p[1] > 0) {
      const float seam = std::abs(frac(v * node.p[1] + lattice(seed, board, 0)) - .5f);
      height *= softThreshold(width * .10f, width * .23f, seam);
    }
    return scalar(height * mix(.65f, 1.f, lattice(seed + 2, board, 0)));
  }
  case Kernel::stonework: {
    const int period = int(node.p[0]);
    if (node.p[3] > 0.f) return scalar(roundedStone(seed, period, node.p[1], node.p[2], node.p[3], u, v));
    const float drift = .28f / period;
    const float x = u + drift * (valueNoise(seed + 1, period * 3, u, v) - .5f);
    const float y = v + drift * (valueNoise(seed + 2, period * 3, u, v) - .5f);
    const auto distances = cellularDistances(seed, period, .71f, x, y);
    const float gap = distances[1] - distances[0];
    // A rounded cap spreads the bevel into the stone. Rubble retains more angular edges.
    const float bevel = mix(.10f, .25f, node.p[2]);
    const float body = softThreshold(node.p[1] * .4f, node.p[1] + bevel, gap);
    const float dome = 1.f - .38f * node.p[2] * unit(distances[0]);
    return scalar(.06f + body * dome * .94f);
  }
  case Kernel::archBand: {
    const float vx = u - .5f, vy = .79f - v;
    const float arch = std::abs(std::sqrt(vx * vx + vy * vy) - .306f);
    const float profile = 1.f - softThreshold(node.p[1] * .28f, node.p[1] * .54f, arch);
    // Approximate arc length in each octant without platform-dependent atan2 rounding.
    const float x = std::abs(vx), y = std::max(0.f, vy);
    const float ratio = std::min(x, y) / std::max(.00001f, std::max(x, y));
    const float octant = ratio * (.25f + .087f * (1.f - ratio));
    const float quadrant = x >= y ? octant : .5f - octant;
    const float stone = (vx < 0 ? 1.f - quadrant : quadrant) * node.p[0];
    const float joint = softThreshold(.015f, .085f, std::min(frac(stone), 1.f - frac(stone)));
    const float cut = softThreshold(-.005f, .025f, vy);
    const float material = mix(.64f, .95f, lattice(seed, unsigned(stone), 0));
    return scalar(.09f + .91f * profile * cut * joint * material);
  }
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
    const float phase = input[0]->sample(x, y)[0] * node.p[1];
    const int patch = int(node.p[0]);
    if (node.p[3] == 4) {
      const float drift = gradientNoise(seed, 4, u, v) * node.p[2];
      const float longWave = cycleSine(u * patch + v * 3.f - phase + drift);
      const float crossWave = cycleSine(u * 2.f - v * (patch + 2.f) + phase * 2.f - drift);
      const float ripples = cycleSine(u * (patch * 3.f) + v * 5.f + phase * 3.f + .34f * (longWave + crossWave));
      return scalar(.5f + .18f * longWave + .11f * crossWave + .084f * ripples);
    }
    if (node.p[3] == 1) {
      const float position = frac(u - phase);
      const float large = gradientNoise(seed, patch, position, v);
      const float fine = gradientNoise(seed + 1, patch * 2, u, frac(v + phase));
      return scalar(1.f - std::abs(large * 2 - 1.f + (fine - .5f) * node.p[2]));
    }
    if (node.p[3] == 2) {
      const float sway = .11f * node.p[2] * cycleSine(v + frac(phase));
      const auto cells = cellularDistances(seed, patch, .78f, frac(u + sway), frac(v - phase));
      return scalar((cells[1] - cells[0]) * 2.1f);
    }
    if (node.p[3] == 3) {
      const float drift = gradientNoise(seed, 4, u, v) * node.p[2];
      const float signal = cycleSine(u * patch + drift + phase);
      const float pulse = .72f + .28f * cycleSine(phase);
      return scalar(pulse * (.53f + .47f * signal));
    }
    const float perturbation = node.p[2] * cycleSine(v * node.p[0] - phase);
    return scalar(.5f + .5f * cycleSine(u * node.p[0] + phase + perturbation));
  }
  case Kernel::bricks:
  case Kernel::flemish: {
    if (node.p[5] > 0.f) return scalar(handLaidBrick(node, seed, u, v, input[0]->sample(x, y)[0]));
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
  case Kernel::scalarBlend:
    return scalar(mix(input[0]->sample(x, y)[0], input[1]->sample(x, y)[0], input[2]->sample(x, y)[0]));
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
    const int radius = std::max(1, int(node.p[1]));
    const float height = input[0]->sample(x, y)[0];
    const float sides = input[0]->sample(x - radius, y)[0] + input[0]->sample(x + radius, y)[0];
    const float ends = input[0]->sample(x, y - radius)[0] + input[0]->sample(x, y + radius)[0];
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
    case Kernel::edge: return 1;
    case Kernel::ao: return std::max(1, int(node.p[1]));
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
    "resolution", "The render size is outside the 1Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р Р†РІР‚С›РЎС›Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р В РІР‚В Р В Р’В Р Р†Р вЂљРЎв„ўР В Р вЂ Р Р†Р вЂљРЎвЂєР РЋРЎвЂєР В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р Р†РІР‚С›РЎС›Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р Р†РІР‚С›РЎС›Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р В РІР‚В Р В Р’В Р Р†Р вЂљРЎв„ўР В Р Р‹Р Р†РІР‚С›РЎС›Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р Р†РІР‚С›РЎС›Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р Р†РІР‚С›РЎС›Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р В РІР‚В Р В Р’В Р Р†Р вЂљРЎв„ўР В Р вЂ Р Р†Р вЂљРЎвЂєР РЋРЎвЂєР В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р Р†РІР‚С›РЎС›Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р В РІР‚В Р В Р’В Р Р†Р вЂљРЎв„ўР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р’В Р Р†Р вЂљР’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р РЋРІвЂћСћР В Р’В Р В Р вЂ№Р В Р вЂ Р Р†Р вЂљРЎвЂєР РЋРЎвЂєР В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћвЂ“Р В Р’В Р вЂ™Р’В Р В Р’В Р Р†Р вЂљР’В Р В Р’В Р В РІР‚В Р В Р’В Р Р†Р вЂљРЎв„ўР В Р Р‹Р Р†Р вЂљРЎвЂќР В Р’В Р В Р вЂ№Р В Р Р‹Р Р†Р вЂљРЎвЂќР В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р Р†РІР‚С›РЎС›Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р’В Р Р†Р вЂљР’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р РЋРІвЂћСћР В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р Р†РІР‚С›РЎС›Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р В РІР‚В Р В Р’В Р Р†Р вЂљРЎв„ўР В Р Р‹Р Р†РІР‚С›РЎС›Р В Р’В Р вЂ™Р’В Р В Р’В Р В РІР‚в„–Р В Р’В Р В РІР‚В Р В Р вЂ Р В РІР‚С™Р РЋРІР‚С”Р В Р Р‹Р РЋРІР‚С”Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћСћР В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р вЂ Р В РІР‚С™Р Р†РІР‚С›РІР‚вЂњР В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В Р В Р’В Р вЂ™Р’В Р В Р’В Р Р†Р вЂљРІвЂћвЂ“Р В Р’В Р вЂ™Р’В Р В Р’В Р Р†Р вЂљР’В Р В Р’В Р В РІР‚В Р В Р’В Р Р†Р вЂљРЎв„ўР В Р Р‹Р Р†Р вЂљРЎвЂќР В Р’В Р В Р вЂ№Р В Р Р‹Р Р†Р вЂљРЎвЂќ8192 range.");
  require(threads > 0 && threads <= maxThreads, "threads", "Choose 1 to 16 CPU workers.");
  require(plan.frames > 0 && (plan.loop || frame < plan.frames), "frame", "Frame is outside the animation range.");
  const float time = plan.frames == 1 ? 0.f : plan.loop ? float(frame % plan.frames) / plan.frames
    : float(frame) / (plan.frames - 1);
  const size_t pixels = size_t(size.width) * size.height;
  const size_t imageBytes = pixels * sizeof(Pixel);
  require(!cache || (budget > 0 && cache->limit() <= (budget - 1) / 2),
    "assets", "The preview cache exceeds the render budget.");
  // A render can retain old hits while it replaces their entries with new images.
  if (cache) budget -= 2 * cache->limit();
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
  const auto checkCancel = [&] {
    if (cancelled && cancelled->load(std::memory_order_relaxed))
      throw Error("render", "Rendering was cancelled.");
  };
  std::vector<std::shared_ptr<const Image>> images(plan.nodes.size());
  std::vector<std::string> keys;
  if (cache) {
    keys.resize(plan.nodes.size());
    for (const int index : plan.order) {
      checkCancel();
      const auto& node = plan.nodes[index];
      const auto seed = stableSeed(plan.seed, node.seedIdentity.empty() ? node.id : node.seedIdentity);
      keys[index] = evalKey(node, seed, size, time, keys);
    }
  }
  std::vector<unsigned> uses(plan.nodes.size());
  for (const auto& [name, index] : plan.outputs) ++uses[index];
  // Find reusable results before inserting new entries. Walking backwards also
  // lets a cached result satisfy a branch without rendering its inputs again.
  for (auto it = plan.order.rbegin(); it != plan.order.rend(); ++it) {
    const int index = *it;
    if (!uses[index]) continue;
    if (cache) images[index] = cache->find(keys[index]);
    if (!images[index])
      for (const int input : plan.nodes[index].inputs) if (input >= 0) ++uses[input];
  }
  unsigned resident = 0, peak = 0;
  auto estimated = uses;
  for (int n : plan.order) {
    if (!uses[n] || images[n]) continue;
    peak = std::max(peak, ++resident);
    for (int in : plan.nodes[n].inputs) if (in >= 0 && !images[in] && !--estimated[in]) --resident;
  }
  require(!plan.outputs.empty() && peak <= budget / imageBytes, "resolution",
    "Evaluation exceeds its RAM budget. Reduce the size or select fewer channels.");

  constexpr unsigned tileSize = 128;
  const unsigned columns = (size.width + tileSize - 1) / tileSize;
  const unsigned tileCount = columns * ((size.height + tileSize - 1) / tileSize);
  const unsigned workers = std::min(threads, tileCount);
  for (int n : plan.order) {
    checkCancel();
    if (!uses[n] || images[n]) continue;
    const auto& node = plan.nodes[n];
    const auto seed = stableSeed(plan.seed, node.seedIdentity.empty() ? node.id : node.seedIdentity);
    const auto path = "graph.nodes." + node.id;
    {
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
