// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/plant/export.h"
#include "../native/plant/scale.h"
#include <iostream>
using namespace ftebroom::plant;
static int checks=0;
static void require(bool value,const std::string& label){if(!value)throw std::runtime_error(label);++checks;}
static Project find(const std::vector<Project>& library,const char* id){for(auto& p:library)if(p.preset==id)return p;throw std::runtime_error(id);}
int main() try {
  const auto original=presets();const auto saved=serialize(original.front());
  auto authored=original.front();authored.p.height=123;authored.nodes[1].offset={2,3,4};
  const auto authoredJson=serialize(authored);
  for(int profile=0;profile<5;profile++) {
    auto library=presetsForProfile(profile);
    require(library.size()==original.size(),"all presets available for each profile");
    for(auto& p:library){validate(p);require(serialize(deserialize(serialize(p)))==serialize(p),p.preset+" scaled defaults persist exactly");}
    const double human=(profile==1||profile==4)?72.:56.;
    require(humanHeightUnits(profile)==human,"standing player hull uses game convention");
    struct Range{const char* id;double low,high;};
    for(auto range:{Range{"oak",7,18},Range{"apple",1.5,4},Range{"lawn-grass",.03,.12},
        Range{"meadow-grass",.10,.35},Range{"amanita",.04,.2},Range{"boxwood",.25,.7},
        Range{"dwarf-bamboo",.15,.5},Range{"saguaro",3,7},Range{"fantasy",1,2.5}}) {
      auto p=find(library,range.id);auto mesh=generate(p);validate(mesh);
      const double ratio=(mesh.high[2]-mesh.low[2])/human;
      std::cout<<profile<<" "<<range.id<<" "<<ratio<<" player heights\n";
      require(ratio>=range.low&&ratio<=range.high,std::string(range.id)+" actual mesh size fits its botanical stature");
      auto runtime=targetGeometry(p,mesh,profile);validate(runtime);
      const double gameRatio=(runtime.high[2]-runtime.low[2])/human;
      require(gameRatio>=range.low&&gameRatio<=range.high,std::string(range.id)+" game LOD retains botanical stature");
    }
    require(serialize(deserialize(authoredJson))==authoredJson,"authored dimensions are not rescaled on open");
  }
  auto fte=presetsForProfile(1),hl=presetsForProfile(4),quake=presetsForProfile(2);
  for(size_t i=0;i<fte.size();i++) {
    require(serialize(fte[i])==serialize(hl[i]),"FTE defaults use Half-Life scale");
    require(std::abs(fte[i].p.height/quake[i].p.height-72./56.)<1e-12,"cross-game defaults preserve stature relative to player");
  }
  require(serialize(presets().front())==saved,"original presets remain immutable");
  std::cout<<checks<<" plant scale checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
