// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_solver.h"
namespace al {
std::vector<std::vector<size_t>> groupBuilders(const std::vector<Builder>&);
void connectBuilders(Builder& source,Builder& target,int side,bool connect=true);
SolveRequest builderRequest(const RepoRuntime&,Builder&,const std::vector<Builder>&,const EditorSettings&,bool selection,std::atomic<bool>*,std::atomic<float>*);
struct GroupResult {bool success=false;std::vector<SolveResult> results;std::vector<size_t> order;std::string message;};
GroupResult solveBuilders(std::vector<Builder>&,const std::map<std::string,std::shared_ptr<const RepoRuntime>>&,const std::vector<size_t>&,const EditorSettings&,std::atomic<bool>*,std::atomic<float>*);
}
