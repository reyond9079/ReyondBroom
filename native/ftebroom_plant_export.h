// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "plant/export.h"
#include <filesystem>

namespace ftebroom::plantExport {
enum class Format { Glb, Obj, Mdl, Md2, Md3, Iqm, Md5, Studio };
struct Choice {Format format;const char* label;const char* extension;const char* hint;int profile;};
inline constexpr Choice choices[]={
  {Format::Glb,"glTF binary (.glb)",".glb","Full detail, embedded PBR textures. Static pose.",-1},
  {Format::Obj,"Wavefront (.obj)",".obj","Full detail, MTL and PNG textures beside the model. Static pose.",-1},
  {Format::Mdl,"Quake MDL (.mdl)",".mdl","Quake limits and embedded indexed skin; foliage uses opaque geometry.",2},
  {Format::Md2,"Quake II MD2 (.md2)",".md2","Quake II limits; writes a companion PCX skin.",3},
  {Format::Md3,"Quake III MD3 (.md3)",".md3","Static pose with a companion TGA atlas. Coordinates must fit MD3 precision.",0},
  {Format::Iqm,"Inter-Quake Model (.iqm)",".iqm","Static pose with a companion TGA atlas.",1},
  {Format::Md5,"Doom 3 MD5 mesh (.md5mesh)",".md5mesh","Static mesh with a root joint and companion TGA atlas.",1},
  {Format::Studio,"Half-Life / GoldSrc MDL (.mdl)",".mdl","GoldSrc limits and embedded indexed skin; separate from Quake MDL.",4}
};
inline const Choice& choice(Format format){
  for(const auto& item:choices)if(item.format==format)return item;
  throw std::runtime_error("Unknown plant export format");
}
inline std::filesystem::path pathFor(std::filesystem::path path,Format format){
  if(path.filename().empty())throw std::runtime_error("Choose a model filename");
  return path.replace_extension(choice(format).extension);
}
using Bundle=std::map<std::string,std::string>;
// Return the complete file set before touching disk. In particular the two MDL
// encodings are selected explicitly, never inferred from their shared suffix.
inline Bundle encode(Format format,const std::string& filename,const plant::Mesh& mesh,
    const plant::Textures& maps,const std::string& palette={},const std::string& material={}){
  plant::validate(mesh);Bundle out;
  const auto stem=std::filesystem::u8path(filename).stem().u8string();
  if(stem.empty()||std::filesystem::u8path(filename).filename().u8string()!=filename)
    throw std::runtime_error("Plant export requires a plain model filename");
  if(format==Format::Glb)out[filename]=plant::glb(mesh,maps);
  else if(format==Format::Obj){
    out[stem+"-albedo.png"]=plant::png(maps.albedo);
    out[stem+"-normal.png"]=plant::png(maps.normal);
    out[stem+"-orm.png"]=plant::png(maps.orm);
    out[stem+".mtl"]="newmtl plant\nKd 1 1 1\nd 1\nmap_Kd "+stem+"-albedo.png\nmap_d "+stem+"-albedo.png\nnorm "+stem+"-normal.png\n";
    out[filename]=plant::obj(mesh,stem+".mtl");
  }else if(format==Format::Mdl||format==Format::Md2||format==Format::Studio){
    auto model=plant::aliasMesh(mesh,"plant");auto skin=plant::aliasSkin(maps.albedo);
    if(format==Format::Mdl)out[filename]=modelExport::mdl(model,{skin},palette);
    else if(format==Format::Studio)out[filename]=studioExport::bake(model,{skin},1);
    else {auto result=modelExport::md2(model,{skin},palette,material.empty()?stem+".pcx":material);out[filename]=std::move(result.model);out[stem+".pcx"]=std::move(result.skin);}
  }else{
    auto model=plant::modelMesh(mesh,material.empty()?stem+"-albedo.tga":material,format!=Format::Md3);
    out[filename]=format==Format::Md3?modelExport::md3(model):format==Format::Iqm?modelExport::iqm(model):modelExport::md5mesh(model);
    out[stem+"-albedo.tga"]=plant::tga(maps.albedo);
  }
  return out;
}
}
