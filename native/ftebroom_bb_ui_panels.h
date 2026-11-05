// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
static std::function<void()> bbDeferred;
static std::string bbActiveId() {
  if (!bbSelection.last.empty() && bbProject.find(bbSelection.last))
    return bbSelection.last;
  if (!bbSelection.elements.empty())
    return *bbSelection.elements.begin();
  if (!bbSelection.groups.empty())
    return *bbSelection.groups.begin();
  return {};
}
static std::set<std::string> bbIds() {
  auto ids = bbSelection.elements;
  ids.insert(bbSelection.groups.begin(), bbSelection.groups.end());
  return ids;
}
static bb::Selection bbTransformSelection() {
  auto selection = bbSelection;
  if (bbSelectionMode == 0) {
    selection.vertices.clear();
    return selection;
  }
  for (auto id : selection.elements)
    if (auto e = bbProject.find(id))
      if (e->value("type", "") == "mesh") {
        if (bbSelectionMode == 1)
          for (auto face : selection.faces[id])
            if ((*e)["faces"].contains(face))
              for (auto &vertex : (*e)["faces"][face]["vertices"])
                selection.vertices[id].insert(vertex.get<std::string>());
        if (bbSelectionMode == 2)
          for (auto edge : selection.edges[id]) {
            auto at = edge.find('|');
            selection.vertices[id].insert(edge.substr(0, at));
            selection.vertices[id].insert(edge.substr(at + 1));
          }
      }
  return selection;
}
static void bbAssignTexture(const std::set<std::string> &ids, int texture) {
  for (auto &id : bb::descendants(bbProject, ids))
    if (auto e = bbProject.find(id))
      if (!bb::locked(bbProject, id) && e->contains("faces"))
        for (auto &f : (*e)["faces"])
          f["texture"] = texture;
}
static void bbSelect(std::string id, bool add = false) {
  if (!add)
    bbSelection = {};
  auto e = bbProject.find(id);
  if (!e)
    return;
  auto &selected = e->contains("type") ? bbSelection.elements : bbSelection.groups;
  if (add && selected.count(id))
    selected.erase(id);
  else
    selected.insert(id);
  bbSelection.last = id;
  if (!e->contains("type"))
    bbSelection.activeGroup = id;
}
static void bbVecField(const char *label, const char *key, bb::Vec3 fallback = {},
                       bool size = false, bool pivot = false) {
  auto ids = bbIds();
  if (ids.empty())
    return;
  auto first = bbProject.find(bbActiveId());
  if (!first)
    return;
  auto valueFor = [&](const std::string &id) {
    auto e = bbProject.find(id);
    auto v = bb::getv(*e, key, fallback);
    if (size) {
      if (e->value("type", "") == "mesh") {
        bb::Vec3 low{1e30, 1e30, 1e30}, high{-1e30, -1e30, -1e30};
        for (auto &point : (*e)["vertices"])
          for (int a = 0; a < 3; a++) {
            double n = point[a];
            low[a] = std::min(low[a], n);
            high[a] = std::max(high[a], n);
          }
        v = (*e)["vertices"].empty() ? bb::Vec3{} : bb::sub(high, low);
      } else
        v = bb::sub(bb::getv(*e, "to"), bb::getv(*e, "from"));
    }
    if (bbMode == 2 && bbAnimation >= 0 &&
        size_t(bbAnimation) < bbProject.root["animations"].size())
      v = bb::channel(
          bbProject.root["animations"][bbAnimation]["animators"].value(id, bb::Json::object()),
          std::string(key) == "rotation" ? "rotation"
          : size                         ? "scale"
                                         : "position",
          bbPlayTime);
    if (bbQuakeInspector)
      v = bb::bbToQuake(v, bbProject.front());
    return v;
  };
  auto initial = valueFor(bbActiveId());
  std::vector<bb::Vec3> selectedValues;
  for (auto &id : ids)
    selectedValues.push_back(valueFor(id));
  ImGui::PushID(label);
  ImGui::TextUnformatted(label);
  float width =
      std::max(36.f, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3);
  static std::map<ImGuiID, std::string> edits;
  for (int axis = 0; axis < 3; axis++) {
    if (axis)
      ImGui::SameLine();
    ImGui::PushID(axis);
    auto widget = ImGui::GetID("##value");
    bool active = ImGui::GetActiveID() == widget;
    char text[128];
    bool mixed = false;
    for (auto value : selectedValues)
      if (std::abs(value[axis] - initial[axis]) > 1e-6)
        mixed = true;
    if (!active)
      edits[widget] = mixed ? "" : std::to_string(initial[axis]);
    std::snprintf(text, sizeof text, "%s", edits[widget].c_str());
    ImGui::SetNextItemWidth(width);
    bool enter = ImGui::InputTextWithHint("##value",
                                          mixed       ? "mixed"
                                          : axis == 0 ? "X"
                                          : axis == 1 ? "Y"
                                                      : "Z",
                                          text, sizeof text, ImGuiInputTextFlags_EnterReturnsTrue);
    edits[widget] = text;
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s: %c. Numbers or expressions: +2, *2, 8/3", label, 'X' + axis);
    if ((enter || ImGui::IsItemDeactivatedAfterEdit()) && *text)
      bbEdit(label, [&] {
        for (auto id : ids)
          if (auto e = bbProject.find(id)) {
            if (bb::locked(bbProject, id))
              continue;
            auto v = valueFor(id);
            v[axis] = bb::expression(text, v[axis]);
            if (bbQuakeInspector)
              v = bb::quakeToBb(v, bbProject.front());
            if (bbMode == 2 && !e->contains("type") && bbAnimation >= 0 &&
                size_t(bbAnimation) < bbProject.root["animations"].size())
              bb::setKey(bbProject.root["animations"][bbAnimation], id,
                         std::string(key) == "rotation" ? "rotation"
                         : size                         ? "scale"
                                                        : "position",
                         bbPlayTime, v);
            else if (pivot)
              bb::pivotObject(bbProject, id, v);
            else if (size && e->value("type", "") == "mesh") {
              bb::Vec3 lo{1e30, 1e30, 1e30}, hi{-1e30, -1e30, -1e30};
              for (auto &point : (*e)["vertices"])
                for (int a = 0; a < 3; a++) {
                  double n = point[a];
                  lo[a] = std::min(lo[a], n);
                  hi[a] = std::max(hi[a], n);
                }
              for (auto &point : (*e)["vertices"])
                for (int a = 0; a < 3; a++)
                  if (hi[a] - lo[a] > 1e-9)
                    point[a] = lo[a] + (point[a].get<double>() - lo[a]) * v[a] / (hi[a] - lo[a]);
            } else if (size) {
              (*e)["to"] = bb::add(bb::getv(*e, "from"), v);
              bb::normalizeCube(*e);
              if (e->value("autouv", 0))
                bb::autoUV(*e, e->value("autouv", 0) == 2);
            } else if (std::string(key) == "from" && e->value("type", "") == "cube")
              bb::translateObject(*e, bb::sub(v, bb::getv(*e, "from")));
            else
              (*e)[key] = v;
          }
        bbProject.revision++;
      });
    ImGui::PopID();
  }
  ImGui::PopID();
}
static void bbInspector() {
  auto id = bbActiveId();
  auto e = bbProject.find(id);
  if (!e) {
    ImGui::TextDisabled("Select an element or group");
    return;
  }
  ImGui::PushID(id.c_str());
  uiSectionHeading(bbMode == 2 ? "Animation pose" : "Element");
  ImGui::TextDisabled(bbQuakeInspector ? "Quake coordinates Z = up" : "Project coordinates Y = up");
  char name[256];
  std::snprintf(name, sizeof name, "%s", e->value("name", "object").c_str());
  if (ImGui::InputText("Name", name, sizeof name, ImGuiInputTextFlags_EnterReturnsTrue))
    bbEdit("Rename element", [&] {
      for (auto &k : bbIds())
        (*bbProject.find(k))["name"] = name;
    });
  auto type = e->value("type", "");
  if (bbMode == 2) {
    ImGui::TextWrapped("Transform a selected group to create a key at the playhead.");
    if (type.empty()) {
      bbVecField("Position", "animation_position");
      bbVecField("Rotation", "rotation");
      bbVecField("Scale", "animation_scale", {1, 1, 1}, true);
    }
  } else {
    if (type == "cube") {
      bbVecField("Position", "from");
      bbVecField("Size", "to", {}, true);
      float inflate = e->value("inflate", 0.f);
      if (ImGui::InputFloat("Inflate", &inflate, .25, 1))
        bbEdit("Inflate cube", [&] {
          for (auto &k : bbIds())
            (*bbProject.find(k))["inflate"] = inflate;
        });
      bbVecField("Stretch", "stretch", {1, 1, 1});
    } else if (type == "locator")
      bbVecField("Position", "position");
    else if (type == "mesh") {
      bbVecField("Position", "origin");
      bbVecField("Size", "to", {}, true);
    }
    bbVecField("Pivot", "origin", {}, false, true);
    bbVecField("Rotation", "rotation");
  }
  e = bbProject.find(id);
  if (!e) {
    ImGui::PopID();
    return;
  }
  for (auto field : {std::pair<const char *, const char *>{"Visible", "visibility"},
                     {"Locked", "locked"},
                     {"Export", "export"}}) {
    bool value = e->value(field.second, std::string(field.second) != "locked");
    if (ImGui::Checkbox(field.first, &value))
      bbEdit("Element flags", [&] {
        for (auto &k : bbIds())
          (*bbProject.find(k))[field.second] = value;
      });
    if (std::string(field.second) != "export")
      ImGui::SameLine();
  }
  int color = e->value("color", 0);
  if (ImGui::SliderInt("Marker color", &color, 0, 7))
    bbEdit("Marker color", [&] {
      for (auto &k : bbIds())
        (*bbProject.find(k))["color"] = color;
    });
  if (type == "cube") {
    bool box = e->value("box_uv", false), mirror = e->value("mirror_uv", false),
         shade = e->value("shade", true);
    if (ImGui::Checkbox("Box UV", &box))
      bbEdit("Box UV", [&] { (*bbProject.find(id))["box_uv"] = box; });
    if (ImGui::Checkbox("Mirror UV", &mirror))
      bbEdit("Mirror UV", [&] { (*bbProject.find(id))["mirror_uv"] = mirror; });
    if (ImGui::Checkbox("Shade", &shade))
      bbEdit("Shade cube", [&] { (*bbProject.find(id))["shade"] = shade; });
    int autouv = e->value("autouv", 0);
    if (ImGui::Combo("Auto UV", &autouv, "Off\0On\0Relative\0"))
      bbEdit("Auto UV", [&] {
        (*bbProject.find(id))["autouv"] = autouv;
        bb::autoUV(*bbProject.find(id), autouv == 2);
      });
  }
  if (type == "mesh") {
    int shading = e->value("shading", "flat") == "smooth";
    if (ImGui::Combo("Shading", &shading, "Flat\0Smooth\0"))
      bbEdit("Mesh shading",
             [&] { (*bbProject.find(id))["shading"] = shading ? "smooth" : "flat"; });
  }
  ImGui::PopID();
}
static void bbOutlinerNode(const bb::Json &node, int depth = 0) {
  auto id = node.is_string() ? node.get<std::string>() : node.value("uuid", "");
  auto e = bbProject.find(id);
  if (!e)
    return;
  auto name = e->value("name", "object");
  bbOutlinerBuild.push_back(id);
  bool group = node.is_object();
  if (*bbOutlinerSearch &&
      entityLower(name).find(entityLower(bbOutlinerSearch)) == std::string::npos && !group)
    return;
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::Indent(depth * 12 * uiScale());
  ImGui::PushID(id.c_str());
  bool selected = bbSelection.elements.count(id) || bbSelection.groups.count(id);
  auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
               ImGuiTreeNodeFlags_NoTreePushOnOpen | (selected ? ImGuiTreeNodeFlags_Selected : 0);
  if (!group)
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  if (group && bbCollapseNodes.erase(id))
    ImGui::SetNextItemOpen(false, ImGuiCond_Always);
  else if (group)
    ImGui::SetNextItemOpen(bbProject.root["editor_state"]
                               .value("outliner_open", bb::Json::object())
                               .value(id, node.value("isOpen", true)),
                           ImGuiCond_Once);
  bool opened = ImGui::TreeNodeEx("node", flags, "   %s", name.c_str());
  {
    auto origin = ImGui::GetItemRectMin();
    origin.x += ImGui::GetTreeNodeToLabelSpacing();
    origin.y += (ImGui::GetTextLineHeight() - 12 * uiScale()) * .5f;
    int index = group                            ? 6
                : e->value("type", "") == "cube" ? 1
                : e->value("type", "") == "mesh" ? 3
                                                 : 4;
    auto dl = ImGui::GetWindowDrawList();
    float scale = 12 * uiScale() / 24;
    for (int n = 0; n < FteIcons::icons[index].count; n++) {
      auto &stroke = FteIcons::icons[index].strokes[n];
      dl->PathClear();
      for (int v = 0; v < stroke.count; v++)
        dl->PathLineTo(
            {origin.x + stroke.points[v].x * scale, origin.y + stroke.points[v].y * scale});
      dl->PathStroke(ImGui::GetColorU32(ImGuiCol_Text), 0, std::max(1.f, uiScale()));
    }
  }
  if (group && ImGui::IsItemToggledOpen())
    bbProject.root["editor_state"]["outliner_open"][id] = opened;
  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
    auto anchor = std::find(bbOutlinerOrder.begin(), bbOutlinerOrder.end(), bbSelection.last),
         clicked = std::find(bbOutlinerOrder.begin(), bbOutlinerOrder.end(), id);
    if (ImGui::GetIO().KeyShift && anchor != bbOutlinerOrder.end() &&
        clicked != bbOutlinerOrder.end()) {
      if (!ImGui::GetIO().KeyCtrl)
        bbSelection = {};
      for (auto it = std::min(anchor, clicked); it <= std::max(anchor, clicked); ++it)
        if (auto item = bbProject.find(*it)) {
          if (item->contains("type"))
            bbSelection.elements.insert(*it);
          else
            bbSelection.groups.insert(*it);
        }
      bbSelection.last = id;
    } else
      bbSelect(id, ImGui::GetIO().KeyCtrl);
  }
  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
    bbSelect(id);
    bbDeferred = [] { bbAction("Rename"); };
  }
  if (ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload("RB_BB_NODE", id.c_str(), id.size() + 1);
    ImGui::TextUnformatted(name.c_str());
    ImGui::EndDragDropSource();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (auto payload = ImGui::AcceptDragDropPayload("RB_BB_NODE")) {
      std::string moving = static_cast<const char *>(payload->Data);
      bbDeferred = [moving, id, group] {
        bbEdit("Reparent elements", [&] {
          auto top = std::set<std::string>{moving};
          if (ImGui::GetIO().KeyAlt) {
            bbSelect(moving);
            bb::duplicate(bbProject, bbSelection);
            top = bb::selectedTop(bbProject, bbSelection);
          }
          bb::reparent(bbProject, top, group ? id : bb::parents(bbProject)[id]);
        });
      };
    }
    if (auto payload = ImGui::AcceptDragDropPayload("RB_BB_TEXTURE")) {
      int t = *static_cast<const int *>(payload->Data);
      bbDeferred = [id, t] { bbEdit("Assign texture", [&] { bbAssignTexture({id}, t); }); };
    }
    ImGui::EndDragDropTarget();
  }
  if (ImGui::BeginPopupContextItem("node-menu")) {
    if (!selected)
      bbSelect(id);
    for (auto action : {"Rename", "Duplicate", "Delete", "Convert to Mesh", "Origin to Geometry",
                        "Resolve group", "Sort"})
      if (ImGui::MenuItem(action))
        bbDeferred = [a = std::string(action)] { bbAction(a); };
    if (ImGui::BeginMenu("Marker color")) {
      for (int color = 0; color < 8; color++)
        if (ImGui::MenuItem(std::to_string(color).c_str()))
          bbDeferred = [color] {
            bbEdit("Marker color", [&] {
              for (auto id : bbIds())
                if (auto item = bbProject.find(id))
                  (*item)["color"] = color;
            });
          };
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Texture")) {
      for (size_t i = 0; i < bbProject.textures.size(); i++)
        if (ImGui::MenuItem(bbProject.textures[i].name.c_str()))
          bbDeferred = [id, i] { bbEdit("Assign texture", [&] { bbAssignTexture({id}, i); }); };
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  int flagColumn = 1;
  for (auto field : {std::pair<const char *, const char *>{"Visibility", "visibility"},
                     {"Lock", "locked"},
                     {"Export", "export"}}) {
    ImGui::TableSetColumnIndex(flagColumn++);
    bool value = e->value(field.second, std::string(field.second) != "locked");
    ImGui::PushID(field.second);
    if (ImGui::Checkbox("##flag", &value)) {
      auto key = std::string(field.second);
      bbDeferred = [id, key, value] {
        bbEdit("Element flags", [&] { (*bbProject.find(id))[key] = value; });
      };
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", field.first);
    ImGui::PopID();
  }
  ImGui::TableSetColumnIndex(0);
  ImGui::Unindent(depth * 12 * uiScale());
  ImGui::PopID();
  if (group && opened)
    for (auto &child : node["children"])
      bbOutlinerNode(child, depth + 1);
}
static void bbOutliner() {
  uiSectionHeading("Outliner");
  ImGui::InputTextWithHint("##outliner-search", "Search elements", bbOutlinerSearch,
                           sizeof bbOutlinerSearch);
  for (auto label : {"Cube", "Group", "Mesh", "Locator"}) {
    if (std::string(label) != "Cube")
      ImGui::SameLine();
    if (ImGui::Button(label))
      bbDeferred = [action = std::string("Add ") + label] { bbAction(action); };
  }
  if (ImGui::Button("Collapse all"))
    for (auto &group : bbProject.root["groups"]) {
      std::string id = group["uuid"];
      bbProject.root["editor_state"]["outliner_open"][id] = false;
      bbCollapseNodes.insert(id);
    }
  bbOutlinerBuild.clear();
  ImGui::BeginChild("outliner-tree", {0, 0});
  if (ImGui::BeginTable("outliner-columns", 4, ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    for (auto name : {"V", "L", "E"})
      ImGui::TableSetupColumn(name, ImGuiTableColumnFlags_WidthFixed, 22 * uiScale());
    for (auto &n : bbProject.root["outliner"])
      bbOutlinerNode(n);
    ImGui::EndTable();
  }
  ImGui::EndChild();
  bbOutlinerOrder = bbOutlinerBuild;
  if (bbDeferred) {
    auto fn = std::move(bbDeferred);
    bbDeferred = {};
    fn();
  }
}
static void bbImportTexturePath(const std::filesystem::path &path) {
  auto decoded = fileImageDecode(path, true,
                                 gameProfile == 3 ? q2Palette() : modelResource("gfx/palette.lmp"));
  if (!decoded.error.empty())
    throw std::runtime_error(decoded.error);
  auto skin = modelImportDecode(decoded.bytes);
  bb::Texture t;
  t.id = bb::uuid();
  t.name = path.filename().u8string();
  t.path = modFileModelPath(path);
  if (t.path.empty())
    t.path = path.u8string();
  t.width = t.uvWidth = skin.width;
  t.height = t.uvHeight = skin.height;
  if (t.width > 2048 || t.height > 2048)
    throw std::runtime_error("Model textures are limited to 2048 pixels");
  t.rgba = std::move(skin.rgba);
  bbEdit("Import texture", [&] {
    bb::addTexture(bbProject, t);
    bbTexture = bbProject.textures.size() - 1;
  });
}
static void bbTextureOperation(const std::string &name) {
  if (bbTexture < 0 || size_t(bbTexture) >= bbProject.textures.size())
    return;
  bbEdit(name.c_str(),
         [&] {
           auto &t = bbProject.textures[bbTexture];
           auto old = t;
           for (int y = 0; y < t.height; y++)
             for (int x = 0; x < t.width; x++) {
               auto c = bb::pixel(old, x, y);
               if (name == "Flip X")
                 c = bb::pixel(old, t.width - 1 - x, y);
               else if (name == "Flip Y")
                 c = bb::pixel(old, x, t.height - 1 - y);
               else if (name == "Invert colors")
                 for (int a = 0; a < 3; a++)
                   c[a] = 255 - c[a];
               else if (name == "Limit to palette")
                 c = bb::quantize(c, bbPalette);
               else if (name == "Brightness +10%")
                 for (int a = 0; a < 3; a++)
                   c[a] = std::min(255, int(c[a] * 1.1));
               else if (name == "Opacity 50%")
                 c[3] /= 2;
               std::copy(c.begin(), c.end(), t.rgba.begin() + (size_t(y) * t.width + x) * 4);
             }
           if (name == "Rotate CW" || name == "Rotate CCW") {
             t.width = old.height;
             t.height = old.width;
             t.rgba.resize(old.rgba.size());
             for (int y = 0; y < t.height; y++)
               for (int x = 0; x < t.width; x++) {
                 auto c = name == "Rotate CW" ? bb::pixel(old, y, old.height - 1 - x)
                                              : bb::pixel(old, old.width - 1 - y, x);
                 std::copy(c.begin(), c.end(), t.rgba.begin() + (size_t(y) * t.width + x) * 4);
               }
           }
           if (name == "Use as default")
             for (size_t i = 0; i < bbProject.root["textures"].size(); i++)
               bbProject.root["textures"][i]["use_as_default"] = i == size_t(bbTexture);
           t.saved = false;
           t.revision++;
         },
         {size_t(bbTexture)});
}
static void bbTexturePanel() {
  uiSectionHeading("Textures");
  if (ImGui::Button("Import..."))
    bbChoose(false, modFileRoot(), [](auto path) {
      try {
        bbImportTexturePath(path);
      } catch (const std::exception &e) {
        bbNotice(e);
      }
    });
  ImGui::SameLine();
  if (ImGui::Button("Create..."))
    bbTemplateOpen = true;
  if (ImGui::BeginCombo("Materials", "Import from material browser")) {
    static char filter[128] = {};
    ImGui::InputText("Filter", filter, sizeof filter);
    int shown = 0;
    for (auto &[name, material] : materials)
      if (entityLower(name).find(entityLower(filter)) != std::string::npos && shown++ < 256)
        if (ImGui::Selectable(name.c_str()))
          try {
            auto bytes = modelImportTexture(material.preview.empty() ? name : material.preview);
            auto skin = modelImportDecode(bytes);
            bb::Texture t;
            t.id = bb::uuid();
            t.name = name;
            t.width = t.uvWidth = skin.width;
            t.height = t.uvHeight = skin.height;
            t.rgba = std::move(skin.rgba);
            bbEdit("Import material image", [&] {
              bb::addTexture(bbProject, t);
              bbTexture = bbProject.textures.size() - 1;
            });
          } catch (const std::exception &e) {
            bbNotice(e);
          }
    ImGui::EndCombo();
  }
  for (size_t i = 0; i < bbProject.textures.size(); i++) {
    auto &t = bbProject.textures[i];
    ImGui::PushID(int(i));
    auto image = bbImage(i);
    if (image)
      ImGui::Image((ImTextureID)image, {40 * uiScale(), 40 * uiScale()});
    ImGui::SameLine();
    if (ImGui::Selectable((t.name + (t.saved ? "" : " *") + "  " + std::to_string(t.width) + " x " +
                           std::to_string(t.height))
                              .c_str(),
                          bbTexture == int(i), 0, {0, 40 * uiScale()}))
      bbTexture = i;
    if (ImGui::BeginDragDropSource()) {
      int index = i;
      ImGui::SetDragDropPayload("RB_BB_TEXTURE", &index, sizeof index);
      ImGui::TextUnformatted(t.name.c_str());
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginPopupContextItem("texture-menu")) {
      for (auto action :
           {"Save as...", "Refresh", "Delete", "Resize texture...", "Flip X", "Flip Y", "Rotate CW",
            "Rotate CCW", "Invert colors", "Adjust colors...", "Clear unused pixels",
            "Brightness +10%", "Opacity 50%", "Limit to palette", "Use as default"})
        if (ImGui::MenuItem(action)) {
          bbTexture = i;
          bbDeferred = [name = std::string(action), i] {
            if (name == "Save as...")
              bbChoose(true,
                       (modFileRoot() / "textures" / bbProject.textures[i].name)
                           .replace_extension(".png"),
                       [](auto path) {
                         try {
                           modFileValidate(path);
                           if (path.extension() != ".png" && path.extension() != ".tga")
                             throw std::runtime_error("Save model textures as PNG or TGA");
                           auto &t = bbProject.textures[bbTexture];
                           std::filesystem::create_directories(path.parent_path());
                           bb::atomicWrite(path,
                                           path.extension() == ".tga" ? bb::tga(t) : bb::png(t));
                           t.path = path.u8string();
                           t.saved = true;
                         } catch (const std::exception &e) {
                           bbNotice(e);
                         }
                       });
            else if (name == "Delete")
              bbEdit(
                  "Delete texture",
                  [&] {
                    bb::deleteTexture(bbProject, i);
                    bbTexture = std::min(int(i), int(bbProject.textures.size()) - 1);
                  },
                  bbTextureIndices());
            else if (name == "Refresh")
              try {
                auto &t = bbProject.textures[i];
                auto path = bbPath(t.path);
                auto replacement = bb::decodeImage(modFileBytes(path));
                replacement.id = t.id;
                replacement.name = t.name;
                replacement.path = t.path;
                replacement.revision = t.revision + 1;
                bbEdit("Refresh texture", [&] { bbProject.textures[i] = replacement; }, {i});
              } catch (const std::exception &e) {
                bbNotice(e);
              }
            else if (name == "Resize texture...")
              bbAction(name);
            else
              bbTextureOperation(name);
          };
        }
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }
  if (bbDeferred) {
    auto fn = std::move(bbDeferred);
    bbDeferred = {};
    fn();
  }
}
static void bbColorPanel() {
  uiSectionHeading("Color");
  if (ImGui::ColorPicker4("##paint-color", bbColor,
                          ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex))
    for (int a = 0; a < 4; a++)
      bbBrush.color[a] = std::clamp(int(bbColor[a] * 255 + .5), 0, 255);
  if (ImGui::Button("Swap colors (X)")) {
    std::swap(bbBrush.color, bbSecondary);
    for (int a = 0; a < 4; a++)
      bbColor[a] = bbBrush.color[a] / 255.f;
  }
  ImGui::Checkbox("Limit to palette while painting", &bbLimitPalette);
  uiSectionHeading("Palette");
  if (ImGui::Button("Add to palette"))
    bbPalette.push_back(bbBrush.color);
  ImGui::SameLine();
  if (ImGui::Button("Generate")) {
    std::set<bb::Pixel> unique;
    if (bbTexture >= 0 && size_t(bbTexture) < bbProject.textures.size()) {
      auto &t = bbProject.textures[bbTexture];
      for (size_t n = 0; n < t.rgba.size() && unique.size() < 256; n += 4)
        unique.insert({t.rgba[n], t.rgba[n + 1], t.rgba[n + 2], t.rgba[n + 3]});
      bbPalette.assign(unique.begin(), unique.end());
    }
  }
  if (ImGui::Button("Sort"))
    std::sort(bbPalette.begin(), bbPalette.end(),
              [](auto a, auto b) { return int(a[0]) + a[1] + a[2] < int(b[0]) + b[1] + b[2]; });
  ImGui::SameLine();
  if (ImGui::Button("Import palette..."))
    bbAction("Import palette");
  ImGui::SameLine();
  if (ImGui::Button("Export palette..."))
    bbAction("Export palette");
  for (size_t i = 0; i < bbPalette.size(); i++) {
    if (i == 224 && gameProfile == 2) {
      ImGui::NewLine();
      ImGui::TextDisabled("Fullbright");
    }
    if (i % 8)
      ImGui::SameLine();
    ImGui::PushID(int(i));
    auto c = bbPalette[i];
    if (ImGui::ColorButton("##swatch", {c[0] / 255.f, c[1] / 255.f, c[2] / 255.f, c[3] / 255.f}, 0,
                           {20 * uiScale(), 20 * uiScale()})) {
      bbBrush.color = c;
      for (int a = 0; a < 4; a++)
        bbColor[a] = c[a] / 255.f;
    }
    if (ImGui::IsItemClicked(1) && gameProfile != 2) {
      bbPalette.erase(bbPalette.begin() + i);
      ImGui::PopID();
      break;
    }
    ImGui::PopID();
  }
}
static void bbAnimationPanel() {
  uiSectionHeading("Animations");
  if (ImGui::Button("Add animation"))
    bbEdit("Add animation", [] {
      bbProject.root["animations"].push_back({{"uuid", bb::uuid()},
                                              {"name", "animation.model.new"},
                                              {"loop", "once"},
                                              {"length", 1},
                                              {"snapping", bbAnimationSnap},
                                              {"animators", bb::Json::object()}});
      bbAnimation = bbProject.root["animations"].size() - 1;
    });
  ImGui::SameLine();
  if (ImGui::Button("Duplicate##animation") && bbAnimation >= 0 &&
      size_t(bbAnimation) < bbProject.root["animations"].size())
    bbEdit("Duplicate animation", [] {
      auto a = bbProject.root["animations"][bbAnimation];
      a["uuid"] = bb::uuid();
      a["name"] = a.value("name", "animation") + " copy";
      for (auto &track : a["animators"])
        for (auto &k : track["keyframes"])
          k["uuid"] = bb::uuid();
      bbProject.root["animations"].push_back(a);
      bbAnimation = bbProject.root["animations"].size() - 1;
    });
  ImGui::SameLine();
  if (ImGui::Button("Delete##animation") && bbAnimation >= 0 &&
      size_t(bbAnimation) < bbProject.root["animations"].size())
    bbEdit("Delete animation", [] {
      bbProject.root["animations"].erase(bbAnimation);
      bbAnimation = 0;
    });
  for (size_t i = 0; i < bbProject.root["animations"].size(); i++) {
    auto &a = bbProject.root["animations"][i];
    ImGui::PushID(int(i));
    auto label = a.value("name", "animation") + "  " + a.value("loop", "once");
    if (ImGui::Selectable(label.c_str(), bbAnimation == int(i))) {
      bbAnimation = i;
      bbPlayTime = 0;
      bbSelection.keyframes.clear();
    }
    ImGui::PopID();
  }
  if (bbAnimation < 0 || size_t(bbAnimation) >= bbProject.root["animations"].size())
    return;
  auto &a = bbProject.root["animations"][bbAnimation];
  char name[256];
  std::snprintf(name, sizeof name, "%s", a.value("name", "animation").c_str());
  if (ImGui::InputText("Animation name", name, sizeof name, ImGuiInputTextFlags_EnterReturnsTrue))
    bbEdit("Rename animation", [&] { a["name"] = name; });
  int loop = a.value("loop", "once") == "loop" ? 1 : a.value("loop", "once") == "hold" ? 2 : 0;
  if (ImGui::Combo("Loop mode", &loop, "Once\0Loop\0Hold\0"))
    bbEdit("Animation loop", [&] { a["loop"] = loop == 1 ? "loop" : loop == 2 ? "hold" : "once"; });
  float length = bb::animationLength(a);
  if (ImGui::InputFloat("Length", &length, .1, 1))
    bbEdit("Animation length", [&] { a["length"] = std::clamp(length, 0.f, 3600.f); });
  if (ImGui::Button("Set animation end"))
    bbEdit("Set animation end", [&] { a["length"] = bbPlayTime; });
  int snap = a.value("snapping", 24);
  if (ImGui::InputInt("Snapping FPS", &snap))
    bbEdit("Animation snapping", [&] { a["snapping"] = std::clamp(snap, 10, 500); });
  float fps = a.value("reyondbroom", bb::Json::object()).value("export_fps", 10.f);
  if (ImGui::InputFloat("Export FPS", &fps, 1, 10))
    bbEdit("Export FPS", [&] { a["reyondbroom"]["export_fps"] = std::clamp(fps, 1.f, 120.f); });
  std::snprintf(name, sizeof name, "%s",
                a.value("reyondbroom", bb::Json::object())
                    .value("export_name", a.value("name", "animation"))
                    .c_str());
  if (ImGui::InputText("Export frame name", name, sizeof name,
                       ImGuiInputTextFlags_EnterReturnsTrue))
    bbEdit("Export frame names", [&] { a["reyondbroom"]["export_name"] = name; });
  bool override = a.value("override", false);
  if (ImGui::Checkbox("Override", &override))
    bbEdit("Animation override", [&] { a["override"] = override; });
  uiSectionHeading("Keyframe");
  for (auto &track : a["animators"])
    for (auto &k : track["keyframes"])
      if (bbSelection.keyframes.count(k.value("uuid", ""))) {
        ImGui::PushID(k.value("uuid", "").c_str());
        float time = k.value("time", 0.f);
        if (ImGui::InputFloat("Time", &time, .01, .1))
          bbEdit("Keyframe time", [&] { k["time"] = std::clamp(time, 0.f, 86400.f); });
        auto v = bb::keyValue(k);
        float value[3] = {float(v[0]), float(v[1]), float(v[2])};
        if (ImGui::InputFloat3("Value XYZ", value))
          bbEdit("Keyframe values", [&] {
            auto copy =
                bb::keyframe(k.value("channel", "rotation"), time, {value[0], value[1], value[2]});
            k["data_points"] = copy["data_points"];
          });
        std::string interp = k.value("interpolation", "linear");
        int mode = interp == "catmullrom" ? 1 : interp == "bezier" ? 2 : interp == "step" ? 3 : 0;
        if (ImGui::Combo("Interpolation", &mode, "Linear\0Catmull Rom\0Bezier\0Step\0"))
          bbEdit("Keyframe interpolation", [&] {
            k["interpolation"] = mode == 1   ? "catmullrom"
                                 : mode == 2 ? "bezier"
                                 : mode == 3 ? "step"
                                             : "linear";
          });
        int channel = k.value("channel", "") == "position" ? 1
                      : k.value("channel", "") == "scale"  ? 2
                                                           : 0;
        if (ImGui::Combo("Channel", &channel, "Rotation\0Position\0Scale\0"))
          bbEdit("Keyframe channel", [&] {
            k["channel"] = channel == 1 ? "position" : channel == 2 ? "scale" : "rotation";
          });
        bool second = k["data_points"].size() > 1;
        if (ImGui::Checkbox("Separate before / after values", &second))
          bbEdit("Keyframe discontinuity", [&] {
            if (second)
              k["data_points"].push_back(k["data_points"][0]);
            else
              while (k["data_points"].size() > 1)
                k["data_points"].erase(1);
          });
        if (second) {
          auto point = bb::keyValue(k, true);
          float v[3] = {float(point[0]), float(point[1]), float(point[2])};
          if (ImGui::InputFloat3("After XYZ", v))
            bbEdit("Keyframe post value", [&] {
              auto key = bb::keyframe(k.value("channel", "rotation"), time, {v[0], v[1], v[2]});
              k["data_points"][1] = key["data_points"][0];
            });
        }
        if (k.value("interpolation", "") == "bezier") {
          for (auto key : {"bezier_left_time", "bezier_left_value", "bezier_right_time",
                           "bezier_right_value"}) {
            auto v = bb::getv(k, key);
            float q[3] = {float(v[0]), float(v[1]), float(v[2])};
            if (ImGui::InputFloat3(key, q))
              bbEdit("Bezier handle", [&] {
                k[key] = {q[0], q[1], q[2]};
                if (k.value("bezier_linked", true)) {
                  std::string opposite = std::string(key).find("left") != std::string::npos
                                             ? std::string(key).replace(7, 4, "right")
                                             : std::string(key).replace(7, 5, "left");
                  k[opposite] = {-q[0], -q[1], -q[2]};
                }
              });
          }
          bool linked = k.value("bezier_linked", true);
          if (ImGui::Checkbox("Linked handles", &linked))
            bbEdit("Bezier linked handles", [&] { k["bezier_linked"] = linked; });
        }
        ImGui::PopID();
        break;
      }
}
static void bbLeft() {
  if (bbMode == 0) {
    bbUVPanel();
    bbTexturePanel();
  } else if (bbMode == 1) {
    bbColorPanel();
    bbTexturePanel();
  } else
    bbAnimationPanel();
}
