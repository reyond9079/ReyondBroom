// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace {
enum class JamTone { Neutral, Free, Mine, Occupied, Hub, Action };

struct JamColors {
  ImU32 top, bottom, side, edge, accent;
};

constexpr JamColors jamColors(JamTone tone)
{
  switch (tone) {
    case JamTone::Free: return {IM_COL32(43,76,106,255),IM_COL32(27,44,63,255),IM_COL32(12,24,40,255),
      IM_COL32(73,114,145,255),IM_COL32(160,217,251,255)};
    case JamTone::Mine: return {IM_COL32(39,105,95,255),IM_COL32(18,56,54,255),IM_COL32(9,30,30,255),
      IM_COL32(76,160,143,255),IM_COL32(149,243,211,255)};
    case JamTone::Occupied: return {IM_COL32(91,56,114,255),IM_COL32(52,30,71,255),IM_COL32(29,18,42,255),
      IM_COL32(134,91,163,255),IM_COL32(224,179,250,255)};
    case JamTone::Hub: return {IM_COL32(52,127,145,255),IM_COL32(22,67,86,255),IM_COL32(10,31,47,255),
      IM_COL32(102,204,217,255),IM_COL32(164,249,246,255)};
    case JamTone::Action: return {IM_COL32(46,115,101,255),IM_COL32(23,67,58,255),IM_COL32(10,33,32,255),
      IM_COL32(95,186,153,255),IM_COL32(178,248,222,255)};
    default: return {IM_COL32(53,74,101,255),IM_COL32(27,39,57,255),IM_COL32(15,22,35,255),
      IM_COL32(82,106,138,255),IM_COL32(191,217,245,255)};
  }
}

ImU32 jamTint(ImU32 color, float amount)
{
  auto v = ImGui::ColorConvertU32ToFloat4(color);
  return ImGui::GetColorU32(ImVec4(v.x+(1.f-v.x)*amount,v.y+(1.f-v.y)*amount,v.z+(1.f-v.z)*amount,v.w));
}

struct JamWindowStyle {
  JamWindowStyle()
  {
    const float u = uiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {24*u,18*u});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5*u);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1*u);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10*u,7*u});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {9*u,8*u});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.055f,.074f,.108f,1));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.08f,.108f,.155f,1));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(.075f,.10f,.15f,1));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(.09f,.135f,.20f,1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.24f,.33f,.45f,1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.91f,.95f,1,1));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(.61f,.70f,.81f,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.08f,.12f,.18f,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(.15f,.21f,.30f,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(.18f,.26f,.37f,1));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(.12f,.19f,.28f,1));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(.19f,.29f,.40f,1));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(.25f,.35f,.47f,1));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(.56f,.85f,.94f,1));
  }

  ~JamWindowStyle()
  {
    ImGui::PopStyleColor(14);
    ImGui::PopStyleVar(7);
  }
};

std::array<ImVec2,8> jamCorners(ImVec2 tl, ImVec2 br, float cut)
{
  return {{{tl.x+cut,tl.y},{br.x-cut,tl.y},{br.x,tl.y+cut},{br.x,br.y-cut},
           {br.x-cut,br.y},{tl.x+cut,br.y},{tl.x,br.y-cut},{tl.x,tl.y+cut}}};
}

float jamPressOffset(float depth, bool active)
{
  return active ? depth*.78f : 0.f;
}

void jamFace(ImDrawList* list, const ImRect& area, JamTone tone, float depth, bool hovered, bool active, bool selected)
{
  const float u = uiScale();
  const auto colors = jamColors(tone);
  if (area.GetWidth()<6*u || area.GetHeight()<6*u) return;
  depth = std::min(depth,area.GetHeight()*.35f);
  const float press = jamPressOffset(depth,active);
  const float shadow = std::min(3*u,depth*.24f);
  const ImVec2 tl{area.Min.x+u,area.Min.y+press};
  const ImVec2 br{area.Max.x-u,area.Max.y-depth+press-shadow};
  const float cut = std::min({10*u,(br.x-tl.x)*.15f,(br.y-tl.y)*.20f});
  const auto face = jamCorners(tl,br,cut);
  const auto base = jamCorners({tl.x,area.Min.y+depth},{br.x,area.Max.y-shadow},cut);
  const auto foot = jamCorners({area.Min.x,area.Min.y+depth},{area.Max.x,area.Max.y},cut);
  list->AddConvexPolyFilled(foot.data(),static_cast<int>(foot.size()),ImGui::GetColorU32(ImVec4(0,0,0,.68f)));
  list->AddConvexPolyFilled(base.data(),static_cast<int>(base.size()),jamTint(colors.side,0));
  for (int i=2;i<=6;++i) {
    const auto wall = i==4 ? jamTint(colors.side,.11f) : jamTint(colors.side,.05f);
    list->AddQuadFilled(face[i],face[i+1],base[i+1],base[i],wall);
  }
  const float bevel = std::min({4*u,cut*.55f,(br.y-tl.y)*.12f});
  const ImVec2 innerTL{tl.x+bevel,tl.y+bevel}, innerBR{br.x-bevel,br.y-bevel};
  const auto inner = jamCorners(innerTL,innerBR,std::max(0.f,cut-bevel*.6f));
  const float glow = hovered ? .16f : 0.f;
  for (int i=0;i<8;++i) {
    const int next = (i+1)%8;
    const auto tint = i==0 || i==1 || i==7 ? jamTint(colors.edge,.30f+glow) :
                      i==3 || i==4 || i==5 ? jamTint(colors.side,.08f) : jamTint(colors.edge,glow);
    list->AddQuadFilled(face[i],face[next],inner[next],inner[i],tint);
  }
  const int first = list->VtxBuffer.Size;
  list->AddConvexPolyFilled(inner.data(),static_cast<int>(inner.size()),jamTint(colors.top,glow));
  ImGui::ShadeVertsLinearColorGradientKeepAlpha(list,first,list->VtxBuffer.Size,innerTL,{innerTL.x,innerBR.y},
    jamTint(colors.top,glow),jamTint(colors.bottom,glow*.55f));
  list->AddPolyline(face.data(),static_cast<int>(face.size()),jamTint(selected ? colors.accent : colors.edge,glow),
                    ImDrawFlags_Closed,(selected ? 2.f : 1.f)*u);
  list->AddLine(face[0],face[1],jamTint(colors.accent,glow),u);
  if (!active)
    list->AddLine({base[5].x,base[5].y-u},{base[4].x,base[4].y-u},jamTint(colors.side,.18f),u);
}

void jamCenteredText(ImDrawList* list, const ImRect& area, float centerY, const char* text, float fontSize, ImU32 color)
{
  const auto width = ImGui::GetFont()->CalcTextSizeA(fontSize,FLT_MAX,0,text).x;
  const ImVec2 pos{area.GetCenter().x-width*.5f,centerY-fontSize*.5f};
  const ImVec4 clip{area.Min.x+2,area.Min.y,area.Max.x-2,area.Max.y};
  list->AddText(ImGui::GetFont(),fontSize,{pos.x,pos.y+uiScale()},ImGui::GetColorU32(ImVec4(0,0,0,.47f)),text,nullptr,0,&clip);
  list->AddText(ImGui::GetFont(),fontSize,pos,color,text,nullptr,0,&clip);
}

bool jamButton(const char* label, ImVec2 size = {}, JamTone tone = JamTone::Neutral)
{
  const char* end = ImGui::FindRenderedTextEnd(label);
  const std::string visible(label,end);
  const auto text = ImGui::CalcTextSize(label,end);
  const float u = uiScale(), depth = 9*u;
  const auto available = ImGui::GetContentRegionAvail();
  size.x = size.x < 0 ? available.x : size.x ? size.x : text.x+26*u;
  size.x = std::max(1.f,size.x);
  size.y = size.y > 0 ? size.y : ImGui::GetFrameHeight()+depth+3*u;
  const bool clicked = ImGui::InvisibleButton(label,size,ImGuiButtonFlags_EnableNav);
  if (!ImGui::IsItemVisible()) return clicked;
  const bool active = ImGui::IsItemActive(), hovered = ImGui::IsItemHovered();
  const ImRect area{ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
  auto* list = ImGui::GetWindowDrawList();
  jamFace(list,area,tone,depth,hovered,active,ImGui::IsItemFocused());
  jamCenteredText(list,area,area.GetCenter().y-depth*.6f+jamPressOffset(depth,active),
                  visible.c_str(),ImGui::GetFontSize(),ImGui::GetColorU32(ImGuiCol_Text));
  return clicked;
}
} // namespace
