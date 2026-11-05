// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_bb_export.h"
#include "../native/ftebroom_bb_uv.h"
#include "../native/ftebroom_legacy_models.h"
#include "../native/ftebroom_md3.h"
#include "../native/ftebroom_model_md2_export.h"
#include "../native/ftebroom_model_static_export.h"
#include "../native/ftebroom_model_studio_export.h"
#include <iostream>
using namespace ftebroom;
using namespace ftebroom::bb;
void check(bool ok, const char *name) {
  if (!ok)
    throw std::runtime_error(name);
  std::cout << "PASS " << name << '\n';
}
int main(int argc, char **argv) {
  try {
    std::filesystem::path output;
    if (argc > 1) {
      output = argv[1];
      std::filesystem::create_directories(output);
    }
    std::string palette;
    for (int i = 0; i < 256; i++)
      for (int a = 0; a < 3; a++)
        palette += char(i);
    if (argc > 2)
      palette = readFile(argv[2]);
    for (int type = 1; type <= 4; type++) {
      auto p = makeTemplate(type,
                            type == 1   ? "monster"
                            : type == 2 ? "v_weapon"
                            : type == 3 ? "item"
                                        : "prop",
                            2);
      textureTemplate(p, "skin.png", 16, true, 0, true, true);
      auto original = serialize(p);
      auto baked = bake(p);
      auto mesh = baked.mesh;
      std::vector<modelExport::Skin> skins;
      for (auto index : baked.textures) {
        auto t = p.textures.at(index);
        skins.push_back({unsigned(t.width), unsigned(t.height), t.rgba});
      }
      auto mdl = modelExport::mdl(mesh, skins, palette), md3 = modelExport::md3(mesh);
      auto md2 = modelExport::md2(mesh, skins, palette, "skin.pcx");
      auto studio = studioExport::bake(mesh, skins, 10);
      check(quake1::word(mdl, 68) == mesh.frames && quake1::word(md3, 76) == mesh.frames &&
                quake1::word(md2.model, 40) == mesh.frames,
            "MDL MD2 MD3 retain baked frame counts");
      check(quake1::word(studio, quake1::word(studio, 168) + 56) == mesh.frames,
            "GoldSrc retains complete template animation");
      auto decoded = readMdl(mdl);
      check(!decoded.triangles.empty() &&
                std::all_of(decoded.skin.pixels.begin(), decoded.skin.pixels.end(),
                            [](unsigned char c) { return c < 224; }),
            "MDL skin uses no fullbright palette indices");
      Vec3 low{1e30, 1e30, 1e30}, high{-1e30, -1e30, -1e30}, sourceLow = low, sourceHigh = high;
      for (auto &tri : decoded.triangles)
        for (auto v : tri)
          for (int a = 0; a < 3; a++) {
            low[a] = std::min(low[a], double(v.point[a]));
            high[a] = std::max(high[a], double(v.point[a]));
          }
      for (auto &surface : mesh.surfaces)
        for (auto v : surface.frames[0])
          for (int a = 0; a < 3; a++) {
            sourceLow[a] = std::min(sourceLow[a], double(v.position[a]));
            sourceHigh[a] = std::max(sourceHigh[a], double(v.position[a]));
          }
      check(length(sub(low, sourceLow)) < .5 && length(sub(high, sourceHigh)) < .5,
            "MDL reference AABB within 8-bit quantization");
      check(readMd3(md3).size() == mesh.surfaces.size() &&
                legacyModels::md2(md2.model).size() == 1 &&
                !legacyModels::studio(studio, {}).empty(),
            "native readers reopen all template outputs");
      auto stat = bake(p, false).mesh;
      check(modelExport::obj(stat).find("f ") != std::string::npos &&
                modelExport::iqm(stat).substr(0, 15) == "INTERQUAKEMODEL" &&
                modelExport::md5mesh(stat).find("MD5Version 10") != std::string::npos,
            "OBJ IQM MD5 static writers");
      check(serialize(p) == original, "exports do not mutate authoring project");
      // Frame names survive the vertex-animation writers, including the base pose.
      auto frameAt = 84 + 4 + size_t(quake1::word(mdl, 52)) * quake1::word(mdl, 56) +
                     size_t(quake1::word(mdl, 60)) * 12 + size_t(quake1::word(mdl, 64)) * 16;
      for (unsigned frame = 0; frame < mesh.frames; frame++) {
        check(quake1::name(mdl, frameAt + 12, 16) == mesh.frameNames[frame], "MDL frame name");
        frameAt += 28 + size_t(quake1::word(mdl, 60)) * 4;
      }
      if (!output.empty()) {
        auto stem = type == 1 ? "monster" : type == 2 ? "v_weapon" : type == 3 ? "item" : "prop";
        atomicWrite(output / (std::string(stem) + ".mdl"), mdl);
        atomicWrite(output / (std::string(stem) + ".md3"), md3);
        atomicWrite(output / (std::string(stem) + ".md2"), md2.model);
        atomicWrite(output / (std::string(stem) + "-studio.mdl"), studio);
        atomicWrite(output / (std::string(stem) + ".bbmodel"), serialize(p));
        atomicWrite(output / "skin.pcx", md2.skin);
        atomicWrite(output / "skin.tga", tga(p.textures[0]));
      }
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
