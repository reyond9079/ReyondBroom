// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "schema.h"
#include "catalog_data.h"
namespace ftebroom::plant {
inline const Json& catalogManifest() {
  static const auto manifest=Json::parse(botanicalCatalog);
  return manifest;
}
inline std::vector<Project> presets() {
  static const auto library=[](){
    std::vector<Project> out;
    for(const auto& entry:catalogManifest().at("presets")) {
      Project d;d.preset=d.species=entry.at("id").get<std::string>();
      d.name=entry.at("name");d.family=Family(entry.at("family").get<int>());
      d.tags=entry.at("tags").get<std::vector<std::string>>();d.nodes=defaultNodes();
      if(d.family==Family::Flower||d.family==Family::Vine||d.family==Family::Fern||d.family==Family::Riparian)d.p.bark={.20,.34,.10};
      if(d.family==Family::Mushroom)d.p.bark={.82,.77,.65};
      auto full=serialize(d);full["parameters"].update(entry.at("parameters"));
      out.push_back(deserialize(full));
    }return out;
  }();
  return library;
}
// Upgrading is an explicit user action. Opening generator-1 files never calls it.
inline Project upgradeGenerator(Project d) {
  auto library=presets();auto it=std::find_if(library.begin(),library.end(),[&](auto& p){return p.preset==d.preset;});
  auto defaults=parameters(it==library.end()?Params{}:it->p),values=serialize(d);
  for(auto key:{"crown","phyllotaxy","organ","barkPattern","needleForm","evergreen","leaflets","clusterLeaves","crownBase","crownAspect","crownFill","petiole","serration"})values["parameters"][key]=defaults.at(key);
  values["generatorVersion"]=2;
  if(it!=library.end())values["tags"]=it->tags;
  return deserialize(values);
}
}
