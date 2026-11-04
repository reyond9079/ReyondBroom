// SPDX-License-Identifier: MIT
#pragma once
#include "growth.h"
#include <map>
namespace ivy {
// Scan the actual face triangles, not its enclosing AABB. Each descending
// tendril joins the runner along the upper boundary of its own connected face.
inline double cover(Growth& growth,const Settings& settings,double spacing){
 std::map<int,std::vector<Triangle>> faces;for(auto t:growth.surface.triangles)faces[t.face].push_back(t);
 struct Face{std::vector<Triangle> triangles;V n,u,v,origin;double lo=1e30,hi=-1e30,bottom=1e30,top=-1e30;};
 std::vector<Face> targets;
 for(auto& entry:faces){Face f;f.triangles=entry.second;f.n=f.triangles.front().normal;f.origin=f.triangles.front().a;
  f.v=V{0,0,1}-f.n*f.n.z;if(length(f.v)<.01)f.v=V{0,1,0};f.v=unit(f.v);f.u=unit(cross(f.v,f.n));
  for(auto t:f.triangles)for(auto p:{t.a,t.b,t.c}){auto q=p-f.origin;double x=dot(q,f.u),y=dot(q,f.v);f.lo=std::min(f.lo,x);f.hi=std::max(f.hi,x);f.bottom=std::min(f.bottom,y);f.top=std::max(f.top,y);}targets.push_back(f);
 }
 double step=settings.step;
 auto estimate=[&]{double count=0;for(auto& f:targets)count+=(2+std::ceil((f.hi-f.lo)/spacing))*(4+std::ceil((f.top-f.bottom)/step))+2*std::ceil((f.hi-f.lo)/step);return count;};
 for(int i=0;estimate()>settings.maxNodes&&i<50;i++){spacing*=1.12;step*=1.08;}
 if(estimate()>settings.maxNodes)throw std::runtime_error("Too many separate faces for the coverage budget. Select fewer faces.");
 growth.settings=settings;growth.nodes.clear();growth.shoots.clear();growth.iteration=settings.iterations;
 auto add=[&](V p,V n,int parent){if(growth.nodes.size()>=size_t(settings.maxNodes))throw std::runtime_error("Face coverage exceeds the node budget.");int depth=parent<0?0:growth.nodes[parent].depth+1;growth.nodes.push_back({p,n,parent,depth});return int(growth.nodes.size())-1;};
 auto connect=[&](V p,V n,int parent){if(parent>=0){V a=growth.nodes[parent].p;int count=std::max(1,int(std::ceil(length(p-a)/step)));for(int j=1;j<count;j++)parent=add(a+(p-a)*(double(j)/count),n,parent);}return add(p,n,parent);};
 for(auto& f:targets){
  int columns=std::max(1,int(std::ceil((f.hi-f.lo)/spacing))),runner=-1;
  for(int column=0;column<=columns;column++){
   double margin=std::min(spacing*.12,(f.hi-f.lo)*.03),x=f.lo+margin+(f.hi-f.lo-2*margin)*column/columns;
   std::vector<std::pair<double,double>> intervals;
   for(auto t:f.triangles){std::vector<double> hits;V p[3]={t.a,t.b,t.c};for(int e=0;e<3;e++){V a=p[e]-f.origin,b=p[(e+1)%3]-f.origin;double ax=dot(a,f.u),bx=dot(b,f.u);if(std::abs(ax-bx)<1e-8){if(std::abs(x-ax)<1e-6){hits.push_back(dot(a,f.v));hits.push_back(dot(b,f.v));}}else if(x>=std::min(ax,bx)-1e-7&&x<=std::max(ax,bx)+1e-7)hits.push_back(dot(a,f.v)+(dot(b,f.v)-dot(a,f.v))*(x-ax)/(bx-ax));}if(hits.size()>=2){auto mm=std::minmax_element(hits.begin(),hits.end());intervals.push_back({*mm.first,*mm.second});}}
   std::sort(intervals.begin(),intervals.end());std::vector<std::pair<double,double>> merged;for(auto interval:intervals){if(!merged.empty()&&interval.first<=merged.back().second+1e-5)merged.back().second=std::max(merged.back().second,interval.second);else merged.push_back(interval);}
   for(auto interval:merged){
    double top=interval.second,bottom=interval.first;V start=f.origin+f.u*x+f.v*top+f.n*settings.offset;
    // Do not bridge a gap to an unrelated/disconnected island of a face.
    int parent=runner;if(parent>=0&&length(start-growth.nodes[parent].p)>spacing*2.5)parent=-1;
    runner=connect(start,f.n,parent);parent=runner;
    int count=std::max(1,int(std::ceil((top-bottom)/step)));
    for(int j=1;j<=count;j++){double y=top+(bottom-top)*j/count;double wave=std::sin(y*.035+column*2.3+settings.seed)*spacing*.13;
     V p=f.origin+f.u*(x+wave)+f.v*y;V near;double best=1e60;for(auto t:f.triangles){V q=closest(p,t);double d=dot(q-p,q-p);if(d<best){best=d;near=q;}}parent=add(near+f.n*settings.offset,f.n,parent);
    }
   }
  }
 }
 return spacing;
}
}
