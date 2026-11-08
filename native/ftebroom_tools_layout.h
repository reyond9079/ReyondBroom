// SPDX-License-Identifier: GPL-2.0-or-later
#include <map>
struct UiToolsFlowState{
  ImVec2 observedSize{};
  float fittedHeight=-1;
  bool horizontal=false;
};
static std::map<ImGuiID,UiToolsFlowState> uiToolsFlowStates;
// Dock direction, rather than the current height, determines toolbar flow. An
// expanded horizontal toolbar must not turn into a vertical rail mid-resize.
static bool uiToolsHorizontal(const ImGuiWindow* window)
{
  auto [entry,inserted]=uiToolsFlowStates.try_emplace(window->ID);
  auto& flow=entry->second;
  const auto* node=window->DockNode;
  if(node&&node->ParentNode){
    flow.horizontal=node->ParentNode->SplitAxis==ImGuiAxis_Y;
  }else{
    const bool resized=window->Size.x!=flow.observedSize.x||window->Size.y!=flow.observedSize.y;
    const bool ownHeightChange=flow.fittedHeight>=0&&std::abs(window->Size.y-flow.fittedHeight)<1&&std::abs(window->Size.x-flow.observedSize.x)<1;
    // A user width/height resize chooses the orientation. Applying our own
    // natural content height only resizes the window, preserving its flow.
    if(inserted||(resized&&!ownHeightChange))flow.horizontal=window->Size.x>=window->Size.y;
  }
  flow.observedSize=window->Size;flow.fittedHeight=-1;
  return flow.horizontal;
}
static void uiToolsFitHeight(ImGuiWindow* window,float contentHeight,float scale)
{
  auto* node=window->DockNode;
  // A shared tab group belongs to all its panels; resizing it around Tools
  // would also resize the user's inspector or browser.
  if(node&&(!node->ParentNode||node->ParentNode->SplitAxis!=ImGuiAxis_Y||node->Windows.Size!=1))return;
  const float available=node?node->ParentNode->Size.y:ImGui::GetMainViewport()->WorkSize.y;
  const float maximum=std::max(ImGui::GetStyle().WindowMinSize.y,available-std::min(160*scale,available*.3f));
  const float target=std::clamp(std::ceil(contentHeight),ImGui::GetStyle().WindowMinSize.y,maximum);
  if(std::abs(window->Size.y-target)<1)return;
  if(node){
    node->Size.y=node->SizeRef.y=target;
    node->WantLockSizeOnce=true;
    const int other=node->ParentNode->ChildNodes[0]==node?1:0;
    node->ParentNode->ChildNodes[other]->WantLockSizeOnce=false;
  }else{
    auto found=uiToolsFlowStates.find(window->ID);
    if(found!=uiToolsFlowStates.end())found->second.fittedHeight=target;
    ImGui::SetWindowSize(window,{window->Size.x,target},ImGuiCond_Always);
  }
  ImGui::MarkIniSettingsDirty(window);
}
