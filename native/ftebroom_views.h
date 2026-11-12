// SPDX-License-Identifier: GPL-2.0-or-later
struct ViewRect
{
  float x = 0, y = 0, w = 0, h = 0;
  int type = 0;
};
static ViewRect viewRects[4], viewArea;
static int viewLayout = 1, viewFocus = 0, viewMax = -1, viewSplitDrag = 0;
static int viewTypes[4] = {0, 3, 2, 1};
static float viewSplitX = .5f, viewSplitY = .5f;
static bool viewFocusArmed = false, viewLoaded = false;
static void viewSave()
{
  if (tourActive || cvars->GetFloat("ftebroom_skip_setup"))
    return;
  std::ofstream file(documentPath("view-layout.cfg"));
  file << viewLayout << ' ' << viewSplitX << ' ' << viewSplitY << '\n';
}
static void viewActivate(int index)
{
  viewFocus = std::clamp(index, 0, viewLayout - 1);
  pointerView=viewFocus; // Queued gesture cancellation must target this same pane.
  cvars->SetFloat("ftebroom_view_active", viewFocus);
  int type = viewTypes[viewFocus];
  char typeKey[64];std::snprintf(typeKey,sizeof typeKey,"ftebroom_view%d_type",viewFocus);
  cvars->SetFloat(typeKey,type);
  cvars->SetFloat("ca_brush_view", type);
  cvars->SetFloat(
    "ftebroom_ctx_view", type == 3 ? 1 : (type == 2 ? 2 : (type == 1 ? 3 : 0)));
}
static void viewSetLayout(int count)
{
  cancelGesture();
  cameraEnd();
  viewMax = -1;
  viewSplitDrag = 0;
  viewLayout = std::clamp(count, 1, 4);
  viewTypes[0] = 0;
  viewTypes[1] = 3;
  viewTypes[2] = 2;
  viewTypes[3] = 1;
  cvars->SetFloat("ftebroom_view_layout", viewLayout);
  viewActivate(0);
  viewSave();
}
static void viewAction(const std::string& name)
{
  cancelGesture();
  cameraEnd();
  if (name == "maximize")
    viewMax = viewMax < 0 ? viewFocus : -1;
  if (name == "cycle")
  {
    // Cycle the focused pane in every layout, leaving the other panes alone.
    int index=std::clamp(viewFocus,0,viewLayout-1);
    int& type=viewTypes[index];
    type=type==0?3:type==3?2:type==2?1:0;
    viewActivate(index);
  }
}
static void viewCompute(float w, float h)
{
  if (!viewLoaded)
  {
    viewLoaded = true;
    if (!cvars->GetFloat("ftebroom_skip_setup"))
    {
      int count = 1;
      float sx = .5f, sy = .5f;
      std::ifstream file(documentPath("view-layout.cfg"));
      if (file >> count >> sx >> sy && std::isfinite(sx) && std::isfinite(sy))
      {
        viewLayout = std::clamp(count, 1, 4);
        viewSplitX = std::clamp(sx, .2f, .8f);
        viewSplitY = std::clamp(sy, .2f, .8f);
      }
    }
    cvars->SetFloat("ftebroom_view_layout", viewLayout);
  }
  int requested = std::clamp((int)cvars->GetFloat("ftebroom_view_layout"), 1, 4);
  if (requested != viewLayout)
    viewSetLayout(requested);
  // Keep legacy console view selection working for the focused viewport.
  viewTypes[viewFocus] = std::clamp((int)cvars->GetFloat("ca_brush_view"), 0, 3);
  float x = 48, y = 114, width = std::max(16.f, w - 48 - (inspector ? 310 : 0)),
        height = std::max(16.f, h - y - 26);
  if (uiHasScene)
  {
    x = uiScenePos.x;
    y = uiScenePos.y;
    width = std::max(1.f, uiSceneSize.x);
    height = std::max(1.f, uiSceneSize.y);
  }
  viewArea = {x, y, width, height, 0};
  float left = width * viewSplitX, top = height * viewSplitY;
  for (int i = 0; i < 4; i++)
    viewRects[i] = {x, y, 0, 0, viewTypes[i]};
  if (viewMax >= 0 || viewLayout == 1)
    viewRects[viewMax >= 0 ? viewMax : 0] = {
      x, y, width, height, viewTypes[viewMax >= 0 ? viewMax : 0]};
  else
  {
    viewRects[0] = {x, y, left - 3, viewLayout == 4 ? top - 3 : height, viewTypes[0]};
    viewRects[1] = {
      x + left + 3,
      y,
      width - left - 3,
      viewLayout >= 3 ? top - 3 : height,
      viewTypes[1]};
    if (viewLayout == 3)
      viewRects[2] = {
        x + left + 3, y + top + 3, width - left - 3, height - top - 3, viewTypes[2]};
    if (viewLayout == 4)
    {
      viewRects[2] = {x, y + top + 3, left - 3, height - top - 3, viewTypes[2]};
      viewRects[3] = {
        x + left + 3, y + top + 3, width - left - 3, height - top - 3, viewTypes[3]};
    }
  }
  cvars->SetFloat("ftebroom_view_count", viewLayout);
  cvars->SetFloat("ftebroom_view_active", viewFocus);
  for (int i = 0; i < 4; i++)
  {
    char key[64], value[160];
    auto r = viewRects[i];
    std::snprintf(key, sizeof key, "ftebroom_view%d_rect", i);
    std::snprintf(value, sizeof value, "%g %g %g %g", r.x, r.y, r.w, r.h);
    cvars->SetString(key, value);
    std::snprintf(key, sizeof key, "ftebroom_view%d_type", i);
    cvars->SetFloat(key, r.type);
  }
}
static int viewHit(float x, float y)
{
  for (int i = 0; i < viewLayout; i++)
  {
    auto r = viewRects[i];
    if (r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
      return i;
  }
  return -1;
}
static int viewDivider(float x, float y, float w, float h)
{
  if (
    viewLayout < 2 || viewMax >= 0 || x < viewArea.x || x >= viewArea.x + viewArea.w
    || y < viewArea.y || y >= viewArea.y + viewArea.h)
    return 0;
  float sx = viewArea.x + viewArea.w * viewSplitX,
        sy = viewArea.y + viewArea.h * viewSplitY;
  if (std::abs(x - sx) <= 3)
    return 1;
  if (viewLayout >= 3 && std::abs(y - sy) <= 3 && (viewLayout == 4 || x > sx))
    return 2;
  return 0;
}
static void viewOverlay()
{
  const auto mouse = ImGui::GetIO().MousePos;
  const int divider = viewSplitDrag ? viewSplitDrag : viewDivider(mouse.x, mouse.y, 0, 0);
  if (divider && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
    ImGui::SetMouseCursor(divider == 1 ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
  auto list = ImGui::GetBackgroundDrawList();
  if (viewLayout > 1 && viewMax < 0)
  {
    auto a = viewRects[0], b = viewRects[1];
    list->AddRectFilled(
      {a.x + a.w, a.y}, {b.x, viewArea.y + viewArea.h}, uiChrome(IM_COL32(85, 85, 90, 255),UiTone::Muted));
    if (viewLayout >= 3)
    {
      auto lower = viewRects[viewLayout == 3 ? 2 : 3];
      list->AddRectFilled(
        {viewLayout == 3 ? b.x : a.x, b.y + b.h},
        {b.x + b.w, lower.y},
        uiChrome(IM_COL32(85, 85, 90, 255),UiTone::Muted));
    }
  }
  const char* names[] = {"3D", "YZ / Side", "XZ / Front", "XY / Top"};
  for (int i = 0; i < viewLayout; i++)
  {
    auto r = viewRects[i];
    if (r.w <= 0)
      continue;
    list->AddRect(
      {r.x, r.y},
      {r.x + r.w, r.y + r.h},
      i == viewFocus ? uiChrome(IM_COL32(62, 112, 205, 255),UiTone::Selection) : uiChrome(IM_COL32(100, 100, 110, 255),UiTone::Muted),
      0,
      0,
      i == viewFocus ? 2 : 1);
    if(uiModern()) {
      const auto label=ImGui::CalcTextSize(names[r.type]);
      ImVec2 lo{r.x+3,r.y+3},hi{r.x+std::min(r.w-3,label.x+16*uiScale()),r.y+label.y+11*uiScale()};
      list->AddRectFilled(lo,hi,ImGui::GetColorU32(uiModernPalette().panel),4*uiScale());
      list->AddRect(lo,hi,ImGui::GetColorU32(uiModernPalette().border),4*uiScale());
      list->AddText({lo.x+6*uiScale(),lo.y+4*uiScale()},ImGui::GetColorU32(uiModernPalette().ink),names[r.type]);
    } else {
    list->AddRectFilled(
      {r.x + 3, r.y + 3}, {r.x + 100, r.y + 23}, IM_COL32(25, 25, 30, 210));
    list->AddText({r.x + 8, r.y + 6}, IM_COL32(240, 240, 245, 255), names[r.type]);
    }
  }
}
static void viewPreferences()
{
  uiSectionHeading("View layout");
  int count = viewLayout - 1;
  if (ImGui::Combo("Views", &count, "1 view\0 2 views\0 3 views\0 4 views\0"))
    viewSetLayout(count + 1);
  ImGui::TextUnformatted("Space: cycle | Ctrl+Space: maximize | Ctrl+Shift+U: reset 2D");
}
