// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_model.h"
#include <unordered_map>
#include <atomic>
namespace al {
struct Range {int begin=0,end=0;};
struct State {uint64_t hash=0;std::string blockUid,bigBlockUid;int variant=0;Int3 bigBlockCell;std::vector<BlockAction> actions,groupActions;int group=0,weightGroup=0;SideIds baseIds;uint8_t fill=0;float weight=1;LayerSettings layer;std::array<uint64_t,6> sideKey{};bool isFiller=false,walkable=true;uint8_t walkableSides=63;};
struct RepoRuntime {Repo source;uint64_t repoHash=0;std::vector<State> states;std::vector<std::string> groups,weightGroups,warnings;std::vector<Range> layerRange;std::vector<std::vector<Range>> groupRange;std::array<std::vector<std::vector<int>>,6> conn;std::vector<std::vector<int>> upper,blocksPerWeightGroup;std::array<std::vector<std::vector<uint16_t>>,6> groupCounter;std::unordered_map<uint64_t,int> hashes;int byHash(uint64_t h)const{auto i=hashes.find(h);return i==hashes.end()?-1:i->second;}std::vector<int> upperOf(int i)const;int builtin(int layer,int group)const;bool compatible(int a,int d,int b)const;};
class IDGenerator {std::set<Id> used;Id nextId=1;public:explicit IDGenerator(const Repo&);explicit IDGenerator(std::set<Id> a):used(std::move(a)){}Id next(){while(used.count(nextId)){if(nextId==UINT32_MAX)throw std::runtime_error("side ID space exhausted");nextId++;}used.insert(nextId);return nextId++;}};
void makeConnection(Repo&,BlockSide,BlockSide);void toggleConnection(std::vector<Connection>&,Connection);void applyConnectionsToVariants(Repo&,const std::string&,bool exclusive);
RepoRuntime compileRepo(Repo,std::atomic<bool>* cancel=nullptr);
void integrity(Builder&,const RepoRuntime&,const std::vector<std::string>& previousGroups={});
}
