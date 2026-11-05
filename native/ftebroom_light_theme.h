// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <utility>
// Modern Light keeps its persisted identifier (3). Opaque surfaces suggest
// frosted glass through tonal layering; there are no blur or scene passes.
namespace SoftLight {
constexpr ImVec4 color(unsigned rgb, float alpha = 1) {
  return {float((rgb >> 16) & 255)/255, float((rgb >> 8) & 255)/255,
          float(rgb & 255)/255, alpha};
}
constexpr auto workspace = color(0xAAA9A7), panel = color(0xBABAB8),
  inset = color(0xB2B2B0), field = color(0xB6B6B4), elevated = color(0xCDCDCA),
  ink = color(0x212222), secondary = color(0x474747), disabled = color(0x797979),
  border = color(0x363635,.16f), hover = color(0xC3C5C1), active = color(0xCDD1CE),
  teal = color(0x516E65), focus = color(0x365950), danger = color(0x913D3B),
  warning = color(0x705820), nav = color(0x242426), navText = color(0xF1F0ED),
  navMuted = color(0xA7AAA8), navHover = color(0x414142), navActive = color(0x555655),
  navInput = color(0x373738), transparent = color(0,0);
constexpr float gutter = 16.f, margin = 18.f, panelRadius = 17.f;
constexpr float railWidth = 94.f;
inline void apply(ImGuiStyle& s) {
  ImGui::StyleColorsLight(&s);
  auto& c=s.Colors;
  c[ImGuiCol_Text]=ink;c[ImGuiCol_TextDisabled]=secondary;
  c[ImGuiCol_WindowBg]=panel;c[ImGuiCol_ChildBg]=inset;c[ImGuiCol_PopupBg]=elevated;
  c[ImGuiCol_Border]=border;c[ImGuiCol_BorderShadow]=color(0xFFFFFF,.16f);
  c[ImGuiCol_FrameBg]=field;c[ImGuiCol_FrameBgHovered]=hover;c[ImGuiCol_FrameBgActive]=active;
  c[ImGuiCol_TitleBg]=c[ImGuiCol_TitleBgActive]=c[ImGuiCol_TitleBgCollapsed]=panel;
  c[ImGuiCol_MenuBarBg]=transparent;
  c[ImGuiCol_ScrollbarBg]=transparent;c[ImGuiCol_ScrollbarGrab]=color(0x414141,.25f);
  c[ImGuiCol_ScrollbarGrabHovered]=color(0x414141,.42f);c[ImGuiCol_ScrollbarGrabActive]=secondary;
  c[ImGuiCol_CheckMark]=focus;c[ImGuiCol_SliderGrab]=teal;c[ImGuiCol_SliderGrabActive]=focus;
  c[ImGuiCol_Button]=color(0xFFFFFF,.10f);c[ImGuiCol_ButtonHovered]=hover;c[ImGuiCol_ButtonActive]=active;
  c[ImGuiCol_Header]=color(0xE1E5E0,.28f);c[ImGuiCol_HeaderHovered]=hover;c[ImGuiCol_HeaderActive]=active;
  c[ImGuiCol_Separator]=border;c[ImGuiCol_SeparatorHovered]=color(0x454745,.50f);c[ImGuiCol_SeparatorActive]=focus;
  c[ImGuiCol_ResizeGrip]=color(0x343434,.12f);c[ImGuiCol_ResizeGripHovered]=color(0x343434,.32f);
  c[ImGuiCol_ResizeGripActive]=color(0x343434,.45f);
  c[ImGuiCol_Tab]=c[ImGuiCol_TabDimmed]=transparent;
  c[ImGuiCol_TabHovered]=hover;c[ImGuiCol_TabSelected]=c[ImGuiCol_TabDimmedSelected]=elevated;
  c[ImGuiCol_TabSelectedOverline]=c[ImGuiCol_TabDimmedSelectedOverline]=transparent;
  c[ImGuiCol_DockingPreview]=color(0x4E817A,.22f);c[ImGuiCol_DockingEmptyBg]=transparent;
  c[ImGuiCol_PlotLines]=teal;c[ImGuiCol_PlotLinesHovered]=focus;
  c[ImGuiCol_PlotHistogram]=teal;c[ImGuiCol_PlotHistogramHovered]=focus;
  c[ImGuiCol_TableHeaderBg]=color(0xFFFFFF,.14f);c[ImGuiCol_TableBorderStrong]=border;
  c[ImGuiCol_TableBorderLight]=border;c[ImGuiCol_TableRowBg]=transparent;
  c[ImGuiCol_TableRowBgAlt]=color(0xFFFFFF,.11f);c[ImGuiCol_TextLink]=focus;
  c[ImGuiCol_TextSelectedBg]=color(0x4E817A,.22f);c[ImGuiCol_DragDropTarget]=focus;
  c[ImGuiCol_NavCursor]=focus;c[ImGuiCol_NavWindowingHighlight]=color(0x4E817A,.65f);
  c[ImGuiCol_NavWindowingDimBg]=color(0x252729,.12f);c[ImGuiCol_ModalWindowDimBg]=color(0x252729,.18f);
  s.WindowRounding=panelRadius;s.ChildRounding=11;s.PopupRounding=14;s.FrameRounding=8;
  s.GrabRounding=4;s.ScrollbarRounding=6;s.TabRounding=8;
  s.WindowBorderSize=s.ChildBorderSize=s.FrameBorderSize=1;s.PopupBorderSize=1;s.TabBorderSize=0;
  s.TabBarBorderSize=0;s.TabBarOverlineSize=0;
  s.WindowPadding={10,8};s.FramePadding={7,4};s.ItemSpacing={7,5};s.ItemInnerSpacing={5,4};
  s.CellPadding={6,4};s.IndentSpacing=18;s.ScrollbarSize=9;s.GrabMinSize=10;
  s.SeparatorTextBorderSize=1;s.SeparatorTextPadding={0,5};s.DockingSeparatorSize=gutter;
  s.DisabledAlpha=.5f;
}

}
static bool uiSoftLight(){return uiTheme==3;}
// The engine draws its viewport before ImGui. Only the four strips outside
// that rectangle may carry an opaque workspace background or frame.
template<class Paint>
static void uiOutsideScene(const ImRect& bounds,const ImRect& scene,Paint paint) {
  const ImRect strips[]={
    {bounds.Min,{bounds.Max.x,scene.Min.y}},
    {{bounds.Min.x,scene.Min.y},{scene.Min.x,scene.Max.y}},
    {{scene.Max.x,scene.Min.y},{bounds.Max.x,scene.Max.y}},
    {{bounds.Min.x,scene.Max.y},bounds.Max}
  };
  for(const auto& strip:strips)
    if(strip.GetWidth()>0 && strip.GetHeight()>0)paint(strip);
}
static void uiSoftSurface(ImDrawList* draw,const ImRect& bounds,float radius,bool dark=false,
                          const ImRect* scene=nullptr) {
  if(bounds.IsInverted() || bounds.GetWidth()<1 || bounds.GetHeight()<1)return;
  auto paint=[&]{
    const float scale=uiScale();
    for(int spread:{7,5,3,1}){
      const float pad=spread*scale*(dark?.55f:1.f);
      draw->AddRectFilled({bounds.Min.x-pad,bounds.Min.y+3*scale-pad},
        {bounds.Max.x+pad,bounds.Max.y+4*scale+pad},
        ImGui::GetColorU32(SoftLight::color(0x202020,spread==1?.09f:.035f)),radius+pad);
    }
    const ImU32 top=ImGui::GetColorU32(dark?SoftLight::color(0x2A2A2C):SoftLight::color(0xC0C0BE));
    const ImU32 bottom=ImGui::GetColorU32(dark?SoftLight::nav:SoftLight::panel);
    const int first=draw->VtxBuffer.Size;
    draw->AddRectFilled(bounds.Min,bounds.Max,top,radius);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw,first,draw->VtxBuffer.Size,
      bounds.Min,{bounds.Min.x,bounds.Max.y},top,bottom);
    draw->AddRect(bounds.Min,bounds.Max,ImGui::GetColorU32(SoftLight::color(0xFFFFFF,dark?.17f:.32f)),radius);
  };
  if(!scene){paint();return;}
  const auto* viewport=ImGui::GetMainViewport();
  const ImRect screen{viewport->Pos,{viewport->Pos.x+viewport->Size.x,viewport->Pos.y+viewport->Size.y}};
  uiOutsideScene(screen,*scene,[&](const ImRect& clip){
    draw->PushClipRect(clip.Min,clip.Max,true);
    paint();
    draw->PopClipRect();
  });
}
static bool uiWorkspaceDocked(const ImGuiWindow* window) {
  return window && window->DockNode && ImGui::DockNodeGetRootNode(window->DockNode)->IsDockSpace();
}
static bool uiSoftPanelBegin(const char* name,bool* open,ImGuiWindowFlags flags=0) {
  if(!uiSoftLight())return ImGui::Begin(name,open,flags);
  if(uiWorkspaceDocked(ImGui::FindWindowByName(name)))flags|=ImGuiWindowFlags_NoBackground;
  const bool visible=ImGui::Begin(name,open,flags);
  const auto* window=ImGui::GetCurrentWindow();
  if(uiWorkspaceDocked(window) && window->DockTabIsVisible && !window->Hidden) {
    uiSoftSurface(ImGui::GetBackgroundDrawList(),window->Rect(),SoftLight::panelRadius*uiScale());
  }
  return visible;
}
// Explicit roles keep asset pixels, scene overlays, axes and diagnostic colors
// independent from interface theming. All other themes retain their exact color.
enum class UiTone {Inset, Raised, Selection, Muted, Warning, Error, Ink};
static ImVec4 uiTone(UiTone role) {
  switch(role){
    case UiTone::Inset:return SoftLight::inset;case UiTone::Raised:return SoftLight::elevated;
    case UiTone::Selection:return SoftLight::teal;case UiTone::Muted:return SoftLight::secondary;
    case UiTone::Warning:return SoftLight::warning;case UiTone::Error:return SoftLight::danger;
    default:return SoftLight::ink;
  }
}
static ImU32 uiChrome(ImU32 legacy, UiTone role) {
  return uiSoftLight()?ImGui::GetColorU32(uiTone(role)):legacy;
}
static ImVec4 uiActiveToolColor(){return uiSoftLight()?SoftLight::navActive:uiColor(62,112,205);}
// This scope only surrounds Tools. Menus and controls outside it stay light;
// the dock node, title, docking order and panel dimensions are unchanged.
static void uiToolStripBegin() {
  if(!uiSoftLight())return;
  const std::pair<ImGuiCol,ImVec4> colors[]={
    {ImGuiCol_Text,SoftLight::navText},{ImGuiCol_TextDisabled,SoftLight::navMuted},
    {ImGuiCol_Button,SoftLight::transparent},{ImGuiCol_ButtonHovered,SoftLight::navHover},
    {ImGuiCol_ButtonActive,SoftLight::navActive},{ImGuiCol_FrameBg,SoftLight::navInput},
    {ImGuiCol_FrameBgHovered,SoftLight::navHover},{ImGuiCol_FrameBgActive,SoftLight::navActive},
    {ImGuiCol_Border,SoftLight::navHover},{ImGuiCol_Separator,SoftLight::navHover},
    {ImGuiCol_CheckMark,SoftLight::navText},{ImGuiCol_PopupBg,SoftLight::nav},
    {ImGuiCol_Header,SoftLight::navActive},{ImGuiCol_HeaderHovered,SoftLight::navHover},
    {ImGuiCol_HeaderActive,SoftLight::navActive}};
  for(auto& pair:colors)ImGui::PushStyleColor(pair.first,pair.second);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
}
static ImDrawListSplitter uiToolStripSplitter;
static ImVec2 uiToolStripOrigin,uiToolStripAvailable;
static bool uiToolStripDrawing=false;
static void uiToolStripSurface(){
  if(!uiSoftLight()||ImGui::GetCurrentWindow()->SkipItems)return;
  uiToolStripOrigin=ImGui::GetCursorScreenPos();uiToolStripAvailable=ImGui::GetContentRegionAvail();
  auto d=ImGui::GetWindowDrawList();uiToolStripSplitter.Split(d,2);uiToolStripSplitter.SetCurrentChannel(d,1);uiToolStripDrawing=true;
}
static void uiToolStripEnd(bool fitContents=false){
  if(!uiSoftLight())return;
  if(uiToolStripDrawing){
    auto d=ImGui::GetWindowDrawList();uiToolStripSplitter.SetCurrentChannel(d,0);
    auto pos=uiToolStripOrigin,available=uiToolStripAvailable;
    float bottom=pos.y+available.y;
    if(fitContents||available.x>available.y)bottom=std::min(bottom,ImGui::GetCurrentWindow()->DC.CursorMaxPos.y);
    uiSoftSurface(d,{{pos.x-4*uiScale(),pos.y-4*uiScale()},
      {pos.x+available.x+4*uiScale(),bottom+4*uiScale()}},10*uiScale(),true);
    uiToolStripSplitter.Merge(d);uiToolStripDrawing=false;
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(15);
}
static bool uiBrowserSearch(const char* id,const char* hint,char* text,size_t capacity){
  if(!uiSoftLight())return ImGui::InputTextWithHint(id,hint,text,capacity);
  const auto pos=ImGui::GetCursorScreenPos();
  auto padding=ImGui::GetStyle().FramePadding;
  padding.x+=14*uiScale();
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,padding);
  const bool changed=ImGui::InputTextWithHint(id,hint,text,capacity);
  ImGui::PopStyleVar();
  const float scale=uiScale(),centerY=pos.y+ImGui::GetItemRectSize().y*.5f;
  auto* draw=ImGui::GetWindowDrawList();
  const auto color=ImGui::GetColorU32(SoftLight::secondary);
  draw->AddCircle({pos.x+10*scale,centerY-scale},4*scale,color,16,scale);
  draw->AddLine({pos.x+13*scale,centerY+2*scale},{pos.x+17*scale,centerY+6*scale},color,scale);
  return changed;
}
static bool uiInspectorFold(const char* label,ImGuiTreeNodeFlags flags){
  if(uiSoftLight())ImGui::PushStyleColor(ImGuiCol_Header,SoftLight::transparent);
  const bool open=ImGui::CollapsingHeader(label,flags);
  if(uiSoftLight())ImGui::PopStyleColor();
  return open;
}
// Reuse the existing glyph-complete bold font where available. Other themes
// and platforms without a bold resource retain the existing separator behavior.
static void uiSectionHeading(const char* label){
  bool bold=uiSoftLight()&&uiBoldFonts[uiScaleChoice];
  if(bold)ImGui::PushFont(uiBoldFonts[uiScaleChoice]);
  ImGui::SeparatorText(label);
  if(bold)ImGui::PopFont();
}
