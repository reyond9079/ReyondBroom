// SPDX-License-Identifier: GPL-2.0-or-later
#include "ftebroom_bsp_atlas.h"
#include "ftebroom_brush_model.h"
#include "ftebroom_brush_model_entity.h"
static bool bspModelWindow=false,bspModelBusy=false,bspModelAtlas=true,bspModelChooseFolder=false;
static bool bspModelAsMdl=false,bspModelWorldOnly=false;
static bool bspModelReplace=false,bspModelAllowFteQuake=false;
static char bspModelDestination[4096]={};
static std::filesystem::path bspModelTarget,bspModelWork,bspModelFolder;
static std::string bspModelMessage;
static std::string bspModelPreviewPath,bspModelReplacementPath;
static ftebroom::BrushModelEntity bspModelPlacement;
static int bspModelSteps=0,bspModelProfile=0;
static int bspModelReplaceSerial=0,bspModelReplacePending=0;
static double bspModelReplaceDeadline=0;
static float bspModelYaw=130,bspModelPitch=20,bspModelZoom=1;
static std::future<ftebroom::bspAtlas::Result> bspModelAtlasJob;
static std::future<std::string> bspModelMdlJob;
static std::filesystem::path bspModelDefaultFolder(){
  auto mod=modFileRoot();
  if(mod.empty()&&*baseq3)mod=std::filesystem::u8path(baseq3);
  const auto folder=std::filesystem::path(gameProfile==2?"progs":"models")/"reyondbroom";
  return mod.empty()?documentPath(folder.generic_u8string()):mod/folder;
}
static std::string bspModelExtension(){return bspModelAsMdl?".mdl":".bsp";}
static void bspModelDefaultDestination(){
  const auto folder=bspModelDefaultFolder();
  auto target=folder/("selected-model"+bspModelExtension());
  std::error_code ec;
  for(int suffix=2;suffix<=999&&std::filesystem::exists(target,ec);suffix++)
    target=folder/("selected-model-"+std::to_string(suffix)+bspModelExtension());
  std::snprintf(bspModelDestination,sizeof bspModelDestination,"%s",target.u8string().c_str());
}
static void bspModelShow(bool mdl,bool worldOnly){
  bspModelWindow=true;
  if(bspModelBusy)return;
  const bool formatChanged=bspModelAsMdl!=mdl;
  const bool profileChanged=bspModelProfile!=gameProfile&&!bspModelPreviewPath.empty();
  bspModelAsMdl=mdl;bspModelWorldOnly=worldOnly;bspModelReplace=worldOnly;
  bspModelAllowFteQuake=false;
  if(worldOnly||formatChanged||profileChanged){
    bspModelPreviewPath.clear();bspModelMessage.clear();
    bspModelDefaultDestination();
  }
  if(!*bspModelDestination)bspModelDefaultDestination();
  if(gameProfile>=3)bspModelAtlas=false;
}
static void bspModelWrite(const std::string& bytes){
  if(std::filesystem::exists(bspModelTarget))throw std::runtime_error("The destination appeared during compilation. Choose a new filename.");
  // Stage beside the destination for cross-volume exports.
  auto pending=bspModelTarget;pending+=".reyond-export.tmp";
  if(std::filesystem::exists(pending))throw std::runtime_error("Export temporary file already exists.");
  try{
    soundWrite(pending,bytes);
    if(std::filesystem::exists(bspModelTarget))throw std::runtime_error("The destination appeared during export. Choose a new filename.");
    std::filesystem::rename(pending,bspModelTarget);
  }catch(...){std::error_code ec;std::filesystem::remove(pending,ec);throw;}
}
static void bspModelComplete(const std::string& bytes){
  try{
    const std::string relative="data/bsp-model-export/"+bspModelWork.filename().u8string()+"/preview"+bspModelExtension();
    soundWrite(documentPath(relative),bytes);
    bspModelPreviewPath=relative;
    bspModelYaw=130;bspModelPitch=20;bspModelZoom=1;
  }catch(const std::exception& e){
    bspModelMessage+=" Preview could not be opened: "+std::string(e.what());
  }
  command("fs_restart\n");
  if(bspModelReplacementPath.empty()){
    bspModelBusy=runExclusive=gameProfileLocked=false;
    return;
  }
  bspModelReplacePending=bspModelReplaceSerial;
  bspModelReplaceDeadline=ImGui::GetTime()+5;
  cvars->SetFloat("ftebroom_brush_model_ack",0);
  command(("ftebroom_replace_brush_model "+std::to_string(bspModelReplacePending)+" "+
    organizationQuote(bspModelPlacement.classname)+" "+organizationQuote(bspModelReplacementPath)+" "+
    organizationQuote(bspModelPlacement.writeMdlKey?"mdl":"")+" "+(bspModelPlacement.fteQuake?"1":"0")+"\n").c_str());
}
static void bspModelPublish(ftebroom::bspAtlas::Result result){
  auto assets=bspModelTarget.parent_path()/(bspModelTarget.stem().u8string()+"-atlas");auto staging=bspModelWork/"output";std::filesystem::create_directory(staging);
  for(size_t i=0;i<result.images.size();i++){
    soundWrite(staging/("atlas"+std::to_string(i)+".tga"),ftebroom::bspAtlas::tga(result.images[i]));
    soundWrite(staging/("uv"+std::to_string(i)+".tga"),ftebroom::bspAtlas::tga(result.outlines[i]));
  }
  if(!result.images.empty()){
    std::string notes="Each atlasN.tga is paired with uvN.tga (transparent UV island boundaries). Keep UV islands and image dimensions unchanged when painting.\n";
    if(bspModelProfile==2){
      auto off=ftebroom::quake1::word(result.bsp,20);auto textures=result.bsp.substr(off,ftebroom::quake1::word(result.bsp,24));unsigned n=ftebroom::quake1::word(textures,0);
      for(unsigned i=0;i<result.images.size();i++){auto name=ftebroom::quake1::name(textures,ftebroom::quake1::word(textures,4+(n-result.images.size()+i)*4),16);
        notes+="atlas"+std::to_string(i)+".tga -> embedded texture "+name+". FTE replacement: textures/"+name+".tga under the game/mod root.\n";}
      notes+="Quake BSP stores its textures internally. For engines without external replacements, re-embed painted images with a BSP texture tool. The original collision and UV mapping must be retained.\n";
    }else notes+="Quake III references atlasN images in this folder. Keep this directory beside the BSP at its original game-relative path. Painted TGA changes are visible after reloading resources.\n";
    soundWrite(staging/"PAINTING.txt",notes);
  }
  bool ownAssets=false;
  try{
    if(std::filesystem::exists(bspModelTarget))throw std::runtime_error("Destination appeared during compilation; choose a new filename.");
    if(!result.images.empty()){if(std::filesystem::exists(assets))throw std::runtime_error("Atlas directory already exists; choose a new model name.");ownAssets=std::filesystem::create_directory(assets);if(!ownAssets)throw std::runtime_error("Cannot create atlas directory.");for(const auto& file:std::filesystem::directory_iterator(staging))soundWrite(assets/file.path().filename(),q1Read(file.path()));}
    bspModelWrite(result.bsp);
    bspModelMessage="Exported "+bspModelTarget.filename().u8string()+".";
    if(ownAssets)bspModelMessage+=" Atlas images, UV outlines and PAINTING.txt were saved beside the model.";
  }catch(...){if(ownAssets){std::error_code ec;std::filesystem::remove_all(assets,ec);}throw;}
  bspModelComplete(result.bsp);
}
static void bspModelBegin(){
  if(runActive||runExclusive||buildRunning||assetsLoading)throw std::runtime_error("Wait for the current resource or compilation job.");
  if(jamEditing())throw std::runtime_error("Brush model export is available in the offline editor.");
  if(bspModelAsMdl&&gameProfile!=2&&gameProfile!=4)
    throw std::runtime_error("Select a Quake 1 or Half-Life profile to convert brushwork to MDL.");
  const std::string extension=bspModelExtension();
  const bool atlas=!bspModelAsMdl&&bspModelAtlas;
  runLoad();std::string entered=bspModelDestination;
  if(entered.size()>=2&&entered.front()=='"'&&entered.back()=='"')entered=entered.substr(1,entered.size()-2);
  if(entered.empty())throw std::runtime_error("Enter a model filename or choose a destination folder.");
  auto target=std::filesystem::u8path(entered);if(target.is_relative())target=bspModelDefaultFolder()/target;
  if(std::filesystem::is_directory(target))target/="selected-model"+extension;
  if(target.extension().empty())target.replace_extension(extension);
  target=std::filesystem::absolute(target).lexically_normal();
  std::snprintf(bspModelDestination,sizeof bspModelDestination,"%s",target.u8string().c_str());
  if(modFilePacked(target))throw std::runtime_error("Export to a folder on disk first; exporting directly into PAK/PK3 archives is not supported.");
  if(entityLower(target.extension().u8string())!=extension)throw std::runtime_error("The model filename must end in "+extension+".");
  if(target.parent_path()==std::filesystem::absolute(bspModelDefaultFolder()).lexically_normal())
    std::filesystem::create_directories(target.parent_path());
  if(!std::filesystem::is_directory(target.parent_path()))throw std::runtime_error("The destination folder does not exist: "+target.parent_path().u8string()+". Use Choose folder to select an existing folder.");
  if(std::filesystem::exists(target))throw std::runtime_error("Choose a new filename; the source and existing models are never overwritten.");
  if(!*baseq3)throw std::runtime_error("Configure game resources first.");
  const auto placement=ftebroom::brushModelEntity(gameProfile,bspModelAsMdl,bspModelAllowFteQuake,entityDefinitions);
  const bool replace=bspModelReplace&&placement.classname;
  std::string replacementPath;
  if(replace){
    replacementPath=modFileModelPath(target);
    if(replacementPath.empty()||!modFileInside(target))
      throw std::runtime_error("Save in the active mod to replace the brushes, or clear the replacement checkbox.");
    if(replacementPath.find_first_of("\"\r\n;")!=replacementPath.npos)
      throw std::runtime_error("Choose a model path without quotes, semicolons or line breaks.");
    if(!modelResource(replacementPath).empty())
      throw std::runtime_error("That model path already exists in the active resources. Choose a new filename.");
  }
  auto shaderFolder=target.parent_path()/(target.stem().u8string()+"-atlas");
  if(atlas&&gameProfile>=3)throw std::runtime_error("Atlas creation currently supports Quake 1 and Quake III. Export this profile without an atlas.");
  if(atlas&&std::filesystem::exists(shaderFolder))throw std::runtime_error("The atlas directory already exists. Choose a new model name.");
  if(atlas&&gameProfile!=2&&modFileModelPath(shaderFolder).empty())throw std::runtime_error("Quake III atlases need a destination inside the configured game resource directories.");
  auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
  auto work=std::filesystem::absolute(documentPath("data/bsp-model-export"))/std::to_string(stamp);std::filesystem::create_directories(work);
  RunProfile profile{"Export selected BSP model",work.u8string(),false,
    {{0,"${WORK_DIR_PATH}/selection.map",(bspModelWorldOnly||replace)?"@selected-world-brushes":"@selected-brushes"}}};
  if(gameProfile==3){
    if(runVariables["q2bsp"].empty())throw std::runtime_error("Configure q2bsp in Setup compile.");
    auto mod=std::filesystem::u8path(q2Mod),base=mod.parent_path()/"baseq2";if(!std::filesystem::is_directory(base))base=mod;
    profile.tasks.push_back({1,"${q2bsp}","-q2bsp -nofill -basedir "+modQuote(base.generic_u8string())+" -gamedir "+modQuote(mod.generic_u8string())+" -path \"${WORK_DIR_PATH}\" \"${WORK_DIR_PATH}/selection.map\""});
  }else if(gameProfile==4){
    if(runVariables["hlcsg"].empty()||runVariables["hlbsp"].empty())throw std::runtime_error("Configure hlcsg / hlbsp in Setup compile.");
    profile.tasks.push_back({1,"${hlcsg}","-nowadtextures \"${WORK_DIR_PATH}/selection.map\""});
    profile.tasks.push_back({1,"${hlbsp}","-nofill \"${WORK_DIR_PATH}/selection.map\""});
  }else if(gameProfile==2){if(runVariables["qbsp"].empty())throw std::runtime_error("Configure the ericw qbsp compiler in Setup compile.");
    profile.tasks.push_back({1,"${qbsp}",std::string("-nofill ")+(q1BspFormat==1?"-bsp2 ":"")+"\"${WORK_DIR_PATH}/selection.map\""});
  }else{
    if(runVariables["q3map2"].empty())throw std::runtime_error("Configure q3map2 in Setup compile.");
    auto game=std::filesystem::absolute(std::filesystem::u8path(baseq3)).lexically_normal();
    // The selected-only snapshot contains an exterior flood seed for open geometry.
    std::string args="-game quake3 -fs_basepath "+modQuote(game.parent_path().generic_u8string())+" -fs_basegame "+modQuote(game.filename().u8string())+" -fs_game "+modQuote(game.filename().u8string())+" -threads ${CPU_COUNT} -meta \"${WORK_DIR_PATH}/selection.map\"";
    profile.tasks.push_back({1,"${q3map2}",args});
  }
  runExportHook=nullptr;runStageHook=nullptr;runBegin(profile);runWindow=false;if(!runActive)throw std::runtime_error(runError.empty()?"Cannot start export.":runError);
  bspModelReplaceSerial=bspModelReplaceSerial%1000000+1;
  cvars->SetFloat("ftebroom_brush_model_request",replacementPath.empty()?0:bspModelReplaceSerial);
  bspModelReplacementPath=std::move(replacementPath);
  bspModelPlacement=placement;
  bspModelPreviewPath.clear();
  bspModelTarget=target;bspModelWork=work;bspModelProfile=gameProfile;bspModelSteps=profile.tasks.size();bspModelBusy=runExclusive=gameProfileLocked=true;bspModelMessage="Compiling only the selected brushes. No VIS or LIGHT stage.";
}
static void bspModelTick(){
  if(!bspModelBusy)return;
  if(bspModelReplacePending){
    const bool acknowledged=int(cvars->GetFloat("ftebroom_brush_model_ack"))==bspModelReplacePending;
    if(!acknowledged&&ImGui::GetTime()<bspModelReplaceDeadline)return;
    char status[64]{};
    if(acknowledged)cvars->GetString("ftebroom_brush_model_status",status,sizeof status);
    bspModelMessage+=std::strcmp(status,"ok")==0?" Replaced the original brushes. Undo restores the brushwork.":" The source map changed during export. The model file was saved and the brushes were kept.";
    if(std::strcmp(status,"ok")==0&&bspModelPlacement.fteQuake)
      bspModelMessage+=" In game, this entity requires the FTE Quake 1 static-model extension.";
    bspModelReplacePending=0;
    cvars->SetFloat("ftebroom_brush_model_request",0);
    bspModelBusy=runExclusive=gameProfileLocked=false;
    return;
  }
  try{
    if(bspModelMdlJob.valid()){
      if(bspModelMdlJob.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
      const auto bytes=bspModelMdlJob.get();
      bspModelWrite(bytes);
      bspModelMessage="Exported "+bspModelTarget.filename().u8string()+".";
      bspModelComplete(bytes);return;
    }
    if(bspModelAtlasJob.valid()){
      if(bspModelAtlasJob.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
      bspModelPublish(bspModelAtlasJob.get());return;
    }
    if(runActive)return;
    soundWrite(bspModelWork/"compile.log",runLog);
    if(runHadError||runStep!=bspModelSteps)throw std::runtime_error(runHadError?runError:"BSP model export cancelled.");
    auto bytes=q1Read(bspModelWork/"selection.bsp");if(!(bspModelProfile>=3?ftebroom::quake1::gameBsp(bytes,bspModelProfile):ftebroom::quake1::bsp(bytes,bspModelProfile==2)))throw std::runtime_error("Compiler produced an invalid BSP.");
    if(bspModelAsMdl){
      auto palette=bspModelProfile==2?modelResource("gfx/palette.lmp"):std::string{};
      bspModelMessage="Baking brush surfaces and textures into MDL...";
      bspModelMdlJob=std::async(std::launch::async,[source=std::move(bytes),palette=std::move(palette),goldSrc=bspModelProfile==4]{
        return ftebroom::brushModel::mdl(source,palette,goldSrc);
      });
      return;
    }
    // Model exports carry no gameplay entities, including the Q3 flood seed.
    while(bytes.size()%4)bytes+='\0';size_t entities=bytes.size();bytes+="{\n\"classname\" \"worldspawn\"\n}\n";bytes+='\0';
    size_t header=(bspModelProfile==2||bspModelProfile==4)?4:8;ftebroom::bspAtlas::put(bytes,header,entities);ftebroom::bspAtlas::put(bytes,header+4,bytes.size()-entities);
    if(!bspModelAtlas){ftebroom::bspAtlas::Result result;result.bsp=std::move(bytes);bspModelPublish(std::move(result));return;}
    uint64_t imagePixels=0;std::map<std::string,ftebroom::bspAtlas::Image> images;
    std::string palette=bspModelProfile==2?modelResource("gfx/palette.lmp"):std::string{};
    std::string prefix="rb";uint32_t hash=2166136261u;for(unsigned char c:bspModelTarget.u8string())hash=(hash^c)*16777619u;prefix+=std::to_string(hash%100000000)+"_";
    if(bspModelProfile!=2){
      auto& b=bytes;auto off=ftebroom::quake1::word(b,16),length=ftebroom::quake1::word(b,20);ftebroom::quake1::range(b,off,length);if(length%72||length/72>4096)throw std::runtime_error("Invalid BSP shader directory.");
      auto faceOffset=ftebroom::quake1::word(b,112),faceLength=ftebroom::quake1::word(b,116);ftebroom::quake1::range(b,faceOffset,faceLength);if(faceLength%104||faceLength/104>4096)throw std::runtime_error("Invalid BSP surfaces.");
      std::set<unsigned> used;for(unsigned f=0;f<faceLength;f+=104)used.insert(ftebroom::quake1::word(b,faceOffset+f));
      for(unsigned index:used){unsigned i=index*72;if(index>=length/72)throw std::runtime_error("Invalid BSP material index.");auto name=ftebroom::quake1::name(b,off+i,64);if(images.count(name))continue;auto material=materials.find(name);if(material!=materials.end()&&(material->second.tags&(64|32|512)))throw std::runtime_error("Atlas requires static opaque materials: "+name);auto imageName=material!=materials.end()&&!material->second.preview.empty()?material->second.preview:name;std::string sourceImage;for(const char* suffix:{"",".tga",".png",".jpg",".jpeg"}){sourceImage=modelResource(imageName+suffix);if(!sourceImage.empty())break;}if(sourceImage.empty())throw std::runtime_error("Missing atlas material image: "+imageName);auto skin=modelImportDecode(sourceImage);imagePixels+=uint64_t(skin.width)*skin.height;if(imagePixels>33554432)throw std::runtime_error("Atlas source images exceed 32 million pixels; export a smaller selection.");ftebroom::bspAtlas::Image image;image.width=skin.width;image.height=skin.height;image.rgba=std::move(skin.rgba);images[name]=std::move(image);}
      prefix=modFileModelPath(bspModelTarget.parent_path()/(bspModelTarget.stem().u8string()+"-atlas"))+"/atlas";
    }
    bspModelMessage="Packing UV islands and baking atlas images...";
    bspModelAtlasJob=std::async(std::launch::async,[bytes=std::move(bytes),palette=std::move(palette),prefix,images=std::move(images)](){return ftebroom::bspAtlas::create(bytes,palette,prefix,[&](const std::string& name){auto it=images.find(name);if(it==images.end())throw std::runtime_error("Missing atlas source: "+name);return it->second;});});
  }catch(const std::exception& e){
    bspModelMessage=e.what();
    cvars->SetFloat("ftebroom_brush_model_request",0);
    bspModelBusy=runExclusive=gameProfileLocked=false;
  }
}
static void bspModelPreview(){
  ImGui::SeparatorText("Model preview");
  if(bspModelPreviewPath.empty()){
    ImGui::TextDisabled("The compiled model will appear here.");
    return;
  }
  ImGui::TextDisabled("Drag to orbit. Scroll to zoom.");
  const float u=uiScale();
  const auto available=ImGui::GetContentRegionAvail();
  const ImVec2 size{std::max(64.f,available.x),std::clamp(available.y,170*u,390*u)};
  static qhandle_t image=0;
  if(!image)image=draw->LoadImageShader("ftebroom/brush-model-preview",
    "{\n program default2d\n {\n map $rt:ftebroom_brush_model\n rgbgen vertex\n alphagen vertex\n }\n }");
  const auto pos=ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddImage((ImTextureID)image,pos,{pos.x+size.x,pos.y+size.y});
  ImGui::InvisibleButton("brush-model-orbit",size,ImGuiButtonFlags_MouseButtonLeft);
  if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0,0)){
    bspModelYaw-=ImGui::GetIO().MouseDelta.x*.4f;
    bspModelPitch=std::clamp(bspModelPitch-ImGui::GetIO().MouseDelta.y*.4f,-89.f,89.f);
  }
  if(ImGui::IsItemHovered())
    bspModelZoom=std::clamp(bspModelZoom*std::pow(1.12f,-ImGui::GetIO().MouseWheel),.25f,4.f);
  cvars->SetString("ftebroom_brush_model_preview",bspModelPreviewPath.c_str());
  cvars->SetFloat("ftebroom_brush_model_yaw",bspModelYaw);
  cvars->SetFloat("ftebroom_brush_model_pitch",bspModelPitch);
  cvars->SetFloat("ftebroom_brush_model_zoom",bspModelZoom);
  const float resolution=std::min({1.f,1280/size.x,720/size.y});
  cvars->SetFloat("ftebroom_brush_model_width",size.x*resolution);
  cvars->SetFloat("ftebroom_brush_model_height",size.y*resolution);
}
static void bspModelPanel(){
  cvars->SetString("ftebroom_brush_model_preview","");
  bspModelTick();if(!bspModelWindow)return;
  ImGui::SetNextWindowSize({700*uiScale(),575*uiScale()},ImGuiCond_FirstUseEver);
  if(ImGui::Begin(bspModelAsMdl?"Convert selected brushes to MDL###brush-model-export":"Export selected brushes as BSP model###brush-model-export",&bspModelWindow,ImGuiWindowFlags_NoCollapse)){
    if(bspModelAsMdl)
      ImGui::TextWrapped("Select whole world brushes with static opaque textures. MDL stores geometry and textures. It has no brush collision and cannot seal the map.");
    else
      ImGui::TextWrapped("Exports the selected whole brushes around their center, retaining brush collision and texture alignment.");
    ImGui::BeginDisabled(bspModelBusy);ImGui::InputText(bspModelAsMdl?"Destination MDL":"Destination BSP",bspModelDestination,sizeof bspModelDestination);
    if(*bspModelDestination&&ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
      ImGui::SetTooltip("%s",bspModelDestination);
    if(ImGui::Button("Choose folder...")){
      bspModelFolder=std::filesystem::u8path(bspModelDestination).parent_path();
      std::error_code ec;
      while(bspModelFolder.has_relative_path()&&!std::filesystem::is_directory(bspModelFolder,ec))
        bspModelFolder=bspModelFolder.parent_path();
      if(bspModelFolder.empty())bspModelFolder=bspModelDefaultFolder();
      bspModelChooseFolder=true;
    }
    if(bspModelChooseFolder)ImGui::OpenPopup("BSP model folder");
    if(ImGui::BeginPopupModal("BSP model folder",&bspModelChooseFolder,ImGuiWindowFlags_AlwaysAutoResize)){
      ImGui::TextWrapped("%s",bspModelFolder.u8string().c_str());if(ImGui::Button("Up")&&bspModelFolder.has_parent_path())bspModelFolder=bspModelFolder.parent_path();
      ImGui::BeginChild("Directories",{460*uiScale(),240*uiScale()},ImGuiChildFlags_Borders);
      try{std::vector<std::filesystem::path> dirs;for(const auto& e:std::filesystem::directory_iterator(bspModelFolder,std::filesystem::directory_options::skip_permission_denied))if(e.is_directory()&&dirs.size()<1024)dirs.push_back(e.path());std::sort(dirs.begin(),dirs.end());for(const auto& p:dirs)if(ImGui::Selectable(p.filename().u8string().c_str())){bspModelFolder=p;break;}}catch(const std::exception& e){ImGui::TextWrapped("%s",e.what());}ImGui::EndChild();
      if(ImGui::Button("Use this folder")){auto name=std::filesystem::u8path(bspModelDestination).filename();if(name.empty())name=bspModelAsMdl?"selected-model.mdl":"selected-model.bsp";auto target=bspModelFolder/name;std::snprintf(bspModelDestination,sizeof bspModelDestination,"%s",target.u8string().c_str());bspModelChooseFolder=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
    if(!bspModelAsMdl){
      ImGui::BeginDisabled(gameProfile>=3);ImGui::Checkbox("Create texture atlas and UV painting template",&bspModelAtlas);ImGui::EndDisabled();
      if(gameProfile>=3)ImGui::TextDisabled("Atlas export currently supports Quake 1 / Quake III. UV inspection supports all four BSP formats.");
      if(ImGui::IsItemHovered())
        ImGui::SetTooltip("%s",uiTr("Static opaque materials only. Exports a painting guide and UV outlines beside the model."));
    }
    if(gameProfile==2&&!ftebroom::brushModelEntity(gameProfile,bspModelAsMdl,false,entityDefinitions).classname)
      ImGui::Checkbox("Use FTE static models for Quake 1",&bspModelAllowFteQuake);
    const auto placement=ftebroom::brushModelEntity(gameProfile,bspModelAsMdl,bspModelAllowFteQuake,entityDefinitions);
    bool replace=bspModelReplace&&placement.classname;
    ImGui::BeginDisabled(!placement.classname);
    if(ImGui::Checkbox("Replace selected brushes with the model",&replace))bspModelReplace=replace;
    ImGui::EndDisabled();
    if(!placement.classname)
      ImGui::TextWrapped("The active game has no supported placement entity for this format. Export keeps the original brushes.");
    else if(replace){
      ImGui::TextDisabled("%s %s",uiTr("Entity"),placement.classname);
      if(!bspModelAsMdl)ImGui::TextDisabled("In-game collision depends on this entity.");
    }
    if(ImGui::Button("Compile selected brushes"))try{bspModelBegin();}catch(const std::exception& e){bspModelMessage=e.what();}ImGui::EndDisabled();
    if(bspModelBusy){if(runActive){ImGui::SameLine();if(ImGui::Button("Cancel compilation"))runStop();}ImGui::ProgressBar(-float(ImGui::GetTime()),{-1,0},bspModelAsMdl?"Exporting MDL model":"Exporting BSP model");}
    if(!bspModelMessage.empty())ImGui::TextWrapped("%s",bspModelMessage.c_str());
    bspModelPreview();
  }ImGui::End();
}
