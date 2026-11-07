// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "plant/export.h"
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
struct PlantState {
  bool open=false,owner=false,advanced=false,dirtyRender=true,framePending=true,ortho=false,wind=false,uv=false;
  bool floor=true,axes=false,handles=false,qa=false,closeRequest=false,allowFteQ1=false;
  int lod=0,display=0,selected=1,category=-1,filter=0,thumbSlot=0;
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
  std::shared_ptr<PlantWork> work;
  std::shared_ptr<PlantResult> result;
  uint64_t revision=0;
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
static std::string plantTextureKey(const plants::Project& d) {
  auto& p=d.p;return plants::Json{{"generator",d.generator},{"barkPattern",p.barkPattern},{"serration",p.serration},{"needleForm",p.needleForm},{"evergreen",p.evergreen},{"leaflets",p.leaflets},{"organ",p.organ},{"seed",d.seed},{"species",d.species},{"family",int(d.family)},{"bark",p.bark},{"leaf",p.leaf},{"flower",p.flower},
    {"shape",p.leafShape},{"profile",p.leafProfile},{"season",p.season},{"quality",p.quality},{"dryness",p.dryness},{"roughness",p.roughness},{"ao",p.ao},{"variation",p.variation},{"ribs",p.ribs},{"gills",p.gills},{"override",d.texture}}.dump();
}
static std::string plantGeometryKey(const plants::Project& d) {
  auto p=plants::parameters(d.p);
  for(auto key:{"bark","leaf","flower","roughness","ao","variation","dryness","windStrength","windDirection","windFrequency","stiffness","flutter","barkPattern","serration"})p.erase(key);
  return plants::Json{{"generator",d.generator},{"parameters",p},{"family",int(d.family)},{"species",d.species},{"seed",d.seed}}.dump();
}
static void plantSchedule(bool immediate=false) {
  auto& s=plantState;s.revision++;s.pending=true;s.nextGenerate=ImGui::GetTime()+(immediate?0:.20);
  if(s.work)s.work->cancel=true;if(s.thumbCancel)*s.thumbCancel=true;s.error.clear();
}
static void plantChanged(const plants::Json& before) {
  auto& s=plantState;auto active=ImGui::GetActiveID();
  if(!active||active!=s.gesture){s.undo.push_back(before);if(s.undo.size()>100)s.undo.erase(s.undo.begin());}
  s.gesture=active;s.redo.clear();plantSchedule();
}
static void plantUndo(bool redo=false) {
  auto& s=plantState;auto& from=redo?s.redo:s.undo;auto& to=redo?s.undo:s.redo;
  if(from.empty())return;to.push_back(plants::serialize(s.doc));s.doc=plants::deserialize(from.back());from.pop_back();s.gesture=0;plantSchedule(true);
}
static void plantSelect(const plants::Project& doc) {
  auto& s=plantState;auto before=plants::serialize(s.doc);s.doc=doc;s.file.clear();s.selected=std::min<int>(1,doc.nodes.size()-1);s.framePending=true;
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
    plants::Image imported;
    if(!doc.texture.empty())try{auto bytes=q1Read(std::filesystem::u8path(doc.texture));key+=std::to_string(plants::hash(bytes));auto decoded=modelImportDecode(bytes);imported={int(decoded.width),int(decoded.height),std::move(decoded.rgba)};}
      catch(const std::exception& e){s.error=e.what();return;}
    s.work=std::make_shared<PlantWork>();auto work=s.work;
    s.job=std::async(std::launch::async,[doc,revision,key,geometryKey,cached,work,imported=std::move(imported)]()mutable {
      PlantResult out;out.revision=revision;out.textureKey=key;out.geometryKey=geometryKey;auto start=std::chrono::steady_clock::now();
      try {
        for(int i=0;i<3;i++){
          work->stage=i;if(work->cancel.load())throw std::runtime_error("Plant generation cancelled");
          out.baseLods[i]=cached&&cached->geometryKey==geometryKey?cached->baseLods[i]:plants::generateBase(doc,i,&work->cancel);
          out.lods[i]=plants::hierarchy(out.baseLods[i],doc,i);
          if(out.lods[i].faces.empty())out.lods[i].low=out.lods[i].high={0,0,0};else plants::validate(out.lods[i]);
        }
        work->stage=3;
        if(cached&&cached->textureKey==key)out.maps=cached->maps;
        else out.maps=std::make_shared<plants::Textures>(plants::textureMaps(doc,imported.width?&imported:nullptr,&work->cancel));
      }catch(const std::exception& e){out.error=e.what();}
      out.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return out;
    });
  }
  if(s.thumbJob.valid()&&s.thumbJob.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
    try {auto result=s.thumbJob.get();if(result.first.empty())return;int slot=s.thumbSlot++%160;
      for(auto it=s.thumbs.begin();it!=s.thumbs.end();)if(it->second.slot==slot)it=s.thumbs.erase(it);else ++it;
      s.thumbs[result.first]={std::move(result.second),0,slot};
    }catch(const std::exception& e){s.error=e.what();}
  }
}
static int plantThumbnail(const plants::Project& d) {
  auto& s=plantState;auto it=s.thumbs.find(std::to_string(plants::hash(plants::serialize(d).dump())));
  if(it!=s.thumbs.end()) {
    if(!it->second.handle)if(auto api=bbImageAPI()){auto& t=it->second;auto name="bbtex/plant-thumb-"+std::to_string(t.slot)+".tga";t.handle=api->upload(name.c_str(),t.image.rgba.data(),t.image.width,t.image.height);}
    return it->second.handle;
  }
  if(!s.thumbJob.valid()&&!s.job.valid()){
    s.thumbCancel=std::make_shared<std::atomic<bool>>(false);auto cancel=s.thumbCancel;
    s.thumbJob=std::async(std::launch::async,[d,cancel](){try{auto mesh=plants::generate(d,2,cancel.get());auto maps=plants::textureMaps(d,nullptr,cancel.get(),48);return std::make_pair(std::to_string(plants::hash(plants::serialize(d).dump())),plants::thumbnail(mesh,maps));}catch(...){if(cancel->load())return std::make_pair(std::string{},plants::Image{});throw;}});
  }
  return 0;
}
static void plantVideoChanged() {
  auto& s=plantState;s.dirtyRender=true;for(auto& item:s.thumbs)item.second.handle=0;
}
static void plantResumePreview() {
  auto& s=plantState;
  bbLeave([](){auto& s=plantState;bbActive=false;modelViewerOpen=false;modelEditorActive=false;modelEditorCameraEnd();bbRegister();s.owner=true;plantVideoChanged();});
}
static void plantShow() {
  auto& s=plantState;
  cvars->GetNVFDG("ftebroom_plant_active","0",0,"Plant Editor preview","ReyondBroom");
  cvars->GetNVFDG("ftebroom_plant_background","0.075 0.09 0.11",0,"Plant Editor background","ReyondBroom");
  if(!s.open){plantLibraryLoad();if(!s.revision)s.saved=plants::serialize(s.doc).dump();s.open=true;plantSchedule(true);}
  plantResumePreview();input->SetMenuFocus(qtrue,"",0,0,1);
}
static void plantClose() {
  auto& s=plantState;s.open=false;s.closeRequest=false;placementCancel(plantPlacement);if(s.thumbCancel)*s.thumbCancel=true;if(s.work)s.work->cancel=true;s.pending=false;
  if(s.owner){cvars->SetFloat("ftebroom_plant_active",0);cvars->SetFloat("ftebroom_bb_active",0);cvars->SetFloat("ftebroom_modelviewer_open",0);cvars->SetFloat("ftebroom_modeleditor_active",0);if(auto api=bbAPI())api->clear();}s.owner=false;
}
static void plantShutdown(){placementShutdown();if(plantState.work)plantState.work->cancel=true;if(plantState.thumbCancel)*plantState.thumbCancel=true;if(plantState.job.valid())plantState.job.wait();if(plantState.thumbJob.valid())plantState.thumbJob.wait();}
static bool plantBusy(){return plantState.pending||plantState.job.valid();}
static bool plantReady(){return plantState.result&&plantState.result->revision==plantState.revision&&!plantBusy();}
static void plantSave(const std::filesystem::path& file) {
  auto& s=plantState;plants::validate(s.doc);if(std::filesystem::exists(file)&&file!=s.file)throw std::runtime_error("Choose a new project filename; open an existing plant to edit it.");
  std::filesystem::create_directories(file.parent_path());auto portable=plants::serialize(s.doc);
  if(!s.doc.texture.empty()){
    auto texture=std::filesystem::u8path(s.doc.texture);auto bytes=q1Read(texture);std::ostringstream id;id<<"leaf-"<<std::hex<<plants::hash(bytes)<<texture.extension().u8string();
    auto target=file.parent_path()/std::filesystem::u8path(id.str());
    if(std::filesystem::exists(target)&&q1Read(target)!=bytes)throw std::runtime_error("Imported texture asset already exists with different contents");
    if(!std::filesystem::exists(target))bb::atomicWrite(target,bytes);portable["texture"]=id.str();
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
  auto& s=plantState;if(plants::serialize(s.doc).dump()!=s.saved){s.nextAction=std::move(next);s.closeRequest=true;}else next();
}
static void plantOpenFile(const std::filesystem::path& file) {
  if(std::filesystem::file_size(file)>4*1024*1024)throw std::runtime_error("Plant project exceeds 4 MiB");
  auto d=plants::deserialize(plants::Json::parse(q1Read(file)));
  if(!d.texture.empty()&&std::filesystem::u8path(d.texture).is_relative())d.texture=(file.parent_path()/std::filesystem::u8path(d.texture)).u8string();
  placementCancel(plantPlacement);plantSelect(d);plantState.file=placementSnapshotPath(file)?std::filesystem::path{}:file;plantState.saved=plants::serialize(d).dump();plantState.undo.clear();plantState.redo.clear();
}
static void plantWriteNew(const std::filesystem::path& file,const std::string& bytes) {
  if(std::filesystem::exists(file)){if(q1Read(file)==bytes)return;throw std::runtime_error("Existing asset differs; choose a new export name: "+file.filename().u8string());}
  std::filesystem::create_directories(file.parent_path());bb::atomicWrite(file,bytes);
}
static void plantExport(const std::filesystem::path& target) {
  auto& s=plantState;if(!plantReady())throw std::runtime_error("Wait for the current plant to finish generating");
  auto& mesh=s.result->lods[s.lod];auto& maps=*s.result->maps;auto ext=entityLower(target.extension().u8string());
  plants::validate(mesh);
  if(ext==".glb")plantWriteNew(target,plants::glb(mesh,maps));
  else if(ext==".obj") {
    auto stem=target.stem().u8string();auto folder=target.parent_path();
    plantWriteNew(folder/(stem+"-albedo.png"),plants::png(maps.albedo));
    plantWriteNew(folder/(stem+"-normal.png"),plants::png(maps.normal));
    plantWriteNew(folder/(stem+"-orm.png"),plants::png(maps.orm));
    plantWriteNew(folder/(stem+".mtl"),"newmtl plant\nKd 1 1 1\nd 1\nmap_Kd "+stem+"-albedo.png\nmap_d "+stem+"-albedo.png\nnorm "+stem+"-normal.png\n");
    plantWriteNew(target,plants::obj(mesh,stem+".mtl"));
  }else throw std::runtime_error("Choose .obj or .glb for a portable plant export");
  s.message="Exported "+target.filename().u8string()+" (static wind fallback)";
}
static void plantAddToMap() {
  if(plantPlacement.active){placementPlantSaved();return;}
  auto& s=plantState;if(!plantReady())throw std::runtime_error("Wait for the current plant to finish generating");
  if(gameProfile==3||gameProfile==4)throw std::runtime_error("This profile requires MD2 or studio MDL. Export OBJ, convert with Model Editor, then place its compatible model.");
  if(gameProfile==2&&!s.allowFteQ1)throw std::runtime_error("Enable the explicit FTE static-model extension for Quake 1, or export and convert to MDL for a compatible mod.");
  auto id=plants::assetId(s.doc,s.lod)+(gameProfile==0?"-q3":"-fte"),material="textures/reyond-plugins/"+id,model="models/reyond-plugins/"+id+".md3",script="scripts/"+id+".shader";
  auto& mesh=s.result->lods[s.lod];auto bytes=plants::md3(mesh,material); // Validate before writing anything.
  auto source=documentPath("ftebroom.cfg").parent_path();std::vector<std::filesystem::path> destinations{source};
  // FTE may have an authoring asset directory without a separate runtime mod.
  // Match plugin asset installation: both configured roots must see new models.
  auto addRoot=[&](const std::filesystem::path& root){if(root.empty()||!std::filesystem::is_directory(root))return;auto canonical=std::filesystem::weakly_canonical(root);if(std::none_of(destinations.begin(),destinations.end(),[&](auto& path){return std::filesystem::weakly_canonical(path)==canonical;}))destinations.push_back(root);};
  addRoot(modFileRoot());if(gameProfile==1)addRoot(std::filesystem::u8path(baseq3));
  plants::Json metadata={{"kind","reyondbroom-authoring-model"},{"low",mesh.low},{"high",mesh.high},{"dependencies",{material+".tga",script}},{"plantProject",plants::serialize(s.doc)}};
  for(auto& root:destinations) {
    plantWriteNew(root/std::filesystem::u8path(material+".tga"),plants::tga(s.result->maps->albedo));
    plantWriteNew(root/std::filesystem::u8path(script),plants::shader(material,material+".tga",gameProfile!=0));
    auto list=root/"scripts/shaderlist.txt";std::string shaderNames;
    if(std::filesystem::is_regular_file(list))shaderNames=q1Read(list);
    else {size_t size=0;auto data=static_cast<char*>(files->LoadFile("scripts/shaderlist.txt",&size));if(data){shaderNames.assign(data,size);core->Free(data);}}
    bb::atomicWrite(list,ftebroom::appendShaderList(shaderNames,{id}));
    plantWriteNew(root/std::filesystem::u8path(model),bytes);
    plantWriteNew(root/std::filesystem::u8path(model+".reyond.json"),metadata.dump(2));
  }
  auto shader=plants::shader(material,material+".tga",gameProfile!=0);shader=shader.substr(shader.find('{'));
  s.pendingShaders[id]={material,shader};
  auto path=source/std::filesystem::u8path(model);auto view=viewRects[viewFocus];
  auto vector=[](plants::V v){std::ostringstream out;out.imbue(std::locale::classic());out<<v[0]<<' '<<v[1]<<' '<<v[2];return out.str();};
  auto action="ftebroom_model_drop "+organizationQuote(model)+" "+std::to_string(view.x+view.w*.5f)+" "+std::to_string(view.y+view.h*.5f)+" "+organizationQuote("func_model")+" authoring "+organizationQuote(vector(mesh.low))+" "+organizationQuote(vector(mesh.high))+(s.allowFteQ1?" fte-q1":"")+"\n";
  command(("fs_flush\nftebroom_plant shader "+id+"\n"+action).c_str());s.lastModel=path;s.message="Placed model in map — Undo removes the entity; assets remain reusable.";
}
static void plantPresetSave() {
  auto& s=plantState;auto id=plants::assetId(s.doc,0);auto path=s.userFiles.count(s.doc.preset)?s.userFiles[s.doc.preset]:plantFolder()/"presets"/(id+".json");
  auto doc=s.doc;doc.preset="user:"+path.filename().u8string();bb::atomicWrite(path,plants::serialize(doc).dump(2));s.doc.preset=doc.preset;plantLibraryLoad();s.message="Saved editable user preset";
}
static bool plantBeforeDocument(const std::string& next) {
  auto& s=plantState;if(next!="exit"||!s.open||plants::serialize(s.doc).dump()==s.saved)return false;
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
