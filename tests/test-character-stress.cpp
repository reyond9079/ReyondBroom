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
  if (argc != 2 && argc != 3)
    throw std::runtime_error("Expected runtime/characters path and optional round-trip output directory");
  const std::filesystem::path path(argv[1]);
  const auto saved = argc == 3 ? std::filesystem::path(argv[2]) : std::filesystem::path{};
  if (!saved.empty()) std::filesystem::create_directories(saved);
  auto rig = readHumanoid(read(path/"humanoid-v1.rbh"));
  readFaceShapes(rig,read(path/"adult-face-shapes.rbf"));
  readBodyShapes(rig,read(path/"adult-body-shapes.rbs"));
  std::map<std::string,Wardrobe> assets;
  for (const auto& file : std::filesystem::directory_iterator(path/"wardrobe"))
    if (file.path().extension() == ".rbw")
      assets.emplace(file.path().stem().string(),readWardrobe(read(file.path()),rig,file.path().stem().string()));
  auto lookup = [&](const auto& id) -> const auto& { return assets.at(id); };
  std::vector<double> ms, typical;
  constexpr double bodyProfiles[]{-.78, 0., .78};
  for (unsigned seed = 0; seed < 1000; ++seed) {
    Document doc;
    doc.outfit = seed%outfitCount;
    doc.hairstyle = (seed/outfitCount)%hairstyleCount;
    doc.expression = seed%std::size(expressions);
    doc.beard = seed%std::size(beards);
    randomize(doc,seed);
    doc.bodyProfile = bodyProfiles[seed%std::size(bodyProfiles)];
    if (!saved.empty() && seed % 10 == 0) {
      auto file = saved/("character-"+std::to_string(seed)+".rbchar");
      auto expected = writeDocument(doc,rig);
      std::ofstream stream(file,std::ios::binary);
      stream << expected.dump() << '\n';
      stream.close();
      if (!stream) throw std::runtime_error("Cannot write character round-trip fixture");
      doc = readDocument(read(file),rig);
      if (writeDocument(doc,rig) != expected)
        throw std::runtime_error("Character changed on save and reopen");
    }
    const auto started = std::chrono::steady_clock::now();
    const auto fit = assemble(rig,doc,lookup,true);
    const auto elapsed = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    ms.push_back(elapsed);
    if (fit.triangles.size() <= 20000) typical.push_back(elapsed);
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
  if (!saved.empty()) std::cout << "PASS 100 character document saves and reopens\n";
  std::cout << "PASS 1000 fitted and skinned characters with the FTE profile. CPU fitting p95 " << ms[949] << " ms\n";
  if (!typical.empty()) {
    std::sort(typical.begin(), typical.end());
    std::cout << "CPU fitting p95 for " << typical.size() << " full meshes up to 20k triangles: "
              << typical[(typical.size()*95+99)/100-1] << " ms\n";
  }
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
}
