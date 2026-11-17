// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/character/preview.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace ftebroom::character;

static std::string read(const std::filesystem::path& path) {
  std::ifstream f(path,std::ios::binary);
  if (!f) throw std::runtime_error("Cannot read test asset");
  return {std::istreambuf_iterator<char>(f),{}};
}

int main(int argc,char** argv) try {
  if (argc != 2) throw std::runtime_error("Expected runtime/characters path");
  const std::filesystem::path path(argv[1]);
  auto rig = readHumanoid(read(path/"humanoid-v1.rbh"));
  readFaceShapes(rig,read(path/"adult-face-shapes.rbf"));
  readBodyShapes(rig,read(path/"adult-body-shapes.rbs"));
  std::map<std::string,Wardrobe> assets;
  for (const auto& file : std::filesystem::directory_iterator(path/"wardrobe"))
    if (file.path().extension() == ".rbw")
      assets.emplace(file.path().stem().string(),readWardrobe(read(file.path()),rig,file.path().stem().string()));
  auto lookup = [&](const auto& id) -> const auto& { return assets.at(id); };
  std::vector<double> ms;
  constexpr double bodyProfiles[]{-.78, 0., .78};
  for (unsigned seed = 0; seed < 1000; ++seed) {
    Document doc;
    doc.outfit = seed%outfitCount;
    doc.hairstyle = (seed/outfitCount)%hairstyleCount;
    doc.expression = seed%std::size(expressions);
    doc.beard = seed%std::size(beards);
    randomize(doc,seed);
    doc.bodyProfile = bodyProfiles[seed%std::size(bodyProfiles)];
    const auto started = std::chrono::steady_clock::now();
    const auto fit = assemble(rig,doc,lookup,true);
    ms.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
    const auto skin = skinMatrices(fit,std::vector<Mat4>(fit.joints.size(),Mat4::identity()));
    if (fit.vertices.empty() || fit.triangles.empty() || fit.rigHash != rig.rigHash)
      throw std::runtime_error("Invalid fitted character");
    for (const auto& vertex : fit.vertices) {
      const auto p = deform(vertex,vertex.position,skin);
      const auto squaredLength = dot(p,p);
      if (!std::isfinite(squaredLength) || squaredLength > 26*26)
        throw std::runtime_error("Invalid position after fitting and skinning");
    }
    for (const auto& triangle : fit.triangles)
      if (std::any_of(triangle.indices.begin(),triangle.indices.end(),[&](auto index) {
        return index >= fit.vertices.size();
      })) throw std::runtime_error("Invalid triangle after fitting");
  }
  std::sort(ms.begin(),ms.end());
  std::cout << "PASS 1000 fitted and skinned characters with the FTE profile. CPU fitting p95 " << ms[949] << " ms\n";
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
}
