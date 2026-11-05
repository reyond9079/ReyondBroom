// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
static ImVec2 bbUVCanvasStart;
static bool bbUVBoxSelecting = false;
static bb::Vec2 bbUVBoxStart;
static std::string bbUVDragFace, bbUVDragKey;
static bool bbPaintMoving = false, bbShapeEllipse = false, bbShapeFilled = true,
            bbLastStrokeReady = false;
static int bbLastStrokeTexture = -1;
static bb::Vec2 bbLastStroke{};
static std::array<int, 4> bbPaintOriginalRect{};
static std::vector<std::pair<int, int>> bbPixelPath;
static int bbUVDrag = -1, bbPaintTexture = -1;
static ImVec2 bbDragMouse;
static bb::Json bbUVStart;
static bb::Vec2 bbUVAnchor{}, bbPaintStart{}, bbPaintLast{}, bbCloneSource{}, bbCloneAnchor{};
static bool bbCloneReady = false, bbPaintSelecting = false, bbPaintSelection = false;
static std::array<int, 4> bbPaintRect{0, 0, 2048, 2048};
static bb::Texture bbPaintSource;
static std::string bbPaintFace, bbPaintElement;
static bool bbInTriangle(bb::Vec2 p, bb::Vec2 a, bb::Vec2 b, bb::Vec2 c) {
  auto cross = [](bb::Vec2 u, bb::Vec2 v, bb::Vec2 w) {
    return (v[0] - u[0]) * (w[1] - u[1]) - (v[1] - u[1]) * (w[0] - u[0]);
  };
  double x = cross(a, b, p), y = cross(b, c, p), z = cross(c, a, p);
  return (x >= -1e-6 && y >= -1e-6 && z >= -1e-6) || (x <= 1e-6 && y <= 1e-6 && z <= 1e-6);
}
static void bbBrushOptions() {
  ImGui::SetNextItemWidth(90 * uiScale());
  ImGui::SliderInt("Size", &bbBrush.size, 1, 50);
  ImGui::SameLine();
  float opacity = bbBrush.opacity * 100;
  ImGui::SetNextItemWidth(90 * uiScale());
  if (ImGui::SliderFloat("Opacity", &opacity, 0, 100, "%.0f%%"))
    bbBrush.opacity = opacity / 100;
  ImGui::SameLine();
  float softness = bbBrush.softness * 100;
  ImGui::SetNextItemWidth(90 * uiScale());
  if (ImGui::SliderFloat("Softness", &softness, 0, 100, "%.0f%%"))
    bbBrush.softness = softness / 100;
  ImGui::Checkbox("Circle", &bbBrush.circle);
  ImGui::SameLine();
  ImGui::Checkbox("Lock alpha", &bbBrush.lockAlpha);
  ImGui::SameLine();
  ImGui::Checkbox("Mirror painting X", &bbMirrorPaint);
  ImGui::Checkbox("Pixel perfect", &bbBrush.pixelPerfect);
  ImGui::SameLine();
  ImGui::Checkbox("Selected elements only", &bbOnlySelectedPaint);
  int blend = int(bbBrush.mode);
  ImGui::SetNextItemWidth(160 * uiScale());
  if (ImGui::Combo("Blend", &blend,
                   "Default\0Set opacity\0Color\0Behind\0Multiply\0Add\0Screen\0Difference\0"))
    bbBrush.mode = bb::Blend(blend);
  if (bbTool == 4) {
    ImGui::Checkbox("Ellipse", &bbShapeEllipse);
    ImGui::SameLine();
    ImGui::Checkbox("Filled", &bbShapeFilled);
  }
  if (bbTool == 2) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140 * uiScale());
    ImGui::Combo("Fill mode", &bbFillMode, "Face\0Element\0Color\0Color connected\0");
    ImGui::SliderInt("Tolerance", &bbFillTolerance, 0, 255);
  }
}
static bb::PaintOptions bbPaintOptions(int texture, const bb::Hit *hit = nullptr) {
  auto o = bbBrush;
  o.eraser = bbTool == 1;
  auto &palette = gameProfile == 2 ? bbGamePalette : bbPalette;
  o.palette =
      bbLimitPalette
          ? std::vector<bb::Pixel>(
                palette.begin(),
                palette.begin() + std::min(palette.size(), size_t(gameProfile == 2 ? 224 : 256)))
          : std::vector<bb::Pixel>{};
  if (bbPaintSelection)
    o.clip = bbPaintRect;
  if (hit) {
    auto &tex = bbProject.textures[texture];
    int width = tex.width, height = tex.height, uvWidth = tex.uvWidth, uvHeight = tex.uvHeight;
    auto polys = bbSurfaceUV[hit->element + "\n" + std::to_string(texture) + "\n" +
                             (bbFillMode == 1 ? "*" : hit->face)];
    o.mask = [width, height, uvWidth, uvHeight, polys](int x, int y) {
      bb::Vec2 p{(x + .5) * uvWidth / width, (y + .5) * uvHeight / height};
      for (auto &uv : polys)
        if (bbInTriangle(p, uv[0], uv[1], uv[2]))
          return true;
      return false;
    };
  }
  return o;
}
static void bbPaintDab(int texture, int x, int y, const bb::Hit *hit = nullptr) {
  auto &t = bbProject.textures[texture];
  auto o = bbPaintOptions(texture, hit);
  if (o.pixelPerfect && o.size == 1 && (bbTool == 0 || bbTool == 1)) {
    if (!bbPixelPath.empty() && bbPixelPath.back() == std::make_pair(x, y))
      return;
    if (bbPixelPath.size() > 1) {
      auto a = bbPixelPath[bbPixelPath.size() - 2], b = bbPixelPath.back();
      if ((a.first == b.first && b.second == y && a.second != y && b.first != x) ||
          (a.second == b.second && b.first == x && a.first != x && b.second != y)) {
        auto original = bb::pixel(bbPaintSource, b.first, b.second);
        if (b.first >= 0 && b.second >= 0 && b.first < t.width && b.second < t.height)
          std::copy(original.begin(), original.end(),
                    t.rgba.begin() + (size_t(b.second) * t.width + b.first) * 4);
        bbPixelPath.pop_back();
      }
    }
    bbPixelPath.push_back({x, y});
  }
  if (bbTool == 6 && bbCloneReady) {
    int dx = int(bbCloneSource[0] - bbCloneAnchor[0]),
        dy = int(bbCloneSource[1] - bbCloneAnchor[1]);
    int radius = std::max(0, o.size / 2);
    for (int yy = y - radius; yy <= y + radius; yy++)
      for (int xx = x - radius; xx <= x + radius; xx++) {
        auto brush = o;
        brush.size = 1;
        brush.color = bb::pixel(bbPaintSource, xx + dx, yy + dy);
        bb::dab(t, xx, yy, brush);
      }
  } else {
    bb::dab(t, x, y, o);
    if (bbMirrorPaint) {
      if (!hit)
        bb::dab(t, t.width - 1 - x, y, o);
      else {
        auto mirror = hit->point;
        mirror[0] = -mirror[0];
        auto normal = hit->normal;
        normal[0] = -normal[0];
        bb::Ray ray{bb::add(mirror, bb::mul(normal, .05)), bb::mul(normal, -1)};
        auto target = bb::pick(bbProject, bbTriangles, ray);
        if (target.found && target.texture == texture && target.distance < .1) {
          auto mask = bbPaintOptions(texture, &target);
          bb::dab(t, target.x, target.y, mask);
        }
      }
    }
  }
}
static void bbPaintMove(bb::Vec2 point, const bb::Hit *hit);
static void bbPaintBegin(int texture, bb::Vec2 point, const bb::Hit *hit = nullptr) {
  if (texture < 0 || size_t(texture) >= bbProject.textures.size())
    return;
  auto &io = ImGui::GetIO();
  if (bbTool == 3 || io.KeyAlt) {
    bbBrush.color = bb::pixel(bbProject.textures[texture], point[0], point[1]);
    for (int a = 0; a < 4; a++)
      bbColor[a] = bbBrush.color[a] / 255.f;
    return;
  }
  if (bbTool == 6 && io.KeyCtrl) {
    bbCloneSource = point;
    bbCloneReady = true;
    bbPaintSource = bbProject.textures[texture];
    return;
  }
  if (bbTool == 7) {
    if (bbPaintSelection && point[0] >= bbPaintRect[0] && point[0] < bbPaintRect[2] &&
        point[1] >= bbPaintRect[1] && point[1] < bbPaintRect[3]) {
      bbHistory.begin(bbProject, bbSelection, {size_t(texture)});
      bbPaintMoving = true;
      bbPaintTexture = texture;
      bbPaintSource = bbProject.textures[texture];
      bbPaintOriginalRect = bbPaintRect;
      bbPaintStart = point;
      return;
    }
    bbPaintSelecting = true;
    bbPaintSelection = false;
    bbPaintStart = point;
    return;
  }
  bbHistory.begin(bbProject, bbSelection, {size_t(texture)});
  bbPaintTexture = texture;
  bbPixelPath.clear();
  bbPaintStart = bbPaintLast = point;
  bbPaintFace = hit ? hit->face : "";
  bbPaintElement = hit ? hit->element : "";
  if (bbTool != 6)
    bbPaintSource = bbProject.textures[texture];
  else
    bbCloneAnchor = point;
  if (bbTool == 2) {
    auto &t = bbProject.textures[texture];
    auto o = bbPaintOptions(texture, hit);
    if (bbFillMode <= 1) {
      for (int y = 0; y < t.height; y++)
        for (int x = 0; x < t.width; x++)
          if (bb::allowed(t, x, y, o)) {
            auto b = o;
            b.size = 1;
            bb::dab(t, x, y, b);
          }
    } else
      bb::flood(t, point[0], point[1], o, bbFillTolerance, bbFillMode == 3);
  } else if (bbTool == 0 || bbTool == 1 || bbTool == 6) {
    if (io.KeyShift && bbLastStrokeReady && bbLastStrokeTexture == texture) {
      bbPaintLast = bbLastStroke;
      bbPaintMove(point, hit);
    } else
      bbPaintDab(texture, point[0], point[1], hit);
  }
}
static void bbPaintMove(bb::Vec2 point, const bb::Hit *hit = nullptr) {
  if (bbPaintMoving && bbPaintTexture >= 0) {
    auto &t = bbProject.textures[bbPaintTexture];
    auto revision = t.revision;
    t = bbPaintSource;
    t.revision = revision + 1;
    int dx = std::round(point[0] - bbPaintStart[0]), dy = std::round(point[1] - bbPaintStart[1]);
    auto rect = bbPaintOriginalRect;
    auto copy = bb::cropImage(bbPaintSource, rect);
    for (int y = std::max(0, rect[1]); y < std::min(t.height, rect[3]); y++)
      for (int x = std::max(0, rect[0]); x < std::min(t.width, rect[2]); x++)
        t.rgba[(size_t(y) * t.width + x) * 4 + 3] = 0;
    bbPaintRect = {rect[0] + dx, rect[1] + dy, rect[2] + dx, rect[3] + dy};
    auto options = bbPaintOptions(bbPaintTexture);
    options.clip = {0, 0, t.width, t.height};
    bb::pasteImage(t, copy, rect[0] + dx, rect[1] + dy, options);
    return;
  }
  if (bbPaintSelecting) {
    bbPaintRect = {int(std::floor(std::min(point[0], bbPaintStart[0]))),
                   int(std::floor(std::min(point[1], bbPaintStart[1]))),
                   int(std::ceil(std::max(point[0], bbPaintStart[0]))) + 1,
                   int(std::ceil(std::max(point[1], bbPaintStart[1]))) + 1};
    return;
  }
  if (bbPaintTexture < 0)
    return;
  if (hit && (hit->texture != bbPaintTexture || hit->face != bbPaintFace ||
              hit->element != bbPaintElement)) {
    bbPaintLast = point;
    return;
  }
  auto &t = bbProject.textures[bbPaintTexture];
  auto o = bbPaintOptions(bbPaintTexture, hit);
  if (bbTool == 4 || bbTool == 5) {
    auto revision = t.revision;
    t = bbPaintSource;
    t.revision = revision + 1;
    if (bbTool == 5)
      bb::gradient(t, bbPaintStart, point, bbBrush.color, bbSecondary, o);
    else
      bb::shape(t, bbPaintStart[0], bbPaintStart[1], point[0], point[1], o,
                bbShapeEllipse || ImGui::GetIO().KeyAlt, bbShapeFilled && !ImGui::GetIO().KeyShift);
  } else if (bbTool == 0 || bbTool == 1 || bbTool == 6) {
    int x0 = bbPaintLast[0], y0 = bbPaintLast[1], x1 = point[0], y1 = point[1];
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0),
        sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
      bbPaintDab(bbPaintTexture, x0, y0, hit);
      if (x0 == x1 && y0 == y1)
        break;
      int e = 2 * err;
      if (e >= dy) {
        err += dy;
        x0 += sx;
      }
      if (e <= dx) {
        err += dx;
        y0 += sy;
      }
    }
  }
  bbPaintLast = point;
  t.revision++;
}
static void bbPaintEnd(bool cancel = false) {
  if (bbPaintSelecting) {
    bbPaintSelecting = false;
    bbPaintSelection = !cancel;
    return;
  }
  if (bbPaintTexture >= 0) {
    try {
      if (cancel)
        bbHistory.cancel(bbProject, bbSelection);
      else
        bbHistory.finish(bbProject, bbSelection, bbPaintMoving ? "Move pixels" : "Paint");
    } catch (const std::exception &e) {
      bbNotice(e);
    }
    if (!cancel) {
      bbLastStroke = bbPaintLast;
      bbLastStrokeReady = true;
      bbLastStrokeTexture = bbPaintTexture;
    }
    if (cancel && bbPaintMoving)
      bbPaintRect = bbPaintOriginalRect;
    bbPaintMoving = false;
    bbPaintTexture = -1;
  }
}
struct BbUVLine {
  bb::Vec2 a, b;
  ImU32 color;
  float width;
};
struct BbUVPoint {
  bb::Vec2 uv;
  std::string face, key;
  int corner;
};
static std::vector<BbUVLine> bbUVLines;
static std::vector<BbUVPoint> bbUVPoints;
static uint64_t bbUVCacheRevision = ~uint64_t(0);
static int bbUVCacheTexture = -1;
static std::set<std::string> bbUVCacheElements;
static std::map<std::string, std::set<std::string>> bbUVCacheFaces;
static void bbUVPanel(bool paint) {
  if (bbTexture < 0 || size_t(bbTexture) >= bbProject.textures.size()) {
    ImGui::TextDisabled("Import or create a texture to edit UVs and paint");
    return;
  }
  auto &tex = bbProject.textures[bbTexture];
  auto id = bbActiveId();
  auto element = bbProject.find(id);
  if (!paint && element && element->contains("faces")) {
    if (element->value("type", "") == "cube") {
      ImGui::SetNextItemWidth(120 * uiScale());
      bool faceChanged = false;
      for (int side = 0; side < 6; side++) {
        if (side)
          ImGui::SameLine();
        const char *labels[] = {"N", "E", "S", "W", "U", "D"};
        ImGui::PushID(side);
        if (ImGui::Selectable(
                labels[side], bbFace == side, 0,
                {std::max(18.f, (ImGui::GetContentRegionAvail().x + side * 22.f) / 6 - 5),
                 ImGui::GetFrameHeight()})) {
          bbFace = side;
          faceChanged = true;
        }
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s", bb::faceNames[side]);
        ImGui::PopID();
      }
      std::string name = bb::faceNames[bbFace];
      if (faceChanged || bbSelection.faces[id].empty())
        bbSelection.faces[id] = {name};
      auto f = (*element)["faces"][name];
      float values[4];
      auto uv = f.value("uv", bb::Vec4{});
      for (int n = 0; n < 4; n++)
        values[n] = uv[n];
      if (ImGui::InputFloat4("UV", values))
        bbEdit("UV coordinates", [&] {
          (*bbProject.find(id))["faces"][name]["uv"] = {values[0], values[1], values[2], values[3]};
        });
      int rot = f.value("rotation", 0) / 90;
      if (ImGui::Combo("Rotation", &rot, "0\0 90\0 180\0 270\0"))
        bbEdit("UV rotation", [&] { (*bbProject.find(id))["faces"][name]["rotation"] = rot * 90; });
      bool enabled = !f.contains("texture") || !f["texture"].is_null();
      if (ImGui::Checkbox("Enabled", &enabled))
        bbEdit("Face enabled", [&] {
          (*bbProject.find(id))["faces"][name]["texture"] =
              enabled ? bb::Json(bbTexture) : bb::Json(nullptr);
        });
      element = bbProject.find(id);
      if (element && element->value("box_uv", false)) {
        auto offset = element->value("uv_offset", bb::Vec2{});
        float v[2] = {float(offset[0]), float(offset[1])};
        if (ImGui::InputFloat2("UV offset", v))
          bbEdit("Box UV offset", [&] { (*bbProject.find(id))["uv_offset"] = {v[0], v[1]}; });
      }
    }
    if (element && element->value("type", "") == "mesh") {
      auto faces = bbSelection.faces[id];
      bb::Vec2 low{1e30, 1e30}, high{-1e30, -1e30};
      for (auto it = (*element)["faces"].begin(); it != (*element)["faces"].end(); ++it)
        if (faces.empty() || faces.count(it.key()))
          for (auto &uv : it.value()["uv"])
            for (int a = 0; a < 2; a++) {
              low[a] = std::min(low[a], uv[a].get<double>());
              high[a] = std::max(high[a], uv[a].get<double>());
            }
      if (low[0] <= high[0]) {
        float position[2] = {float(low[0]), float(low[1])},
              size[2] = {float(high[0] - low[0]), float(high[1] - low[1])};
        if (ImGui::InputFloat2("UV position", position))
          bbEdit("Position mesh UV", [&] {
            bb::uvTransform(*bbProject.find(id), faces,
                            {position[0] - low[0], position[1] - low[1]});
          });
        if (ImGui::InputFloat2("UV size", size))
          bbEdit("Size mesh UV", [&] {
            bb::uvTransform(*bbProject.find(id), faces, {},
                            {std::abs(high[0] - low[0]) > 1e-9 ? size[0] / (high[0] - low[0]) : 1,
                             std::abs(high[1] - low[1]) > 1e-9 ? size[1] / (high[1] - low[1]) : 1},
                            low);
          });
      }
    }
    if (ImGui::BeginCombo("Face texture", tex.name.c_str())) {
      for (size_t i = 0; i < bbProject.textures.size(); i++)
        if (ImGui::Selectable(bbProject.textures[i].name.c_str(), bbTexture == int(i)))
          bbEdit("Face texture", [&] {
            for (auto &face : bbSelection.faces[id])
              (*bbProject.find(id))["faces"][face]["texture"] = int(i);
            bbTexture = i;
          });
      ImGui::EndCombo();
    }
  }
  if (ImGui::Button("Fit##uv")) {
    auto avail = ImGui::GetContentRegionAvail();
    bbUVZoom =
        std::min(avail.x / std::max(1, tex.width), 220 * uiScale() / std::max(1, tex.height));
    bbUVPan = {};
  }
  ImGui::SameLine();
  ImGui::Text("Zoom %.0f%%", bbUVZoom * 100);
  ImGui::BeginChild("UV canvas", {0, paint ? 0 : 260 * uiScale()}, ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  auto origin = ImGui::GetCursorScreenPos(), available = ImGui::GetContentRegionAvail();
  auto dl = ImGui::GetWindowDrawList();
  ImGui::InvisibleButton("##uv-canvas", available,
                         ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);
  bool hovered = ImGui::IsItemHovered();
  auto &io = ImGui::GetIO();
  if (hovered) {
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    if (io.MouseWheel) {
      auto before = bbUVZoom;
      bbUVZoom = std::clamp(bbUVZoom * std::pow(1.2f, io.MouseWheel), .05f, 128.f);
      auto offset =
          ImVec2(io.MousePos.x - origin.x - bbUVPan.x, io.MousePos.y - origin.y - bbUVPan.y);
      bbUVPan.x -= offset.x * (bbUVZoom / before - 1);
      bbUVPan.y -= offset.y * (bbUVZoom / before - 1);
    }
    if (ImGui::IsMouseDragging(2)) {
      bbUVPan.x += io.MouseDelta.x;
      bbUVPan.y += io.MouseDelta.y;
    }
  }
  ImVec2 start{origin.x + bbUVPan.x, origin.y + bbUVPan.y},
      end{start.x + tex.width * bbUVZoom, start.y + tex.height * bbUVZoom};
  dl->PushClipRect(origin, {origin.x + available.x, origin.y + available.y}, true);
  for (int y = 0; y < available.y; y += 12)
    for (int x = 0; x < available.x; x += 12)
      dl->AddRectFilled({origin.x + x, origin.y + y}, {origin.x + x + 12, origin.y + y + 12},
                        ((x / 12 + y / 12) & 1) ? IM_COL32(75, 75, 75, 255)
                                                : IM_COL32(45, 45, 45, 255));
  bbUVCanvasStart = start;
  auto image = bbImage(bbTexture);
  if (image)
    dl->AddImage((ImTextureID)image, start, end);
  if (bbPixelGrid && bbUVZoom >= 4) {
    int x0 = std::max(0, int((origin.x - start.x) / bbUVZoom)),
        x1 = std::min(tex.width, int((origin.x + available.x - start.x) / bbUVZoom) + 1),
        y0 = std::max(0, int((origin.y - start.y) / bbUVZoom)),
        y1 = std::min(tex.height, int((origin.y + available.y - start.y) / bbUVZoom) + 1);
    for (int x = x0; x <= x1; x++)
      dl->AddLine({start.x + x * bbUVZoom, start.y}, {start.x + x * bbUVZoom, end.y},
                  IM_COL32(0, 0, 0, 60));
    for (int y = y0; y <= y1; y++)
      dl->AddLine({start.x, start.y + y * bbUVZoom}, {end.x, start.y + y * bbUVZoom},
                  IM_COL32(0, 0, 0, 60));
  }
  auto screen = [&](bb::Vec2 uv) {
    return ImVec2(start.x + uv[0] * tex.width / tex.uvWidth * bbUVZoom,
                  start.y + uv[1] * tex.height / tex.uvHeight * bbUVZoom);
  };
  bb::Vec2 point{(io.MousePos.x - start.x) / bbUVZoom, (io.MousePos.y - start.y) / bbUVZoom};
  int corner = -1;
  std::string currentFace;
  // UV topology changes only with the uploaded document or selection. Cache
  // deduplication and colors, retaining every visible edge and control point.
  if (bbUVCacheRevision != bbRenderRevision || bbUVCacheTexture != bbTexture ||
      bbUVCacheElements != bbSelection.elements || bbUVCacheFaces != bbSelection.faces) {
    bbUVLines.clear();
    bbUVPoints.clear();
    std::set<std::tuple<bb::Vec2, bb::Vec2, bool>> drawn;
    std::map<bb::Vec2, size_t> handles;
    std::map<std::string, ImU32> colors;
    const ImU32 marker[] = {IM_COL32(130, 156, 180, 255), IM_COL32(174, 106, 80, 255),
                            IM_COL32(190, 167, 83, 255),  IM_COL32(93, 160, 124, 255),
                            IM_COL32(139, 112, 167, 255), IM_COL32(155, 155, 150, 255),
                            IM_COL32(171, 110, 136, 255), IM_COL32(81, 135, 161, 255)};
    for (auto &edge : bbPreviewEdges)
      if (edge.texture == bbTexture && bbSelection.elements.count(edge.element)) {
        bool selected = bbSelection.faces[edge.element].empty() ||
                        bbSelection.faces[edge.element].count(edge.face);
        if (!colors.count(edge.element)) {
          auto item = bbProject.find(edge.element);
          int color = item ? (item->value("color", 0) % 8 + 8) % 8 : 0;
          colors[edge.element] = marker[color];
        }
        auto u = edge.u, v = edge.v;
        if (v < u)
          std::swap(u, v);
        if (drawn.emplace(u, v, selected).second)
          bbUVLines.push_back({edge.u, edge.v,
                               selected ? IM_COL32(255, 195, 65, 255) : colors[edge.element],
                               selected ? 2.f : 1.f});
        if (selected)
          for (int n = 0; n < 2; n++) {
            auto uv = n ? edge.v : edge.u;
            auto inserted = handles.emplace(uv, bbUVPoints.size());
            if (inserted.second)
              bbUVPoints.push_back({uv, edge.face, n ? edge.bKey : edge.aKey, n});
            else {
              auto &point = bbUVPoints[inserted.first->second];
              point.face = edge.face;
              point.key = n ? edge.bKey : edge.aKey;
              point.corner = n;
            }
          }
      }
    bbUVCacheRevision = bbRenderRevision;
    bbUVCacheTexture = bbTexture;
    bbUVCacheElements = bbSelection.elements;
    bbUVCacheFaces = bbSelection.faces;
  }
  for (auto &edge : bbUVLines)
    dl->AddLine(screen(edge.a), screen(edge.b), edge.color, edge.width);
  for (auto &handle : bbUVPoints) {
    auto q = screen(handle.uv);
    dl->AddCircleFilled(q, 3, IM_COL32(255, 195, 65, 255));
    if (std::hypot(io.MousePos.x - q.x, io.MousePos.y - q.y) < 7) {
      currentFace = handle.face;
      bbUVDragKey = handle.key;
      corner = handle.corner;
    }
  }

  if (paint) {
    if (hovered && ImGui::IsMouseClicked(0))
      bbPaintBegin(bbTexture, point);
    if (ImGui::IsMouseDown(0) && (bbPaintTexture >= 0 || bbPaintSelecting))
      bbPaintMove(point);
    if (ImGui::IsMouseReleased(0))
      bbPaintEnd();
    if (bbPaintSelection || bbPaintSelecting) {
      auto a = screen({bbPaintRect[0] * double(tex.uvWidth) / tex.width,
                       bbPaintRect[1] * double(tex.uvHeight) / tex.height}),
           b = screen({bbPaintRect[2] * double(tex.uvWidth) / tex.width,
                       bbPaintRect[3] * double(tex.uvHeight) / tex.height});
      dl->AddRect(a, b, IM_COL32(255, 255, 255, 255));
    }
    if (hovered && bbBrushOutline) {
      float radius = std::max(.5f, bbBrush.size * .5f) * bbUVZoom;
      dl->AddRect({io.MousePos.x - radius, io.MousePos.y - radius},
                  {io.MousePos.x + radius, io.MousePos.y + radius}, IM_COL32(255, 230, 120, 255));
    }
  } else if (element && element->contains("faces")) {
    if (hovered && ImGui::IsMouseClicked(0) && io.KeyCtrl) {
      bbUVBoxSelecting = true;
      bbUVBoxStart = point;
      if (!io.KeyShift)
        bbUVVertices.clear();
    }
    if (hovered && ImGui::IsMouseClicked(0) && !io.KeyCtrl) {
      bbHistory.begin(bbProject, bbSelection);
      bbDragMouse = io.MousePos;
      bbUVAnchor = point;
      bbUVStart = *element;
      bbUVDrag = corner >= 0 ? corner : 4;
      bbUVDragFace = currentFace;
      if (element->value("type", "") == "mesh" && corner >= 0) {
        auto key = currentFace + "/" + bbUVDragKey;
        if (!io.KeyShift)
          bbUVVertices.clear();
        if (io.KeyShift && bbUVVertices.count(key))
          bbUVVertices.erase(key);
        else
          bbUVVertices.insert(key);
      } else
        bbUVVertices.clear();
    }
    if (bbUVDrag >= 0 && ImGui::IsMouseDown(0)) {
      auto e = bbProject.find(id);
      if (e) {
        *e = bbUVStart;
        double stepX = double(tex.uvWidth) / tex.width, stepY = double(tex.uvHeight) / tex.height;
        bb::Vec2 delta{(point[0] - bbUVAnchor[0]) * stepX, (point[1] - bbUVAnchor[1]) * stepY};
        if (!io.KeyShift) {
          delta[0] = bb::snap(delta[0], stepX);
          delta[1] = bb::snap(delta[1], stepY);
        }
        if (e->value("box_uv", false)) {
          auto o = e->value("uv_offset", bb::Vec2{});
          (*e)["uv_offset"] = {o[0] + delta[0], o[1] + delta[1]};
        } else if (bbUVDrag < 4 && e->value("type", "") == "mesh") {
          for (auto &key : bbUVVertices) {
            auto split = key.find('/');
            auto f = key.substr(0, split), v = key.substr(split + 1);
            if ((*e)["faces"].contains(f) && (*e)["faces"][f]["uv"].contains(v)) {
              bb::Vec2 uv = (*e)["faces"][f]["uv"][v];
              (*e)["faces"][f]["uv"][v] = {uv[0] + delta[0], uv[1] + delta[1]};
            }
          }
        } else if (bbUVDrag < 4 && e->value("type", "") == "cube") {
          auto &face = (*e)["faces"][bbUVDragFace];
          auto uv = face["uv"].get<bb::Vec4>();
          double px = bbUVAnchor[0] * stepX, py = bbUVAnchor[1] * stepY;
          int xi = std::abs(px - uv[0]) < std::abs(px - uv[2]) ? 0 : 2,
              yi = std::abs(py - uv[1]) < std::abs(py - uv[3]) ? 1 : 3;
          uv[xi] += delta[0];
          uv[yi] += delta[1];
          face["uv"] = uv;
        } else
          bb::uvTransform(*e, bbSelection.faces[id], delta);
        bbProject.revision++;
      }
    }
    if (bbUVDrag >= 0 && ImGui::IsMouseReleased(0)) {
      bbHistory.finish(bbProject, bbSelection, "Move UV");
      bbUVDrag = -1;
    }
  }
  if (bbUVBoxSelecting) {
    ImVec2 a{start.x + bbUVBoxStart[0] * bbUVZoom, start.y + bbUVBoxStart[1] * bbUVZoom};
    dl->AddRect(a, io.MousePos, IM_COL32(255, 190, 60, 255));
    if (ImGui::IsMouseReleased(0)) {
      for (auto &triangle : bbTriangles)
        if (triangle.element == id && triangle.texture == bbTexture)
          for (int i = 0; i < 3; i++) {
            auto q = screen(triangle.uv[i]);
            if (q.x >= std::min(a.x, io.MousePos.x) && q.x <= std::max(a.x, io.MousePos.x) &&
                q.y >= std::min(a.y, io.MousePos.y) && q.y <= std::max(a.y, io.MousePos.y))
              bbUVVertices.insert(triangle.face + "/" + triangle.keys[i]);
          }
      bbUVBoxSelecting = false;
    }
  }
  if (hovered && ImGui::IsMouseClicked(1))
    ImGui::OpenPopup("UV actions");
  if (ImGui::BeginPopup("UV actions")) {
    for (auto name :
         {"Maximize", "Auto UV", "Relative Auto UV", "Project from View", "Mirror X", "Mirror Y",
          "Rotate Left", "Rotate Right", "Turn Mapping", "Reset", "Apply to All Faces",
          "Transparent", "Snap UV to pixels", "Merge UV vertices", "Connect UV faces", "Cycle"})
      if (ImGui::MenuItem(name))
        bbAction(name);
    ImGui::EndPopup();
  }
  dl->PopClipRect();
  ImGui::EndChild();
}
static ImVec2 bbViewportMin, bbViewportSize;
static bb::Vec3 bbEye{}, bbForward{}, bbRight{}, bbUp{};
static double bbFocal = 1;
static int bbGizmoAxis = -1;
static ImVec2 bbGizmoScreenStart;
static double bbGizmoExtent = 1;
static bb::Vec3 bbGizmoAxes[3];
static bb::Json bbGizmoStart;
static bb::Vec3 bbGizmoPivot{}, bbSnapSource{};
static bool bbSnapReady = false;
static ImVec2 bbBoxStart;
static bool bbBoxSelecting = false;
static int bbCameraFront = 0;
static double bbCameraScale = 1, bbOrthoScale = 1;
static std::map<bb::Vec3, bool> bbVisiblePoints;
static void bbCameraBasis() {
  auto eye = modelEditRead("ftebroom_modeleditor_eye"),
       angles = modelEditRead("ftebroom_modeleditor_angles");
  bbEye = {eye[0], eye[1], eye[2]};
  double yaw = angles[1] * bb::pi / 180, pitch = angles[0] * bb::pi / 180;
  bbForward = {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), -std::sin(pitch)};
  bbRight = {std::sin(yaw), -std::cos(yaw), 0};
  bbUp = {std::sin(pitch) * std::cos(yaw), std::sin(pitch) * std::sin(yaw), std::cos(pitch)};
  bbFocal = modelEditPlayer ? bbViewportSize.x * .5
                            : bbViewportSize.y / (2 * std::tan(22.5 * bb::pi / 180));
  // Projection constants are shared by every edge/handle; do not query JSON or
  // engine variables tens of thousands of times per frame.
  bbCameraFront = bbProject.front();
  bbCameraScale = bbProject.exportScale();
  if (bbOrtho) {
    auto lo = modelEditRead("ftebroom_bb_low"), hi = modelEditRead("ftebroom_bb_high");
    double extent = std::max(16., bb::length({double(hi[0] - lo[0]), double(hi[1] - lo[1]),
                                              double(hi[2] - lo[2])})) *
                    modelViewerZoom;
    bbOrthoScale = bbViewportSize.x / extent;
  }
  static bb::Vec3 previousEye{}, previousForward{};
  static uint64_t epoch = ~uint64_t(0), images = 0;
  static bool orthographic = false;
  uint64_t stamp = 0;
  for (auto &texture : bbProject.textures)
    stamp = stamp * 1099511628211ull + texture.revision;
  if (epoch != bbVisibilityEpoch || previousEye != bbEye || previousForward != bbForward ||
      orthographic != bbOrtho || images != stamp) {
    bbVisiblePoints.clear();
    epoch = bbVisibilityEpoch;
    previousEye = bbEye;
    previousForward = bbForward;
    orthographic = bbOrtho;
    images = stamp;
  }
}
static bool bbScreen(bb::Vec3 v, ImVec2 &out) {
  auto q = bb::sub(bb::bbToQuake(v, bbCameraFront, bbCameraScale), bbEye);
  double z = bb::dot(q, bbForward);
  if (z <= .01)
    return false;
  double scale = bbFocal / z;
  if (bbOrtho)
    scale = bbOrthoScale;
  out = {float(bbViewportMin.x + bbViewportSize.x * .5 + scale * bb::dot(q, bbRight)),
         float(bbViewportMin.y + bbViewportSize.y * .5 - scale * bb::dot(q, bbUp))};
  return true;
}
static bb::Ray bbMouseRay(ImVec2 mouse) {
  double x = (mouse.x - bbViewportMin.x - bbViewportSize.x * .5) / bbFocal,
         y = -(mouse.y - bbViewportMin.y - bbViewportSize.y * .5) / bbFocal;
  auto direction = bb::unit(bb::add(bbForward, bb::add(bb::mul(bbRight, x), bb::mul(bbUp, y))));
  bb::Ray ray;
  ray.origin = bb::quakeToBb(bbEye, bbProject.front(), bbProject.exportScale());
  ray.direction = bb::unit(bb::quakeToBb(direction, bbProject.front()));
  if (bbOrtho) {
    auto low = modelEditRead("ftebroom_bb_low"), high = modelEditRead("ftebroom_bb_high");
    double extent = std::max(16., bb::length({double(high[0] - low[0]), double(high[1] - low[1]),
                                              double(high[2] - low[2])})) *
                    modelViewerZoom;
    auto origin = bb::add(
        bbEye, bb::add(bb::mul(bbRight, (mouse.x - bbViewportMin.x - bbViewportSize.x * .5) *
                                            extent / bbViewportSize.x),
                       bb::mul(bbUp, -(mouse.y - bbViewportMin.y - bbViewportSize.y * .5) * extent /
                                         bbViewportSize.x)));
    ray.origin = bb::quakeToBb(origin, bbProject.front(), bbProject.exportScale());
    ray.direction = bb::quakeToBb(bbForward, bbProject.front());
  }
  return ray;
}
static bool bbVisiblePoint(bb::Vec3 point) {
  auto cached = bbVisiblePoints.find(point);
  if (cached != bbVisiblePoints.end())
    return cached->second;
  ImVec2 screen;
  bool visible = false;
  if (bbScreen(point, screen)) {
    auto ray = bbMouseRay(screen);
    double distance = bb::dot(bb::sub(point, ray.origin), ray.direction);
    visible = !bbVisibility.blocked(bbProject, ray, distance - std::max(.002, distance * 1e-6));
  }
  bbVisiblePoints[point] = visible;
  return visible;
}
static bool bbVisibleEdge(bb::Vec3 a, bb::Vec3 b) {
  return bbVisiblePoint(a) || bbVisiblePoint(b) || bbVisiblePoint(bb::mul(bb::add(a, b), .5));
}
static bb::Vec3 bbSelectionPivot() {
  auto id = bbActiveId();
  auto e = bbProject.find(id);
  if (e) {
    auto par = bb::parents(bbProject);
    std::map<std::string, bb::Pose> pose;
    if (bbMode == 2 && bbAnimation >= 0 &&
        size_t(bbAnimation) < bbProject.root["animations"].size())
      pose = bb::animationPose(bbProject.root["animations"][bbAnimation], bbPlayTime);
    auto matrices = bb::groupMatrices(bbProject, pose);
    auto m = matrices.count(par[id]) ? matrices[par[id]] : bb::Mat4::identity();
    auto origin = bb::getv(*e, "origin");
    if (pose.count(id))
      origin = bb::add(origin, pose[id].position);
    return m.point(origin);
  }
  return {};
}
static void bbViewport() {
  auto &io = ImGui::GetIO();
  if (bbMode == 2) {
    ImGui::Checkbox("Auto key", &bbAutoKey);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120 * uiScale());
    ImGui::Combo("Key channel", &bbKeyChannel, "Rotation\0Position\0Scale\0");
  }
  if (bbMode == 1) {
    bbBrushOptions();
    ImGui::SameLine();
    ImGui::Checkbox("2D", &bbPaint2D);
    if (bbPaint2D) {
      bbUVPanel(true);
      return;
    }
  } else {
    ImGui::SetNextItemWidth(100 * uiScale());
    ImGui::Combo("Space", &bbSpace, "Global\0Local\0Parent\0");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70 * uiScale());
    if (ImGui::InputFloat("Grid snap", &bbGrid))
      bbSettingsSave();
    bbGrid = std::clamp(bbGrid, .001f, 1024.f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(95 * uiScale());
    ImGui::Combo("Selection", &bbSelectionMode, "Object\0Face\0Edge\0Vertex\0");
    if (bbTool == 4) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100 * uiScale());
      ImGui::Combo("Snap mode", &bbSnapMode, "Move\0Scale\0");
    }
  }
  if (!ImGui::BeginTable("author-tools-and-view", 2, ImGuiTableFlags_Resizable))
    return;
  ImGui::TableSetupColumn("Tools", ImGuiTableColumnFlags_WidthFixed, 42 * uiScale());
  ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  const char *editNames[] = {"Move (V)",        "Resize (T)", "Rotate (R)", "Pivot (P)",
                             "Vertex Snap (N)", "Stretch",    "Knife (K)",  "Seam"};
  const char *paintNames[] = {"Brush (B)",  "Eraser (E)", "Fill (G)",   "Color Picker (I)",
                              "Draw Shape", "Gradient",   "Copy Brush", "Selection (M)"};
  int icons[] = {0, 9, 7, 4, 10, 9, 2, 3};
  for (int i = 0; i < (bbMode == 2 ? 4 : 8); i++) {
    ImGui::PushID(i);
    bool wasSelected = i == bbTool;
    if (wasSelected)
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    ImGui::BeginDisabled(bbMode == 0 && i == 6);
    if (FteIcons::button("##tool", icons[i], {34 * uiScale(), 34 * uiScale()}))
      bbTool = i;
    ImGui::EndDisabled();
    if (wasSelected)
      ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", bbMode == 1 ? paintNames[i] : editNames[i]);
    ImGui::PopID();
  }
  ImGui::TableSetColumnIndex(1);
  auto size = ImGui::GetContentRegionAvail();
  size.x = std::max(64.f, size.x);
  size.y = std::max(64.f, size.y);
  bbViewportMin = ImGui::GetCursorScreenPos();
  bbViewportSize = size;
  modelEditViewMin = bbViewportMin;
  modelEditViewSize = size;
  if (!modelViewerImage)
    modelViewerImage = draw->LoadImageShader(
        "ftebroom/modelviewer-image",
        "{ program default2d { map $rt:ftebroom_modelviewer rgbgen vertex alphagen vertex } }");
  auto dl = ImGui::GetWindowDrawList();
  dl->AddImage((ImTextureID)modelViewerImage, bbViewportMin,
               {bbViewportMin.x + size.x, bbViewportMin.y + size.y});
  ImGui::InvisibleButton("bb-scene", size, ImGuiButtonFlags_MouseButtonLeft);
  bool hovered = ImGui::IsItemHovered();
  if (hovered && io.MouseWheel) {
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    if (!modelEditorZoom(io.MouseWheel))
      modelViewerZoom = std::clamp(modelViewerZoom * std::pow(1.12f, -io.MouseWheel), .03f, 50.f);
  }
  cvars->SetFloat("ftebroom_modelviewer_width", size.x);
  cvars->SetFloat("ftebroom_modelviewer_height", size.y);
  cvars->SetFloat("ftebroom_modelviewer_yaw", modelViewerYaw);
  cvars->SetFloat("ftebroom_modelviewer_pitch", modelViewerPitch);
  cvars->SetFloat("ftebroom_modelviewer_zoom", modelViewerZoom);
  cvars->SetFloat("ftebroom_modeleditor_player", modelEditPlayer);
  cvars->SetFloat("ftebroom_modeleditor_floor", modelEditFloor);
  cvars->SetFloat("ftebroom_modeleditor_axes", modelEditOriginAxes);
  cvars->SetFloat("ftebroom_bb_ortho", bbOrtho);
  bbCameraBasis();
  dl->PushClipRect(bbViewportMin, {bbViewportMin.x + size.x, bbViewportMin.y + size.y}, true);
  auto hit = hovered ? bb::pick(bbProject, bbTriangles, bbMouseRay(io.MousePos), modelEditCull)
                     : bb::Hit{};
  for (auto &locator : bbProject.root["elements"])
    if (locator.value("type", "") == "locator" && locator.value("visibility", true)) {
      auto parents = bb::parents(bbProject);
      auto matrices = bb::groupMatrices(bbProject);
      std::string id = locator["uuid"];
      auto matrix = matrices.count(parents[id]) ? matrices[parents[id]] : bb::Mat4::identity();
      auto point = matrix.point(bb::getv(locator, "position"));
      ImVec2 q;
      if (bbScreen(point, q)) {
        ImU32 c = bbSelection.elements.count(id) ? IM_COL32(255, 170, 45, 255)
                                                 : IM_COL32(200, 160, 240, 255);
        dl->AddLine({q.x - 7, q.y}, {q.x + 7, q.y}, c, 2);
        dl->AddLine({q.x, q.y - 7}, {q.x, q.y + 7}, c, 2);
        if (hovered && bbSelectionMode == 0 &&
            std::hypot(io.MousePos.x - q.x, io.MousePos.y - q.y) < 8) {
          hit.found = true;
          hit.element = id;
          hit.face = "";
          hit.texture = -1;
          hit.point = point;
        }
      }
    }
  std::set<std::string> selected =
      bb::descendants(bbProject, bb::selectedTop(bbProject, bbSelection));
  for (auto &edge : bbPreviewEdges)
    if (selected.count(edge.element) ||
        (hit.found && edge.element == hit.element && edge.face == hit.face)) {
      bool hover = hit.found && edge.element == hit.element && edge.face == hit.face;
      ImVec2 a, b;
      if (bbVisibleEdge(edge.a, edge.b) && bbScreen(edge.a, a) && bbScreen(edge.b, b))
        dl->AddLine(a, b, hover ? IM_COL32(255, 200, 70, 255) : IM_COL32(240, 240, 240, 220), 1.2f);
    }
  if (bbSelectionMode == 3) {
    std::set<std::pair<std::string, std::string>> drawn;
    for (auto &triangle : bbTriangles)
      if (selected.count(triangle.element))
        for (int i = 0; i < 3; i++)
          if (drawn.emplace(triangle.element, triangle.keys[i]).second) {
            ImVec2 q;
            if (bbVisiblePoint(triangle.p[i]) && bbScreen(triangle.p[i], q)) {
              bool active = bbSelection.vertices[triangle.element].count(triangle.keys[i]);
              if (active)
                dl->AddCircleFilled(q, 4, IM_COL32(255, 160, 65, 255), 8);
              else
                dl->AddRectFilled({q.x - 1.5f, q.y - 1.5f}, {q.x + 1.5f, q.y + 1.5f},
                                  IM_COL32(200, 190, 175, 255));
            }
          }
  }

  if (bbSelectionMode == 2 || bbTool == 7)
    for (auto id : bbSelection.elements)
      if (auto e = bbProject.find(id))
        if (e->value("type", "") == "mesh") {
          auto matrix = bb::objectMatrix(bbProject, id);
          std::set<std::string> drawn;
          for (auto &face : (*e)["faces"]) {
            auto keys = face["vertices"].get<std::vector<std::string>>();
            for (size_t n = 0; n < keys.size(); n++) {
              auto a = keys[n], b = keys[(n + 1) % keys.size()], edge = bb::edgeKey(a, b);
              if (!drawn.insert(edge).second)
                continue;
              ImVec2 pa, pb;
              auto worldA = matrix.point(bb::vec((*e)["vertices"][a])),
                   worldB = matrix.point(bb::vec((*e)["vertices"][b]));
              if (bbVisibleEdge(worldA, worldB) && bbScreen(worldA, pa) && bbScreen(worldB, pb)) {
                bool seam = e->value("seams", bb::Json::object()).value(edge, "") == "divide",
                     active = bbSelection.edges[id].count(edge);
                dl->AddLine(pa, pb,
                            seam     ? IM_COL32(255, 90, 90, 255)
                            : active ? IM_COL32(255, 170, 45, 255)
                                     : IM_COL32(175, 175, 175, 200),
                            active || seam ? 3 : 1);
              }
            }
          }
        }
  if (bbSnapReady) {
    ImVec2 q;
    if (bbScreen(bbSnapSource, q)) {
      dl->AddCircle(q, 7, IM_COL32(255, 220, 60, 255), 16, 2);
      dl->AddLine(q, io.MousePos, IM_COL32(255, 220, 60, 160));
    }
  }
  if (bbShowBones || !bbSelection.groups.empty()) {
    std::map<std::string, bb::Pose> poses;
    if (bbMode == 2 && bbAnimation >= 0 &&
        size_t(bbAnimation) < bbProject.root["animations"].size())
      poses = bb::animationPose(bbProject.root["animations"][bbAnimation], bbPlayTime);
    auto matrices = bb::groupMatrices(bbProject, poses);
    auto parents = bb::parents(bbProject);
    for (auto &g : bbProject.root["groups"]) {
      std::string id = g["uuid"];
      auto parent = parents[id];
      auto origin = bb::getv(g, "origin");
      if (poses.count(id))
        origin = bb::add(origin, poses[id].position);
      auto a = matrices[parent].point(origin);
      ImVec2 point;
      if (bbScreen(a, point)) {
        bool active = bbSelection.groups.count(id);
        if (active) {
          dl->AddLine({point.x - 10, point.y}, {point.x + 10, point.y},
                      IM_COL32(255, 170, 60, 255));
          dl->AddLine({point.x, point.y - 10}, {point.x, point.y + 10},
                      IM_COL32(255, 170, 60, 255));
        }
        dl->AddCircle(point, active ? 7 : 3,
                      active ? IM_COL32(255, 170, 60, 255) : IM_COL32(80, 210, 150, 170));
        if (!parent.empty()) {
          auto p = bbProject.find(parent);
          if (p) {
            ImVec2 other;
            if (bbScreen(matrices[parents[parent]].point(
                             bb::add(bb::getv(*p, "origin"),
                                     poses.count(parent) ? poses[parent].position : bb::Vec3{})),
                         other))
              dl->AddLine(point, other, IM_COL32(80, 230, 160, 200), 2);
          }
        }
      }
    }
  }
  if (bbSilhouette) {
    std::array<bb::Vec3, 8> box;
    for (int i = 0; i < 8; i++)
      box[i] = {i & 1 ? 16. : -16., i & 2 ? 32. : -24., i & 4 ? 16. : -16.};
    for (int i = 0; i < 8; i++)
      for (int axis = 0; axis < 3; axis++)
        if (!(i & (1 << axis))) {
          ImVec2 a, b;
          if (bbScreen(box[i], a) && bbScreen(box[i | (1 << axis)], b))
            dl->AddLine(a, b, IM_COL32(120, 180, 220, 90));
        }
  }
  if (bbMode == 1) {
    if (hovered && hit.found && hit.texture >= 0 &&
        (!bbOnlySelectedPaint || selected.count(hit.element)) && ImGui::IsMouseClicked(0)) {
      bbTexture = hit.texture;
      bbPaintBegin(hit.texture, {double(hit.x), double(hit.y)}, &hit);
    }
    if (ImGui::IsMouseDown(0) && bbPaintTexture >= 0 && hit.found)
      bbPaintMove({double(hit.x), double(hit.y)}, &hit);
    if (ImGui::IsMouseReleased(0))
      bbPaintEnd();
  } else {
    auto pivot = bbSelectionPivot();
    ImVec2 center;
    bool hasSelection = !bbIds().empty(), visible = hasSelection && bbScreen(pivot, center);
    int hoveredAxis = -1;
    double nearest = 9;
    float radius = 70 * uiScale();
    ImVec2 ends[3];
    bb::Vec3 axisVectors[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    if (bbSpace && hasSelection) {
      auto e = bbProject.find(bbActiveId());
      auto matrices = bb::groupMatrices(bbProject);
      auto parents = bb::parents(bbProject);
      auto basis = bbSpace == 2
                       ? matrices[parents[bbActiveId()]]
                       : matrices[parents[bbActiveId()]] * bb::rotation(bb::getv(*e, "rotation"));
      for (int a = 0; a < 3; a++)
        axisVectors[a] = bb::unit(basis.vector(axisVectors[a]));
    }
    ImU32 colors[] = {IM_COL32(240, 75, 65, 255), IM_COL32(75, 220, 95, 255),
                      IM_COL32(80, 145, 250, 255)};
    double distance = bb::length(
               bb::sub(bb::bbToQuake(pivot, bbProject.front(), bbProject.exportScale()), bbEye)),
           extent = std::max(.1, distance * .12 / bbProject.exportScale());
    auto segmentDistance = [&](ImVec2 a, ImVec2 b) {
      double x = b.x - a.x, y = b.y - a.y,
             t = std::clamp(((io.MousePos.x - a.x) * x + (io.MousePos.y - a.y) * y) /
                                std::max(1e-9, x * x + y * y),
                            0., 1.);
      return std::hypot(io.MousePos.x - a.x - t * x, io.MousePos.y - a.y - t * y);
    };
    auto activeElement = bbProject.find(bbActiveId());
    bool cubeResize =
        visible && bbTool == 1 && activeElement && activeElement->value("type", "") == "cube";
    if (visible && !cubeResize)
      for (int a = 0; a < 3; a++) {
        if (bbTool == 2) {
          ImVec2 previous;
          bool have = false;
          int b = (a + 1) % 3, c = (a + 2) % 3;
          for (int n = 0; n <= 64; n++) {
            double angle = n * 2 * bb::pi / 64;
            auto point = bb::add(pivot, bb::add(bb::mul(axisVectors[b], extent * std::cos(angle)),
                                                bb::mul(axisVectors[c], extent * std::sin(angle))));
            ImVec2 current;
            if (bbScreen(point, current)) {
              if (have) {
                dl->AddLine(previous, current, colors[a], 2);
                double d = segmentDistance(previous, current);
                if (d < nearest) {
                  nearest = d;
                  hoveredAxis = a;
                }
              }
              previous = current;
              have = true;
            }
          }
        } else {
          auto point = bb::add(pivot, bb::mul(axisVectors[a], extent));
          if (bbScreen(point, ends[a])) {
            dl->AddLine(center, ends[a], colors[a], 3);
            dl->AddCircleFilled(ends[a], 5, colors[a]);
            dl->AddText(ends[a], colors[a], a == 0 ? "X" : a == 1 ? "Y" : "Z");
            double d = segmentDistance(center, ends[a]);
            if (d < nearest) {
              nearest = d;
              hoveredAxis = a;
            }
          }
        }
      }
    if (cubeResize) {
      auto &e = *activeElement;
      auto matrix = bb::objectMatrix(bbProject, bbActiveId());
      auto from = bb::getv(e, "from"), to = bb::getv(e, "to"),
           middle = bb::mul(bb::add(from, to), .5);
      for (int a = 0; a < 3; a++) {
        bb::Vec3 vector{};
        vector[a] = 1;
        axisVectors[a] = bb::unit(matrix.vector(vector));
        for (int side = 0; side < 2; side++) {
          auto point = middle;
          point[a] = side ? to[a] : from[a];
          auto world = matrix.point(point);
          ImVec2 q, end;
          if (bbScreen(world, q) &&
              bbScreen(bb::add(world, bb::mul(axisVectors[a], extent * .4 * (side ? 1 : -1))),
                       end)) {
            dl->AddLine(q, end, colors[a], 3);
            dl->AddRectFilled({end.x - 5, end.y - 5}, {end.x + 5, end.y + 5}, colors[a]);
            double d = segmentDistance(q, end);
            if (d < nearest) {
              nearest = d;
              hoveredAxis = a + (side ? 0 : 3);
            }
          }
        }
      }
    }

    if (visible && bbTool == 0)
      for (int normal = 0; normal < 3; normal++) {
        int b = (normal + 1) % 3, c = (normal + 2) % 3;
        ImVec2 q[4];
        bool shown = true;
        for (int i = 0; i < 4; i++) {
          auto point =
              bb::add(pivot, bb::add(bb::mul(axisVectors[b], extent * (i & 1 ? .38 : .18)),
                                     bb::mul(axisVectors[c], extent * (i & 2 ? .38 : .18))));
          shown &= bbScreen(point, q[i]);
        }
        if (shown) {
          dl->AddQuadFilled(q[0], q[1], q[3], q[2], (colors[normal] & 0xffffff) | 0x66000000);
          if (bbInTriangle({io.MousePos.x, io.MousePos.y}, {q[0].x, q[0].y}, {q[1].x, q[1].y},
                           {q[3].x, q[3].y}) ||
              bbInTriangle({io.MousePos.x, io.MousePos.y}, {q[0].x, q[0].y}, {q[3].x, q[3].y},
                           {q[2].x, q[2].y})) {
            hoveredAxis = 6 + normal;
            nearest = 0;
          }
        }
      }
    if (hovered && ImGui::IsMouseClicked(0)) {
      if (hoveredAxis >= 0 && hasSelection) {
        bbGizmoAxis = hoveredAxis;
        bbGizmoStart = bbProject.root;
        bbGizmoPivot = pivot;
        bbGizmoScreenStart = center;
        bbGizmoExtent = extent;
        for (int a = 0; a < 3; a++)
          bbGizmoAxes[a] = axisVectors[a];
        bbDragMouse = io.MousePos;
        bbHistory.begin(bbProject, bbSelection);
      } else if (io.KeyCtrl || !hit.found) {
        bbBoxSelecting = true;
        bbBoxStart = io.MousePos;
      } else {
        if (bbTool == 4) {
          auto &tri = bbTriangles[hit.triangle];
          auto nearestVertex = tri.p[0];
          double best = 1e30;
          for (auto p : tri.p) {
            ImVec2 q;
            if (bbScreen(p, q)) {
              double d = std::hypot(io.MousePos.x - q.x, io.MousePos.y - q.y);
              if (d < best) {
                best = d;
                nearestVertex = p;
              }
            }
          }
          if (!bbSnapReady) {
            if (bb::descendants(bbProject, bbIds()).count(hit.element)) {
              bbSnapSource = nearestVertex;
              bbSnapReady = true;
            } else
              bbMessage = "Choose the source vertex on selected geometry first";
          } else {
            bbEdit("Vertex snap", [&] {
              if (!bbSnapMode)
                bb::moveWorld(bbProject, bbTransformSelection(),
                              bb::sub(nearestVertex, bbSnapSource));
              else {
                auto pivot = bbSelectionPivot(), source = bb::sub(bbSnapSource, pivot),
                     target = bb::sub(nearestVertex, pivot);
                bb::Vec3 factor{1, 1, 1};
                for (int a = 0; a < 3; a++)
                  if (std::abs(source[a]) > 1e-8)
                    factor[a] = target[a] / source[a];
                for (auto id : bbSelection.elements)
                  if (auto e = bbProject.find(id))
                    if (e->value("type", "") == "cube")
                      *e = bb::toMesh(*e);
                bb::transformVertices(bbProject, bbTransformSelection(),
                                      bb::translate(pivot) * bb::scale(factor) *
                                          bb::translate(bb::mul(pivot, -1)));
              }
            });
            bbSnapReady = false;
          }
        } else {
          if (bbSelectionMode == 0)
            bbSelect(hit.element, io.KeyShift);
          else {
            if (!io.KeyShift)
              bbSelection = {};
            bbSelection.elements.insert(hit.element);
            bbSelection.last = hit.element;
          }
          if (bbSelectionMode == 1) {
            if (io.KeyShift && bbSelection.faces[hit.element].count(hit.face))
              bbSelection.faces[hit.element].erase(hit.face);
            else
              bbSelection.faces[hit.element].insert(hit.face);
            for (int n = 0; n < 6; n++)
              if (hit.face == bb::faceNames[n])
                bbFace = n;
          } else if (bbSelectionMode == 2 || bbTool == 7) {
            auto e = bbProject.find(hit.element);
            if (e && e->value("type", "") == "mesh") {
              auto keys = (*e)["faces"][hit.face]["vertices"].get<std::vector<std::string>>();
              auto matrix = bb::objectMatrix(bbProject, hit.element);
              std::string edge;
              double best = 1e30;
              for (size_t i = 0; i < keys.size(); i++) {
                ImVec2 a, b;
                if (bbScreen(matrix.point(bb::vec((*e)["vertices"][keys[i]])), a) &&
                    bbScreen(matrix.point(bb::vec((*e)["vertices"][keys[(i + 1) % keys.size()]])),
                             b)) {
                  double d = segmentDistance(a, b);
                  if (d < best) {
                    best = d;
                    edge = bb::edgeKey(keys[i], keys[(i + 1) % keys.size()]);
                  }
                }
              }
              if (!edge.empty()) {
                if (bbTool == 7)
                  bbEdit("Toggle UV seam", [&] {
                    auto &seams = (*e)["seams"];
                    if (seams.is_null())
                      seams = bb::Json::object();
                    seams[edge] = seams.value(edge, "") == "divide" ? "join" : "divide";
                  });
                else if (io.KeyAlt) {
                  auto loop = bb::edgeLoop(*e, edge);
                  bbSelection.edges[hit.element].insert(loop.begin(), loop.end());
                } else if (io.KeyShift && bbSelection.edges[hit.element].count(edge))
                  bbSelection.edges[hit.element].erase(edge);
                else
                  bbSelection.edges[hit.element].insert(edge);
              }
            }
          } else if (bbSelectionMode == 3) {
            auto &tri = bbTriangles[hit.triangle];
            int best = 0;
            double d = 1e30;
            for (int i = 0; i < 3; i++) {
              ImVec2 q;
              if (bbScreen(tri.p[i], q)) {
                double next = std::hypot(io.MousePos.x - q.x, io.MousePos.y - q.y);
                if (next < d) {
                  d = next;
                  best = i;
                }
              }
            }
            auto key = tri.keys[best];
            bbLastVertex = key;
            if (io.KeyShift && bbSelection.vertices[hit.element].count(key))
              bbSelection.vertices[hit.element].erase(key);
            else
              bbSelection.vertices[hit.element].insert(key);
          }
          if (hit.texture >= 0)
            bbTexture = hit.texture;
        }
      }
    }
    if (bbGizmoAxis >= 0 && ImGui::IsMouseDown(0)) {
      bbProject.root = bbGizmoStart;
      int axis = bbGizmoAxis % 3;
      bool negativeHandle = bbGizmoAxis >= 3 && bbGizmoAxis < 6;
      auto direction = bbGizmoAxes[axis];
      center = bbGizmoScreenStart;
      extent = bbGizmoExtent;
      ImVec2 end;
      bbScreen(bb::add(bbGizmoPivot, bb::mul(direction, extent)), end);
      double x = end.x - center.x, y = end.y - center.y,
             units = ((io.MousePos.x - bbDragMouse.x) * x + (io.MousePos.y - bbDragMouse.y) * y) /
                     std::max(1e-6, x * x + y * y) * extent;
      double step = io.KeyCtrl ? 0 : io.KeyShift ? bbGrid * .25 : bbGrid;
      units = bb::snap(units, step);
      if (bbMode == 2 && bbAnimation >= 0 &&
          size_t(bbAnimation) < bbProject.root["animations"].size()) {
        for (auto id : bbSelection.groups) {
          std::string channelName = bbTool == 2 ? "rotation" : bbTool == 1 ? "scale" : "position";
          auto &anim = bbProject.root["animations"][bbAnimation];
          bool existing = false;
          for (auto &key : anim["animators"]
                               .value(id, bb::Json::object())
                               .value("keyframes", bb::Json::array()))
            if (key.value("channel", "") == channelName &&
                std::abs(key.value("time", 0.f) - bbPlayTime) < 1e-5)
              existing = true;
          if (!bbAutoKey && !existing) {
            bbMessage = "Enable Auto key or add a key at the playhead first";
            continue;
          }
          bb::Vec3 value =
              bb::channel(anim["animators"].value(id, bb::Json::object()), channelName, bbPlayTime);
          value[axis] += bbTool == 2   ? bb::snap((io.MousePos.x - bbDragMouse.x) * .5,
                                                io.KeyCtrl    ? 0
                                                  : io.KeyShift ? 1
                                                                : bbRotationSnap)
                         : bbTool == 1 ? units / std::max(.1, extent)
                                       : units;
          bb::setKey(bbProject.root["animations"][bbAnimation], id,
                     bbTool == 2   ? "rotation"
                     : bbTool == 1 ? "scale"
                                   : "position",
                     bbPlayTime, value);
        }
      } else if (bbTool == 0) {
        if (bbGizmoAxis >= 6) {
          auto begin = bbMouseRay(bbDragMouse), now = bbMouseRay(io.MousePos);
          double d0 = bb::dot(begin.direction, direction), d1 = bb::dot(now.direction, direction);
          if (std::abs(d0) > 1e-8 && std::abs(d1) > 1e-8) {
            auto a = bb::add(begin.origin,
                             bb::mul(begin.direction,
                                     bb::dot(bb::sub(bbGizmoPivot, begin.origin), direction) / d0)),
                 b = bb::add(now.origin,
                             bb::mul(now.direction,
                                     bb::dot(bb::sub(bbGizmoPivot, now.origin), direction) / d1)),
                 delta = bb::sub(b, a);
            for (auto &v : delta)
              v = bb::snap(v, step);
            bb::moveWorld(bbProject, bbTransformSelection(), delta);
          }
        } else
          bb::moveWorld(bbProject, bbTransformSelection(), bb::mul(direction, units));
      } else if (bbTool == 3) {
        for (auto id : bbIds())
          if (auto e = bbProject.find(id))
            bb::pivotObject(bbProject, id,
                            bb::add(bb::getv(*e, "origin"), bb::mul(direction, units)));
      } else if (bbTool == 2) {
        double rawAngle = (io.MousePos.x - bbDragMouse.x) * .5;
        auto begin = bbMouseRay(bbDragMouse), now = bbMouseRay(io.MousePos);
        double den0 = bb::dot(begin.direction, direction), den1 = bb::dot(now.direction, direction);
        if (std::abs(den0) > 1e-6 && std::abs(den1) > 1e-6) {
          auto a = bb::sub(
                   bb::add(begin.origin,
                           bb::mul(begin.direction,
                                   bb::dot(bb::sub(bbGizmoPivot, begin.origin), direction) / den0)),
                   bbGizmoPivot),
               b = bb::sub(
                   bb::add(now.origin,
                           bb::mul(now.direction,
                                   bb::dot(bb::sub(bbGizmoPivot, now.origin), direction) / den1)),
                   bbGizmoPivot);
          rawAngle = std::atan2(bb::dot(bb::cross(a, b), direction), bb::dot(a, b)) * 180 / bb::pi;
        }
        double angle = bb::snap(rawAngle, io.KeyCtrl ? 0 : io.KeyShift ? 1 : bbRotationSnap);
        if (bbSelectionMode > 0 && bbProject.find(bbActiveId()) &&
            bbProject.find(bbActiveId())->value("type", "") == "mesh") {
          bb::Vec3 rotation{};
          rotation[axis] = angle;
          bb::transformVertices(bbProject, bbTransformSelection(),
                                bb::translate(bbGizmoPivot) * bb::axisRotation(direction, angle) *
                                    bb::translate(bb::mul(bbGizmoPivot, -1)));
        } else
          bb::rotateWorld(bbProject, bbSelection, direction, angle);
      } else if (bbTool == 1 || bbTool == 5) {
        for (auto id : bbIds())
          if (auto e = bbProject.find(id)) {
            if (e->value("type", "") == "cube") {
              if (bbTool == 5) {
                auto stretch = bb::getv(*e, "stretch", {1, 1, 1});
                stretch[axis] = std::max(.001, stretch[axis] + units / std::max(1., extent));
                (*e)["stretch"] = stretch;
              } else {
                auto from = bb::getv(*e, "from"), to = bb::getv(*e, "to");
                if (negativeHandle) {
                  from[axis] += units;
                  if (io.KeyAlt)
                    to[axis] -= units;
                } else {
                  to[axis] += units;
                  if (io.KeyAlt)
                    from[axis] -= units;
                }
                (*e)["from"] = from;
                (*e)["to"] = to;
                bb::normalizeCube(*e);
                if (e->value("autouv", 0))
                  bb::autoUV(*e, e->value("autouv", 0) == 2);
              }
            } else if (e->value("type", "") == "mesh") {
              auto selected = bbTransformSelection();
              selected.elements = {id};
              bb::transformVertices(
                  bbProject, selected,
                  bb::translate(bbGizmoPivot) *
                      bb::axisScale(direction, std::max(.001, 1 + units / std::max(.1, extent))) *
                      bb::translate(bb::mul(bbGizmoPivot, -1)));
            }
          }
      }
      bbProject.revision++;
    }
    if (bbGizmoAxis >= 0 && ImGui::IsMouseReleased(0)) {
      try {
        bbHistory.finish(bbProject, bbSelection, "Transform elements");
      } catch (const std::exception &e) {
        bbNotice(e);
      }
      bbGizmoAxis = -1;
    }
    if (bbBoxSelecting) {
      dl->AddRect(bbBoxStart, io.MousePos, IM_COL32(100, 170, 255, 255));
      if (ImGui::IsMouseReleased(0)) {
        if (!io.KeyShift)
          bbSelection = {};
        float x0 = std::min(bbBoxStart.x, io.MousePos.x),
              x1 = std::max(bbBoxStart.x, io.MousePos.x),
              y0 = std::min(bbBoxStart.y, io.MousePos.y),
              y1 = std::max(bbBoxStart.y, io.MousePos.y);
        auto inside = [&](bb::Vec3 p) {
          ImVec2 q;
          return bbScreen(p, q) && q.x >= x0 && q.x <= x1 && q.y >= y0 && q.y <= y1;
        };
        for (auto &t : bbTriangles) {
          if (bb::locked(bbProject, t.element))
            continue;
          bool any = false;
          for (int i = 0; i < 3; i++)
            any |= inside(t.p[i]);
          if (bbSelectionMode == 0 && any)
            bbSelection.elements.insert(t.element);
          else if (bbSelectionMode == 1 &&
                   inside(bb::mul(bb::add(bb::add(t.p[0], t.p[1]), t.p[2]), 1. / 3))) {
            bbSelection.elements.insert(t.element);
            bbSelection.faces[t.element].insert(t.face);
          } else if (bbSelectionMode == 3)
            for (int i = 0; i < 3; i++)
              if (inside(t.p[i]) && !t.keys[i].empty()) {
                bbSelection.elements.insert(t.element);
                bbSelection.vertices[t.element].insert(t.keys[i]);
              }
        }
        if (bbSelectionMode == 2)
          for (auto &edge : bbPreviewEdges)
            if (!edge.aKey.empty() && !edge.bKey.empty() && !bb::locked(bbProject, edge.element) &&
                inside(bb::mul(bb::add(edge.a, edge.b), .5))) {
              bbSelection.elements.insert(edge.element);
              bbSelection.edges[edge.element].insert(bb::edgeKey(edge.aKey, edge.bKey));
            }
        bbBoxSelecting = false;
      }
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    if (auto payload = ImGui::AcceptDragDropPayload("RB_BB_TEXTURE")) {
      int texture = *static_cast<const int *>(payload->Data);
      if (hit.found)
        bbEdit("Assign texture", [&] { bbAssignTexture({hit.element}, texture); });
    }
    ImGui::EndDragDropTarget();
  }
  if (hovered && ImGui::IsMouseDoubleClicked(0) && hit.found) {
    auto parent = bb::parents(bbProject)[hit.element];
    if (!parent.empty())
      bbSelect(parent);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    bbPaintEnd(true);
    if (bbGizmoAxis >= 0 || bbUVDrag >= 0) {
      bbHistory.cancel(bbProject, bbSelection);
      bbGizmoAxis = bbUVDrag = -1;
    }
    bbBoxSelecting = bbSnapReady = false;
  }
  dl->PopClipRect();
  ImGui::EndTable();
}
static void bbTimeline() {
  if (bbAnimation < 0 || size_t(bbAnimation) >= bbProject.root["animations"].size())
    return;
  auto &animation = bbProject.root["animations"][bbAnimation];
  auto &io = ImGui::GetIO();
  if (FteIcons::button("##animation-play", bbPlaying ? 18 : 17, {28 * uiScale(), 28 * uiScale()}))
    bbPlaying = !bbPlaying;
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(bbPlaying ? "Pause" : "Play");
  ImGui::SameLine();
  if (ImGui::Button("Start"))
    bbPlayTime = 0;
  ImGui::SameLine();
  if (ImGui::Button("End"))
    bbPlayTime = bb::animationLength(animation);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80 * uiScale());
  ImGui::InputFloat("Time", &bbPlayTime, 0, 0, "%.3f");
  bbPlayTime = std::max(0.f, bbPlayTime);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80 * uiScale());
  ImGui::SliderFloat("Speed", &bbPlaySpeed, .1, 2);
  ImGui::SameLine();
  ImGui::Checkbox("Loop", &bbLoopPreview);
  if (ImGui::Button("Add keyframe"))
    bbAction("Add keyframe");
  ImGui::SameLine();
  if (ImGui::Button("Add marker"))
    bbEdit("Add marker",
           [&] { animation["markers"].push_back({{"time", bbPlayTime}, {"color", 0}}); });
  ImGui::BeginChild("timeline-scroll", {0, 0}, 0, ImGuiWindowFlags_HorizontalScrollbar);
  auto origin = ImGui::GetCursorScreenPos();
  auto avail = ImGui::GetContentRegionAvail();
  float labelWidth = 180 * uiScale(), rowHeight = 22 * uiScale(),
        width = std::max(avail.x,
                         float(bb::animationLength(animation) * bbTimelineZoom + labelWidth + 50));
  int fps = std::max(1, animation.value("snapping", 24));
  auto dl = ImGui::GetWindowDrawList();
  auto clipMin = ImGui::GetWindowPos(),
       clipMax = ImVec2(clipMin.x + ImGui::GetWindowSize().x, clipMin.y + ImGui::GetWindowSize().y);
  int firstFrame = std::max(0, int((clipMin.x - origin.x - labelWidth) * fps / bbTimelineZoom)),
      lastFrame = std::min(int(bb::animationLength(animation) * fps),
                           int((clipMax.x - origin.x - labelWidth) * fps / bbTimelineZoom) + 1);
  int frameStep = std::max(1, int(8 * fps / bbTimelineZoom));
  for (int frame = firstFrame; frame <= lastFrame; frame += frameStep) {
    float x = origin.x + labelWidth + frame * bbTimelineZoom / fps;
    dl->AddLine({x, origin.y}, {x, origin.y + 9}, IM_COL32(110, 110, 110, 255));
    if (frame % std::max(1, fps / 2) == 0)
      dl->AddText({x, origin.y + 11}, IM_COL32(210, 210, 210, 255), std::to_string(frame).c_str());
  }
  if (cvars->GetFloat("ftebroom_skip_setup"))
    bbTimelineProbe.clear();
  struct KeyPoint {
    std::string id, group, channel;
    float time, x, y;
  };
  std::vector<KeyPoint> points;
  int rows = 0;
  auto &closed = bbProject.root["editor_state"]["timeline_closed"];
  if (closed.is_null())
    closed = bb::Json::object();
  for (auto &group : bbProject.root["groups"]) {
    std::string id = group["uuid"], name = group.value("name", "group");
    float y = origin.y + 36 + rows++ * rowHeight;
    bool collapsed = closed.value(id, false);
    if (y >= clipMin.y - 20 && y < clipMax.y) {
      dl->AddText({origin.x, y}, IM_COL32(220, 220, 220, 255),
                  ((collapsed ? "> " : "v ") + name).c_str());
      if (io.MousePos.x < origin.x + labelWidth && io.MousePos.y >= y &&
          io.MousePos.y < y + rowHeight && ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
        closed[id] = !collapsed;
    }
    if (collapsed)
      continue;
    for (auto channel : {"rotation", "position", "scale"}) {
      float y = origin.y + 36 + rows++ * rowHeight;
      if (y >= clipMin.y - 20 && y < clipMax.y)
        dl->AddText({origin.x + 15, y}, IM_COL32(160, 175, 190, 255), channel);
      auto track = animation.value("animators", bb::Json::object()).value(id, bb::Json::object());
      for (auto &key : track.value("keyframes", bb::Json::array()))
        if (key.value("channel", "") == channel) {
          std::string uuid = key.value("uuid", "");
          float time = key.value("time", 0.f), x = origin.x + labelWidth + time * bbTimelineZoom;
          ImVec2 q{x, y + 7};
          points.push_back({uuid, id, channel, time, x, y + 7});
          if (cvars->GetFloat("ftebroom_skip_setup"))
            bbTimelineProbe.push_back({uuid, q});
          if (x < clipMin.x - 8 || x > clipMax.x + 8 || y < clipMin.y - 20 || y > clipMax.y)
            continue;
          auto color = bbSelection.keyframes.count(uuid) ? IM_COL32(255, 165, 55, 255)
                                                         : IM_COL32(150, 190, 235, 255);
          auto interp = key.value("interpolation", "linear");
          if (interp == "catmullrom")
            dl->AddCircleFilled(q, 5, color);
          else if (interp == "step")
            dl->AddRectFilled({x - 5, y + 2}, {x + 5, y + 12}, color);
          else {
            dl->AddQuadFilled({x, y + 1}, {x + 6, y + 7}, {x, y + 13}, {x - 6, y + 7}, color);
            if (interp == "bezier")
              dl->AddLine({x - 10, y + 7}, {x + 10, y + 7}, color, 1);
          }
        }
    }
  }
  float height = std::max(80.f, rows * rowHeight + 50);
  ImGui::InvisibleButton("timeline-area", {width, height});
  bool hovered = ImGui::IsItemHovered();
  if (hovered && io.MouseWheel && io.KeyCtrl) {
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    bbTimelineZoom = std::clamp(bbTimelineZoom * std::pow(1.2f, io.MouseWheel), 10.f, 2000.f);
  }
  static bool armed = false, dragging = false, box = false, scrub = false;
  static ImVec2 began;
  static std::map<std::string, double> startTimes;
  if (dragging && !bbHistory.active())
    armed = dragging = box = scrub = false;
  KeyPoint *hit = nullptr;
  double nearest = 8;
  for (auto &key : points) {
    double distance = std::hypot(io.MousePos.x - key.x, io.MousePos.y - key.y);
    if (distance < nearest) {
      nearest = distance;
      hit = &key;
    }
  }
  if (hovered && ImGui::IsMouseClicked(0) && io.MousePos.x >= origin.x + labelWidth) {
    began = io.MousePos;
    if (hit) {
      if (io.KeyShift && bbSelection.keyframes.count(hit->id))
        bbSelection.keyframes.erase(hit->id);
      else {
        if (!io.KeyShift && !bbSelection.keyframes.count(hit->id))
          bbSelection.keyframes.clear();
        bbSelection.keyframes.insert(hit->id);
      }
      bbSelection.groups.insert(hit->group);
      bbSelection.last = hit->group;
      armed = true;
    } else if (io.MousePos.y < origin.y + 30) {
      scrub = true;
    } else {
      box = true;
      if (!io.KeyShift)
        bbSelection.keyframes.clear();
    }
  }
  if (scrub && ImGui::IsMouseDown(0))
    bbPlayTime = std::max(
        0.f, float(bb::snap((io.MousePos.x - origin.x - labelWidth) / bbTimelineZoom, 1. / fps)));
  if (armed && !dragging && ImGui::IsMouseDragging(0, 3)) {
    bbHistory.begin(bbProject, bbSelection);
    dragging = true;
    startTimes.clear();
    if (io.KeyCtrl) {
      std::set<std::string> copies;
      for (auto &track : animation["animators"]) {
        auto old = track["keyframes"];
        for (auto key : old)
          if (bbSelection.keyframes.count(key.value("uuid", ""))) {
            key["uuid"] = bb::uuid();
            copies.insert(key["uuid"]);
            track["keyframes"].push_back(key);
          }
      }
      bbSelection.keyframes = copies;
    }
    for (auto &track : animation["animators"])
      for (auto &key : track["keyframes"])
        if (bbSelection.keyframes.count(key.value("uuid", "")))
          startTimes[key["uuid"].get<std::string>()] = key.value("time", 0.);
  }
  if (dragging && ImGui::IsMouseDown(0)) {
    double delta = bb::snap((io.MousePos.x - began.x) / bbTimelineZoom, 1. / fps), minimum = 0;
    for (auto [id, time] : startTimes)
      minimum = std::min(minimum, time + delta);
    delta -= minimum;
    for (auto &track : animation["animators"])
      for (auto &key : track["keyframes"]) {
        auto id = key.value("uuid", "");
        if (startTimes.count(id))
          key["time"] = startTimes[id] + delta;
      }
    bbProject.revision++;
  }
  if (box) {
    dl->AddRect(began, io.MousePos, IM_COL32(255, 180, 60, 255));
    if (ImGui::IsMouseReleased(0))
      for (auto &key : points)
        if (key.x >= std::min(began.x, io.MousePos.x) &&
            key.x <= std::max(began.x, io.MousePos.x) &&
            key.y >= std::min(began.y, io.MousePos.y) && key.y <= std::max(began.y, io.MousePos.y))
          bbSelection.keyframes.insert(key.id);
  }
  if (ImGui::IsMouseReleased(0)) {
    if (dragging)
      try {
        bbHistory.finish(bbProject, bbSelection, "Move keyframes");
      } catch (const std::exception &e) {
        bbNotice(e);
      }
    armed = dragging = box = scrub = false;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    if (dragging)
      bbHistory.cancel(bbProject, bbSelection);
    armed = dragging = box = scrub = false;
  }
  for (auto &marker : animation.value("markers", bb::Json::array())) {
    float time = marker.is_number() ? marker.get<float>() : marker.value("time", 0.f);
    float x = origin.x + labelWidth + time * bbTimelineZoom;
    dl->AddTriangleFilled({x - 5, origin.y + 2}, {x + 5, origin.y + 2}, {x, origin.y + 12},
                          IM_COL32(255, 200, 70, 255));
    if (hovered && std::abs(io.MousePos.x - x) < 6 && io.MousePos.y < origin.y + 20 &&
        ImGui::IsMouseClicked(0))
      bbPlayTime = time;
  }
  float x = origin.x + labelWidth + bbPlayTime * bbTimelineZoom;
  dl->AddLine({x, origin.y}, {x, origin.y + height}, IM_COL32(255, 100, 70, 255), 2);
  ImGui::EndChild();
}

static void bbResetInteraction() {
  bbUVCacheRevision = ~uint64_t(0);
  bbPaintEnd(true);
  if (bbHistory.active())
    bbHistory.cancel(bbProject, bbSelection);
  bbGizmoAxis = bbUVDrag = -1;
  bbSnapReady = bbBoxSelecting = bbUVBoxSelecting = false;
  bbUVVertices.clear();
  bbPaintSelection = bbPaintSelecting = bbPaintMoving = false;
}
