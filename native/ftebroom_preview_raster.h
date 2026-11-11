// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_mdl.h"
#include <limits>
namespace ftebroom::preview {
struct Pixels {int width=0,height=0;std::vector<unsigned char> rgba;};
struct Part {Pixels pixels;std::vector<std::array<MdlVertex,3>> triangles;};
// A thumbnail is rasterized once. Each following UI frame submits one quad,
// independent of the original model's triangle count and number of surfaces.
inline std::string raster(const std::vector<Part>& parts,int size=192) {
  if(size<1||size>512)throw std::runtime_error("Invalid thumbnail size.");
  std::string out(18+size_t(size)*size*4,0);out[2]=2;out[12]=size&255;out[13]=size>>8;out[14]=size&255;out[15]=size>>8;out[16]=32;out[17]=40;
  std::vector<float> depth(size_t(size)*size,-std::numeric_limits<float>::infinity());
  const Pixels neutral{1,1,{174,183,190,255}};
  for(const auto& part:parts){const auto& supplied=part.pixels;
    const auto& skin=supplied.width>0&&supplied.height>0&&supplied.rgba.size()==size_t(supplied.width)*supplied.height*4?supplied:neutral;
    for(const auto& tri:part.triangles){
      float x[3],y[3];bool valid=true;for(int i=0;i<3;i++){x[i]=tri[i].point[0]*size;y[i]=tri[i].point[1]*size;valid&=std::isfinite(x[i])&&std::isfinite(y[i])&&std::isfinite(tri[i].point[2])&&std::isfinite(tri[i].u)&&std::isfinite(tri[i].v);}if(!valid)continue;
      float area=(x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]);if(std::abs(area)<1e-6f)continue;
      int left=int(std::floor(std::clamp(std::min({x[0],x[1],x[2]}),0.f,float(size-1)))),right=int(std::ceil(std::clamp(std::max({x[0],x[1],x[2]}),0.f,float(size-1))));
      int top=int(std::floor(std::clamp(std::min({y[0],y[1],y[2]}),0.f,float(size-1)))),bottom=int(std::ceil(std::clamp(std::max({y[0],y[1],y[2]}),0.f,float(size-1))));
      for(int py=top;py<=bottom;py++)for(int px=left;px<=right;px++){
        float a=((x[1]-px-.5f)*(y[2]-py-.5f)-(y[1]-py-.5f)*(x[2]-px-.5f))/area;
        float b=((x[2]-px-.5f)*(y[0]-py-.5f)-(y[2]-py-.5f)*(x[0]-px-.5f))/area,c=1-a-b;if(a<0||b<0||c<0)continue;
        float z=a*tri[0].point[2]+b*tri[1].point[2]+c*tri[2].point[2];size_t at=size_t(py)*size+px;if(z<depth[at])continue;
        float u=a*tri[0].u+b*tri[1].u+c*tri[2].u,v=a*tri[0].v+b*tri[1].v+c*tri[2].v;
        int sx=std::min(skin.width-1,int((u-std::floor(u))*skin.width)),sy=std::min(skin.height-1,int((v-std::floor(v))*skin.height));
        const auto* src=skin.rgba.data()+(size_t(sy)*skin.width+sx)*4;if(src[3]<16)continue;
        auto* dst=reinterpret_cast<unsigned char*>(out.data())+18+at*4;
        float alpha=src[3]/255.f,old=dst[3]/255.f,combined=alpha+old*(1-alpha);
        for(int ch=0;ch<3;ch++)dst[ch]=static_cast<unsigned char>((src[2-ch]*alpha+dst[ch]*old*(1-alpha))/combined);
        dst[3]=static_cast<unsigned char>(combined*255);depth[at]=z;
      }
    }
  }return out;
}
}
