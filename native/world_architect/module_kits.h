// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Included inside worldArchitect after the navigation and brush primitives.
inline bool authoredRoom(WorldPlan& w,const Settings& settings,const Building& b,const Room& room){
  auto assignment=settings.roomModules.find(room.id);if(assignment==settings.roomModules.end())return false;
  auto found=std::find_if(settings.modules.begin(),settings.modules.end(),[&](const auto& m){return m.id==assignment->second;});
  if(found==settings.modules.end())throw std::runtime_error("Assigned authored module is missing");const auto& module=*found;
  if(module.profile!=w.profile||(module.function!="any"&&module.function!=room.function))throw std::runtime_error("Authored module profile or functional program does not match its assigned room");
  auto geometry=al::parseMap(module.map);if(geometry.brushes.empty()||geometry.brushes.size()>128||geometry.entities.size()>32)throw std::runtime_error("Authored module exceeds its geometry budget");
  V low{1e30,1e30,1e30},high{-1e30,-1e30,-1e30};
  for(const auto& brush:geometry.brushes){al::validateBrush(brush);auto poly=al::polyhedron(brush);for(int a=0;a<3;a++){low[a]=std::min(low[a],poly.min[a]);high[a]=std::max(high[a],poly.max[a]);}}
  V anchor{(room.low.x+room.high.x)/2,(room.low.y+room.high.y)/2,room.low.z},translation=anchor-V{(low.x+high.x)/2,(low.y+high.y)/2,low.z};
  geometry=al::translateGeometry(geometry,translation);WorldPlan candidate;candidate.profile=w.profile;candidate.theme=w.theme;candidate.assetKey=w.assetKey;
  for(auto& brush:geometry.brushes){
    auto poly=al::polyhedron(brush);for(int a=0;a<3;a++)if(poly.min[a]<room.low[a]+(a==2?-.01:8)||poly.max[a]>room.high[a]-8)throw std::runtime_error("Authored module does not fit the room without rescaling or clipping");
    for(auto& face:brush.faces){auto alias=module.materials.find(face.material);if(alias==module.materials.end())throw std::runtime_error("Every authored material needs an explicit portable theme slot");face.material=material(w,alias->second);}
    addBrush(candidate,b.id,"authored module / "+module.id,std::move(brush));
  }
  Collision hull(candidate);
  for(const auto& node:w.navigation.nodes)if(hull.blocked(node.feet,node.feet,settings.player))throw std::runtime_error("Authored module obstructs a reserved player anchor");
  for(auto edge:w.navigation.edges)if(hull.blocked(w.navigation.nodes[edge[0]].feet,w.navigation.nodes[edge[1]].feet,settings.player))throw std::runtime_error("Authored module obstructs a reserved player connection");
  for(auto& entity:geometry.entities){
    auto name=entity.fields.find("classname");if(name==entity.fields.end()||(name->second!="light"&&name->second!="info_null"))throw std::runtime_error("Authored modules currently support light and semantic point entities; other source entities remain in the preserved MAP");
    auto origin=entity.fields.find("origin");if(origin==entity.fields.end())throw std::runtime_error("Authored module point entity needs an origin");std::istringstream text(origin->second);V point;if(!(text>>point.x>>point.y>>point.z)||!inside(point,room.low,room.high))throw std::runtime_error("Authored module point entity leaves its room");
    for(auto it=entity.fields.begin();it!=entity.fields.end();)if(it->first.find("_tb_")==0||it->first.find("_wa_")==0)it=entity.fields.erase(it);else ++it;
  }
  if(brushCount(w)+candidate.architecture.elements.size()>16000)throw std::runtime_error("Authored modules exceed the world brush budget");
  for(auto& element:candidate.architecture.elements)w.architecture.elements.push_back(std::move(element));
  for(auto& entity:geometry.entities)w.architecture.entities.push_back({b.id,std::move(entity)});
  auto key=module.id.substr(0,48)+"-"+std::to_string(hash(moduleJson(module).dump()));w.authoredSources.emplace(key,module);
  w.assets.push_back({b.id,"authored room module / "+module.id,"authored: "+module.source+" / "+module.license,anchor});return true;
}
