// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_repo_compile.h"
#include <memory>
#include <chrono>
namespace al {
struct BoundaryValue {enum Kind{Nothing,Block,Group} kind=Nothing;uint64_t hash=0;uint32_t mask=INT32_MAX;};
struct ILevelBoundary {virtual ~ILevelBoundary()=default;virtual BoundaryValue evaluate(Int3 world,int layer)const=0;};
struct GroupsBoundary:ILevelBoundary {uint32_t mask;explicit GroupsBoundary(uint32_t m):mask(m){}BoundaryValue evaluate(Int3,int l)const override{return l==0?BoundaryValue{BoundaryValue::Group,0,mask}:BoundaryValue{};}};
struct BlockBoundary:ILevelBoundary {uint64_t hash;explicit BlockBoundary(uint64_t h):hash(h){}BoundaryValue evaluate(Int3,int)const override{return {BoundaryValue::Block,hash,0};}};
struct LevelBoundary:ILevelBoundary {const Builder* other;uint32_t fallback;LevelBoundary(const Builder* b,uint32_t mask):other(b),fallback(mask){}BoundaryValue evaluate(Int3 world,int l)const override{auto p=world-other->levelData.position;if(inside(p,other->levelData.size)){auto i=index(p,other->levelData.size);if(size_t(l)<other->levelData.layers.size()&&i<other->levelData.layers[l].blocks.size()&&other->levelData.layers[l].blocks[i])return {BoundaryValue::Block,other->levelData.layers[l].blocks[i],0};if(l==0&&i<other->inputWave.size())return {BoundaryValue::Group,0,uint32_t(other->inputWave[i].groups)};}return l==0&&fallback?BoundaryValue{BoundaryValue::Group,0,fallback}:BoundaryValue{};}};
struct SolveRequest {const RepoRuntime* repo=nullptr;LevelData* level=nullptr;const std::vector<InputWaveCell>* inputWave=nullptr;std::array<std::shared_ptr<ILevelBoundary>,6> boundaries;std::vector<float> weightOverride;std::map<std::string,float> moduleWeightMultipliers;Bounds3i bounds;int layer=0,iterations=10;uint32_t seed=0;CellPick cellPick=CellPick::RandomOrder;std::atomic<bool>* cancel=nullptr;std::atomic<float>* progress=nullptr;bool multiThreaded=false,keepLargestComponent=false,requireAllFloors=false;float minCoverage=0;};
struct Quality {double coverage=0;int floors=0,walkable=0,verticalLinks=0,halls=0,atria=0,corridors=0;std::vector<size_t> component;};
struct SolveResult {int iterationsUsed=0;uint32_t seedUsed=0;double ms=0;Int3 failCell;std::string failStage,message;int backtracks=0;bool accepted=false;Quality quality;};
Quality quality(const RepoRuntime&,const LevelData&);bool validAdjacency(const RepoRuntime&,const LevelData&,int layer);
SolveResult solve(SolveRequest&);
}
