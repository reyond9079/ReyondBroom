// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_geometry.h"
#include <cstring>
namespace ftebroom::bb {
// The ABI contains no engine pointers or VM-owned data. Upload validates and
// copies its input immediately; scene drawing never touches authoring memory.
struct DynamicImageAPI {
  int (*upload)(const char *, const void *, unsigned, unsigned);
  void (*clear)();
};
struct DynamicMeshAPI {
  int (*upload)(const void *, size_t);
  void (*clear)();
  void (*stats)(double *);
};
inline std::string textureName(const std::string &id) {
  std::ostringstream out;
  out << "bbtex/" << std::hex << std::hash<std::string>{}(id) << ".tga";
  return out.str();
}
inline void word(std::string &out, unsigned n) {
  out.append(reinterpret_cast<const char *>(&n), 4);
}
inline std::string renderBuffer(const Project &p, const std::vector<Triangle> &ts,
                                int display = 0) {
  std::map<int, std::vector<const Triangle *>> surfaces;
  for (auto &t : ts)
    surfaces[t.texture].push_back(&t);
  unsigned batches = 0;
  for (auto &entry : surfaces)
    batches += (entry.second.size() + 21844) / 21845;
  if (batches > 256)
    throw std::runtime_error("Preview exceeds 256 material batches");
  std::string out = "RBB1";
  word(out, batches);
  const Vec3 colors[] = {{.55, .65, .8}, {.68, .38, .25}, {.7, .65, .35}, {.32, .6, .48},
                         {.5, .4, .65},  {.6, .6, .6},    {.65, .4, .5},  {.3, .5, .6}};
  for (auto &[tex, all] : surfaces)
    for (size_t first = 0; first < all.size(); first += 21845) {
      std::vector<const Triangle *> tris(all.begin() + first,
                                         all.begin() + std::min(all.size(), first + 21845));
      word(out, tris.size() * 3);
      word(out, tris.size() * 3);
      std::string name = tex >= 0 && size_t(tex) < p.textures.size()
                             ? textureName(p.textures[tex].id)
                             : "$whiteimage";
      if (display == 1 || display == 2)
        name = "$whiteimage";
      if (display == 3)
        name = "bbtex/checker.tga";
      if (name.size() > 127)
        throw std::runtime_error("Preview material name too long");
      out += name;
      out.append(128 - name.size(), 0);
      for (auto t : tris)
        for (int i = 0; i < 3; i++) {
          auto q = bbToQuake(t->p[i], p.front(), p.exportScale());
          Vec2 uv = t->uv[i];
          if (tex >= 0 && size_t(tex) < p.textures.size()) {
            uv[0] /= p.textures[tex].uvWidth;
            uv[1] /= p.textures[tex].uvHeight;
          }
          auto c = display == 1 ? colors[(t->color % 8 + 8) % 8] : Vec3{1, 1, 1};
          double shade =
              t->shade ? .45 + .55 * std::max(0.0, dot(unit(t->n[i]), unit({-.4, 1, -.6}))) : 1;
          for (auto &v : c)
            v *= shade;
          float v[9] = {float(q[0]),  float(q[1]),  float(q[2]),
                        float(uv[0]), float(uv[1]), float(c[0]),
                        float(c[1]),  float(c[2]),  1};
          out.append(reinterpret_cast<char *>(v), sizeof(v));
        }
      for (unsigned i = 0; i < tris.size() * 3; i++)
        word(out, i);
    }
  return out;
}
} // namespace ftebroom::bb
