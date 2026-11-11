// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/world_architect/generator.h"
#include "../plugins/autolevel/al_brush.cpp"
#include <fstream>
#include <iostream>
using namespace ftebroom::worldArchitect;
int main(int argc,char** argv)try{
 if(argc!=2)throw std::runtime_error("Pass registry.json");Json j;std::ifstream(argv[1])>>j;auto registry=readRegistry(j);Settings settings;int count=0;
 auto check=[&](bool ok,const char* label){if(!ok)throw std::runtime_error(label);++count;std::cout<<"PASS "<<label<<'\n';};
 auto rejects=[&](auto action,const char* label){try{action();}catch(const std::exception&){check(true,label);return;}throw std::runtime_error(std::string("Expected rejection: ")+label);};
 check(registry.presets.size()==8&&registry.themes.size()==6,"versioned prototype registry has eight cases and six original material themes");
 check(serialize(deserialize(serialize(settings),registry))==serialize(settings),"settings roundtrip retains deterministic generation inputs");
 auto future=serialize(settings);future["schemaVersion"]=2;rejects([&]{deserialize(future,registry);},"future schemas fail explicitly");
 auto invalid=settings;invalid.scale=std::numeric_limits<double>::infinity();rejects([&]{validate(invalid,registry);},"nonfinite settings rejected");
 invalid=settings;invalid.locked[0]=true;rejects([&]{validate(invalid,registry);},"unfinished stage locks cannot silently pretend to preserve content");
 auto graph=mission(settings,registry.preset(settings.preset));validateMission(graph);check(!graph.nodes.empty(),"generated mission has a coherent start-to-exit traversal");
 graph.edges.clear();rejects([&]{validateMission(graph);},"disconnected required mission rejected");
 graph.nodes={{1,Role::Start,"start",{},true},{2,Role::Reward,"key",{},false},{3,Role::Exit,"exit",{},true}};
 graph.edges={{1,2,false,true,"irreversible trap",0},{1,3,false,false,"gate",2}};
 rejects([&]{validateMission(graph);},"a key inside an irreversible trap cannot unlock a separately reachable gate");
 graph.edges.push_back({2,1,false,true,"return",0});validateMission(graph);check(true,"a usable key-return route satisfies the gate dependency");
 std::atomic<bool> cancel{true};rejects([&]{generate(settings,registry,&cancel);},"cancelled generation never returns a successful world");
 check(subSeed(17,"Dressing",100)==subSeed(17,"Dressing",100)&&subSeed(17,"Damage",100)!=subSeed(17,"Dressing",100)&&subSeed(17,"Dressing",100)!=subSeed(17,"Dressing",101),"stage and region streams are reproducible and independent");
 auto world=generate(settings,registry);Collision collision(world);
 WorldPlan programmed;programmed.mission=mission(settings,registry.preset(settings.preset));site(programmed,settings,registry.preset(settings.preset));floorplan(programmed,settings,registry.preset(settings.preset));
 check(programmed.architecture.elements.empty()&&!programmed.spatial.buildings[0].rooms.empty(),"functional room programs exist before architecture emits its first brush");
 std::set<uint64_t> reserved;for(const auto& beat:programmed.mission.nodes)if(beat.role!=Role::Start&&beat.role!=Role::Exit)check(beat.room&&reserved.insert(beat.room).second,"each indoor mission beat reserves a distinct functional room before geometry");
 for(const auto& beat:world.mission.nodes)if(beat.required)check(std::any_of(world.navigation.mainRoute.begin(),world.navigation.mainRoute.end(),[&](size_t at){return al::length(world.navigation.nodes[at].feet-beat.position)<.01;}),"physical main route visits the planned required mission room");
 check(al::length(world.navigation.nodes[world.navigation.mainRoute.front()].feet-world.mission.nodes.front().position)<.01,"exported route starts at the planned mission start");
 auto countKind=[&](const WorldPlan& w,const std::string& kind){return std::count_if(w.architecture.elements.begin(),w.architecture.elements.end(),[&](const auto& element){return element.kind==kind;});};
 check(countKind(world,"broken stair remnant")==7&&countKind(world,"exterior fire stair")==32,"hospital has a physically broken central flight and two real exterior flights");
 check(countKind(world,"semicircular reception floor")==1&&std::any_of(world.architecture.elements.begin(),world.architecture.elements.end(),[](const auto& e){return e.kind=="semicircular reception floor"&&e.brush.faces.size()>6;}),"hospital contains convex non-rectangular reception architecture");
 auto exit=std::find_if(world.mission.nodes.begin(),world.mission.nodes.end(),[](const auto& n){return n.role==Role::Exit;});
 check(exit!=world.mission.nodes.end()&&exit->room==0&&exit->position.x>world.spatial.buildings[0].high.x,"hospital journey ends outside the reception in its planned courtyard");
 auto unruined=settings;unruined.damage=0;auto intact=generate(unruined,registry);check(!countKind(intact,"broken stair remnant")&&countKind(intact,"exterior fire stair")==32,"zero damage restores the central flight while retaining the outdoor route");
 const auto& mass=world.spatial.buildings[0];const double holeX=(mass.low.x+144+mass.bays.back()-128)/2,cy=(mass.low.y+mass.high.y)/2;
 check(!collision.supported({holeX,cy,257},settings.player),"upper atrium has a real unsupported floor opening");
 check(collision.supported({holeX,cy+mass.hall-32,257},settings.player),"atrium side gallery retains a supported player-width route");
 auto corridorRegistry=registry;for(auto& preset:corridorRegistry.presets)if(preset.id==settings.preset)preset.layout="spine";
 auto corridor=generate(settings,corridorRegistry);Collision corridorHull(corridor);
 check(corridorHull.supported({holeX,cy,257},settings.player),"switching atrium grammar to spine changes actual floor geometry");
 check(world.diagnostics.score.at("separatedRoutes")>=1&&corridor.diagnostics.score.at("separatedRoutes")==0,"physical alternatives distinguish atrium galleries from dense links in a single corridor");
 auto compact=registry;for(auto& preset:compact.presets)if(preset.id==settings.preset){preset.rooms=2;preset.floors=1;}
 WorldPlan demand;demand.mission=mission(settings,compact.preset(settings.preset));site(demand,settings,compact.preset(settings.preset));floorplan(demand,settings,compact.preset(settings.preset));
 check(demand.spatial.buildings[0].rooms.size()>=demand.mission.nodes.size()-1,"mission demand expands the room program beyond its minimum architectural count");
 auto metroSettings=settings;metroSettings.preset="metro";auto metro=generate(metroSettings,registry);Collision metroHull(metro);
 auto pool=std::find_if(metro.architecture.elements.begin(),metro.architecture.elements.end(),[](const auto& e){return e.kind=="floodwater";});check(pool!=metro.architecture.elements.end(),"metro has an actual liquid brush");auto bounds=al::polyhedron(pool->brush);
 check(std::none_of(metro.navigation.nodes.begin(),metro.navigation.nodes.end(),[&](const auto& n){return inside(n.feet,bounds.min,bounds.max);}),"every planned metro navigation anchor stays on a dry platform or gallery");
 check(!metroHull.blocked((bounds.min+bounds.max)*.5,(bounds.min+bounds.max)*.5,settings.player),"water contents do not create an invisible solid collision hull");
 for(const auto& node:world.navigation.nodes)check(!collision.blocked(node.feet,node.feet,settings.player),"accepted physical node clears the actual player hull");
 auto bytes=world.map;auto selected=settings;selected.dressing=.9;auto dressed=generate(selected,registry);check(world.spatial.buildings[0].low.x==dressed.spatial.buildings[0].low.x&&world.spatial.buildings[0].rooms[0].low.x==dressed.spatial.buildings[0].rooms[0].low.x,"dressing changes preserve the upstream site and room plan");
 auto origin=world.navigation.nodes[0].feet;WorldPlan floor;floor.architecture.elements.push_back({0,"floor",al::box({-100,-100,-16},{100,100,0},"ground")});floor.architecture.elements.push_back({0,"step",al::box({10,-40,0},{50,40,16},"step")});Collision steps(floor);Player player;
 auto foot=V{-20,0,1};for(int i=0;i<70;i++){auto next=steps.move(foot,foot+V{1,0,0},player);check(bool(next),"preview movement follows grounded step-up / step-down physics");foot=*next;}
 check(foot.z>=16&&foot.z<=18,"preview walk raises the camera feet onto the real stair tread");
 WorldPlan triangle;triangle.architecture.elements.push_back({0,"angled test hull",prism({{0,0,0},{100,0,0},{0,100,0}},0,64,"stone")});Collision corners(triangle);
 check(!corners.blocked({120,-15,1},{140,-15,1},player),"exact convex/box sweep does not invent collision outside an angled corner");
 check(corners.blocked({120,-15,1},{80,-10,1},player),"exact convex/box sweep catches a true angled corner intersection");
 check(!corners.supported({120,-15,65},player),"an expanded angled corner cannot provide fictitious player support");
 WorldPlan angled;angled.theme="medical";wall(angled,1,{0,0,0},{100,100,0},0,16,32,0);
 check(angled.architecture.elements.size()==1&&std::abs(al::polyhedron(angled.architecture.elements[0].brush).volume-std::sqrt(20000.)*16*32)<.01,"rotated road/wall extrusion preserves outward planes and positive volume");
 std::cout<<count<<" World Architect contracts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
