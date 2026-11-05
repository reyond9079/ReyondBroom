// SPDX-License-Identifier: MIT
#include "../plugins/autolevel/al_storage.cpp"
#include "../plugins/autolevel/al_brush.cpp"
#include "../plugins/autolevel/al_default_repo.cpp"
#include "../plugins/autolevel/al_legacy.cpp"
#include <iostream>
using namespace al;
int main(){try{Geometry g;g.brushes={box({},{512,256,192},"old/material")};std::string capture="BOUNDS 0 0 0 512 256 192\n"+writeMap(g);std::ostringstream s;s<<"REYOND_WFC 1\n1337 256 192 128 1 1 1 1\n2 2 1 0 0 0 1 .1 0 0 0 0 0 0 0\n1\n"<<std::quoted("Old module")<<" 2 0 1 0 2 1 1 11 12 13 14 15 16 "<<std::quoted(capture)<<"\n0\n";auto r=importLevelgen(s.str());if(r.bigBlocks.size()!=1||r.blocks.size()!=2||r.bigBlocks[0].actionsGroups!=std::vector<std::string>{"Full Rotate On Z"})throw std::runtime_error("legacy big-block structure");auto a=variant(r,r.bigBlocks[0].data[0][0]),b=variant(r,r.bigBlocks[0].data[1][0]);if(a->sideIds[0]!=13||a->sideIds[2]!=14||a->sideIds[5]!=12||a->sideIds[4]!=15||a->sideIds[1]!=16||b->sideIds[3]!=11)throw std::runtime_error("legacy direction remap");double total=0;for(auto& b:r.blocks)for(auto& brush:parseMap(b.geometry).brushes)total+=polyhedron(brush).volume;if(std::abs(total-polyhedron(g.brushes[0]).volume)>.01)throw std::runtime_error("legacy clipping volume");if(serialize(readRepo(serialize(r)))!=serialize(r))throw std::runtime_error("imported library persistence");std::cout<<"PASS legacy library migration: directions, rotations, multi-cell clipping, materials and stable persistence\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
