// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_model.h"
#include <queue>
namespace ftebroom::bb {
using Pixel = std::array<unsigned char, 4>;
enum class Blend { Normal, SetOpacity, Color, Behind, Multiply, Add, Screen, Difference };
inline Pixel pixel(const Texture &t, int x, int y) {
  Pixel p{};
  if (x < 0 || y < 0 || x >= t.width || y >= t.height)
    return p;
  std::copy_n(t.rgba.data() + (size_t(y) * t.width + x) * 4, 4, p.data());
  return p;
}
inline Pixel blend(Pixel dest, Pixel src, double opacity, Blend mode = Blend::Normal,
                   bool lockAlpha = false) {
  double sa = src[3] / 255. * std::clamp(opacity, 0., 1.), da = dest[3] / 255.,
         alpha = sa + da * (1 - sa);
  Pixel out = dest;
  Vec3 color{src[0] / 255., src[1] / 255., src[2] / 255.};
  if (mode == Blend::Color) {
    double luminance = .3 * color[0] + .59 * color[1] + .11 * color[2],
           target = (.3 * dest[0] + .59 * dest[1] + .11 * dest[2]) / 255.;
    for (auto &c : color)
      c += target - luminance;
    double lo = *std::min_element(color.begin(), color.end()),
           hi = *std::max_element(color.begin(), color.end());
    if (lo < 0)
      for (auto &c : color)
        c = target + (c - target) * target / std::max(1e-9, target - lo);
    if (hi > 1)
      for (auto &c : color)
        c = target + (c - target) * (1 - target) / std::max(1e-9, hi - target);
  }
  for (int c = 0; c < 3; c++) {
    double s = color[c], d = dest[c] / 255.;
    if (mode == Blend::Multiply)
      s *= d;
    else if (mode == Blend::Add)
      s = std::min(1., s + d);
    else if (mode == Blend::Screen)
      s = 1 - (1 - s) * (1 - d);
    else if (mode == Blend::Difference)
      s = std::abs(s - d);
    double value = alpha > 1e-9 ? (s * sa + d * da * (1 - sa)) / alpha : 0;
    if (mode == Blend::Behind)
      value = alpha > 1e-9 ? (d * da + s * sa * (1 - da)) / alpha : 0;
    out[c] = std::clamp(int(std::round(value * 255)), 0, 255);
  }
  out[3] = lockAlpha ? dest[3]
           : mode == Blend::SetOpacity
               ? std::clamp(int(dest[3] + (src[3] - dest[3]) * opacity), 0, 255)
               : std::clamp(int(std::round(alpha * 255)), 0, 255);
  return out;
}
struct PaintOptions {
  int size = 1;
  double opacity = 1, softness = 0;
  bool circle = false, eraser = false, lockAlpha = false, pixelPerfect = false;
  Blend mode = Blend::Normal;
  Pixel color{180, 130, 90, 255};
  std::array<int, 4> clip{0, 0, 2048, 2048};
  std::vector<Pixel> palette;
  std::function<bool(int, int)> mask;
};
inline Pixel quantize(Pixel p, const std::vector<Pixel> &palette) {
  if (palette.empty())
    return p;
  unsigned best = ~0u;
  Pixel result = p;
  for (auto q : palette) {
    unsigned d = 0;
    for (int c = 0; c < 3; c++) {
      int delta = int(p[c]) - q[c];
      d += delta * delta;
    }
    if (d < best) {
      best = d;
      result = q;
    }
  }
  result[3] = p[3];
  return result;
}
inline bool allowed(const Texture &t, int x, int y, const PaintOptions &o) {
  return x >= 0 && y >= 0 && x < t.width && y < t.height && x >= o.clip[0] && y >= o.clip[1] &&
         x < o.clip[2] && y < o.clip[3] && (!o.mask || o.mask(x, y));
}
inline void dab(Texture &t, int cx, int cy, const PaintOptions &o) {
  int size = std::clamp(o.size, 1, 50);
  double radius = size * .5;
  for (int y = cy - (size - 1) / 2; y <= cy + size / 2; y++)
    for (int x = cx - (size - 1) / 2; x <= cx + size / 2; x++) {
      if (!allowed(t, x, y, o))
        continue;
      double distance =
          std::sqrt(double((x - cx) * (x - cx) + (y - cy) * (y - cy))) / std::max(.5, radius);
      if (o.circle && distance > 1)
        continue;
      double weight =
          o.softness > 0 ? std::clamp((1 - distance) / std::max(.001, o.softness), 0., 1.) : 1;
      auto old = pixel(t, x, y);
      Pixel result;
      if (o.eraser) {
        result = old;
        if (!o.lockAlpha)
          result[3] = std::round(old[3] * (1 - o.opacity * weight));
      } else
        result = quantize(blend(old, o.color, o.opacity * weight, o.mode, o.lockAlpha), o.palette);
      std::copy(result.begin(), result.end(), t.rgba.begin() + (size_t(y) * t.width + x) * 4);
    }
  t.saved = false;
  t.revision++;
}
inline void stroke(Texture &t, int x0, int y0, int x1, int y1, const PaintOptions &o) {
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1,
      err = dx + dy;
  for (;;) {
    dab(t, x0, y0, o);
    if (x0 == x1 && y0 == y1)
      break;
    int e = 2 * err;
    if (e >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}
inline void flood(Texture &t, int x, int y, const PaintOptions &o, int tolerance = 0,
                  bool connected = true) {
  if (!allowed(t, x, y, o))
    return;
  auto source = pixel(t, x, y);
  auto matches = [&](int xx, int yy) {
    auto p = pixel(t, xx, yy);
    for (int i = 0; i < 4; i++)
      if (std::abs(int(p[i]) - source[i]) > tolerance)
        return false;
    return true;
  };
  std::vector<unsigned char> seen(size_t(t.width) * t.height);
  std::queue<std::pair<int, int>> q;
  if (connected)
    q.push({x, y});
  else
    for (int yy = 0; yy < t.height; yy++)
      for (int xx = 0; xx < t.width; xx++)
        if (allowed(t, xx, yy, o) && matches(xx, yy))
          q.push({xx, yy});
  while (!q.empty()) {
    auto [xx, yy] = q.front();
    q.pop();
    if (!allowed(t, xx, yy, o) || seen[size_t(yy) * t.width + xx] || !matches(xx, yy))
      continue;
    seen[size_t(yy) * t.width + xx] = 1;
    auto result =
        quantize(blend(pixel(t, xx, yy), o.color, o.opacity, o.mode, o.lockAlpha), o.palette);
    std::copy(result.begin(), result.end(), t.rgba.begin() + (size_t(yy) * t.width + xx) * 4);
    if (connected) {
      q.push({xx + 1, yy});
      q.push({xx - 1, yy});
      q.push({xx, yy + 1});
      q.push({xx, yy - 1});
    }
  }
  t.saved = false;
  t.revision++;
}
inline void gradient(Texture &t, Vec2 start, Vec2 end, Pixel first, Pixel second,
                     const PaintOptions &o) {
  double dx = end[0] - start[0], dy = end[1] - start[1], den = dx * dx + dy * dy;
  for (int y = 0; y < t.height; y++)
    for (int x = 0; x < t.width; x++)
      if (allowed(t, x, y, o)) {
        double a =
            den > 0 ? std::clamp(((x - start[0]) * dx + (y - start[1]) * dy) / den, 0., 1.) : 0;
        Pixel color;
        for (int c = 0; c < 4; c++)
          color[c] = std::round(first[c] * (1 - a) + second[c] * a);
        auto result =
            quantize(blend(pixel(t, x, y), color, o.opacity, o.mode, o.lockAlpha), o.palette);
        std::copy(result.begin(), result.end(), t.rgba.begin() + (size_t(y) * t.width + x) * 4);
      }
  t.saved = false;
  t.revision++;
}
inline void shape(Texture &t, int x0, int y0, int x1, int y1, const PaintOptions &o, bool ellipse,
                  bool filled) {
  if (x0 > x1)
    std::swap(x0, x1);
  if (y0 > y1)
    std::swap(y0, y1);
  double rx = (x1 - x0 + 1) * .5, ry = (y1 - y0 + 1) * .5, cx = (x0 + x1) * .5, cy = (y0 + y1) * .5;
  auto brush = o;
  brush.size = 1;
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      bool in = true,
           edge = x < x0 + o.size || x > x1 - o.size || y < y0 + o.size || y > y1 - o.size;
      if (ellipse) {
        double r = std::pow((x - cx) / rx, 2) + std::pow((y - cy) / ry, 2);
        in = r <= 1;
        double innerRx = std::max(.1, rx - o.size), innerRy = std::max(.1, ry - o.size);
        edge = std::pow((x - cx) / innerRx, 2) + std::pow((y - cy) / innerRy, 2) >= 1;
      }
      if (in && (filled || edge))
        dab(t, x, y, brush);
    }
}
} // namespace ftebroom::bb
