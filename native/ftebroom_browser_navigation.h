// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_browser_paths.h"
#include <optional>
#include <map>
struct BrowserQaRect {ImVec2 min,max;bool visible=false,active=false;float scrollY=0,scrollMax=0;};
static bool browserQaEnabled=false;
static std::map<std::string,BrowserQaRect> browserQaWidgets;
static int browserQaMaterialSize=64;
static std::vector<std::string> browserQaEntityClasses;
static void browserQaItem(const std::string& name){
  if(!browserQaEnabled)return;
  const auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
  auto hit=ImRect(min,max);hit.ClipWithFull(ImGui::GetCurrentWindow()->ClipRect);
  const bool visible=ImGui::IsItemVisible()&&hit.GetWidth()>0&&hit.GetHeight()>0;
  // QA clicks the visible portion of a partially clipped tile, as a user does.
  browserQaWidgets[name]={visible?hit.Min:min,visible?hit.Max:max,visible,ImGui::IsItemActive()};
}
static void browserQaWindow(const std::string& name){if(browserQaEnabled){auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();browserQaWidgets[name]={p,{p.x+s.x,p.y+s.y},true,false,ImGui::GetScrollY(),ImGui::GetScrollMaxY()};}}
static bool browserNavigationButton(bool up) {
  const float height=ImGui::GetFrameHeight(),size=height*.64f;
  const bool clicked=ImGui::Button(up?"##folder-up":"##folder-refresh",{height,height});
  const auto min=ImGui::GetItemRectMin();
  const ImVec2 center{min.x+height*.5f,min.y+height*.5f};
  auto* list=ImGui::GetWindowDrawList();
  const auto color=ImGui::GetColorU32(ImGuiCol_Text);
  const float stroke=std::max(1.f,height*.055f);
  if(up) {
    list->AddLine({center.x,center.y+size*.40f},{center.x,center.y-size*.40f},color,stroke);
    list->PathLineTo({center.x-size*.32f,center.y-size*.07f});
    list->PathLineTo({center.x,center.y-size*.40f});
    list->PathLineTo({center.x+size*.32f,center.y-size*.07f});
    list->PathStroke(color,0,stroke);
  } else {
    list->PathArcTo(center,size*.39f,.18f,5.46f,18);
    list->PathStroke(color,0,stroke);
    list->PathLineTo({center.x+size*.33f,center.y-size*.43f});
    list->PathLineTo({center.x+size*.33f,center.y-size*.10f});
    list->PathLineTo({center.x+size*.02f,center.y-size*.16f});
    list->PathStroke(color,0,stroke);
  }
  if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",up?"Up":"Refresh");
  return clicked;
}
// The caller validates disk roots / archive folders. Navigation itself is shared
// by the map picker and mod browser, including virtual PAK and PK3 paths.
struct BrowserNavigationState {bool editing=false,focus=false;char text[4096]{};};
static std::optional<std::filesystem::path> browserNavigation(const char* id,BrowserNavigationState& state,const std::filesystem::path& folder,bool compact=false) {
  std::optional<std::filesystem::path> navigate;
  ImGui::PushID(id);
  ImGui::PushStyleColor(ImGuiCol_ChildBg,ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,compact?ImVec2(6,2):ImVec2(8,5));
  const float frameHeight=ImGui::GetFrameHeight();
  const float width=compact?std::max(ImGui::GetContentRegionAvail().x,frameHeight*3):0;
  ImGui::BeginChild("path",{width,frameHeight+(compact?4.f:12.f)},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
  if(!compact)ImGui::SetWindowFontScale(1.08f);
  if(state.editing){
    if(state.focus){ImGui::SetKeyboardFocusHere();state.focus=false;}
    ImGui::SetNextItemWidth(-FLT_MIN);
    if(ImGui::InputText("##path",state.text,sizeof state.text,ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll)){
      const auto path=ftebroom::browser::typedPath(state.text);
      if(!path.empty())navigate=std::filesystem::u8path(path).lexically_normal();
      state.editing=false;
    }
    browserQaItem(std::string("nav/")+id+"/input");
    if(ImGui::IsKeyPressed(ImGuiKey_Escape)||(ImGui::IsItemDeactivated()&&!ImGui::IsItemActive()))state.editing=false;
  }else{
    const auto parts=ftebroom::browser::ancestors(folder);
    float width=0;for(const auto& part:parts)width+=ImGui::CalcTextSize(part.label.c_str()).x+ImGui::GetStyle().FramePadding.x*2+ImGui::GetStyle().ItemSpacing.x+14;
    size_t first=0;const float available=ImGui::GetContentRegionAvail().x;
    if(width>available-48&&parts.size()>1){
      while(first+1<parts.size()&&width>available-84){width-=ImGui::CalcTextSize(parts[first].label.c_str()).x+ImGui::GetStyle().FramePadding.x*2+ImGui::GetStyle().ItemSpacing.x+14;++first;}
      if(ImGui::Button("..."))ImGui::OpenPopup("ancestors");
      if(ImGui::BeginPopup("ancestors")){for(size_t i=0;i<first;i++)if(ImGui::MenuItem(parts[i].path.u8string().c_str()))navigate=parts[i].path;ImGui::EndPopup();}
      ImGui::SameLine();
    }
    for(size_t i=first;i<parts.size();i++){
      ImGui::PushID(int(i));
      ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
      const float buttonRoom=std::max(20.f,ImGui::GetContentRegionAvail().x-36.f);
      const float labelWidth=ImGui::CalcTextSize(parts[i].label.c_str()).x+ImGui::GetStyle().FramePadding.x*2;
      if(ImGui::Button(parts[i].label.c_str(),{std::min(labelWidth,buttonRoom),0}))navigate=parts[i].path;
      browserQaItem(std::string("nav/")+id+"/segment/"+parts[i].path.generic_u8string());
      ImGui::PopStyleColor();
      if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",parts[i].path.u8string().c_str());
      ImGui::PopID();ImGui::SameLine(0,2);
      if(i+1<parts.size()){ImGui::AlignTextToFramePadding();ImGui::TextDisabled(">");ImGui::SameLine(0,2);}
    }
    if(ImGui::InvisibleButton("edit-path",{std::max(24.f,ImGui::GetContentRegionAvail().x),ImGui::GetFrameHeight()})){
      std::snprintf(state.text,sizeof state.text,"%s",folder.u8string().c_str());state.editing=state.focus=true;
    }
    browserQaItem(std::string("nav/")+id+"/edit");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Edit folder path");
  }
  if(!compact)ImGui::SetWindowFontScale(1);
  ImGui::EndChild();
  browserQaItem(std::string("nav/")+id+"/bar");
  ImGui::PopStyleVar();ImGui::PopStyleColor();ImGui::PopID();return navigate;
}
// Plain wheel scrolls the tile child; Ctrl+wheel changes only thumbnail size.
static void browserHoverZoom(int& size,int low,int high) {
  if(ImGui::GetIO().KeyCtrl&&ImGui::GetIO().MouseWheel!=0&&ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)&&!ImGui::IsAnyItemActive()){
    ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetCurrentWindow()->ID);
    size=ftebroom::browser::zoom(size,ImGui::GetIO().MouseWheel,low,high);
  }
}
static void browserThumbnailFooter(const char* id,int& size,int low,int high) {
  ImGui::PushID(id);const float height=ImGui::GetFrameHeight(),width=std::min(90.f,ImGui::GetContentRegionAvail().x*.28f);
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-width-height-ImGui::GetStyle().ItemSpacing.x));
  auto p=ImGui::GetCursorScreenPos();ImGui::Dummy({height,height});auto list=ImGui::GetWindowDrawList();auto color=ImGui::GetColorU32(ImGuiCol_Text);float u=height/24;
  list->AddCircle({p.x+10*u,p.y+10*u},5*u,color,16,u);list->AddLine({p.x+14*u,p.y+14*u},{p.x+19*u,p.y+19*u},color,1.5f*u);
  ImGui::SameLine();ImGui::SetNextItemWidth(width);ImGui::SliderInt("##size",&size,low,high,"%d");browserQaItem(std::string("thumbnail/")+id);if(ImGui::IsItemHovered())ImGui::SetTooltip("Thumbnail size");ImGui::PopID();
}
