// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/plant/export.h"
#include "../native/plant/presets.h"
#include <fstream>
#include <iostream>
#include <chrono>
using namespace ftebroom::plant;
static int checks=0;
static void check(bool b,const std::string& name){if(!b)throw std::runtime_error(name);++checks;}
static std::string fingerprint(const Mesh& m){std::string bytes;auto append=[&](auto x){bytes.append(reinterpret_cast<const char*>(&x),sizeof x);};for(auto& v:m.vertices){for(double x:v.p)append(x);for(double x:v.n)append(x);for(double x:v.uv)append(x);append(v.wind);append(v.phase);}for(auto& f:m.faces){for(unsigned i:f.i)append(i);append(f.material);append(int(f.part));append(f.organ);}std::ostringstream s;s<<std::hex<<hash(bytes);return s.str();}
int main(int argc,char** argv)try{
 auto start=std::chrono::steady_clock::now();Json report=Json::array();auto catalog=presets();std::ifstream offline("runtime/plants/builtin/catalog.json");Json offlineCatalog;offline>>offlineCatalog;check(offlineCatalog==catalogManifest(),"offline catalogue matches embedded resource");check(catalog.size()>=120,"minimum 120 distinct botanical presets");std::map<std::string,int> categories;std::set<std::string> ids,traits;
 for(auto& row:catalogManifest()["presets"]){categories[row["category"]]++;check(ids.insert(row["id"]).second,"unique stable ID");auto p=row["parameters"];for(auto key:{"bark","leaf","flower","seed"})p.erase(key);check(traits.insert(p.dump()).second,"architectural diversity, not recolors or seeds");}
 for(auto it=catalogManifest()["minimumByCategory"].begin();it!=catalogManifest()["minimumByCategory"].end();++it)check(categories[it.key()]>=it.value().get<int>(),"category minimum "+it.key());
 for(auto d:catalog){Json entry={{"id",d.preset},{"seeds",Json::array()}};
  for(unsigned seed:{137u,138u,424242u}){d.seed=seed;Json row={{"seed",seed},{"triangles",Json::array()},{"fingerprints",Json::array()}};size_t previous=SIZE_MAX;
   for(int l=0;l<3;l++){auto mesh=generate(d,l);validate(mesh);check(!mesh.faces.empty(),"nonempty "+d.preset);check(mesh.faces.size()<previous,"strict LOD reduction "+d.preset);previous=mesh.faces.size();check(mesh.faces.size()<=size_t(d.p.budget*(l==0?1:l==1?d.p.lod1Ratio:d.p.lod2Ratio)),"budget "+d.preset);
    auto key=fingerprint(mesh);check(fingerprint(generate(d,l))==key,"exact deterministic regeneration "+d.preset);for(auto& f:mesh.faces)check(f.material>=0&&f.material<Count,"valid material slot");row["triangles"].push_back(mesh.faces.size());row["fingerprints"].push_back(key);
   }entry["seeds"].push_back(row);
  }report.push_back(entry);std::cout<<"PASS matrix "<<d.preset<<std::endl;
 }
 std::ifstream input(argc>1?argv[1]:"tests/fixtures/plant-generator1.json");Json legacy;input>>legacy;check(legacy.size()==30,"all original presets in golden corpus");
 for(auto& item:legacy){auto original=item["project"];auto d=deserialize(original);check(d.generator==1,"opening legacy source never upgrades");check(serialize(d)==original,"legacy serialized identity preserved");check(ids.count(d.preset),"original preset ID retained");
  for(auto& seed:item["seeds"]){d.seed=seed["seed"].get<unsigned>();for(int l=0;l<3;l++)check(fingerprint(generate(d,l))==seed["lods"][l],"exact legacy geometry "+d.preset);auto maps=textureMaps(d,nullptr,nullptr,48);int slot=0;for(auto* image:{&maps.albedo,&maps.normal,&maps.orm}){std::ostringstream id;id<<std::hex<<hash(std::string(image->rgba.begin(),image->rgba.end()));check(id.str()==seed["textures"][slot++],"exact legacy atlas "+d.preset);}}
  auto upgraded=upgradeGenerator(d);check(upgraded.generator==2&&upgraded.seed==d.seed,"explicit upgrade preserves seed");check(deserialize(serialize(upgraded)).generator==2,"new project roundtrip");
 }
 auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::ofstream out(argc>2?argv[2]:"plant-botanical-results.json");out<<Json{{"checks",checks},{"presets",catalog.size()},{"seeds",3},{"lods",3},{"mesh_cases",catalog.size()*9},{"legacy_cases",270},{"elapsed_seconds",seconds},{"matrix",report}}.dump(2);std::cout<<"PASS "<<checks<<" checks in "<<seconds<<" s\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
