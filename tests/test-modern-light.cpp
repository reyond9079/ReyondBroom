// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone ImGui style/compatibility checks. No FTE renderer is required.
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
void FteBroomTranslate(const char*&,const char*&){}
#ifndef THEME_HEADER
#define THEME_HEADER "../native/ftebroom_appearance.h"
#endif
#include THEME_HEADER
static uint64_t signature() {
  uint64_t h=1469598103934665603ull;
  auto hash=[&](const void* data,size_t size){auto bytes=static_cast<const unsigned char*>(data);for(size_t n=0;n<size;n++){h^=bytes[n];h*=1099511628211ull;}};
  auto& s=ImGui::GetStyle();hash(s.Colors,sizeof(s.Colors));
  for(int n=0;n<ImGuiStyleVar_COUNT;n++){auto info=ImGui::GetStyleVarInfo(n);hash(info->GetVarPtr(&s),info->Count*sizeof(float));}
  hash(&s.AntiAliasedLines,sizeof(s.AntiAliasedLines));hash(&s.AntiAliasedFill,sizeof(s.AntiAliasedFill));
  hash(&s.CircleTessellationMaxError,sizeof(float));return h;
}
static double luminance(ImVec4 c){auto linear=[](float x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};return .2126*linear(c.x)+.7152*linear(c.y)+.0722*linear(c.z);}
static double contrast(ImVec4 a,ImVec4 b){auto x=luminance(a),y=luminance(b);return (std::max(x,y)+.05)/(std::min(x,y)+.05);}
int main(int argc,char** argv){
  ImGui::CreateContext();
  std::ostringstream snapshots;
  for(int theme:{1,2,4,5})for(int scale=0;scale<5;scale++){
    uiTheme=theme;uiScaleChoice=scale;uiStyle();snapshots<<theme<<' '<<scale<<' '<<signature()<<'\n';
  }
  if(argc>1&&std::string(argv[1])=="--snapshot"){std::cout<<snapshots.str();return 0;}
  std::ifstream baseline("tests/fixtures/modern-light-legacy-style.txt");std::string expected((std::istreambuf_iterator<char>(baseline)),{});
  expected.erase(std::remove(expected.begin(),expected.end(),'\r'),expected.end());
  if(expected!=snapshots.str()){std::cerr<<"Unrelated theme changed\n";return 1;}
#ifndef BASELINE_ONLY
  for(int theme:{0,3})for(int scale=0;scale<5;scale++){
    uiTheme=theme;uiScaleChoice=scale;uiStyle();auto& s=ImGui::GetStyle();
    if(s.WindowRounding!=std::floor(ModernChrome::panelRadius*uiScale())||s.FrameRounding!=std::floor(8*uiScale())||
       s.DockingSeparatorSize!=std::floor(ModernChrome::gutter*uiScale())||s.WindowPadding.x>10*uiScale()+1)return 2;
    for(auto c:s.Colors)for(float value:{c.x,c.y,c.z,c.w})if(!std::isfinite(value)||value<0||value>1)return 3;
    const auto& p=uiModernPalette();
    if(contrast(p.ink,p.panel)<7||contrast(p.secondary,p.panel)<4.5||
       contrast(p.navText,p.nav)<7||contrast(p.focus,p.elevated)<4.5||
       contrast(ModernChrome::color(0xFFFFFF),p.action)<4.5)return 4;
    if(luminance(p.workspace)>=luminance(p.panel)||luminance(p.nav)>=luminance(p.panel))return 9;
    auto before=signature();int stack=ImGui::GetCurrentContext()->ColorStack.Size;
    uiToolStripBegin();if(ImGui::GetCurrentContext()->ColorStack.Size!=stack+15)return 5;uiToolStripEnd();
    if(signature()!=before||ImGui::GetCurrentContext()->ColorStack.Size!=stack)return 6;
    if(uiChrome(IM_COL32(4,5,6,255),UiTone::Inset)==IM_COL32(4,5,6,255))return 7;
  }
  uiTheme=2;if(uiChrome(IM_COL32(4,5,6,255),UiTone::Inset)!=IM_COL32(4,5,6,255))return 8;
  std::cout<<"PASS: 20 unchanged legacy theme/scale signatures; both Modern themes at all five scales, palette/contrast, scoped color restoration and legacy role passthrough\n";
#endif
  ImGui::DestroyContext();
}
