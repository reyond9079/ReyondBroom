// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "textures.h"
namespace ftebroom::plant {
// Render the generated geometry, material atlas and alpha silhouette in each thumbnail.
inline Image thumbnail(const Mesh& mesh,const Textures& maps,int width=96,int height=96) {
  Image out{width,height,std::vector<unsigned char>(size_t(width)*height*4,0)};
  std::vector<double> depth(size_t(width)*height,1e30);
  V forward=unit({-.65,.72,-.25}),right=unit(cross(forward,V{0,0,1})),up=unit(cross(right,forward));
  V center=mul(add(mesh.low,mesh.high),.5);double extent=std::max(.01,length(sub(mesh.high,mesh.low)));
  double scale=std::min(width,height)*1.30/extent;
  auto project=[&](V p){V delta=sub(p,center);return V{width*.5+dot(delta,right)*scale,height*.52-dot(delta,up)*scale,dot(delta,forward)};};
  for(auto& face:mesh.faces) {
    const auto& a=mesh.vertices[face.i[0]];const auto& b=mesh.vertices[face.i[1]];const auto& c=mesh.vertices[face.i[2]];
    V x=project(a.p),y=project(b.p),z=project(c.p);
    double denominator=(y[1]-z[1])*(x[0]-z[0])+(z[0]-y[0])*(x[1]-z[1]);if(std::abs(denominator)<1e-8)continue;
    int lowX=std::max(0,int(std::floor(std::min({x[0],y[0],z[0]})))),highX=std::min(width-1,int(std::ceil(std::max({x[0],y[0],z[0]}))));
    int lowY=std::max(0,int(std::floor(std::min({x[1],y[1],z[1]})))),highY=std::min(height-1,int(std::ceil(std::max({x[1],y[1],z[1]}))));
    for(int py=lowY;py<=highY;py++)for(int px=lowX;px<=highX;px++) {
      double u=((y[1]-z[1])*(px+.5-z[0])+(z[0]-y[0])*(py+.5-z[1]))/denominator;
      double v=((z[1]-x[1])*(px+.5-z[0])+(x[0]-z[0])*(py+.5-z[1]))/denominator,w=1-u-v;
      if(std::min({u,v,w})<0)continue;double distance=u*x[2]+v*y[2]+w*z[2];size_t i=py*width+px;if(distance>=depth[i])continue;
      double tu=a.uv[0]*u+b.uv[0]*v+c.uv[0]*w,tv=a.uv[1]*u+b.uv[1]*v+c.uv[1]*w;
      int sx=std::clamp(int(tu*(maps.albedo.width-1)),0,maps.albedo.width-1),sy=std::clamp(int(tv*(maps.albedo.height-1)),0,maps.albedo.height-1);
      size_t sample=(sy*maps.albedo.width+sx)*4;if(maps.albedo.rgba[sample+3]<128)continue;
      V normal=unit(add(add(mul(a.n,u),mul(b.n,v)),mul(c.n,w)));double shade=.38+.62*std::max(0.,dot(normal,unit({-.4,-.6,.8})));
      for(int channel=0;channel<3;channel++)out.rgba[i*4+channel]=unsigned(maps.albedo.rgba[sample+channel]*shade);
      out.rgba[i*4+3]=255;depth[i]=distance;
    }
  }return out;
}
}
