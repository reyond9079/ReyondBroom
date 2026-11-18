// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_bsp_atlas.h"
#include "ftebroom_model_mdl_export.h"
#include "ftebroom_model_studio_export.h"

namespace ftebroom::brushModel {
// GoldSrc MDL stores UVs in a finite skin. Bake repeated brush textures inside
// it so a face cannot sample blank padding or a neighboring texture in FTE.
inline void bakeStudioSkin(modelExport::Surface& surface,modelExport::Skin& skin)
{
  constexpr unsigned margin=2;
  auto& frame=surface.frames.front();
  const std::array<unsigned,2> sourceSize{skin.width,skin.height};
  std::array<unsigned,2> outputSize{};
  std::array<double,2> offset{},scale{};
  for(int axis=0;axis<2;axis++){
    double low=frame.front().uv[axis],high=low;
    for(const auto& vertex:frame){
      low=std::min(low,double(vertex.uv[axis]));
      high=std::max(high,double(vertex.uv[axis]));
    }
    offset[axis]=std::floor(low*sourceSize[axis]);
    const double span=std::max(1.,std::ceil(high*sourceSize[axis])-offset[axis]);
    outputSize[axis]=unsigned(std::min(512.,std::ceil((span+2*margin)/4)*4));
    scale[axis]=std::min(1.,double(outputSize[axis]-2*margin)/span);
  }
  const auto width=outputSize[0],height=outputSize[1];
  modelExport::Skin baked{width,height,std::vector<unsigned char>(size_t(width)*height*4)};
  auto pixel=[&](unsigned position,int axis){
    const double sample=std::floor((double(position)+.5-margin)/scale[axis]+offset[axis]);
    const double wrapped=std::fmod(sample,sourceSize[axis]);
    return unsigned(wrapped<0?wrapped+sourceSize[axis]:wrapped);
  };
  for(unsigned y=0;y<height;y++){
    const size_t sourceRow=size_t(pixel(y,1))*skin.width;
    for(unsigned x=0;x<width;x++)
      std::copy_n(skin.rgba.data()+4*(sourceRow+pixel(x,0)),4,baked.rgba.data()+4*(size_t(y)*width+x));
  }
  for(auto& vertex:frame)
    for(int axis=0;axis<2;axis++)
      vertex.uv[axis]=float((margin+(double(vertex.uv[axis])*sourceSize[axis]-offset[axis])*scale[axis])/outputSize[axis]);
  skin=std::move(baked);
}

// Decode the compiler's visible faces so overlapping world brushes go through
// the same CSG as a BSP model. Keep the compiler's face winding and texture axes.
inline std::string mdl(const std::string& bsp,const std::string& quakePalette,bool goldSrc)
{
  using quake1::range;
  using quake1::word;
  using bspAtlas::real;
  using bspAtlas::u16;
  if(!(goldSrc?quake1::gameBsp(bsp,4):quake1::bsp(bsp,true)))
    throw std::runtime_error("The compiler did not produce a supported brush model BSP.");
  const bool wide=bsp.compare(0,4,"BSP2")==0||bsp.compare(0,4,"2PSB")==0;
  auto lump=[&](unsigned index){return bsp.substr(word(bsp,4+index*8),word(bsp,8+index*8));};
  const auto planes=lump(1), textures=lump(2), vertices=lump(3), texinfo=lump(6),
    faces=lump(7), edges=lump(12), surfaceEdges=lump(13), models=lump(14);
  const unsigned stride=wide?28:20, edgeStride=wide?8:4;
  const unsigned first=word(models,56), count=word(models,60);
  if(count>4096)throw std::runtime_error("Brush MDL conversion supports up to 4096 compiled faces.");
  range(faces,size_t(first)*stride,size_t(count)*stride);
  const unsigned textureCount=word(textures,0);
  range(textures,4,size_t(textureCount)*4);
  struct Material {
    std::string name;
    unsigned surface=0;
  };
  modelExport::Mesh mesh;
  mesh.frames=1;
  mesh.frameNames={"static"};
  std::vector<modelExport::Skin> skins;
  std::map<unsigned,Material> materials;
  std::vector<std::map<std::array<float,8>,unsigned>> vertexIndices;
  size_t totalPixels=0;
  auto resolveMaterial=[&](unsigned index)->const Material&{
    auto found=materials.find(index);
    if(found!=materials.end())return found->second;
    if(index>=textureCount)throw std::runtime_error("Invalid BSP material index.");
    const auto offset=word(textures,4+size_t(index)*4);
    range(textures,offset,40);
    const auto name=quake1::name(textures,offset,16);
    auto lower=name;
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
    constexpr unsigned skip=~0u;
    for(const char* tool:{"skip","hint","null","clip","playerclip","monsterclip","origin","trigger","aaatrigger"})
      if(lower==tool)return materials.emplace(index,Material{name,skip}).first->second;
    if(name.empty()||name[0]=='*'||name[0]=='!'||name[0]=='{'||name[0]=='+'||name[0]=='-'||lower.rfind("sky",0)==0)
      throw std::runtime_error("MDL conversion requires static opaque textures. Use BSP for "+name+".");
    const unsigned width=word(textures,offset+16),height=word(textures,offset+20);
    const unsigned pixels=word(textures,offset+24);
    if(!width||!height||width>4096||height>4096||!pixels)
      throw std::runtime_error("The compiler must embed the brush texture "+name+" before MDL conversion.");
    if(goldSrc&&(width>512||height>512||width%4||height%4))
      throw std::runtime_error("GoldSrc MDL textures must fit 512 x 512 pixels and be divisible by four: "+name+".");
    if(mesh.surfaces.size()>=(goldSrc?32:16))
      throw std::runtime_error("The selection exceeds the target MDL material limit.");
    const size_t imageSize=size_t(width)*height, start=size_t(offset)+pixels;
    if((totalPixels+=imageSize)>16777216)throw std::runtime_error("Brush MDL textures exceed the 16-million-pixel budget.");
    range(textures,start,imageSize);
    std::string palette=quakePalette;
    if(goldSrc){
      const auto lastMip=word(textures,offset+36);
      if(!lastMip)throw std::runtime_error("No GoldSrc mip levels for "+name+".");
      const size_t table=size_t(offset)+lastMip+imageSize/64;
      if(u16(textures,table)!=256)throw std::runtime_error("Invalid GoldSrc texture palette: "+name+".");
      range(textures,table+2,768);
      palette=textures.substr(table+2,768);
    }
    if(palette.size()!=768)throw std::runtime_error("Load the Quake palette before converting brushwork to MDL.");
    modelExport::Skin skin{width,height,std::vector<unsigned char>(imageSize*4)};
    for(size_t pixel=0;pixel<imageSize;pixel++){
      const auto color=static_cast<unsigned char>(textures[start+pixel]);
      std::copy_n(palette.data()+3*color,3,skin.rgba.data()+4*pixel);
      skin.rgba[4*pixel+3]=255;
    }
    const auto surface=unsigned(mesh.surfaces.size());
    mesh.surfaces.push_back({name,{},{{}}});
    skins.push_back(std::move(skin));
    vertexIndices.emplace_back();
    return materials.emplace(index,Material{name,surface}).first->second;
  };

  size_t totalVertices=0,totalTriangles=0;
  for(unsigned f=0;f<count;f++){
    const size_t face=(size_t(first)+f)*stride;
    const size_t ti=size_t(wide?word(faces,face+16):u16(faces,face+10))*40;
    range(texinfo,ti,40);
    const auto& material=resolveMaterial(word(texinfo,ti+32));
    if(material.surface==~0u)continue;
    const auto& skin=skins[material.surface];
    auto& surface=mesh.surfaces[material.surface];
    auto& indices=vertexIndices[material.surface];
    const size_t plane=size_t(wide?word(faces,face):u16(faces,face))*20;
    range(planes,plane,20);
    const bool back=(wide?word(faces,face+4):u16(faces,face+2))!=0;
    const auto start=word(faces,face+(wide?8:4));
    const auto points=wide?word(faces,face+12):u16(faces,face+8);
    if(points<3||points>2048)throw std::runtime_error("Invalid brush face winding.");
    range(surfaceEdges,size_t(start)*4,size_t(points)*4);
    std::vector<modelExport::Vertex> winding(points);
    std::array<float,2> minUV{1e30f,1e30f};
    for(unsigned i=0;i<points;i++){
      const auto signedEdge=int32_t(word(surfaceEdges,(size_t(start)+i)*4));
      if(signedEdge==INT32_MIN)throw std::runtime_error("Invalid BSP edge.");
      const size_t edge=size_t(signedEdge<0?-signedEdge:signedEdge)*edgeStride+(signedEdge<0?edgeStride/2:0);
      const size_t vertex=size_t(wide?word(edges,edge):u16(edges,edge))*12;
      auto& v=winding[i];
      for(int axis=0;axis<3;axis++){
        v.position[axis]=real(vertices,vertex+axis*4);
        v.normal[axis]=real(planes,plane+axis*4)*(back?-1.f:1.f);
      }
      for(int axis=0;axis<2;axis++){
        const size_t row=ti+axis*16;
        float uv=real(texinfo,row+12);
        for(int k=0;k<3;k++)uv+=real(texinfo,row+4*k)*v.position[k];
        v.uv[axis]=uv/(axis?skin.height:skin.width);
        if(!std::isfinite(v.uv[axis]))throw std::runtime_error("Invalid brush texture coordinates.");
        minUV[axis]=std::min(minUV[axis],v.uv[axis]);
      }
    }
    std::vector<unsigned> polygon;
    for(auto& v:winding){
      // Discard whole texture repeats before baking. Texture phase remains exact.
      for(int axis=0;axis<2;axis++)v.uv[axis]-=std::floor(minUV[axis]);
      const std::array<float,8> key{v.position[0],v.position[1],v.position[2],
        v.normal[0],v.normal[1],v.normal[2],v.uv[0],v.uv[1]};
      auto [it,added]=indices.emplace(key,unsigned(surface.frames[0].size()));
      if(added){
        if(++totalVertices>2048)throw std::runtime_error("The compiled selection exceeds the 2048-vertex MDL limit.");
        surface.frames[0].push_back(v);
      }
      polygon.push_back(it->second);
    }
    for(size_t i=2;i<polygon.size();i++){
      if(++totalTriangles>(goldSrc?20000:4096))
        throw std::runtime_error("The compiled selection exceeds the target MDL triangle limit.");
      surface.triangles.push_back({polygon[0],polygon[i-1],polygon[i]});
    }
  }
  if(!totalTriangles)throw std::runtime_error("The compiler produced no visible brush faces.");
  if(goldSrc)
    for(size_t i=0;i<mesh.surfaces.size();i++)bakeStudioSkin(mesh.surfaces[i],skins[i]);
  return goldSrc?studioExport::bake(mesh,skins,10.f):modelExport::mdl(mesh,skins,quakePalette);
}
}
