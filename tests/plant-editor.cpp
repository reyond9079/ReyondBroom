// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/plant/export.h"
#include "../native/plant/presets.h"
#include "../native/plant/thumbnail.h"
#include "../native/ftebroom_md3.h"
#include "../native/ftebroom_bb_obj.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <chrono>
using namespace ftebroom::plant;
static int checks=0;
static void expect(bool value,const std::string& label) {if(!value)throw std::runtime_error(label);checks++;std::cout<<"PASS "<<label<<'\n';}
template<class F>static void rejects(F action,const char* label){try{action();}catch(const std::exception&){expect(true,label);return;}throw std::runtime_error(std::string("Expected rejection: ")+label);}
static uint64_t fingerprint(const Mesh& m) {
  std::string bytes;
  auto append=[&](auto value){bytes.append(reinterpret_cast<const char*>(&value),sizeof value);};
  for(auto& v:m.vertices){for(double x:v.p)append(x);for(double x:v.n)append(x);for(double x:v.uv)append(x);}
  for(auto& f:m.faces){for(unsigned i:f.i)append(i);append(f.material);append(int(f.part));}return hash(bytes);
}
static void write(const std::filesystem::path& path,const std::string& bytes){std::ofstream f(path,std::ios::binary);f.write(bytes.data(),bytes.size());if(!f)throw std::runtime_error("Cannot write test artifact");}
int main(int argc,char** argv) try {
  std::filesystem::path output=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::path("plant-test-output");std::filesystem::create_directories(output);
  auto catalog=presets();expect(catalog.size()>=20,"at least twenty usable presets");std::set<int> families;std::set<std::string> identities;
  Json report=Json::array();double maxMilliseconds=0;
  for(auto d:catalog) {
    validate(d);expect(serialize(deserialize(serialize(d)))==serialize(d),d.preset+" editable project roundtrip");
    families.insert(int(d.family));identities.insert(d.preset);auto start=std::chrono::steady_clock::now();
    std::array<Mesh,3> lod;for(int i=0;i<3;i++){lod[i]=generate(d,i);validate(lod[i]);expect(!lod[i].faces.empty()&&lod[i].faces.size()<=size_t(d.p.budget*(i==0?1:i==1?d.p.lod1Ratio:d.p.lod2Ratio)),d.preset+" valid bounded LOD"+std::to_string(i));}
    auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();maxMilliseconds=std::max(ms,maxMilliseconds);
    expect(lod[0].faces.size()>lod[1].faces.size()&&lod[1].faces.size()>lod[2].faces.size(),d.preset+" triangle reduction across LODs");
    size_t opposed=0;for(auto& face:lod[0].faces){auto a=lod[0].vertices[face.i[0]],b=lod[0].vertices[face.i[1]],c=lod[0].vertices[face.i[2]];if(dot(cross(sub(b.p,a.p),sub(c.p,a.p)),add(add(a.n,b.n),c.n))<0)opposed++;}
    expect(opposed<lod[0].faces.size()/50+1,d.preset+" normals agree with triangle winding");
    expect(fingerprint(generate(d))==fingerprint(lod[0]),d.preset+" seed regeneration exact");
    auto changed=d;changed.seed++;expect(fingerprint(generate(changed))!=fingerprint(lod[0]),d.preset+" new seed changes geometry");
    auto maps=textureMaps(d,nullptr,nullptr,48);auto image=thumbnail(lod[0],maps,160,160);
    expect(std::count(image.rgba.begin()+3,image.rgba.end(),255)>100,d.preset+" actual mesh thumbnail visible");
    write(output/(d.preset+".png"),png(image));
    report.push_back({{"preset",d.preset},{"family",familyName(d.family)},{"triangles",{lod[0].faces.size(),lod[1].faces.size(),lod[2].faces.size()}},{"generation_ms",ms}});
    if(d.preset=="oak"||d.preset=="pine"||d.preset=="amanita"||d.preset=="daisy") {
      auto encoded=md3(lod[0],"textures/reyond-plugins/plant-test");auto decoded=ftebroom::readMd3(encoded);size_t triangles=0;double error=0;
      size_t at=0;for(auto& surface:decoded)for(auto& triangle:surface.triangles){auto face=lod[0].faces[at++];for(int i=0;i<3;i++)for(int a=0;a<3;a++)error=std::max(error,std::abs(double(triangle[i].point[a])-lod[0].vertices[face.i[i]].p[a]));triangles++;}
      std::cout<<"MD3 "<<d.preset<<" triangles="<<triangles<<" max_position_error="<<std::setprecision(12)<<error<<'\n';
      expect(triangles==lod[0].faces.size()&&error<=1./128.+2e-5,d.preset+" production MD3 decoder roundtrip");
      write(output/(d.preset+".md3"),encoded);
      auto objText=obj(lod[0],d.preset+".mtl");auto albedo=png(maps.albedo);auto mtl="newmtl plant\nmap_Kd "+d.preset+"-atlas.png\n";
      auto imported=ftebroom::bb::importObj(objText,[&](const std::string& name){return name==d.preset+".mtl"?mtl:albedo;});
      auto trianglesOut=ftebroom::bb::triangles(imported);expect(trianglesOut.size()==lod[0].faces.size()&&!imported.textures.empty()&&!imported.textures[0].rgba.empty(),d.preset+" production OBJ material/UV reimport");
      write(output/(d.preset+".obj"),objText);write(output/(d.preset+".mtl"),mtl);write(output/(d.preset+"-atlas.png"),albedo);write(output/(d.preset+".glb"),glb(lod[0],maps));
      write(output/(d.preset+".plant.json"),serialize(d).dump(2));
    }
  }
  expect(families.size()>=11&&identities.size()==catalog.size(),"every botanical family and unique preset identity");
  auto d=catalog.front();auto base=generate(d);auto original=fingerprint(base);
  std::vector<std::pair<std::string,std::function<void(Project&)>>> edits={
    {"height",[](auto& x){x.p.height*=.7;}},{"radius",[](auto& x){x.p.radius*=1.4;}},{"taper",[](auto& x){x.p.taper=.4;}},
    {"curvature",[](auto& x){x.p.curvature=.9;}},{"spread",[](auto& x){x.p.spread=.4;}},{"age",[](auto& x){x.p.age=.4;}},
    {"density",[](auto& x){x.p.density=.3;}},{"irregularity",[](auto& x){x.p.irregularity=.9;}},{"gravity",[](auto& x){x.p.gravity=1;}},
    {"light",[](auto& x){x.p.light=.8;}},{"branch count",[](auto& x){x.p.branches=6;}},{"depth",[](auto& x){x.p.depth=2;}},
    {"segments",[](auto& x){x.p.segments=4;}},{"sides",[](auto& x){x.p.sides=5;}},{"branch angle",[](auto& x){x.p.branchAngle=90;}},
    {"branch length",[](auto& x){x.p.branchLength=.3;}},{"branch taper",[](auto& x){x.p.branchTaper=.3;}},{"dead wood",[](auto& x){x.p.dead=.7;}},
    {"leaf size",[](auto& x){x.p.leafSize=7;}},{"leaf width",[](auto& x){x.p.leafWidth=.2;}},{"leaf curl",[](auto& x){x.p.leafCurl=.8;}},
    {"leaf twist",[](auto& x){x.p.leafTwist=.8;}},{"leaf count",[](auto& x){x.p.leaves=4;}},{"leaf shape",[](auto& x){x.p.leafShape=1;}},
    {"leaf cards",[](auto& x){x.p.foliageMode=2;}},{"root flare",[](auto& x){x.p.rootFlare=3;}},{"winter",[](auto& x){x.p.season=3;}},
    {"growth curve",[](auto& x){x.p.growth={0,.1,.2,.4,1.2};}},{"spline point",[](auto& x){x.p.controls[2][0]=.3;}},
    {"triangle budget",[](auto& x){x.p.budget=3000;}},{"optimized geometry",[](auto& x){x.p.quality=1;}}
  };
  for(auto& edit:edits){auto x=d;edit.second(x);expect(fingerprint(generate(x))!=original,"wired parameter "+edit.first);}
  for(auto id:{"bamboo","ivy"}){auto x=*std::find_if(catalog.begin(),catalog.end(),[&](auto& p){return p.preset==id;});auto before=fingerprint(generate(x));x.p.segments=4;expect(fingerprint(generate(x))!=before,std::string(id)+" spline tessellation control changes mesh");}
  {auto x=*std::find_if(catalog.begin(),catalog.end(),[](auto& p){return p.preset=="bracken";});auto before=fingerprint(generate(x));x.p.sides=4;expect(fingerprint(generate(x))!=before,"fern radial sides change actual frond stems");}
  {auto x=*std::find_if(catalog.begin(),catalog.end(),[](auto& p){return p.preset=="saguaro";});auto before=fingerprint(generate(x));x.p.irregularity=.8;expect(fingerprint(generate(x))!=before,"saguaro irregularity changes arm placement");auto skin=textureMaps(x,nullptr,nullptr,48);x.p.dryness=.8;expect(textureMaps(x,nullptr,nullptr,48).albedo.rgba!=skin.albedo.rgba,"cactus dryness changes its skin material");x.p.branches=0;validate(generate(x));expect(true,"saguaro permits a valid unbranched plant");}
  auto dense=d;dense.p.budget=100000;dense.p.branches=30;dense.p.depth=4;dense.p.leaves=40;dense.p.age=1;
  auto denseStart=std::chrono::steady_clock::now();auto denseMesh=generate(dense);validate(denseMesh);
  double denseMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-denseStart).count();
  expect(denseMesh.faces.size()>base.faces.size()&&denseMesh.faces.size()<=100000,"a dense mature tree can add detail within the triangle budget");
  auto hidden=d;for(auto& n:hidden.nodes)n.visible=false;expect(generate(hidden).faces.empty(),"hide all components produces an empty preview");
  auto moved=d;moved.nodes[1].offset={10,0,0};expect(fingerprint(generate(moved))!=original,"hierarchy transform changes mesh");
  auto cache=generateBase(d);expect(fingerprint(hierarchy(cache,d))==original,"cached base mesh matches full generation");
  auto restored=deserialize(serialize(moved));expect(fingerprint(generate(restored))==fingerprint(generate(moved)),"hierarchy save/load retains geometry");
  auto duplicated=d;auto copied=duplicateComponent(duplicated,1);validate(duplicated);
  expect(duplicated.nodes.size()==15&&duplicated.nodes[copied].parent.empty(),"duplicate copies the complete trunk subtree");
  expect(std::any_of(duplicated.nodes.begin(),duplicated.nodes.end(),[&](auto& n){return n.parent==duplicated.nodes[copied].id;}),"duplicate children use copied parent IDs");
  expect(generate(duplicated,2).faces.size()<=size_t(duplicated.p.budget*duplicated.p.lod2Ratio),"duplicated components retain the selected LOD budget");
  rejects([&](){while(true)duplicateComponent(duplicated,copied);},"duplicate enforces the component limit");
  auto maps=textureMaps(d,nullptr,nullptr,48);auto again=textureMaps(d,nullptr,nullptr,48);expect(maps.albedo.rgba==again.albedo.rgba&&maps.normal.rgba==again.normal.rgba&&maps.orm.rgba==again.orm.rgba,"procedural textures deterministic");
  auto material=d;material.p.leaf={.8,.1,.1};expect(textureMaps(material,nullptr,nullptr,48).albedo.rgba!=maps.albedo.rgba,"foliage color affects actual texture");
  material=d;material.p.roughness=.2;expect(textureMaps(material,nullptr,nullptr,48).orm.rgba!=maps.orm.rgba,"roughness changes exported PBR data");
  material=d;material.p.quality=3;expect(textureMaps(material,nullptr,nullptr,48).albedo.rgba!=maps.albedo.rgba,"stylized quality changes actual material palette");
  size_t alpha=0,opaque=0;for(int y=0;y<48;y++)for(int x=48;x<96;x++){auto value=maps.albedo.rgba[(y*192+x)*4+3];if(value<128)alpha++;else opaque++;}
  expect(alpha>0&&opaque>0,"leaf atlas contains real alpha cutout");
  Image override{2,2,std::vector<unsigned char>(16,255)};auto replacement=textureMaps(d,&override,nullptr,48);expect(replacement.albedo.rgba!=maps.albedo.rgba,"imported leaf pixels replace generated material");
  auto vertex=base.vertices.back();vertex.p[2]=d.p.height*.7;vertex.wind=1;
  auto p=d.p;p.windStrength=0;expect(windPosition(vertex,p,.5,d.p.height)==vertex.p,"zero wind preserves static geometry");
  p.windStrength=1;expect(windPosition(vertex,p,.5,d.p.height)!=vertex.p,"wind actually deforms vertices");
  std::atomic<bool> cancelled{true};rejects([&](){generate(d,0,&cancelled);},"cancel generation before allocating a full plant");
  rejects([&](){auto j=serialize(d);j["generatorVersion"]=99;deserialize(j);},"reject unknown generator version");
  rejects([&](){auto x=d;x.p.height=std::numeric_limits<double>::infinity();generate(x);},"reject non-finite parameters");
  rejects([&](){auto x=d;x.p.branches=10000000;generate(x);},"reject extreme topology inputs");
  rejects([&](){auto x=d;x.nodes[1].parent=x.nodes[1].id;validate(x);},"reject cyclic hierarchy");
  rejects([&](){auto x=d;x.nodes[1].parent="missing";validate(x);},"reject missing hierarchy parent");
  rejects([&](){auto x=d;x.p.height=1800;md3(generate(x),"textures/plant");},"reject out-of-range MD3 coordinates");
  write(output/"presets.json",report.dump(2));write(output/"results.json",Json{{"checks",checks},{"presets",catalog.size()},{"max_generation_ms",maxMilliseconds},{"dense_tree_ms",denseMs},{"dense_tree_triangles",denseMesh.faces.size()}}.dump(2));
  std::cout<<"Plant core: "<<checks<<" checks passed; maximum three-LOD generation "<<maxMilliseconds<<" ms\n";return 0;
} catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
