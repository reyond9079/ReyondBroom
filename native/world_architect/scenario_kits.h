// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Included inside worldArchitect after the convex, navigation and art primitives.
inline void mallService(WorldPlan& w,const Building& b);
inline void scenarioSite(WorldPlan& w,const Settings&,const Preset& preset){
  if(preset.scenario.empty())return;auto& b=w.spatial.buildings.front();
  if(preset.scenario=="factory"||preset.scenario=="mall"){
    if(preset.floors<2)throw std::runtime_error("This scenario requires two floors for its gallery / gantry route");
    b.floors=std::max(2,b.floors);b.high.z=b.floors*256.;w.high.z=std::max(w.high.z,b.high.z+384);
  }
  if(preset.scenario=="checkpoint"||preset.scenario=="courtyard"){
    w.low.x=std::min(w.low.x,b.low.x-288);w.low.y=std::min(w.low.y,(b.low.y+b.high.y)/2-224);w.high.y=std::max(w.high.y,(b.low.y+b.high.y)/2+224);w.high.z=std::max(w.high.z,b.high.z+256);
  }
  if(preset.scenario=="overpass"&&(preset.floors<2||w.spatial.buildings.size()<2))throw std::runtime_error("Overpass traversal requires two multi-floor buildings");
}
inline void scenarioMission(WorldPlan& w,const Preset& preset){
  if(preset.scenario.empty())return;const auto& first=w.spatial.buildings.front();const auto& last=w.spatial.buildings.back();
  auto place=[&](Role role,V position,int floor){for(auto& node:w.mission.nodes)if(node.role==role&&!node.pinned){for(auto& b:w.spatial.buildings)for(auto& room:b.rooms)if(room.mission==node.id)room.mission=0;node.room=0;node.floor=floor;node.position=position;}};
  double cy=(first.low.y+first.high.y)/2;
  if(preset.scenario=="checkpoint"||preset.scenario=="courtyard")place(Role::Landmark,{first.low.x+64,cy,first.high.z+1},first.floors);
  else if(preset.scenario=="factory"||preset.scenario=="mall")place(Role::Landmark,{first.low.x+168,cy+first.hall-32,257},1);
  else if(preset.scenario=="overpass")place(Role::Landmark,{first.high.x-24,cy,257},1);
  if(preset.scenario=="mall")place(Role::Exit,{first.high.x+56,cy,1},0);
  else place(Role::Exit,{last.low.x-56,(last.low.y+last.high.y)/2,1},0);
}
inline void rearApproach(WorldPlan& w,const Building& b,double z){
  double cy=(b.low.y+b.high.y)/2,y=cy+std::max(144.,b.hall+64);std::array<V,4> points{{{b.high.x-360,cy,z+1},{b.high.x-360,y,z+1},{b.high.x-24,y,z+1},{b.high.x-24,cy,z+1}}};size_t last=SIZE_MAX;for(auto p:points){auto n=navNode(w,p,b.id);if(last!=SIZE_MAX)navEdge(w,last,n);last=n;}
}
inline void roofStair(WorldPlan& w,const Settings& settings,const Building& b){
  exteriorTraversalLights(w,b,b.floors);
  const double cy=(b.low.y+b.high.y)/2,half=std::max(28.,settings.player.radius+8);
  for(int floor=1;floor<=b.floors;floor++){
    double z=floor*256.;box(w,b.id,"roof escape gallery",{b.low.x-80,cy-176,z-16},{b.low.x+8,cy+176,z},2);
    for(int sign:{-1,1})box(w,b.id,"roof escape landing",{b.low.x-272,cy+sign*152-24,z-16},{b.low.x+8,cy+sign*152+24,z},2);
    navNode(w,{b.low.x-40,cy,z+1},b.id);
  }
  for(int floor=0;floor<b.floors;floor++){
    bool reverse=floor%2;double z=floor*256.,x=b.low.x-(reverse?224:144),start=cy+(reverse?148:-148),end=cy+(reverse?-148:148);
    auto middle=navNode(w,{b.low.x-40,cy,z+1},b.id),turn=navNode(w,{b.low.x-40,start,z+1},b.id),previous=navNode(w,{x,start,z+1},b.id);navEdge(w,middle,turn);navEdge(w,turn,previous);
    for(int step=0;step<16;step++){double y=cy-128+(reverse?15-step:step)*16,height=z+(step+1)*16;box(w,b.id,"roof escape tread",{x-half,y,height-12},{x+half,y+16,height},2);auto n=navNode(w,{x,y+(reverse?24:-8),height+1},b.id);navEdge(w,previous,n);previous=n;}
    for(int side:{-1,1})tube(w,b.id,"roof stair stringer",{x+side*(half-4),cy+(reverse?128:-128),z+4},{x+side*(half-4),cy+(reverse?-128:128),z+252},4,3);
    auto finish=navNode(w,{x,end,z+257},b.id),back=navNode(w,{b.low.x-40,end,z+257},b.id),center=navNode(w,{b.low.x-40,cy,z+257},b.id);navEdge(w,previous,finish);navEdge(w,finish,back);navEdge(w,back,center);
  }
  auto out=navNode(w,{b.low.x-40,cy,b.high.z+1},b.id),inside=navNode(w,{b.low.x+64,cy,b.high.z+1},b.id);navEdge(w,out,inside);
  for(int side:{-1,1})box(w,b.id,"roof lookout rail",{b.low.x-4,side<0?b.low.y:cy+64,b.high.z+36},{b.low.x+4,side<0?cy-64:b.high.y,b.high.z+44},3);
  box(w,b.id,"roof lookout rear rail",{b.high.x-4,b.low.y,b.high.z+36},{b.high.x+4,b.high.y,b.high.z+44},3);
}
inline void factoryHall(WorldPlan& w,const Settings& settings,const Building& b){
  double cy=(b.low.y+b.high.y)/2,from=b.low.x+208,to=b.bays.back()-192;const double lane=(b.hall-32)/2;
  for(double x=from;x<=to;x+=160)for(int sign:{-1,1}){
    V p{x,cy+sign*lane,0};if(protectedRoute(w,p,24+settings.player.radius+8))continue;
    box(w,b.id,"production press plinth",p+V{-24,-24,0},p+V{24,24,16},2);for(int side:{-1,1})box(w,b.id,"production press column",p+V{side*20.-4,-20,16},p+V{side*20.+4,20,96},3);box(w,b.id,"production press head",p+V{-28,-24,88},p+V{28,24,104},3);tube(w,b.id,"production press ram",p+V{0,0,40},p+V{0,0,88},9,3);
  }
  double beam=b.atrium?432:208;box(w,b.id,"gantry crane rail",{b.low.x+144,cy-b.hall+16,beam},{b.bays.back()-128,cy-b.hall+32,beam+16},3);box(w,b.id,"gantry crane rail",{b.low.x+144,cy+b.hall-32,beam},{b.bays.back()-128,cy+b.hall-16,beam+16},3);
  double x=(b.low.x+144+b.bays.back()-128)/2;box(w,b.id,"gantry crane bridge",{x-12,cy-b.hall+16,beam},{x+12,cy+b.hall-16,beam+20},5);tube(w,b.id,"suspended crane hook",{x,cy,beam-112},{x,cy,beam},5,3);
  navNode(w,{b.low.x+168,cy+b.hall-32,257},b.id);
  if(settings.damage>.2)for(auto& e:w.architecture.elements)if(e.region==b.id&&e.kind=="atrium roof frame"){
    auto poly=al::polyhedron(e.brush);auto center=(poly.min+poly.max)*.5;if(influence(settings,center,"preserve",0)>0)continue;auto normal=al::unit({1,.3,1});al::Brush fractured;if(al::clipBrush(e.brush,normal,al::dot(normal,poly.max)-32,fractured)){e.brush=std::move(fractured);e.kind="broken industrial roof frame";}
  }
  for(int i=0;i<8;i++){
    double px=b.low.x+(b.high.x-b.low.x)*(i+.5)/8,y=b.high.y+20;double z=32+(i%3)*24.;tube(w,b.id,"overgrown facade vine",{px,y,z},{px+12,y,z+112},3,3);
    for(int leaf=0;leaf<3;leaf++)addBrush(w,b.id,"ivy leaf cluster",bevel({px-10,y-4,z+leaf*32},{px+18,y+4,z+leaf*32+18},material(w,3),6));
  }
}
inline void bridgeDeck(WorldPlan& w,uint64_t region,V a,V b,const Player& player){
  addBrush(w,region,"broken overpass deck",orientedPrism({{-64,-16,0},{64,-16,0},{64,0,0},{-64,0,0}},a,b,material(w,2)));
  auto direction=al::unit(b-a),side=al::cross(direction,{0,0,1});for(int sign:{-1,1}){
    auto offset=side*(sign*58.);WorldPlan candidate;candidate.profile=w.profile;candidate.theme=w.theme;candidate.assetKey=w.assetKey;tube(candidate,region,"remaining overpass guardrail",a+offset+V{0,0,40},b+offset+V{0,0,40},4,3);Collision rail(candidate);
    bool clear=std::none_of(w.navigation.nodes.begin(),w.navigation.nodes.end(),[&](const auto& n){return rail.blocked(n.feet,n.feet,player);})&&std::none_of(w.navigation.edges.begin(),w.navigation.edges.end(),[&](auto e){return rail.blocked(w.navigation.nodes[e[0]].feet,w.navigation.nodes[e[1]].feet,player);});
    if(clear)addBrush(w,region,"remaining overpass guardrail",std::move(candidate.architecture.elements.front().brush));
  }
  double length=al::length(b-a);int count=std::max(1,int(std::ceil(length/64)));size_t last=SIZE_MAX;for(int i=0;i<=count;i++){auto point=a+(b-a)*(double(i)/count)+V{0,0,1};auto n=navNode(w,point,region);if(last!=SIZE_MAX)navEdge(w,last,n);last=n;}
}
inline void overpass(WorldPlan& w,const Building& first,const Building& second,const Player& player){
  double cy=(first.low.y+first.high.y)/2,otherY=(second.low.y+second.high.y)/2;
  V a{first.high.x-8,cy,256},b{second.low.x+8,otherY,256};double length=al::length(b-a);if(length<384)throw std::runtime_error("Overpass parcels leave insufficient room for a real broken span");auto direction=(b-a)/length;double gap=std::min(320.,length*.55),piece=(length-gap)/2;
  bridgeDeck(w,first.id,a,a+direction*piece,player);bridgeDeck(w,second.id,b-direction*piece,b,player);rearApproach(w,first,256);
}
inline void evacuationShelter(WorldPlan& w,const Building& b){
  double cy=(b.low.y+b.high.y)/2;box(w,b.id,"shelter assembly canopy",{b.low.x-128,cy-136,160},{b.low.x+8,cy+136,176},2);for(int sign:{-1,1})box(w,b.id,"shelter portico column",{b.low.x-120,cy+sign*120-8,0},{b.low.x-104,cy+sign*120+8,160},2);
  V mast{b.high.x-128,b.high.y-96,b.high.z};for(int x:{-1,1})for(int y:{-1,1})tube(w,b.id,"evacuation beacon tower",mast+V{x*16.,y*16.,0},mast+V{x*16.,y*16.,128},4,3);box(w,b.id,"beacon lantern",mast+V{-24,-24,112},mast+V{24,24,144},5);
  addBrush(w,b.id,"evacuation pennant",orientedPrism({{0,0,0},{80,24,0},{0,64,0}},mast+V{0,0,112},mast+V{0,3,112},material(w,5)));
}
inline void checkpoint(WorldPlan& w,const Settings& settings,const Building& b){
  double cy=(b.low.y+b.high.y)/2;for(const auto& road:w.spatial.roads)if(road.id==2000+b.id-100){double length=al::length(road.b-road.a);int segments=std::max(1,int(std::ceil(length/128)));auto p=road.a+(road.b-road.a)*((segments-.5)/segments);if(length/segments<2*(settings.player.radius+12))continue;addBrush(w,b.id,"checkpoint concrete barrier",bevel(p+V{-8,-72,0},p+V{8,72,40},material(w,2),6));box(w,b.id,"checkpoint security screen",p+V{-4,-72,40},p+V{4,72,112},4);box(w,b.id,"checkpoint warning stripe",p+V{-5,-74,96},p+V{5,74,112},5);}
  box(w,b.id,"sentry booth roof",{b.low.x-96,cy+72,96},{b.low.x,cy+136,104},2);box(w,b.id,"sentry booth rear",{b.low.x-8,cy+72,0},{b.low.x,cy+136,96},1);for(int sign:{-1,1})box(w,b.id,"sentry booth side",{b.low.x-96,cy+(sign<0?72:128),0},{b.low.x,cy+(sign<0?80:136),96},1);
}
inline void garages(WorldPlan& w,const Building& b){
  double cy=(b.low.y+b.high.y)/2;for(const auto& room:b.rooms)if(!room.floor&&room.function=="garage"){
    int sign=room.low.y>cy?1:-1;double x=(room.low.x+room.high.x)/2,y=sign>0?b.high.y:b.low.y;
    box(w,b.id,"garage apron",{x-64,std::min(y,y+sign*80),0},{x+64,std::max(y,y+sign*80),16},2);
    box(w,b.id,"raised garage shutter",{x-52,y-14,112},{x+52,y+14,132},3);for(int z=114;z<132;z+=6)box(w,b.id,"shutter roll seam",{x-53,y-16,double(z)},{x+53,y+16,double(z+2)},2);
    auto outside=navNode(w,{x,y+sign*112,1},b.id),ramp=navNode(w,{x,y+sign*48,17},b.id),inside=navNode(w,{x,(room.low.y+room.high.y)/2,1},b.id);navEdge(w,outside,ramp);navEdge(w,ramp,inside);
  }
}
inline void scenarioArchitecture(WorldPlan& w,const Settings& settings,const Preset& preset,const Building& b){
  if(preset.scenario.empty())return;bool primary=b.id==w.spatial.buildings.front().id;
  if(primary&&(preset.scenario=="checkpoint"||preset.scenario=="courtyard"))roofStair(w,settings,b);
  if(primary&&preset.scenario=="checkpoint")checkpoint(w,settings,b);
  if(primary&&preset.scenario=="factory")factoryHall(w,settings,b);
  if(preset.scenario=="courtyard")garages(w,b);
  if(preset.scenario=="mall"){mallService(w,b);navNode(w,{b.low.x+168,(b.low.y+b.high.y)/2+b.hall-32,257},b.id);}
  if(preset.scenario=="evacuation"&&b.id==w.spatial.buildings.back().id)evacuationShelter(w,b);
  if(preset.scenario=="overpass"){
    double cy=(b.low.y+b.high.y)/2;box(w,b.id,"overpass service balcony",{b.high.x-8,cy-72,240},{b.high.x+72,cy+72,256},2);rearApproach(w,b,256);navNode(w,{b.high.x+40,cy,257},b.id);
    if(b.id==w.spatial.buildings.back().id)overpass(w,w.spatial.buildings[0],w.spatial.buildings[1],settings.player);
  }
}
inline void mallService(WorldPlan& w,const Building& b){
  double wallY=b.high.y-112,corridorY=b.high.y-64;std::vector<std::array<double,4>> doors;
  size_t previous=navNode(w,{b.low.x+48,corridorY,1},b.id);
  for(const auto& room:b.rooms)if(!room.floor&&room.low.y>(b.low.y+b.high.y)/2){
    double x=(room.low.x+room.high.x)/2;doors.push_back({x-b.low.x-40,x-b.low.x+40,0,112});auto inside=navNode(w,{x,(room.low.y+room.high.y)/2,1},b.id),service=navNode(w,{x,corridorY,1},b.id);navEdge(w,inside,service);navEdge(w,previous,service);previous=service;
    box(w,b.id,"service corridor lamp",{x-16,corridorY-12,216},{x+16,corridorY+12,232},2);w.architecture.entities.push_back({b.id,{{{"classname","light"},{"origin",vectorText({x,corridorY,176})},{"light","200"},{"_color","0.8 0.9 1"}}}});
  }
  auto end=navNode(w,{b.bays.back()-48,corridorY,1},b.id);navEdge(w,previous,end);
  wall(w,b.id,{b.low.x,wallY,0},{b.bays.back(),wallY,0},0,240,12,1,doors);
  // The ordinary rear exit includes a deliberate stairwell-side route.
  rearApproach(w,b,0);auto inner=navNode(w,{b.high.x-24,(b.low.y+b.high.y)/2,1},b.id),outside=navNode(w,{b.high.x+56,(b.low.y+b.high.y)/2,1},b.id);navEdge(w,inner,outside);
  for(const auto& room:b.rooms)if(room.function=="retail"||room.function=="food-court"||room.function=="anchor-store"){
    double x=room.entry.x,y=room.entry.y,z=room.low.z;for(int side:{-1,1})box(w,b.id,"retail entrance mullion",{x+side*72-3,y-10,z},{x+side*72+3,y+10,z+144},3);box(w,b.id,"retail fascia sign",{x-64,y-12,z+144},{x+64,y+12,z+168},5);
  }
}
