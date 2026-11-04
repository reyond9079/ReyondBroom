// SPDX-License-Identifier: MIT
// Independent surface-following growth, using directional inertia, light seeking,
// random exploration, adhesion and gravity. No Cinema 4D implementation is used.
#pragma once
#include "../common/geometry.h"
#include <numeric>
#include <random>
namespace ivy {
using namespace rbgen;
struct Triangle{V a,b,c,normal;int face=0;};
inline V closest(V p,const Triangle& t){
 V ab=t.b-t.a,ac=t.c-t.a,ap=p-t.a;double d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return t.a;
 V bp=p-t.b;double d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return t.b;
 double vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return t.a+ab*(d1/(d1-d3));
 V cp=p-t.c;double d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return t.c;
 double vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return t.a+ac*(d2/(d2-d6));
 double va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return t.b+(t.c-t.b)*((d4-d3)/((d4-d3)+(d5-d6)));
 double den=1/(va+vb+vc);return t.a+ab*(vb*den)+ac*(vc*den);
}
struct Surface {
 struct Node{V lo,hi;int begin,end,left=-1,right=-1;};
 std::vector<Triangle> triangles;std::vector<int> order;std::vector<Node> nodes;
 int buildNode(int begin,int end){
  Node node;node.begin=begin;node.end=end;node.lo={1e30,1e30,1e30};node.hi={-1e30,-1e30,-1e30};
  for(int i=begin;i<end;i++){auto t=triangles[order[i]];for(V p:{t.a,t.b,t.c})for(int a=0;a<3;a++){node.lo[a]=std::min(node.lo[a],p[a]);node.hi[a]=std::max(node.hi[a],p[a]);}}
  int index=int(nodes.size());nodes.push_back(node);if(end-begin>8){int axis=0;for(int a=1;a<3;a++)if(node.hi[a]-node.lo[a]>node.hi[axis]-node.lo[axis])axis=a;int mid=(begin+end)/2;
   std::nth_element(order.begin()+begin,order.begin()+mid,order.begin()+end,[&](int a,int b){auto x=triangles[a],y=triangles[b];return (x.a[axis]+x.b[axis]+x.c[axis])<(y.a[axis]+y.b[axis]+y.c[axis]);});
   int l=buildNode(begin,mid),rr=buildNode(mid,end);nodes[index].left=l;nodes[index].right=rr;
  }return index;
 }
 void build(){order.resize(triangles.size());std::iota(order.begin(),order.end(),0);nodes.clear();if(!order.empty())buildNode(0,int(order.size()));}
 double nearest(V p,V& point,V& normal)const{
  if(nodes.empty())return 1e30;double best=1e60;std::vector<int> stack{0};
  while(!stack.empty()){auto n=nodes[stack.back()];stack.pop_back();double distance=0;for(int a=0;a<3;a++){double v=p[a]-std::clamp(p[a],n.lo[a],n.hi[a]);distance+=v*v;}if(distance>best)continue;
   if(n.left>=0){stack.push_back(n.left);stack.push_back(n.right);continue;}
   for(int i=n.begin;i<n.end;i++){auto t=triangles[order[i]];V q=closest(p,t);double d=dot(q-p,q-p);if(d<best){best=d;point=q;normal=t.normal;}}
  }return std::sqrt(best);
 }
};
struct Settings{int seed=1337,iterations=180,maxBranches=16,maxNodes=1800;double step=8,primary=.65,random=.3,adhesion=.85,gravity=.12,up=.35,branchProbability=.035,maxFloat=96,offset=3;};
struct Node{V p,normal;int parent=-1,depth=0;};
struct Shoot{int node,age=0;V direction;double floating=0;bool alive=true;};
struct Growth{
 Settings settings;Surface surface;std::vector<Node> nodes;std::vector<Shoot> shoots;std::mt19937 rng;int iteration=0;
 double random(){return double(rng())/4294967296.;}
 V randomVector(){return unit({random()*2-1,random()*2-1,random()*2-1});}
 void start(V root,const Settings& s){settings=s;rng.seed(s.seed);iteration=0;nodes.clear();shoots.clear();V p,n;if(surface.nearest(root,p,n)>1e20)throw std::runtime_error("No target surface.");nodes.push_back({p+n*s.offset,n,-1,0});shoots.push_back({0,0,{0,0,s.up<0?-1.:1.},0,true});}
 bool advance(){
  if(iteration++>=settings.iterations||int(nodes.size())>=settings.maxNodes)return false;
  int alive=0,count=int(shoots.size());
  for(int i=0;i<count;i++){
   Shoot& shoot=shoots[i];if(!shoot.alive)continue;auto previous=nodes[shoot.node];V nearest,normal;double distance=surface.nearest(previous.p,nearest,normal);
   V attraction=nearest+normal*settings.offset-previous.p;
   V direction=shoot.direction*settings.primary+randomVector()*settings.random+V{0,0,settings.up-settings.gravity*std::min(4.,shoot.floating/settings.step)};
   if(distance<settings.maxFloat)direction=direction+unit(attraction)*(settings.adhesion*std::min(1.,length(attraction)/settings.step));
   direction=unit(direction);V p=previous.p+direction*settings.step;
   double nextDistance=surface.nearest(p,nearest,normal);
   // Push out of a crossed surface and follow its tangent without tunnelling.
   double signedDistance=dot(p-nearest,normal);
   if(nextDistance<settings.step*1.75&&signedDistance<settings.offset){p=nearest+normal*settings.offset;nextDistance=settings.offset;}
   if(nextDistance>settings.offset*2)shoot.floating+=settings.step;else shoot.floating=0;
   if(shoot.floating>settings.maxFloat||length(p-previous.p)<settings.step*.08||int(nodes.size())>=settings.maxNodes){shoot.alive=false;continue;}
   int parent=shoot.node;nodes.push_back({p,normal,parent,previous.depth+1});shoot.node=int(nodes.size())-1;shoot.age++;shoot.direction=unit(p-previous.p);alive++;
   if(int(shoots.size())<settings.maxBranches&&shoot.age>4&&random()<settings.branchProbability){V branch=unit(shoot.direction*.3+randomVector()*.7+V{0,0,.2});shoots.push_back({shoot.node,0,branch,0,true});}
  }return alive>0;
 }
};
}
