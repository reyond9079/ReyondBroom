// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/ftebroom_bb_io.h"
#include "../native/ftebroom_bb_paint.h"
#include "../native/ftebroom_bb_pick.h"
#include "../native/ftebroom_bb_undo.h"
#include "../native/ftebroom_model_mdl_export.h"
#include <iostream>
using namespace ftebroom::bb;
int main() {
  try {
    Texture t;
    t.width = t.uvWidth = 16;
    t.height = t.uvHeight = 16;
    t.rgba.resize(16 * 16 * 4);
    PaintOptions o;
    stroke(t, 0, 0, 15, 7, o);
    int count = 0;
    for (size_t i = 3; i < t.rgba.size(); i += 4)
      count += t.rgba[i] > 0;
    if (count != 16)
      throw std::runtime_error("Bresenham line count");
    std::cout << "PASS 1 px Bresenham line\n";
    t.rgba.assign(16 * 16 * 4, 0);
    for (int i = 0; i < 16; i++) {
      size_t n = (i * 16 + i) * 4;
      t.rgba[n] = 255;
      t.rgba[n + 3] = 255;
    }
    o.color = {0, 255, 0, 255};
    flood(t, 0, 0, o, 0, true);
    if (pixel(t, 1, 1)[1] != 0)
      throw std::runtime_error("Flood crossed diagonal");
    std::cout << "PASS four connected fill\n";
    Project p;
    addTexture(p, t);
    Selection s;
    History history;
    history.begin(p, s, {0});
    auto before = p.textures[0].rgba;
    dab(p.textures[0], 8, 8, o);
    history.finish(p, s, "Paint");
    history.undo(p, s);
    if (p.textures[0].rgba != before)
      throw std::runtime_error("Paint undo mismatch");
    history.redo(p, s);
    if (pixel(p.textures[0], 8, 8)[1] != 255)
      throw std::runtime_error("Paint redo mismatch");
    o.clip = {2, 2, 5, 5};
    stroke(p.textures[0], 0, 3, 15, 3, o);
    if (pixel(p.textures[0], 7, 3)[1])
      throw std::runtime_error("Stroke outside face UV");
    if (blend({255, 0, 0, 255}, {0, 0, 255, 255}, .5) != Pixel{128, 0, 128, 255})
      throw std::runtime_error("Normal blend reference");
    std::cout << "PASS rectangle pixel Undo Redo, UV clipping and blend\n";
    Pixel d{64, 128, 192, 255}, src{192, 64, 32, 255};
    const std::pair<Blend, Pixel> cases[] = {{Blend::Normal, src},
                                             {Blend::SetOpacity, src},
                                             {Blend::Behind, d},
                                             {Blend::Multiply, {48, 32, 24, 255}},
                                             {Blend::Add, {255, 192, 224, 255}},
                                             {Blend::Screen, {208, 160, 200, 255}},
                                             {Blend::Difference, {128, 64, 160, 255}}};
    for (auto sample : cases)
      if (blend(d, src, 1, sample.first) != sample.second)
        throw std::runtime_error("Blend known-pair mismatch");
    if (blend({128, 128, 128, 255}, {210, 210, 210, 255}, 1, Blend::Color) !=
        Pixel{128, 128, 128, 255})
      throw std::runtime_error("Color blend luminance");
    if (blend(d, src, 1, Blend::Normal, true)[3] != 255 ||
        blend({64, 128, 192, 0}, src, 1, Blend::Behind) != src)
      throw std::runtime_error("Behind / alpha lock");
    std::cout << "PASS all eight blend modes on known colors and alpha lock\n";
    std::string palette;
    std::vector<Pixel> swatches;
    for (int i = 0; i < 256; i++) {
      int r = i, g = (i * 17) % 256, b = (i * 37) % 256;
      palette += char(r);
      palette += char(g);
      palette += char(b);
      if (i < 224)
        swatches.push_back({(unsigned char)r, (unsigned char)g, (unsigned char)b, 255});
    }
    ftebroom::PaletteQuantizer quantizer(palette, 224);
    for (int i = 0; i < 1000; i++) {
      Pixel c{(unsigned char)(i * 13), (unsigned char)(i * 53), (unsigned char)(i * 97), 255};
      if (quantize(c, swatches) != swatches[quantizer.nearest(c[0], c[1], c[2])])
        throw std::runtime_error("Paint palette differs from MDL quantizer");
    }
    std::cout << "PASS 1000 exact paint / MDL palette comparisons (first 224)\n";
    Project scene;
    auto cubeElement = cube("paint", {-1, -1, -1}, {1, 1, 1});
    cubeElement["faces"]["north"]["uv"] = {0, 0, 16, 16};
    cubeElement["faces"]["north"]["texture"] = 0;
    scene.root["elements"].push_back(cubeElement);
    appendNode(scene, cubeElement["uuid"]);
    Texture image;
    image.width = image.height = image.uvWidth = image.uvHeight = 16;
    image.rgba.assign(16 * 16 * 4, 0);
    addTexture(scene, image);
    auto hit = pick(scene, triangles(scene), {{0, 0, -10}, {0, 0, 1}});
    if (!hit.found || hit.face != "north" || hit.x != 8 || hit.y != 8)
      throw std::runtime_error("3D stroke center UV");
    History paintHistory;
    Selection selected;
    paintHistory.begin(scene, selected, {0});
    PaintOptions brush;
    brush.color = {19, 42, 98, 255};
    dab(scene.textures[0], hit.x, hit.y, brush);
    paintHistory.finish(scene, selected, "3D dab");
    if (pixel(scene.textures[0], 8, 8) != brush.color)
      throw std::runtime_error("3D stroke did not paint hit texel");
    paintHistory.undo(scene, selected);
    if (pixel(scene.textures[0], 8, 8)[3] != 0)
      throw std::runtime_error("3D paint undo");
    std::cout << "PASS 3D ray center paint and pixel Undo\n";
    {
      auto check = [](bool ok, const char *name) {
        if (!ok) throw std::runtime_error(name);
        std::cout << "PASS " << name << '\n';
      };
      Project scene;
      Triangle triangle;
      triangle.p = {Vec3{-2, -2, 0}, Vec3{2, -2, 0}, Vec3{0, 2, 0}};
      triangle.uv = {Vec2{0, 0}, Vec2{1, 0}, Vec2{.5, 1}};
      triangle.texture = 0;
      Texture texture;
      texture.width = texture.height = texture.uvWidth = texture.uvHeight = 1;
      texture.rgba = {255, 255, 255, 255};
      scene.textures.push_back(texture);
      std::vector<Triangle> geometry{triangle};
      VisibilityTree tree;
      tree.rebuild(geometry);
      Ray ray{{0, 0, 5}, {0, 0, -1}};
      check(tree.blocked(scene, ray, 7), "front surface occludes a rear vertex");
      check(!tree.blocked(scene, ray, 4.999), "front vertex does not occlude itself");
      scene.textures[0].rgba[3] = 0;
      check(!tree.blocked(scene, ray, 7), "transparent texels do not occlude handles");
      scene.textures[0].rgba[3] = 255;
      for (int n = 1; n < 100; n++) {
        auto t = triangle;
        for (auto &p : t.p)
          p[0] += n * 10;
        geometry.push_back(t);
      }
      tree.rebuild(geometry);
      check(tree.blocked(scene, ray, 7), "visibility tree traverses multiple spatial nodes");
      check(!tree.blocked(scene, {{-50, 0, 5}, {0, 0, -1}}, 7),
            "visibility tree rejects rays outside bounds");
      geometry.clear();
      tree.rebuild(geometry);
      check(!tree.blocked(scene, ray, 7), "empty models have no occluders");
    }
    return 0;
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
