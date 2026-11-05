// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_geometry.h"
#include "ftebroom_bb_paint.h"
namespace ftebroom::bb {
inline Vec3 rgbHsv(Pixel p) {
  double r = p[0] / 255., g = p[1] / 255., b = p[2] / 255., hi = std::max({r, g, b}),
         lo = std::min({r, g, b}), d = hi - lo, h = 0;
  if (d > 1e-9)
    h = hi == r ? std::fmod((g - b) / d + 6, 6) : hi == g ? (b - r) / d + 2 : (r - g) / d + 4;
  return {h / 6, hi > 0 ? d / hi : 0, hi};
}
inline Pixel hsvRgb(Vec3 c, unsigned char alpha = 255) {
  double h = c[0] - std::floor(c[0]), s = std::clamp(c[1], 0., 1.), v = std::clamp(c[2], 0., 1.);
  int i = int(h * 6);
  double f = h * 6 - i, p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
  Vec3 rgb;
  switch (i % 6) {
  case 0:
    rgb = {v, t, p};
    break;
  case 1:
    rgb = {q, v, p};
    break;
  case 2:
    rgb = {p, v, t};
    break;
  case 3:
    rgb = {p, q, v};
    break;
  case 4:
    rgb = {t, p, v};
    break;
  default:
    rgb = {v, p, q};
  }
  return {(unsigned char)std::round(rgb[0] * 255), (unsigned char)std::round(rgb[1] * 255),
          (unsigned char)std::round(rgb[2] * 255), alpha};
}
inline void adjustImage(Texture &t, double brightness, double contrast, double hue,
                        double saturation, double opacity) {
  for (size_t i = 0; i < t.rgba.size(); i += 4) {
    Pixel p;
    std::copy_n(t.rgba.data() + i, 4, p.data());
    for (int a = 0; a < 3; a++)
      p[a] = std::clamp(int(std::round(((p[a] / 255. - .5) * contrast + .5 + brightness) * 255)), 0,
                        255);
    auto hsv = rgbHsv(p);
    hsv[0] += hue;
    hsv[1] *= saturation;
    p = hsvRgb(hsv, std::clamp(int(p[3] * opacity), 0, 255));
    std::copy(p.begin(), p.end(), t.rgba.begin() + i);
  }
  t.revision++;
  t.saved = false;
}
inline Texture cropImage(const Texture &t, std::array<int, 4> rect) {
  rect[0] = std::clamp(rect[0], 0, t.width - 1);
  rect[1] = std::clamp(rect[1], 0, t.height - 1);
  rect[2] = std::clamp(rect[2], rect[0] + 1, t.width);
  rect[3] = std::clamp(rect[3], rect[1] + 1, t.height);
  Texture result = t;
  result.width = rect[2] - rect[0];
  result.height = rect[3] - rect[1];
  result.rgba.resize(size_t(result.width) * result.height * 4);
  for (int y = 0; y < result.height; y++)
    for (int x = 0; x < result.width; x++) {
      auto color = pixel(t, x + rect[0], y + rect[1]);
      std::copy(color.begin(), color.end(),
                result.rgba.begin() + (size_t(y) * result.width + x) * 4);
    }
  result.revision++;
  result.saved = false;
  return result;
}
inline void pasteImage(Texture &t, const Texture &source, int x, int y,
                       const PaintOptions &options) {
  for (int yy = 0; yy < source.height; yy++)
    for (int xx = 0; xx < source.width; xx++) {
      int tx = x + xx, ty = y + yy;
      if (!allowed(t, tx, ty, options))
        continue;
      auto color = quantize(blend(pixel(t, tx, ty), pixel(source, xx, yy), options.opacity,
                                  options.mode, options.lockAlpha),
                            options.palette);
      std::copy(color.begin(), color.end(), t.rgba.begin() + (size_t(ty) * t.width + tx) * 4);
    }
  t.revision++;
  t.saved = false;
}
inline void clearUnusedPixels(Project &p, int texture) {
  if (texture < 0 || size_t(texture) >= p.textures.size())
    return;
  auto &t = p.textures[texture];
  auto triangles_ = triangles(p);
  std::vector<unsigned char> used(size_t(t.width) * t.height);
  for (auto &f : triangles_)
    if (f.texture == texture) {
      auto cross2 = [](Vec2 a, Vec2 b, Vec2 c) {
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
      };
      int x0 = t.width, y0 = t.height, x1 = 0, y1 = 0;
      for (auto uv : f.uv) {
        int x = int(uv[0] * t.width / t.uvWidth), y = int(uv[1] * t.height / t.uvHeight);
        x0 = std::min(x0, x);
        x1 = std::max(x1, x + 1);
        y0 = std::min(y0, y);
        y1 = std::max(y1, y + 1);
      }
      for (int y = std::max(0, y0); y < std::min(t.height, y1); y++)
        for (int x = std::max(0, x0); x < std::min(t.width, x1); x++) {
          Vec2 q{(x + .5) * t.uvWidth / t.width, (y + .5) * t.uvHeight / t.height};
          double a = cross2(f.uv[0], f.uv[1], q), b = cross2(f.uv[1], f.uv[2], q),
                 c = cross2(f.uv[2], f.uv[0], q);
          if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0))
            used[size_t(y) * t.width + x] = 1;
        }
    }
  for (size_t i = 0; i < used.size(); i++)
    if (!used[i])
      t.rgba[i * 4 + 3] = 0;
  t.revision++;
  t.saved = false;
}
} // namespace ftebroom::bb
