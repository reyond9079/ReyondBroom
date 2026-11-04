// SPDX-License-Identifier: MIT
#include "../plugins/ivy/growth.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace ivy;Surface s;s.triangles={{{-500,0,0},{500,0,0},{500,0,3000},{0,-1,0}},{{-500,0,0},{500,0,3000},{-500,0,3000},{0,-1,0}}};s.build();
 V p,n;assert(std::abs(s.nearest({3,-7,200},p,n)-7)<1e-8);assert(std::abs(p.y)<1e-8);
 Settings settings;settings.iterations=120;settings.maxBranches=8;settings.branchProbability=.08;Growth a,b;a.surface=s;b.surface=s;a.start({0,-3,10},settings);b.start({0,-3,10},settings);
 while(a.advance()){}while(b.advance()){}assert(a.nodes.size()>80&&a.nodes.size()==b.nodes.size());bool rose=false,branched=a.shoots.size()>1;
 for(size_t i=0;i<a.nodes.size();i++){auto q=a.nodes[i];assert(length(q.p-b.nodes[i].p)<1e-10);assert(std::isfinite(q.p.x)&&std::isfinite(q.p.y)&&std::isfinite(q.p.z));assert(q.p.y<=-2.9);rose|=q.p.z>200;assert(q.parent<int(i));}
 assert(rose&&branched);assert(a.nodes.size()<=size_t(settings.maxNodes));
 auto box=rbgen::box({-16,-32,0},{16,32,64},"fb_grey");assert(std::count(box.begin(),box.end(),'(')==18);
 std::cout<<"PASS surface BVH, deterministic branched growth, wall collision, upward growth, budgets and convex brush planes: "<<a.nodes.size()<<" nodes\n";
}
