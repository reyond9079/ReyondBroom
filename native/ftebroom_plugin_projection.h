// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace rbviewport {
using V=std::array<float,3>;
inline float dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
struct Projection {V origin{},forward{},right{},up{};float x=0,y=0,w=1,h=1,fov=90,span=1024;int type=0;
 bool project(V p,float& px,float& py)const{V d{p[0]-origin[0],p[1]-origin[1],p[2]-origin[2]};float z=dot(d,forward),scale=type?std::max(w,h)/span:w*.5f/std::tan(fov*.00872664626f)/z;if(!type&&z<1)return false;px=x+w*.5f+dot(d,right)*scale;py=y+h*.5f-dot(d,up)*scale;return std::isfinite(px)&&std::isfinite(py);}
 void ray(float px,float py,V& start,V& direction)const{start=origin;if(type){float scale=span/std::max(w,h);for(int a=0;a<3;a++)start[a]+=(px-x-w*.5f)*scale*right[a]-(py-y-h*.5f)*scale*up[a]-65536*forward[a];direction=forward;}else{float scale=2*std::tan(fov*.00872664626f)/w,n=0;for(int a=0;a<3;a++){direction[a]=forward[a]+(px-x-w*.5f)*scale*right[a]-(py-y-h*.5f)*scale*up[a];n+=direction[a]*direction[a];}for(auto& v:direction)v/=std::sqrt(n);}}
};
}
