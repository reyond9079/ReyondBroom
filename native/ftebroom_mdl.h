// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_quake1.h"
#include <array>
#include <cstring>
namespace ftebroom {
struct MdlVertex{std::array<float,3> point;float u,v;};
struct MdlPreview{quake1::Texture skin;std::vector<std::array<MdlVertex,3>> triangles;};
inline MdlPreview readMdl(const std::string& bytes,int skinIndex=0,unsigned frameIndex=0){
  using namespace quake1;range(bytes,0,84);if(bytes.substr(0,4)!="IDPO"||word(bytes,4)!=6)throw std::runtime_error("Unsupported model thumbnail format.");
  auto real=[&](size_t at){auto bits=word(bytes,at);float f;std::memcpy(&f,&bits,4);if(!std::isfinite(f))throw std::runtime_error("Invalid model scale.");return f;};
  float scale[3],origin[3];for(int i=0;i<3;i++){scale[i]=real(8+i*4);origin[i]=real(20+i*4);}
  auto skins=word(bytes,48),w=word(bytes,52),h=word(bytes,56),verts=word(bytes,60),tris=word(bytes,64),frames=word(bytes,68);
  if(!skins||skins>256||!w||!h||w>4096||h>4096||!verts||verts>65536||!tris||tris>131072||!frames)throw std::runtime_error("Invalid model dimensions.");
  size_t at=84,pixels=size_t(w)*h;MdlPreview result;result.skin={word(bytes,76)&(1u<<14)?"{preview":"preview",w,h,{}};skinIndex=std::clamp(skinIndex,0,int(skins)-1);
  for(unsigned s=0;s<skins;s++){auto group=word(bytes,at);at+=4;unsigned count=1;if(group){count=word(bytes,at);at+=4;if(!count||count>1024)throw std::runtime_error("Invalid skin group.");range(bytes,at,count*4);at+=count*4;}range(bytes,at,pixels*count);if(s==unsigned(skinIndex))result.skin.pixels.assign(bytes.begin()+at,bytes.begin()+at+pixels);at+=pixels*count;}
  size_t uv=at;range(bytes,uv,size_t(verts)*12);at+=size_t(verts)*12;size_t triangles=at;range(bytes,at,size_t(tris)*16);at+=size_t(tris)*16;
  frameIndex=std::min(frameIndex,frames-1);
  const size_t frameSize=24+size_t(verts)*4;
  for(unsigned frame=0;frame<=frameIndex;++frame){
    const auto group=word(bytes,at);at+=4;
    unsigned count=1;
    if(group){
      count=word(bytes,at);
      if(!count||count>65536)throw std::runtime_error("Invalid frame group.");
      range(bytes,at,12+count*4);at+=12+count*4;
    }
    if(frame==frameIndex){range(bytes,at,frameSize);at+=24;break;}
    range(bytes,at,frameSize*count);at+=frameSize*count;
  }
  for(unsigned t=0;t<tris;t++){std::array<MdlVertex,3> triangle;bool front=word(bytes,triangles+t*16)!=0;for(int j=0;j<3;j++){auto v=word(bytes,triangles+t*16+4+j*4);if(v>=verts)throw std::runtime_error("Invalid model vertex index.");auto& out=triangle[j];for(int k=0;k<3;k++)out.point[k]=(unsigned char)bytes[at+v*4+k]*scale[k]+origin[k];float u=float(int32_t(word(bytes,uv+v*12+4)));if(!front&&word(bytes,uv+v*12))u+=w*.5f;out.u=(u+.5f)/w;out.v=(float(int32_t(word(bytes,uv+v*12+8)))+.5f)/h;}result.triangles.push_back(triangle);}
  return result;
}
}
