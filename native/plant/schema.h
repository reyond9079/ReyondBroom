// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../third_party/json.hpp"
#include "../ftebroom_bb_math.h"
#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <functional>
namespace ftebroom::plant {
using Json = nlohmann::json;
using V = bb::Vec3;
using bb::add; using bb::sub; using bb::mul; using bb::dot; using bb::cross;
using bb::unit; using bb::length;
constexpr double pi = bb::pi;
enum class Family { Broadleaf, Conifer, Bush, Grass, Fern, Flower, Mushroom, Cactus, Palm, Bamboo, Vine, Custom };
enum class Part { Roots, Trunk, Primary, Secondary, Twigs, Leaves, Flowers, Surface };
inline const char* familyName(Family f) {
  static const char* names[] = {"Trees","Conifers","Bushes","Grass","Ferns","Flowers","Mushrooms","Cacti","Palms","Reeds / Bamboo","Vines / Ivy","Custom"};
  return names[int(f)];
}
inline const char* partName(Part p) {
  static const char* names[] = {"Roots","Trunk / stems","Primary branches","Secondary branches","Twigs / fronds","Leaves","Flowers / fruit","Cap / surface"};
  return names[int(p)];
}
struct Rng {
  uint64_t state;
  explicit Rng(uint64_t seed) : state(seed) {}
  uint64_t next() {uint64_t z=(state+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
  double real() {return (next()>>11)*(1.0/9007199254740992.0);}
  double range(double low,double high) {return low+(high-low)*real();}
};
inline uint64_t mix(uint64_t a,uint64_t b) {Rng r(a^(b*0x9e3779b97f4a7c15ULL));return r.next();}
struct Component {
  std::string id, name, parent;
  Part kind=Part::Trunk;
  bool visible=true;
  V offset{}, scale{1,1,1};
  double rotation=0;
};
struct Params {
  double height=240, radius=8, taper=.8, curvature=.22, spread=.72;
  double age=.85, density=.85, irregularity=.28, gravity=.2, light=.25;
  int branches=14, depth=3, segments=8, sides=9;
  double branchAngle=52, branchLength=.65, branchTaper=.62, dead=.06;
  double leafSize=14, leafWidth=.60, leafCurl=.2, leafTwist=.25;
  int leaves=18, leafShape=0, foliageMode=0;
  double rootFlare=1.3, flowerSize=12, bloom=.85, dryness=0;
  int petals=12, flowerLayers=1, clumps=1;
  double capThickness=.2, gills=.6, ribs=10, spines=.3;
  int season=1, quality=0, budget=32000;
  double lod1Ratio=.5,lod2Ratio=.2;
  double windStrength=.28, windDirection=30, windFrequency=1.1, stiffness=.65, flutter=.4;
  V bark{.31,.20,.12}, leaf{.21,.43,.10}, flower{.94,.72,.19};
  double roughness=.8, ao=.35, variation=.18;
  std::array<double,5> growth{.1,.5,.8,.65,.2};
  std::array<double,5> leafProfile{0,.75,1,.65,0};
  std::array<V,4> controls{{{0,0,0},{0,0,.33},{0,0,.67},{0,0,1}}};
};
struct Project {
  int version=1, generator=1;
  std::string name="English oak", preset="oak", species="oak";
  Family family=Family::Broadleaf;
  uint32_t seed=137;
  Params p;
  std::vector<Component> nodes;
  std::vector<uint32_t> variations;
  std::string texture;
};
inline std::vector<Component> defaultNodes() {
  std::vector<Component> out;
  const char* ids[]={"roots","trunk","primary","secondary","twigs","leaves","flowers","surface"};
  const char* parents[]={"","","trunk","primary","secondary","twigs","twigs","trunk"};
  for(int i=0;i<8;i++)out.push_back({ids[i],partName(Part(i)),parents[i],Part(i)});
  return out;
}
// Copy a complete component branch and retain its internal parent relationships.
inline size_t duplicateComponent(Project& d,size_t selected) {
  if(selected>=d.nodes.size())throw std::runtime_error("Select a plant component first");
  std::set<std::string> branch{d.nodes[selected].id};
  bool changed=true;while(changed){changed=false;for(auto& n:d.nodes)if(branch.count(n.parent)&&branch.insert(n.id).second)changed=true;}
  if(d.nodes.size()+branch.size()>64)throw std::runtime_error("Duplicating this branch exceeds the 64 component limit");
  std::map<std::string,std::string> ids;
  for(auto& id:branch){auto copy=id+"-copy";while(std::any_of(d.nodes.begin(),d.nodes.end(),[&](auto& n){return n.id==copy;})||std::any_of(ids.begin(),ids.end(),[&](auto& n){return n.second==copy;}))copy+="-copy";ids[id]=copy;}
  std::vector<Component> copies;size_t result=0;
  for(size_t i=0;i<d.nodes.size();i++)if(branch.count(d.nodes[i].id)){auto n=d.nodes[i];n.id=ids[n.id];if(ids.count(n.parent))n.parent=ids[n.parent];if(i==selected){result=d.nodes.size()+copies.size();n.name+=" copy";n.offset[0]+=d.p.height*.08;}copies.push_back(n);}
  d.nodes.insert(d.nodes.end(),copies.begin(),copies.end());return result;
}
inline double curve(const std::array<double,5>& c,double t) {
  t=std::clamp(t,0.,1.)*4;int i=std::min(3,int(t));double f=t-i;
  return c[i]*(1-f)+c[i+1]*f;
}
inline Json parameters(const Params& p) {
  Json j;
#define RB_PLANT_FIELD(n) j[#n]=p.n
  RB_PLANT_FIELD(height);RB_PLANT_FIELD(radius);RB_PLANT_FIELD(taper);RB_PLANT_FIELD(curvature);RB_PLANT_FIELD(spread);
  RB_PLANT_FIELD(age);RB_PLANT_FIELD(density);RB_PLANT_FIELD(irregularity);RB_PLANT_FIELD(gravity);RB_PLANT_FIELD(light);
  RB_PLANT_FIELD(branches);RB_PLANT_FIELD(depth);RB_PLANT_FIELD(segments);RB_PLANT_FIELD(sides);
  RB_PLANT_FIELD(branchAngle);RB_PLANT_FIELD(branchLength);RB_PLANT_FIELD(branchTaper);RB_PLANT_FIELD(dead);
  RB_PLANT_FIELD(leafSize);RB_PLANT_FIELD(leafWidth);RB_PLANT_FIELD(leafCurl);RB_PLANT_FIELD(leafTwist);
  RB_PLANT_FIELD(leaves);RB_PLANT_FIELD(leafShape);RB_PLANT_FIELD(foliageMode);RB_PLANT_FIELD(rootFlare);
  RB_PLANT_FIELD(flowerSize);RB_PLANT_FIELD(bloom);RB_PLANT_FIELD(dryness);RB_PLANT_FIELD(petals);RB_PLANT_FIELD(flowerLayers);RB_PLANT_FIELD(clumps);
  RB_PLANT_FIELD(capThickness);RB_PLANT_FIELD(gills);RB_PLANT_FIELD(ribs);RB_PLANT_FIELD(spines);
  RB_PLANT_FIELD(season);RB_PLANT_FIELD(quality);RB_PLANT_FIELD(budget);
  RB_PLANT_FIELD(lod1Ratio);RB_PLANT_FIELD(lod2Ratio);
  RB_PLANT_FIELD(windStrength);RB_PLANT_FIELD(windDirection);RB_PLANT_FIELD(windFrequency);RB_PLANT_FIELD(stiffness);RB_PLANT_FIELD(flutter);
  RB_PLANT_FIELD(bark);RB_PLANT_FIELD(leaf);RB_PLANT_FIELD(flower);RB_PLANT_FIELD(roughness);RB_PLANT_FIELD(ao);RB_PLANT_FIELD(variation);
  RB_PLANT_FIELD(growth);RB_PLANT_FIELD(leafProfile);RB_PLANT_FIELD(controls);
#undef RB_PLANT_FIELD
  return j;
}
inline Json serialize(const Project& d) {
  Json nodes=Json::array();
  for(auto& n:d.nodes)nodes.push_back({{"id",n.id},{"name",n.name},{"parent",n.parent},{"kind",int(n.kind)},
    {"visible",n.visible},{"offset",n.offset},{"scale",n.scale},{"rotation",n.rotation}});
  return {{"format","ReyondBroomPlant"},{"version",d.version},{"generatorVersion",d.generator},{"name",d.name},
    {"preset",d.preset},{"species",d.species},{"family",int(d.family)},{"seed",d.seed},{"parameters",parameters(d.p)},
    {"hierarchy",nodes},{"variations",d.variations},{"texture",d.texture}};
}
inline void validate(const Project& d) {
  if(d.version!=1||d.generator!=1)throw std::runtime_error("Unsupported plant project / generator version");
  if(int(d.family)<0||int(d.family)>11||d.name.size()>256||d.species.size()>80)throw std::runtime_error("Invalid plant identity");
  const auto values=parameters(d.p);
  for(auto& item:values.items()) {
    std::function<void(const Json&)> finite=[&](const Json& x){if(x.is_array())for(auto& v:x)finite(v);else if(!x.is_number()||!std::isfinite(x.get<double>()))throw std::runtime_error("Non-finite plant parameter: "+item.key());};
    finite(item.value());
  }
  const auto& p=d.p;
  auto bounded=[](double x,double low,double high){if(x<low||x>high)throw std::runtime_error("Plant parameter outside its supported range");};
  for(double x:{p.taper,p.age,p.irregularity,p.dead,p.bloom,p.dryness,p.capThickness,p.gills,p.spines,p.roughness,p.ao,p.variation,p.stiffness,p.flutter})bounded(x,0,1);
  bounded(p.curvature,0,2);bounded(p.spread,.05,3);bounded(p.density,0,2);bounded(p.gravity,-2,2);bounded(p.light,-1,1);
  bounded(p.branchAngle,5,140);bounded(p.branchLength,.05,2);bounded(p.branchTaper,.05,1);bounded(p.leafWidth,.01,2);
  bounded(p.leafCurl,-2,2);bounded(p.leafTwist,-2,2);bounded(p.rootFlare,0,4);bounded(p.ribs,3,24);bounded(p.windFrequency,.01,10);bounded(p.windDirection,-360,360);
  bounded(p.lod1Ratio,.1,.9);bounded(p.lod2Ratio,.03,.8);if(p.lod2Ratio>=p.lod1Ratio)throw std::runtime_error("LOD2 budget must be below LOD1");
  for(auto color:{p.bark,p.leaf,p.flower})for(double x:color)bounded(x,0,1);
  for(double x:p.growth)bounded(x,0,2);for(double x:p.leafProfile)bounded(x,0,2);
  for(auto v:p.controls)for(double x:v)bounded(x,-2,2);
  if(p.height<.1||p.height>2048||p.radius<.01||p.radius>200||p.branches<(d.family==Family::Cactus?0:1)||p.branches>48||p.depth<1||p.depth>4||
    p.segments<3||p.segments>20||p.sides<3||p.sides>20||p.leaves<1||p.leaves>40||p.clumps<1||p.clumps>40||
    p.budget<256||p.budget>180000||p.petals<3||p.petals>40||p.flowerLayers<1||p.flowerLayers>8||
    p.season<0||p.season>3||p.quality<0||p.quality>3||p.foliageMode<0||p.foliageMode>4||p.leafShape<0||p.leafShape>7||
    p.leafSize<.01||p.leafSize>160||p.flowerSize<.01||p.flowerSize>240||p.windStrength<0||p.windStrength>4)
    throw std::runtime_error("Plant parameters exceed safe generation bounds");
  if(d.nodes.size()>64||d.variations.size()>64)throw std::runtime_error("Plant hierarchy / variations exceed 64 entries");
  std::set<std::string> ids;
  for(auto& n:d.nodes) {
    if(n.id.empty()||n.id.size()>80||!ids.insert(n.id).second||int(n.kind)<0||int(n.kind)>7)throw std::runtime_error("Invalid plant component ID");
    for(double x:n.offset)if(!std::isfinite(x)||std::abs(x)>4096)throw std::runtime_error("Invalid component offset");
    for(double x:n.scale)if(!std::isfinite(x)||x<.01||x>8)throw std::runtime_error("Invalid component scale");
    if(!std::isfinite(n.rotation))throw std::runtime_error("Invalid component rotation");
  }
  for(auto& n:d.nodes) {
    std::set<std::string> visited{n.id};auto parent=n.parent;
    while(!parent.empty()) {
      auto it=std::find_if(d.nodes.begin(),d.nodes.end(),[&](auto& v){return v.id==parent;});
      if(it==d.nodes.end()||!visited.insert(parent).second)throw std::runtime_error("Missing or cyclic plant parent");
      parent=it->parent;
    }
  }
}
inline Project deserialize(const Json& j) {
  if(j.value("format","")!="ReyondBroomPlant")throw std::runtime_error("Not a ReyondBroom plant project");
  Project d;d.version=j.at("version");d.generator=j.at("generatorVersion");d.name=j.at("name");d.preset=j.at("preset");
  d.species=j.at("species");d.family=Family(j.at("family").get<int>());d.seed=j.at("seed");auto& p=d.p;auto& a=j.at("parameters");
#define RB_PLANT_FIELD(n) if(a.contains(#n))a.at(#n).get_to(p.n)
  RB_PLANT_FIELD(height);RB_PLANT_FIELD(radius);RB_PLANT_FIELD(taper);RB_PLANT_FIELD(curvature);RB_PLANT_FIELD(spread);
  RB_PLANT_FIELD(age);RB_PLANT_FIELD(density);RB_PLANT_FIELD(irregularity);RB_PLANT_FIELD(gravity);RB_PLANT_FIELD(light);
  RB_PLANT_FIELD(branches);RB_PLANT_FIELD(depth);RB_PLANT_FIELD(segments);RB_PLANT_FIELD(sides);RB_PLANT_FIELD(branchAngle);RB_PLANT_FIELD(branchLength);RB_PLANT_FIELD(branchTaper);RB_PLANT_FIELD(dead);
  RB_PLANT_FIELD(leafSize);RB_PLANT_FIELD(leafWidth);RB_PLANT_FIELD(leafCurl);RB_PLANT_FIELD(leafTwist);RB_PLANT_FIELD(leaves);RB_PLANT_FIELD(leafShape);RB_PLANT_FIELD(foliageMode);RB_PLANT_FIELD(rootFlare);
  RB_PLANT_FIELD(flowerSize);RB_PLANT_FIELD(bloom);RB_PLANT_FIELD(dryness);RB_PLANT_FIELD(petals);RB_PLANT_FIELD(flowerLayers);RB_PLANT_FIELD(clumps);RB_PLANT_FIELD(capThickness);RB_PLANT_FIELD(gills);RB_PLANT_FIELD(ribs);RB_PLANT_FIELD(spines);
  RB_PLANT_FIELD(season);RB_PLANT_FIELD(quality);RB_PLANT_FIELD(budget);RB_PLANT_FIELD(windStrength);RB_PLANT_FIELD(windDirection);RB_PLANT_FIELD(windFrequency);RB_PLANT_FIELD(stiffness);RB_PLANT_FIELD(flutter);
  RB_PLANT_FIELD(lod1Ratio);RB_PLANT_FIELD(lod2Ratio);
  RB_PLANT_FIELD(bark);RB_PLANT_FIELD(leaf);RB_PLANT_FIELD(flower);RB_PLANT_FIELD(roughness);RB_PLANT_FIELD(ao);RB_PLANT_FIELD(variation);RB_PLANT_FIELD(growth);RB_PLANT_FIELD(leafProfile);RB_PLANT_FIELD(controls);
#undef RB_PLANT_FIELD
  for(auto& n:j.at("hierarchy"))d.nodes.push_back({n.at("id"),n.at("name"),n.at("parent"),Part(n.at("kind").get<int>()),n.at("visible"),n.at("offset").get<V>(),n.at("scale").get<V>(),n.at("rotation")});
  d.variations=j.value("variations",std::vector<uint32_t>{});d.texture=j.value("texture",std::string{});validate(d);return d;
}
}
