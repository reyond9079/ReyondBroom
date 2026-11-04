// SPDX-License-Identifier: MIT
#pragma once
#include "growth.h"
#include <array>
#include <iomanip>
#include <map>
namespace ivy {
// Lobed, asymmetric silhouette; geometry provides the cutout even in Quake MDL.
static constexpr double outline[][2]={{0,1},{-.14,.62},{-.58,.68},{-.46,.25},{-.68,-.05},{-.22,-.24},{0,-.7},{.22,-.24},{.68,-.05},{.46,.25},{.58,.68},{.14,.62}};
struct ModelVertex {rbgen::V p,n;double u,v;};
struct ModelSurface {std::vector<ModelVertex> vertices;std::vector<std::array<unsigned,3>> triangles;};
struct Model {
 std::array<ModelSurface,2> surfaces;
 size_t vertices()const{return surfaces[0].vertices.size()+surfaces[1].vertices.size();}
 void append(const std::vector<rbgen::V>& p,bool leaf,bool capStart=true,bool capEnd=true,double vStart=0,double vEnd=1){
  using namespace rbgen;auto& s=surfaces[leaf?1:0];const unsigned base=s.vertices.size(),half=p.size()/2;V center{};for(V v:p)center=center+v;center=center/double(p.size());
  V first{},last{};for(unsigned i=0;i<half;i++){first=first+p[i];last=last+p[half+i];}const V axis=unit(last-first);
  for(unsigned i=0;i<p.size();i++){
   const unsigned ring=i%half;V n=leaf?axis*(i<half?-1:1):unit(p[i]-(center+axis*dot(p[i]-center,axis)));
   double u=double(ring)/half,v=i<half?vStart:vEnd;
   if(leaf){u=.5+outline[ring][0]/1.5;v=1-(outline[ring][1]+.75)/1.8;}
   s.vertices.push_back({p[i],n,u,v});
  }
  auto triangle=[&](unsigned a,unsigned b,unsigned c,V normal){if(dot(cross(s.vertices[base+b].p-s.vertices[base+a].p,s.vertices[base+c].p-s.vertices[base+a].p),normal)<0)std::swap(b,c);s.triangles.push_back({base+a,base+b,base+c});};
  for(unsigned end=0;end<2;end++){
   if(!leaf&&!(end?capEnd:capStart))continue;
   V c{};for(unsigned i=0;i<half;i++)c=c+p[end*half+i];c=c/double(half);
   const unsigned index=s.vertices.size()-base;V normal=axis*(end?1:-1);s.vertices.push_back({c,normal,.5,.5});
   for(unsigned i=0;i<half;i++)triangle(index,end*half+i,end*half+(i+1)%half,normal);
  }
  if(!leaf){unsigned seam=s.vertices.size()-base;auto a=s.vertices[base],b=s.vertices[base+half];a.u=b.u=1;s.vertices.push_back(a);s.vertices.push_back(b);
   for(unsigned i=0;i<half;i++){unsigned j=(i+1)%half,ja=j?j:seam,jb=j?j+half:seam+1;V n=unit((p[i]+p[j]+p[i+half]+p[j+half])*.25-center);triangle(i,ja,jb,n);triangle(i,jb,i+half,n);}
  }
 }
 void weldStems(){
  auto& s=surfaces[0];std::map<std::array<long long,5>,unsigned> lookup;std::vector<ModelVertex> vertices;std::vector<unsigned> indices;
  for(auto v:s.vertices){std::array<long long,5> key{std::llround(v.p.x*1e6),std::llround(v.p.y*1e6),std::llround(v.p.z*1e6),std::llround(v.u*1e6),std::llround(v.v*1e6)};auto found=lookup.find(key);
   if(found==lookup.end()){lookup[key]=unsigned(vertices.size());indices.push_back(vertices.size());vertices.push_back(v);}else{indices.push_back(found->second);vertices[found->second].n=unit(vertices[found->second].n+v.n);}}
  for(auto& t:s.triangles)for(auto& i:t)i=indices[i];s.vertices=std::move(vertices);
 }
 std::string snapshot(rbgen::V origin,const std::string& bark,const std::string& leaf)const{
  std::ostringstream out;out.imbue(std::locale::classic());size_t count=0;for(auto& s:surfaces)if(!s.vertices.empty())count++;
  out<<std::setprecision(9)<<"FTEBROOM_MESH 1 1 "<<count<<'\n';
  for(int i=0;i<2;i++){auto& s=surfaces[i];if(s.vertices.empty())continue;out<<std::quoted(i?leaf:bark)<<' '<<s.vertices.size()<<' '<<s.triangles.size()<<'\n';
   for(auto v:s.vertices){auto p=v.p-origin;out<<p.x<<' '<<p.y<<' '<<p.z<<' '<<v.n.x<<' '<<v.n.y<<' '<<v.n.z<<' '<<v.u<<' '<<v.v<<'\n';}
   for(auto t:s.triangles)out<<t[0]<<' '<<t[1]<<' '<<t[2]<<'\n';
  }return out.str();
 }
};
inline double hash(int x,int y){uint32_t n=uint32_t(x)*374761393u+uint32_t(y)*668265263u;n=(n^(n>>13))*1274126177u;return (n^(n>>16))/4294967295.;}
inline double noise(double x,double y){int ix=int(std::floor(x)),iy=int(std::floor(y));x-=ix;y-=iy;x=x*x*(3-2*x);y=y*y*(3-2*y);return (hash(ix,iy)*(1-x)+hash(ix+1,iy)*x)*(1-y)+(hash(ix,iy+1)*(1-x)+hash(ix+1,iy+1)*x)*y;}
inline std::string texture(bool leaf){
 const int size=256;std::string image(18+size*size*4,'\0');image[2]=2;image[13]=1;image[15]=1;image[16]=32;image[17]=0x28;
 for(int y=0;y<size;y++)for(int x=0;x<size;x++){
  double u=(x+.5)/size,v=(y+.5)/size,grain=noise(x*.44,y*.44),detail=noise(x*1.7,y*1.7),r,g,b;
  if(leaf){
   double across=(u-.5)*1.5,along=(1-v)*1.8-.75;
   double mottling=noise(u*9,v*8)*.55+noise(u*23,v*23)*.25+grain*.2;
   double main=std::exp(-std::pow((across+.012*std::sin(along*5))/.012,2.));
   double vein=0;for(int i=0;i<6;i++){double line=along-(-.42+i*.22+std::abs(across)*(.65+i*.08));vein=std::max(vein,std::exp(-line*line/.00022)*std::exp(-std::abs(across)*.8));}
   double tiny=std::pow(std::max(0.,std::cos(across*126+along*64+noise(u*12,v*18)*8)),14.);
   double lighting=.76+.22*std::cos(across*2.5)+.16*noise(u*4,v*5);
   r=(48+23*mottling+15*main+9*vein+2*tiny)*lighting;
   g=(64+32*mottling+18*main+10*vein+3*tiny)*lighting;
   b=(39+21*mottling+10*main+7*vein)*lighting;
  }else{
   // Periodic vertical bark ridges, wandering cracks and multi-scale pores.
   double angle=u*6.28318530718;
   auto periodic=[&](double frequency,double stretch){return noise(20+std::cos(angle)*frequency+v*stretch,20+std::sin(angle)*frequency+v*stretch*.37);};
   double warp=periodic(3,1.2)*.65+periodic(9,4)*.2;
   double ridge=.5+.5*std::sin(angle*23+warp*8+std::sin(v*12+angle)*.2);
   double crack=std::pow(1-ridge,13)*(.35+.65*periodic(7,18));
   double fiber=periodic(26,3),patch=periodic(6,9),pores=std::pow(1-detail,7);
   double light=.65+.2*ridge+.12*grain+.12*fiber-.34*crack-.2*pores;
   r=(84+19*patch)*light;g=(77+16*patch)*light;b=(64+16*patch)*light;
  }
  size_t at=18+(y*size+x)*4;image[at]=char(std::clamp(int(b),0,255));image[at+1]=char(std::clamp(int(g),0,255));image[at+2]=char(std::clamp(int(r),0,255));image[at+3]=char(255);
 }return image;
}
}
