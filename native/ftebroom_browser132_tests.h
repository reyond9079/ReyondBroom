// SPDX-License-Identifier: GPL-2.0-or-later
// Opt-in, read-only state/rectangles for actual OS-input browser acceptance.
// Setup only selects the initial fixture and opens the browser.
static void browser132QaBegin(){
 browserQaEnabled=cvars->GetFloat("ftebroom_skip_setup")&&cvars->GetFloat("ftebroom_browser132_qa");
 if(browserQaEnabled){browserQaWidgets.clear();browserQaEntityClasses.clear();}
}
static void browser132QaSetup(){
 if(!cvars->GetFloat("ftebroom_skip_setup"))return;
 cvars->SetFloat("ftebroom_browser132_qa",1);recentWindow=false;setupNeeded=false;firstSetupStep=-1;
 gameProfile=0;q3Root=documentPath("ftebroom.cfg").parent_path().u8string();std::snprintf(baseq3,sizeof baseq3,"%s",q3Root.c_str());
 uiScaleChoice=0;uiTheme=0;uiStyle();viewSetLayout(4);
 modFilesOpen=true;modFilesRequestFocus=true;modFilesSources=false;modFilesRoot=modFileRoot();modFilesFolder=modFileRoot()/"browser132";modFilesRefresh=true;
 ftebroom::EntityDefinition point;point.name="browser132_point";point.brush=false;entityDefinitions[point.name]=point;
 auto brush=point;brush.name="browser132_brush";brush.brush=true;entityDefinitions[brush.name]=brush;
 cvars->SetString("ftebroom_test_entity_filter","browser132");
}
static void browser132QaWrite(){
 if(!browserQaEnabled)return;static double last=0;if(ImGui::GetTime()-last<.08)return;last=ImGui::GetTime();
 nlohmann::json widgets=nlohmann::json::object();
 for(auto& item:browserQaWidgets){auto& r=item.second;widgets[item.first]={{"x",r.min.x},{"y",r.min.y},{"width",r.max.x-r.min.x},{"height",r.max.y-r.min.y},{"visible",r.visible},{"active",r.active},{"scrollY",r.scrollY},{"scrollMax",r.scrollMax}};}
 for(auto name:{"Entity","Face","Mod file manager"})if(auto w=ImGui::FindWindowByName(name))if(w->DockNode&&w->DockNode->TabBar){
  auto bar=w->DockNode->TabBar;for(const auto& tab:bar->Tabs)if(tab.ID==w->TabId){
   float x=bar->BarRect.Min.x+tab.Offset-bar->ScrollingAnim;
   widgets[std::string("tab/")+name]={{"x",x},{"y",bar->BarRect.Min.y},{"width",tab.Width},{"height",bar->BarRect.GetHeight()},{"visible",true}};
  }
 }
 size_t ready=0;for(const auto& item:entityPreviews)ready+=item.second.thumbnail!=0;
 auto j=nlohmann::json{{"widgets",widgets},{"display",{ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y}},{"folder",modFilesFolder.generic_u8string()},{"modRoot",modFileRoot().generic_u8string()},{"error",modFilesError},{"thumbnail",modFilesThumbnailSize},{"entityThumbnail",entityThumbnailSize},{"materialThumbnail",browserQaMaterialSize},{"entityClasses",browserQaEntityClasses},{"entries",modFilesEntries.size()},{"cachedModels",entityPreviews.size()},{"readyModels",ready},{"previewBusy",entityPreviewJob.valid()},{"modelReads",entityPreviewResourceReads},{"modelBuilds",entityPreviewBuilds},{"modelQuads",entityPreviewQuadFrame==ImGui::GetFrameCount()?entityPreviewQuadCount:0},{"frameMs",ImGui::GetIO().DeltaTime*1000},{"frame",ImGui::GetFrameCount()}};
 try{bb::atomicWrite(documentPath("browser132-live.json"),j.dump());}catch(const std::exception& e){core->Print((std::string("Browser QA: ")+e.what()+"\n").c_str());}
}
