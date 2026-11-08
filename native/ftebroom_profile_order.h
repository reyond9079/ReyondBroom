// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cctype>
#include <numeric>
#include <string>
#include <vector>
namespace ftebroom {
// Track selection by original index: names can be empty or duplicated.
template<class T> bool reorderProfiles(std::vector<T>& profiles,int& selected,const std::string& operation){
 std::vector<size_t> order(profiles.size());std::iota(order.begin(),order.end(),size_t(0));
 if(operation=="sort"){
  auto key=[](std::string name){for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));return name;};
  std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return key(profiles[a].name)<key(profiles[b].name);});
 }else if(selected>=0&&selected<int(order.size())){
  int target=selected+(operation=="up"?-1:operation=="down"?1:0);
  if(target<0||target>=int(order.size())||target==selected)return false;
  std::swap(order[selected],order[target]);
 }else return false;
 bool changed=false;for(size_t i=0;i<order.size();++i)changed|=i!=order[i];if(!changed)return false;
 std::vector<T> sorted;sorted.reserve(profiles.size());int next=-1;
 for(size_t i=0;i<order.size();++i){if(int(order[i])==selected)next=int(i);sorted.push_back(std::move(profiles[order[i]]));}
 profiles=std::move(sorted);selected=next;return true;
}
}
