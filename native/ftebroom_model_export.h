// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <algorithm>
namespace ftebroom::modelExport {
struct Vertex {std::array<float,3> position,normal;std::array<float,2> uv;};
struct Surface {std::string material;std::vector<std::array<unsigned,3>> triangles;std::vector<std::vector<Vertex>> frames;};
struct Mesh {std::vector<Surface> surfaces;unsigned frames=0;std::vector<std::string> frameNames;};
inline Mesh parse(const std::string& bytes) {
  std::istringstream input(bytes);input.imbue(std::locale::classic());Mesh mesh;std::string magic;
  unsigned version=0,surfaces=0;input>>magic>>version>>mesh.frames>>surfaces;
  if(magic!="FTEBROOM_MESH"||version!=1||!mesh.frames||mesh.frames>1024||!surfaces||surfaces>256)throw std::runtime_error("Invalid model extraction header.");
  mesh.surfaces.resize(surfaces);size_t total=0;
  for(unsigned frame=0;frame<mesh.frames;frame++)for(auto& surface:mesh.surfaces) {
    unsigned vertices=0,triangles=0;std::string material;input>>std::quoted(material)>>vertices>>triangles;
    if(!vertices||vertices>65536||!triangles||triangles>131072||(total+=vertices)>4000000)throw std::runtime_error("Model exceeds conversion limits.");
    if(!frame)surface.material=material;
    else if(material!=surface.material||vertices!=surface.frames[0].size()||triangles!=surface.triangles.size())throw std::runtime_error("Animated topology changes cannot be exported.");
    std::vector<Vertex> pose(vertices);
    for(auto& vertex:pose){for(auto& value:vertex.position)input>>value;for(auto& value:vertex.normal)input>>value;for(auto& value:vertex.uv)input>>value;
      for(float value:vertex.position)if(!std::isfinite(value))throw std::runtime_error("Non-finite model position.");
      for(float value:vertex.normal)if(!std::isfinite(value))throw std::runtime_error("Non-finite model normal.");
      for(float value:vertex.uv)if(!std::isfinite(value))throw std::runtime_error("Non-finite model UV.");}
    for(unsigned i=0;i<triangles;i++){std::array<unsigned,3> tri{};for(auto& index:tri){input>>index;if(index>=vertices)throw std::runtime_error("Invalid triangle index.");}
      if(!frame)surface.triangles.push_back(tri);else if(surface.triangles[i]!=tri)throw std::runtime_error("Animated topology changes cannot be exported.");}
    if(!input)throw std::runtime_error("Model extraction is truncated.");surface.frames.push_back(std::move(pose));
  }
  input>>std::ws;if(!input.eof())throw std::runtime_error("Unexpected model extraction data.");return mesh;
}
struct Binary {
  std::string bytes;
  void u32(unsigned n){for(int i=0;i<4;i++)bytes+=char(n>>(i*8));}
  void u16(unsigned n){bytes+=char(n);bytes+=char(n>>8);}
  void real(float v){unsigned n;std::memcpy(&n,&v,4);u32(n);}
  void text(const std::string& value,size_t length){if(value.size()>=length)throw std::runtime_error("Model name or material path is too long for the target format.");bytes+=value;bytes.append(length-value.size(),'\0');}
};
inline std::string md3(const Mesh& mesh) {
  if(!mesh.frames||mesh.surfaces.empty())throw std::runtime_error("Empty model.");
  Binary surfaces;
  for(size_t i=0;i<mesh.surfaces.size();i++) {
    const auto& s=mesh.surfaces[i];unsigned nv=s.frames[0].size(),nt=s.triangles.size();
    if(nv>4096||nt>8192)throw std::runtime_error("MD3 allows at most 4096 vertices and 8192 triangles per surface.");
    unsigned triOffset=108,shaderOffset=triOffset+nt*12,uvOffset=shaderOffset+68,xyzOffset=uvOffset+nv*8,end=xyzOffset+nv*8*mesh.frames;
    surfaces.bytes+="IDP3";surfaces.text("surface"+std::to_string(i),64);surfaces.u32(0);surfaces.u32(mesh.frames);surfaces.u32(1);surfaces.u32(nv);surfaces.u32(nt);
    for(unsigned n:{triOffset,shaderOffset,uvOffset,xyzOffset,end})surfaces.u32(n);
    for(const auto& t:s.triangles)for(auto n:t)surfaces.u32(n);
    surfaces.text(s.material,64);surfaces.u32(0);
    for(const auto& v:s.frames[0])for(float n:v.uv)surfaces.real(n);
    for(const auto& frame:s.frames)for(const auto& v:frame) {
      for(float n:v.position){float value=std::round(n*64);if(value<-32768||value>32767)throw std::runtime_error("MD3 coordinates must fit -512 to 511.984 units. Scale the source before converting.");surfaces.u16(unsigned(int(value))&65535);}
      float length=std::sqrt(v.normal[0]*v.normal[0]+v.normal[1]*v.normal[1]+v.normal[2]*v.normal[2]);
      float lat=length>0?std::atan2(v.normal[1],v.normal[0]):0,longitude=length>0?std::acos(std::clamp(v.normal[2]/length,-1.f,1.f)):0;
      unsigned a=unsigned(int(std::round(lat*255/(2*3.141592653589793))))&255,b=unsigned(std::round(longitude*255/(2*3.141592653589793)))&255;surfaces.u16((a<<8)|b);
    }
  }
  Binary out;out.bytes+="IDP3";out.u32(15);out.text("ReyondBroom conversion",64);out.u32(0);out.u32(mesh.frames);out.u32(0);out.u32(mesh.surfaces.size());out.u32(0);
  out.u32(108);out.u32(108+mesh.frames*56);out.u32(108+mesh.frames*56);out.u32(108+mesh.frames*56+surfaces.bytes.size());
  for(unsigned frame=0;frame<mesh.frames;frame++) {
    std::array<float,3> low{1e30f,1e30f,1e30f},high{-1e30f,-1e30f,-1e30f};float radius=0;
    for(const auto& surface:mesh.surfaces)for(const auto& v:surface.frames[frame]){float r=0;for(int a=0;a<3;a++){low[a]=std::min(low[a],v.position[a]);high[a]=std::max(high[a],v.position[a]);r+=v.position[a]*v.position[a];}radius=std::max(radius,std::sqrt(r));}
    for(float n:low)out.real(n);for(float n:high)out.real(n);for(int a=0;a<3;a++)out.real(0);out.real(radius);out.text(frame<mesh.frameNames.size()?mesh.frameNames[frame]:"frame"+std::to_string(frame),16);
  }
  out.bytes+=surfaces.bytes;return out.bytes;
}
inline std::string obj(const Mesh& mesh) {
  std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9)<<"# ReyondBroom static model export\n";unsigned base=1;
  for(size_t i=0;i<mesh.surfaces.size();i++){const auto& s=mesh.surfaces[i];out<<"o surface"<<i<<"\nusemtl surface"<<i<<"\n";
    for(const auto& v:s.frames[0])out<<"v "<<v.position[0]<<' '<<v.position[2]<<' '<<v.position[1]<<'\n';
    for(const auto& v:s.frames[0])out<<"vt "<<v.uv[0]<<' '<<1-v.uv[1]<<'\n';
    for(const auto& v:s.frames[0])out<<"vn "<<v.normal[0]<<' '<<v.normal[2]<<' '<<v.normal[1]<<'\n';
    for(const auto& t:s.triangles){out<<"f";for(auto n:t){n+=base;out<<' '<<n<<'/'<<n<<'/'<<n;}out<<'\n';}base+=s.frames[0].size();}
  return out.str();
}
}
