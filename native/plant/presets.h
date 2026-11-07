// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "schema.h"
namespace ftebroom::plant {
inline std::vector<Project> presets() {
  std::vector<Project> out;
  auto make=[&](const char* id,const char* name,Family family,double height) -> Project& {
    Project d;d.preset=d.species=id;d.name=name;d.family=family;d.nodes=defaultNodes();d.p.height=height;
    if(family==Family::Flower||family==Family::Vine||family==Family::Fern)d.p.bark={.20,.34,.10};
    if(family==Family::Mushroom)d.p.bark={.82,.77,.65};
    out.push_back(d);return out.back();
  };
  out.reserve(32);
  {auto& d=make("oak","English oak",Family::Broadleaf,240);d.p.spread=.86;d.p.rootFlare=1.6;d.p.leafShape=3;d.p.leafSize=18;d.p.leaves=24;d.p.density=1.1;}
  {auto& d=make("birch","Silver birch",Family::Broadleaf,280);d.p.radius=4;d.p.spread=.43;d.p.gravity=.35;d.p.bark={.82,.80,.72};d.p.leafSize=11;d.p.leafShape=1;}
  {auto& d=make("maple","Field maple",Family::Broadleaf,210);d.p.spread=.9;d.p.branchAngle=64;d.p.leafShape=4;d.p.leafWidth=.9;d.p.leaf={.29,.48,.13};}
  {auto& d=make("willow","Weeping willow",Family::Broadleaf,245);d.p.gravity=1.35;d.p.spread=.93;d.p.branchLength=.95;d.p.leafSize=18;d.p.leafWidth=.16;d.p.leafShape=2;d.p.branches=18;d.p.leaves=40;d.p.foliageMode=1;d.p.density=1.4;}
  {auto& d=make("pine","Scots pine",Family::Conifer,300);d.p.radius=5;d.p.spread=.48;d.p.branches=10;d.p.leaves=12;d.p.leafShape=5;d.p.leafSize=14;d.p.leafWidth=1.1;d.p.leaf={.12,.29,.17};d.p.branchAngle=72;}
  {auto& d=make("spruce","Norway spruce",Family::Conifer,290);d.p.radius=5;d.p.spread=.41;d.p.branches=16;d.p.gravity=.65;d.p.leafShape=5;d.p.leafSize=11;d.p.leafWidth=1;d.p.leaf={.10,.24,.19};d.p.density=1.1;}
  {auto& d=make("date-palm","Date palm",Family::Palm,280);d.p.radius=10;d.p.branches=14;d.p.spread=.78;d.p.leafSize=30;d.p.leaves=18;d.p.gravity=.6;}
  {auto& d=make("coconut-palm","Coconut palm",Family::Palm,340);d.p.radius=7;d.p.branches=10;d.p.curvature=.65;d.p.spread=.95;d.p.leafSize=38;d.p.leaves=24;d.p.gravity=.9;d.p.bark={.42,.34,.24};}
  {auto& d=make("wild-shrub","Wild hawthorn shrub",Family::Bush,82);d.p.radius=1.8;d.p.clumps=5;d.p.spread=.95;d.p.branches=8;d.p.leafSize=8;d.p.leafShape=3;d.p.foliageMode=1;}
  {auto& d=make("flowering-shrub","Flowering dog rose",Family::Bush,96);d.p.radius=1.5;d.p.clumps=4;d.p.branches=7;d.p.flower={.88,.37,.51};d.p.flowerSize=7;d.p.petals=5;d.p.leafShape=1;d.p.leafSize=7.5;d.p.foliageMode=1;}
  {auto& d=make("hedge","Clipped hornbeam hedge",Family::Bush,104);d.p.clumps=7;d.p.radius=1.8;d.p.branches=6;d.p.spread=.65;d.p.leafSize=9;d.p.leafWidth=.7;d.p.density=1.2;d.p.foliageMode=1;}
  {auto& d=make("boxwood","Round boxwood",Family::Bush,64);d.p.clumps=5;d.p.radius=1.2;d.p.branches=8;d.p.spread=.85;d.p.leafSize=6;d.p.leafShape=1;d.p.density=1.2;d.p.leaf={.17,.32,.10};d.p.foliageMode=1;}
  {auto& d=make("meadow-grass","Meadow grass clump",Family::Grass,38);d.p.branches=40;d.p.clumps=4;d.p.leafWidth=.045;d.p.curvature=.5;d.p.leaf={.31,.46,.13};d.p.leafCurl=.3;}
  {auto& d=make("dry-grass","Dry fountain grass",Family::Grass,56);d.p.branches=38;d.p.clumps=5;d.p.leafWidth=.035;d.p.curvature=.8;d.p.dryness=.8;d.p.flowerSize=7;d.p.leaf={.54,.48,.26};}
  {auto& d=make("bracken","Common bracken fern",Family::Fern,65);d.p.branches=9;d.p.leaves=18;d.p.leafSize=11;d.p.radius=.55;d.p.leafWidth=.25;d.p.gravity=.65;}
  {auto& d=make("daisy","Oxeye daisy",Family::Flower,42);d.p.radius=.4;d.p.flowerSize=13;d.p.petals=18;d.p.clumps=3;d.p.flower={.95,.93,.85};d.p.leafSize=11;d.p.leafShape=2;}
  {auto& d=make("sunflower","Wild sunflower",Family::Flower,90);d.p.radius=1;d.p.flowerSize=22;d.p.petals=26;d.p.flowerLayers=2;d.p.leafSize=18;d.p.leafWidth=.8;d.p.flower={.96,.67,.06};}
  {auto& d=make("rose","Garden rose",Family::Flower,60);d.p.radius=.55;d.p.flowerSize=13;d.p.petals=9;d.p.flowerLayers=5;d.p.clumps=2;d.p.leafSize=16;d.p.flower={.66,.12,.22};d.p.leafShape=7;}
  {auto& d=make("amanita","Fly agaric",Family::Mushroom,32);d.p.radius=2.5;d.p.spread=.72;d.p.clumps=3;d.p.flower={.70,.10,.055};d.p.gills=.9;}
  {auto& d=make("boletus","Porcini bolete",Family::Mushroom,29);d.p.radius=4.5;d.p.spread=.8;d.p.capThickness=.4;d.p.flower={.34,.19,.08};d.p.gills=.15;d.p.clumps=2;}
  {auto& d=make("oyster","Oyster mushroom cluster",Family::Mushroom,25);d.p.radius=1.8;d.p.spread=1.15;d.p.clumps=5;d.p.flower={.56,.51,.42};d.p.gills=1;}
  {auto& d=make("chanterelle","Golden chanterelle",Family::Mushroom,28);d.p.radius=2.3;d.p.spread=.65;d.p.capThickness=.12;d.p.flower={.91,.55,.10};d.p.gills=.75;d.p.clumps=4;}
  {auto& d=make("woodland","Woodland inkcap",Family::Mushroom,24);d.p.radius=.8;d.p.spread=.4;d.p.clumps=8;d.p.flower={.61,.45,.26};d.p.gills=.9;}
  {auto& d=make("fantasy","Giant violet parasol",Family::Mushroom,150);d.p.radius=9;d.p.spread=.95;d.p.capThickness=.15;d.p.flower={.42,.23,.61};d.p.gills=1;d.p.clumps=1;}
  {auto& d=make("saguaro","Saguaro cactus",Family::Cactus,180);d.p.radius=13;d.p.branches=4;d.p.ribs=12;d.p.spines=.6;d.p.leaf={.28,.42,.23};d.p.flowerSize=9;}
  {auto& d=make("barrel-cactus","Golden barrel cactus",Family::Cactus,48);d.p.radius=24;d.p.ribs=18;d.p.spines=.9;d.p.flower={.94,.69,.14};d.p.leaf={.27,.38,.15};}
  {auto& d=make("bamboo","Golden bamboo grove",Family::Bamboo,210);d.p.radius=2;d.p.clumps=5;d.p.branches=10;d.p.leafSize=20;d.p.leafWidth=.16;d.p.leafShape=2;d.p.bark={.47,.49,.20};}
  {auto& d=make("reeds","Common reed stand",Family::Bamboo,130);d.p.radius=.55;d.p.clumps=11;d.p.branches=7;d.p.leafSize=30;d.p.leafWidth=.09;d.p.flowerSize=14;d.p.bark={.45,.45,.24};}
  {auto& d=make("ivy","English ivy vine",Family::Vine,155);d.p.radius=.65;d.p.leafShape=4;d.p.leafSize=13;d.p.leafWidth=.9;d.p.leaves=25;d.p.curvature=.65;d.p.spread=.5;}
  {auto& d=make("climbing-vine","Wild climbing vine",Family::Vine,210);d.p.radius=.85;d.p.leafShape=1;d.p.leafSize=16;d.p.leafWidth=.65;d.p.leaves=28;d.p.curvature=1.1;d.p.spread=.8;}
  return out;
}
}
