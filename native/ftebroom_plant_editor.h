// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "plant/export.h"
#include "ftebroom_plant_export.h"
#include "plant/presets.h"
#include "plant/thumbnail.h"
#include <future>
#include <memory>
namespace plants=ftebroom::plant;
struct PlantResult {
  uint64_t revision=0;
  std::array<plants::Mesh,3> lods;
  std::array<plants::Mesh,3> baseLods;
  std::shared_ptr<plants::Textures> maps;
  std::string textureKey,geometryKey,error;
  double milliseconds=0;
};
struct PlantWork {std::atomic<bool> cancel{false};std::atomic<int> stage{0};};
struct PlantThumb {plants::Image image;int handle=0,slot=0;};
struct PlantGamePreview {
  uint64_t revision=0;
  int profile=-1;
  bool libre=false,simplified=false;
  size_t triangles=0,vertices=0,trees=0;
  plants::Mesh mesh;
  std::shared_ptr<const plants::Image> albedo;
  std::string error;
};
struct PlantState {
  bool open=false,hidden=false,owner=false,advanced=false,dirtyRender=true,framePending=true,ortho=false,wind=false,uv=false;
  bool floor=true,axes=false,handles=false,qa=false,closeRequest=false,allowFteQ1=false;
  bool gamePreview=true,gameAtlasUploaded=false;
  int lod=0,display=0,selected=1,category=-1,filter=0,thumbSlot=0;
  ftebroom::plantExport::Format exportFormat=ftebroom::plantExport::Format::Glb;
  bool exportRequest=false;
  double yaw=135,pitch=18,zoom=1,sun=-120,nextGenerate=0,lastUpload=0;
  plants::V cameraLow{-100,-100,0},cameraHigh{100,100,260},background{.075,.09,.11};
  plants::Project doc=plants::presets().front();
  std::vector<plants::Project> library=plants::presets();
  std::map<std::string,std::filesystem::path> userFiles;
  std::vector<std::string> favorites,recent;
  std::map<std::string,PlantThumb> thumbs;
  std::map<std::string,std::pair<std::string,std::string>> pendingShaders;
  std::future<std::pair<std::string,plants::Image>> thumbJob;
  std::shared_ptr<std::atomic<bool>> thumbCancel;
  std::future<PlantResult> job;
  std::future<PlantGamePreview> gameJob;
  PlantGamePreview gameResult;
  std::shared_ptr<PlantWork> work;
  std::shared_ptr<PlantResult> result;
  std::shared_ptr<plants::Textures> previewMaps;
  bool auxiliaryMapsUploaded=false,checkerUploaded=false;
  uint64_t revision=0;
  uint64_t selectedBranch=0;
  bool pending=false;
  std::vector<plants::Json> undo,redo;
  ImGuiID gesture=0;
  std::filesystem::path file,lastModel;
  std::string saved,message,error;
  std::function<void()> nextAction;
  std::array<char,160> search{};
  ImVec2 viewportMin{},viewportSize{};
  plants::Json widgets=plants::Json::object();
} plantState;
static bool plantFocused() {
  if(!plantState.open||!ImGui::GetCurrentContext())return false;
  for(auto* w=ImGui::GetCurrentContext()->NavWindow;w;w=w->ParentWindowInBeginStack)
    if(std::strstr(w->Name,"Plant Editor")||std::strstr(w->Name,"Plant action")||std::strstr(w->Name,"Save plant"))return true;
  return false;
}
static void plantRect(const std::string& id) {
  if(!plantState.qa)return;auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
  auto clip=ImGui::GetCurrentWindow()->ClipRect;
  plantState.widgets[id]={{"x",a.x},{"y",a.y},{"width",b.x-a.x},{"height",b.y-a.y},{"active",ImGui::IsItemActive()},{"visible",ImGui::IsItemVisible()},{"clip",{clip.Min.x,clip.Min.y,clip.Max.x,clip.Max.y}}};
}
static std::filesystem::path plantFolder(){return documentPath("plants");}
static void plantLibrarySave() {
  auto& s=plantState;
  try{bb::atomicWrite(plantFolder()/"library.json",plants::Json{{"version",1},{"favorites",s.favorites},{"recent",s.recent},{"advanced",s.advanced}}.dump(2));}
  catch(const std::exception& e){s.error=e.what();}
}
static void plantLibraryLoad() {
  auto& s=plantState;s.library=plants::presets();s.userFiles.clear();
  try {
    auto settings=plantFolder()/"library.json";
    if(std::filesystem::exists(settings)){auto j=plants::Json::parse(q1Read(settings));s.favorites=j.value("favorites",std::vector<std::string>{});s.recent=j.value("recent",std::vector<std::string>{});s.advanced=j.value("advanced",false);}
    auto folder=plantFolder()/"presets";std::filesystem::create_directories(folder);
    for(auto& entry:std::filesystem::directory_iterator(folder))if(entry.is_regular_file()&&entry.path().extension()==".json"&&s.userFiles.size()<32) {
      try{auto d=plants::deserialize(plants::Json::parse(q1Read(entry.path())));d.preset="user:"+entry.path().filename().u8string();s.userFiles[d.preset]=entry.path();s.library.push_back(std::move(d));}
      catch(const std::exception& e){s.error="Preset "+entry.path().filename().u8string()+": "+e.what();}
    }
  }catch(const std::exception& e){s.error=e.what();}
}
static const plants::PhotoAtlas* plantPhotos() {
  static const auto atlas=[](){
    size_t size=0;
    auto raw=static_cast<char*>(files->LoadFile("plants/builtin/photographic.rgba.z",&size));
    if(!raw)return plants::PhotoAtlas{};
    std::string bytes(raw,size);core->Free(raw);
    try{
      auto atlas=plants::PhotoAtlas::decode(bytes.data(),bytes.size());
      auto detail=static_cast<char*>(files->LoadFile("plants/builtin/photographic-details.rgba.z",&size));
      if(detail){
        bytes.assign(detail,size);core->Free(detail);
        try{atlas.loadDetails(bytes.data(),bytes.size());}
        catch(const std::runtime_error& e){core->Print(e.what());}
      }
      return atlas;
    }
    catch(const std::runtime_error& e){core->Print(e.what());return plants::PhotoAtlas{};}
  }();
  return atlas.rgba.empty()?nullptr:&atlas;
}
static std::string plantTextureKey(const plants::Project& d) {
  auto& p=d.p;return plants::Json{{"generator",d.generator},{"foliageMode",d.p.foliageMode},{"barkPattern",p.barkPattern},{"serration",p.serration},{"needleForm",p.needleForm},{"evergreen",p.evergreen},{"leaflets",p.leaflets},{"organ",p.organ},{"seed",d.seed},{"species",d.species},{"family",int(d.family)},{"bark",p.bark},{"leaf",p.leaf},{"flower",p.flower},
    {"shape",p.leafShape},{"profile",p.leafProfile},{"season",p.season},{"quality",p.quality},{"dryness",p.dryness},{"roughness",p.roughness},{"ao",p.ao},{"variation",p.variation},{"ribs",p.ribs},{"gills",p.gills},{"override",d.texture}}.dump();
}
static auto plantTextureFiles(plants::Project& doc) {
  struct Slot {const char* key;const char* stem;std::string* file;};
  return std::array<Slot,3>{{
    {"texture","leaf",&doc.texture},{"barkTexture","bark",&doc.barkTexture},{"cutTexture","cut",&doc.cutTexture}}};
}
struct PlantTextureInputs {
  plants::Image leaf,bark,cut;
};
static PlantTextureInputs plantReadTextures(const plants::Project& doc,std::string* cacheKey=nullptr) {
  auto read=[&](const std::string& name){
    if(cacheKey)*cacheKey+=":";
    if(name.empty())return plants::Image{};
    const auto bytes=q1Read(std::filesystem::u8path(name));
    if(cacheKey)*cacheKey+=std::to_string(plants::hash(bytes));
    auto decoded=modelImportDecode(bytes);
    return plants::Image{int(decoded.width),int(decoded.height),std::move(decoded.rgba)};
  };
  return {read(doc.texture),read(doc.barkTexture),read(doc.cutTexture)};
}
static plants::Textures plantTextureMaps(const plants::Project& doc,const PlantTextureInputs& input,
                                         const std::atomic<bool>* cancel,int resolution,const plants::PhotoAtlas* photos) {
  return plants::textureMaps(doc,input.leaf.width?&input.leaf:nullptr,cancel,resolution,photos,
    input.bark.width?&input.bark:nullptr,input.cut.width?&input.cut:nullptr);
}
static std::string plantGeometryKey(const plants::Project& d) {
  auto p=plants::parameters(d.p);
  for(auto key:{"bark","leaf","flower","roughness","ao","variation","dryness","windStrength","windDirection","windFrequency","stiffness","flutter","barkPattern","serration"})p.erase(key);
  return plants::Json{{"generator",d.generator},{"parameters",p},{"textureOverride",!d.texture.empty()},{"family",int(d.family)},{"species",d.species},{"seed",d.seed},{"branchEdits",plants::serializeBranchEdits(d)},{"smoothJunctions",d.smoothJunctions},{"stableLods",d.stableLods},{"woodCut",!d.cutTexture.empty()},{"barkURepeats",d.barkURepeats},{"barkVUnits",d.barkVUnits}}.dump();
}
static void plantSchedule(bool immediate=false) {
  auto& s=plantState;s.revision++;s.pending=true;s.nextGenerate=ImGui::GetTime()+(immediate?0:.20);
  if(s.work)s.work->cancel=true;if(s.thumbCancel)*s.thumbCancel=true;s.error.clear();
}
static void plantChanged(const plants::Json& before) {
  auto& s=plantState;auto active=ImGui::GetActiveID();
  if(!active||active!=s.gesture){s.undo.push_back(before);if(s.undo.size()>100)s.undo.erase(s.undo.begin());}
  if(before.value("seed",s.doc.seed)!=s.doc.seed)s.selectedBranch=0;
  s.gesture=active;s.redo.clear();plantSchedule();
}
static void plantUndo(bool redo=false) {
  auto& s=plantState;auto& from=redo?s.redo:s.undo;auto& to=redo?s.undo:s.redo;
  if(from.empty())return;to.push_back(plants::serialize(s.doc));s.doc=plants::deserialize(from.back());from.pop_back();s.gesture=0;plantSchedule(true);
}
static void plantSelect(const plants::Project& doc) {
  auto& s=plantState;auto before=plants::serialize(s.doc);s.doc=doc;s.file.clear();s.selected=std::min<int>(1,doc.nodes.size()-1);s.selectedBranch=0;s.framePending=true;
  s.undo.push_back(before);s.redo.clear();s.saved.clear();
  s.recent.erase(std::remove(s.recent.begin(),s.recent.end(),doc.preset),s.recent.end());s.recent.insert(s.recent.begin(),doc.preset);if(s.recent.size()>12)s.recent.resize(12);
  plantLibrarySave();plantSchedule(true);
}
static void plantTick() {
  auto& s=plantState;
  if(s.job.valid()&&s.job.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
    auto result=s.job.get();
    if(result.revision==s.revision&&!result.error.empty())s.error=result.error;
    if(result.revision==s.revision&&result.error.empty()) {
      s.result=std::make_shared<PlantResult>(std::move(result));s.dirtyRender=true;
      if(s.framePending){s.cameraLow=s.result->lods[0].low;s.cameraHigh=s.result->lods[0].high;s.zoom=1;s.framePending=false;}
      s.message="Generated three LODs in "+std::to_string(int(s.result->milliseconds))+" ms";
    }
  }
  if(s.pending&&!s.job.valid()&&ImGui::GetTime()>=s.nextGenerate) {
    s.pending=false;auto doc=s.doc;auto revision=s.revision;auto cached=s.result;auto key=plantTextureKey(doc),geometryKey=plantGeometryKey(doc);
    const plants::PhotoAtlas* photos=nullptr;
    try{if(doc.generator>=3)photos=plantPhotos();}catch(const std::exception& e){s.error=e.what();return;}
    s.work=std::make_shared<PlantWork>();auto work=s.work;
    s.job=std::async(std::launch::async,[doc,revision,key,geometryKey,cached,work,photos]()mutable {
      PlantResult out;out.revision=revision;out.geometryKey=geometryKey;auto start=std::chrono::steady_clock::now();
      try {
        const bool reuse=cached&&cached->geometryKey==geometryKey;
        std::unique_ptr<plants::Botanical> plan;
        if(!reuse&&doc.generator>=4&&doc.stableLods&&
            (doc.family==plants::Family::Broadleaf||doc.family==plants::Family::Conifer)){
          plants::validate(doc);
          plan=std::make_unique<plants::Botanical>(doc,0,&work->cancel);
          if(doc.family==plants::Family::Conifer)plan->planEvergreenTree();
          else plan->planWoody();
        }
        for(int i=0;i<3;i++){
          work->stage=i;if(work->cancel.load())throw std::runtime_error("Plant generation cancelled");
          if(reuse)out.baseLods[i]=cached->baseLods[i];
          else if(plan)out.baseLods[i]=plants::Botanical(doc,i,&work->cancel).renderTreePlan(*plan);
          else out.baseLods[i]=plants::generateBase(doc,i,&work->cancel);
          out.lods[i]=plants::hierarchy(out.baseLods[i],doc,i);
          if(out.lods[i].faces.empty())out.lods[i].low=out.lods[i].high={0,0,0};else plants::validate(out.lods[i]);
        }
        work->stage=3;
        auto imported=plantReadTextures(doc,&key);
        out.textureKey=key;
        if(cached&&cached->textureKey==key)out.maps=cached->maps;
        else out.maps=std::make_shared<plants::Textures>(plantTextureMaps(doc,imported,&work->cancel,256,photos));
      }catch(const std::exception& e){out.error=e.what();}
      out.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return out;
    });
  }
  if(s.thumbJob.valid()&&s.thumbJob.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
    try {auto result=s.thumbJob.get();if(result.first.empty())return;int slot=s.thumbSlot++%160;
      for(auto it=s.thumbs.begin();it!=s.thumbs.end();){
        if(it->second.slot!=slot){++it;continue;}
        if(it->second.handle)draw->UnloadImage(it->second.handle);
        it=s.thumbs.erase(it);
      }
      s.thumbs[result.first]={std::move(result.second),0,slot};
    }catch(const std::exception& e){s.error=e.what();}
  }
}
static int plantThumbnail(const plants::Project& d) {
  auto& s=plantState;auto it=s.thumbs.find(std::to_string(plants::hash(plants::serialize(d).dump())));
  if(it!=s.thumbs.end()) {
    auto& thumb=it->second;
    if(!thumb.handle){
      // Catalogue images outlive individual model previews and profile mounts.
      // Use the same independent UI image path as entity thumbnails.
      auto name="ftebroom/plant-thumb-"+std::to_string(thumb.slot)+".tga";
      auto pixels=plants::tga(thumb.image);
      draw->LoadImageData(name.c_str(),"image/tga",pixels.data(),pixels.size());
      thumb.handle=draw->LoadImage(name.c_str());
    }
    return thumb.handle;
  }
  if(!s.thumbJob.valid()&&!s.job.valid()){
    s.thumbCancel=std::make_shared<std::atomic<bool>>(false);auto cancel=s.thumbCancel;
    const auto photos=d.generator>=3?plantPhotos():nullptr;
    s.thumbJob=std::async(std::launch::async,[d,cancel,photos](){try{auto mesh=plants::generate(d,2,cancel.get());auto maps=plants::textureMaps(d,nullptr,cancel.get(),48,photos);return std::make_pair(std::to_string(plants::hash(plants::serialize(d).dump())),plants::thumbnail(mesh,maps));}catch(...){if(cancel->load())return std::make_pair(std::string{},plants::Image{});throw;}});
  }
  return 0;
}
static void plantVideoChanged(bool resetThumbnails=true) {
  auto& s=plantState;s.dirtyRender=true;s.previewMaps.reset();s.auxiliaryMapsUploaded=false;s.checkerUploaded=false;s.gameAtlasUploaded=false;
  if(resetThumbnails)for(auto& item:s.thumbs)item.second.handle=0;
}
static void plantResumePreview() {
  auto& s=plantState;
  bbLeave([](){auto& s=plantState;bbActive=false;modelViewerOpen=false;modelEditorActive=false;modelEditorCameraEnd();bbRegister();s.owner=true;plantVideoChanged(false);});
}
static void plantShow() {
  auto& s=plantState;
  cvars->GetNVFDG("ftebroom_plant_active","0",0,"Plant Editor preview","ReyondBroom");
  cvars->GetNVFDG("ftebroom_plant_background","0.075 0.09 0.11",0,"Plant Editor background","ReyondBroom");
  if(!s.open){plantLibraryLoad();if(!s.revision)s.saved=plants::serialize(s.doc).dump();s.open=true;s.gamePreview=true;plantSchedule(true);}
  s.hidden=false;plantResumePreview();input->SetMenuFocus(qtrue,"",0,0,1);
}
static void plantReleasePreview() {
  auto& s=plantState;
  if(s.owner){cvars->SetFloat("ftebroom_plant_active",0);cvars->SetFloat("ftebroom_bb_active",0);cvars->SetFloat("ftebroom_modelviewer_open",0);cvars->SetFloat("ftebroom_modeleditor_active",0);if(auto api=bbAPI())api->clear();}s.owner=false;
}
static void plantClose() {
  auto& s=plantState;s.open=false;s.hidden=false;s.closeRequest=false;placementCancel(plantPlacement);if(s.thumbCancel)*s.thumbCancel=true;if(s.work)s.work->cancel=true;s.pending=false;
  plantReleasePreview();
}
static void plantBrushShutdown();
static void plantBrushFromEditor();
static void plantShutdown(){plantBrushShutdown();placementShutdown();if(plantState.work)plantState.work->cancel=true;if(plantState.thumbCancel)*plantState.thumbCancel=true;if(plantState.job.valid())plantState.job.wait();if(plantState.thumbJob.valid())plantState.thumbJob.wait();if(plantState.gameJob.valid())plantState.gameJob.wait();for(const auto& entry:plantState.thumbs)if(entry.second.handle)draw->UnloadImage(entry.second.handle);plantState.thumbs.clear();}
static bool plantBusy(){return plantState.pending||plantState.job.valid();}
static bool plantReady(){return plantState.result&&plantState.result->revision==plantState.revision&&!plantBusy();}
static void plantSave(const std::filesystem::path& file) {
  auto& s=plantState;plants::validate(s.doc);if(std::filesystem::exists(file)&&file!=s.file)throw std::runtime_error("Choose a new project filename; open an existing plant to edit it.");
  std::filesystem::create_directories(file.parent_path());auto portable=plants::serialize(s.doc);
  for(const auto& slot:plantTextureFiles(s.doc))if(!slot.file->empty()){
    auto texture=std::filesystem::u8path(*slot.file);auto bytes=q1Read(texture);std::ostringstream id;id<<slot.stem<<'-'<<std::hex<<plants::hash(bytes)<<texture.extension().u8string();
    auto target=file.parent_path()/std::filesystem::u8path(id.str());
    if(std::filesystem::exists(target)&&q1Read(target)!=bytes)throw std::runtime_error("Imported texture asset already exists with different contents");
    if(!std::filesystem::exists(target))bb::atomicWrite(target,bytes);portable[slot.key]=id.str();
  }
  bb::atomicWrite(file,portable.dump(2));s.file=file;s.saved=plants::serialize(s.doc).dump();s.message="Saved "+file.filename().u8string();
  placementPlantSaved();if(plantReady())s.error.clear();
}
static void plantAction(const std::function<void()>& action){try{action();}catch(const std::exception& e){plantState.error=e.what();}}
static void plantAskSave(std::function<void()> next={},bool choose=false) {
  auto& s=plantState;
  if(!choose&&!s.file.empty()){plantSave(s.file);placementAfterPlantSave(std::move(next));return;}
  std::filesystem::create_directories(plantFolder()/"projects");
  auto initial=s.file.empty()?plantFolder()/"projects"/(plants::assetId(s.doc,0)+".plant.json"):s.file;
  runUiChoose(initial.u8string(),[next](const std::string& p){plantSave(std::filesystem::u8path(p));placementAfterPlantSave(next);},true);
}
static void plantRequest(std::function<void()> next) {
  // The initial preset is not an unsaved document until its editor is opened.
  // Otherwise the first viewport Create queues a confirmation in a hidden
  // Plant Editor, and no window ever opens to display that confirmation.
  auto& s=plantState;if(s.open&&plants::serialize(s.doc).dump()!=s.saved){s.nextAction=std::move(next);s.closeRequest=true;}else next();
}
static void plantOpenFile(const std::filesystem::path& file) {
  if(std::filesystem::file_size(file)>4*1024*1024)throw std::runtime_error("Plant project exceeds 4 MiB");
  auto d=plants::deserialize(plants::Json::parse(q1Read(file)));
  for(const auto& slot:plantTextureFiles(d))if(!slot.file->empty()&&std::filesystem::u8path(*slot.file).is_relative())
    *slot.file=(file.parent_path()/std::filesystem::u8path(*slot.file)).u8string();
  placementCancel(plantPlacement);plantSelect(d);plantState.file=placementSnapshotPath(file)?std::filesystem::path{}:file;plantState.saved=plants::serialize(d).dump();plantState.undo.clear();plantState.redo.clear();
}
static void plantWriteNew(const std::filesystem::path& file,const std::string& bytes) {
  if(std::filesystem::exists(file)){if(q1Read(file)==bytes)return;throw std::runtime_error("Existing asset differs; choose a new export name: "+file.filename().u8string());}
  std::filesystem::create_directories(file.parent_path());bb::atomicWrite(file,bytes);
}
static plants::Textures plantOpaqueMaps(plants::Project d,int profile,const plants::PhotoAtlas* photos) {
  // Opaque runtime geometry uses the original shaped-leaf UVs. Keep bark
  // detail from generator 3, with a smaller atlas for the native skin.
  d.p.foliageMode=2;
  return plantTextureMaps(d,plantReadTextures(d),nullptr,profile==4?128:64,photos);
}
static void plantExport(const std::filesystem::path& selected,ftebroom::plantExport::Format format) {
  auto& s=plantState;if(!plantReady())throw std::runtime_error("Wait for the current plant to finish generating");
  const auto target=ftebroom::plantExport::pathFor(selected,format);
  const auto& spec=ftebroom::plantExport::choice(format);
  auto geometry=s.result->lods[s.lod];auto maps=*s.result->maps;
  if(spec.profile==2||spec.profile==3||spec.profile==4){
    geometry=plants::targetGeometry(s.doc,geometry,spec.profile);
    if(s.doc.generator>=3)maps=plantOpaqueMaps(s.doc,spec.profile,plantPhotos());
  }
  const auto image=target.parent_path()/(target.stem().u8string()+(spec.profile==3?".pcx":"-albedo.tga"));
  auto material=modFileModelPath(image);if(material.empty())material=image.filename().generic_u8string();
  auto bundle=ftebroom::plantExport::encode(format,target.filename().u8string(),geometry,maps,
    spec.profile==2?q1Palette():spec.profile==3?q2Palette():std::string{},material);
  // Check every companion before writing anything. Keep existing exports intact
  // when one texture conflicts; roll back only files created by this attempt.
  for(const auto& [name,bytes]:bundle){auto path=target.parent_path()/std::filesystem::u8path(name);
    if(std::filesystem::exists(path)&&q1Read(path)!=bytes)throw std::runtime_error("Existing asset differs; choose a new export name: "+name);
  }
  std::vector<std::filesystem::path> added;
  try{for(const auto& [name,bytes]:bundle){auto path=target.parent_path()/std::filesystem::u8path(name);
    if(!std::filesystem::exists(path)){added.push_back(path);plantWriteNew(path,bytes);}
  }}catch(...){std::error_code ec;for(const auto& path:added)std::filesystem::remove(path,ec);throw;}
  s.message="Exported "+target.filename().u8string()+" (static pose)";
}
static void placementPublish(const std::map<std::string,std::string>& bundle);
struct PlantRuntimeAsset {
  std::map<std::string,std::string> bundle;
  std::string id,model,source,material,shader;
  plants::V low{},high{};
  double scale=1;
};
struct PlantRuntimeTarget {
  plants::Mesh geometry;
  ftebroom::modelExport::Mesh model;
  ftebroom::modelExport::Skin skin;
  std::shared_ptr<const plants::Image> albedo;
  bool alias=false,trueColorQuake=false,maskedQuake=false;
};
static PlantRuntimeTarget plantPrepareRuntime(const plants::Project& doc,const PlantResult& result,int profile,bool libre,const plants::PhotoAtlas* photos) {
  PlantRuntimeTarget target;
  target.alias=plants::opaquePlantTarget(profile);
  target.trueColorQuake=profile==2&&libre&&doc.generator>=4;
  target.maskedQuake=target.trueColorQuake&&plants::foliagePhoto(doc)!=plants::Photo::None;
  target.geometry=plants::targetGeometry(doc,result.lods[0],profile,target.maskedQuake);
  target.model=plants::targetModel(target.geometry,"plant",profile);
  if(target.alias&&!target.maskedQuake){
    auto opaque=plantOpaqueMaps(doc,profile,photos);
    target.albedo=std::make_shared<plants::Image>(std::move(opaque.albedo));
    target.skin=plants::aliasSkin(*target.albedo);
  }else{
    target.albedo=std::shared_ptr<const plants::Image>(result.maps,&result.maps->albedo);
    if(target.maskedQuake){
      const auto& image=*target.albedo;
      target.skin={unsigned(image.width),unsigned(image.height),image.rgba};
    }
  }
  return target;
}
static bool plantGameCurrent() {
  const auto& s=plantState;
  return s.gameResult.profile==gameProfile&&s.gameResult.libre==(gameProfile==2&&q1UseLibre)&&s.gameResult.revision==s.revision;
}
static void plantGameTick() {
  auto& s=plantState;
  if(s.gameJob.valid()&&s.gameJob.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
    s.gameResult=s.gameJob.get();s.dirtyRender=true;s.gameAtlasUploaded=false;
  }
  if(!s.open||!s.gamePreview||!plantReady()||plantGameCurrent()||s.gameJob.valid())return;
  const int profile=gameProfile;
  const bool libre=profile==2&&q1UseLibre;
  const auto palette=libre?q1Palette():std::string{};
  const auto* photos=s.doc.generator>=3?plantPhotos():nullptr;
  s.gameJob=std::async(std::launch::async,[doc=s.doc,source=s.result,profile,libre,palette,photos](){
    PlantGamePreview out;out.revision=source->revision;out.profile=profile;out.libre=libre;
    try{
      auto target=plantPrepareRuntime(doc,*source,profile,libre,photos);
      for(const auto& surface:target.model.surfaces){out.vertices+=surface.frames[0].size();out.triangles+=surface.triangles.size();}
      out.simplified=target.alias||target.geometry.faces.size()!=source->lods[0].faces.size();
      out.trees=plants::plantTrunkCount(target.geometry);
      if(target.trueColorQuake){
        ftebroom::modelExport::Skin replacement;
        std::vector<ftebroom::modelExport::MdlSkinRegion> regions;
        ftebroom::modelExport::mdl(target.model,{target.skin},palette,target.maskedQuake,&replacement,&regions);
        plants::remapMdlPreview(target.geometry,regions.front(),replacement);
        out.albedo=std::make_shared<plants::Image>(plants::Image{int(replacement.width),int(replacement.height),std::move(replacement.rgba)});
      }else if(target.alias){
        out.albedo=std::make_shared<plants::Image>(plants::Image{int(target.skin.width),int(target.skin.height),std::move(target.skin.rgba)});
      }else out.albedo=std::move(target.albedo);
      out.mesh=std::move(target.geometry);
    }catch(const std::exception& e){out.error=e.what();}
    return out;
  });
}
static PlantRuntimeAsset plantBakeRuntime(plants::Project doc,const PlantResult& result,int profile,
    bool libre,const std::string& palette,const plants::PhotoAtlas* photos) {
  auto target=plantPrepareRuntime(doc,result,profile,libre,photos);
  const auto& geometry=target.geometry;
  auto& mesh=target.model;
  const bool alias=target.alias,trueColorQuake=target.trueColorQuake,maskedQuake=target.maskedQuake;
  PlantRuntimeAsset out;out.low=geometry.low;out.high=geometry.high;
  if(profile==0){double extent=0;for(auto& surface:mesh.surfaces)for(auto& vertex:surface.frames[0])for(float x:vertex.position)extent=std::max(extent,std::abs(double(x)));out.scale=std::max(1.,extent/450.);for(auto& surface:mesh.surfaces)for(auto& vertex:surface.frames[0])for(auto& x:vertex.position)x/=out.scale;}
  auto atlas=plants::tga(*target.albedo);
  auto& skin=target.skin;
  std::string identity;
  ftebroom::modelExport::Skin replacementSkin;
  if(profile==2)identity=ftebroom::modelExport::mdl(mesh,{skin},palette,maskedQuake,
    trueColorQuake?&replacementSkin:nullptr);
  else if(profile==3)identity=ftebroom::modelExport::md2(mesh,{skin},palette,"plant.pcx").model;
  else if(profile==4)identity=ftebroom::studioExport::bake(mesh,{skin},1);
  else if(profile==1)identity=ftebroom::modelExport::iqm(mesh);
  else identity=ftebroom::modelExport::md3(mesh);
  std::ostringstream key;key<<"plant-"<<std::hex<<plants::hash("authoring-target-v5:"+plants::serialize(doc).dump()+identity+atlas+palette);
  out.id=key.str()+(profile==0?"-q3":profile==2?"-q1":profile==3?"-q2":profile==4?"-hl":"-fte");
  out.material="textures/reyond-plugins/"+out.id;out.model="models/reyond-plugins/"+out.id+plants::targetExtension(profile);out.source="models/reyond-plugins/"+out.id+".plant.json";
  plants::Json dependencies=plants::Json::array();
  if(alias){
    if(profile==2||profile==4)out.bundle[out.model]=std::move(identity);
    else {auto skinName="models/reyond-plugins/"+out.id+".pcx";auto output=ftebroom::modelExport::md2(mesh,{skin},palette,skinName);out.bundle[out.model]=std::move(output.model);out.bundle[skinName]=std::move(output.skin);dependencies.push_back(skinName);}
  }else{
    for(auto& surface:mesh.surfaces)surface.material=out.material;
    out.bundle[out.model]=profile==1?ftebroom::modelExport::iqm(mesh):ftebroom::modelExport::md3(mesh);
    auto image=out.material+(profile==1?".dds":".tga");out.bundle[image]=profile==1?plants::coverageDds(result.maps->albedo):atlas;dependencies.push_back(image);
    if(profile==1){out.bundle[out.material+".tga"]=atlas;dependencies.push_back(out.material+".tga");}
    const bool wind=profile==1&&doc.runtimeWind;
    out.shader=plants::shader(out.material,image,profile!=0,wind);
    auto script="scripts/"+out.id+".shader";out.bundle[script]=out.shader;dependencies.push_back(script);
    if(wind){
      const std::string program="glsl/reyond_plant_wind_v1.glsl";
      out.bundle[program]=plants::windProgramSource();dependencies.push_back(program);
    }
  }
  if(trueColorQuake){
    // FTE loads this optional replacement for skin 0 of a classic MDL.
    auto path=out.model+"_0.png";
    out.bundle[path]=plants::png({int(replacementSkin.width),int(replacementSkin.height),std::move(replacementSkin.rgba)});
    dependencies.push_back(path);
  }
  auto portable=plants::serialize(doc);dependencies.push_back(out.source);
  for(const auto& slot:plantTextureFiles(doc))if(!slot.file->empty()){
    auto original=std::filesystem::u8path(*slot.file);
    auto name=out.id+"-"+slot.stem+original.extension().u8string(),dependency="models/reyond-plugins/"+name;
    out.bundle[dependency]=q1Read(original);dependencies.push_back(dependency);portable[slot.key]=name;
  }
  out.bundle[out.source]=portable.dump(2);
  size_t vertices=0,triangles=0;for(auto& surface:mesh.surfaces){vertices+=surface.frames[0].size();triangles+=surface.triangles.size();}
  out.bundle[out.model+".reyond.json"]=plants::Json{{"kind","reyondbroom-authoring-model"},{"version",2},{"low",out.low},{"high",out.high},{"normalization",out.scale},{"dependencies",dependencies},{"plantProject",portable},
    // This immutable asset records format capabilities, not the scene entity's
    // FTE opt-in. Toggling that option must reuse the same model and manifest.
    {"target",{{"profile",profile},{"format",plants::targetExtension(profile)},{"vertices",vertices},{"triangles",triangles},{"vertexLimit",alias?plants::aliasPlantVertices(profile):0},{"triangleLimit",alias?plants::aliasPlantTriangles(profile):0},{"runtimeLod",alias||geometry.faces.size()!=result.lods[0].faces.size()},{"opaqueGeometricFoliage",alias&&!maskedQuake},{"fteMaskedSkin",maskedQuake},{"coverageMipmaps",profile==1},{"vanillaRequiresCompatibleGameCode",profile==2||profile==3},{"fteStaticModelExtensionAvailable",profile==2}}}}.dump(2);
  return out;
}
static PlantRuntimeAsset plantBakeRuntime() {
  auto& s=plantState;
  if(!plantReady())throw std::runtime_error("Wait for the current plant to finish generating");
  const auto palette=gameProfile==2?q1Palette():gameProfile==3?q2Palette():std::string{};
  return plantBakeRuntime(s.doc,*s.result,gameProfile,q1UseLibre,palette,s.doc.generator>=3?plantPhotos():nullptr);
}
static void plantAddToMap() {
  if(plantPlacement.active){placementPlantSaved();return;}
  auto& s=plantState;if(!plantReady())throw std::runtime_error("Wait for the current plant to finish generating");
  auto asset=plantBakeRuntime();placementPublish(asset.bundle);
  if(!asset.shader.empty()){s.pendingShaders[asset.id]={asset.material,asset.shader.substr(asset.shader.find('{'))};command(("ftebroom_plant shader "+asset.id+"\n").c_str());}
  auto path=documentPath(asset.model);auto view=viewRects[viewFocus];
  auto vector=[](plants::V v){std::ostringstream out;out.imbue(std::locale::classic());out<<v[0]<<' '<<v[1]<<' '<<v[2];return out.str();};
  auto action="ftebroom_model_drop "+organizationQuote(asset.model)+" "+std::to_string(view.x+view.w*.5f)+" "+std::to_string(view.y+view.h*.5f)+" "+organizationQuote(plants::targetClass(gameProfile))+" authoring "+organizationQuote(vector(asset.low))+" "+organizationQuote(vector(asset.high))+" "+organizationQuote(s.allowFteQ1?"fte-q1":"")+" "+std::to_string(asset.scale)+" "+organizationQuote(asset.source)+"\n";
  command(action.c_str());s.lastModel=path;s.message="Placed target-format model in map — Undo removes the entity; assets remain reusable.";
  if(gameProfile==2||gameProfile==3)s.message+=" Runtime placement requires compatible game code; Quake 1 may explicitly use the FTE static-model extension.";
}
static void plantPresetSave() {
  auto& s=plantState;auto id=plants::assetId(s.doc,0);auto path=s.userFiles.count(s.doc.preset)?s.userFiles[s.doc.preset]:plantFolder()/"presets"/(id+".json");
  auto doc=s.doc;doc.preset="user:"+path.filename().u8string();bb::atomicWrite(path,plants::serialize(doc).dump(2));s.doc.preset=doc.preset;plantLibraryLoad();s.message="Saved editable user preset";
}
static bool plantBeforeDocument(const std::string& next) {
  auto& s=plantState;if(!documentIsExit(next)||!s.open||plants::serialize(s.doc).dump()==s.saved)return false;
  plantRequest([next](){plantClose();documentRequest(next);});return true;
}
static void QDECL plantCommand() {
  char action[64]={},argument[160]={};commands->Argv(1,action,sizeof action);commands->Argv(2,argument,sizeof argument);
  plantAction([&](){
    if(std::string(action)=="qa-assets") {
      if(!plantState.qa||!cvars->GetFloat("ftebroom_skip_setup"))throw std::runtime_error("Asset QA requires a disposable plant session");
      auto source=q1Read(documentPath("data/export-map.map"));auto models=ftebroom::generatedIvyModels(source);
      if(models.empty())throw std::runtime_error("No placed plant in the reopened map");
      auto folder=documentPath("plant-qa-build");stageGeneratedModels(source,folder);size_t buildAdapted=0;bb::atomicWrite(folder/"plant.map",ftebroom::prepareStaticModels(source,gameProfile,buildAdapted));
      auto entities=ftebroom::rawPreviewEntities(source);std::string classic;
      for(auto& e:entities){if(e.get("_reyondbroom_authoring")=="1"){e.fields["classname"]="func_model";e.fields["_reyondbroom_fte_q1"]="1";}classic+="{\n";for(auto& f:e.fields)classic+=ftebroom::prefab::quote(f.first)+" "+ftebroom::prefab::quote(f.second)+"\n";classic+=e.geometry+"}\n";}
      size_t migrated=0;auto adapted=ftebroom::prepareStaticModels(classic,2,migrated);
      if(migrated!=models.size()||adapted.find("misc_model")==adapted.npos)throw std::runtime_error("Quake 1 static-model adaptation failed");
      bb::atomicWrite(folder/"q1-fte-static.map",adapted);
      bb::atomicWrite(documentPath("plant-asset-qa.json"),plants::Json{{"models",models},{"q1_migrated",migrated},{"staged",folder.u8string()}}.dump(2));return;
    }
    if(std::string(action)=="shader") {
      auto& pending=plantState.pendingShaders;auto found=pending.find(argument);
      if(found!=pending.end()){draw->LoadImageShader(found->second.first.c_str(),found->second.second.c_str());pending.erase(found);}return;
    }
    if(std::string(action)=="qa") {
      if(!cvars->GetFloat("ftebroom_skip_setup"))throw std::runtime_error("Plant QA requires a disposable skip-setup runtime");
      plantState.qa=true;uiScaleChoice=0;if(std::string(argument)=="light")uiTheme=3;else if(std::string(argument)=="dark")uiTheme=0;uiStyle();plantShow();return;
    }
    if(std::string(action)=="preset") {
      plantShow();auto& library=plantState.library;auto it=std::find_if(library.begin(),library.end(),[&](auto& d){return d.preset==argument;});
      if(it==library.end())throw std::runtime_error("Unknown plant preset");plantSelect(*it);return;
    }
    plantShow();
  });
}
#include "ftebroom_plant_ui.h"
