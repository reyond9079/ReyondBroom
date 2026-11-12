// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Included inside worldArchitect after functional rooms and brush primitives.
inline uint64_t missionRegion(const WorldPlan& w,const MissionNode& beat);
inline void realizeGates(WorldPlan& w,const Settings& settings,const Registry& registry){
  auto requested=missionGates(w.mission);if(requested.empty())return;
  if(w.profile!=0&&w.profile!=1&&w.profile!=2){w.diagnostics.warnings.push_back("This profile retains prerequisite mission metadata; physical door/activation entities are not certified.");return;}
  if(settings.gateMode=="semantic"||(settings.gateMode=="auto"&&w.profile==1)){w.diagnostics.warnings.push_back("Prerequisites are semantic metadata for this backend. FTE Custom needs an explicit standard door/activation backend and compatible game code to instantiate runtime gates.");return;}
  auto node=[&](uint64_t id)->const MissionNode&{for(const auto& n:w.mission.nodes)if(n.id==id)return n;throw std::runtime_error("Gate refers to a missing mission beat");};
  std::set<uint64_t> litKeys;uint64_t id=0;for(const auto& [destination,prerequisite]:requested){
    const auto& target=node(destination);const auto& key=node(prerequisite);const Room* room=nullptr;const Building* building=nullptr;
    for(const auto& b:w.spatial.buildings)for(const auto& r:b.rooms)if(r.id==target.room){room=&r;building=&b;}
    if(!room||!building)throw std::runtime_error("Physical gates require a destination inside a functional room");
    // Multiple real entrances need a coordinated gate composition, rather
    // than labelling one door as a lock that the player can walk around.
    const auto* recipe=w.stageInputs[1].get();if(recipe&&stagePreset(*recipe,registry).scenario=="mall"&&!room->floor&&room->low.y>(building->low.y+building->high.y)/2)throw std::runtime_error("A mall service room needs both entrances gated; choose a single-entrance destination");
    if(room->function=="garage")throw std::runtime_error("Garage gates need coordinated doorway/shutter controls");
    auto entry=room->entry;double half=std::min((room->high.x-room->low.x-16)/2,room->mission?56.:40.);
    if(half<settings.player.radius+8)throw std::runtime_error("Gate doorway does not clear the configured player");
    // A lock must not be bypassable through a damaged facade, window or roof.
    // Add a closed, damage-protected room lining with the one real doorway.
    auto firstProtected=w.architecture.elements.size();V liningLow=room->low-V{4,4,0},liningHigh=room->high+V{4,4,0};
    box(w,building->id,"gate protected side",{liningLow.x,liningLow.y,room->low.z},{room->low.x,liningHigh.y,room->high.z},1);
    box(w,building->id,"gate protected side",{room->high.x,liningLow.y,room->low.z},{liningHigh.x,liningHigh.y,room->high.z},1);
    if(room->low.y>(building->low.y+building->high.y)/2)box(w,building->id,"gate protected facade",{liningLow.x,room->high.y,room->low.z},{liningHigh.x,liningHigh.y,room->high.z},1);
    else box(w,building->id,"gate protected facade",{liningLow.x,liningLow.y,room->low.z},{liningHigh.x,room->low.y,room->high.z},1);
    wall(w,building->id,{liningLow.x,entry.y,0},{liningHigh.x,entry.y,0},room->low.z,room->high.z,16,1,{{entry.x-liningLow.x-half,entry.x-liningLow.x+half,room->low.z,room->low.z+112}});
    box(w,building->id,"gate protected ceiling",{liningLow.x,liningLow.y,room->high.z},{liningHigh.x,liningHigh.y,room->high.z+4},1);
    box(w,building->id,"gate protected floor",{liningLow.x,liningLow.y,room->low.z-4},{liningHigh.x,liningHigh.y,room->low.z},0);
    for(size_t i=firstProtected;i<w.architecture.elements.size();i++)w.architecture.elements[i].kind="gate protected enclosure";
    Gate gate{++id,target.id,key.id,building->id,missionRegion(w,key),key.position};w.gates.push_back(gate);auto identity=std::to_string(gate.id),name="wa_gate_"+identity;
    ArchitectureBrushEntity door;door.region=gate.region;door.entity.fields={{"classname","func_door"},{"targetname",name},{"angle","-1"},{"speed","128"},{"wait","-1"},{"lip","0"},{"_wa_gate",identity},{"_wa_node",std::to_string(target.id)},{"_wa_prerequisite",std::to_string(key.id)}};
    door.brushes.push_back(al::box({entry.x-half,entry.y-6,room->low.z},{entry.x+half,entry.y+6,room->low.z+112},material(w,5)));w.architecture.brushEntities.push_back(std::move(door));
    double radius=settings.player.radius+8;V low=key.position-V{radius,radius,1},high=key.position+V{radius,radius,settings.player.height+8};
    if(!inside(low,w.low,w.high)||!inside(high,w.low,w.high))throw std::runtime_error("Prerequisite activation volume leaves the sealed site");
    ArchitectureBrushEntity trigger;trigger.region=gate.activationRegion;trigger.entity.fields={{"classname","trigger_multiple"},{"target",name},{"wait","-1"},{"_wa_gate",identity},{"_wa_node",std::to_string(target.id)},{"_wa_prerequisite",std::to_string(key.id)}};
    auto brush=al::box(low,high,w.profile<2?"wa_gate_trigger":"trigger");if(w.profile<2)for(auto& face:brush.faces)face.attributes="1073741824 0 0";trigger.brushes.push_back(std::move(brush));w.architecture.brushEntities.push_back(std::move(trigger));
    if(litKeys.insert(key.id).second)w.architecture.entities.push_back({gate.activationRegion,{{{"classname","light"},{"origin",vectorText(key.position+V{0,0,40})},{"light","140"},{"_color","0.25 1 0.35"}}}});
    if(brushCount(w)>16000)throw std::runtime_error("Physical gate geometry exceeds the brush budget");
  }
}

inline bool activateGates(const WorldPlan& w,V feet,const Player& player,std::set<uint64_t>& opened){
  bool changed=false;double radius=player.radius*2+8;for(const auto& gate:w.gates)if(std::abs(feet.x-gate.activation.x)<=radius&&std::abs(feet.y-gate.activation.y)<=radius&&feet.z<gate.activation.z+player.height+8&&feet.z+player.height>gate.activation.z-1)changed=opened.insert(gate.id).second||changed;return changed;
}
