// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "plan.h"

namespace ftebroom::worldArchitect {
inline std::string vectorText(V p){std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(12)<<p.x<<' '<<p.y<<' '<<p.z;return out.str();}
inline void cancelled(const std::atomic<bool>* flag){if(flag&&flag->load())throw std::runtime_error("Generation cancelled");}
inline std::string assetId(const WorldPlan& w,int slot){return slot==6?"wa_water_"+w.theme.substr(0,7)+(w.assetKey.empty()?"":"_"+w.assetKey):"wa_"+w.theme.substr(0,w.assetKey.empty()?7:4)+(w.assetKey.empty()?"":"_"+w.assetKey)+std::to_string(slot);}
inline std::string classicWaterId(const WorldPlan& w,bool hl=false){return (hl?"!wa":"*wa")+w.theme.substr(0,w.assetKey.empty()?7:5)+(w.assetKey.empty()?"":"_"+w.assetKey);}
inline std::string materialId(const WorldPlan& w,int slot){return slot==6&&w.profile>=2?classicWaterId(w,w.profile==4):assetId(w,slot);}
inline std::string material(const WorldPlan& w,int slot){return materialId(w,slot);}
inline std::string classicSkyId(const WorldPlan& w){return w.assetKey.empty()?"skywa"+w.theme.substr(0,7):"sky"+w.theme.substr(0,5)+"_"+w.assetKey;}
inline std::string skyMaterial(const WorldPlan& w){return w.profile<2?"wa_sky_"+w.theme+(w.assetKey.empty()?"":"_"+w.assetKey):classicSkyId(w);}
inline bool inside(V p,V lo,V hi){return p.x>=lo.x&&p.y>=lo.y&&p.z>=lo.z&&p.x<=hi.x&&p.y<=hi.y&&p.z<=hi.z;}
inline bool overlap(V lo,V hi,V otherLo,V otherHi,double gap=0){return lo.x<otherHi.x+gap&&hi.x>otherLo.x-gap&&lo.y<otherHi.y+gap&&hi.y>otherLo.y-gap&&lo.z<otherHi.z+gap&&hi.z>otherLo.z-gap;}
inline uint64_t regionSeed(const Settings& s,uint64_t id,const char* stage){auto it=s.regionSeeds.find(id);return subSeed(it==s.regionSeeds.end()?s.seed:it->second,stage,id);}
inline Settings regionInput(const Settings& s,uint64_t id,int stage,const Registry& registry){
  auto found=s.regionRecipes.find(id);return found==s.regionRecipes.end()?s:deserialize(found->second.at(size_t(stage-2)),registry,true);
}
inline size_t navNode(WorldPlan& w,V p,uint64_t region,bool required=true){if(w.navigation.nodes.size()>=4096)throw std::runtime_error("World Architect navigation exceeds 4096 nodes");w.navigation.nodes.push_back({p,region,required});return w.navigation.nodes.size()-1;}
inline void navEdge(WorldPlan& w,size_t a,size_t b){if(a!=b)w.navigation.edges.push_back({a,b});}
inline void addBrush(WorldPlan& w,uint64_t region,const std::string& kind,al::Brush brush){
  if(brushCount(w)>=16000)throw std::runtime_error("World Architect exceeds the 16000 brush budget");al::validateBrush(brush);
  const auto poly=al::polyhedron(brush);const double limit=capability(w.profile).coordinateLimit;for(int axis=0;axis<3;axis++)if(poly.min[axis]<-limit||poly.max[axis]>limit)throw std::runtime_error("Architectural brush exceeds profile coordinate limits");
  w.architecture.elements.push_back({region,kind,std::move(brush)});
}
inline void box(WorldPlan& w,uint64_t region,const std::string& kind,V lo,V hi,int slot){
  if(hi.x-lo.x<.5||hi.y-lo.y<.5||hi.z-lo.z<.5)return;
  addBrush(w,region,kind,al::box(lo,hi,material(w,slot)));
}
// Exact convex hull sweeps against brushes expanded by the configured player's
// AABB. An angled piece of rubble is tested against its planes, not its bounds.
class Collision {
  struct Solid {V low,high;std::vector<std::pair<V,double>> planes;};
  std::vector<Solid> solids;
public:
  explicit Collision(const WorldPlan& w,const std::set<uint64_t>* opened=nullptr,bool closedDoors=false){
    std::vector<const al::Brush*> brushes;for(const auto& element:w.architecture.elements)if(element.kind!="floodwater")brushes.push_back(&element.brush);
    if(closedDoors)for(const auto& entity:w.architecture.brushEntities)if(entity.entity.fields.at("classname")=="func_door"&&(!opened||!opened->count(std::stoull(entity.entity.fields.at("_wa_gate")))))for(const auto& brush:entity.brushes)brushes.push_back(&brush);
    solids.reserve(brushes.size());
    for(const auto* brush:brushes){
      auto poly=al::polyhedron(*brush);Solid solid;solid.low=poly.min;solid.high=poly.max;
      auto axis=[&](V direction){
        if(al::length(direction)<1e-8)return;direction=al::unit(direction);
        if(std::any_of(solid.planes.begin(),solid.planes.end(),[&](const auto& plane){return al::near(plane.first,direction,1e-7);}))return;
        if(solid.planes.size()>=256)throw std::runtime_error("Convex collision axis budget exceeded");
        double distance=-1e30;for(auto p:poly.vertices)distance=std::max(distance,al::dot(direction,p));solid.planes.push_back({direction,distance});
      };
      for(const auto& face:brush->faces)axis(face.normal());
      const V boxAxes[]={{1,0,0},{0,1,0},{0,0,1}};for(auto direction:boxAxes){axis(direction);axis(direction*-1);}
      std::vector<V> directions;
      for(const auto& polygon:poly.polygons)for(size_t i=0;i<polygon.size();i++){
        auto edge=polygon[(i+1)%polygon.size()]-polygon[i];if(al::length(edge)<1e-8)continue;edge=al::unit(edge);
        if(std::any_of(directions.begin(),directions.end(),[&](V other){return std::abs(al::dot(edge,other))>1-1e-8;}))continue;directions.push_back(edge);
        // Face normals alone over-expand angled corners. Box face normals and
        // brush-edge x box-edge axes complete the exact convex Minkowski hull.
        for(auto direction:boxAxes){auto normal=al::cross(edge,direction);axis(normal);axis(normal*-1);}
      }
      solids.push_back(std::move(solid));
    }
  }
  bool blocked(V start,V end,const Player& player)const{
    const V lo{std::min(start.x,end.x)-player.radius,std::min(start.y,end.y)-player.radius,std::min(start.z,end.z)},hi{std::max(start.x,end.x)+player.radius,std::max(start.y,end.y)+player.radius,std::max(start.z,end.z)+player.height};
    for(const auto& s:solids){if(!overlap(lo,hi,s.low,s.high,-.01))continue;double enter=0,leave=1;bool hit=true;
      for(auto [n,d]:s.planes){d+=std::abs(n.x)*player.radius+std::abs(n.y)*player.radius+std::max(0.,-n.z*player.height)-.01;
        double a=al::dot(n,start)-d,delta=al::dot(n,end-start);
        if(std::abs(delta)<1e-12){if(a>0){hit=false;break;}}
        else if(delta<0)enter=std::max(enter,-a/delta);else leave=std::min(leave,-a/delta);
        if(enter>leave){hit=false;break;}
      }if(hit&&enter<=1&&leave>=0)return true;
    }return false;
  }
  bool supported(V feet,const Player& player)const{
    Player foot{player.radius,.1,player.step};return blocked(feet,feet-V{0,0,player.step+2},foot);
  }
  bool walk(V a,V b,const Player& p)const{
    if(blocked(a,a,p)||blocked(b,b,p))return false;
    double dz=b.z-a.z;
    if(std::abs(dz)>.1&&std::abs(dz)<=p.step+.1){
      if(dz>0){auto high=a+V{0,0,dz};return !blocked(a,high,p)&&!blocked(high,b,p);}
      auto high=b+V{0,0,-dz};return !blocked(a,high,p)&&!blocked(high,b,p);
    }
    if(!blocked(a,b,p))return true;
    if(std::abs(dz)<.1){auto up=V{0,0,p.step};return !blocked(a,a+up,p)&&!blocked(a+up,b+up,p)&&!blocked(b+up,b,p);}
    return false;
  }
  std::optional<V> move(V from,V target,const Player& player)const{
    if(blocked(from,from,player))return {};
    auto base=target;
    if(blocked(target,target,player)){
      bool found=false;for(int rise=1;rise<=int(player.step);rise++){target=base+V{0,0,double(rise)};if(!blocked(target,target,player)){found=true;break;}}if(!found)return {};
    }
    for(int fall=0;fall<int(player.step);fall++){
      auto lower=target-V{0,0,1};if(blocked(lower,lower,player))break;target=lower;
    }
    if(!supported(target,player)||!walk(from,target,player))return {};return target;
  }

};
inline void site(WorldPlan& w,const Settings& s,const Preset& preset){
  Random rng(subSeed(s.seed,"Site"));const bool interior=preset.scope==Scope::Interior||preset.scope==Scope::Arena;
  int count=interior?1:preset.blocks;
  const double factor=s.scale*(interior?1:.66),street=preset.street;
  const int cols=count>1?2:1,rows=(count+cols-1)/cols;
  const double totalW=cols*(preset.width*factor+256)+(cols+1)*street,totalD=rows*(preset.depth*factor+256)+(rows+1)*street;
  struct Parcel {V low,high;};std::vector<Parcel> parcels;
  if(interior)parcels.push_back({{-preset.width*factor/2-128,-preset.depth*factor/2-128,0},{preset.width*factor/2+128,preset.depth*factor/2+128,0}});
  else if(preset.roadPattern=="radial"){
    // Cardinal parcels face a real central gathering space. Connector streets
    // follow parcel boundaries instead of crossing inaccessible building shells.
    double hub=std::max(preset.width*factor,preset.depth*factor)/2+192,setback=street/2+72;
    double halfW=preset.width*factor/2+setback,halfD=preset.depth*factor/2+setback;
    for(int i=0;i<count;i++){V center=i==0?V{hub+halfW,0,0}:i==1?V{-hub-halfW,0,0}:i==2?V{0,hub+halfD,0}:V{0,-hub-halfD,0};parcels.push_back({center-V{halfW,halfD,0},center+V{halfW,halfD,0}});}
    uint64_t id=1000;auto connect=[&](V a,V b){if(al::length(a-b)>1)w.spatial.roads.push_back({id++,a,b,street,"radial connector",false});};
    for(size_t i=0;i<parcels.size();i++){const auto& parcel=parcels[i];double y=(parcel.low.y+parcel.high.y)/2;
      if(!i)connect({0,0,0},{parcel.low.x,y,0});
      else if(i==1){connect({0,0,0},{parcel.high.x,y,0});connect({parcel.high.x,y,0},parcel.high);connect(parcel.high,{parcel.low.x,parcel.high.y,0});connect({parcel.low.x,parcel.high.y,0},{parcel.low.x,y,0});}
      else{connect({0,0,0},{parcel.low.x,0,0});connect({parcel.low.x,0,0},{parcel.low.x,y,0});}
    }
  }
  else{
    // Recursive binary parcel subdivision produces real T junctions and unequal
    // frontage/depth. Roads are the partition boundaries, not lines painted
    // through masses laid out on an unrelated regular grid.
    uint64_t roadId=1000;V low{-totalW/2+street/2,-totalD/2+street/2,0},high{totalW/2-street/2,totalD/2-street/2,0};
    auto road=[&](V a,V b,const char* hierarchy){w.spatial.roads.push_back({roadId++,a,b,street,hierarchy,false});};
    road(low,{high.x,low.y,0},"secondary");road({high.x,low.y,0},high,"secondary");road(high,{low.x,high.y,0},"secondary");road({low.x,high.y,0},low,"secondary");
    std::function<void(Parcel,int,int)> divide=[&](Parcel parcel,int leaves,int level){
      if(leaves==1){parcels.push_back(parcel);return;}
      int axis;
      if(!level)axis=preset.layout=="spine"?0:preset.layout=="courtyard"?1:0;
      else axis=(parcel.high.x-parcel.low.x)/(preset.width*factor+street)>(parcel.high.y-parcel.low.y)/(preset.depth*factor+street)?0:1;
      const int first=leaves/2;double fraction=.5+rng.between(-.035,.035);
      double cut=parcel.low[axis]+(parcel.high[axis]-parcel.low[axis])*fraction;
      auto left=parcel,right=parcel;left.high[axis]=right.low[axis]=cut;
      if(axis==0)road({cut,parcel.low.y,0},{cut,parcel.high.y,0},level?"local":"primary");
      else road({parcel.low.x,cut,0},{parcel.high.x,cut,0},level?"local":"primary");
      divide(left,first,level+1);divide(right,leaves-first,level+1);
    };
    divide({low,high},count,0);
  }
  double high=0;
  for(size_t i=0;i<parcels.size();i++){
    uint64_t id=100+i;Random local(regionSeed(s,id,"Site"));auto parcel=parcels[i];
    const double setback=interior?128:street/2+72;
    double width=std::min(preset.width*factor,(parcel.high.x-parcel.low.x)-2*setback),depth=std::min(preset.depth*factor,(parcel.high.y-parcel.low.y)-2*setback);
    if(!interior){width*=local.between(.92,1.);depth*=local.between(.9,1.);}
    if(width<672||depth<320)throw std::runtime_error("Parcel cannot fit the requested room/stair program; increase generation scale");
    double x=(parcel.low.x+parcel.high.x)/2,y=(parcel.low.y+parcel.high.y)/2;
    Building b;b.id=id;b.low={x-width/2,y-depth/2,0};b.high={x+width/2,y+depth/2,double(preset.floors*256)};b.floors=preset.floors;b.archetype=preset.archetype;
    if(!interior)b.floors=std::max(1,preset.floors+local.integer(-1,0));b.high.z=b.floors*256;
    b.pinned=std::find(s.pinnedRegions.begin(),s.pinnedRegions.end(),id)!=s.pinnedRegions.end();
    w.spatial.lots.push_back({id,parcel.low+V{interior?0:street/2,interior?0:street/2,0},V{parcel.high.x-(interior?0:street/2),parcel.high.y-(interior?0:street/2),b.high.z},preset.archetype});
    w.spatial.buildings.push_back(b);high=std::max(high,b.high.z);
    if(!interior)w.spatial.roads.push_back({2000+uint64_t(i),{parcel.low.x,y,0},{b.low.x-56,y,0},80,"pedestrian frontage",false});
  }
  if(interior){w.low={-preset.width*factor/2-160,-preset.depth*factor/2-160,-64};w.high={-w.low.x,-w.low.y,high+256};if(preset.ruinEvent=="collapsed-stair"){w.low.x-=192;w.high.x+=192;}}
  else{
    w.low={-totalW/2-street/2,-totalD/2-street/2,-64};w.high={totalW/2+street/2,totalD/2+street/2,high+384};
    if(preset.roadPattern=="radial"){w.low={-street,-street,-64};w.high={street,street,high+384};for(const auto& parcel:parcels){w.low.x=std::min(w.low.x,parcel.low.x-street);w.low.y=std::min(w.low.y,parcel.low.y-street);w.high.x=std::max(w.high.x,parcel.high.x+street);w.high.y=std::max(w.high.y,parcel.high.y+street);}}
    if(preset.roadPattern=="organic"||preset.roadPattern=="hybrid")for(auto& road:w.spatial.roads)if(road.id<2000&&(preset.roadPattern=="organic"||road.hierarchy=="local")){
      auto side=al::unit(al::cross(road.b-road.a,{0,0,1}));road.control=(road.a+road.b)*.5+side*rng.between(street*.12,street*.24);
    }

    for(const auto& pin:s.roadPins){bool found=false;for(auto& road:w.spatial.roads)if(road.id==pin.id){road=pin;found=true;}if(!found)w.spatial.roads.push_back(pin);}
  }
  for(int a=0;a<3;a++)if(std::max(std::abs(w.low[a]),std::abs(w.high[a]))>capability(s.profile).coordinateLimit)throw std::runtime_error("World exceeds the selected classic BSP profile bounds; reduce scale or choose FTE / Quake III");
  for(const auto& road:w.spatial.roads){
    if(interior)throw std::runtime_error("Road authoring requires a city or mixed site");
    double margin=road.width/2+12;const auto points=roadPoints(road);
    for(size_t segment=1;segment<points.size();segment++){auto a=points[segment-1],b=points[segment];int samples=std::max(1,int(std::ceil(al::length(b-a)/16)));
    for(int i=0;i<=samples;i++){auto p=a+(b-a)*(double(i)/samples);
      if(p.x-margin<w.low.x||p.x+margin>w.high.x||p.y-margin<w.low.y||p.y+margin>w.high.y)throw std::runtime_error("Designer road leaves the sealed site boundary");
      for(const auto& b:w.spatial.buildings)if(p.x>b.low.x-margin&&p.x<b.high.x+margin&&p.y>b.low.y-margin&&p.y<b.high.y+margin)throw std::runtime_error("Designer road intersects a building envelope; move the road or revise the site");
    }}
  }
  for(const auto& mask:s.influences)if(mask.kind=="avoid")for(const auto& b:w.spatial.buildings)if(overlap(b.low,b.high,mask.low,mask.high))throw std::runtime_error("Building overlaps a designer keep-out volume; move it or reduce the generation scope");
}
// Establish room programs before producing any brushes. Mission beats reserve
// real rooms and determine occupied-room/opening requirements; navigation subsequently
// proves those planned locations instead of assigning tags to an arbitrary path.
inline void floorplan(WorldPlan& w,const Settings& s,const Preset& preset){
  std::vector<Room*> rooms;
  for(auto& b:w.spatial.buildings){
    b.atrium=preset.layout=="atrium"||preset.layout=="ring"||preset.ruinEvent=="flooded-platform";
    b.parallel=preset.layout=="parallel";
    b.hall=std::max(s.player.radius*2+24,preset.hallHalfWidth);
    Random rng(regionSeed(s,b.id,"Floorplan"));
    const double end=b.high.x-480;
    const int minimum=int(std::ceil(double(w.mission.nodes.size())/std::max(1,int(w.spatial.buildings.size())*b.floors*2)));
    const int pairs=std::clamp(std::max(preset.rooms/2,minimum),2,std::max(2,int((end-b.low.x)/128)));
    b.bays={b.low.x};for(int i=1;i<pairs;i++)b.bays.push_back(b.low.x+(end-b.low.x)*(i+rng.between(-.18,.18))/pairs);b.bays.push_back(end);
    for(int floor=0;floor<b.floors;floor++)for(int side=0;side<2;side++)for(int i=0;i<pairs;i++){
      double x0=b.bays[i],x1=b.bays[i+1],cy=(b.low.y+b.high.y)/2;
      double y0=side?cy+b.hall:b.low.y,y1=side?b.high.y:cy-b.hall;
      if(y1-y0<96)throw std::runtime_error("Room program leaves insufficient occupied depth; increase scale or reduce gallery width");
      Room room;room.id=b.id*10000+floor*100+side*pairs+i;room.low={x0+8,y0+8,floor*256.};room.high={x1-8,y1-8,floor*256.+240};room.floor=floor;room.entry={(x0+x1)/2,side?y0:y1,floor*256.+1};
      if(preset.scenario=="mall"&&side&&!floor){room.high.y-=112;if(room.high.y-room.low.y<96)throw std::runtime_error("Mall service corridor needs a deeper room envelope; increase depth or scale");}
      const auto& functions=preset.roomFunctions;
      room.function=functions[(floor*pairs*2+side*pairs+i)%functions.size()];
      // Functional suspended ceilings vary independently from the structural
      // floor pitch. Public halls retain full height; service/occupied rooms
      // have legal, seeded headroom rather than identical cubic cells.
      if(room.function!="production"&&room.function!="retail"&&room.function!="food-court"&&room.function!="anchor-store")room.high.z=room.low.z+192+8*rng.integer(0,5);
      b.rooms.push_back(room);
      if(preset.scenario=="courtyard"&&!floor&&!side&&!i)b.rooms.back().function="garage";
    }
  }
  // Vectors are complete before taking pointers: no later room append may
  // invalidate a mission reservation.
  for(auto& b:w.spatial.buildings)for(auto& r:b.rooms)rooms.push_back(&r);
  if(rooms.size()+1<w.mission.nodes.size())throw std::runtime_error("Mission needs more distinct functional rooms than this site can provide");
  std::vector<Room*> available=rooms;
  std::stable_sort(available.begin(),available.end(),[](auto a,auto b){return std::tie(a->floor,a->id)<std::tie(b->floor,b->id);});
  for(auto& node:w.mission.nodes)if(node.pinned&&node.role!=Role::Start){
    auto found=std::find_if(available.begin(),available.end(),[&](auto r){return node.position.x>=r->low.x&&node.position.x<=r->high.x&&node.position.y>=r->low.y&&node.position.y<=r->high.y&&std::abs(node.position.z-r->low.z-1)<18;});
    if(found==available.end())throw std::runtime_error("Pinned mission beat must occupy its own functional room on the selected floor");
    auto* room=*found;room->mission=node.id;room->required=node.required;node.room=room->id;node.floor=room->floor;node.position={(room->low.x+room->high.x)/2,(room->low.y+room->high.y)/2,room->low.z+1};available.erase(found);
  }
  std::vector<MissionNode*> required;for(auto& node:w.mission.nodes)if(node.role!=Role::Start&&node.required&&!node.pinned)required.push_back(&node);
  for(size_t i=0;i<required.size();i++){
    auto* node=required[i];size_t index=std::min(available.size()-1,i*(available.size()-1)/std::max(size_t(1),required.size()-1));
    auto* room=available[index];room->mission=node->id;node->room=room->id;node->floor=room->floor;
    if(!node->pinned)node->position={(room->low.x+room->high.x)/2,(room->low.y+room->high.y)/2,room->low.z+1};
  }
  for(auto& node:w.mission.nodes){
    if(node.role==Role::Start){const auto& b=w.spatial.buildings.front();if(!node.pinned)node.position={b.low.x-56,(b.low.y+b.high.y)/2,1};continue;}
    if(node.required||node.pinned)continue;
    auto found=std::find_if(available.rbegin(),available.rend(),[](auto r){return !r->mission;});if(found==available.rend())throw std::runtime_error("Optional mission branch has no unreserved room");
    auto* room=*found;room->mission=node.id;node.room=room->id;node.floor=room->floor;room->required=false;
    if(!node.pinned)node.position={(room->low.x+room->high.x)/2,(room->low.y+room->high.y)/2,room->low.z+1};
  }
  if(preset.ruinEvent=="collapsed-stair")for(auto& node:w.mission.nodes)if(node.role==Role::Exit&&!node.pinned){
    for(auto room:rooms)if(room->mission==node.id)room->mission=0;const auto& b=w.spatial.buildings.front();node.room=0;node.floor=0;node.position={b.high.x+296,(b.low.y+b.high.y)/2,1};
  }
  if(preset.ruinEvent=="flooded-platform")for(auto& node:w.mission.nodes)if(node.role==Role::Exit&&!node.pinned){
    for(auto room:rooms)if(room->mission==node.id)room->mission=0;const auto& b=w.spatial.buildings.front();node.room=0;node.floor=0;node.position={b.high.x+56,(b.low.y+b.high.y)/2,1};
  }
}
// Wall bays with actual openings and lintels. Damage and windows remove solid
// geometry; there is no hidden collision sheet behind an apparent hole.
inline void wall(WorldPlan& w,uint64_t id,V a,V b,double bottom,double top,double thickness,int slot,
                 std::vector<std::array<double,4>> openings={}){
  V direction=b-a;double length=al::length(direction);if(length<1)return;direction=direction/length;
  if(std::abs(direction.z)>.001)throw std::runtime_error("Wall baseline must be horizontal");V side=al::cross({0,0,1},direction);
  auto span=[&](double lo,double hi,double z0,double z1){if(hi-lo<1||z1-z0<1)return;auto p=a+direction*lo,q=a+direction*hi;V low{std::min(p.x,q.x),std::min(p.y,q.y),z0},high{std::max(p.x,q.x),std::max(p.y,q.y),z1};
    if(std::abs(direction.x)<.001||std::abs(direction.y)<.001){low=low-V{std::abs(side.x)*thickness/2,std::abs(side.y)*thickness/2,0};high=high+V{std::abs(side.x)*thickness/2,std::abs(side.y)*thickness/2,0};box(w,id,"wall bay",low,high,slot);}
    else{
      // Start with a long local box and rotate its planes and UV axes.
      auto brush=al::box({lo,-thickness/2,z0},{hi,thickness/2,z1},material(w,slot));
      auto transform=[&](V p){return V{a.x+direction.x*p.x+side.x*p.y,a.y+direction.y*p.x+side.y*p.y,p.z};};
      for(auto& face:brush.faces){for(auto& p:face.points)p=transform(p);auto rotate=[&](V p){return V{direction.x*p.x+side.x*p.y,direction.y*p.x+side.y*p.y,p.z};};face.u=rotate(face.u);face.v=rotate(face.v);}addBrush(w,id,"angled wall bay",brush);
    }
  };
  std::sort(openings.begin(),openings.end());double cursor=0;
  for(auto opening:openings){double lo=std::clamp(opening[0],0.,length),hi=std::clamp(opening[1],lo,length);
    if(lo<cursor-.01)throw std::runtime_error("Overlapping architectural openings");span(cursor,lo,bottom,top);span(lo,hi,bottom,std::clamp(opening[2],bottom,top));span(lo,hi,std::clamp(opening[3],bottom,top),top);cursor=hi;
  }span(cursor,length,bottom,top);
}
inline void shell(WorldPlan& w){
  // A sealed sky boundary gives outdoor windows/ruins real voids without leaks.
  const auto lo=w.low,hi=w.high;box(w,0,"ground",{lo.x,lo.y,-32},{hi.x,hi.y,0},0);
  for(int side=0;side<4;side++){
    V a{lo.x-32,lo.y-32,-32},b{hi.x+32,hi.y+32,hi.z+32};
    if(side==0)b.x=lo.x;else if(side==1)a.x=hi.x;else if(side==2)b.y=lo.y;else a.y=hi.y;
    auto brush=al::box(a,b,skyMaterial(w));addBrush(w,0,"sealed sky boundary",brush);
  }addBrush(w,0,"sealed sky ceiling",al::box({lo.x-32,lo.y-32,hi.z},{hi.x+32,hi.y+32,hi.z+32},skyMaterial(w)));
  if(w.profile>=2){
    // Classic BSP sky is a special non-solid contents volume. Back it with
    // ordinary solid geometry so flood filling never depends on portal split
    // order when interior liquid/detail planes reach the sky leaves.
    for(int side=0;side<4;side++){
      V a{lo.x-64,lo.y-64,lo.z-64},b{hi.x+64,hi.y+64,hi.z+64};
      if(side==0)b.x=lo.x-32;else if(side==1)a.x=hi.x+32;else if(side==2)b.y=lo.y-32;else a.y=hi.y+32;
      box(w,0,"sealed sky solid backing",a,b,2);
    }
    box(w,0,"sealed sky bottom backing",{lo.x-64,lo.y-64,lo.z-64},{hi.x+64,hi.y+64,lo.z-32},2);
    box(w,0,"sealed sky top backing",{lo.x-64,lo.y-64,hi.z+32},{hi.x+64,hi.y+64,hi.z+64},2);
  }
}
inline double influence(const Settings& settings,V p,const std::string& kind,double fallback);
inline std::vector<std::array<double,2>> facadeGaps(const Building& b,int floor,int side){
  std::vector<std::array<double,2>> gaps;if(floor)return gaps;double cy=(b.low.y+b.high.y)/2;
  for(const auto& room:b.rooms)if(room.floor==floor&&room.function=="garage"&&int(room.low.y>cy)==side){double x=(room.low.x+room.high.x)/2;gaps.push_back({x-48,x+48});}std::sort(gaps.begin(),gaps.end());return gaps;
}
inline void facadeStrip(WorldPlan& w,const Building& b,const std::string& kind,int floor,int side,double bottom,double top,double depth,int slot,double overhang=0){
  double y=side?b.high.y:b.low.y,cursor=b.low.x-overhang;for(auto gap:facadeGaps(b,floor,side)){box(w,b.id,kind,{cursor,y-depth,bottom},{gap[0],y+depth,top},slot);cursor=gap[1];}box(w,b.id,kind,{cursor,y-depth,bottom},{b.high.x+overhang,y+depth,top},slot);
}
// Local safety lighting keeps the required exterior traversal legible even
// on the face shaded by the directional sun. These point entities do not
// modify geometry, collision, room assignment or navigation streams.
inline void exteriorTraversalLights(WorldPlan& w,const Building& b,int flights){
  double cy=(b.low.y+b.high.y)/2;for(int floor=0;floor<flights;floor++)for(int lamp=0;lamp<2;lamp++){
    V origin{b.low.x-264,cy+(lamp?64:-64),floor*256.+(lamp?320:128)};
    if(!inside(origin,w.low,w.high))throw std::runtime_error("Exterior stair light leaves the sealed site");
    al::PointEntity light;light.fields={{"classname","light"},{"origin",vectorText(origin)},{"light","320"},{"_color","0.72 0.84 1"},{"_wa_light_role","exterior-traversal"}};if(w.profile==4)light.fields["_light"]="184 214 255 320";w.architecture.entities.push_back({b.id,std::move(light)});
  }
}
inline void building(WorldPlan& w,const Settings& s,const Preset& preset,Building& b,const std::atomic<bool>* cancel){
  const auto low=b.low,high=b.high;const double cy=(low.y+high.y)/2,hall=b.hall,stairX=high.x-320,returnOffset=std::max(144.,hall+64);
  const bool collapsed=preset.ruinEvent=="collapsed-stair"&&influence(s,{stairX,cy,0},"damage",s.damage)>.2&&influence(s,{stairX,cy,0},"preserve",0)==0;
  Random rng(regionSeed(s,b.id,"Architecture"));
  const auto& splits=b.bays;const int roomPairs=int(splits.size())-1;const double roomEnd=splits.back();
  const double voidX0=low.x+144,voidX1=roomEnd-128,voidY0=cy-hall+64,voidY1=cy+hall-64;
  const bool openCourt=b.atrium&&voidX1-voidX0>160;
  const bool roofAccess=b.id==w.spatial.buildings.front().id&&(preset.scenario=="checkpoint"||preset.scenario=="courtyard");
  std::vector<size_t> halls;
  for(int floor=0;floor<b.floors;floor++){
    cancelled(cancel);double z=floor*256.;
    // Leave the stairwell open at upper levels. Wide side landings and the
    // central corridor are actual connected floor plates.
    if(!floor)box(w,b.id,"floor plate",{low.x,low.y,-16},{high.x,high.y,0},0);
    else{
      if(openCourt){
        box(w,b.id,"atrium entry landing",{low.x,low.y,z-16},{voidX0,high.y,z},0);
        box(w,b.id,"atrium exit landing",{voidX1,low.y,z-16},{stairX,high.y,z},0);
        box(w,b.id,"atrium gallery",{voidX0,low.y,z-16},{voidX1,voidY0,z},0);
        box(w,b.id,"atrium gallery",{voidX0,voidY1,z-16},{voidX1,high.y,z},0);
        for(double y:{voidY0,voidY1}){
          box(w,b.id,"gallery handrail",{voidX0,y-3,z+36},{voidX1,y+3,z+44},2);
          for(double x=voidX0;x<=voidX1;x+=96)box(w,b.id,"gallery baluster",{x-3,y-3,z},{x+3,y+3,z+40},2);
        }
      }else box(w,b.id,"floor plate",{low.x,low.y,z-16},{stairX,high.y,z},0);
      box(w,b.id,"stair side landing",{stairX,low.y,z-16},{high.x,cy-80,z},0);
      box(w,b.id,"stair side landing",{stairX,cy+80,z-16},{high.x,high.y,z},0);
      box(w,b.id,"stair end landing",{high.x-48,cy-80,z-16},{high.x,cy+80,z},0);
    }
    // Full-height outer frame, windows, a deliberate door to the street, and
    // corridor exits to side routes. Bay dimensions vary by archetype.
    double width=high.x-low.x,depth=high.y-low.y;
    for(int side=0;side<2;side++){
      double y=side?high.y:low.y;std::vector<std::array<double,4>> windows;
      int bays=std::max(3,int(width/(preset.archetype=="factory"?192:128)));
      for(int bay=0;bay<bays;bay++){double center=width*(bay+.5)/bays;windows.push_back({center-30,center+30,z+72,z+168});}
      for(auto gap:facadeGaps(b,floor,side)){double a=gap[0]-low.x,c=gap[1]-low.x;windows.erase(std::remove_if(windows.begin(),windows.end(),[&](const auto& window){return window[0]<c&&window[1]>a;}),windows.end());windows.push_back({a,c,z,z+112});}
      wall(w,b.id,{low.x,y,0},{high.x,y,0},z,z+240,16,1,windows);
      facadeStrip(w,b,"window sill course",floor,side,z+60,z+72,13,2,8);
      box(w,b.id,"floor facade cornice",{low.x-12,y-14,z+236},{high.x+12,y+14,z+252},2);
    }
    std::vector<std::array<double,4>> entrance{{depth/2-48,depth/2+48,z,z+112}};
    wall(w,b.id,{low.x,low.y,0},{low.x,high.y,0},z,z+240,16,1,entrance);
    wall(w,b.id,{high.x,low.y,0},{high.x,high.y,0},z,z+240,16,1,(!floor||(preset.scenario=="overpass"&&floor==1))?entrance:std::vector<std::array<double,4>>{});
    const auto hallLeft=navNode(w,{low.x+40,cy,z+1},b.id),hallRight=navNode(w,{stairX-40,cy,z+1},b.id);
    if(!floor||!openCourt)navEdge(w,hallLeft,hallRight);halls.push_back(hallRight);
    std::array<size_t,2> laneLeft{},laneRight{};
    for(int side=0;side<2;side++){
      double lane=cy+(side?1:-1)*(hall-32);
      laneLeft[side]=navNode(w,{low.x+40,lane,z+1},b.id);laneRight[side]=navNode(w,{stairX-40,lane,z+1},b.id);
      navEdge(w,hallLeft,laneLeft[side]);navEdge(w,laneLeft[side],laneRight[side]);navEdge(w,laneRight[side],hallRight);
      for(double x=low.x+96;x<stairX-32;x+=192){
        box(w,b.id,"circulation light fixture",{x-24,lane-20,z+208},{x+24,lane+20,z+240},2);
        al::PointEntity light;light.fields={{"classname","light"},{"origin",vectorText({x,lane,z+192})},{"light","320"},{"_color",side?"0.82 0.9 1":"1 0.9 0.72"}};if(s.profile==4)light.fields["_light"]="255 235 205 320";w.architecture.entities.push_back({b.id,light});
      }
    }
    if(b.parallel&&preset.ruinEvent!="flooded-platform"){
      double length=roomEnd-low.x-176;
      wall(w,b.id,{low.x+112,cy,0},{roomEnd-64,cy,0},z,z+240,12,3,{{length/2-48,length/2+48,z,z+112}});
      auto crossing=navNode(w,{(low.x+112+roomEnd-64)/2,cy,z+1},b.id);navEdge(w,crossing,laneLeft[0]);navEdge(w,crossing,laneLeft[1]);
    }
    if(!floor){auto outside=navNode(w,{low.x-56,cy,1},b.id);navEdge(w,outside,hallLeft);}
    for(int side=0;side<2;side++)for(int i=0;i<roomPairs;i++){
      double x0=splits[i],x1=splits[i+1],door=(x0+x1)/2;
      auto& room=b.rooms.at(size_t(floor*roomPairs*2+side*roomPairs+i));
      double y0=side?cy+hall:low.y,y1=side?room.high.y+8:cy-hall,inner=side?y0:y1;
      if(i)wall(w,b.id,{x0,y0,0},{x0,y1,0},z,z+240,12,1);
      if(room.high.z<z+240)box(w,b.id,"functional suspended ceiling",{room.low.x,room.low.y,room.high.z},{room.high.x,room.high.y,z+240},2);
      double halfDoor=std::min((x1-x0-32)/2,room.mission?56.:40.);
      if(halfDoor<s.player.radius+8)throw std::runtime_error("Functional room door does not clear the configured player hull");
      wall(w,b.id,{x0,inner,0},{x1,inner,0},z,z+240,12,1,{{door-x0-halfDoor,door-x0+halfDoor,z,z+112}});
      const double lane=cy+(side?1:-1)*(hall-32);
      const auto corridor=navNode(w,{door,lane,z+1},b.id),entry=navNode(w,room.entry,b.id),center=navNode(w,{door,(room.low.y+room.high.y)/2,z+1},b.id);
      navEdge(w,laneLeft[side],corridor);navEdge(w,corridor,laneRight[side]);navEdge(w,corridor,entry);navEdge(w,entry,center);
      // Lighting follows the room function, with warmer public spaces and
      // cooler service rooms. No game-specific enemy/entity vocabulary.
      al::PointEntity light;light.fields={{"classname","light"},{"origin",vectorText({door,(room.low.y+room.high.y)/2,std::min(z+192,room.high.z-32)})},{"light",preset.archetype=="station"?"280":"320"},{"_color",side?"0.72 0.83 1":"1 0.82 0.59"}};if(s.profile==4)light.fields["_light"]="255 220 180 320";w.architecture.entities.push_back({b.id,light});
    }
    // Rectangular room partitions end before the stairwell; a service lobby
    // and a pair of columns distinguish the larger terminal spatial beat.
    wall(w,b.id,{roomEnd,low.y,0},{roomEnd,cy-hall,0},z,z+240,12,1);
    wall(w,b.id,{roomEnd,cy+hall,0},{roomEnd,high.y,0},z,z+240,12,1);
    for(int side:{-1,1})box(w,b.id,"structural column",{stairX-18,cy+side*(hall+24)-12,z},{stairX+2,cy+side*(hall+24)+12,z+240},2);
    if(floor+1<b.floors||roofAccess){
      if(!floor&&collapsed){
        // The missing central flight is visible geometry, not a fake blocked
        // graph edge. Its upper floors are reached by the exterior fire stair.
        for(int step=0;step<16;step++)if(step<4||step>12){double x=stairX+step*16;box(w,b.id,"broken stair remnant",{x,cy-68,z},{x+16,cy-12,z+(step+1)*16},2);}
        continue;
      }
      // An enclosed switchback flight: every 16-unit riser is a real brush.
      // Walk nodes are sampled on the actual treads and tested with step-up
      // hull sweeps; a logical graph edge alone never proves stairs playable.
      const bool reverse=floor%2;const double lane=cy+(reverse?40:-40);
      size_t previous=hallRight;
      if(reverse){
        auto back=navNode(w,{stairX-40,cy-returnOffset,z+1},b.id),turn=navNode(w,{high.x-24,cy-returnOffset,z+1},b.id);
        navEdge(w,previous,back);navEdge(w,back,turn);previous=turn;
      }
      auto before=navNode(w,{reverse?stairX+276:stairX-20,lane,z+1},b.id);navEdge(w,previous,before);previous=before;
      for(int step=0;step<16;step++){
        const double x=stairX+(reverse?15-step:step)*16;
        box(w,b.id,"stair tread",{x,lane-28,z},{x+16,lane+28,z+(step+1)*16},2);
        auto node=navNode(w,{x+(reverse?24:-8),lane,z+(step+1)*16+1},b.id);navEdge(w,previous,node);previous=node;
      }
      auto end=navNode(w,{reverse?stairX-20:stairX+276,lane,z+257},b.id);navEdge(w,previous,end);
      if(!reverse){
        const auto turn=navNode(w,{high.x-24,cy-returnOffset,z+257},b.id),back=navNode(w,{stairX-40,cy-returnOffset,z+257},b.id),center=navNode(w,{stairX-40,cy,z+257},b.id);
        navEdge(w,end,turn);navEdge(w,turn,back);navEdge(w,back,center);
      }else{
        auto center=navNode(w,{stairX-40,cy,z+257},b.id);navEdge(w,end,center);
      }

    }
  }
  // A roof with a retained frame. Later damage may open it to the sealed sky.
  if(roofAccess){
    box(w,b.id,"roof access slab",{low.x-16,low.y-16,high.z-16},{stairX,high.y+16,high.z},2);
    box(w,b.id,"roof access side landing",{stairX,low.y-16,high.z-16},{high.x+16,cy-80,high.z},2);
    box(w,b.id,"roof access side landing",{stairX,cy+80,high.z-16},{high.x+16,high.y+16,high.z},2);
    box(w,b.id,"roof access end landing",{high.x-48,cy-80,high.z-16},{high.x+16,cy+80,high.z},2);
    for(int side:{-1,1})box(w,b.id,"roof stairwell guard",{stairX,cy+side*80-3,high.z+36},{high.x-80,cy+side*80+3,high.z+44},3);
    size_t previous=SIZE_MAX;int count=std::max(1,int(std::ceil((stairX-40-low.x-64)/192)));for(int i=0;i<=count;i++){auto n=navNode(w,{low.x+64+(stairX-40-low.x-64)*double(i)/count,cy,high.z+1},b.id);if(previous!=SIZE_MAX)navEdge(w,previous,n);previous=n;}
  }else if(openCourt){
    box(w,b.id,"atrium roof frame",{low.x-16,low.y-16,high.z-16},{voidX0,high.y+16,high.z},2);
    box(w,b.id,"atrium roof frame",{voidX1,low.y-16,high.z-16},{high.x+16,high.y+16,high.z},2);
    box(w,b.id,"atrium roof frame",{voidX0,low.y-16,high.z-16},{voidX1,voidY0,high.z},2);
    box(w,b.id,"atrium roof frame",{voidX0,voidY1,high.z-16},{voidX1,high.y+16,high.z},2);
  }else box(w,b.id,"roof plate",{low.x-16,low.y-16,high.z-16},{high.x+16,high.y+16,high.z},2);
  for(int side:{-1,1})box(w,b.id,"roof parapet",{low.x-16,side<0?low.y-16:high.y,high.z},{high.x+16,side<0?low.y:high.y+16,high.z+48},1);
  // Vertical bays, deep entrance canopy and corner pilasters give the mass an
  // architectural hierarchy rather than identical solid cells.
  for(double x:{low.x,high.x})for(double y:{low.y,high.y})box(w,b.id,"corner pilaster",{x-18,y-18,0},{x+18,y+18,high.z+20},2);
  if(preset.ruinEvent!="collapsed-stair")box(w,b.id,"entrance canopy",{low.x-96,cy-80,120},{low.x+8,cy+80,136},2);
  else{
    // Two lanes alternate between floors, preserving headroom above the
    // lower flight. A separate wall-side gallery joins every existing door.
    exteriorTraversalLights(w,b,b.floors-1);
    const double halfWidth=std::max(28.,s.player.radius+8);
    for(int floor=1;floor<b.floors;floor++){
      double z=floor*256.;
      box(w,b.id,"fire-stair wall gallery",{low.x-80,cy-176,z-16},{low.x+8,cy+176,z},2);
      for(int sign:{-1,1})box(w,b.id,"fire-stair end landing",{low.x-240,cy+sign*152-24,z-16},{low.x+8,cy+sign*152+24,z},2);
      auto center=navNode(w,{low.x-40,cy,z+1},b.id),inside=navNode(w,{low.x+40,cy,z+1},b.id);navEdge(w,center,inside);
    }
    for(int floor=0;floor+1<b.floors;floor++){
      const bool reverse=floor%2;double z=floor*256.,x=low.x-(reverse?192:112),startY=cy+(reverse?148:-148),endY=cy+(reverse?-148:148);
      auto door=navNode(w,{low.x-40,cy,z+1},b.id),turn=navNode(w,{low.x-40,startY,z+1},b.id),previous=navNode(w,{x,startY,z+1},b.id);
      navEdge(w,door,turn);navEdge(w,turn,previous);
      for(int step=0;step<16;step++){
        double y=cy-128+(reverse?15-step:step)*16;
        box(w,b.id,"exterior fire stair",{x-halfWidth,y,z},{x+halfWidth,y+16,z+(step+1)*16},2);
        auto at=navNode(w,{x,y+(reverse?24:-8),z+(step+1)*16+1},b.id);navEdge(w,previous,at);previous=at;
      }
      auto end=navNode(w,{x,endY,z+257},b.id),back=navNode(w,{low.x-40,endY,z+257},b.id),upper=navNode(w,{low.x-40,cy,z+257},b.id);
      navEdge(w,previous,end);navEdge(w,end,back);navEdge(w,back,upper);
    }
  }
}
inline al::Brush prism(const std::vector<V>& polygon,double bottom,double top,const std::string& texture){
  if(polygon.size()<3||polygon.size()>24||top-bottom<1)throw std::runtime_error("Invalid convex extrusion");
  V center{};for(auto p:polygon)center=center+p;center=center/double(polygon.size());center.z=(bottom+top)/2;
  al::Brush brush;
  auto face=[&](V a,V b,V c){al::Face f;f.points={a,b,c};f.material=texture;if(al::dot(f.normal(),center)>f.distance())std::swap(f.points[1],f.points[2]);
    auto n=f.normal();if(std::abs(n.z)>.7){f.u={1,0,0};f.v={0,-1,0};}else{f.u=al::unit(al::cross({0,0,1},n));f.v={0,0,-1};}brush.faces.push_back(f);};
  for(size_t i=0;i<polygon.size();i++){auto a=polygon[i],b=polygon[(i+1)%polygon.size()];a.z=b.z=bottom;face(a,b,{b.x,b.y,top});}
  auto a=polygon[0],b=polygon[1],c=polygon[2];face({a.x,a.y,bottom},{b.x,b.y,bottom},{c.x,c.y,bottom});face({a.x,a.y,top},{b.x,b.y,top},{c.x,c.y,top});al::validateBrush(brush);return brush;
}
inline al::Brush bevel(V lo,V hi,const std::string& texture,double amount){
  auto brush=al::box(lo,hi,texture);V n=al::unit({1,1,.4});al::Brush clipped;double cut=al::dot(n,hi)-amount;
  // Avoid tiny edges at a box corner: the engine winding clipper treats
  // distances below 0.1 as on-plane, while manifold validation merges at 1/32.
  // Preserve a one-unit margin before MAP quantization instead of weakening it.
  for(int attempt=0;attempt<16;attempt++){
    bool nearCorner=false;for(int corner=0;corner<8;corner++){V p{corner&1?hi.x:lo.x,corner&2?hi.y:lo.y,corner&4?hi.z:lo.z};if(std::abs(al::dot(n,p)-cut)<1)nearCorner=true;}
    if(!nearCorner){if(al::clipBrush(brush,n,cut,clipped))return clipped;return brush;}cut+=1.25;
  }
  return brush;
}
inline void receptionAnnex(WorldPlan& w,const Settings& s,const Building& b){
  const double x=b.high.x,cy=(b.low.y+b.high.y)/2,radius=256;
  std::vector<V> polygon;
  for(int i=0;i<=16;i++){double angle=-1.5707963267948966+i*3.141592653589793/16;polygon.push_back({x+radius*std::cos(angle),cy+radius*std::sin(angle),0});}
  addBrush(w,b.id,"semicircular reception floor",prism(polygon,0,16,material(w,0)));
  addBrush(w,b.id,"semicircular reception roof",prism(polygon,240,256,material(w,2)));
  for(int i=0;i<16;i++){
    auto a=polygon[i],c=polygon[i+1];double length=al::length(c-a);bool exit=i==7||i==8;
    wall(w,b.id,a,c,16,240,16,1,{{0,length,exit?16.:80.,exit?128.:176.}});
    if(!exit)wall(w,b.id,a,c,72,80,24,2);
  }
  for(size_t i=0;i<polygon.size();i++)if(i!=8){auto p=polygon[i];box(w,b.id,"reception arc pier",{p.x-8,p.y-8,16},{p.x+8,p.y+8,240},2);}
  // The arc is structural, editable convex MAP geometry. Its public hall
  // opens through the existing rear door and into an actual exit courtyard.
  double turnY=cy+std::max(144.,b.hall+64);
  auto hall=navNode(w,{x-360,cy,1},b.id),turn=navNode(w,{x-360,turnY,1},b.id),back=navNode(w,{x-24,turnY,1},b.id),inner=navNode(w,{x-24,cy,1},b.id),entry=navNode(w,{x+24,cy,17},b.id),center=navNode(w,{x+128,cy,17},b.id),door=navNode(w,{x+216,cy,17},b.id),outside=navNode(w,{x+296,cy,1},b.id);
  for(auto edge:std::vector<std::array<size_t,2>>{{hall,turn},{turn,back},{back,inner},{inner,entry},{entry,center},{center,door},{door,outside}})navEdge(w,edge[0],edge[1]);
  al::PointEntity light;light.fields={{"classname","light"},{"origin",vectorText({x+128,cy,192})},{"light","320"},{"_color","1 0.85 0.64"}};w.architecture.entities.push_back({b.id,light});
}
inline void floodedPlatform(WorldPlan& w,const Settings& s,const Building& b){
  const double cy=(b.low.y+b.high.y)/2,x0=b.low.x+144,x1=b.bays.back()-128;
  if(x1-x0<160)throw std::runtime_error("Flooded station requires a longer platform envelope");
  auto water=al::box({x0,cy-56,0},{x1,cy+56,24},material(w,6));if(w.profile<2)for(auto& face:water.faces)face.attributes="32 0 0";addBrush(w,b.id,"floodwater",std::move(water));
  // The critical route uses either dry platform. Physical curb walls keep
  // navigation out of the flood rather than assuming swimming capabilities.
  for(int sign:{-1,1})box(w,b.id,"platform flood barrier",{x0-8,cy+sign*64-8,0},{x1+8,cy+sign*64+8,32},5);
  for(double x:{x0,x1})box(w,b.id,"platform flood end",{x-8,cy-56,0},{x+8,cy+56,32},2);
  double x=b.high.x,turnY=cy+std::max(144.,b.hall+64);
  auto hall=navNode(w,{x-360,cy,1},b.id),turn=navNode(w,{x-360,turnY,1},b.id),back=navNode(w,{x-24,turnY,1},b.id),inner=navNode(w,{x-24,cy,1},b.id),outside=navNode(w,{x+56,cy,1},b.id);
  for(auto edge:std::vector<std::array<size_t,2>>{{hall,turn},{turn,back},{back,inner},{inner,outside}})navEdge(w,edge[0],edge[1]);
}
inline double distanceToSegment(V p,V a,V b){V d=b-a;double den=al::dot(d,d);return al::length(p-(a+d*(den>0?std::clamp(al::dot(p-a,d)/den,0.,1.):0)));}
inline bool protectedRoute(const WorldPlan& w,V p,double radius){
  for(const auto& edge:w.navigation.edges)if(distanceToSegment(p,w.navigation.nodes[edge[0]].feet,w.navigation.nodes[edge[1]].feet)<radius)return true;return false;
}
inline double influence(const Settings& settings,V p,const std::string& kind,double fallback){
  double result=fallback;for(const auto& mask:settings.influences)if(mask.kind==kind&&inside(p,mask.low,mask.high))result=mask.strength;return result;
}
#include "theme_kits.h"
#include "scenario_kits.h"
#include "module_kits.h"
#include "gates.h"
inline void damage(WorldPlan& w,const Settings& input,const std::atomic<bool>* cancel,const Registry& registry){
  for(const auto& building:w.spatial.buildings){
    const auto s=regionInput(input,building.id,3,registry);
    cancelled(cancel);Random rng(regionSeed(s,building.id,"Damage"));
    const V origin{rng.between(building.low.x+96,building.high.x-96),building.high.y,building.high.z*.65};
    double amount=influence(s,origin,"damage",s.damage);
    if(influence(s,origin,"preserve",0)>0)amount=0;
    DamageZone field{building.id,origin,180+amount*280,amount,"blast / exposure"};w.damage.zones.push_back(field);
    if(amount<=.01)continue;
    std::vector<ArchitectureElement> changed;changed.reserve(w.architecture.elements.size());
    for(auto& e:w.architecture.elements){
      if(e.region!=building.id||(e.kind!="wall bay"&&e.kind!="roof plate")){changed.push_back(std::move(e));continue;}
      auto poly=al::polyhedron(e.brush);const auto center=(poly.min+poly.max)*.5;
      const double falloff=std::max(0.,1-al::length(center-origin)/field.radius);
      if(falloff*amount<.16){changed.push_back(std::move(e));continue;}
      // A clipped fracture plane removes an exposed upper corner. Floor,
      // tread, column and lintel support remain explicitly protected.
      V n=al::unit({rng.between(-.5,.5),.35,1});double cut=al::dot(n,center)+24*(1-falloff);
      al::Brush retained;
      if(al::clipBrush(e.brush,n,cut,retained))changed.push_back({e.region,"fractured masonry",std::move(retained)});
    }
    w.architecture.elements=std::move(changed);
    for(int i=0;i<int(5+amount*16);i++){
      V p{origin.x+rng.between(-field.radius*.55,field.radius*.55),building.high.y+rng.between(-80,100),0};
      if(protectedRoute(w,p,80)||p.x<w.low.x+64||p.x>w.high.x-64||p.y>w.high.y-64||influence(s,p,"preserve",0)>0)continue;
      double r=rng.between(10,34),h=rng.between(8,28);
      addBrush(w,building.id,"fallen masonry",bevel({p.x-r,p.y-r,0},{p.x+r,p.y+r,h},material(w,2),r*.4));
      w.assets.push_back({building.id,"gravity rubble", "original fractured brush",p});
    }
  }
}
inline void dressing(WorldPlan& w,const Settings& input,const Preset&,const std::atomic<bool>* cancel,const Registry& registry){
  const auto& theme=registry.theme(w.theme);
  for(const auto& [id,seed]:input.roomSeeds){bool found=false;for(const auto& b:w.spatial.buildings)for(const auto& room:b.rooms)if(room.id==id)found=true;if(!found)throw std::runtime_error("Room seed refers to a room outside the current floor plan");}
  for(const auto& [id,module]:input.roomModules){bool found=false;for(const auto& b:w.spatial.buildings)for(const auto& room:b.rooms)if(room.id==id)found=true;if(!found)throw std::runtime_error("Authored module refers to a room outside the current floor plan");}
  for(const auto& building:w.spatial.buildings){
    cancelled(cancel);const auto settings=regionInput(input,building.id,4,registry);std::vector<V> occupied;
    for(const auto& room:building.rooms){
      if(authoredRoom(w,settings,building,room))continue;
      auto override=settings.roomSeeds.find(room.id);Random rng(subSeed(override==settings.roomSeeds.end()?regionSeed(settings,building.id,"Dressing"):override->second,"Room",room.id));
      for(auto p:std::array<V,4>{{{room.low.x+48,room.low.y+48,room.low.z},{room.high.x-48,room.low.y+48,room.low.z},{room.low.x+48,room.high.y-48,room.low.z},{room.high.x-48,room.high.y-48,room.low.z}}}){
        const double density=influence(settings,p,"dressing",settings.dressing);if(rng.unit()>=density||influence(settings,p,"preserve",0)>0||protectedRoute(w,p,90))continue;
        if(p.x<room.low.x+32||p.x>room.high.x-32||p.y<room.low.y+32||p.y>room.high.y-32)continue;
        if(std::any_of(occupied.begin(),occupied.end(),[&](V other){return al::length(p-other)<90;}))continue;
        occupied.push_back(p);semanticProp(w,building,room,theme,p);
      }
    }
  }
}
inline void streets(WorldPlan& w){
  for(const auto& road:w.spatial.roads){
    const auto points=roadPoints(road);size_t previous=SIZE_MAX;
    for(size_t segment=1;segment<points.size();segment++){
    auto a=points[segment-1],b=points[segment];const V delta=b-a;const double length=al::length(delta);if(length<1)throw std::runtime_error("Road curve contains a degenerate segment");
    wall(w,road.id,a,b,-4,0,road.width,0);
    V side=al::unit(al::cross(delta,{0,0,1}));
    // Curbs remain below step height, with pedestrian access at every frontage.
    for(int sign:{-1,1}){auto offset=side*(sign*(road.width*.5+12));wall(w,road.id,a+offset,b+offset,0,8,24,2);}
    int count=std::max(1,int(std::ceil(length/128)));
    for(int i=0;i<=count;i++){size_t node=navNode(w,a+delta*(double(i)/count)+V{0,0,1},road.id);if(previous!=SIZE_MAX)navEdge(w,previous,node);previous=node;}
    }
  }
}
#include "quality.h"
// Hard planning constraints are retained with Site, so frozen/pinned recipes
// cannot quietly adopt a different set of route or keep-out requirements.
inline void reservePlanning(WorldPlan& w,const Settings& s){
  for(const auto& volume:s.influences)if(volume.kind=="must-pass"||volume.kind=="vertical-link"||volume.kind=="no-generation"){
    w.planningVolumes.push_back(volume);
    if(volume.kind=="must-pass"){
      V anchor{(volume.low.x+volume.high.x)/2,(volume.low.y+volume.high.y)/2,volume.low.z+1};uint64_t region=0;
      for(const auto& building:w.spatial.buildings)for(const auto& room:building.rooms){V center{(room.low.x+room.high.x)/2,(room.low.y+room.high.y)/2,room.low.z+1};if(inside(center,volume.low,volume.high)){anchor=center;region=building.id;}}
      auto node=navNode(w,anchor,region);w.passAnchors.emplace(volume.id,node);
    }
  }
}
inline bool intersectsVolume(const al::Brush& brush,const Influence& volume){
  auto bounds=al::polyhedron(brush);if(!overlap(bounds.min,bounds.max,volume.low,volume.high))return false;
  auto clipped=brush;for(int axis=0;axis<3;axis++)for(int side:{-1,1}){V normal{};normal[axis]=side;al::Brush next;if(!al::clipBrush(clipped,normal,side>0?volume.high[axis]:-volume.low[axis],next))return false;clipped=std::move(next);}return al::polyhedron(clipped).volume>.01;
}
inline void navigation(WorldPlan& w,const Settings& settings,const std::atomic<bool>* cancel){
  auto& nav=w.navigation;Collision collision(w);std::vector<std::array<size_t,2>> verified;
  for(auto& node:nav.nodes)if(collision.blocked(node.feet,node.feet,settings.player)){
    auto base=node.feet;for(int rise=1;rise<=int(settings.player.step);rise++){node.feet=base+V{0,0,double(rise)};if(!collision.blocked(node.feet,node.feet,settings.player)&&collision.supported(node.feet,settings.player))break;}
  }
  std::set<std::array<size_t,2>> unique;
  auto edge=[&](size_t a,size_t b){if(a>b)std::swap(a,b);if(a==b||!unique.insert({a,b}).second)return;
    auto p=nav.nodes[a].feet,q=nav.nodes[b].feet;
    if(!collision.walk(p,q,settings.player))return;
    // Test the supporting surface throughout long links. A clear hull sweep
    // across an open stairwell is not a walkable connection.
    int samples=std::max(1,int(al::length(q-p)/16));for(int i=0;i<=samples;i++)if(!collision.supported(p+(q-p)*(double(i)/samples),settings.player))return;
    verified.push_back({a,b});
  };
  auto proposed=nav.edges;for(auto e:proposed)edge(e[0],e[1]);
  for(size_t a=0;a<nav.nodes.size();a++){
    cancelled(cancel);for(size_t b=a+1;b<nav.nodes.size();b++){
      auto p=nav.nodes[a].feet,q=nav.nodes[b].feet;
      if(std::abs(p.z-q.z)<=settings.player.step&&al::length(p-q)<384)edge(a,b);
    }
  }
  nav.edges=std::move(verified);if(nav.nodes.empty())throw std::runtime_error("No physical navigation nodes");
  std::vector<std::vector<size_t>> adjacency(nav.nodes.size());for(auto e:nav.edges){adjacency[e[0]].push_back(e[1]);adjacency[e[1]].push_back(e[0]);}
  auto nearest=[&](V point){size_t result=SIZE_MAX;double distance=48;for(size_t i=0;i<nav.nodes.size();i++){double d=al::length(point-nav.nodes[i].feet);if(d<distance&&!collision.blocked(nav.nodes[i].feet,nav.nodes[i].feet,settings.player)){distance=d;result=i;}}return result;};
  std::map<uint64_t,size_t> beats;size_t start=SIZE_MAX;
  for(auto& beat:w.mission.nodes){
    size_t at=nearest(beat.position);if(at==SIZE_MAX)throw std::runtime_error("Planned mission room has no collision-free player anchor: "+beat.name);
    beats[beat.id]=at;beat.position=nav.nodes[at].feet;if(beat.role==Role::Start)start=at;
  }
  if(start==SIZE_MAX)throw std::runtime_error("No physically realized mission start");
  std::vector<int> visited(nav.nodes.size(),-1);std::deque<size_t> queue{start};visited[start]=int(start);
  while(!queue.empty()){size_t n=queue.front();queue.pop_front();for(auto next:adjacency[n])if(visited[next]<0){visited[next]=int(n);queue.push_back(next);}}
  nav.reachable=std::count_if(visited.begin(),visited.end(),[](int n){return n>=0;});
  for(size_t i=0;i<nav.nodes.size();i++)if(nav.nodes[i].required&&visited[i]<0)throw std::runtime_error("Physical navigation rejected disconnected room, street or stair at "+vectorText(nav.nodes[i].feet));
  for(const auto& beat:w.mission.nodes)if(visited[beats.at(beat.id)]<0)throw std::runtime_error("Planned mission beat is unreachable: "+beat.name);
  auto path=[&](size_t from,size_t to){
    std::vector<double> distance(nav.nodes.size(),std::numeric_limits<double>::infinity());std::vector<size_t> parent(nav.nodes.size(),SIZE_MAX);
    using Item=std::pair<double,size_t>;std::priority_queue<Item,std::vector<Item>,std::greater<Item>> pending;distance[from]=0;pending.push({0,from});
    while(!pending.empty()){
      auto [length,at]=pending.top();pending.pop();if(length>distance[at]+.001)continue;if(at==to)break;
      for(size_t next:adjacency[at]){double d=length+al::length(nav.nodes[at].feet-nav.nodes[next].feet);if(d<distance[next]-.001){distance[next]=d;parent[next]=at;pending.push({d,next});}}
    }
    if(!std::isfinite(distance[to]))throw std::runtime_error("Mission edge has no walkable spatial realization");
    std::vector<size_t> result;for(size_t at=to;;at=parent[at]){result.push_back(at);if(at==from)break;if(parent[at]==SIZE_MAX)throw std::runtime_error("Broken mission path predecessor");}std::reverse(result.begin(),result.end());return result;
  };
  for(const auto& volume:w.planningVolumes)if(volume.kind=="no-generation"){
    for(const auto& element:w.architecture.elements)if(element.region&&intersectsVolume(element.brush,volume))throw std::runtime_error("No-generation volume "+std::to_string(volume.id)+" intersects generated "+element.kind+"; move the volume or revise the site");
    for(const auto& entity:w.architecture.entities){auto origin=entity.entity.fields.find("origin");if(origin!=entity.entity.fields.end()){V point;std::istringstream in(origin->second);if(in>>point.x>>point.y>>point.z&&inside(point,volume.low,volume.high))throw std::runtime_error("No-generation volume intersects a generated point entity");}}
    for(const auto& entity:w.architecture.brushEntities)for(const auto& brush:entity.brushes)if(intersectsVolume(brush,volume))throw std::runtime_error("No-generation volume intersects a runtime brush entity");
  }
  std::vector<size_t> spatialStops;for(const auto& volume:w.planningVolumes){
    if(volume.kind=="must-pass"){auto at=w.passAnchors.at(volume.id);if(!inside(nav.nodes[at].feet,volume.low,volume.high)||visited[at]<0)throw std::runtime_error("Must-pass volume has no reachable supported player anchor");spatialStops.push_back(at);}
    if(volume.kind=="vertical-link"){
      if(volume.high.z-volume.low.z<256)throw std::runtime_error("Required vertical-link volume must span at least one 256-unit floor");
      std::set<size_t> examined;bool connected=false;
      for(size_t first=0;first<nav.nodes.size()&&!connected;first++){
        if(visited[first]<0||examined.count(first)||!inside(nav.nodes[first].feet,volume.low,volume.high+V{0,0,2}))continue;
        size_t bottom=first,top=first;std::deque<size_t> pending{first};examined.insert(first);
        while(!pending.empty()){auto at=pending.front();pending.pop_front();if(nav.nodes[at].feet.z<nav.nodes[bottom].feet.z)bottom=at;if(nav.nodes[at].feet.z>nav.nodes[top].feet.z)top=at;for(auto next:adjacency[at])if(!examined.count(next)&&inside(nav.nodes[next].feet,volume.low,volume.high+V{0,0,2})){examined.insert(next);pending.push_back(next);}}
        if(nav.nodes[bottom].feet.z<=volume.low.z+settings.player.step+2&&nav.nodes[top].feet.z>=volume.high.z-settings.player.step-2){spatialStops.push_back(bottom);spatialStops.push_back(top);connected=true;}
      }
      if(!connected)throw std::runtime_error("Required vertical-link volume contains no continuous collision-verified stair connection");
    }
  }
  nav.mainRoute={start};
  for(auto beat:missionTraversal(w.mission)){
    auto leg=path(nav.mainRoute.back(),beats.at(beat));for(size_t i=1;i<leg.size();i++){w.diagnostics.routeLength+=al::length(nav.nodes[leg[i]].feet-nav.nodes[leg[i-1]].feet);nav.mainRoute.push_back(leg[i]);}
  }
  if(!spatialStops.empty()){
    auto exit=nav.mainRoute.back();spatialStops.push_back(exit);for(auto stop:spatialStops){auto leg=path(nav.mainRoute.back(),stop);for(size_t i=1;i<leg.size();i++){w.diagnostics.routeLength+=al::length(nav.nodes[leg[i]].feet-nav.nodes[leg[i-1]].feet);nav.mainRoute.push_back(leg[i]);}}
    w.diagnostics.score["planningStops"]=double(spatialStops.size()-1);
  }
  // Count a circulation alternative only if it stays spatially separated
  // from the complete primary polyline. Dense links along one corridor do
  // not count as loops; the alternative must walk around a different side.
  int alternatives=0;
  for(const auto& b:w.spatial.buildings)for(int floor=0;floor<b.floors;floor++){
    double cy=(b.low.y+b.high.y)/2,z=floor*256.+1;
    size_t left=nearest({b.low.x+40,cy,z}),right=nearest({b.high.x-360,cy,z});if(left==SIZE_MAX||right==SIZE_MAX)continue;
    auto primary=path(left,right);std::set<size_t> reached{left};std::deque<size_t> pending{left};std::map<size_t,size_t> parent;
    auto allowed=[&](V point){
      if(std::abs(point.z-z)>1||point.x<b.low.x||point.x>b.high.x||point.y<b.low.y||point.y>b.high.y)return false;
      if(al::length(point-nav.nodes[left].feet)<128||al::length(point-nav.nodes[right].feet)<128)return true;
      for(size_t i=1;i<primary.size();i++)if(distanceToSegment(point,nav.nodes[primary[i-1]].feet,nav.nodes[primary[i]].feet)<64)return false;return true;
    };
    while(!pending.empty()&&!reached.count(right)){
      size_t at=pending.front();pending.pop_front();for(size_t next:adjacency[at]){
        if(reached.count(next))continue;V a=nav.nodes[at].feet,delta=nav.nodes[next].feet-a;bool valid=true;int samples=std::max(1,int(al::length(delta)/24));
        for(int i=0;i<=samples;i++)if(!allowed(a+delta*(double(i)/samples))){valid=false;break;}
        if(valid){reached.insert(next);parent[next]=at;pending.push_back(next);}
      }
    }
    if(reached.count(right)){
      bool separated=false;
      for(size_t at=right;at!=left&&!separated;at=parent.at(at)){
        auto a=nav.nodes[at].feet,b=nav.nodes[parent.at(at)].feet;int samples=std::max(1,int(al::length(b-a)/48));
        for(int i=0;i<=samples&&!separated;i++){
          auto point=a+(b-a)*(double(i)/samples);if(al::length(point-nav.nodes[left].feet)<128||al::length(point-nav.nodes[right].feet)<128)continue;
          V closest{};double distance=1e30;
          for(size_t p=1;p<primary.size();p++){
            V from=nav.nodes[primary[p-1]].feet,delta=nav.nodes[primary[p]].feet-from;double length=al::dot(delta,delta);
            auto q=from+delta*(length?std::clamp(al::dot(point-from,delta)/length,0.,1.):0.);double d=al::length(q-point);if(d<distance){distance=d;closest=q;}
          }
          if(distance<64)continue;
          separated=!collision.walk(point,closest,settings.player);
          for(int p=1;p<8&&!separated;p++)if(!collision.supported(point+(closest-point)*(p/8.),settings.player))separated=true;
        }
      }
      if(separated)alternatives++;
    }
  }
  auto& scores=w.diagnostics.score;
  scores["route"]=std::min(1.,w.diagnostics.routeLength/settings.targetRoute);
  std::set<int> areas;for(const auto& b:w.spatial.buildings)for(const auto& r:b.rooms)areas.insert(int((r.high.x-r.low.x)*(r.high.y-r.low.y)/8192));
  scores["variety"]=std::min(1.,areas.size()/6.);
  scores["circulation"]=std::min(1.,double(alternatives));scores["separatedRoutes"]=alternatives;
  w.diagnostics.warnings.push_back("Circulation counts same-floor alternatives separated by at least 64 units and a physical obstruction or floor opening outside endpoint landings.");
  spatialQuality(w,settings,collision,adjacency);
  double sum=0,weight=0;for(auto [name,value]:settings.weights){auto it=scores.find(name);if(it!=scores.end()){sum+=it->second*value;weight+=value;}}scores["weighted"]=weight?sum/weight:0;
}
inline void exportMap(WorldPlan& w,const Settings& settings){
  if(w.navigation.mainRoute.empty())throw std::runtime_error("Cannot export without validated player route");
  std::ostringstream out;out.imbue(std::locale::classic());out<<"{\n\"classname\" \"worldspawn\"\n\"_wa_schema\" \"1\"\n\"_wa_seed\" \""<<w.seed<<"\"\n\"_wa_preset\" \""<<w.preset<<"\"\n";
  if(settings.profile>=2)out<<"\"_sunlight\" \"110\"\n\"_sunlight_color\" \"1 0.9 0.75\"\n\"_sun_mangle\" \"120 -50 0\"\n";
  for(const auto& element:w.architecture.elements)out<<al::writeBrush(element.brush);out<<"}\n";
  for(const auto& entity:w.architecture.entities)out<<al::writeEntity(entity.entity);
  for(const auto& entity:w.architecture.brushEntities)out<<writeBrushEntity(entity);
  auto spawn=w.navigation.nodes[w.navigation.mainRoute.front()].feet+V{0,0,24};
  out<<al::writeEntity({{{"classname",capability(settings.profile).spawn},{"origin",vectorText(spawn)},{"angle","0"}}});
  // Markers are editor metadata; they do not invent unsupported game entities.
  for(const auto& beat:w.mission.nodes)out<<al::writeEntity({{{"classname","info_null"},{"origin",vectorText(beat.position+V{0,0,32})},{"_wa_role",roleName(beat.role)},{"_wa_id",std::to_string(beat.id)}}});
  w.map=out.str();if(w.map.size()>16*1024*1024)throw std::runtime_error("Generated MAP exceeds the 16 MiB import budget");
  auto roundtrip=al::parseMap(w.map);if(roundtrip.brushes.size()!=brushCount(w))throw std::runtime_error("Quantized MAP brush roundtrip failed");
  w.diagnostics.mapBytes=w.map.size();w.diagnostics.brushes=brushCount(w);
}
// Editor groups carry stable ownership independently of engine entity/model
// indices. Both supported compilers merge func_group brushes into worldspawn.
inline uint64_t missionRegion(const WorldPlan& w,const MissionNode& beat){
  if(beat.room)return beat.room/10000;
  // Roofs and galleries are building-owned even when they are not rooms.
  // Keep street markers in the shared region so local replacement cannot
  // remove another building's approach or the global spawn.
  for(const auto& b:w.spatial.buildings)if(inside(beat.position,b.low-V{0,0,32},b.high+V{0,0,80}))return b.id;
  return 0;
}
inline std::string groupedMap(const WorldPlan& w,const std::string& project,std::optional<uint64_t> onlyRegion={}){
  safeToken(project);if(project.size()>64)throw std::runtime_error("Project ownership identifier exceeds 64 bytes");if(w.architectureKey.empty())throw std::runtime_error("Missing architecture provenance");if(!w.diagnostics.valid)throw std::runtime_error("Cannot group an unvalidated world");
  std::map<uint64_t,std::vector<const ArchitectureElement*>> groups;
  for(const auto& e:w.architecture.elements)if(!onlyRegion||e.region==*onlyRegion)groups[e.region].push_back(&e);
  if(groups.empty())throw std::runtime_error("Selected World Architect region is empty");
  auto firstBrush=w.map.find("{",w.map.find("\n")+1);if(firstBrush==w.map.npos)throw std::runtime_error("Generated worldspawn has no brush boundary");
  std::ostringstream out;out.imbue(std::locale::classic());out<<w.map.substr(0,firstBrush)<<"}\n";
  std::map<uint64_t,size_t> parents;size_t next=0;
  for(const auto& [region,elements]:groups){
    parents[region]=++next;
    out<<"{\n\"classname\" \"func_group\"\n\"_tb_type\" \"_tb_group\"\n\"_tb_id\" \""<<next<<"\"\n\"_tb_name\" \"World Architect region "<<region<<"\"\n\"_wa_project\" \""<<project<<"\"\n\"_wa_region\" \""<<region<<"\"\n\"_wa_structure\" \""<<w.architectureKey<<"\"\n";
    for(auto e:elements)out<<al::writeBrush(e->brush);out<<"}\n";
  }
  auto entity=[&](al::PointEntity e,uint64_t region){auto parent=parents.find(region);if(parent==parents.end())return;e.fields["_tb_group"]=std::to_string(parent->second);out<<al::writeEntity(e);};
  for(const auto& e:w.architecture.entities)entity(e.entity,e.region);
  for(auto brushEntity:w.architecture.brushEntities){auto parent=parents.find(brushEntity.region);if(parent==parents.end())continue;brushEntity.entity.fields["_tb_group"]=std::to_string(parent->second);
    for(const auto* key:{"target","targetname"}){auto field=brushEntity.entity.fields.find(key);if(field!=brushEntity.entity.fields.end()&&field->second.find("wa_gate_")==0)field->second="wa_"+std::to_string(hash(project))+"_gate_"+field->second.substr(8);}
    out<<writeBrushEntity(brushEntity);
  }
  if(!onlyRegion){auto spawn=w.navigation.nodes[w.navigation.mainRoute.front()].feet+V{0,0,24};entity({{{"classname",capability(w.profile).spawn},{"origin",vectorText(spawn)},{"angle","0"}}},0);}
  for(const auto& beat:w.mission.nodes)entity({{{"classname","info_null"},{"origin",vectorText(beat.position+V{0,0,32})},{"_wa_role",roleName(beat.role)},{"_wa_id",std::to_string(beat.id)}}},missionRegion(w,beat));
  auto result=out.str();if(result.size()>16*1024*1024)throw std::runtime_error("Grouped world exceeds the MAP import budget");auto parsed=al::parseMap(result);size_t count=0;for(const auto& group:groups)count+=group.second.size();for(const auto& e:w.architecture.brushEntities)if(parents.count(e.region))count+=e.brushes.size();if(parsed.brushes.size()!=count)throw std::runtime_error("Grouped world brush roundtrip failed");return result;
}
struct PrefixCache {std::array<std::string,5> keys;std::array<std::shared_ptr<const WorldPlan>,5> plans;};
inline Settings freezeThrough(Settings settings,const WorldPlan& accepted,const Registry& registry,int stage){
  if(!accepted.diagnostics.valid||accepted.registryFingerprint!=registry.fingerprint||accepted.profile!=settings.profile||accepted.preset!=settings.preset||stage<0||stage>4)throw std::runtime_error("Freeze requires a current accepted candidate from this registry and a generative stage");
  settings.locked.fill(false);settings.frozen.fill(Json(nullptr));settings.frozenRegistry=registry.fingerprint;
  for(int i=0;i<=stage;i++){if(!accepted.stageInputs[i])throw std::runtime_error("Accepted candidate is missing its stage recipe");settings.locked[i]=true;settings.frozen[i]=serialize(cleanSettings(*accepted.stageInputs[i]));}
  validate(settings,registry);return settings;
}
inline Settings regenerateDetails(Settings settings,const WorldPlan& accepted,const Registry& registry,uint64_t region,uint64_t seed){
  if(settings.regionRecipes.count(region))throw std::runtime_error("Unpin this building before regenerating its details");
  if(std::none_of(accepted.spatial.buildings.begin(),accepted.spatial.buildings.end(),[&](const auto& b){return b.id==region;}))throw std::runtime_error("Select an existing building for detail regeneration");
  settings=freezeThrough(settings,accepted,registry,4);settings.seed=accepted.seed;settings.candidates=1;
  // Only the requested building's Damage/Dressing streams change. Every
  // upstream recipe and every other region's original stream remain intact.
  for(int stage=3;stage<=4;stage++){auto input=deserialize(settings.frozen[stage],registry,true);input.regionSeeds[region]=seed;settings.frozen[stage]=serialize(cleanSettings(input));}
  validate(settings,registry);return settings;
}
inline Settings regenerateRoomDressing(Settings settings,const WorldPlan& accepted,const Registry& registry,uint64_t room,uint64_t seed){
  bool found=false;for(const auto& b:accepted.spatial.buildings)for(const auto& r:b.rooms)if(r.id==room){if(settings.regionRecipes.count(b.id))throw std::runtime_error("Unpin the building before regenerating a room");found=true;}
  if(!found)throw std::runtime_error("Select an existing functional room for furnishing regeneration");
  settings=freezeThrough(settings,accepted,registry,4);settings.seed=accepted.seed;settings.candidates=1;
  auto input=deserialize(settings.frozen[4],registry,true);input.roomSeeds[room]=seed;settings.frozen[4]=serialize(cleanSettings(input));validate(settings,registry);return settings;
}
inline Settings pinBuilding(Settings settings,const WorldPlan& accepted,const Registry& registry,uint64_t region){
  if(settings.regionRecipes.count(region))throw std::runtime_error("This building is already pinned");
  if(std::none_of(accepted.spatial.buildings.begin(),accepted.spatial.buildings.end(),[&](const auto& b){return b.id==region;}))throw std::runtime_error("Select an existing building to pin");
  settings=freezeThrough(settings,accepted,registry,1);
  std::array<Json,3> recipes;for(int n=2;n<=4;n++){
    if(!accepted.stageInputs[n])throw std::runtime_error("Accepted building is missing its generation recipe");
    recipes[n-2]=serialize(unpinnedSettings(regionInput(*accepted.stageInputs[n],region,n,registry)));
  }
  settings.pinnedRegions.push_back(region);settings.regionRecipes.emplace(region,std::move(recipes));settings.pinnedRegistry=registry.fingerprint;validate(settings,registry);return settings;
}
inline Settings unpinBuilding(Settings settings,uint64_t region){
  settings.regionRecipes.erase(region);settings.pinnedRegions.erase(std::remove(settings.pinnedRegions.begin(),settings.pinnedRegions.end(),region),settings.pinnedRegions.end());
  if(settings.pinnedRegions.empty())settings.pinnedRegistry=0;
  // Older frozen Architecture/Damage/Dressing inputs may themselves contain
  // the removed pin. Keep only the shared Mission/Site baseline on unpin.
  for(int n=2;n<5;n++){settings.locked[n]=false;settings.frozen[n]=nullptr;}return settings;
}
inline WorldPlan generate(const Settings& settings,const Registry& registry,const std::atomic<bool>* cancel=nullptr,std::atomic<int>* progress=nullptr,PrefixCache* cache=nullptr){
  validate(settings,registry);WorldPlan w;w.seed=settings.seed;w.registryFingerprint=registry.fingerprint;w.profile=settings.profile;w.preset=settings.preset;const auto& preset=registry.preset(settings.preset);w.theme=preset.theme;
  const int last=lockedThrough(settings);std::array<Settings,7> inputs;for(int n=0;n<7;n++)inputs[n]=n<=last?deserialize(settings.frozen[n],registry,true):cleanSettings(settings);
  std::array<std::string,5> keys;int first=0;
  if(last>=0){Json recipes=Json::array();for(int n=0;n<=last;n++){recipes.push_back(settings.frozen[n]);keys[n]=std::to_string(registry.fingerprint)+recipes.dump();}
    if(cache)for(int n=last;n>=0;n--)if(cache->plans[n]&&cache->keys[n]==keys[n]){w=*cache->plans[n];w.seed=settings.seed;w.diagnostics.cachedStages.clear();first=n+1;for(int i=0;i<=n;i++)w.diagnostics.cachedStages.push_back(stages[i]);break;}
  }
  auto stage=[&](int n,auto action){
    if(n<first)return;cancelled(cancel);if(progress)progress->store(n);w.seeds[stages[n]]=subSeed(inputs[n].seed,stages[n]);w.stageInputs[n]=std::make_shared<const Settings>(inputs[n]);auto before=std::chrono::steady_clock::now();action(inputs[n]);w.diagnostics.timing[stages[n]]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
    if(n<=last&&cache){cache->keys[n]=keys[n];cache->plans[n]=std::make_shared<const WorldPlan>(w);}
  };
  stage(0,[&](const auto& input){w.mission=mission(input,stagePreset(input,registry));validateMission(w.mission);});
  stage(1,[&](const auto& input){const auto p=stagePreset(input,registry);site(w,input,p);scenarioSite(w,input,p);floorplan(w,input,p);scenarioMission(w,p);reservePlanning(w,input);});
  stage(2,[&](const auto& input){w.theme=stagePreset(input,registry).theme;w.assetKey=themeAssetKey(registry.theme(w.theme));shell(w);streets(w);if(stagePreset(input,registry).roadPattern=="radial"&&!w.spatial.roads.empty()){double radius=std::min(240.,stagePreset(input,registry).width*input.scale*.12);std::vector<V> polygon;for(int i=0;i<8;i++){double angle=i*3.141592653589793/4;polygon.push_back({radius*std::cos(angle),radius*std::sin(angle),0});}addBrush(w,900,"radial gathering plaza",prism(polygon,-4,0,material(w,2)));}for(auto& b:w.spatial.buildings){const auto local=regionInput(input,b.id,2,registry);const auto p=stagePreset(local,registry);building(w,local,p,b,cancel);if(p.ruinEvent=="collapsed-stair")receptionAnnex(w,local,b);if(p.ruinEvent=="flooded-platform")floodedPlatform(w,local,b);scenarioArchitecture(w,local,p,b);themeArchitecture(w,local,b,registry.theme(w.theme),cancel);}realizeGates(w,input,registry);std::string structure;for(const auto& e:w.architecture.elements)structure+=al::writeBrush(e.brush);for(const auto& e:w.architecture.entities)structure+=al::writeEntity(e.entity);for(const auto& e:w.architecture.brushEntities)structure+=writeBrushEntity(e);w.architectureKey=std::to_string(hash(structure));});
  stage(3,[&](const auto& input){damage(w,input,cancel,registry);});
  stage(4,[&](const auto& input){dressing(w,input,stagePreset(input,registry),cancel,registry);});
  stage(5,[&](const auto& input){navigation(w,input,cancel);});
  stage(6,[&](const auto& input){exportMap(w,input);});
  for(auto& b:w.spatial.buildings)b.pinned=settings.regionRecipes.count(b.id)!=0;
  w.diagnostics.valid=true;
  w.diagnostics.warnings.push_back("Quality scores are heuristics; visual and first-person review remains required.");
  if(settings.profile!=2&&settings.profile!=0&&settings.profile!=1)w.diagnostics.warnings.push_back("Quake II and GoldSrc compiler/play acceptance is not yet certified for this generator.");
  return w;
}
struct CandidateBatch {std::vector<WorldPlan> worlds;std::vector<std::string> rejects;};
inline CandidateBatch candidates(const Settings& settings,const Registry& registry,const std::atomic<bool>* cancel=nullptr,std::atomic<int>* progress=nullptr,PrefixCache* cache=nullptr){
  validate(settings,registry);CandidateBatch out;PrefixCache local;if(!cache)cache=&local;
  for(int i=0;i<settings.candidates;i++){
    cancelled(cancel);auto next=settings;next.seed=i?subSeed(settings.seed,"Candidate",uint64_t(i)):settings.seed;
    try{out.worlds.push_back(generate(next,registry,cancel,progress,cache));}catch(const std::exception& e){cancelled(cancel);out.rejects.push_back(std::to_string(next.seed)+": "+e.what());}
  }
  std::stable_sort(out.worlds.begin(),out.worlds.end(),[](const auto& a,const auto& b){return a.diagnostics.score.at("weighted")>b.diagnostics.score.at("weighted");});
  return out;
}
}
