// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "generator.h"
#include "textures.h"
#include "../ftebroom_model_export.h"
#include "../ftebroom_model_md2_export.h"
#include "../ftebroom_model_static_export.h"
#include "../ftebroom_model_studio_export.h"
#include <locale>
#include <deque>
namespace ftebroom::plant {
inline uint64_t hash(const std::string& s) {uint64_t h=1469598103934665603ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
inline std::string assetId(const Project& d,int lod) {std::ostringstream s;s<<"plant-"<<std::hex<<hash((d.generator>=4?"plant-export-v4:":"plant-export-v3:")+serialize(d).dump()+std::to_string(lod));return s.str();}
// Match the original Quake III renderer's per-surface tessellation limits.
inline constexpr size_t md3PlantVertices=999,md3PlantTriangles=1999,md3PlantSurfaces=32;
// IQM uses mesh-local indices in FTE. Keep each mesh addressable even on
// FTE renderers built with 16-bit indices; there is no MD3-style model cap.
inline constexpr size_t iqmPlantVertices=65536;
inline modelExport::Mesh modelMesh(const Mesh& mesh,const std::string& material,bool iqm=false) {
  modelExport::Mesh out;out.frames=1;out.frameNames={"plant"};
  const size_t vertexLimit=iqm?iqmPlantVertices:md3PlantVertices;
  const size_t triangleLimit=iqm?mesh.faces.size():md3PlantTriangles;
  std::vector<int> local(mesh.vertices.size(),-1);
  for(size_t i=0;i<mesh.faces.size();) {
    modelExport::Surface surface;surface.material=material;surface.frames.resize(1);
    std::vector<unsigned> used;
    for(;i<mesh.faces.size()&&surface.triangles.size()<triangleLimit;i++) {
      auto& face=mesh.faces[i];
      size_t missing=0;for(auto index:face.i)missing+=local[index]<0;
      if(used.size()+missing>vertexLimit)break;
      std::array<unsigned,3> triangle{};
      for(size_t corner=0;corner<3;corner++){
        auto index=face.i[corner];
        if(local[index]<0){
          const auto& v=mesh.vertices[index];modelExport::Vertex vertex;
          for(int a=0;a<3;a++){vertex.position[a]=v.p[a];vertex.normal[a]=v.n[a];}
          vertex.uv={float(v.uv[0]),float(v.uv[1])};
          local[index]=surface.frames[0].size();
          surface.frames[0].push_back(vertex);used.push_back(index);
        }
        triangle[corner]=local[index];
      }
      // Quake model formats use clockwise front faces. Internal geometry,
      // OBJ and glTF use outward counterclockwise winding.
      std::swap(triangle[1],triangle[2]);
      surface.triangles.push_back(triangle);
    }
    for(auto index:used)local[index]=-1;
    out.surfaces.push_back(std::move(surface));
  }
  return out;
}
inline std::string md3(const Mesh& mesh,const std::string& material) {
  auto out=modelMesh(mesh,material);
  if(out.surfaces.size()>md3PlantSurfaces)throw std::runtime_error("MD3 exceeds 32 surfaces. Export a lower LOD or a smaller triangle budget.");
  return modelExport::md3(out);
}
// Vanilla alias renderers do not support alpha-cutout skins. Generate shaped
// leaves instead of cards, retain complete organs, and weld their shared
// vertices before applying each game's actual limits. The editable source and
// portable exports retain full detail; this is the target game's runtime LOD.
inline modelExport::Mesh aliasMesh(const Mesh& mesh,const std::string& material) {
  modelExport::Mesh out;out.frames=1;out.frameNames={"plant"};
  modelExport::Surface surface;surface.material=material;surface.frames.resize(1);
  std::map<std::array<float,8>,unsigned> indices;
  for(auto& face:mesh.faces){std::array<unsigned,3> triangle{};
    for(int a=0;a<3;a++){
      const auto& v=mesh.vertices[face.i[a]];std::array<float,8> key{};
      for(int k=0;k<3;k++){key[k]=float(v.p[k]);key[k+3]=float(v.n[k]);}key[6]=float(v.uv[0]);key[7]=float(v.uv[1]);
      auto [it,added]=indices.emplace(key,surface.frames[0].size());
      if(added)surface.frames[0].push_back({{key[0],key[1],key[2]},{key[3],key[4],key[5]},{key[6],key[7]}});
      triangle[a]=it->second;
    }
    std::swap(triangle[1],triangle[2]); // clockwise in MDL / MD2 / GoldSrc
    surface.triangles.push_back(triangle);
    // Quake MDL and Quake II MD2 cull backfaces. A leaf must still be visible
    // from below, without depending on an engine-specific cull-none shader.
    if(face.part==Part::Leaves||face.part==Part::Flowers||face.part==Part::Twigs)
      surface.triangles.push_back({triangle[2],triangle[1],triangle[0]});
  }
  out.surfaces.push_back(std::move(surface));return out;
}
inline bool opaquePlantTarget(int profile){return profile>=2&&profile<=4;}
inline size_t aliasPlantVertices(int profile){return profile==2?1024:2048;}
inline size_t aliasPlantTriangles(int profile){return profile==2?2048:profile==4?20000:4096;}
inline const char* targetClass(int profile){return profile==4?"cycler":profile==1?"func_model":"misc_model";}
inline size_t plantTrunkCount(const Mesh& mesh) {
  std::set<unsigned> trunks;
  for(auto& face:mesh.faces)if(face.part==Part::Trunk)trunks.insert(face.organ);
  return trunks.size();
}
inline Mesh targetGeometry(const Project& source,const Mesh& full,int profile,bool maskedQuake=false) {
  if(profile==1)return full;
  if(!opaquePlantTarget(profile)){
    auto surfaces=modelMesh(full,"").surfaces.size();
    if(surfaces<=md3PlantSurfaces)return full;
    auto d=source;
    // Search using the actual surface count: shared vertices can fit many
    // more triangles than the worst case of three new vertices per face.
    Mesh best;
    for(int low=256,high=source.p.budget-1;low<=high;){
      d.p.budget=low+(high-low)/2;
      auto candidate=generate(d);
      if(modelMesh(candidate,"").surfaces.size()<=md3PlantSurfaces){
        if(candidate.faces.size()>best.faces.size())best=std::move(candidate);
        low=d.p.budget+1;
      }else high=d.p.budget-1;
    }
    if(!best.faces.empty())return best;
    throw std::runtime_error("The plant cannot fit in 32 Quake III MD3 surfaces.");
  }
  const size_t maxVertices=aliasPlantVertices(profile),maxTriangles=aliasPlantTriangles(profile);
  const bool foliage=std::any_of(full.faces.begin(),full.faces.end(),[](auto& f){return f.part==Part::Leaves;});
  const bool treeGroup=source.generator>=4&&source.p.quality!=3&&source.p.clumps>1&&
    (source.family==Family::Broadleaf||source.family==Family::Conifer);
  const size_t requiredTrunks=treeGroup&&plantTrunkCount(full)?size_t(source.p.clumps):0;
  auto fits=[&](const Mesh& mesh){
    if(mesh.faces.empty()||(requiredTrunks&&plantTrunkCount(mesh)!=requiredTrunks))return false;
    if(foliage&&std::none_of(mesh.faces.begin(),mesh.faces.end(),[](auto& f){return f.part==Part::Leaves;}))return false;
    const auto model=aliasMesh(mesh,"plant");
    const auto& surface=model.surfaces.front();
    return surface.frames[0].size()<=maxVertices&&surface.triangles.size()<=maxTriangles;
  };
  // The full mesh is already suitable for opaque skins when it does not use
  // foliage cards. Keep the authored topology if it fits the target format.
  const bool opaque=source.generator>=4&&!maskedQuake;
  if((maskedQuake||(!foliage&&foliagePhoto(source)==Photo::None))&&fits(full))return full;
  auto d=source;
  if(opaque){
    d.p.foliageMode=0;
    auto candidate=generate(d,0,nullptr,true);
    if(fits(candidate))return candidate;
  }
  // Try the original branch and stem settings with the game triangle cap
  // before changing the topology. Encoded vertex counts include UV seams.
  d.p.budget=std::min(source.p.budget,int(maxTriangles));
  auto candidate=generate(d,0,nullptr,opaque,treeGroup);
  if(fits(candidate))return candidate;
  if(d.generator<4)d.generator=std::min(2,d.generator);
  d.p.quality=2;d.p.foliageMode=0;
  // Spend a fixed budget over the whole crown. Repeatedly regenerate complete
  // geometry, never truncate triangle indices or drop late canopy triangles.
  for(int detail=0;detail<5;detail++){
    d.p.sides=std::max(3,source.p.sides/(detail+2));
    d.p.segments=std::max(3,source.p.segments/(detail+2));
    d.p.depth=std::min(source.p.depth,detail?1:2);
    d.p.branches=std::max(source.family==Family::Cactus?0:1,std::min(source.p.branches/(detail+1),profile==2?8:12));
    if(treeGroup){
      // Keep the branching plan spread over the original crown. The grouped
      // generator reduces tessellation and samples that plan within the cap.
      d.p.depth=std::min(source.p.depth,2);
      d.p.branches=std::min(source.p.branches,16);
      d.p.sides=std::max(3,std::min(d.p.sides,6-detail));
    }else d.p.clumps=std::max(1,source.p.clumps/(1<<detail));
    d.p.leaflets=std::max(3,source.p.leaflets/(detail+1));
    d.p.petals=std::max(3,source.p.petals/(detail+1));d.p.flowerLayers=std::max(1,source.p.flowerLayers/(detail+1));
    int ceiling=std::min(source.p.budget,int(maxTriangles));
    Mesh best;
    for(int low=256,high=ceiling;low<=high;){
      d.p.budget=low+(high-low+1)/2;
      auto mesh=generate(d,0,nullptr,opaque,treeGroup);
      if(fits(mesh)){
        if(mesh.faces.size()>best.faces.size())best=std::move(mesh);
        low=d.p.budget+1;
      }else high=d.p.budget-1;
    }
    if(!best.faces.empty())return best;
  }
  if(treeGroup)throw std::runtime_error("The target model cannot retain every tree within its format limits. Place the trees as separate models or use smaller groups. The editable source is preserved.");
  throw std::runtime_error(std::string(profile==2?"Quake MDL":profile==3?"Quake II MD2":"GoldSrc MDL")+" plant LOD cannot retain the visible plant within its vertex/triangle limits. Reduce stems, branches or component copies; the editable source is preserved.");
}
inline modelExport::Mesh targetModel(const Mesh& mesh,const std::string& material,int profile){return opaquePlantTarget(profile)?aliasMesh(mesh,material):modelMesh(mesh,material,profile==1);}
inline const char* targetExtension(int profile){return profile==2||profile==4?".mdl":profile==3?".md2":profile==1?".iqm":".md3";}
inline void remapMdlPreview(Mesh& mesh,const modelExport::MdlSkinRegion& region,const modelExport::Skin& skin){
  for(auto& vertex:mesh.vertices){
    const auto uv=region.pixel(float(vertex.uv[0]),float(vertex.uv[1]));
    vertex.uv={(uv[0]+.5)/skin.width,(uv[1]+.5)/skin.height};
  }
}
// Fill transparent texels from the nearest coloured texel of their own tile.
// Opaque legacy skins then colour the shaped geometry without black fringes.
inline modelExport::Skin aliasSkin(const Image& atlas){
  modelExport::Skin skin{unsigned(atlas.width),unsigned(atlas.height),atlas.rgba};
  for(int ty=0;ty<4;ty++)for(int tx=0;tx<4;tx++){
    int x0=tx*atlas.width/4,x1=(tx+1)*atlas.width/4,y0=ty*atlas.height/4,y1=(ty+1)*atlas.height/4;
    std::deque<std::pair<int,int>> queue;std::vector<bool> seen(size_t(x1-x0)*(y1-y0),false);
    auto index=[&](int x,int y){return size_t(y-y0)*(x1-x0)+x-x0;};
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++)if(skin.rgba[(y*atlas.width+x)*4+3]>=128){seen[index(x,y)]=true;queue.emplace_back(x,y);}
    while(!queue.empty()){auto [x,y]=queue.front();queue.pop_front();for(auto [dx,dy]:{std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}}){int a=x+dx,b=y+dy;if(a<x0||a>=x1||b<y0||b>=y1||seen[index(a,b)])continue;seen[index(a,b)]=true;for(int c=0;c<3;c++)skin.rgba[(b*atlas.width+a)*4+c]=skin.rgba[(y*atlas.width+x)*4+c];queue.emplace_back(a,b);}}
  }
  for(size_t p=3;p<skin.rgba.size();p+=4)skin.rgba[p]=255;return skin;
}
// FTE accepts uncompressed RGBA DDS with an explicit mip chain. Ordinary box
// mipmaps average sparse leaf alpha below GE128, erasing the crown at distance.
// Preserve alpha coverage per atlas tile, including its final one-pixel mip.
// Keep mipmapping enabled: removing it causes aliasing and unnecessary sampling.
inline std::vector<Image> coverageMips(const Image& atlas){
  if(atlas.width<4||atlas.height<4||atlas.rgba.size()!=size_t(atlas.width)*atlas.height*4)throw std::runtime_error("Invalid plant atlas");
  std::vector<Image> chain{atlas};
  while(chain.back().width>1||chain.back().height>1){
    const auto& previous=chain.back();Image next{std::max(1,previous.width/2),std::max(1,previous.height/2),{}};next.rgba.resize(size_t(next.width)*next.height*4);
    for(int y=0;y<next.height;y++)for(int x=0;x<next.width;x++){
      unsigned sum=0,color[3]{};for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){auto p=&previous.rgba[(std::min(previous.height-1,y*2+dy)*previous.width+std::min(previous.width-1,x*2+dx))*4];sum+=p[3];for(int c=0;c<3;c++)color[c]+=unsigned(p[c])*p[3];}
      auto p=&next.rgba[(y*next.width+x)*4];p[3]=static_cast<unsigned char>((sum+2)/4);for(int c=0;c<3;c++)p[c]=sum?static_cast<unsigned char>((color[c]+sum/2)/sum):0;
    }
    int tiles=next.width>=4&&next.height>=4?4:1;
    for(int ty=0;ty<tiles;ty++)for(int tx=0;tx<tiles;tx++){
      size_t occupied=0,total=0;for(int y=ty*atlas.height/tiles;y<(ty+1)*atlas.height/tiles;y++)for(int x=tx*atlas.width/tiles;x<(tx+1)*atlas.width/tiles;x++){occupied+=atlas.rgba[(y*atlas.width+x)*4+3]>=128;total++;}
      std::vector<unsigned char> alpha;for(int y=ty*next.height/tiles;y<(ty+1)*next.height/tiles;y++)for(int x=tx*next.width/tiles;x<(tx+1)*next.width/tiles;x++)alpha.push_back(next.rgba[(y*next.width+x)*4+3]);
      if(alpha.empty()||!occupied)continue;std::sort(alpha.begin(),alpha.end(),std::greater<unsigned char>());
      size_t keep=std::clamp<size_t>(size_t(std::lround(double(occupied)*alpha.size()/total)),1,alpha.size());double gain=128./std::max(.5,double(alpha[keep-1]));
      for(int y=ty*next.height/tiles;y<(ty+1)*next.height/tiles;y++)for(int x=tx*next.width/tiles;x<(tx+1)*next.width/tiles;x++){auto& a=next.rgba[(y*next.width+x)*4+3];a=static_cast<unsigned char>(std::clamp(std::lround(a*gain),0l,255l));}
    }
    chain.push_back(std::move(next));
  }return chain;
}
inline std::string coverageDds(const Image& atlas){
  auto mips=coverageMips(atlas);modelExport::Binary out;out.bytes="DDS ";out.u32(124);out.u32(0x2100f);out.u32(atlas.height);out.u32(atlas.width);out.u32(atlas.width*4);out.u32(0);out.u32(mips.size());for(int i=0;i<11;i++)out.u32(0);
  for(unsigned n:{32u,0x41u,0u,32u,0xffu,0xff00u,0xff0000u,0xff000000u,0x401008u,0u,0u,0u,0u})out.u32(n);
  for(auto& mip:mips)out.bytes.append(reinterpret_cast<const char*>(mip.rgba.data()),mip.rgba.size());return out.bytes;
}
inline std::string obj(const Mesh& mesh,const std::string& materialFile) {
  std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9)<<"# ReyondBroom Plant Editor; right-handed Y-up\nmtllib "<<materialFile<<"\no plant\nusemtl plant\n";
  for(auto& v:mesh.vertices)out<<"v "<<v.p[0]<<' '<<v.p[2]<<' '<<-v.p[1]<<'\n';
  for(auto& v:mesh.vertices)out<<"vt "<<v.uv[0]<<' '<<1-v.uv[1]<<'\n';
  for(auto& v:mesh.vertices)out<<"vn "<<v.n[0]<<' '<<v.n[2]<<' '<<-v.n[1]<<'\n';
  for(auto& f:mesh.faces){out<<"f";for(unsigned i:f.i){i++;out<<' '<<i<<'/'<<i<<'/'<<i;}out<<'\n';}return out.str();
}
// q3map2 bakes misc_model lighting into world vertices. FTE func_model stays
// a runtime model entity: it has normals but no baked vertex lighting.
inline const char* windProgramSource() {
  return R"GLSL(!!ver 100 150
!!permu FOG
!!samps diffuse
#include "sys/defs.h"
#include "sys/fog.h"
varying vec2 tc;
varying vec3 light;
#ifdef VERTEX_SHADER
void main() {
    vec3 p = v_position.xyz;
    float height = max(0.0, p.z);
    float weight = height / (height + 84.0);
    float phase = dot(m_model[3].xy, vec2(0.007, 0.011));
    float sway = sin(e_time * 1.9 + p.z * 0.018 + phase);
    float flutter = sin(e_time * 4.6 + dot(p.xy, vec2(0.11, 0.07)) + phase);
    p.xy += vec2(4.2 * sway + 0.55 * flutter, 1.9 * sway - 0.38 * flutter) * weight * weight;
    gl_Position = m_modelviewprojection * vec4(p, 1.0);
    tc = v_texcoord;
    light = max(vec3(0.12), e_light_ambient + max(0.0, dot(normalize(v_normal), e_light_dir)) * e_light_mul);
}
#endif
#ifdef FRAGMENT_SHADER
void main() {
    vec4 leaf = texture2D(s_diffuse, tc);
    if (leaf.a < 0.5) discard;
    gl_FragColor = fog4(vec4(leaf.rgb * light, 1.0));
}
#endif
)GLSL";
}
inline std::string shader(const std::string& material,const std::string& image,bool dynamicModel=false,bool wind=false) {
  auto editorImage=image.size()>4&&image.substr(image.size()-4)==".dds"?image.substr(0,image.size()-4)+".tga":image;
  auto program=wind?"\n program reyond_plant_wind_v1\n diffusemap "+image:std::string{};
  return material+"\n{\n qer_editorimage "+editorImage+(editorImage!=image?"\n q3map_lightimage "+editorImage:"")+"\n q3map_nonplanar\n q3map_shadeangle 60\n surfaceparm trans\n surfaceparm alphashadow\n cull none"+program+"\n {\n  map "+image+"\n  alphaFunc GE128\n  depthWrite\n  rgbGen "+(dynamicModel?"lightingDiffuse":"vertex")+"\n  alphaGen identity\n }\n}\n";
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
inline std::string preview(const Mesh& mesh,const Project& doc,int display,double time,bool wind,double lightAngle,
                           const char* atlas="bbtex/plant-atlas.tga",uint64_t selectedBranch=0) {
  modelExport::Binary out;out.bytes="RBB1";out.u32((mesh.faces.size()+9999)/10000);
  const double a=lightAngle*pi/180;V light=unit({std::cos(a)*.6,std::sin(a)*.6,.8});
  for(size_t first=0;first<mesh.faces.size();first+=10000) {
    size_t count=std::min<size_t>(10000,mesh.faces.size()-first);out.u32(count*3);out.u32(count*3);
    out.text(display==0?atlas:display==3?"bbtex/plant-uv.tga":display==4?"bbtex/plant-normal.tga":"$whiteimage",128);
    for(size_t i=first;i<first+count;i++)for(unsigned n:mesh.faces[i].i) {
      auto& v=mesh.vertices[n];V position=wind?windPosition(v,doc.p,time,doc.p.height):v.p;
      double diffuse=.36+.64*std::max(0.,dot(v.n,light));double ambient=1-doc.p.ao*.16*(1-std::clamp(v.p[2]/doc.p.height,0.,1.));
      V color={diffuse*ambient,diffuse*ambient,diffuse*ambient};
      if(display==2)color={v.n[0]*.5+.5,v.n[1]*.5+.5,v.n[2]*.5+.5};
      else if(display==1)color={.73*diffuse,.77*diffuse,.68*diffuse};
      else if(display==4)color={1.,1.,1.};
      if(selectedBranch&&mesh.faces[i].branch==selectedBranch)color={1.,.92,.55};
      for(double x:position)out.real(x);for(double x:v.uv)out.real(x);for(double x:color)out.real(x);out.real(1);
    }
    for(unsigned i=0;i<count*3;i++)out.u32(i);
  }return out.bytes;
}
}
