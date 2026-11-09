// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_model_export.h"
#include "ftebroom_palette_quantize.h"
#include <cstdint>
namespace ftebroom::modelExport {
struct Skin {unsigned width=0,height=0;std::vector<unsigned char> rgba;};
struct MdlSkinRegion {
  float u0,v0,du,dv;
  unsigned x,y,tile;
  std::array<int,2> pixel(float u,float v) const {
    return {int(x+1+std::round((u-u0)/du*(tile-3))),
            int(y+1+std::round((v-v0)/dv*(tile-3)))};
  }
};
// Preserve the source coordinate system. MDL quantization uses one bounding box
// across all animation frames, rather than recentering or rescaling each frame.
inline std::string mdl(const Mesh& mesh,const std::vector<Skin>& skins,const std::string& palette,bool masked=false,Skin* replacement=nullptr,std::vector<MdlSkinRegion>* regions=nullptr) {
  if(mesh.surfaces.empty()||!mesh.frames||mesh.frames>1024||skins.size()!=mesh.surfaces.size()||palette.size()!=768)
    throw std::runtime_error("MDL needs a model, all surface textures and the active Quake palette.");
  unsigned nv=0,nt=0;std::array<float,3> low{1e30f,1e30f,1e30f},high{-1e30f,-1e30f,-1e30f},scale{};
  for(const auto& s:mesh.surfaces){nv+=s.frames.at(0).size();nt+=s.triangles.size();if(s.frames.size()!=mesh.frames)throw std::runtime_error("Missing animation frames.");
    for(const auto& f:s.frames){if(f.size()!=s.frames[0].size())throw std::runtime_error("Animated topology changes.");for(const auto& v:f)for(int a=0;a<3;a++){if(!std::isfinite(v.position[a]))throw std::runtime_error("Invalid position.");low[a]=std::min(low[a],v.position[a]);high[a]=std::max(high[a],v.position[a]);}}}
  if(nv>2048||nt>4096||mesh.surfaces.size()>16)throw std::runtime_error("Quake MDL export limit: 2048 vertices, 4096 triangles and 16 materials. Simplify the source explicitly; automatic resizing/decimation is disabled.");
  const unsigned tile=masked?512:256,columns=unsigned(std::ceil(std::sqrt(double(skins.size())))),rows=(skins.size()+columns-1)/columns,w=columns*tile,h=rows*tile;
  if(replacement)*replacement={w,h,std::vector<unsigned char>(size_t(w)*h*4)};
  if(regions)regions->clear();
  std::vector<unsigned char> pixels(w*h,0);std::vector<std::array<int,2>> uv;
  ftebroom::PaletteQuantizer quantizer(palette,224);
  for(size_t i=0;i<skins.size();i++){
    const auto& image=skins[i];if(!image.width||!image.height||image.rgba.size()!=size_t(image.width)*image.height*4)throw std::runtime_error("Missing or invalid surface texture.");
    float u0=1e30f,v0=1e30f,u1=-1e30f,v1=-1e30f;
    for(const auto& v:mesh.surfaces[i].frames[0]){u0=std::min(u0,v.uv[0]);u1=std::max(u1,v.uv[0]);v0=std::min(v0,v.uv[1]);v1=std::max(v1,v.uv[1]);}
    float du=std::max(u1-u0,.000001f),dv=std::max(v1-v0,.000001f);unsigned ox=(i%columns)*tile,oy=(i/columns)*tile;
    MdlSkinRegion region{u0,v0,du,dv,ox,oy,tile};
    if(regions)regions->push_back(region);
    for(const auto& v:mesh.surfaces[i].frames[0])uv.push_back(region.pixel(v.uv[0],v.uv[1]));
    // Each surface gets a padded tile. Repeated/out-of-range UVs are baked into
    // that tile, so wrapping cannot sample a neighbouring material.
    for(unsigned y=0;y<tile;y++)for(unsigned x=0;x<tile;x++){
      float u=u0+std::clamp((float(x)-1)/(tile-3),0.f,1.f)*du,v=v0+std::clamp((float(y)-1)/(tile-3),0.f,1.f)*dv;
      unsigned sx=std::min(image.width-1,unsigned((u-std::floor(u))*image.width)),sy=std::min(image.height-1,unsigned((v-std::floor(v))*image.height));
      auto p=&image.rgba[(sy*image.width+sx)*4];
      const size_t offset=(oy+y)*w+ox+x;
      if(replacement)std::copy_n(p,4,replacement->rgba.data()+offset*4);
      if(masked&&p[3]<128)pixels[offset]=255;
      else {
        auto index=quantizer.nearest(p[0],p[1],p[2]);
        pixels[offset]=masked?std::max(1,int(index)):index;
      }
    }
  }
  // FTE uses 0/255 for masked alias skins. The outer padding at (0,0)
  // uses 255 to keep the classic skin flood fill away from the cutout.
  if(masked)pixels[0]=255;
  for(int a=0;a<3;a++)scale[a]=high[a]>low[a]?(high[a]-low[a])/255.f:1.f;
  Binary out;out.bytes="IDPO";out.u32(6);for(float v:scale)out.real(v);for(float v:low)out.real(v);
  float radius=0;for(int a=0;a<3;a++)radius+=std::max(low[a]*low[a],high[a]*high[a]);out.real(std::sqrt(radius));for(int a=0;a<3;a++)out.real(0);
  for(unsigned v:{1u,w,h,nv,nt,mesh.frames,0u,masked?1u<<14:0u})out.u32(v);out.real(1);
  out.u32(0);out.bytes.append(reinterpret_cast<const char*>(pixels.data()),pixels.size());
  for(auto v:uv){out.u32(0);out.u32(v[0]);out.u32(v[1]);}
  unsigned base=0;for(const auto& s:mesh.surfaces){for(auto t:s.triangles){out.u32(1);for(auto v:t){if(v>=s.frames[0].size())throw std::runtime_error("Invalid triangle.");out.u32(base+v);}}base+=s.frames[0].size();}
  static const float normals[][3]={
#include "third_party/quake_anorms.h"
  };
  for(unsigned f=0;f<mesh.frames;f++){
    out.u32(0);out.bytes.append(4,'\0');out.bytes.append(3,char(255));out.bytes+='\0';out.text(f<mesh.frameNames.size()?mesh.frameNames[f]:"frame"+std::to_string(f),16);
    for(const auto& s:mesh.surfaces)for(const auto& v:s.frames[f]){
      for(int a=0;a<3;a++)out.bytes+=char(std::clamp(int(std::round((v.position[a]-low[a])/scale[a])),0,255));
      unsigned normal=0;float best=-1e30f;for(unsigned n=0;n<162;n++){float d=0;for(int a=0;a<3;a++)d+=v.normal[a]*normals[n][a];if(d>best){best=d;normal=n;}}out.bytes+=char(normal);
    }
  }
  return out.bytes;
}
}
