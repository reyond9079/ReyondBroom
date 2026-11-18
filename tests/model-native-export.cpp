// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_model_md2_export.h"
#include "../native/ftebroom_model_studio_export.h"
#include "../native/ftebroom_legacy_models.h"
#include "../native/ftebroom_legacy_images.h"
#include "../native/ftebroom_bb_io.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace ftebroom;
static void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);std::cout<<"PASS "<<name<<'\n';}
static void save(const std::filesystem::path& p,const std::string& b){std::ofstream f(p,std::ios::binary);f.write(b.data(),b.size());if(!f)throw std::runtime_error("Cannot write fixture.");}

static void checkLongSourcePaths(const std::filesystem::path& root) {
  auto folder = std::filesystem::absolute(root);
  for (int i = 0; i < 5; ++i)
    folder /= "portable-project-directory-with-many-characters-" + std::to_string(i);
  std::filesystem::create_directories(filesystemAccessPath(folder));
  const auto source = folder / std::filesystem::u8path(u8"текстура-素材.rbtexpkg");
  bb::atomicWrite(source, "original");
  bb::atomicWrite(source, "replacement");
  check(source.native().size() > 260 && bb::readFile(source) == "replacement",
        "atomic_write_and_read_long_unicode_path");
  const auto absent = folder / "missing-parent" / "file.rbtexpkg";
  bool diagnosed = false;
  try {
    bb::atomicWrite(absent, "invalid");
  } catch (const std::filesystem::filesystem_error& error) {
    diagnosed = error.path1().parent_path() == absent.parent_path() && bool(error.code());
  }
  check(diagnosed && bb::readFile(source) == "replacement", "atomic_write_reports_os_error_and_preserves_prior_file");
}

int main(int argc,char** argv){try{
  if(argc!=2)throw std::runtime_error("Pass a private output directory.");std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
  checkLongSourcePaths(root);
  auto lights=quake1::halfLifeLights("{\n\"classname\" \"light\"\n\"light\" \"300\"\n}\n");check(lights.find("\"_light\" \"255 255 255 300\"")!=lights.npos,"halflife_legacy_light_conversion");
  auto explicitLight=quake1::halfLifeLights("{\n\"classname\" \"light\"\n\"_light\" \"200 100 50 400\"\n}\n");check(explicitLight.find("255 255 255")==explicitLight.npos,"halflife_explicit_light_preserved");
  modelExport::Mesh mesh;mesh.frames=3;modelExport::Surface s;s.material="skin";s.triangles={{0,1,2}};
  for(int f=0;f<3;f++)s.frames.push_back({{{-8.f,-8.f,10.f+f*2},{0,0,1},{0,0}},{{8,-8,10.f+f*2},{0,0,1},{1,0}},{{0,8,10.f+f*2},{0,0,1},{0,1}}});mesh.surfaces.push_back(s);
  std::string palette;for(int c=0;c<256;c++)for(int a=0;a<3;a++)palette+=char(c);modelExport::Skin skin;skin.width=skin.height=16;skin.rgba.resize(16*16*4);for(size_t i=0;i<skin.rgba.size();i+=4){skin.rgba[i]=skin.rgba[i+1]=skin.rgba[i+2]=160;skin.rgba[i+3]=255;}
  auto md2=modelExport::md2(mesh,{skin},palette,"models/qa/native95.pcx");save(root/"native95.md2",md2.model);save(root/"native95.pcx",md2.skin);
  check(md2.model.substr(0,4)=="IDP2"&&quake1::word(md2.model,40)==3,"md2_native_header_and_all_frames");auto pcx=legacy::pcx(md2.skin);check(pcx.width==256&&pcx.height==256&&pcx.palette==palette,"md2_pcX_palette_and_atlas");
  for(unsigned f=0;f<3;f++){auto p=legacyModels::md2(md2.model,0,f);auto a=p[0].triangles[0][0].point;check(std::abs(a[0]+8)<.1f&&std::abs(a[2]-(10+f*2))<.1f,"md2_coordinate_origin_and_animation");}
  modelEdit::Node n;n.total=2;n.position={3,4,5};auto moved=modelExport::transformMd2(md2.model,modelEdit::local(n));auto p=legacyModels::md2(moved,0,2);check(std::abs(p[0].triangles[0][0].point[2]-33)<.1,"md2_native_transform_all_poses");
  auto studio=studioExport::bake(mesh,{skin},24);save(root/"baked95.mdl",studio);check(studio.substr(0,4)=="IDST"&&quake1::word(studio,4)==10&&quake1::word(studio,72)==studio.size(),"studio_native_header_and_length");check(quake1::word(studio,quake1::word(studio,168)+56)==3,"studio_baked_animation_preserved");auto tri=legacyModels::studio(studio,{});check(std::abs(tri[0].triangles[0][0].point[2]-10)<.001,"studio_reference_pose_and_texture");
  auto twoSurfaceMesh = mesh;
  twoSurfaceMesh.surfaces.push_back(s);
  studioExport::Data packed{studioExport::bake(twoSurfaceMesh, {skin, skin}, 24)};
  const auto textureTable = packed.u(184);
  const auto payload = packed.u(188);
  const auto body = packed.u(208);
  const auto model = packed.u(body + 72);
  const auto meshes = packed.u(model + 76);
  check(textureTable + 2 * 80 <= payload && packed.u(200) + 4 <= payload &&
        packed.u(packed.u(168) + 124) < payload && packed.u(model + 88) < payload &&
        packed.u(meshes + 4) < payload && packed.u(meshes + 20 + 4) < payload,
        "studio_geometry_animation_and_skin_table_precede_texture_payload");
  const auto textureBytes = skin.width * skin.height + 768;
  check(packed.u(textureTable + 76) == payload &&
        packed.u(textureTable + 80 + 76) == payload + textureBytes &&
        packed.b.size() == payload + 2 * textureBytes,
        "studio_embeds_contiguous_goldsrc_texture_pixels_and_palettes");
  auto output=studioExport::transform(studio,modelEdit::local(n),[](auto){return std::string();});save(root/"transformed95.mdl",output);check(quake1::word(output,140)==4&&quake1::word(output,164)==1,"studio_transform_retains_rig_and_sequence");
  bool rejected=false;try{n.scale={1,2,1};studioExport::transform(studio,modelEdit::local(n),[](auto){return std::string();});}catch(...){rejected=true;}check(rejected,"studio_rejects_unrepresentable_shear");
  rejected=false;try{auto bad=studio.substr(0,180);studioExport::transform(bad,{},[](auto){return std::string();});}catch(...){rejected=true;}check(rejected,"studio_rejects_truncated_input");
  return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
