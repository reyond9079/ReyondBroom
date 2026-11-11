// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_mdl.h"
namespace ftebroom {
struct Md3Surface {std::string name,shader;std::vector<std::array<MdlVertex,3>> triangles;};
inline std::vector<Md3Surface> readMd3(const std::string& data,unsigned frameIndex=0) {
 using namespace quake1;range(data,0,108);
 if(data.substr(0,4)!="IDP3" || word(data,4)!=15)throw std::runtime_error("Unsupported MD3 version.");
 auto count=word(data,84),end=word(data,104);size_t at=word(data,100);
 if(!count||count>1024||end>data.size()||at<108)throw std::runtime_error("Invalid MD3 header.");
 auto real=[&](size_t p){auto b=word(data,p);float f;std::memcpy(&f,&b,4);if(!std::isfinite(f))throw std::runtime_error("Invalid MD3 UV.");return f;};
 auto name=[&](size_t p){range(data,p,64);size_t n=0;while(n<64&&data[p+n])n++;return data.substr(p,n);};
 std::vector<Md3Surface> result;
 for(unsigned s=0;s<count;s++) {
  range(data,at,108);if(data.compare(at,4,"IDP3"))throw std::runtime_error("Invalid MD3 surface.");
  auto frames=word(data,at+72),shaders=word(data,at+76),verts=word(data,at+80),tris=word(data,at+84),length=word(data,at+104);
  if(!frames||!verts||verts>65536||tris>131072||shaders>4096||length<108||at+length>end)throw std::runtime_error("Invalid MD3 surface bounds.");
  auto section=[&](size_t field,size_t bytes){size_t p=word(data,at+field);if(p<108||p>length||bytes>length-p)throw std::runtime_error("Invalid MD3 section.");return at+p;};
  auto indices=section(88,size_t(tris)*12),skin=section(92,size_t(shaders)*68),uv=section(96,size_t(verts)*8),xyz=section(100,size_t(verts)*8*frames);
  xyz+=size_t(std::min(frameIndex,frames-1))*verts*8;
  Md3Surface surface;surface.name=name(at+4);if(shaders)surface.shader=name(skin);
  for(unsigned t=0;t<tris;t++){std::array<MdlVertex,3> triangle;for(int j=0;j<3;j++){auto index=word(data,indices+t*12+j*4);if(index>=verts)throw std::runtime_error("Invalid MD3 triangle.");auto& v=triangle[j];for(int a=0;a<3;a++){size_t p=xyz+index*8+a*2;v.point[a]=int16_t(uint16_t((unsigned char)data[p])|(uint16_t((unsigned char)data[p+1])<<8))/64.f;}v.u=real(uv+index*8);v.v=real(uv+index*8+4);}surface.triangles.push_back(triangle);}
  result.push_back(std::move(surface));at+=length;
 }
 return result;
}
}
