// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/plant/export.h"
#include "../native/plant/presets.h"
#include "../native/ftebroom_mdl.h"
#include "../native/ftebroom_legacy_models.h"
#include "../native/ftebroom_legacy_images.h"
#include "../native/ftebroom_quake_palette.h"
#include "../native/ftebroom_static_models.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace ftebroom::plant;
static void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static void write(const std::filesystem::path& path,const std::string& bytes){std::ofstream out(path,std::ios::binary);out.write(bytes.data(),bytes.size());require(bool(out),"write artifact");}
static void checkBarkMappingAndWinding(const std::string& palette){
  const int sides=8;
  Builder tube(150);
  tube.repeatBark=true;
  require(tube.tube({{0,0,0},{0,0,200}},10,8,sides,Bark,Part::Trunk),"tall tiled trunk fits geometry budget");
  const double repeat=barkRepeatLength;
  for(int ring=0;ring<4;ring++){
    const auto& v=tube.mesh.vertices.at(ring*(sides+1));
    require(std::abs(v.p[2]-ring*repeat)<1e-6&&std::abs(v.uv[1]-atlasUV(Bark,0,ring%2)[1])<1e-6,
      "long trunk has a real UV repeat boundary every 64 units");
  }
  auto turn=barkStations({{0,0,0},{48,0,0},{48,80,0}});
  require(turn.size()==4&&turn[2].segment==1&&std::abs(turn[2].blend-.20)<1e-6&&turn.back().distance==128.,
    "branch repeats use distance along its bends without duplicate boundary rings");

  auto alias=aliasMesh(tube.mesh,"plant");
  ftebroom::modelExport::Skin solid{16,16,std::vector<unsigned char>(16*16*4,255)};
  auto mdl=ftebroom::readMdl(ftebroom::modelExport::mdl(alias,{solid},palette));
  require(mdl.triangles.size()==tube.mesh.faces.size(),"opaque wood is not duplicated as a two-sided leaf");
  auto orientation=[&](size_t i,const auto& positions){
    const auto& face=tube.mesh.faces[i];
    auto expected=add(add(tube.mesh.vertices[face.i[0]].n,tube.mesh.vertices[face.i[1]].n),tube.mesh.vertices[face.i[2]].n);
    return dot(cross(sub(positions[1],positions[0]),sub(positions[2],positions[0])),expected);
  };
  auto md3=modelMesh(tube.mesh,"plant");
  const auto& md3Surface=md3.surfaces.at(0);
  for(size_t i=0;i<tube.mesh.faces.size();i++){
    std::array<V,3> generated{},decoded{},exported{};
    for(int j=0;j<3;j++){
      generated[j]=tube.mesh.vertices[tube.mesh.faces[i].i[j]].p;
      auto& point=mdl.triangles[i][j].point;
      auto& md3Point=md3Surface.frames[0][md3Surface.triangles[i][j]].position;
      decoded[j]={point[0],point[1],point[2]};
      exported[j]={md3Point[0],md3Point[1],md3Point[2]};
    }
    require(orientation(i,generated)>0&&orientation(i,decoded)<0&&orientation(i,exported)<0,
      "wood normals face outward while Quake triangles are clockwise, including the closed ends");
  }
}
static void checkWoodJunction() {
  const WoodJunction joint{{0.,0.,0.},{0.,0.,1.},6.};
  std::vector<V> path{{0.,0.,0.},{24.,0.,0.},{48.,0.,0.}};
  addJunctionRing(path,joint);
  require(path.size()==4&&path[1]==V{6.,0.,0.},"junction ring is placed on the parent surface");
  Builder straight(200),collar(200);
  require(straight.tube(path,2.,.15,8,Bark,Part::Primary),"plain branch fixture fits");
  require(collar.tube(path,2.,.15,8,Bark,Part::Primary,0.,0.,0.,0,&joint),"branch collar fixture fits");
  validate(collar.mesh);
  bool differentPosition=false,differentNormal=false;
  for(size_t i=9;i<18;i++){
    differentPosition|=straight.mesh.vertices[i].p!=collar.mesh.vertices[i].p;
    differentNormal|=straight.mesh.vertices[i].n!=collar.mesh.vertices[i].n;
  }
  require(differentPosition&&differentNormal,"collar modifies the shape and blends normals near the parent surface");
  require(straight.mesh.vertices[18].p==collar.mesh.vertices[18].p,"the branch beyond the collar stays unchanged");
}
static void checkWoodMaterials() {
  Builder tree(300);
  tree.repeatBark=true;tree.barkURepeats=2;tree.barkVUnits=32.;tree.separateWoodCut=true;
  require(tree.tube({{0.,0.,0.},{0.,0.,96.}},4.,2.,8,Bark,Part::Trunk),"tiled wood and caps fit budget");
  require(tree.mesh.vertices[0].uv==atlasUV(Bark,0.,0.)&&tree.mesh.vertices[4].uv==atlasUV(Bark,1.,0.)&&
    tree.mesh.vertices[8].uv==atlasUV(Bark,0.,0.)&&tree.mesh.vertices[9].uv==atlasUV(Bark,0.,1.),
    "bark repeats correctly across the circumference and every 32 world units");
  require(std::count_if(tree.mesh.faces.begin(),tree.mesh.faces.end(),[](const Face& face){return face.material==Wood;})==16,
    "the end caps use the cut wood tile");
  auto p=presets().front();p.texture="leaf.png";p.barkTexture="bark.png";p.cutTexture="cut.png";p.barkURepeats=2;p.barkVUnits=32.;
  require(serialize(deserialize(serialize(p)))==serialize(p),"separate materials and bark scale persist");
  const Image leaf{1,1,{20,30,40,140}},bark{1,1,{50,60,70,80}},cut{1,1,{90,100,110,120}};
  auto maps=textureMaps(p,&leaf,nullptr,16,nullptr,&bark,&cut);
  auto center=[&](int material){
    const auto at=((material/4*16+8)*maps.albedo.width+(material%4*16+8))*4;
    return std::vector<unsigned char>(maps.albedo.rgba.begin()+at,maps.albedo.rgba.begin()+at+4);
  };
  require(center(Leaf)==leaf.rgba&&center(Bark)==std::vector<unsigned char>{50,60,70,255}&&
    center(Wood)==std::vector<unsigned char>{90,100,110,255},"custom materials stay independent and only foliage uses imported alpha");
}
static void checkTreeGroup(const Project& preset,const std::string& palette,const Textures& maps,
                           const std::filesystem::path& folder) {
  auto project=preset;
  project.seed=137;
  project.p.clumps=8;
  const auto source=generate(project);
  const auto runtime=targetGeometry(project,source,2,true);
  require(plantTrunkCount(source)==8&&plantTrunkCount(runtime)==8,preset.preset+" keeps eight trunks");
  Botanical layout(project,0,nullptr);
  for(int i=0;i<project.p.clumps;i++){
    const auto base=layout.treeTrunk(0.,layout.treeMember(i,project.p.clumps));
    const auto hasBase=[&](const Mesh& mesh){
      return std::any_of(mesh.vertices.begin(),mesh.vertices.end(),[&](const Vertex& vertex){
        return length(sub(vertex.p,base))<1e-6;
      });
    };
    require(hasBase(source)&&hasBase(runtime),preset.preset+" retains tree base "+std::to_string(i));
  }
  auto model=aliasMesh(runtime,"plant");
  require(model.surfaces[0].frames[0].size()<=1024&&model.surfaces[0].triangles.size()<=2048,
    preset.preset+" group respects MDL budgets");
  ftebroom::modelExport::Skin skin{unsigned(maps.albedo.width),unsigned(maps.albedo.height),maps.albedo.rgba};
  ftebroom::modelExport::Skin replacement;
  auto encoded=ftebroom::modelExport::mdl(model,{skin},palette,true,&replacement);
  require(ftebroom::readMdl(encoded).triangles.size()==model.surfaces[0].triangles.size(),
    preset.preset+" group round-trips through the MDL decoder");
  auto name=preset.preset+"-group.mdl";
  write(folder/name,encoded);
  write(folder/(name+"_0.png"),png({int(replacement.width),int(replacement.height),std::move(replacement.rgba)}));
}
static void checkBranchEdits(Project project) {
  const auto source=generate(project);
  std::set<uint64_t> keys;
  for(const auto& face:source.faces)if(face.part==Part::Primary&&face.branch)keys.insert(face.branch);
  require(keys.size()>2,"branches have independent IDs "+project.preset);
  const auto id=*keys.begin(),other=*std::next(keys.begin());
  const auto points=[](const Mesh& mesh,uint64_t key){
    std::vector<V> positions;
    for(const auto& face:mesh.faces)if(face.branch==key)
      for(auto index:face.i)positions.push_back(mesh.vertices[index].p);
    return positions;
  };
  project.branchEdits[id]={1.25,10.,1.2,false};
  const auto edited=generate(deserialize(serialize(project)));
  require(!points(edited,id).empty()&&points(source,id)!=points(edited,id),"branch transform survives save "+project.preset);
  require(points(source,other)==points(edited,other),"unselected branch geometry is preserved "+project.preset);
  project.branchEdits[id].removed=true;
  const auto pruned=generate(project);
  require(points(pruned,id).empty()&&!points(pruned,other).empty(),"remove only selected branch "+project.preset);
  project.seed++;
  const auto nextSeed=generate(project);
  require(std::none_of(nextSeed.faces.begin(),nextSeed.faces.end(),[&](const Face& face){return face.branch==id;}),
    "branch edits are scoped to the original seed");
}
int main(int argc,char** argv)try{
  auto folder=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::path("plant-target-results");std::filesystem::create_directories(folder);
  auto q1Palette=ftebroom::defaultQuakePalette(),q2Palette=q1Palette;
  checkBarkMappingAndWinding(q1Palette);
  checkWoodJunction();
  checkWoodMaterials();
  // CI may supply the active game's palette; both encoders must retain it.
  if(argc>2){std::ifstream in(argv[2],std::ios::binary);std::string pcx((std::istreambuf_iterator<char>(in)),{});q2Palette=ftebroom::legacy::pcx(pcx).palette;}
  Json rows=Json::array();int cases=0;
  for(auto source:presets()){
    auto saved=serialize(source);auto full=generate(source,0);auto maps=textureMaps(source,nullptr,nullptr,16);
    for(int profile:{0,1,4}){
      auto geometry=targetGeometry(source,full,profile);
      auto model=targetModel(geometry,"plant",profile);
      require(!model.surfaces.empty(),"default preset produces a game model "+source.preset);
      if(profile==4){
        const auto& surface=model.surfaces.front();
        require(surface.frames[0].size()<=2048&&surface.triangles.size()<=4096,"GoldSrc budget "+source.preset);
        require(ftebroom::studioExport::bake(model,{aliasSkin(maps.albedo)},1).substr(0,4)=="IDST","GoldSrc preset encodes "+source.preset);
      }else{
        require(model.surfaces.size()<=32,"MD3 surface budget "+source.preset);
        for(const auto& surface:model.surfaces)
          require(surface.frames[0].size()<=md3PlantVertices&&surface.triangles.size()<=md3PlantTriangles,"MD3 preset budgets "+source.preset);
      }
    }
    if(foliagePhoto(source)!=Photo::None){
      auto model=aliasMesh(targetGeometry(source,full,2,true),"plant");
      const auto& surface=model.surfaces.front();
      require(surface.frames[0].size()<=1024&&surface.triangles.size()<=2048,"LibreQuake preset budget "+source.preset);
    }
    for(int profile:{2,3}){
      auto geometry=targetGeometry(source,full,profile);validate(geometry);auto model=aliasMesh(geometry,"plant");auto& surface=model.surfaces.at(0);
      require(model.surfaces.size()==1&&model.frames==1,"one vanilla alias frame / material");
      require(surface.frames[0].size()<=(profile==2?1024u:2048u)&&surface.triangles.size()<=(profile==2?2048u:4096u),"actual vanilla vertex and triangle limits "+source.preset);
      bool sourceLeaves=std::any_of(full.faces.begin(),full.faces.end(),[](auto& f){return f.part==Part::Leaves;});
      require(!sourceLeaves||std::any_of(geometry.faces.begin(),geometry.faces.end(),[](auto& f){return f.part==Part::Leaves;}),"runtime LOD retains foliage "+source.preset);
      auto skin=aliasSkin(maps.albedo);for(size_t p=3;p<skin.rgba.size();p+=4)require(skin.rgba[p]==255,"vanilla skin is opaque");
      std::string encoded;
      if(profile==2){
        encoded=ftebroom::modelExport::mdl(model,{skin},q1Palette);auto imported=ftebroom::readMdl(encoded);
        require(encoded.substr(0,4)=="IDPO"&&imported.triangles.size()==surface.triangles.size(),"production Quake MDL importer roundtrip");
        require(imported.skin.width==256&&imported.skin.height==256,"vanilla skin dimensions");
        for(auto p:imported.skin.pixels)require(p<224,"Quake skin avoids fullbright entries");
      }else{
        auto exported=ftebroom::modelExport::md2(model,{skin},q2Palette,"models/plant.pcx");encoded=exported.model;
        auto imported=ftebroom::legacyModels::md2(encoded);auto image=ftebroom::legacy::pcx(exported.skin);
        require(encoded.substr(0,4)=="IDP2"&&imported.at(0).triangles.size()==surface.triangles.size()&&imported.at(0).image=="models/plant.pcx","production Quake II MD2 importer roundtrip");
        // Independently follow vanilla ref_gl/gl_mesh.c's command-stream path,
        // which does not use the triangle table accepted by our CPU importer.
        auto word=[&](size_t p){return ftebroom::quake1::word(encoded,p);};
        size_t cursor=word(60),commandEnd=cursor+size_t(word(36))*4;require(commandEnd==encoded.size()&&word(64)==encoded.size(),"MD2 GL command offsets/end are valid");size_t commandTriangles=0;
        auto canonical=[](std::array<unsigned,3> a){auto b=std::array<unsigned,3>{a[1],a[2],a[0]},c=std::array<unsigned,3>{a[2],a[0],a[1]};return std::min(a,std::min(b,c));};
        std::multiset<std::array<unsigned,3>> expectedTriangles,drawnTriangles;for(auto triangle:surface.triangles)expectedTriangles.insert(canonical(triangle));
        for(;;){require(cursor+4<=commandEnd,"MD2 GL stream has a terminator");int count=int32_t(word(cursor));cursor+=4;if(!count)break;
          unsigned points=unsigned(std::abs(count));require(points>=3&&cursor+size_t(points)*12<=commandEnd,"bounded MD2 GL strip/fan");commandTriangles+=points-2;
          std::vector<unsigned> sequence;
          for(unsigned point=0;point<points;point++,cursor+=12){float u=ftebroom::legacyModels::real(encoded,cursor),v=ftebroom::legacyModels::real(encoded,cursor+4);unsigned vertex=word(cursor+8);require(u>=0&&u<=1&&v>=0&&v<=1&&vertex<surface.frames[0].size(),"MD2 GL command has valid UV and vertex");sequence.push_back(vertex);
            auto st=word(48)+vertex*4;float su=(ftebroom::legacyModels::half(encoded,st)+.5f)/word(8),sv=(ftebroom::legacyModels::half(encoded,st+2)+.5f)/word(12);require(std::abs(u-su)<.000001&&std::abs(v-sv)<.000001,"GL commands retain indexed UVs with pixel centers");}
          for(size_t i=2;i<sequence.size();i++){std::array<unsigned,3> triangle=count<0?std::array<unsigned,3>{sequence[0],sequence[i-1],sequence[i]}:std::array<unsigned,3>{sequence[i-2],sequence[i-1],sequence[i]};if(count>0&&i%2)std::swap(triangle[0],triangle[1]);drawnTriangles.insert(canonical(triangle));}
        }
        require(cursor==commandEnd&&commandTriangles==surface.triangles.size()&&drawnTriangles==expectedTriangles,"vanilla Quake II OpenGL stream draws every exported triangle with original winding");
        require(image.palette==q2Palette&&image.width==256&&image.height==256,"Quake II PCX retains active palette and valid dimensions");
        if(source.preset=="oak")write(folder/"oak.pcx",exported.skin);
      }
      if(source.preset=="oak"||source.preset=="pine"||source.preset=="umbrella-acacia")write(folder/(source.preset+targetExtension(profile)),encoded);
      rows.push_back({{"preset",source.preset},{"profile",profile},{"format",targetExtension(profile)},{"vertices",surface.frames[0].size()},{"triangles",surface.triangles.size()},{"decoded",true}});++cases;
    }
    require(serialize(source)==saved,"target baking preserves editable source");
  }
  auto oak=presets().front();
  std::ifstream input("runtime/plants/builtin/photographic.rgba.z",std::ios::binary);
  std::string compressed((std::istreambuf_iterator<char>(input)),{});
  auto photos=PhotoAtlas::decode(compressed.data(),compressed.size());
  std::ifstream details("runtime/plants/builtin/photographic-details.rgba.z",std::ios::binary);
  std::string detailBytes((std::istreambuf_iterator<char>(details)),{});
  photos.loadDetails(detailBytes.data(),detailBytes.size());
  for(auto& preset:presets())if(preset.preset=="oak"||preset.preset=="pine"){
    checkTreeGroup(preset,q1Palette,textureMaps(preset,nullptr,nullptr,64,&photos),folder);
    checkBranchEdits(preset);
  }
  for(int layer=int(Photo::Oak);layer<int(Photo::None);layer++){
    unsigned visible=0,empty=0;
    for(int y=0;y<PhotoAtlas::side;y++)for(int x=0;x<PhotoAtlas::side;x++){
      auto a=photos.sample(Photo(layer),x/double(PhotoAtlas::side-1),y/double(PhotoAtlas::side-1))[3];
      visible+=a>=.5;empty+=a<.05;
    }
    require(visible>PhotoAtlas::side*PhotoAtlas::side/(layer<int(Photo::Lawn)?10:20)&&empty>PhotoAtlas::side*PhotoAtlas::side/10,
      "photographic foliage has a real cutout silhouette");
  }
  const std::array<std::string,5> classes={"misc_model","func_model","misc_model","misc_model","cycler"};
  for(int profile=0;profile<5;profile++)require(targetClass(profile)==classes[profile],"runtime classname for every profile");
  auto oakMesh=generate(oak);auto gold=targetModel(targetGeometry(oak,oakMesh,4),"plant",4);
  auto maps=textureMaps(oak,nullptr,nullptr,64,&photos);
  auto oldOak=oak;oldOak.generator=2;oldOak.p.budget=32000;auto oldMesh=generate(oldOak);
  require(oakMesh.faces.size()*3<oldMesh.faces.size(),"photographic oak uses less than a third of the former geometry");
  auto maskedGeometry=targetGeometry(oak,oakMesh,2,true);
  auto maskedModel=aliasMesh(maskedGeometry,"plant");
  require(maskedModel.surfaces[0].frames[0].size()<=1024&&maskedModel.surfaces[0].triangles.size()<=2048,
    "LibreQuake masked MDL keeps classic mesh budgets");
  require(std::any_of(maskedGeometry.faces.begin(),maskedGeometry.faces.end(),[](auto& f){return f.material==PhotoFoliage;}),
    "LibreQuake runtime uses complete photographic branch cards");
  ftebroom::modelExport::Skin photographicSkin{unsigned(maps.albedo.width),unsigned(maps.albedo.height),maps.albedo.rgba};
  ftebroom::modelExport::Skin replacement;
  auto maskedMdl=ftebroom::modelExport::mdl(maskedModel,{photographicSkin},q1Palette,true,&replacement);
  if(replacement.width!=512||replacement.height!=512||replacement.rgba.size()!=512*512*4)
    throw std::runtime_error("FTE replacement skin must match the classic MDL UV dimensions");
  auto mask=ftebroom::readMdl(maskedMdl);
  require(mask.skin.name.front()=='{'&&mask.skin.width==512&&mask.skin.pixels.front()==255,
    "FTE mask flag, skin size and safe flood-fill corner");
  require(std::count(mask.skin.pixels.begin(),mask.skin.pixels.end(),255)>2048&&
    std::none_of(mask.skin.pixels.begin(),mask.skin.pixels.end(),[](unsigned char i){return i==0;}),
    "masked alias skin has transparent holes and reserves the zero index");
  write(folder/"oak-librequake.mdl",maskedMdl);
  auto studio=ftebroom::studioExport::bake(gold,{aliasSkin(maps.albedo)},1);
  require(studio.substr(0,4)=="IDST"&&ftebroom::legacyModels::studio(studio,{}).at(0).triangles.size()==gold.surfaces[0].triangles.size(),"GoldSrc plant uses the native importer");
  write(folder/"oak-goldsrc.mdl",studio);
  require(std::any_of(oakMesh.faces.begin(),oakMesh.faces.end(),[](auto& f){return f.material==PhotoFoliage;}),"generated oak uses the photographic tile");
  auto mips=coverageMips(maps.albedo);
  Json mipRows=Json::array();for(auto& mip:mips){
    int tiles=mip.width>=4&&mip.height>=4?4:1;
    for(int material:{Leaf,LeafLight,LeafMature,LeafYoung,Needle,NeedleLight,NeedleMature,PhotoFoliage}){
      int tx=tiles==4?material%4:0,ty=tiles==4?material/4:0;size_t coverage=0;
      for(int y=ty*mip.height/tiles;y<(ty+1)*mip.height/tiles;y++)for(int x=tx*mip.width/tiles;x<(tx+1)*mip.width/tiles;x++)coverage+=mip.rgba[(y*mip.width+x)*4+3]>=128;
      require(coverage>0,"foliage survives every distance mip down to 1x1");
    }
    mipRows.push_back({{"width",mip.width},{"height",mip.height}});
  }
  auto dds=coverageDds(maps.albedo);require(dds.substr(0,4)=="DDS "&&ftebroom::quake1::word(dds,28)==mips.size(),"DDS header has real explicit mip chain");
  size_t bytes=128;for(auto& mip:mips)bytes+=mip.rgba.size();require(dds.size()==bytes,"DDS mip offsets and final size");write(folder/"oak-atlas.dds",dds);write(folder/"oak-atlas.tga",tga(maps.albedo));
  auto shaderText=shader("textures/reyond-plugins/oak","textures/reyond-plugins/oak.dds",true);require(shaderText.find("nomipmaps")==std::string::npos&&shaderText.find("alphaFunc GE128")!=shaderText.npos,"cutout mipmapping stays enabled");
  write(folder/"oak.md3",md3(generate(oak),"textures/reyond-plugins/oak"));write(folder/"oak.shader",shaderText);
  auto dense=oak;dense.p.budget=100000;dense.p.branches=30;dense.p.depth=4;dense.p.leaves=40;
  auto fullDense=generate(dense);auto q3=targetModel(targetGeometry(dense,fullDense,0),"textures/reyond-plugins/oak",0);
  require(q3.surfaces.size()<=32,"Quake III runtime LOD respects 32 MD3 surfaces");
  auto q3Bytes=ftebroom::modelExport::md3(q3);auto q3Imported=ftebroom::readMd3(q3Bytes);require(q3Imported.size()==q3.surfaces.size(),"dense Q3 runtime LOD uses actual MD3 decoder");
  std::string entity="{\n\"classname\" \"worldspawn\"\n}\n{\n\"classname\" \"func_model\"\n\"model\" \"models/reyond-plugins/oak.mdl\"\n\"_reyondbroom_authoring\" \"1\"\n}\n";
  for(int profile:{2,3}){size_t changed=0;bool stopped=false;try{ftebroom::prepareStaticModels(entity,profile,changed);}catch(std::runtime_error&){stopped=true;}require(stopped,"base game without arbitrary-model game code cannot silently compile broken func_model");}
  auto direct=entity;direct.replace(direct.find("func_model"),10,"misc_model");
  size_t directChanged=0;auto directQ1=ftebroom::prepareStaticModels(direct,2,directChanged);
  require(directChanged==1&&directQ1.find("misc_model")!=directQ1.npos,"Quake mod can compile an explicitly placed misc_model");
  require(ftebroom::prepareStaticModels(direct,3,directChanged)==direct&&directChanged==0,"Quake II keeps the target mod entity");
  auto fte=entity;fte.insert(fte.find("\"model\""),"\"_reyondbroom_fte_q1\" \"1\"\n");size_t changed=0;auto adapted=ftebroom::prepareStaticModels(fte,2,changed);require(changed==1&&adapted.find("misc_model")!=adapted.npos&&adapted.find("oak.mdl")!=adapted.npos,"explicit FTE Q1 fallback retains native MDL");
  write(folder/"results.json",Json{{"targetCases",cases},{"targets",rows},{"coverageMips",mipRows},{"vanillaCompileGate",true},{"explicitFteQ1",true}}.dump(2));
  std::cout<<"PASS "<<cases<<" native plant MDL/MD2 importer roundtrips; all mip distances retain foliage; compile capabilities checked\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
