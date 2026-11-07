// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "generator.h"
#include "textures.h"
#include "../ftebroom_model_export.h"
#include <locale>
namespace ftebroom::plant {
inline uint64_t hash(const std::string& s) {uint64_t h=1469598103934665603ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
inline std::string assetId(const Project& d,int lod) {std::ostringstream s;s<<"plant-"<<std::hex<<hash(serialize(d).dump()+std::to_string(lod));return s.str();}
inline modelExport::Mesh modelMesh(const Mesh& mesh,const std::string& material) {
  modelExport::Mesh out;out.frames=1;out.frameNames={"plant"};
  for(size_t first=0;first<mesh.faces.size();first+=1200) {
    modelExport::Surface surface;surface.material=material;surface.frames.resize(1);
    for(size_t i=first;i<std::min(mesh.faces.size(),first+1200);i++) {
      auto& f=mesh.faces[i];unsigned base=surface.frames[0].size();
      for(auto n:f.i) {const auto& v=mesh.vertices[n];modelExport::Vertex vertex;
        for(int a=0;a<3;a++){vertex.position[a]=v.p[a];vertex.normal[a]=v.n[a];}vertex.uv={float(v.uv[0]),float(v.uv[1])};surface.frames[0].push_back(vertex);}
      surface.triangles.push_back({base,base+1,base+2});
    }out.surfaces.push_back(std::move(surface));
  }return out;
}
inline std::string md3(const Mesh& mesh,const std::string& material) {
  auto out=modelMesh(mesh,material);
  if(out.surfaces.size()>32)throw std::runtime_error("MD3 exceeds 32 surfaces. Export a lower LOD or a smaller triangle budget.");
  return modelExport::md3(out);
}
inline std::string obj(const Mesh& mesh,const std::string& materialFile) {
  std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9)<<"# ReyondBroom Plant Editor; right-handed Y-up\nmtllib "<<materialFile<<"\no plant\nusemtl plant\n";
  for(auto& v:mesh.vertices)out<<"v "<<v.p[0]<<' '<<v.p[2]<<' '<<-v.p[1]<<'\n';
  for(auto& v:mesh.vertices)out<<"vt "<<v.uv[0]<<' '<<1-v.uv[1]<<'\n';
  for(auto& v:mesh.vertices)out<<"vn "<<v.n[0]<<' '<<v.n[2]<<' '<<-v.n[1]<<'\n';
  for(auto& f:mesh.faces){out<<"f";for(unsigned i:f.i){i++;out<<' '<<i<<'/'<<i<<'/'<<i;}out<<'\n';}return out.str();
}
inline std::string shader(const std::string& material,const std::string& image) {
  return material+"\n{\n qer_editorimage "+image+"\n q3map_nonplanar\n q3map_shadeangle 60\n surfaceparm trans\n surfaceparm alphashadow\n cull none\n {\n  map "+image+"\n  alphaFunc GE128\n  depthWrite\n  rgbGen vertex\n }\n}\n";
}
inline std::string glb(const Mesh& mesh,const Textures& maps) {
  using modelExport::Binary;Binary data;Json views=Json::array(),accessors=Json::array();
  auto align=[&](){while(data.bytes.size()%4)data.bytes+='\0';};
  auto view=[&](size_t offset,int target=0){Json j={{"buffer",0},{"byteOffset",offset},{"byteLength",data.bytes.size()-offset}};if(target)j["target"]=target;views.push_back(j);return int(views.size()-1);};
  auto accessor=[&](int v,int component,size_t count,const char* type){accessors.push_back({{"bufferView",v},{"componentType",component},{"count",count},{"type",type}});return int(accessors.size()-1);};
  align();size_t start=data.bytes.size();
  for(auto& v:mesh.vertices){data.real(v.p[0]);data.real(v.p[2]);data.real(-v.p[1]);}
  int positions=accessor(view(start,34962),5126,mesh.vertices.size(),"VEC3");
  accessors[positions]["min"]={mesh.low[0],mesh.low[2],-mesh.high[1]};accessors[positions]["max"]={mesh.high[0],mesh.high[2],-mesh.low[1]};
  start=data.bytes.size();for(auto& v:mesh.vertices){data.real(v.n[0]);data.real(v.n[2]);data.real(-v.n[1]);}int normals=accessor(view(start,34962),5126,mesh.vertices.size(),"VEC3");
  start=data.bytes.size();for(auto& v:mesh.vertices){data.real(v.uv[0]);data.real(v.uv[1]);}int uv=accessor(view(start,34962),5126,mesh.vertices.size(),"VEC2");
  start=data.bytes.size();for(auto& f:mesh.faces)for(unsigned i:f.i)data.u32(i);int indices=accessor(view(start,34963),5125,mesh.faces.size()*3,"SCALAR");
  Json images=Json::array();for(auto image:{&maps.albedo,&maps.normal,&maps.orm}){align();start=data.bytes.size();data.bytes+=png(*image);images.push_back({{"bufferView",view(start)},{"mimeType","image/png"}});}align();
  Json root={{"asset",{{"version","2.0"},{"generator","ReyondBroom Plant Editor 1"}}},{"scene",0},
    {"scenes",Json::array({{{"nodes",{0}}}})},{"nodes",Json::array({{{"mesh",0},{"name","Plant"}}})},
    {"meshes",Json::array({{{"primitives",Json::array({{{"attributes",{{"POSITION",positions},{"NORMAL",normals},{"TEXCOORD_0",uv}}},{"indices",indices},{"material",0}}})}}})},
    {"materials",Json::array({{{"name","Procedural botanical atlas"},{"doubleSided",true},{"alphaMode","MASK"},{"alphaCutoff",.5},
      {"pbrMetallicRoughness",{{"baseColorTexture",{{"index",0}}},{"metallicRoughnessTexture",{{"index",2}}},{"metallicFactor",0.0},{"roughnessFactor",1.0}}},
      {"normalTexture",{{"index",1}}},{"occlusionTexture",{{"index",2}}}}})},
    {"images",images},{"textures",Json::array({{{"source",0},{"sampler",0}},{{"source",1},{"sampler",0}},{{"source",2},{"sampler",0}}})},
    {"samplers",Json::array({{{"magFilter",9729},{"minFilter",9987},{"wrapS",33071},{"wrapT",33071}}})},
    {"buffers",Json::array({{{"byteLength",data.bytes.size()}}})},{"bufferViews",views},{"accessors",accessors}};
  std::string json=root.dump();while(json.size()%4)json+=' ';
  Binary out;out.u32(0x46546c67);out.u32(2);out.u32(12+8+json.size()+8+data.bytes.size());out.u32(json.size());out.u32(0x4e4f534a);out.bytes+=json;
  out.u32(data.bytes.size());out.u32(0x004e4942);out.bytes+=data.bytes;return out.bytes;
}
// RBB1 is the existing engine's validated dynamic mesh ABI. It supports
// depth-tested alpha cutout and two-sided drawing, with baked vertex lighting.
inline std::string preview(const Mesh& mesh,const Project& doc,int display,double time,bool wind,double lightAngle) {
  modelExport::Binary out;out.bytes="RBB1";out.u32((mesh.faces.size()+9999)/10000);
  const double a=lightAngle*pi/180;V light=unit({std::cos(a)*.6,std::sin(a)*.6,.8});
  for(size_t first=0;first<mesh.faces.size();first+=10000) {
    size_t count=std::min<size_t>(10000,mesh.faces.size()-first);out.u32(count*3);out.u32(count*3);
    out.text(display==0?"bbtex/plant-atlas.tga":display==3?"bbtex/plant-uv.tga":"$whiteimage",128);
    for(size_t i=first;i<first+count;i++)for(unsigned n:mesh.faces[i].i) {
      auto& v=mesh.vertices[n];V position=wind?windPosition(v,doc.p,time,doc.p.height):v.p;
      double diffuse=.36+.64*std::max(0.,dot(v.n,light));double ambient=1-doc.p.ao*.16*(1-std::clamp(v.p[2]/doc.p.height,0.,1.));
      V color={diffuse*ambient,diffuse*ambient,diffuse*ambient};
      if(display==2)color={v.n[0]*.5+.5,v.n[1]*.5+.5,v.n[2]*.5+.5};
      else if(display==1)color={.73*diffuse,.77*diffuse,.68*diffuse};
      for(double x:position)out.real(x);for(double x:v.uv)out.real(x);for(double x:color)out.real(x);out.real(1);
    }
    for(unsigned i=0;i<count*3;i++)out.u32(i);
  }return out.bytes;
}
}
