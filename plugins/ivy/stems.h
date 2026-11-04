// SPDX-License-Identifier: MIT
#pragma once
#include "growth.h"
namespace ivy {
struct StemFrame{V u,v,t;double radius=0,distance=0;int children=0;};
inline std::vector<StemFrame> stemFrames(const std::vector<Node>& nodes,double thickness){
 std::vector<StemFrame> frames(nodes.size());std::vector<int> continuation(nodes.size(),-1);std::vector<double> remaining(nodes.size(),0);
 for(int i=int(nodes.size())-1;i>=0;i--){int p=nodes[i].parent;if(p<0)continue;frames[p].children++;double reach=remaining[i]+length(nodes[i].p-nodes[p].p);if(reach>remaining[p]){remaining[p]=reach;continuation[p]=i;}}
 for(size_t i=0;i<nodes.size();i++){int p=nodes[i].parent,c=continuation[i];V incoming=p>=0?unit(nodes[i].p-nodes[p].p):V{0,0,1},outgoing=c>=0?unit(nodes[c].p-nodes[i].p):incoming;
  auto& f=frames[i];f.t=unit(incoming+outgoing);if(p<0)f.t=outgoing;
  V u=p>=0?frames[p].u:cross(f.t,nodes[i].normal);u=u-f.t*dot(u,f.t);if(length(u)<.01)u=cross(f.t,std::abs(f.t.z)>.8?V{1,0,0}:V{0,0,1});f.u=unit(u);f.v=unit(cross(f.t,f.u));
  f.distance=p>=0?frames[p].distance+length(nodes[i].p-nodes[p].p):0;
  f.radius=std::max(.45,thickness*(.4+.6*std::sqrt(remaining[i]/std::max(1.,remaining[i]+f.distance))));
 }
 return frames;
}
inline std::vector<V> stem(const std::vector<Node>& nodes,const std::vector<StemFrame>& frames,int index){
 int parent=nodes[index].parent;std::vector<V> result;if(parent<0)return result;
 for(int node:{parent,index})for(int i=0;i<6;i++){double angle=i*6.283185307179586/6;auto f=frames[node];result.push_back(nodes[node].p+(f.u*std::cos(angle)+f.v*std::sin(angle))*f.radius);}return result;
}
}
