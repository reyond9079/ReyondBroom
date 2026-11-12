// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Included inside worldArchitect after the convex collision implementation.
inline void spatialQuality(WorldPlan& w,const Settings& settings,const Collision& collision,const std::vector<std::vector<size_t>>& adjacency){
  auto& scores=w.diagnostics.score;const auto& nav=w.navigation;const size_t samples=std::min<size_t>(32,nav.nodes.size());if(!samples)return;
  Player ray{.1,.1,0};double eye=std::min(48.,settings.player.height-4);std::array<int,3> ranges{};int cover=0,visibleLandmarks=0,landmarkTests=0;
  for(size_t i=0;i<samples;i++){
    auto feet=nav.nodes[i*nav.nodes.size()/samples].feet,origin=feet+V{0,0,eye};bool lowCover=false;
    for(int direction=0;direction<8;direction++){
      double angle=direction*6.283185307179586/8;V axis{std::cos(angle),std::sin(angle),0};double distance=1024;
      for(int step=1;step<=16;step++)if(collision.blocked(origin,origin+axis*(step*64.),ray)){distance=step*64.-32;break;}
      ranges[distance<192?0:distance<512?1:2]++;
      if(distance>=192&&collision.blocked(feet+V{0,0,24},feet+V{0,0,24}+axis*96,ray))lowCover=true;
    }
    if(lowCover)cover++;
    for(const auto& beat:w.mission.nodes)if(beat.role==Role::Landmark&&al::length(beat.position-feet)<=1024){landmarkTests++;if(!collision.blocked(origin,beat.position+V{0,0,eye},ray))visibleLandmarks++;}
  }
  double entropy=0;for(int count:ranges)if(count){double p=double(count)/(samples*8);entropy-=p*std::log(p);}
  scores["visibility"]=entropy/std::log(3.);scores["shortSightlines"]=double(ranges[0])/(samples*8);scores["mediumSightlines"]=double(ranges[1])/(samples*8);scores["longSightlines"]=double(ranges[2])/(samples*8);
  scores["cover"]=double(cover)/samples;scores["landmarkVisibility"]=landmarkTests?double(visibleLandmarks)/landmarkTests:0;
  double low=1e30,high=-1e30;size_t choices=0;for(size_t i=0;i<nav.nodes.size();i++){
    const auto p=nav.nodes[i].feet;low=std::min(low,p.z);high=std::max(high,p.z);std::set<int> directions;
    for(auto next:adjacency[i]){auto delta=nav.nodes[next].feet-p;if(al::length(delta)<48)continue;int bin=int(std::floor((std::atan2(delta.y,delta.x)+3.141592653589793)*8/6.283185307179586))%8;if(std::abs(delta.z)>settings.player.step)bin+=delta.z>0?8:16;directions.insert(bin);}
    if(directions.size()>=3)choices++;
  }
  scores["verticality"]=std::min(1.,(high-low)/768.);scores["choiceAnchors"]=double(choices);scores["reachableHeight"]=high-low;
  auto spawn=nav.nodes[nav.mainRoute.empty()?0:nav.mainRoute.front()].feet+V{0,0,eye};int open=0;
  for(int direction=0;direction<8;direction++){double angle=direction*6.283185307179586/8;V axis{std::cos(angle),std::sin(angle),0};if(!collision.blocked(spawn,spawn+axis*768,ray))open++;}scores["spawnExposure"]=open/8.;
  size_t furnished=0,rooms=0;for(const auto& b:w.spatial.buildings)for(const auto& room:b.rooms){rooms++;if(std::any_of(w.assets.begin(),w.assets.end(),[&](const auto& asset){return asset.region==b.id&&inside(asset.position,room.low,room.high);}))furnished++;}scores["furnishedRooms"]=double(furnished);scores["roomAssetCoverage"]=rooms?double(furnished)/rooms:0;
  scores["damageRouteIntegrity"]=nav.reachable==nav.nodes.size()?1:0;
  int transitions=0;auto interior=[&](V p){for(const auto& b:w.spatial.buildings)if(inside(p,b.low,b.high))return true;return false;};
  for(size_t i=1;i<nav.mainRoute.size();i++)if(interior(nav.nodes[nav.mainRoute[i-1]].feet)!=interior(nav.nodes[nav.mainRoute[i]].feet))transitions++;scores["indoorOutdoorTransitions"]=transitions;
  w.diagnostics.warnings.push_back("Visibility uses up to 32 physical anchors and eight eye-height rays each (64-unit samples, 1024-unit cap). Cover means low obstruction with clear eye-height view. Choice anchors group neighbor directions; these are static brush heuristics, not enemy AI, lighting or proof of enjoyable gameplay.");
}
