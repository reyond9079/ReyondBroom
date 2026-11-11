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
 for(const auto& node:world.navigation.nodes)check(!collision.blocked(node.feet,node.feet,settings.player),"accepted physical node clears the actual player hull");
 auto bytes=world.map;auto selected=settings;selected.dressing=.9;auto dressed=generate(selected,registry);check(world.spatial.buildings[0].low.x==dressed.spatial.buildings[0].low.x&&world.spatial.buildings[0].rooms[0].low.x==dressed.spatial.buildings[0].rooms[0].low.x,"dressing changes preserve the upstream site and room plan");
 auto origin=world.navigation.nodes[0].feet;WorldPlan floor;floor.architecture.elements.push_back({0,"floor",al::box({-100,-100,-16},{100,100,0},"ground")});floor.architecture.elements.push_back({0,"step",al::box({10,-40,0},{50,40,16},"step")});Collision steps(floor);Player player;
 auto foot=V{-20,0,1};for(int i=0;i<70;i++){auto next=steps.move(foot,foot+V{1,0,0},player);check(bool(next),"preview movement follows grounded step-up / step-down physics");foot=*next;}
 check(foot.z>=16&&foot.z<=18,"preview walk raises the camera feet onto the real stair tread");
 std::cout<<count<<" World Architect contracts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
