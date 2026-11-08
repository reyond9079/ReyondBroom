// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "schema.h"
namespace ftebroom::plant {
// The mesh and the cutout atlas share these profiles. In particular, a coarse
// LOD must have the same pointed apex, basal notch and lobes as a close leaf.
inline double botanicalLeafWidth(int shape,double t,const std::array<double,5>& profile,double serration=0) {
  t=std::clamp(t,0.,1.);double w=std::pow(std::max(0.,std::sin(pi*std::pow(t,.82))),.88);
  if(shape==1)w=std::pow(std::max(0.,std::sin(pi*std::pow(t,.72))),.78);
  if(shape==2||shape==7)w=std::pow(std::max(0.,std::sin(pi*std::pow(t,.88))),1.13);
  if(shape==3)w=std::pow(std::max(0.,std::sin(pi*t)),.72)*(.71+.29*std::cos((t-.08)*pi*8))*(.78+.22*t);
  if(shape==5)w=.08*std::pow(std::max(0.,std::sin(pi*t)),.3);
  if(shape==6)w=std::max(0.,curve(profile,t));
  // Fine teeth follow the edge, rather than cutting holes through the lamina.
  return w*(1-serration*.045*(.5+.5*std::sin(t*pi*46)));
}
inline std::array<double,2> botanicalPalmatePoint(int index,int lobes) {
  lobes=std::clamp(lobes,3,9);
  int n=lobes*4+2;
  if(index==0||index==n)return {.5,.015};
  if(index==1)return {.35,.105};
  if(index==n-1)return {.65,.105};
  int lobe=(index-2)/4,point=(index-2)%4;double step=2.50/(lobes-1);
  double a=-1.25+step*lobe,r;
  if(point==3){a+=step*.5;r=lobes<=5?.49+.08*std::cos(a):.34+.07*std::cos(a);}
  else {a+=(point-1)*step*.09;r=(.80+.20*std::cos(a))*(point==1?1:.90);}
  return {.5+std::sin(a)*r*.49,.10+std::cos(a)*r*.87};
}
inline bool botanicalPalmateMask(double u,double v,int lobes) {
  bool inside=false;int n=std::clamp(lobes,3,9)*4+2;
  auto previous=botanicalPalmatePoint(n-1,lobes);
  for(int i=0;i<n;i++){
    auto q=botanicalPalmatePoint(i,lobes);
    if((q[1]>v)!=(previous[1]>v))if(u<(previous[0]-q[0])*(v-q[1])/(previous[1]-q[1])+q[0])inside=!inside;
    previous=q;
  }return inside;
}
}
