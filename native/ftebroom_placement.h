// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <tuple>
static uint64_t placementPlantRevision=0;
static bool placementPlantPending=false;
static std::function<void()> placementPlantNext;
static std::string placementVector(plants::V v){std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9)<<v[0]<<' '<<v[1]<<' '<<v[2];return out.str();}
static void placementValid(AssetPlacement& p){
  if(!p.active)return;
  if(p.epoch!=organizationMapEpoch||p.profile!=gameProfile||p.sourceRoot!=std::string(baseq3)||p.runtimeRoot!=gameRuntime()||p.assetRoot!=documentPath("ftebroom.cfg").parent_path().u8string()){p={};throw std::runtime_error("Placement cancelled: the map or game profile changed. The source project is saved.");}
  if(p.awaiting)throw std::runtime_error("Wait for the map to finish the previous Save.");
}
static void placementPublish(const std::map<std::string,std::string>& bundle){
  auto source=documentPath("ftebroom.cfg").parent_path();std::vector<std::filesystem::path> roots{source};
  for(auto root:{modFileRoot(),gameProfile==1?std::filesystem::u8path(baseq3):std::filesystem::path{}}){
    if(root.empty()||!std::filesystem::is_directory(root))continue;
    auto canonical=std::filesystem::weakly_canonical(root);
    if(std::none_of(roots.begin(),roots.end(),[&](auto& r){return std::filesystem::weakly_canonical(r)==canonical;}))roots.push_back(root);
  }
  // Verify the complete bundle before publishing any immutable content.
  for(auto& root:roots)for(auto& [name,bytes]:bundle){auto path=root/std::filesystem::u8path(name);if(std::filesystem::exists(path)&&q1Read(path)!=bytes)throw std::runtime_error("Asset hash collision or modified immutable asset: "+name);}
  std::set<std::string> shaderNames;
  for(auto& [name,bytes]:bundle)if(name.rfind("scripts/",0)==0&&std::filesystem::u8path(name).extension()==".shader")shaderNames.insert(std::filesystem::u8path(name).stem().u8string());
  std::vector<std::filesystem::path> added;
  std::vector<std::tuple<std::filesystem::path,bool,std::string>> lists;
  try{
    for(bool manifest:{false,true})for(auto& root:roots)for(auto& [name,bytes]:bundle){if((name.size()>12&&name.substr(name.size()-12)==".reyond.json")!=manifest)continue;auto path=root/std::filesystem::u8path(name);if(std::filesystem::exists(path))continue;plantWriteNew(path,bytes);added.push_back(path);}
    for(auto& root:roots)if(!shaderNames.empty()){
      auto list=root/"scripts/shaderlist.txt";std::string previous;
      if(std::filesystem::is_regular_file(list))previous=q1Read(list);
      else{size_t size=0;auto data=static_cast<char*>(files->LoadFile("scripts/shaderlist.txt",&size));if(data){previous.assign(data,size);core->Free(data);}}
      auto next=ftebroom::appendShaderList(previous,shaderNames);if(next!=previous){lists.emplace_back(list,std::filesystem::exists(list),previous);bb::atomicWrite(list,next);}
    }
    command("fs_flush\n");
  }catch(...){for(auto i=lists.rbegin();i!=lists.rend();++i){try{if(std::get<1>(*i))bb::atomicWrite(std::get<0>(*i),std::get<2>(*i));else{std::error_code ec;std::filesystem::remove(std::get<0>(*i),ec);}}catch(...){}}for(auto& path:added){std::error_code ec;std::filesystem::remove(path,ec);}throw;}
}
static void placementSend(AssetPlacement& p,const std::string& model,const std::string& source,plants::V low,plants::V high,double scale,bool optin){
  placementValid(p);if(!p.active)return;
  auto origin=p.point;double support=0;for(int i=0;i<3;i++)support+=p.normal[i]*(p.normal[i]>=0?low[i]:high[i]);
  for(int i=0;i<3;i++)origin[i]-=p.normal[i]*support;
  if(p.grid>0){auto delta=plants::sub(origin,p.gridOrigin);origin=p.gridOrigin;for(auto& axis:p.basis)origin=plants::add(origin,plants::mul(axis,std::round(plants::dot(delta,axis)/p.grid)*p.grid));}
  auto correction=plants::dot(p.point,p.normal)-plants::dot(origin,p.normal)-support;
  origin=plants::add(origin,plants::mul(p.normal,correction));
  p.request=p.id+"-"+std::to_string(++placementSerial);
  std::string q="ftebroom_authoring_save "+organizationQuote(p.request)+" "+std::to_string(p.epoch)+" "+organizationQuote(p.id)+" "+organizationQuote(model)+" "+organizationQuote(source)+" "+(p.created?"0":"1")+" "+organizationQuote(placementVector(origin))+" "+std::to_string(p.layer)+" "+(optin?"1":"0")+" "+std::to_string(scale)+"\n";
  p.created=true;p.awaiting=true;p.runtimeSupported=p.profile==0||p.profile==1||(p.profile==2&&optin);command(q.c_str());
}
static bool placementSnapshotPath(const std::filesystem::path& file){
  auto name=file.filename().u8string();std::string stem;
  if(name.size()>8&&name.substr(name.size()-8)==".bbmodel")stem=name.substr(0,name.size()-8);
  else if(name.size()>11&&name.substr(name.size()-11)==".plant.json")stem=name.substr(0,name.size()-11);
  else return false;
  auto manifest=file.parent_path()/std::filesystem::u8path(stem+".md3.reyond.json");
  if(!std::filesystem::is_regular_file(manifest)||std::filesystem::file_size(manifest)>4*1024*1024)return false;
  try{auto j=plants::Json::parse(q1Read(manifest));if(j.value("kind","")!="reyondbroom-authoring-model"||!j.contains("dependencies"))return false;for(auto& dependency:j["dependencies"])if(std::filesystem::u8path(dependency.get<std::string>()).filename()==file.filename())return true;}catch(const std::exception&){}return false;
}
static void placementCreate(bool plant){
  auto captured=contextPlacement;
  // Guards run before installing the new binding, so Save in a guard always
  // belongs to the previous document and cannot use this click by mistake.
  plantRequest([captured,plant](){bbLeave([captured,plant](){
    if(captured.epoch!=organizationMapEpoch||captured.profile!=gameProfile||captured.sourceRoot!=std::string(baseq3)||captured.runtimeRoot!=gameRuntime())return;
    bbSavedStamp=bbHistory.state();bbSaved=bb::serialize(bbProject,bbEmbed);
    if(plant){plantSelect(plants::presets().front());plantPlacement=captured;plantState.saved=plants::serialize(plantState.doc).dump();plantState.undo.clear();plantState.redo.clear();plantShow();}
    else{bbNew();modelNewPlacement=captured;}
  });});
}
static double placementNormalize(ftebroom::modelExport::Mesh& mesh,plants::V& low,plants::V& high){
  low={1e30,1e30,1e30};high={-1e30,-1e30,-1e30};double extent=0;
  for(auto& surface:mesh.surfaces)for(auto& frame:surface.frames)for(auto& v:frame)for(int a=0;a<3;a++){low[a]=std::min(low[a],double(v.position[a]));high[a]=std::max(high[a],double(v.position[a]));extent=std::max(extent,std::abs(double(v.position[a])));}
  double scale=std::max(1.,extent/450.);for(auto& surface:mesh.surfaces)for(auto& frame:surface.frames)for(auto& v:frame)for(auto& x:v.position)x/=scale;return scale;
}
struct PlacementModelResult{std::map<std::string,std::string> bundle;plants::V low{},high{};double scale=1;bool optin=false;std::string model,source,error;std::map<std::string,std::pair<std::string,std::string>> shaders;};
static std::future<PlacementModelResult> placementModelJob;
static std::shared_ptr<std::atomic<bool>> placementModelCancel;
static std::shared_ptr<bb::Project> placementModelPending;
static unsigned placementModelGeneration=0,placementModelRunning=0;
static std::function<void()> placementModelNext;
static std::string placementModelBinding;
static PlacementModelResult placementBakeModel(const bb::Project& project,int profile,const std::atomic<bool>* cancel){
  PlacementModelResult result;
  try{
  auto baked=bb::bake(project,true,cancel);plants::V low,high;auto scale=placementNormalize(baked.mesh,low,high);
  auto sourceBytes=bb::serialize(project,true);auto identity=baked.mesh;for(auto& surface:identity.surfaces)surface.material="model";std::ostringstream key;key<<"model-"<<std::hex<<plants::hash("authoring-v1:"+sourceBytes+ftebroom::modelExport::md3(identity));
  auto id=key.str(),model="models/reyond-plugins/"+id+".md3",source="models/reyond-plugins/"+id+".bbmodel";
  std::map<std::string,std::string> bundle;plants::Json deps=plants::Json::array();
  for(size_t i=0;i<baked.textures.size();i++){
    bb::Texture texture;int n=baked.textures[i];
    if(n>=0&&size_t(n)<project.textures.size())texture=project.textures[n];
    else{texture.width=texture.height=16;texture.rgba.assign(16*16*4,180);for(size_t a=3;a<texture.rgba.size();a+=4)texture.rgba[a]=255;}
    auto material="textures/reyond-plugins/"+id+"-"+std::to_string(i)+".tga";
    auto shaderMaterial=material.substr(0,material.size()-4),script="scripts/"+id+"-"+std::to_string(i)+".shader";
    baked.mesh.surfaces[i].material=shaderMaterial;bundle[material]=bb::tga(texture);deps.push_back(material);
    auto shader=plants::shader(shaderMaterial,material,profile!=0);bundle[script]=shader;deps.push_back(script);
  
  }
  bundle[source]=sourceBytes;deps.push_back(source);bundle[model]=ftebroom::modelExport::md3(baked.mesh);
  bundle[model+".reyond.json"]=plants::Json{{"kind","reyondbroom-authoring-model"},{"version",1},{"low",low},{"high",high},{"normalization",scale},{"dependencies",deps}}.dump(2);
  for(size_t i=0;i<baked.textures.size();i++){auto name=id+"-"+std::to_string(i);auto material=baked.mesh.surfaces[i].material;auto shader=bundle.at("scripts/"+name+".shader");result.shaders[name]={material,shader.substr(shader.find('{'))};}
  result.optin=project.root["reyondbroom"].value("fte_static_model",false);result.bundle=std::move(bundle);result.model=model;result.source=source;result.low=low;result.high=high;result.scale=scale;
  }catch(const std::exception& e){result.error=e.what();}return result;
}
static void placementModelSaved(){
  if(!modelPlacement.active)return;placementValid(modelPlacement);
  if(placementModelCancel)*placementModelCancel=true;
  placementModelPending=std::make_shared<bb::Project>(bbProject);++placementModelGeneration;
  bbMessage="Project saved; preparing the linked model assets in the background.";
}
static void placementAfterModelSave(std::function<void()> next){
  if(!next)return;if(placementModelPending||placementModelJob.valid()||modelPlacement.awaiting){placementModelNext=std::move(next);return;}next();
}
static void placementShutdown(){
  if(placementModelCancel)*placementModelCancel=true;
  placementModelPending={};placementModelNext={};if(placementModelJob.valid())placementModelJob.wait();
}
static void placementPlantSaved(){
  auto& p=plantPlacement;auto& s=plantState;if(!p.active)return;placementValid(p);
  if(!plantReady()){placementPlantRevision=s.revision;placementPlantPending=true;return;}
  auto identity=plants::modelMesh(s.result->lods[0],"plant");plants::V identityLow,identityHigh;placementNormalize(identity,identityLow,identityHigh);
  auto atlas=plants::tga(s.result->maps->albedo);std::ostringstream key;key<<"plant-"<<std::hex<<plants::hash("authoring-bundle-v1:"+plants::serialize(s.doc).dump()+ftebroom::modelExport::md3(identity)+atlas);
  auto id=key.str()+(gameProfile==0?"-q3":"-fte");
  auto material="textures/reyond-plugins/"+id,model="models/reyond-plugins/"+id+".md3",script="scripts/"+id+".shader",source="models/reyond-plugins/"+id+".plant.json";
  plants::V low,high;auto mesh=plants::modelMesh(s.result->lods[0],material);auto scale=placementNormalize(mesh,low,high);
  std::map<std::string,std::string> bundle{{model,ftebroom::modelExport::md3(mesh)},{material+".tga",atlas},{script,plants::shader(material,material+".tga",gameProfile!=0)},{source,plants::serialize(s.doc).dump(2)}};
  auto portable=plants::serialize(s.doc);plants::Json dependencies={material+".tga",script,source};
  if(!s.doc.texture.empty()){auto original=std::filesystem::u8path(s.doc.texture);auto name=id+"-leaf"+original.extension().u8string();auto dependency="models/reyond-plugins/"+name;bundle[dependency]=q1Read(original);dependencies.push_back(dependency);portable["texture"]=name;bundle[source]=portable.dump(2);}
  bundle[model+".reyond.json"]=plants::Json{{"kind","reyondbroom-authoring-model"},{"version",1},{"low",low},{"high",high},{"normalization",scale},{"dependencies",dependencies},{"plantProject",portable}}.dump(2);
  placementPublish(bundle);
  auto shader=plants::shader(material,material+".tga",gameProfile!=0);s.pendingShaders[id]={material,shader.substr(shader.find('{'))};command(("ftebroom_plant shader "+id+"\n").c_str());
  placementSend(p,model,source,low,high,scale,s.allowFteQ1);s.lastModel=documentPath(model);s.message="Project and runtime assets saved; updating linked func_model.";
}
static void placementAfterPlantSave(std::function<void()> next){
  if(!next)return;if(placementPlantPending||plantPlacement.awaiting){placementPlantNext=std::move(next);return;}next();
}
static void placementTick(){
  if(!modelPlacement.active){if(placementModelCancel)*placementModelCancel=true;placementModelPending={};placementModelNext={};}
  if(placementModelJob.valid()&&placementModelJob.wait_for(std::chrono::seconds(0))==std::future_status::ready){
    auto result=placementModelJob.get();
    if(placementModelRunning==placementModelGeneration&&modelPlacement.active&&placementModelBinding==modelPlacement.id){
      try{
        placementValid(modelPlacement);if(!result.error.empty())throw std::runtime_error(result.error);
        placementPublish(result.bundle);
        for(auto& [id,shader]:result.shaders){plantState.pendingShaders[id]=shader;command(("ftebroom_plant shader "+id+"\n").c_str());}
        placementSend(modelPlacement,result.model,result.source,result.low,result.high,result.scale,result.optin);bbMessage="Saved model assets; updating linked func_model.";

      }catch(const std::exception& e){placementModelNext={};bbMessage=std::string("Project saved; model placement failed: ")+e.what();}
    }
  }
  if(placementModelPending&&!placementModelJob.valid()){
    auto project=std::move(placementModelPending);placementModelPending={};
    if(modelPlacement.active){placementModelRunning=placementModelGeneration;placementModelBinding=modelPlacement.id;auto profile=gameProfile;
      placementModelCancel=std::make_shared<std::atomic<bool>>(false);auto cancel=placementModelCancel;
      placementModelJob=std::async(std::launch::async,[project,profile,cancel](){return placementBakeModel(*project,profile,cancel.get());});
    }
  }

  for(auto* p:{&plantPlacement,&modelPlacement}){
    if(p->active&&(p->epoch!=organizationMapEpoch||p->profile!=gameProfile||p->sourceRoot!=std::string(baseq3)||p->runtimeRoot!=gameRuntime()))*p={};
    if(!p->awaiting)continue;char ack[200]{},status[64]{};cvars->GetString("ftebroom_placement_ack",ack,sizeof ack);
    if(p->request!=ack)continue;cvars->GetString("ftebroom_placement_status",status,sizeof status);p->awaiting=false;
    auto& message=p==&plantPlacement?plantState.message:bbMessage;
    if(std::string(status)=="ok"){message="Saved linked func_model. Undo/Redo restores its previous model and assets.";if(!p->runtimeSupported)message+=" Authoring only: this runtime requires a compatible model/mod, or the explicit Quake 1 FTE extension. Compilation will report the unsupported entity.";}
    else{p->active=false;message=std::string(status)=="ambiguous"?"Source saved. Placement cancelled: multiple entities share the source link; no entity was changed.":std::string(status)=="missing"?"Source saved. Linked entity was deleted or undone; placement cancelled.":"Source saved. Placement cancelled: map, layer or position is no longer valid.";}
    auto& after=p==&plantPlacement?placementPlantNext:placementModelNext;
    if(std::string(status)=="ok"&&after){auto next=std::move(after);after={};try{next();}catch(const std::exception& e){message=e.what();}}else if(std::string(status)!="ok")after={};
  }
  if(placementPlantPending){
    if(!plantPlacement.active||placementPlantRevision!=plantState.revision||(!plantBusy()&&!plantState.error.empty())){placementPlantPending=false;placementPlantNext={};plantState.message="Source saved. Pending placement cancelled because the document changed or generation failed; Save again when ready.";}
    else if(plantReady()){placementPlantPending=false;plantAction(placementPlantSaved);}
  }
}

static void placementWriteQa(){
 if(!placementQa())return;static double last=0;if(ImGui::GetTime()-last<.08)return;last=ImGui::GetTime();
 nlohmann::json views=nlohmann::json::array();for(auto& v:viewRects)views.push_back({{"x",v.x},{"y",v.y},{"width",v.w},{"height",v.h},{"type",v.type}});
 auto binding=[](auto& p){return nlohmann::json{{"active",p.active},{"created",p.created},{"awaiting",p.awaiting},{"id",p.id},{"epoch",p.epoch},{"point",p.point},{"normal",p.normal},{"layer",p.layer}};};
 auto widgets=placementWidgets;if(plantState.qa)for(auto it=plantState.widgets.begin();it!=plantState.widgets.end();++it)widgets["plant/"+it.key()]=it.value();
 auto j=nlohmann::json{{"widgets",widgets},{"views",views},{"display",{ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y}},{"profile",gameProfile},{"epoch",organizationMapEpoch},{"context",binding(contextPlacement)},{"plant",binding(plantPlacement)},{"model",binding(modelPlacement)},{"contextGesture",{{"candidate",sceneContextCandidate},{"duration_ms",sceneContextDuration},{"dx",sceneContextDX},{"dy",sceneContextDY},{"pending",sceneContextPending},{"looking",sceneLook}}},{"browseError",runUiBrowseError},{"browsePath",runUiBrowsePath},{"modelBusy",bool(placementModelPending)||placementModelJob.valid()||modelPlacement.awaiting},{"modelOpen",bbActive},{"modelPath",bbProject.path},{"modelMessage",bbMessage},{"modelElements",bbProject.root["elements"].size()},{"newModel",bbNewOpen},{"guardModel",bbGuardOpen},{"plantOpen",plantState.open},{"plantReady",plantReady()},{"plantMessage",plantState.message},{"plantError",plantState.error},{"plantPath",plantState.file.u8string()},{"mapPath",documentFile.u8string()},{"generation_ms",plantState.result?plantState.result->milliseconds:0.}};
 try{bb::atomicWrite(documentPath("placement-ui-live.json"),j.dump());}catch(const std::exception& e){core->Print((std::string("Placement QA: ")+e.what()+"\n").c_str());}
}
