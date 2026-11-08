// SPDX-License-Identifier: GPL-3.0-or-later
// Compile unchanged against either native/ or frozen baseline native/ headers.
#include <plant/generator.h>
#include <plant/presets.h>
#include <plant/textures.h>
#include <chrono>
#include <iostream>
#include <set>
using namespace ftebroom::plant;
int main(){
 auto all=presets();size_t maxFaces=0;std::string heaviest;
 for(auto& p:all){auto m=generate(p);if(m.faces.size()>maxFaces){maxFaces=m.faces.size();heaviest=p.preset;}}
 std::vector<std::string> ids={"oak","spruce","meadow-grass",heaviest};Json result=Json::array();
 for(auto& id:ids){auto p=*std::find_if(all.begin(),all.end(),[&](auto& x){return x.preset==id;});Json lods=Json::array();
  for(int l=0;l<3;l++){auto warm=generate(p,l);std::vector<double> ms;for(int i=0;i<15;i++){auto start=std::chrono::steady_clock::now();auto m=generate(p,l);ms.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}std::sort(ms.begin(),ms.end());std::set<int> mats;for(auto& f:warm.faces)mats.insert(f.material);lods.push_back({{"lod",l},{"triangles",warm.faces.size()},{"vertices",warm.vertices.size()},{"materials",mats.size()},{"mesh_bytes",warm.faces.size()*sizeof(Face)+warm.vertices.size()*sizeof(Vertex)},{"median_ms",ms[7]},{"p95_ms",ms[14]},{"samples_ms",ms}});}
  auto start=std::chrono::steady_clock::now();auto tex=textureMaps(p);auto textureMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();result.push_back({{"id",id},{"seed",p.seed},{"budget",p.p.budget},{"heaviest",id==heaviest},{"lods",lods},{"texture_ms",textureMs},{"texture_bytes",tex.albedo.rgba.size()+tex.normal.rgba.size()+tex.orm.rgba.size()}});
 }
 std::cout<<result.dump(2)<<'\n';
}
