// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace ftebroom::bb {
using Vec2 = std::array<double, 2>;
using Vec3 = std::array<double, 3>;
using Vec4 = std::array<double, 4>;
inline Vec3 add(Vec3 a, Vec3 b) {
  for (int i = 0; i < 3; i++)
    a[i] += b[i];
  return a;
}
inline Vec3 sub(Vec3 a, Vec3 b) {
  for (int i = 0; i < 3; i++)
    a[i] -= b[i];
  return a;
}
inline Vec3 mul(Vec3 a, double n) {
  for (auto &v : a)
    v *= n;
  return a;
}
inline double dot(Vec3 a, Vec3 b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline Vec3 cross(Vec3 a, Vec3 b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
inline double length(Vec3 a) {
  return std::sqrt(dot(a, a));
}
inline Vec3 unit(Vec3 a) {
  double l = length(a);
  return l > 1e-12 ? mul(a, 1 / l) : Vec3{0, 1, 0};
}
constexpr double pi = 3.14159265358979323846;
struct Mat4 {
  double m[4][4]{};
  static Mat4 identity() {
    Mat4 a;
    for (int i = 0; i < 4; i++)
      a.m[i][i] = 1;
    return a;
  }
  Vec3 point(Vec3 p) const {
    Vec3 r{};
    for (int i = 0; i < 3; i++) {
      r[i] = m[i][3];
      for (int j = 0; j < 3; j++)
        r[i] += m[i][j] * p[j];
    }
    return r;
  }
  Vec3 vector(Vec3 p) const {
    return sub(point(p), point({}));
  }
};
inline Mat4 operator*(Mat4 a, Mat4 b) {
  Mat4 r;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      for (int k = 0; k < 4; k++)
        r.m[i][j] += a.m[i][k] * b.m[k][j];
  return r;
}
inline Mat4 translate(Vec3 v) {
  auto r = Mat4::identity();
  for (int i = 0; i < 3; i++)
    r.m[i][3] = v[i];
  return r;
}
inline Mat4 scale(Vec3 v) {
  auto r = Mat4::identity();
  for (int i = 0; i < 3; i++)
    r.m[i][i] = v[i];
  return r;
}
inline Mat4 rotation(Vec3 degrees) {
  Mat4 r = Mat4::identity();
  for (int a = 0; a < 3; a++) {
    auto t = Mat4::identity();
    int i = (a + 1) % 3, j = (a + 2) % 3;
    double c = std::cos(degrees[a] * pi / 180), s = std::sin(degrees[a] * pi / 180);
    t.m[i][i] = t.m[j][j] = c;
    t.m[i][j] = -s;
    t.m[j][i] = s;
    r = t * r;
  }
  return r;
}
inline Mat4 inverse(Mat4 a) {
  auto r = Mat4::identity();
  for (int i = 0; i < 4; i++) {
    int pivot = i;
    for (int j = i + 1; j < 4; j++)
      if (std::abs(a.m[j][i]) > std::abs(a.m[pivot][i]))
        pivot = j;
    if (std::abs(a.m[pivot][i]) < 1e-12)
      throw std::runtime_error("Singular transform");
    for (int k = 0; k < 4; k++) {
      std::swap(a.m[i][k], a.m[pivot][k]);
      std::swap(r.m[i][k], r.m[pivot][k]);
    }
    double d = a.m[i][i];
    for (int k = 0; k < 4; k++) {
      a.m[i][k] /= d;
      r.m[i][k] /= d;
    }
    for (int j = 0; j < 4; j++)
      if (i != j) {
        d = a.m[j][i];
        for (int k = 0; k < 4; k++) {
          a.m[j][k] -= a.m[i][k] * d;
          r.m[j][k] -= r.m[i][k] * d;
        }
      }
  }
  return r;
}
inline Vec3 eulerAngles(Mat4 m) {
  double y = std::asin(std::clamp(-m.m[2][0], -1., 1.)), x = 0, z = 0;
  if (std::abs(std::cos(y)) > 1e-8) {
    x = std::atan2(m.m[2][1], m.m[2][2]);
    z = std::atan2(m.m[1][0], m.m[0][0]);
  } else
    x = std::atan2(-m.m[1][2], m.m[1][1]);
  return {x * 180 / pi, y * 180 / pi, z * 180 / pi};
}
inline Vec3 bbToQuake(Vec3 v, int front = 0, double s = 1) {
  if (front == 1)
    v = rotation({0, 180, 0}).vector(v);
  if (front == 2)
    v = rotation({0, 90, 0}).vector(v);
  if (front == 3)
    v = rotation({0, -90, 0}).vector(v);
  return mul({-v[2], -v[0], v[1]}, s);
}
inline Vec3 quakeToBb(Vec3 v, int front = 0, double s = 1) {
  v = mul({-v[1], v[2], -v[0]}, 1 / s);
  if (front == 1)
    v = rotation({0, -180, 0}).vector(v);
  if (front == 2)
    v = rotation({0, -90, 0}).vector(v);
  if (front == 3)
    v = rotation({0, 90, 0}).vector(v);
  return v;
}
inline double snap(double v, double step) {
  return step > 0 ? std::round(v / step) * step : v;
}
} // namespace ftebroom::bb
