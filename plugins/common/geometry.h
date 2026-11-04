// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace rbgen {
struct V {double x=0,y=0,z=0;double& operator[](int a){return a==0?x:a==1?y:z;}double operator[](int a)const{return a==0?x:a==1?y:z;}};
inline V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}inline V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V operator*(V a,double t){return {a.x*t,a.y*t,a.z*t};}inline V operator/(V a,double t){return a*(1/t);}
inline double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}inline V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double length(V a){return std::sqrt(dot(a,a));}inline V unit(V a){double n=length(a);return n>1e-9?a/n:V{0,0,1};}
inline std::string material(std::string s){if(s.rfind("textures/",0)==0)s.erase(0,9);if(s.size()>255)throw std::runtime_error("Material name is too long.");for(unsigned char c:s)if(c<32||c==127)throw std::runtime_error("Invalid material name.");return s.empty()?"fb_grey":s;}
inline std::string point(V p){std::ostringstream o;o<<std::fixed<<std::setprecision(5)<<"( "<<p.x<<' '<<p.y<<' '<<p.z<<" ) ";return o.str();}
inline std::string convex(const std::vector<V>& points,const std::string& tex){
 V center{};for(auto p:points)center=center+p;center=center/double(points.size());std::vector<std::pair<V,double>> planes;std::ostringstream o;o<<"{\n";
 for(size_t a=0;a<points.size();a++)for(size_t b=a+1;b<points.size();b++)for(size_t c=b+1;c<points.size();c++){
  V n=cross(points[b]-points[a],points[c]-points[a]);if(length(n)<1e-6)continue;n=unit(n);double d=dot(n,points[a]);bool pos=false,neg=false;
  for(auto p:points){double t=dot(n,p)-d;pos|=t>1e-4;neg|=t< -1e-4;}if(pos&&neg)continue;
  V p=points[b],q=points[c];if(dot(n,center)-d<0){std::swap(p,q);n=n*-1;d=-d;}
  bool duplicate=false;for(auto plane:planes)if(dot(n,plane.first)>.999999&&std::abs(d-plane.second)<1e-4)duplicate=true;
  if(duplicate)continue;planes.push_back({n,d});o<<point(points[a])<<point(p)<<point(q)<<std::quoted(material(tex))<<" 0 0 0 1 1\n";
 }
 if(planes.size()<4||planes.size()>64)throw std::runtime_error("Generated degenerate convex geometry.");o<<"}\n";return o.str();
}
inline std::string box(V lo,V hi,const std::string& tex){
 if(hi.x-lo.x<.01||hi.y-lo.y<.01||hi.z-lo.z<.01)throw std::runtime_error("Invalid box size.");
 std::vector<V> p;for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++)p.push_back({x?hi.x:lo.x,y?hi.y:lo.y,z?hi.z:lo.z});return convex(p,tex);
}
}
