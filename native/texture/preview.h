// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "evaluate.h"

namespace ftebroom::textureStudio {

struct PreviewVector {
  float x, y, z;
  PreviewVector operator+(PreviewVector rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
  PreviewVector operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
};

inline float dot(PreviewVector a, PreviewVector b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline PreviewVector cross(PreviewVector a, PreviewVector b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline PreviewVector normalized(PreviewVector vector) {
  const float length = std::sqrt(dot(vector, vector));
  return length > 1e-8f ? vector * (1.f / length) : PreviewVector{0, 0, 1};
}

struct PreviewSurface {
  PreviewVector n, t, b;
  float u, v;
};

struct PreviewCamera {
  float yaw = 0, pitch = 0, panX = 0, panY = 0, zoom = 1, relief = 1;
  bool softLight = false;
};

inline std::optional<PreviewSurface> sphereSurface(float x, float y, PreviewVector view,
                                                   PreviewVector right, PreviewVector up) {
  const float distance = x * x + y * y;
  if (distance > 1) return std::nullopt;
  const auto n = view * std::sqrt(1 - distance) + right * x + up * y;
  const PreviewVector t = std::abs(n.x) + std::abs(n.z) > 1e-6f
    ? normalized({n.z, 0, -n.x}) : right;
  constexpr float pi = 3.141592653589793f;
  return PreviewSurface{n, t, cross(n, t), .5f + std::atan2(n.x, n.z) / (2 * pi), std::acos(std::clamp(n.y, -1.f, 1.f)) / pi};
}

inline std::optional<PreviewSurface> cubeSurface(float x, float y, PreviewVector view,
                                                 PreviewVector right, PreviewVector up) {
  const auto start = view * 4.f + right * x + up * y;
  constexpr float side = .63f;
  const std::array origin{start.x, start.y, start.z};
  const std::array direction{view.x, view.y, view.z};
  int axis = 0;
  float enter = 0, leave = 10;
  for (int i = 0; i < 3; ++i) {
    if (std::abs(direction[i]) < 1e-6f) {
      if (std::abs(origin[i]) > side) return std::nullopt;
      continue;
    }
    const float a = (origin[i] - side) / direction[i];
    const float b = (origin[i] + side) / direction[i];
    const float first = std::min(a, b), last = std::max(a, b);
    if (first > enter) { enter = first; axis = i; }
    leave = std::min(leave, last);
  }
  if (enter > leave) return std::nullopt;
  const auto hit = start + view * -enter;
  const auto uv = [side](float value) { return .5f + value / (2 * side); };
  const float sign = direction[axis] >= 0 ? 1.f : -1.f;
  if (axis == 0) return PreviewSurface{{sign,0,0}, {0,0,-sign}, {0,1,0}, uv(-hit.z*sign), uv(-hit.y)};
  if (axis == 1) return PreviewSurface{{0,sign,0}, {1,0,0}, {0,0,-sign}, uv(hit.x), uv(hit.z*sign)};
  return PreviewSurface{{0,0,sign}, {sign,0,0}, {0,1,0}, uv(hit.x*sign), uv(-hit.y)};
}

inline Image materialPreview(const Evaluation& image, bool cube, const std::atomic<bool>* cancel = nullptr,
                             PreviewCamera camera = {}) {
  require(std::isfinite(camera.yaw) && std::isfinite(camera.pitch) && std::isfinite(camera.panX) &&
    std::isfinite(camera.panY) && std::isfinite(camera.zoom) && std::isfinite(camera.relief) && camera.zoom > 0,
    "preview", "Reset the preview camera to restore valid settings.");
  constexpr unsigned edge = 256;
  const auto& color = *image.outputs.at("baseColor");
  const auto find = [&](const char* channel) {
    const auto found = image.outputs.find(channel);
    return found == image.outputs.end() ? nullptr : found->second.get();
  };
  const auto* normals = find("normal");
  const auto* gloss = find("roughness");
  const auto* metal = find("metalness");
  const auto* ambient = find("ao");
  const auto* emission = find("emissive");
  const auto initial = cube ? normalized({.85f,.62f,1.f}) : normalized({.22f,.18f,1.f});
  const float yaw = camera.yaw + std::atan2(initial.x, initial.z);
  const float pitch = std::clamp(camera.pitch + std::asin(initial.y), -1.52f, 1.52f);
  const auto view = normalized({std::sin(yaw)*std::cos(pitch), std::sin(pitch), std::cos(yaw)*std::cos(pitch)});
  const auto right = normalized(cross({0,1,0}, view));
  const auto up = cross(view, right);
  const auto light = normalized(camera.softLight ? view + up * .35f : view + right * -.7f + up * .85f);
  const auto half = normalized(view + light);
  const float ambientLight = camera.softLight ? .53f : .34f;
  const float directLight = camera.softLight ? .52f : .72f;
  Image result{{edge, edge}, Port::color, std::vector<Pixel>(edge * edge)};
  for (unsigned y = 0; y < edge; ++y) {
    if (cancel && cancel->load(std::memory_order_relaxed)) throw Error("preview", "Rendering was cancelled.");
    for (unsigned x = 0; x < edge; ++x) {
      const float a = 2.30f * (((x + .5f) / edge - .5f) / camera.zoom - camera.panX);
      const float b = 2.30f * ((.5f - (y + .5f) / edge) / camera.zoom + camera.panY);
      const auto sample = cube ? cubeSurface(a, b, view, right, up) : sphereSurface(a, b, view, right, up);
      if (!sample) continue;
      const auto& surface = *sample;
      const auto sampleData = [&](const Image* data, Pixel fallback) {
        if (!data) return fallback;
        return data->sample(int(frac(surface.u) * data->size.width), int(frac(surface.v) * data->size.height));
      };
      const auto albedo = sampleColor(color, surface.u, surface.v);
      const auto bump = sampleData(normals, {0,0,1,1});
      const auto normal = normalized(surface.t * (bump[0]*camera.relief) +
        surface.b * (bump[1]*camera.relief) + surface.n * bump[2]);
      const float diffuse = unit(dot(light, normal));
      const float roughness = sampleData(gloss, {.6f,0,0,1})[0];
      const float metalness = sampleData(metal, {0,0,0,1})[0];
      const float ao = sampleData(ambient, {1,0,0,1})[0];
      const auto glow = sampleData(emission, {0,0,0,1});
      const float specular = std::pow(unit(dot(half, normal)), mix(76.f, 4.f, roughness)) * (.28f * diffuse);
      auto& pixel = result.pixels[y * edge + x];
      for (size_t c = 0; c < 3; ++c) {
        const float reflected = mix(.04f, albedo[c], metalness);
        pixel[c] = unit(albedo[c] * (ao * ambientLight + diffuse * (directLight - .37f * metalness)) +
          specular * reflected + glow[c]);
      }
      pixel[3] = albedo[3];
    }
  }
  return result;
}

} // namespace ftebroom::textureStudio
