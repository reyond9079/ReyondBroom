// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <utility>
// Both Modern themes share layout metrics and draw-list chrome. Their opaque
// palettes keep the scene render and its picking area independent of the UI.
namespace ModernChrome {
constexpr ImVec4 color(unsigned rgb, float alpha = 1) {
  return {float((rgb >> 16) & 255)/255, float((rgb >> 8) & 255)/255,
          float(rgb & 255)/255, alpha};
}
struct Palette {
  ImVec4 workspace, dock, panel, inset, field, elevated;
  ImVec4 ink, secondary, border, hover, active, teal, focus, danger, warning;
  ImVec4 nav, navText, navMuted, navHover, navActive, navInput, navTop;
  ImVec4 navInputHover, navInputBorder;
  ImVec4 navRecess, navRecessHover, navRecessEdge, navRecessLight;
  ImVec4 scrollbar, scrollbarHover, edgeHover, resize, shadow, dim;
  ImVec4 action, actionHover, tileOutline;
};

constexpr Palette light{
  color(0xD3D5D7),color(0xDADCDE),color(0xE0E2E4),color(0xD7DADC),color(0xE7E9EB),color(0xEBEDEF),
  color(0x282B30),color(0x53585E),color(0xBEC2C6),color(0xD3D8DC),color(0xC8D4D2),
  color(0x4E817A),color(0x396A63),color(0x913D3B),color(0x705820),
  color(0xCDD1D5),color(0x282B30),color(0x53585E),color(0xBFC5CB),color(0xB3CBC5),color(0xEFF1F3),color(0xD6DADD),
  color(0xE1E8EC),color(0x9DA4AD),
  color(0xBFC5CB),color(0xCBD1D6),color(0xA4ABB2),color(0xEBEDF0,.74f),
  color(0x414141,.25f),color(0x414141,.42f),color(0x454745,.50f),color(0x343434),color(0x252729),color(0x252729),
  color(0x396A63),color(0x4E817A),color(0xFFFFFF,.48f)
};
constexpr Palette dark{
  color(0x2D2F32),color(0x34363A),color(0x393C40),color(0x323438),color(0x33363A),color(0x42454A),
  color(0xEDF0F2),color(0xBFC3C8),color(0x55595E),color(0x484C52),color(0x455452),
  color(0x75B3A7),color(0xA1D5C9),color(0xF7A59B),color(0xE9C788),
  color(0x34373C),color(0xEDF0F2),color(0xB1B6BD),color(0x464A51),color(0x49635F),color(0x51565E),color(0x3B3E43),
  color(0x5D646D),color(0x777D86),
  color(0x26292D),color(0x30353B),color(0x191C20),color(0x626972,.62f),
  color(0xCBD7D2,.24f),color(0xCBD7D2,.43f),color(0xCBD7D2,.40f),color(0xCBD7D2),color(0x000000),color(0x080A0A),
  color(0x30564F),color(0x39685E),color(0xA1D5C9,.70f)
};
constexpr auto transparent = color(0,0);
constexpr float gutter = 15.f, margin = 17.f, panelRadius = 13.f;
constexpr float railWidth = 94.f;
constexpr bool usesModern(int theme) { return theme == 0 || theme == 3; }
constexpr const Palette& palette(int theme) { return theme == 3 ? light : dark; }
constexpr ImVec4 alpha(ImVec4 color, float opacity) {
  return {color.x,color.y,color.z,opacity};
}
inline void apply(ImGuiStyle& s, int theme) {
  const auto& p=palette(theme);
  if(theme==3)ImGui::StyleColorsLight(&s);
  else ImGui::StyleColorsDark(&s);
  auto& c=s.Colors;
  c[ImGuiCol_Text]=p.ink;c[ImGuiCol_TextDisabled]=p.secondary;
  c[ImGuiCol_WindowBg]=p.panel;c[ImGuiCol_ChildBg]=p.inset;c[ImGuiCol_PopupBg]=p.elevated;
  c[ImGuiCol_Border]=p.border;c[ImGuiCol_BorderShadow]=color(0xFFFFFF,theme==3?.16f:.035f);
  c[ImGuiCol_FrameBg]=p.field;c[ImGuiCol_FrameBgHovered]=p.hover;c[ImGuiCol_FrameBgActive]=p.active;
  c[ImGuiCol_TitleBg]=c[ImGuiCol_TitleBgActive]=c[ImGuiCol_TitleBgCollapsed]=p.panel;
  c[ImGuiCol_MenuBarBg]=p.dock;
  c[ImGuiCol_ScrollbarBg]=transparent;c[ImGuiCol_ScrollbarGrab]=p.scrollbar;
  c[ImGuiCol_ScrollbarGrabHovered]=p.scrollbarHover;c[ImGuiCol_ScrollbarGrabActive]=p.secondary;
  c[ImGuiCol_CheckMark]=p.focus;c[ImGuiCol_SliderGrab]=p.teal;c[ImGuiCol_SliderGrabActive]=p.focus;
  c[ImGuiCol_Button]=p.elevated;c[ImGuiCol_ButtonHovered]=p.hover;c[ImGuiCol_ButtonActive]=p.active;
  c[ImGuiCol_Header]=p.active;c[ImGuiCol_HeaderHovered]=p.hover;c[ImGuiCol_HeaderActive]=p.active;
  c[ImGuiCol_Separator]=p.border;c[ImGuiCol_SeparatorHovered]=p.edgeHover;c[ImGuiCol_SeparatorActive]=p.focus;
  c[ImGuiCol_ResizeGrip]=alpha(p.resize,.12f);c[ImGuiCol_ResizeGripHovered]=alpha(p.resize,.32f);
  c[ImGuiCol_ResizeGripActive]=alpha(p.resize,.45f);
  c[ImGuiCol_Tab]=c[ImGuiCol_TabDimmed]=transparent;
  c[ImGuiCol_TabHovered]=p.hover;c[ImGuiCol_TabSelected]=c[ImGuiCol_TabDimmedSelected]=p.active;
  c[ImGuiCol_TabSelectedOverline]=c[ImGuiCol_TabDimmedSelectedOverline]=transparent;
  c[ImGuiCol_DockingPreview]=alpha(p.teal,.22f);c[ImGuiCol_DockingEmptyBg]=transparent;
  c[ImGuiCol_PlotLines]=p.teal;c[ImGuiCol_PlotLinesHovered]=p.focus;
  c[ImGuiCol_PlotHistogram]=p.teal;c[ImGuiCol_PlotHistogramHovered]=p.focus;
  c[ImGuiCol_TableHeaderBg]=p.inset;c[ImGuiCol_TableBorderStrong]=p.border;
  c[ImGuiCol_TableBorderLight]=p.border;c[ImGuiCol_TableRowBg]=transparent;
  c[ImGuiCol_TableRowBgAlt]=alpha(p.inset,.55f);c[ImGuiCol_TextLink]=p.focus;
  c[ImGuiCol_TextSelectedBg]=alpha(p.teal,.22f);c[ImGuiCol_DragDropTarget]=p.focus;
  c[ImGuiCol_NavCursor]=p.focus;c[ImGuiCol_NavWindowingHighlight]=alpha(p.teal,.65f);
  c[ImGuiCol_NavWindowingDimBg]=alpha(p.dim,theme==3?.12f:.32f);c[ImGuiCol_ModalWindowDimBg]=alpha(p.dim,theme==3?.18f:.58f);
  s.WindowRounding=panelRadius;s.ChildRounding=11;s.PopupRounding=14;s.FrameRounding=8;
  s.GrabRounding=4;s.ScrollbarRounding=6;s.TabRounding=8;
  s.WindowBorderSize=s.ChildBorderSize=s.FrameBorderSize=1;s.PopupBorderSize=1;s.TabBorderSize=0;
  s.TabBarBorderSize=0;s.TabBarOverlineSize=0;
  s.WindowPadding={10,9};s.FramePadding={7,4};s.ItemSpacing={7,5};s.ItemInnerSpacing={5,4};
  s.CellPadding={6,4};s.IndentSpacing=18;s.ScrollbarSize=9;s.GrabMinSize=10;
  s.SeparatorTextBorderSize=1;s.SeparatorTextPadding={0,5};s.DockingSeparatorSize=gutter;
  s.DisabledAlpha=.48f;
}

}
static bool uiModern(){return ModernChrome::usesModern(uiTheme);}
static const ModernChrome::Palette& uiModernPalette(){return ModernChrome::palette(uiTheme);}
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
static void uiSoftSurface(ImDrawList* draw,const ImRect& bounds,float radius,bool toolStrip=false,
                          const ImRect* scene=nullptr) {
  if(bounds.IsInverted() || bounds.GetWidth()<1 || bounds.GetHeight()<1)return;
  auto paint=[&]{
    const float scale=uiScale();
    const auto& p=uiModernPalette();
    if(!toolStrip)for(int spread:{7,4,2}){
      const float pad=spread*scale;
      draw->AddRectFilled({bounds.Min.x-pad,bounds.Min.y+2*scale-pad},
        {bounds.Max.x+pad,bounds.Max.y+3*scale+pad},
        ImGui::GetColorU32(ModernChrome::alpha(p.shadow,spread==2?.032f:.019f)),radius+pad);
    }
    const ImU32 top=ImGui::GetColorU32(toolStrip?p.navTop:p.elevated);
    const ImU32 bottom=ImGui::GetColorU32(toolStrip?p.nav:p.panel);
    const int first=draw->VtxBuffer.Size;
    draw->AddRectFilled(bounds.Min,bounds.Max,top,radius);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw,first,draw->VtxBuffer.Size,
      bounds.Min,{bounds.Min.x,bounds.Max.y},top,bottom);
    draw->AddRect(bounds.Min,bounds.Max,ImGui::GetColorU32(toolStrip?ModernChrome::color(0xFFFFFF,.09f):p.border),radius);
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
  if(!uiModern())return ImGui::Begin(name,open,flags);
  if(uiWorkspaceDocked(ImGui::FindWindowByName(name)))flags|=ImGuiWindowFlags_NoBackground;
  const bool visible=ImGui::Begin(name,open,flags);
  const auto* window=ImGui::GetCurrentWindow();
  if(uiWorkspaceDocked(window) && window->DockTabIsVisible && !window->Hidden) {
    uiSoftSurface(ImGui::GetBackgroundDrawList(),window->Rect(),ModernChrome::panelRadius*uiScale());
  }
  return visible;
}
// Explicit roles keep asset pixels, scene overlays, axes and diagnostic colors
// independent from interface theming. All other themes retain their exact color.
enum class UiTone {Inset, Raised, Selection, Muted, Warning, Error, Ink};
static ImVec4 uiTone(UiTone role) {
  const auto& p=uiModernPalette();
  switch(role){
    case UiTone::Inset:return p.inset;case UiTone::Raised:return p.elevated;
    case UiTone::Selection:return p.teal;case UiTone::Muted:return p.secondary;
    case UiTone::Warning:return p.warning;case UiTone::Error:return p.danger;
    default:return p.ink;
  }
}
static ImU32 uiChrome(ImU32 legacy, UiTone role) {
  return uiModern()?ImGui::GetColorU32(uiTone(role)):legacy;
}
static ImVec4 uiActiveToolColor(){return uiModern()?uiModernPalette().navActive:uiColor(62,112,205);}
template<class Draw>
static bool uiToolDropdown(Draw draw)
{
  if(!uiModern())return draw();
  const auto& palette=uiModernPalette();
  ImGui::PushStyleColor(ImGuiCol_FrameBg,palette.navInput);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,palette.navInputHover);
  ImGui::PushStyleColor(ImGuiCol_Button,palette.navInput);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,palette.navInputHover);
  ImGui::PushStyleColor(ImGuiCol_Border,palette.navInputBorder);
  ImGui::PushStyleColor(ImGuiCol_BorderShadow,ModernChrome::transparent);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,uiScale());
  const bool result=draw();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(6);
  return result;
}
template<class... Args>
static bool uiToolCombo(Args&&... args)
{
  return uiToolDropdown([&]{return ImGui::Combo(std::forward<Args>(args)...);});
}
static bool uiToolValueCombo(const char* id,const char* value,ImVec2 cell)
{
  const float scale=uiScale();
  const auto min=ImGui::GetCursorScreenPos();
  const ImVec2 max{min.x+cell.x,min.y+cell.y};
  const bool visible=ImGui::IsRectVisible(min,max);
  auto* draw=ImGui::GetWindowDrawList();
  auto* font=ImGui::GetFont();
  const float fontSize=ImGui::GetFontSize();
  const auto ink=ImGui::GetColorU32(ImGuiCol_Text);
  const float radius=ImGui::GetStyle().FrameRounding;
  const bool modern=uiModern();
  const auto& palette=uiModernPalette();
  if(modern){
    ImGui::PushStyleColor(ImGuiCol_FrameBg,palette.navRecess);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,palette.navRecessHover);
  }
  ImGui::SetNextItemWidth(cell.x);
  ImGui::SetNextWindowSizeConstraints({128*scale,0},{FLT_MAX,FLT_MAX});
  const bool open=ImGui::BeginCombo(id,"",ImGuiComboFlags_NoArrowButton);
  if(modern)ImGui::PopStyleColor(2);
  if(!visible)return open;
  if(modern){
    draw->AddRect(min,max,ImGui::GetColorU32(palette.navRecessEdge),radius);
    draw->AddLine({min.x+radius,max.y-scale},{max.x-radius,max.y-scale},
      ImGui::GetColorU32(palette.navRecessLight),scale);
  }
  const auto text=font->CalcTextSizeA(fontSize,FLT_MAX,0,value);
  const float fit=std::min(1.f,(cell.x-6*scale)/std::max(1.f,text.x));
  const float centerX=min.x+cell.x*.5f;
  draw->AddText(font,fontSize*fit,
    {std::floor(centerX-text.x*fit*.5f),std::floor(min.y+cell.y*.39f-text.y*fit*.5f)},
    ink,value);
  const float arrowY=min.y+cell.y*.75f;
  const ImVec2 arrow[]={{centerX-3.4f*scale,arrowY-1.5f*scale},
    {centerX,arrowY+1.8f*scale},{centerX+3.4f*scale,arrowY-1.5f*scale}};
  draw->AddPolyline(arrow,IM_ARRAYSIZE(arrow),ink,0,1.3f*scale);
  return open;
}
// This scope only surrounds Tools. Other widgets use the workspace palette.
static void uiToolStripBegin() {
  if(!uiModern())return;
  const auto& p=uiModernPalette();
  const std::pair<ImGuiCol,ImVec4> colors[]={
    {ImGuiCol_Text,p.navText},{ImGuiCol_TextDisabled,p.navMuted},
    {ImGuiCol_Button,ModernChrome::transparent},{ImGuiCol_ButtonHovered,p.navHover},
    {ImGuiCol_ButtonActive,p.navActive},{ImGuiCol_FrameBg,p.navInput},
    {ImGuiCol_FrameBgHovered,p.navHover},{ImGuiCol_FrameBgActive,p.navActive},
    {ImGuiCol_Border,p.navHover},{ImGuiCol_Separator,p.navHover},
    {ImGuiCol_CheckMark,p.navText},{ImGuiCol_PopupBg,p.nav},
    {ImGuiCol_Header,p.navActive},{ImGuiCol_HeaderHovered,p.navHover},
    {ImGuiCol_HeaderActive,p.navActive}};
  for(auto& pair:colors)ImGui::PushStyleColor(pair.first,pair.second);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
}
static ImDrawListSplitter uiToolStripSplitter;
static ImVec2 uiToolStripOrigin,uiToolStripAvailable;
static bool uiToolStripDrawing=false;
static void uiToolStripSurface(){
  if(!uiModern()||ImGui::GetCurrentWindow()->SkipItems)return;
  uiToolStripOrigin=ImGui::GetCursorScreenPos();uiToolStripAvailable=ImGui::GetContentRegionAvail();
  auto d=ImGui::GetWindowDrawList();uiToolStripSplitter.Split(d,2);uiToolStripSplitter.SetCurrentChannel(d,1);uiToolStripDrawing=true;
}
static void uiToolStripEnd(bool fitContents=false){
  if(!uiModern())return;
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
  if(!uiModern())return ImGui::InputTextWithHint(id,hint,text,capacity);
  const auto pos=ImGui::GetCursorScreenPos();
  auto padding=ImGui::GetStyle().FramePadding;
  padding.x+=14*uiScale();
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,padding);
  const bool changed=ImGui::InputTextWithHint(id,hint,text,capacity);
  ImGui::PopStyleVar();
  const float scale=uiScale(),centerY=pos.y+ImGui::GetItemRectSize().y*.5f;
  auto* draw=ImGui::GetWindowDrawList();
  const auto color=ImGui::GetColorU32(uiModernPalette().secondary);
  draw->AddCircle({pos.x+10*scale,centerY-scale},4*scale,color,16,scale);
  draw->AddLine({pos.x+13*scale,centerY+2*scale},{pos.x+17*scale,centerY+6*scale},color,scale);
  return changed;
}
static bool uiInspectorFold(const char* label,ImGuiTreeNodeFlags flags){
  if(uiModern())ImGui::PushStyleColor(ImGuiCol_Header,ModernChrome::transparent);
  const bool open=ImGui::CollapsingHeader(label,flags);
  if(uiModern())ImGui::PopStyleColor();
  return open;
}
// Reuse the existing glyph-complete bold font where available. Other themes
// and platforms without a bold resource retain the existing separator behavior.
static void uiSectionHeading(const char* label){
  bool bold=uiModern()&&uiBoldFonts[uiScaleChoice];
  if(bold)ImGui::PushFont(uiBoldFonts[uiScaleChoice]);
  ImGui::SeparatorText(label);
  if(bold)ImGui::PopFont();
}
