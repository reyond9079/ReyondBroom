// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "plant/schema.h"
// A context request owns world coordinates and a map instance, never a screen
// coordinate that could be reprojected after the user moves the camera.
struct AssetPlacement {
  bool active=false,created=false,awaiting=false,runtimeSupported=true;
  int epoch=0,profile=0,layer=0;
  std::array<double,3> point{},normal{0,0,1};
  double grid=1;
  std::array<double,3> gridOrigin{};
  std::array<std::array<double,3>,3> basis{{{1,0,0},{0,1,0},{0,0,1}}};
  std::string id,request;
};
static AssetPlacement contextPlacement,plantPlacement,modelPlacement,modelNewPlacement;
static unsigned placementSerial=0;
static void placementCreate(bool plant);
static void placementModelSaved();
static void placementAfterModelSave(std::function<void()> next);
static void placementShutdown();
static void placementPlantSaved();
static void placementTick();
static void placementAfterPlantSave(std::function<void()> next);
static void placementCancel(AssetPlacement& p){p={};}
static void placementCapture() {
  auto read=[](const char* name){char value[160]{};cvars->GetString(name,value,sizeof value);std::string text(value);text.erase(std::remove(text.begin(),text.end(),'\''),text.end());std::array<double,3> out{};std::istringstream in(text);for(auto& x:out)in>>x;return out;};
  AssetPlacement p;p.active=true;p.epoch=organizationMapEpoch;p.profile=gameProfile;
  p.layer=int(cvars->GetFloat("ftebroom_context_active_layer"));p.grid=cvars->GetFloat("ftebroom_context_grid");
  p.point=read("ftebroom_context_point");p.normal=read("ftebroom_context_normal");
  if(ftebroom::plant::length(p.normal)<.1)p.normal={0,0,1};else p.normal=ftebroom::plant::unit(p.normal);
  p.id="placement-"+std::to_string(core->GetMilliseconds())+"-"+std::to_string(++placementSerial);
  p.gridOrigin=read("ftebroom_context_grid_origin");
  for(int i=0;i<3;i++)p.basis[i]=read(("ftebroom_context_basis"+std::to_string(i)).c_str());
  contextPlacement=std::move(p);
}
static nlohmann::json placementWidgets=nlohmann::json::object();
static bool placementQa(){return cvars->GetFloat("ftebroom_skip_setup")&&cvars->GetFloat("ftebroom_placement_qa");}
static void placementRect(const char* id){
  if(!placementQa())return;auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();auto clip=ImGui::GetCurrentWindow()->ClipRect;
  placementWidgets[id]={{"x",a.x},{"y",a.y},{"width",b.x-a.x},{"height",b.y-a.y},{"active",ImGui::IsItemActive()},{"visible",ImGui::IsItemVisible()},{"clip",{clip.Min.x,clip.Min.y,clip.Max.x,clip.Max.y}}};
}
static bool placementButton(const char* label,const char* id){bool b=ImGui::Button(label);placementRect(id);return b;}
static void placementWriteQa();
