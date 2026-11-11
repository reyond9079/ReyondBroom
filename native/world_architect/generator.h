// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "plan.h"

namespace ftebroom::worldArchitect {
inline std::string vectorText(V p){std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(12)<<p.x<<' '<<p.y<<' '<<p.z;return out.str();}
inline void cancelled(const std::atomic<bool>* flag){if(flag&&flag->load())throw std::runtime_error("Generation cancelled");}
inline std::string materialId(const WorldPlan& w,int slot){return "wa_"+w.theme.substr(0,7)+std::to_string(slot);}
inline std::string material(const WorldPlan& w,int slot){return materialId(w,slot);}
inline std::string skyMaterial(const WorldPlan& w){return w.profile<2?"wa_sky_"+w.theme:"skywa"+w.theme.substr(0,7);}
inline bool inside(V p,V lo,V hi){return p.x>=lo.x&&p.y>=lo.y&&p.z>=lo.z&&p.x<=hi.x&&p.y<=hi.y&&p.z<=hi.z;}
inline bool overlap(V lo,V hi,V otherLo,V otherHi,double gap=0){return lo.x<otherHi.x+gap&&hi.x>otherLo.x-gap&&lo.y<otherHi.y+gap&&hi.y>otherLo.y-gap&&lo.z<otherHi.z+gap&&hi.z>otherLo.z-gap;}
inline uint64_t regionSeed(const Settings& s,uint64_t id,const char* stage){auto it=s.regionSeeds.find(id);return subSeed(it==s.regionSeeds.end()?s.seed:it->second,stage,id);}
inline size_t navNode(WorldPlan& w,V p,uint64_t region,bool required=true){if(w.navigation.nodes.size()>=4096)throw std::runtime_error("World Architect navigation exceeds 4096 nodes");w.navigation.nodes.push_back({p,region,required});return w.navigation.nodes.size()-1;}
inline void navEdge(WorldPlan& w,size_t a,size_t b){if(a!=b)w.navigation.edges.push_back({a,b});}
inline void addBrush(WorldPlan& w,uint64_t region,const std::string& kind,al::Brush brush){
  if(w.architecture.elements.size()>=16000)throw std::runtime_error("World Architect exceeds the 16000 brush budget");al::validateBrush(brush);w.architecture.elements.push_back({region,kind,std::move(brush)});
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
  explicit Collision(const WorldPlan& w){
    solids.reserve(w.architecture.elements.size());
    for(const auto& element:w.architecture.elements){
      auto poly=al::polyhedron(element.brush);Solid solid;solid.low=poly.min;solid.high=poly.max;
      auto axis=[&](V direction){
        if(al::length(direction)<1e-8)return;direction=al::unit(direction);
        if(std::any_of(solid.planes.begin(),solid.planes.end(),[&](const auto& plane){return al::near(plane.first,direction,1e-7);}))return;
        if(solid.planes.size()>=256)throw std::runtime_error("Convex collision axis budget exceeded");
        double distance=-1e30;for(auto p:poly.vertices)distance=std::max(distance,al::dot(direction,p));solid.planes.push_back({direction,distance});
      };
      for(const auto& face:element.brush.faces)axis(face.normal());
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
  const double factor=s.scale*(interior?1:.66),plotW=preset.width*factor+256,plotD=preset.depth*factor+256,street=preset.street;
  int cols=count>1?2:1,rows=(count+cols-1)/cols;
  const double totalW=cols*plotW+(cols+1)*street,totalD=rows*plotD+(rows+1)*street;
  double high=0;
  for(int i=0;i<count;i++){
    uint64_t id=100+uint64_t(i);Random local(regionSeed(s,id,"Site"));
    double width=preset.width*factor,depth=preset.depth*factor;
    if(!interior){width*=local.between(.85,1.07);depth*=local.between(.86,1.04);}
    // A genuine site hierarchy: roads define plots; masses are inset inside
    // those plots, with independent frontage, depth and height variation.
    double x=interior?0:-totalW/2+street+(i%cols)*(plotW+street)+plotW/2;
    double y=interior?0:-totalD/2+street+(i/cols)*(plotD+street)+plotD/2;
    Building b;b.id=id;b.low={x-width/2,y-depth/2,0};b.high={x+width/2,y+depth/2,double(preset.floors*256)};b.floors=preset.floors;b.archetype=preset.archetype;
    if(!interior)b.floors=std::max(1,preset.floors+local.integer(-1,0));b.high.z=b.floors*256;
    b.pinned=std::find(s.pinnedRegions.begin(),s.pinnedRegions.end(),id)!=s.pinnedRegions.end();
    w.spatial.lots.push_back({id,{x-plotW/2,y-plotD/2,0},{x+plotW/2,y+plotD/2,b.high.z},preset.archetype});
    w.spatial.buildings.push_back(b);high=std::max(high,b.high.z);
  }
  if(interior){w.low={-preset.width*factor/2-160,-preset.depth*factor/2-160,-64};w.high={-w.low.x,-w.low.y,high+256};}
  else{
    w.low={-totalW/2-street/2,-totalD/2-street/2,-64};w.high={totalW/2+street/2,totalD/2+street/2,high+384};
    for(int x=0;x<=cols;x++){double at=-totalW/2+street/2+x*(plotW+street);w.spatial.roads.push_back({1000+uint64_t(x),{at,-totalD/2,0},{at,totalD/2,0},street,x==1?"primary":"secondary",false});}
    for(int y=0;y<=rows;y++){double at=-totalD/2+street/2+y*(plotD+street);w.spatial.roads.push_back({1100+uint64_t(y),{-totalW/2,at,0},{totalW/2,at,0},street,y==1?"primary":"secondary",false});}
    for(const auto& pin:s.roadPins){bool found=false;for(auto& road:w.spatial.roads)if(road.id==pin.id){road=pin;found=true;}if(!found)w.spatial.roads.push_back(pin);}
  }
  for(int a=0;a<3;a++)if(std::max(std::abs(w.low[a]),std::abs(w.high[a]))>capability(s.profile).coordinateLimit)throw std::runtime_error("World exceeds the selected classic BSP profile bounds; reduce scale or choose FTE / Quake III");
  for(const auto& mask:s.influences)if(mask.kind=="avoid")for(const auto& b:w.spatial.buildings)if(overlap(b.low,b.high,mask.low,mask.high))throw std::runtime_error("Building overlaps a designer keep-out volume; move it or reduce the generation scope");
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
}
inline void building(WorldPlan& w,const Settings& s,const Preset& preset,Building& b,const std::atomic<bool>* cancel){
  const auto low=b.low,high=b.high;const double cy=(low.y+high.y)/2,hall=std::max(72.,s.player.radius*2+24),stairX=high.x-320;
  Random rng(regionSeed(s,b.id,"Architecture"));
  const int roomPairs=std::max(2,preset.rooms/2);const double roomEnd=stairX-160;
  std::vector<double> splits{low.x};for(int i=1;i<roomPairs;i++)splits.push_back(low.x+(roomEnd-low.x)*(i+rng.between(-.18,.18))/roomPairs);splits.push_back(roomEnd);
  std::vector<size_t> halls;
  for(int floor=0;floor<b.floors;floor++){
    cancelled(cancel);double z=floor*256.;
    // Leave the stairwell open at upper levels. Wide side landings and the
    // central corridor are actual connected floor plates.
    if(!floor)box(w,b.id,"floor plate",{low.x,low.y,-16},{high.x,high.y,0},0);
    else{
      box(w,b.id,"floor plate",{low.x,low.y,z-16},{stairX,high.y,z},0);
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
      wall(w,b.id,{low.x,y,0},{high.x,y,0},z,z+240,16,1,windows);
      box(w,b.id,"window sill course",{low.x-8,y-13,z+60},{high.x+8,y+13,z+72},2);
      box(w,b.id,"floor facade cornice",{low.x-12,y-14,z+236},{high.x+12,y+14,z+252},2);
    }
    std::vector<std::array<double,4>> entrance{{depth/2-48,depth/2+48,z,z+112}};
    wall(w,b.id,{low.x,low.y,0},{low.x,high.y,0},z,z+240,16,1,entrance);
    wall(w,b.id,{high.x,low.y,0},{high.x,high.y,0},z,z+240,16,1,floor?std::vector<std::array<double,4>>{}:entrance);
    const auto hallLeft=navNode(w,{low.x+40,cy,z+1},b.id),hallRight=navNode(w,{stairX-40,cy,z+1},b.id);navEdge(w,hallLeft,hallRight);halls.push_back(hallRight);
    if(!floor){auto outside=navNode(w,{low.x-56,cy,1},b.id);navEdge(w,outside,hallLeft);}
    for(int side=0;side<2;side++)for(int i=0;i<roomPairs;i++){
      double x0=splits[i],x1=splits[i+1],door=(x0+x1)/2;
      double y0=side?cy+hall:low.y,y1=side?high.y:cy-hall,inner=side?y0:y1;
      if(i)wall(w,b.id,{x0,y0,0},{x0,y1,0},z,z+240,12,1);
      wall(w,b.id,{x0,inner,0},{x1,inner,0},z,z+240,12,1,{{door-x0-48,door-x0+48,z,z+112}});
      Room room;room.id=b.id*10000+floor*100+side*roomPairs+i;room.low={x0+8,y0+8,z};room.high={x1-8,y1-8,z+240};room.floor=floor;room.entry={door,inner,z+1};
      static const char* functions[]={"reception","ward","service","storage","treatment","utility","waiting","workshop"};room.function=functions[(i+side*2+floor)%8];
      b.rooms.push_back(room);
      const auto corridor=navNode(w,{door,cy,z+1},b.id),entry=navNode(w,room.entry,b.id),center=navNode(w,{door,(y0+y1)/2,z+1},b.id);
      navEdge(w,hallLeft,corridor);navEdge(w,corridor,hallRight);navEdge(w,corridor,entry);navEdge(w,entry,center);
      // Lighting follows the room function, with warmer public spaces and
      // cooler service rooms. No game-specific enemy/entity vocabulary.
      al::PointEntity light;light.fields={{"classname","light"},{"origin",vectorText({door,(y0+y1)/2,z+192})},{"light",preset.archetype=="station"?"180":"240"},{"_color",side?"0.72 0.83 1":"1 0.82 0.59"}};if(s.profile==4)light.fields["_light"]="255 220 180 240";w.architecture.entities.push_back(light);
    }
    // Rectangular room partitions end before the stairwell; a service lobby
    // and a pair of columns distinguish the larger terminal spatial beat.
    wall(w,b.id,{roomEnd,low.y,0},{roomEnd,cy-hall,0},z,z+240,12,1);
    wall(w,b.id,{roomEnd,cy+hall,0},{roomEnd,high.y,0},z,z+240,12,1);
    for(int side:{-1,1})box(w,b.id,"structural column",{stairX-18,cy+side*(hall+24)-12,z},{stairX+2,cy+side*(hall+24)+12,z+240},2);
    if(floor+1<b.floors){
      // An enclosed switchback flight: every 16-unit riser is a real brush.
      // Walk nodes are sampled on the actual treads and tested with step-up
      // hull sweeps; a logical graph edge alone never proves stairs playable.
      const bool reverse=floor%2;const double lane=cy+(reverse?40:-40);
      size_t previous=hallRight;
      if(reverse){
        auto back=navNode(w,{stairX-40,cy-144,z+1},b.id),turn=navNode(w,{high.x-24,cy-144,z+1},b.id);
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
        const auto turn=navNode(w,{high.x-24,cy-144,z+257},b.id),back=navNode(w,{stairX-40,cy-144,z+257},b.id),center=navNode(w,{stairX-40,cy,z+257},b.id);
        navEdge(w,end,turn);navEdge(w,turn,back);navEdge(w,back,center);
      }else{
        auto center=navNode(w,{stairX-40,cy,z+257},b.id);navEdge(w,end,center);
      }

    }
  }
  // A roof with a retained frame. Later damage may open it to the sealed sky.
  box(w,b.id,"roof plate",{low.x-16,low.y-16,high.z-16},{high.x+16,high.y+16,high.z},2);
  for(int side:{-1,1})box(w,b.id,"roof parapet",{low.x-16,side<0?low.y-16:high.y,high.z},{high.x+16,side<0?low.y:high.y+16,high.z+48},1);
  // Vertical bays, deep entrance canopy and corner pilasters give the mass an
  // architectural hierarchy rather than identical solid cells.
  for(double x:{low.x,high.x})for(double y:{low.y,high.y})box(w,b.id,"corner pilaster",{x-18,y-18,0},{x+18,y+18,high.z+20},2);
  box(w,b.id,"entrance canopy",{low.x-96,cy-80,120},{low.x+8,cy+80,136},2);
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
  auto brush=al::box(lo,hi,texture);V n=al::unit({1,1,.4});al::Brush clipped;
  if(al::clipBrush(brush,n,al::dot(n,hi)-amount,clipped))brush=std::move(clipped);return brush;
}
inline double distanceToSegment(V p,V a,V b){V d=b-a;double den=al::dot(d,d);return al::length(p-(a+d*(den>0?std::clamp(al::dot(p-a,d)/den,0.,1.):0)));}
inline bool protectedRoute(const WorldPlan& w,V p,double radius){
  for(const auto& edge:w.navigation.edges)if(distanceToSegment(p,w.navigation.nodes[edge[0]].feet,w.navigation.nodes[edge[1]].feet)<radius)return true;return false;
}
inline double influence(const Settings& settings,V p,const std::string& kind,double fallback){
  double result=fallback;for(const auto& mask:settings.influences)if(mask.kind==kind&&inside(p,mask.low,mask.high))result=mask.strength;return result;
}
inline void damage(WorldPlan& w,const Settings& s,const std::atomic<bool>* cancel){
  for(const auto& building:w.spatial.buildings){
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
inline void dressing(WorldPlan& w,const Settings& s,const Preset& preset,const std::atomic<bool>* cancel){
  for(const auto& building:w.spatial.buildings){
    cancelled(cancel);Random rng(regionSeed(s,building.id,"Dressing"));
    for(const auto& room:building.rooms){
      const double density=influence(s,room.low,"dressing",s.dressing);if(density<.01)continue;
      V p{room.low.x+48,room.low.y+48,room.low.z};if(protectedRoute(w,p,72)||influence(s,p,"preserve",0)>0)continue;
      const bool bed=preset.archetype=="hospital"||preset.archetype=="apartments";
      if(bed){
        box(w,building.id,"bed frame",{p.x-20,p.y-28,p.z+12},{p.x+20,p.y+28,p.z+20},4);
        box(w,building.id,"bed mattress",{p.x-19,p.y-27,p.z+20},{p.x+19,p.y+27,p.z+28},2);
        for(int x:{-1,1})for(int y:{-1,1})box(w,building.id,"bed leg",{p.x+x*16-2,p.y+y*23-2,p.z},{p.x+x*16+2,p.y+y*23+2,p.z+14},4);
      }else{
        double h=preset.archetype=="station"?24:40;
        addBrush(w,building.id,"salvage crate",bevel({p.x-24,p.y-20,p.z},{p.x+24,p.y+20,p.z+h},material(w,4),8));
        box(w,building.id,"crate band",{p.x-25,p.y-22,p.z+8},{p.x+25,p.y-18,p.z+12},2);
      }
      w.assets.push_back({building.id,bed?"abandoned sleeping / treatment area":"service storage","original procedural brush kit",p});
      if(density>.65){
        V q{room.high.x-32,room.high.y-32,p.z};if(!protectedRoute(w,q,70)){
          std::vector<V> polygon;for(int a=0;a<8;a++){double angle=a*6.283185307179586/8;polygon.push_back({q.x+16*std::cos(angle),q.y+16*std::sin(angle),0});}
          addBrush(w,building.id,"utility barrel",prism(polygon,q.z,q.z+40,material(w,3)));w.assets.push_back({building.id,"utility / water storage","original eight-sided barrel",q});
        }
      }
    }
  }
}
inline void streets(WorldPlan& w){
  for(const auto& road:w.spatial.roads){
    const V delta=road.b-road.a;const double length=al::length(delta);if(length<1)continue;
    wall(w,road.id,road.a,road.b,-4,0,road.width,0);
    V side=al::unit(al::cross(delta,{0,0,1}));
    // Curbs remain below step height, with pedestrian access at every frontage.
    for(int sign:{-1,1}){auto offset=side*(sign*(road.width*.5+12));wall(w,road.id,road.a+offset,road.b+offset,0,8,24,2);}
    size_t previous=SIZE_MAX;int count=std::max(1,int(std::ceil(length/128)));
    for(int i=0;i<=count;i++){size_t node=navNode(w,road.a+delta*(double(i)/count)+V{0,0,1},road.id);if(previous!=SIZE_MAX)navEdge(w,previous,node);previous=node;}
  }
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
  std::vector<int> visited(nav.nodes.size(),-1);std::deque<size_t> queue{0};visited[0]=0;
  while(!queue.empty()){size_t n=queue.front();queue.pop_front();for(auto next:adjacency[n])if(visited[next]<0){visited[next]=int(n);queue.push_back(next);}}
  nav.reachable=std::count_if(visited.begin(),visited.end(),[](int n){return n>=0;});
  for(size_t i=0;i<nav.nodes.size();i++)if(nav.nodes[i].required&&visited[i]<0)throw std::runtime_error("Physical navigation rejected disconnected room, street or stair at "+vectorText(nav.nodes[i].feet));
  // Use graph distance for the destination, so a visually distant room behind
  // a wall cannot masquerade as the end of a playable journey.
  std::vector<double> distance(nav.nodes.size(),-1);distance[0]=0;queue={0};
  while(!queue.empty()){auto a=queue.front();queue.pop_front();for(auto b:adjacency[a]){double d=distance[a]+al::length(nav.nodes[b].feet-nav.nodes[a].feet);if(distance[b]<0||d<distance[b]-.01){distance[b]=d;visited[b]=int(a);queue.push_back(b);}}}
  size_t end=size_t(std::max_element(distance.begin(),distance.end())-distance.begin());
  for(size_t n=end,count=0;count++<=nav.nodes.size();n=size_t(visited[n])){nav.mainRoute.push_back(n);if(!n)break;}
  std::reverse(nav.mainRoute.begin(),nav.mainRoute.end());w.diagnostics.routeLength=distance[end];
  for(size_t i=0;i<w.mission.nodes.size();i++){
    auto& node=w.mission.nodes[i];size_t index=std::min(nav.mainRoute.size()-1,i*nav.mainRoute.size()/w.mission.nodes.size());
    if(node.role==Role::Exit)index=nav.mainRoute.size()-1;
    if(node.pinned){auto nearest=nav.nodes.size();double distance=32;for(size_t n=0;n<nav.nodes.size();n++){double d=al::length(node.position-nav.nodes[n].feet);if(d<distance&&visited[n]>=0){distance=d;nearest=n;}}if(nearest==nav.nodes.size())throw std::runtime_error("Pinned mission beat has no reachable collision-free player position");node.position=nav.nodes[nearest].feet;}
    else node.position=nav.nodes[nav.mainRoute[index]].feet;
  }
  auto& scores=w.diagnostics.score;
  scores["route"]=std::min(1.,w.diagnostics.routeLength/2500);
  std::set<int> areas;for(const auto& b:w.spatial.buildings)for(const auto& r:b.rooms)areas.insert(int((r.high.x-r.low.x)*(r.high.y-r.low.y)/8192));
  scores["variety"]=std::min(1.,areas.size()/6.);
  w.diagnostics.warnings.push_back("This slice scores route length and room-area variation only. Meaningful loops, sightlines, landmarks and narrative quality are not yet scored.");
  double sum=0,weight=0;for(auto [name,value]:settings.weights){auto it=scores.find(name);if(it!=scores.end()){sum+=it->second*value;weight+=value;}}scores["weighted"]=weight?sum/weight:0;
}
inline void exportMap(WorldPlan& w,const Settings& settings){
  if(w.navigation.mainRoute.empty())throw std::runtime_error("Cannot export without validated player route");
  std::ostringstream out;out.imbue(std::locale::classic());out<<"{\n\"classname\" \"worldspawn\"\n\"_wa_schema\" \"1\"\n\"_wa_seed\" \""<<w.seed<<"\"\n\"_wa_preset\" \""<<w.preset<<"\"\n";
  for(const auto& element:w.architecture.elements)out<<al::writeBrush(element.brush);out<<"}\n";
  for(const auto& entity:w.architecture.entities)out<<al::writeEntity(entity);
  auto spawn=w.navigation.nodes[w.navigation.mainRoute.front()].feet+V{0,0,24};
  out<<al::writeEntity({{{"classname",capability(settings.profile).spawn},{"origin",vectorText(spawn)},{"angle","0"}}});
  // Markers are editor metadata; they do not invent unsupported game entities.
  for(const auto& beat:w.mission.nodes)out<<al::writeEntity({{{"classname","info_null"},{"origin",vectorText(beat.position+V{0,0,32})},{"_wa_role",roleName(beat.role)},{"_wa_id",std::to_string(beat.id)}}});
  w.map=out.str();if(w.map.size()>16*1024*1024)throw std::runtime_error("Generated MAP exceeds the 16 MiB import budget");
  auto roundtrip=al::parseMap(w.map);if(roundtrip.brushes.size()!=w.architecture.elements.size())throw std::runtime_error("Quantized MAP brush roundtrip failed");
  w.diagnostics.mapBytes=w.map.size();w.diagnostics.brushes=w.architecture.elements.size();
}
inline WorldPlan generate(const Settings& settings,const Registry& registry,const std::atomic<bool>* cancel=nullptr,std::atomic<int>* progress=nullptr){
  validate(settings,registry);WorldPlan w;w.seed=settings.seed;w.profile=settings.profile;w.preset=settings.preset;const auto& preset=registry.preset(settings.preset);w.theme=preset.theme;
  auto stage=[&](int n,auto action){cancelled(cancel);if(progress)progress->store(n);w.seeds[stages[n]]=subSeed(settings.seed,stages[n]);auto before=std::chrono::steady_clock::now();action();w.diagnostics.timing[stages[n]]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();};
  stage(0,[&]{w.mission=mission(settings,preset);validateMission(w.mission);});
  stage(1,[&]{site(w,settings,preset);});
  stage(2,[&]{shell(w);streets(w);for(auto& b:w.spatial.buildings)building(w,settings,preset,b,cancel);});
  stage(3,[&]{damage(w,settings,cancel);});
  stage(4,[&]{dressing(w,settings,preset,cancel);});
  stage(5,[&]{navigation(w,settings,cancel);});
  stage(6,[&]{exportMap(w,settings);});
  w.diagnostics.valid=true;
  w.diagnostics.warnings.push_back("Quality scores are heuristics; visual and first-person review remains required.");
  if(settings.profile!=2&&settings.profile!=0&&settings.profile!=1)w.diagnostics.warnings.push_back("Quake II and GoldSrc compiler/play acceptance is not yet certified for this generator.");
  return w;
}
struct CandidateBatch {std::vector<WorldPlan> worlds;std::vector<std::string> rejects;};
inline CandidateBatch candidates(const Settings& settings,const Registry& registry,const std::atomic<bool>* cancel=nullptr,std::atomic<int>* progress=nullptr){
  validate(settings,registry);CandidateBatch out;
  for(int i=0;i<settings.candidates;i++){
    cancelled(cancel);auto next=settings;next.seed=i?subSeed(settings.seed,"Candidate",uint64_t(i)):settings.seed;
    try{out.worlds.push_back(generate(next,registry,cancel,progress));}catch(const std::exception& e){cancelled(cancel);out.rejects.push_back(std::to_string(next.seed)+": "+e.what());}
  }
  std::stable_sort(out.worlds.begin(),out.worlds.end(),[](const auto& a,const auto& b){return a.diagnostics.score.at("weighted")>b.diagnostics.score.at("weighted");});
  return out;
}
}
