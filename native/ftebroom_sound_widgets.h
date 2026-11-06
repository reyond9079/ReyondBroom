// SPDX-License-Identifier: GPL-2.0-or-later
struct SoundStyle {
  int colors = 0;
  SoundStyle() {
    if (uiTheme == 0) {
      auto c = [&](ImGuiCol id, unsigned rgb) {
        ImGui::PushStyleColor(id, ModernChrome::color(rgb));
        colors++;
      };
      c(ImGuiCol_WindowBg, 0x17171A);
      c(ImGuiCol_ChildBg, 0x1E1E21);
      c(ImGuiCol_PopupBg, 0x242428);
      c(ImGuiCol_Border, 0x35353B);
      c(ImGuiCol_Text, 0xE9E9EE);
      c(ImGuiCol_TextDisabled, 0x96969F);
      c(ImGuiCol_Button, 0x303035);
      c(ImGuiCol_ButtonHovered, 0x3D3D45);
      c(ImGuiCol_ButtonActive, 0x31585C);
      c(ImGuiCol_FrameBg, 0x303035);
      c(ImGuiCol_FrameBgHovered, 0x3D3D45);
      c(ImGuiCol_FrameBgActive, 0x42424D);
      c(ImGuiCol_Header, 0x27464C);
      c(ImGuiCol_HeaderHovered, 0x2B454C);
      c(ImGuiCol_HeaderActive, 0x31585C);
      c(ImGuiCol_CheckMark, 0x11C5CA);
      c(ImGuiCol_SliderGrab, 0xD4D4DE);
      c(ImGuiCol_SliderGrabActive, 0xFFFFFF);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5 * uiScale());
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4 * uiScale());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        {7 * uiScale(), 4 * uiScale()});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        {9 * uiScale(), 9 * uiScale()});
  }
  ~SoundStyle() {
    ImGui::PopStyleVar(5);
    if (colors)
      ImGui::PopStyleColor(colors);
  }
};
static bool soundIcon(const char *id, const char *title, bool selected = false,
                      float size = 28) {
  auto pos = ImGui::GetCursorScreenPos();
  float scale = uiScale();
  size *= scale;
  bool click = ImGui::InvisibleButton(id, {size, size});
  auto d = ImGui::GetWindowDrawList();
  if (selected || ImGui::IsItemHovered())
    d->AddRectFilled(pos, {pos.x + size, pos.y + size},
                     ImGui::GetColorU32(selected ? ImGuiCol_HeaderActive
                                                 : ImGuiCol_ButtonHovered),
                     3 * scale);
  auto color =
      ImGui::GetColorU32(selected ? ImGuiCol_CheckMark : ImGuiCol_Text);
  float u = size / 24.f;
  auto pt = [&](float x, float y) {
    return ImVec2(pos.x + x * u, pos.y + y * u);
  };
  auto line = [&](float x, float y, float a, float b) {
    d->AddLine(pt(x, y), pt(a, b), color, 1.4f * scale);
  };
  std::string key = id;
  if (key == "tool.select") {
    line(6, 4, 6, 20);
    line(6, 4, 19, 13);
    line(19, 13, 12, 14);
    line(12, 14, 6, 20);
    line(12, 14, 16, 21);
  } else if (key == "edit.undo" || key == "edit.redo") {
    bool right = key == "edit.redo";
    float x = right ? 18 : 6;
    line(x, 5, x + (right ? -4 : 4), 9);
    line(x, 5, x + (right ? -4 : 4), 1);
    d->AddBezierCubic(pt(x, 5), pt(right ? 0 : 24, 3), pt(right ? 0 : 24, 20),
                      pt(right ? 15 : 9, 20), color, 1.4f * scale);
  } else if (key == "clip.split") {
    line(8, 4, 8, 20);
    line(16, 4, 16, 20);
    line(4, 12, 20, 12);
  } else if (key == "clip.trimLeft" || key == "clip.trimRight") {
    bool right = key == "clip.trimRight";
    float x = right ? 16 : 8;
    line(x, 4, x, 20);
    line(x, 4, x + (right ? 3 : -3), 4);
    line(x, 20, x + (right ? 3 : -3), 20);
    line(right ? 10 : 14, 8, right ? 6 : 18, 12);
    line(right ? 6 : 18, 12, right ? 10 : 14, 16);
  } else if (key == "clip.delete") {
    line(5, 6, 19, 6);
    line(9, 3, 15, 3);
    line(7, 6, 8, 21);
    line(17, 6, 16, 21);
    line(8, 21, 16, 21);
  } else if (key == "marker.add") {
    line(6, 3, 6, 22);
    line(6, 3, 18, 3);
    line(18, 3, 15, 8);
    line(15, 8, 18, 13);
    line(18, 13, 6, 13);
  } else if (key == "record.open") {
    d->AddRect(pt(9, 2), pt(15, 15), color, 3 * scale);
    line(6, 10, 6, 14);
    line(18, 10, 18, 14);
    d->AddBezierCubic(pt(6, 14), pt(6, 21), pt(18, 21), pt(18, 14), color,
                      1.4f * scale);
    line(12, 19, 12, 23);
  } else if (key == "timeline.magnet") {
    line(6, 4, 6, 14);
    line(10, 4, 10, 14);
    line(14, 4, 14, 14);
    line(18, 4, 18, 14);
    line(6, 4, 10, 4);
    line(14, 4, 18, 4);
    d->AddBezierCubic(pt(6, 14), pt(6, 24), pt(18, 24), pt(18, 14), color,
                      1.4f * scale);
    line(10, 14, 14, 14);
  } else if (key == "timeline.snap" || key == "timeline.previewAxis") {
    line(12, 3, 12, 21);
    line(4, 7, 8, 7);
    line(4, 17, 8, 17);
    line(16, 7, 20, 7);
    line(16, 17, 20, 17);
    if (key == "timeline.snap") {
      line(4, 12, 8, 10);
      line(8, 10, 8, 14);
      line(8, 14, 4, 12);
    }
  } else if (key == "timeline.linkage") {
    d->AddRect(pt(4, 8), pt(14, 17), color, 3 * scale);
    d->AddRect(pt(10, 5), pt(20, 14), color, 3 * scale);
  } else if (key == "timeline.fit") {
    d->AddRect(pt(6, 6), pt(18, 18), color);
    line(3, 8, 3, 3);
    line(3, 3, 8, 3);
    line(16, 3, 21, 3);
    line(21, 3, 21, 8);
    line(3, 16, 3, 21);
    line(3, 21, 8, 21);
    line(21, 16, 21, 21);
    line(21, 21, 16, 21);
  } else if (key == "timeline.zoomOut" || key == "timeline.zoomIn") {
    line(5, 12, 19, 12);
    if (key == "timeline.zoomIn")
      line(12, 5, 12, 19);
  } else if (key == "track.lock") {
    d->AddRect(pt(6, 10), pt(18, 21), color, 2 * scale);
    d->AddBezierCubic(pt(8, 10), pt(8, -1), pt(16, -1), pt(16, 10), color,
                      1.4f * scale);
  } else if (key == "track.visibility") {
    d->AddBezierCubic(pt(3, 12), pt(8, 3), pt(16, 3), pt(21, 12), color,
                      1.4f * scale);
    d->AddBezierCubic(pt(3, 12), pt(8, 21), pt(16, 21), pt(21, 12), color,
                      1.4f * scale);
    d->AddCircle(pt(12, 12), 3 * u, color);
  } else if (key == "track.mute") {
    line(4, 9, 8, 9);
    line(8, 9, 13, 5);
    line(13, 5, 13, 19);
    line(13, 19, 8, 15);
    line(8, 15, 4, 15);
    line(4, 15, 4, 9);
    line(17, 8, 21, 16);
    line(21, 8, 17, 16);
  } else if (key == "transport.play") {
    d->AddTriangleFilled(pt(8, 5), pt(19, 12), pt(8, 19), color);
  } else if (key == "transport.stop") {
    d->AddRectFilled(pt(7, 7), pt(17, 17), color);
  } else if (key == "clip.keyframe") {
    line(12, 4, 20, 12);
    line(20, 12, 12, 20);
    line(12, 20, 4, 12);
    line(4, 12, 12, 4);
  } else {
    const char *text = key == "track.solo" ? "S" : "...";
    auto extent = ImGui::CalcTextSize(text);
    d->AddText(
        {pos.x + (size - extent.x) * .5f, pos.y + (size - extent.y) * .5f},
        color, text);
  }
  if (ImGui::IsItemHovered()) {
    std::string tooltip = title;
    for (auto &b : soundState.bindings)
      if (b.id == id && !b.chord.empty())
        tooltip += "  (" + b.chord + ")";
    ImGui::SetTooltip("%s", tooltip.c_str());
  }
  return click;
}
static bool soundFloatRow(const char *label, float &value, float step,
                          float low, float high, const char *format) {
  ImGui::PushID(label);
  ImGui::TextUnformatted(label);
  ImGui::SameLine();
  ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(), ImGui::GetCursorPosX() +
                                           ImGui::GetContentRegionAvail().x -
                                           85 * uiScale()));
  ImGui::SetNextItemWidth(85 * uiScale());
  bool changed = ImGui::DragFloat("##value", &value, step, low, high, format,
                                  ImGuiSliderFlags_AlwaysClamp);
  ImGui::PopID();
  return changed;
}
