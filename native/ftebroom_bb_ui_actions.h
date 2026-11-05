// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
static bool bbResizeOpen = false;
static int bbResizeW = 64, bbResizeH = 64;
static bool bbResizeUV = true;
static bool bbNativeImport = false;
static std::string bbNativeSource;
static void bbConvertNative(const std::string &source) {
  if (modelConverterPending || bbNativeImport)
    throw std::runtime_error("Wait for the active model conversion");
  bbNativeSource = source;
  bbNativeImport = true;
  cvars->SetString("ftebroom_convert_source", source.c_str());
  cvars->SetFloat("ftebroom_convert_sequence", 0);
  cvars->SetFloat("ftebroom_convert_frames", 1);
  cvars->SetFloat("ftebroom_convert_fps", 10);
  cvars->SetFloat("ftebroom_convert_skin", modelViewerSkin);
  cvars->SetFloat("ftebroom_convert_status", 0);
  command("ftebroom_model_extract\n");
}
static void bbNativeImportTick() {
  if (!bbNativeImport || !cvars->GetFloat("ftebroom_convert_status"))
    return;
  bbNativeImport = false;
  try {
    if (cvars->GetFloat("ftebroom_convert_status") != 1)
      throw std::runtime_error("The engine could not extract this model");
    auto mesh = ftebroom::modelExport::parse(modelResource("data/model-convert.mesh"));
    if (modelEditLoaded && modelEditNodes.size() == mesh.surfaces.size() + 1) {
      std::vector<ftebroom::modelEdit::Matrix> transforms;
      for (size_t i = 1; i < modelEditNodes.size(); i++)
        transforms.push_back(modelEditWorld(i));
      ftebroom::modelEdit::bake(mesh, transforms);
    }
    auto p = bb::importMesh(mesh, std::filesystem::u8path(bbNativeSource).stem().u8string());
    auto bytes = modelResource(bbNativeSource);
    for (size_t i = 0; i < mesh.surfaces.size(); i++) {
      ftebroom::modelExport::Skin skin;
      if (bytes.substr(0, 4) == "IDPO") {
        auto mdl = ftebroom::readMdl(bytes, modelViewerSkin);
        skin = modelImportDecode(ftebroom::quake1::tga(mdl.skin, modelResource("gfx/palette.lmp")));
      } else if (bytes.substr(0, 4) == "IDST" &&
                 mesh.surfaces[i].material.rfind("__hl_texture_", 0) == 0)
        skin = modelImportHalfLifeSkin(bytes, std::stoul(mesh.surfaces[i].material.substr(13)),
                                       bbNativeSource);
      else
        skin = modelImportDecode(modelImportTexture(mesh.surfaces[i].material));
      bb::Texture t;
      t.id = p.root["textures"][i]["uuid"];
      t.name = "skin" + std::to_string(i) + ".png";
      t.width = skin.width;
      t.height = skin.height;
      t.uvWidth = t.uvHeight = 64;
      t.rgba = std::move(skin.rgba);
      p.textures.push_back(std::move(t));
    }
    auto path = bbPath(bbNativeSource);
    path.replace_extension(".bbmodel");
    p.path = path.u8string();
    p.warnings.push_back("Vertex animation is not converted; only the first pose is imported");
    bbActivate(std::move(p));
    bbSavedStamp = ~uint64_t(0);
  } catch (const std::exception &e) {
    bbNotice(e);
  }
}
static std::future<std::string> bbBridgeJob;
static std::filesystem::path bbBridgeStage;
static std::string bbBridgeSource;
static void bbImportedMesh(ftebroom::modelExport::Mesh mesh, const std::filesystem::path &stage,
                           const std::string &source) {
  auto ext = entityLower(std::filesystem::u8path(source).extension().u8string());
  if (ext == ".gltf" || ext == ".glb")
    for (auto &surface : mesh.surfaces)
      for (auto &frame : surface.frames)
        for (auto &vertex : frame) {
          auto p = vertex.position, n = vertex.normal;
          vertex.position = {p[1], -p[0], p[2]};
          vertex.normal = {n[1], -n[0], n[2]};
        }
  auto p = bb::importMesh(mesh, std::filesystem::u8path(source).stem().u8string());
  for (size_t i = 0; i < mesh.surfaces.size(); i++) {
    bb::Texture t;
    try {
      auto image = stage / std::filesystem::u8path(mesh.surfaces[i].material);
      auto bytes = std::filesystem::is_regular_file(image)
                       ? bb::readFile(image)
                       : modelImportTexture(mesh.surfaces[i].material);
      t = bb::decodeImage(bytes);
    } catch (...) {
      t.width = t.height = 16;
      t.rgba.assign(16 * 16 * 4, 180);
      p.warnings.push_back("Missing imported texture: " + mesh.surfaces[i].material);
    }
    t.id = p.root["textures"][i]["uuid"];
    t.name = mesh.surfaces[i].material;
    t.uvWidth = t.uvHeight = 64;
    p.textures.push_back(t);
  }
  bbActivate(std::move(p));
  bbSavedStamp = ~uint64_t(0);
}
static void bbImportFile(const std::filesystem::path &source) {
  auto ext = entityLower(source.extension().u8string());
  if (ext == ".bbmodel") {
    bbOpen(source.u8string());
    return;
  }
  if (ext == ".obj") {
    auto parent = source.parent_path();
    auto p = bb::importObj(
        modFileBytes(source),
        [parent](const std::string &path) {
          return modFileBytes(parent / std::filesystem::u8path(path));
        },
        source.stem().u8string());
    bbActivate(std::move(p));
    bbSavedStamp = ~uint64_t(0);
    return;
  }
  if (!modelImportSource(source)) {
    bbConvertNative(modFileModelPath(source));
    return;
  }
  if (bbBridgeJob.valid())
    throw std::runtime_error("Wait for the current import");
  modelImportFindBlender();
  char script[4096] = {};
  if (!files->NativePath("model-import-blender.py", (fs_relative)5, script, sizeof script))
    throw std::runtime_error("Blender import helper is missing");
  auto runtime = modelImportRuntimeRoot();
  std::vector<std::string> installer;
  if (!modelImportRuntimeReady()) {
    char download[4096] = {};
#ifdef _WIN32
    if (!files->NativePath("download-blender.ps1", (fs_relative)5, download, sizeof download))
      throw std::runtime_error("Blender downloader is missing");
    installer = {"powershell.exe", "-NoLogo", "-NoProfile",   "-ExecutionPolicy", "Bypass",
                 "-File",          download,  "-Destination", runtime.u8string()};
#else
    if (!files->NativePath("download-blender.py", (fs_relative)5, download, sizeof download))
      throw std::runtime_error("Blender downloader is missing");
    installer = {"python3", download, "--destination", runtime.u8string()};
#endif
    std::filesystem::create_directories(runtime);
    std::ofstream(runtime / "progress.txt") << "0 0 Connecting to blender.org";
  }
  bbBridgeStage = documentPath("data/model-import") / bb::uuid();
  std::filesystem::create_directories(bbBridgeStage);
  bbBridgeSource = source.u8string();
  auto output = bbBridgeStage;
  std::vector<std::string> args = {modelImportRuntime().u8string(),
                                   "--background",
                                   "--factory-startup",
                                   "--disable-autoexec",
                                   "--python",
                                   script,
                                   "--",
                                   "--source",
                                   source.u8string(),
                                   "--output",
                                   output.u8string(),
                                   "--frames",
                                   "1",
                                   "--start",
                                   "0",
                                   "--fps",
                                   "24"};
  bbBridgeJob = std::async(std::launch::async, [installer, runtime, args, output] {
    if (!installer.empty()) {
      auto error = modelImportRun(installer, runtime);
      if (!error.empty())
        return error;
    }
    return modelImportRun(args, output);
  });
  bbMessage = "Importing the first pose; Blender is downloaded automatically if needed";
}
static void bbBridgeTick() {
  if (bbBridgeJob.valid() &&
      bbBridgeJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    try {
      auto error = bbBridgeJob.get();
      if (!error.empty())
        throw std::runtime_error(error);
      auto mesh = ftebroom::modelExport::parse(bb::readFile(bbBridgeStage / "model.mesh"));
      auto stage = bbBridgeStage;
      auto source = bbBridgeSource;
      bbLeave([mesh = std::move(mesh), stage, source]() mutable {
        bbImportedMesh(std::move(mesh), stage, source);
      });
    } catch (const std::exception &e) {
      bbNotice(e);
    }
}
static void bbAction(const std::string &action) {
  try {
    if (action == "Import") {
      bbChoose(false, modFileRoot(), [](auto path) {
        bbLeave([path] {
          try {
            bbImportFile(path);
          } catch (const std::exception &e) {
            bbNotice(e);
          }
        });
      });
      return;
    }
    if (action == "Change animation speed") {
      bbAnimationSpeedOpen = true;
      return;
    }
    if (action == "Merge animations") {
      bbMergeAnimationOpen = true;
      return;
    }
    if (action == "Copy pose") {
      if (bbAnimation >= 0 && size_t(bbAnimation) < bbProject.root["animations"].size()) {
        bbPoseClipboard = bb::Json::object();
        auto &a = bbProject.root["animations"][bbAnimation];
        for (auto id : bbSelection.groups)
          for (auto name : {"position", "rotation", "scale"})
            bbPoseClipboard[id][name] =
                bb::channel(a["animators"].value(id, bb::Json::object()), name, bbPlayTime);
      }
      return;
    }
    if (action == "Paste pose") {
      bbEdit("Paste animation pose", [] {
        if (bbAnimation < 0 || size_t(bbAnimation) >= bbProject.root["animations"].size())
          return;
        for (auto it = bbPoseClipboard.begin(); it != bbPoseClipboard.end(); ++it)
          if (bbProject.find(it.key()))
            for (auto name : {"position", "rotation", "scale"})
              if (it.value().contains(name))
                bb::setKey(bbProject.root["animations"][bbAnimation], it.key(), name, bbPlayTime,
                           bb::vec(it.value()[name]));
      });
      return;
    }
    if (bbTimelineFocused && (action == "Move left" || action == "Move right")) {
      bbAction(action == "Move left" ? "Previous frame" : "Next frame");
      return;
    }
    if (action == "Timeline start") {
      bbPlayTime = 0;
      return;
    }
    if (action == "Timeline end") {
      if (bbAnimation >= 0 && size_t(bbAnimation) < bbProject.root["animations"].size())
        bbPlayTime = bb::animationLength(bbProject.root["animations"][bbAnimation]);
      return;
    }
    if (action == "Previous frame" || action == "Next frame") {
      if (bbAnimation >= 0 && size_t(bbAnimation) < bbProject.root["animations"].size()) {
        int fps = bbProject.root["animations"][bbAnimation].value("snapping", 24);
        bbPlayTime =
            std::max(0.f, bbPlayTime + (action == "Next frame" ? 1.f : -1.f) / std::max(1, fps));
      }
      return;
    }
    if (action == "Key F") {
      bbAction(bbMode == 1 ? "Fill" : "Create Face");
      return;
    }
    if (action == "Key X") {
      if (bbMode == 1) {
        std::swap(bbBrush.color, bbSecondary);
        for (int a = 0; a < 4; a++)
          bbColor[a] = bbBrush.color[a] / 255.f;
      } else
        bbAction("Vertex Snap");
      return;
    }
    if (action == "Key E") {
      bbAction(bbMode == 1 ? "Eraser" : "Extrude");
      return;
    }
    if (action == "Key I") {
      bbAction(bbMode == 1 ? "Color Picker" : "Inset");
      return;
    }
    if (action == "Key M") {
      bbAction(bbMode == 1 ? "Selection" : "Merge by distance");
      return;
    }
    if (bbMode == 2 &&
        (action == "Add Cube" || action == "Add Mesh" || action == "Convert to Mesh" ||
         action == "Extrude" || action == "Inset" || action == "Loop Cut" || action == "Solidify" ||
         action == "Create Face" || action == "Delete" && bbSelection.keyframes.empty()))
      throw std::runtime_error("Switch to Edit mode to change model geometry");
    if (action == "new" || action == "New model") {
      bbNew();
      return;
    }
    if (action == "Undo") {
      bbHistory.undo(bbProject, bbSelection);
      return;
    }
    if (action == "Redo") {
      bbHistory.redo(bbProject, bbSelection);
      return;
    }
    if (action == "Save") {
      if (bbProject.path.empty())
        bbSaveAs();
      else
        bbSave(std::filesystem::u8path(bbProject.path));
      return;
    }
    if (action == "Save as") {
      bbSaveAs();
      return;
    }
    if (action == "Open") {
      bbChoose(false, modFileRoot(), [](auto path) { bbOpen(path.u8string()); });
      return;
    }
    if (action == "Export") {
      bbExportOpen = true;
      return;
    }
    if (action == "Close") {
      bbLeave([] {
        bbActive = false;
        modelViewerOpen = modelEditorActive = false;
        modelEditorCameraEnd();
        cvars->SetFloat("ftebroom_bb_active", 0);
        bbReleaseImages();
        if (auto api = bbAPI())
          api->clear();
      });
      return;
    }
    if (action == "Focus") {
      bbFrameSelection();
      return;
    }
    if (action == "Play") {
      bbPlaying = !bbPlaying;
      return;
    }
    if (action == "Edit" || action == "Paint" || action == "Animate") {
      bbPaintEnd();
      if (bbHistory.active())
        bbHistory.cancel(bbProject, bbSelection);
      bbGizmoAxis = bbUVDrag = -1;
      bbMode = action == "Paint" ? 1 : action == "Animate" ? 2 : 0;
      bbProject.root["editor_state"]["mode"] = action == "Paint"     ? "paint"
                                               : action == "Animate" ? "animate"
                                                                     : "edit";
      bbTool = 0;
      bbProject.revision++;
      return;
    }
    const std::vector<std::string> tools =
        bbMode == 1
            ? std::vector<std::string>{"Brush",      "Eraser",   "Fill",       "Color Picker",
                                       "Draw Shape", "Gradient", "Copy Brush", "Selection"}
            : std::vector<std::string>{"Move",        "Resize",  "Rotate", "Pivot",
                                       "Vertex Snap", "Stretch", "Knife",  "Seam"};
    for (size_t i = 0; i < tools.size(); i++)
      if (action == tools[i]) {
        bbTool = i;
        return;
      }
    if (action == "Select all" || action == "Invert selection") {
      bool invert = action == "Invert selection";
      if (bbSelectionMode == 0) {
        if (!invert)
          bbSelection = {};
        for (auto &e : bbProject.root["elements"]) {
          std::string id = e["uuid"];
          if (bb::locked(bbProject, id))
            continue;
          if (invert && bbSelection.elements.count(id))
            bbSelection.elements.erase(id);
          else
            bbSelection.elements.insert(id);
        }
      } else {
        auto ids = bbSelection.elements;
        if (ids.empty())
          for (auto &e : bbProject.root["elements"])
            ids.insert(e["uuid"]);
        for (auto id : ids)
          if (auto e = bbProject.find(id)) {
            if (bb::locked(bbProject, id) || !e->contains("faces"))
              continue;
            std::set<std::string> all;
            if (bbSelectionMode == 1)
              for (auto it = (*e)["faces"].begin(); it != (*e)["faces"].end(); ++it)
                all.insert(it.key());
            else if (e->value("type", "") == "mesh") {
              if (bbSelectionMode == 3)
                for (auto it = (*e)["vertices"].begin(); it != (*e)["vertices"].end(); ++it)
                  all.insert(it.key());
              else
                for (auto &face : (*e)["faces"]) {
                  auto vertices = face["vertices"].get<std::vector<std::string>>();
                  for (size_t i = 0; i < vertices.size(); i++)
                    all.insert(bb::edgeKey(vertices[i], vertices[(i + 1) % vertices.size()]));
                }
            }
            auto &selected = bbSelectionMode == 1   ? bbSelection.faces[id]
                             : bbSelectionMode == 2 ? bbSelection.edges[id]
                                                    : bbSelection.vertices[id];
            if (invert) {
              auto old = selected;
              selected.clear();
              for (auto key : all)
                if (!old.count(key))
                  selected.insert(key);
            } else
              selected = all;
            if (!all.empty())
              bbSelection.elements.insert(id);
          }
      }
      return;
    }
    if (action == "Object selection" || action == "Face selection" || action == "Edge selection" ||
        action == "Vertex selection") {
      bbSelectionMode = action == "Face selection"     ? 1
                        : action == "Edge selection"   ? 2
                        : action == "Vertex selection" ? 3
                                                       : 0;
      return;
    }
    if (action == "Toggle ortho") {
      bbOrtho = !bbOrtho;
      return;
    }
    if (action == "Front view" || action == "Right view" || action == "Top view") {
      modelEditorCameraEnd();
      modelEditCameraFree = false;
      bbOrtho = true;
      modelViewerYaw = action == "Front view" ? 180 : action == "Right view" ? 90 : 0;
      modelViewerPitch = action == "Top view" ? 89.9f : 0;
      if (ImGui::GetIO().KeyCtrl) {
        modelViewerYaw += 180;
        modelViewerPitch = -modelViewerPitch;
      }
      return;
    }
    if (bbMode == 1 && bbTexture >= 0 && size_t(bbTexture) < bbProject.textures.size() &&
        (action == "Copy" || action == "Cut" || action == "Paste" || action == "Delete" ||
         action == "Crop selection" || action == "Move selection")) {
      auto &tex = bbProject.textures[bbTexture];
      auto rect = bbPaintSelection ? bbPaintRect : std::array<int, 4>{0, 0, tex.width, tex.height};
      if (action == "Copy" || action == "Cut")
        bbPixelClipboard = bb::cropImage(tex, rect);
      if (action != "Copy")
        bbEdit(
            action.c_str(),
            [&] {
              if (action == "Paste") {
                if (bbPixelClipboard.rgba.empty())
                  throw std::runtime_error("Copy a pixel selection first");
                bb::pasteImage(tex, bbPixelClipboard, rect[0], rect[1], bbPaintOptions(bbTexture));
              } else if (action == "Crop selection") {
                auto cropped = bb::cropImage(tex, rect);
                double dx = rect[0] * double(tex.uvWidth) / tex.width,
                       dy = rect[1] * double(tex.uvHeight) / tex.height;
                for (auto &e : bbProject.root["elements"])
                  if (e.contains("faces"))
                    for (auto it = e["faces"].begin(); it != e["faces"].end(); ++it)
                      if (bb::textureIndex(it.value()) == bbTexture)
                        bb::uvTransform(e, {it.key()}, {-dx, -dy});
                cropped.uvWidth = std::max(1, int(tex.uvWidth * double(cropped.width) / tex.width));
                cropped.uvHeight =
                    std::max(1, int(tex.uvHeight * double(cropped.height) / tex.height));
                tex = cropped;
                bbPaintSelection = false;
              } else {
                for (int y = std::max(0, rect[1]); y < std::min(tex.height, rect[3]); y++)
                  for (int x = std::max(0, rect[0]); x < std::min(tex.width, rect[2]); x++)
                    tex.rgba[(size_t(y) * tex.width + x) * 4 + 3] = 0;
                tex.revision++;
                tex.saved = false;
              }
            },
            {size_t(bbTexture)});
      return;
    }
    if (action == "Adjust colors...") {
      bbAdjustOpen = true;
      return;
    }
    if (action == "Clear unused pixels") {
      bbEdit("Clear unused pixels", [] { bb::clearUnusedPixels(bbProject, bbTexture); },
             {size_t(bbTexture)});
      return;
    }
    if (action == "Copy" || action == "Cut") {
      if (bbMode == 2 && !bbSelection.keyframes.empty()) {
        bbClipboardData = bb::Json::array();
        for (auto &track : bbProject.root["animations"][bbAnimation]["animators"])
          for (auto &k : track["keyframes"])
            if (bbSelection.keyframes.count(k.value("uuid", "")))
              bbClipboardData.push_back(k);
      } else {
        auto snapshot = bbProject;
        auto keep = bb::descendants(snapshot, bb::selectedTop(snapshot, bbSelection));
        bb::Selection remove;
        for (auto &e : snapshot.root["elements"])
          if (!keep.count(e.value("uuid", "")))
            remove.elements.insert(e["uuid"]);
        for (auto &g : snapshot.root["groups"])
          if (!keep.count(g.value("uuid", "")))
            remove.groups.insert(g["uuid"]);
        auto par = bb::parents(snapshot);
        for (auto &id : keep)
          if (!keep.count(par[id])) {
            auto n = snapshot.find(id);
            if (n)
              bb::reparent(snapshot, {id}, "");
          }
        bb::eraseSelection(snapshot, remove);
        bbClipboard = bb::serialize(snapshot);
        ImGui::SetClipboardText(bbClipboard.c_str());
      }
      if (action == "Cut")
        bbAction("Delete");
      return;
    }
    if (action == "Paste") {
      if (bbMode == 2 && bbClipboardData.is_array() && !bbSelection.groups.empty() &&
          bbAnimation >= 0) {
        bbEdit("Paste keyframes", [] {
          auto &a = bbProject.root["animations"][bbAnimation];
          for (auto id : bbSelection.groups)
            for (auto key : bbClipboardData) {
              key["uuid"] = bb::uuid();
              key["time"] = key.value("time", 0.f) + bbPlayTime;
              auto &track = a["animators"][id];
              if (track.is_null())
                track = {{"type", "bone"}, {"keyframes", bb::Json::array()}};
              track["keyframes"].push_back(key);
            }
        });
      } else {
        auto text = ImGui::GetClipboardText();
        auto pasted = bb::parseProject(text && *text ? text : bbClipboard);
        bbEdit("Paste elements", [&] {
          auto other = pasted;
          bb::Selection all;
          for (auto &e : other.root["elements"])
            all.elements.insert(e["uuid"]);
          for (auto &g : other.root["groups"])
            all.groups.insert(g["uuid"]);
          bb::duplicate(other, all);
          auto ids = bb::descendants(other, bb::selectedTop(other, all));
          std::vector<int> textureMap;
          for (auto &texture : other.textures) {
            int found = -1;
            for (size_t i = 0; i < bbProject.textures.size(); i++) {
              auto &existing = bbProject.textures[i];
              if (existing.id == texture.id && existing.width == texture.width &&
                  existing.height == texture.height && existing.rgba == texture.rgba) {
                found = i;
                break;
              }
            }
            if (found < 0) {
              auto copy = texture;
              copy.id = bb::uuid();
              found = bbProject.textures.size();
              bb::addTexture(bbProject, copy);
            }
            textureMap.push_back(found);
          }
          for (auto key : {"elements", "groups"})
            for (auto e : other.root[key])
              if (ids.count(e.value("uuid", ""))) {
                if (e.contains("faces"))
                  for (auto &f : e["faces"])
                    if (f["texture"].is_number_integer())
                      f["texture"] =
                          f["texture"].get<int>() < 0 ? -1 : textureMap.at(f["texture"].get<int>());
                bbProject.root[key].push_back(e);
              }
          for (auto &n : other.root["outliner"]) {
            auto id = n.is_string() ? n.get<std::string>() : n.value("uuid", "");
            if (ids.count(id))
              bbProject.root["outliner"].push_back(n);
          }
          bbSelection = all;
        });
      }
      return;
    }
    if (action == "Add Mesh") {
      bbPrimitiveOpen = true;
      return;
    }
    if (action == "Resize texture...") {
      if (bbTexture >= 0 && size_t(bbTexture) < bbProject.textures.size()) {
        bbResizeW = bbProject.textures[bbTexture].width;
        bbResizeH = bbProject.textures[bbTexture].height;
        bbResizeOpen = true;
      }
      return;
    }
    if (action == "Import palette") {
      bbChoose(false, modFileRoot(), [](auto path) {
        try {
          auto bytes = modFileBytes(path);
          std::vector<bb::Pixel> colors;
          if (path.extension() == ".lmp") {
            if (bytes.size() != 768)
              throw std::runtime_error("Palette LMP must contain 768 bytes");
            for (int i = 0; i < 256; i++)
              colors.push_back({(unsigned char)bytes[i * 3], (unsigned char)bytes[i * 3 + 1],
                                (unsigned char)bytes[i * 3 + 2], 255});
          } else {
            std::istringstream lines(bytes);
            std::string line;
            while (std::getline(lines, line) && colors.size() < 256) {
              if (line.empty() || line[0] == '#') {
                if (line.size() == 7 &&
                    std::all_of(line.begin() + 1, line.end(),
                                [](unsigned char c) { return std::isxdigit(c); })) {
                  unsigned c = std::stoul(line.substr(1), nullptr, 16);
                  colors.push_back(
                      {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c, 255});
                }
                continue;
              }
              if (line.size() == 6 && std::all_of(line.begin(), line.end(), [](unsigned char c) {
                    return std::isxdigit(c);
                  })) {
                unsigned c = std::stoul(line, nullptr, 16);
                colors.push_back(
                    {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c, 255});
                continue;
              }
              int r, g, b;
              std::istringstream in(line);
              if (in >> r >> g >> b)
                colors.push_back({(unsigned char)r, (unsigned char)g, (unsigned char)b, 255});
            }
          }
          if (colors.empty())
            throw std::runtime_error("No palette colors found");
          bbPalette = colors;
        } catch (const std::exception &e) {
          bbNotice(e);
        }
      });
      return;
    }
    if (action == "Export palette") {
      bbChoose(true, modFileRoot() / "palette.gpl", [](auto path) {
        try {
          modFileValidate(path);
          std::ostringstream s;
          s << "GIMP Palette\nName: ReyondBroom\n#\n";
          for (auto c : bbPalette)
            s << int(c[0]) << ' ' << int(c[1]) << ' ' << int(c[2]) << '\n';
          auto ext = entityLower(path.extension().u8string());
          if (ext == ".lmp") {
            std::string bytes;
            for (int i = 0; i < 256; i++) {
              auto c = bbPalette.empty() ? bb::Pixel{}
                                         : bbPalette[std::min(size_t(i), bbPalette.size() - 1)];
              for (int a = 0; a < 3; a++)
                bytes += char(c[a]);
            }
            bb::atomicWrite(path, bytes);
          } else if (ext == ".hex") {
            std::ostringstream hex;
            hex << std::hex << std::setfill('0');
            for (auto c : bbPalette)
              hex << '#' << std::setw(2) << int(c[0]) << std::setw(2) << int(c[1]) << std::setw(2)
                  << int(c[2]) << '\n';
            bb::atomicWrite(path, hex.str());
          } else
            bb::atomicWrite(path, s.str());
        } catch (const std::exception &e) {
          bbNotice(e);
        }
      });
      return;
    }
    if (action == "Add keyframe") {
      if (bbAnimation < 0 || size_t(bbAnimation) >= bbProject.root["animations"].size())
        throw std::runtime_error("Create an animation first");
      bbEdit("Add keyframe", [] {
        for (auto id : bbSelection.groups) {
          auto &a = bbProject.root["animations"][bbAnimation];
          std::string name = bbKeyChannel == 1   ? "position"
                             : bbKeyChannel == 2 ? "scale"
                                                 : "rotation";
          bb::setKey(a, id, name, bbPlayTime,
                     bb::channel(a["animators"].value(id, bb::Json::object()), name, bbPlayTime));
        }
      });
      return;
    }
    if (action == "Rename") {
      if (auto item = bbProject.find(bbActiveId())) {
        std::snprintf(bbRenameText, sizeof bbRenameText, "%s",
                      item->value("name", "object").c_str());
        bbRenameOpen = true;
      }
      return;
    }
    if (action != "Lock" && action != "Unhide all")
      for (auto id : bbIds())
        if (bb::locked(bbProject, id))
          throw std::runtime_error("Unlock the selected element or its parent group first");
    bbEdit(action.c_str(), [&] {
      if (action == "Add Cube") {
        auto c = bb::cube("cube", {0, 0, 0}, {bbDefaultCube, bbDefaultCube, bbDefaultCube});
        c["box_uv"] = bbProject.root["meta"].value("box_uv", false);
        if (!bbProject.textures.empty())
          for (auto &f : c["faces"])
            f["texture"] = bbTexture >= 0 ? bbTexture : 0;
        bbProject.root["elements"].push_back(c);
        bb::appendNode(bbProject, c["uuid"], bbSelection.activeGroup);
        bbSelect(c["uuid"]);
      } else if (action == "Add Group") {
        if (bbIds().empty()) {
          auto g = bb::group();
          bbProject.root["groups"].push_back(g);
          bb::appendNode(bbProject, g["uuid"], bbSelection.activeGroup, true);
          bbSelect(g["uuid"]);
        } else
          bb::groupSelection(bbProject, bbSelection);
      } else if (action == "Add Locator") {
        bb::Json e = {{"type", "locator"},
                      {"uuid", bb::uuid()},
                      {"name", "locator"},
                      {"position", {0, 0, 0}},
                      {"rotation", {0, 0, 0}}};
        bbProject.root["elements"].push_back(e);
        bb::appendNode(bbProject, e["uuid"], bbSelection.activeGroup);
        bbSelect(e["uuid"]);
      } else if (action == "Bake current pose") {
        if (bbAnimation < 0 || size_t(bbAnimation) >= bbProject.root["animations"].size())
          throw std::runtime_error("Choose an animation");
        auto ts =
            bb::triangles(bbProject, false,
                          bb::animationPose(bbProject.root["animations"][bbAnimation], bbPlayTime));
        std::map<std::string, bb::Json> meshes;
        for (auto &t : ts) {
          auto &mesh = meshes[t.element];
          if (mesh.is_null())
            mesh = {{"type", "mesh"},
                    {"uuid", t.element},
                    {"name", bbProject.find(t.element)->value("name", "mesh")},
                    {"origin", {0, 0, 0}},
                    {"rotation", {0, 0, 0}},
                    {"vertices", bb::Json::object()},
                    {"faces", bb::Json::object()}};
          std::vector<std::string> keys;
          bb::Json uv = bb::Json::object();
          for (int i = 0; i < 3; i++) {
            auto k = "v" + std::to_string(mesh["vertices"].size());
            mesh["vertices"][k] = t.p[i];
            keys.push_back(k);
            uv[k] = t.uv[i];
          }
          mesh["faces"]["f" + std::to_string(mesh["faces"].size())] = {
              {"vertices", keys},
              {"uv", uv},
              {"texture", t.texture < 0 ? bb::Json(false) : bb::Json(t.texture)}};
        }
        bbProject.root["elements"] = bb::Json::array();
        bbProject.root["outliner"] = bb::Json::array();
        bbProject.root["groups"] = bb::Json::array();
        bbProject.root["animations"] = bb::Json::array();
        for (auto &[id, mesh] : meshes) {
          bbProject.root["elements"].push_back(mesh);
          bb::appendNode(bbProject, id);
        }
        bbSelection = {};
        bbMode = 0;
        bbPlaying = false;
        bbMessage = "Current pose baked to static meshes. Undo restores the rig and animations.";
      } else if (action == "Duplicate")
        bb::duplicate(bbProject, bbSelection);
      else if (action == "Delete") {
        if (bbMode == 2 && !bbSelection.keyframes.empty() && bbAnimation >= 0) {
          for (auto &track : bbProject.root["animations"][bbAnimation]["animators"])
            for (size_t i = track["keyframes"].size(); i-- > 0;)
              if (bbSelection.keyframes.count(track["keyframes"][i].value("uuid", "")))
                track["keyframes"].erase(i);
          bbSelection.keyframes.clear();
        } else if (bbSelectionMode == 3 && !bbSelection.vertices.empty()) {
          for (auto &[id, keys] : bbSelection.vertices)
            if (auto e = bbProject.find(id)) {
              for (auto &key : keys)
                (*e)["vertices"].erase(key);
              std::vector<std::string> remove;
              for (auto it = (*e)["faces"].begin(); it != (*e)["faces"].end(); ++it)
                for (auto &k : it.value()["vertices"])
                  if (keys.count(k.get<std::string>())) {
                    remove.push_back(it.key());
                    break;
                  }
              for (auto &key : remove)
                (*e)["faces"].erase(key);
            }
        } else if (bbSelectionMode == 1 && !bbSelection.faces.empty()) {
          for (auto &[id, keys] : bbSelection.faces)
            if (auto e = bbProject.find(id))
              for (auto &key : keys) {
                if (e->value("type", "") == "cube")
                  (*e)["faces"][key]["texture"] = nullptr;
                else
                  (*e)["faces"].erase(key);
              }
        } else
          bb::deleteComponents(bbProject, bbSelection, bbSelectionMode);
      } else if (action == "Unhide all") {
        for (auto key : {"elements", "groups"})
          for (auto &e : bbProject.root[key])
            e["visibility"] = true;
      } else if (action == "Resolve group") {
        auto parents = bb::parents(bbProject);
        auto groups = bbSelection.groups;
        for (auto id : groups) {
          std::set<std::string> children;
          for (auto [child, parent] : parents)
            if (parent == id)
              children.insert(child);
          bb::reparent(bbProject, children, parents[id]);
          bb::Selection remove;
          remove.groups.insert(id);
          bb::eraseSelection(bbProject, remove);
        }
        bbSelection = {};
      } else if (action == "Sort") {
        std::function<void(bb::Json &)> sort = [&](bb::Json &nodes) {
          std::stable_sort(nodes.begin(), nodes.end(), [](auto &a, auto &b) {
            auto x = bbProject.find(a.is_string() ? a.template get<std::string>()
                                                  : a.value("uuid", "")),
                 y = bbProject.find(b.is_string() ? b.template get<std::string>()
                                                  : b.value("uuid", ""));
            return x && y && x->value("name", "") < y->value("name", "");
          });
          for (auto &n : nodes)
            if (n.is_object())
              sort(n["children"]);
        };
        sort(bbProject.root["outliner"]);
      } else if (action == "Move left" || action == "Move right" || action == "Move up" ||
                 action == "Move down" || action == "Move forth" || action == "Move back") {
        bb::Vec3 d{};
        int axis = action == "Move left" || action == "Move right" ? 0
                   : action == "Move up" || action == "Move down"  ? 1
                                                                   : 2;
        d[axis] =
            (action == "Move left" || action == "Move down" || action == "Move back" ? -1 : 1) *
            bbGrid;
        bb::moveWorld(bbProject, bbTransformSelection(), d);
      } else if (action == "Round values" || action == "Reverse keyframes" ||
                 action == "Reset keyframe") {
        if (bbAnimation >= 0) {
          auto &a = bbProject.root["animations"][bbAnimation];
          double length = bb::animationLength(a);
          for (auto &track : a["animators"])
            for (auto &k : track["keyframes"])
              if (bbSelection.keyframes.empty() ||
                  bbSelection.keyframes.count(k.value("uuid", ""))) {
                if (action == "Reverse keyframes")
                  k["time"] = length - k.value("time", 0.0);
                else {
                  auto v =
                      action == "Reset keyframe"
                          ? (k.value("channel", "") == "scale" ? bb::Vec3{1, 1, 1} : bb::Vec3{})
                          : bb::keyValue(k);
                  if (action == "Round values")
                    for (auto &value : v)
                      value = std::round(value);
                  auto copy = bb::keyframe(k.value("channel", "rotation"), k.value("time", 0.0), v);
                  k["data_points"] = copy["data_points"];
                }
              }
        }
      } else if (action == "Optimize animation") {
        if (bbAnimation >= 0)
          for (auto &track : bbProject.root["animations"][bbAnimation]["animators"]) {
            auto &keys = track["keyframes"];
            for (size_t i = keys.size(); i-- > 1;)
              if (i + 1 < keys.size() &&
                  keys[i].value("channel", "") == keys[i - 1].value("channel", "") &&
                  keys[i].value("channel", "") == keys[i + 1].value("channel", "") &&
                  bb::length(bb::sub(bb::keyValue(keys[i]), bb::keyValue(keys[i - 1]))) < .01 &&
                  bb::length(bb::sub(bb::keyValue(keys[i]), bb::keyValue(keys[i + 1]))) < .01)
                keys.erase(i);
          }
      } else {
        auto ids = bbIds();
        for (auto &id : ids) {
          auto e = bbProject.find(id);
          if (!e || (action != "Lock" && bb::locked(bbProject, id)))
            continue;
          auto type = e->value("type", "");
          if (action == "Hide")
            (*e)["visibility"] = false;
          else if (action == "Lock")
            (*e)["locked"] = !e->value("locked", false);
          else if (action == "Toggle export")
            (*e)["export"] = !e->value("export", true);
          else if (action == "Convert to Mesh" && type == "cube")
            *e = bb::toMesh(*e);
          else if (action == "Origin to Geometry" || action == "Center Individual Pivots") {
            bb::Vec3 center{};
            if (type == "cube")
              center = bb::mul(bb::add(bb::getv(*e, "from"), bb::getv(*e, "to")), .5);
            else if (type == "mesh") {
              for (auto &v : (*e)["vertices"])
                center = bb::add(center, bb::vec(v));
              center = bb::add(bb::getv(*e, "origin"),
                               bb::mul(center, 1. / std::max(size_t(1), (*e)["vertices"].size())));
            }
            bb::pivotObject(bbProject, id, center);
          } else if (action.rfind("Rotate ", 0) == 0 && action.find("90") != std::string::npos) {
            int axis = action.find('X') != std::string::npos   ? 0
                       : action.find('Y') != std::string::npos ? 1
                                                               : 2;
            auto r = bb::getv(*e, "rotation");
            r[axis] += action.find("CCW") != std::string::npos ? -90 : 90;
            (*e)["rotation"] = r;
          } else if (action.rfind("Center ", 0) == 0) {
            int axis = action.back() == 'X' ? 0 : action.back() == 'Y' ? 1 : 2;
            auto from = bb::getv(*e, type == "cube" ? "from" : "origin"),
                 to = type == "cube" ? bb::getv(*e, "to") : from;
            bb::Vec3 d{};
            d[axis] = -(from[axis] + to[axis]) * .5;
            bb::translateObject(*e, d);
          } else if (action.rfind("Flip ", 0) == 0) {
            int axis = action.back() == 'X' ? 0 : action.back() == 'Y' ? 1 : 2;
            if (type == "cube") {
              auto f = bb::getv(*e, "from"), t = bb::getv(*e, "to");
              f[axis] = -f[axis];
              t[axis] = -t[axis];
              (*e)["from"] = f;
              (*e)["to"] = t;
              bb::normalizeCube(*e);
            } else if (type == "mesh") {
              for (auto &v : (*e)["vertices"]) {
                auto p = bb::vec(v);
                p[axis] *= -1;
                v = p;
              }
              for (auto &f : (*e)["faces"])
                std::reverse(f["vertices"].begin(), f["vertices"].end());
            }
          } else if (action == "Apply Rotation") {
            if (type == "cube")
              *e = bb::toMesh(*e);
            else if (type == "mesh") {
              auto m = bb::rotation(bb::getv(*e, "rotation"));
              for (auto &v : (*e)["vertices"])
                v = m.point(bb::vec(v));
              (*e)["rotation"] = {0, 0, 0};
            }
          } else if (type == "mesh" && (action == "Extrude" || action == "Inset"))
            bb::extrude(*e, bbSelection.faces[id], action == "Inset" ? .2 : 1, action == "Inset");
          else if (type == "mesh" && action == "Unwrap") {
            auto islands = bb::unfold(*e);
            bbMessage = "Unfolded " + std::to_string(islands) +
                        " UV islands; use seams to open closed surfaces";
          } else if (type == "mesh" && action == "Loop Cut")
            bb::loopCut(*e);
          else if (type == "mesh" && action == "Create Face")
            bb::createFace(*e, bbSelection.vertices[id]);
          else if (action == "Invert Face") {
            for (auto &k : bbSelection.faces[id]) {
              auto &f = (*e)["faces"][k];
              if (f.contains("vertices"))
                std::reverse(f["vertices"].begin(), f["vertices"].end());
            }
          } else if (type == "mesh" && (action == "Merge by distance" ||
                                        action == "Merge to center" || action == "Merge to last"))
            bb::mergeVertices(*e, bbSelection.vertices[id],
                              action == "Merge by distance" ? 0
                              : action == "Merge to center" ? 1
                                                            : 2,
                              .1, bbLastVertex);
          else if (type == "mesh" && action == "Split")
            bb::splitFaces(*e, bbSelection.faces[id]);
          else if (type == "mesh" && action == "Dissolve Edges")
            bb::dissolveEdges(*e, bbSelection.edges[id]);
          else if (type == "mesh" && action == "Solidify")
            bb::solidify(*e, 1);
          else if (action == "Remove blank faces") {
            std::vector<std::string> remove;
            for (auto it = (*e)["faces"].begin(); it != (*e)["faces"].end(); ++it)
              if (bb::textureIndex(it.value()) < 0)
                remove.push_back(it.key());
            for (auto &key : remove) {
              if (type == "cube")
                (*e)["faces"][key]["texture"] = nullptr;
              else
                (*e)["faces"].erase(key);
            }
          } else if (action == "Project from View" && type == "mesh") {
            for (auto &f : (*e)["faces"])
              for (auto &k : f["vertices"]) {
                ImVec2 point;
                if (bbScreen(bb::add(bb::vec((*e)["vertices"][k.get<std::string>()]),
                                     bb::getv(*e, "origin")),
                             point))
                  f["uv"][k.get<std::string>()] = {
                      (point.x - bbViewportMin.x) / std::max(1.f, bbViewportSize.x) *
                          bbProject.root["resolution"]["width"].get<int>(),
                      (point.y - bbViewportMin.y) / std::max(1.f, bbViewportSize.y) *
                          bbProject.root["resolution"]["height"].get<int>()};
              }
          } else if (e->contains("faces")) {
            auto tex = bbProject.texture(bbTexture);
            bb::uvAction(*e, bbSelection.faces[id], action, tex ? tex->uvWidth : 64,
                         tex ? tex->uvHeight : 64, tex);
          }
        }
      }
    });
  } catch (const std::exception &e) {
    bbNotice(e);
  }
}
static void bbMenus() {
  if (!ImGui::BeginMenuBar())
    return;
  if (ImGui::BeginMenu("File")) {
    for (auto name : {"New model", "Open", "Import", "Save", "Save as", "Export", "Close"})
      if (ImGui::MenuItem(name))
        bbAction(name);
    if (ImGui::MenuItem("Project settings..."))
      bbProjectSettingsOpen = true;
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Edit")) {
    for (auto name : {"Undo", "Redo", "Copy", "Cut", "Paste", "Duplicate", "Delete", "Select all",
                      "Invert selection"})
      if (ImGui::MenuItem(name))
        bbAction(name);
    if (ImGui::MenuItem("Settings..."))
      bbSettingsOpen = true;
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Transform")) {
    for (auto name : {"Add Cube",
                      "Add Group",
                      "Add Mesh",
                      "Add Locator",
                      "Rotate X 90 CW",
                      "Rotate X 90 CCW",
                      "Rotate Y 90 CW",
                      "Rotate Y 90 CCW",
                      "Rotate Z 90 CW",
                      "Rotate Z 90 CCW",
                      "Flip X",
                      "Flip Y",
                      "Flip Z",
                      "Center X",
                      "Center Y",
                      "Center Z",
                      "Origin to Geometry",
                      "Center Individual Pivots",
                      "Hide",
                      "Unhide all",
                      "Lock",
                      "Toggle export",
                      "Remove blank faces",
                      "Auto UV"})
      if (ImGui::MenuItem(name))
        bbAction(name);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Mesh")) {
    ImGui::BeginDisabled(bbMode == 2);
    for (auto name : {"Convert to Mesh", "Extrude", "Inset", "Loop Cut", "Create Face",
                      "Invert Face", "Merge by distance", "Merge to center", "Merge to last",
                      "Split", "Dissolve Edges", "Apply Rotation", "Solidify"})
      if (ImGui::MenuItem(name))
        bbAction(name);
    ImGui::EndDisabled();
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Texture")) {
    if (ImGui::MenuItem("Create texture template..."))
      bbTemplateOpen = true;
    for (auto name : {"Adjust colors...",
                      "Clear unused pixels",
                      "Crop selection",
                      "Maximize",
                      "Auto UV",
                      "Relative Auto UV",
                      "Project from View",
                      "Mirror X",
                      "Mirror Y",
                      "Rotate Left",
                      "Rotate Right",
                      "Turn Mapping",
                      "Reset",
                      "Apply to All Faces",
                      "Transparent",
                      "Snap UV to pixels",
                      "Merge UV vertices",
                      "Connect UV faces",
                      "Unwrap",
                      "Cycle"})
      if (ImGui::MenuItem(name))
        bbAction(name);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Animation")) {
    for (auto name : {"Add keyframe", "Reset keyframe", "Reverse keyframes", "Round values",
                      "Optimize animation", "Change animation speed", "Merge animations",
                      "Copy pose", "Paste pose", "Bake current pose"})
      if (ImGui::MenuItem(name))
        bbAction(name);
    if (ImGui::MenuItem("Copy frame list"))
      ImGui::SetClipboardText(bb::frameList(bbProject).c_str());
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("View")) {
    const char *names[] = {"Textured", "Solid", "Wireframe", "UV check"};
    for (int i = 0; i < 4; i++)
      if (ImGui::MenuItem(names[i], nullptr, bbDisplay == i)) {
        bbDisplay = i;
        bbProject.revision++;
      }
    ImGui::MenuItem("View from player perspective", nullptr, &modelEditPlayer);
    ImGui::MenuItem("View bones", nullptr, &bbShowBones);
    ImGui::MenuItem("Show grid 16", nullptr, &modelEditFloor);
    ImGui::MenuItem("Origin axes", nullptr, &modelEditOriginAxes);
    ImGui::MenuItem("Player silhouette", nullptr, &bbSilhouette);
    ImGui::MenuItem("Backface culling", nullptr, &modelEditCull);
    ImGui::MenuItem("Orthographic", nullptr, &bbOrtho);
    if (ImGui::MenuItem("Focus selection"))
      bbFrameSelection();
    if (ImGui::MenuItem("Save camera view..."))
      bbViewSaveOpen = true;
    if (ImGui::BeginMenu("Saved views")) {
      auto saved = bbProject.root["editor_state"].value("views", bb::Json::array());
      for (size_t i = 0; i < saved.size(); i++)
        if (ImGui::MenuItem(saved[i].value("name", "View").c_str())) {
          auto &v = saved[i];
          modelEditorCameraEnd();
          modelViewerYaw = v.value("yaw", 135.f);
          modelViewerPitch = v.value("pitch", 15.f);
          modelViewerZoom = v.value("zoom", 1.f);
          bbOrtho = v.value("ortho", false);
          modelEditCameraFree = v.value("free", false);
          auto eye = bb::getv(v, "eye");
          modelEditCameraEye = {float(eye[0]), float(eye[1]), float(eye[2])};
        }
      if (ImGui::MenuItem("Clear saved views"))
        bbEdit("Clear saved views",
               [] { bbProject.root["editor_state"]["views"] = bb::Json::array(); });
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Reset layout"))
      ImGui::DockBuilderRemoveNode(ImGui::GetID("Model authoring dockspace"));
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("Model authoring guide"))
      commands->AddText(
          "url https://github.com/reyond9079/ReyondBroom/blob/main/docs/model-authoring.md\n",
          qfalse);
    ImGui::TextUnformatted(
        "RMB + WASD/QX: fly | V/T/R/P: tools | N: vertex snap\n1/2/3/4: object/face/edge/vertex | "
        "Ctrl+Z/Y: model history\nCtrl+1/2/3: Edit/Paint/Animate | .: focus | Space: play");
    ImGui::EndMenu();
  }
  ImGui::Separator();
  for (int i = 0; i < 3; i++) {
    if (i)
      ImGui::SameLine();
    if (ImGui::MenuItem(i == 0 ? "Edit" : i == 1 ? "Paint" : "Animate", nullptr, bbMode == i))
      bbAction(i == 0 ? "Edit" : i == 1 ? "Paint" : "Animate");
  }
  ImGui::EndMenuBar();
}
static void bbDialogs() {
  if (bbRenameOpen)
    ImGui::OpenPopup("Rename model element");
  if (ImGui::BeginPopupModal("Rename model element", &bbRenameOpen,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    bool enter = ImGui::InputText("Name", bbRenameText, sizeof bbRenameText,
                                  ImGuiInputTextFlags_EnterReturnsTrue);
    if ((ImGui::Button("Rename") || enter) && *bbRenameText) {
      bbEdit("Rename element", [] {
        for (auto id : bbIds())
          if (auto item = bbProject.find(id))
            (*item)["name"] = bbRenameText;
      });
      bbRenameOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbRenameOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (bbViewSaveOpen)
    ImGui::OpenPopup("Save model camera view");
  if (ImGui::BeginPopupModal("Save model camera view", &bbViewSaveOpen,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Name", bbViewName, sizeof bbViewName);
    if (ImGui::Button("Save view")) {
      bbEdit("Save camera view", [] {
        auto &views = bbProject.root["editor_state"]["views"];
        if (views.is_null())
          views = bb::Json::array();
        if (views.size() >= 32)
          throw std::runtime_error("At most 32 saved views");
        views.push_back(
            {{"name", bbViewName},
             {"yaw", modelViewerYaw},
             {"pitch", modelViewerPitch},
             {"zoom", modelViewerZoom},
             {"ortho", bbOrtho},
             {"free", modelEditCameraFree},
             {"eye", {modelEditCameraEye[0], modelEditCameraEye[1], modelEditCameraEye[2]}}});
      });
      bbViewSaveOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbViewSaveOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (bbAnimationSpeedOpen)
    ImGui::OpenPopup("Animation speed");
  if (ImGui::BeginPopupModal("Animation speed", &bbAnimationSpeedOpen,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputFloat("Speed factor", &bbAnimationSpeedFactor);
    ImGui::TextUnformatted("2 = twice as fast, 0.5 = twice as slow");
    if (ImGui::Button("Apply")) {
      bbEdit("Change animation speed", [] {
        if (bbAnimation < 0 || size_t(bbAnimation) >= bbProject.root["animations"].size() ||
            bbAnimationSpeedFactor <= 0)
          throw std::runtime_error("Choose an animation and a positive speed");
        auto &a = bbProject.root["animations"][bbAnimation];
        double rate = std::clamp(bbAnimationSpeedFactor, .01f, 100.f);
        a["length"] = bb::animationLength(a) / rate;
        for (auto &track : a["animators"])
          for (auto &key : track["keyframes"]) {
            key["time"] = key.value("time", 0.) / rate;
            for (auto field : {"bezier_left_time", "bezier_right_time"})
              if (key.contains(field))
                for (auto &value : key[field])
                  value = value.get<double>() / rate;
          }
      });
      bbAnimationSpeedOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbAnimationSpeedOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (bbMergeAnimationOpen)
    ImGui::OpenPopup("Merge animations");
  if (ImGui::BeginPopupModal("Merge animations", &bbMergeAnimationOpen,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    for (size_t i = 0; i < bbProject.root["animations"].size(); i++)
      if (int(i) != bbAnimation &&
          ImGui::Selectable(bbProject.root["animations"][i].value("name", "animation").c_str(),
                            bbMergeAnimation == int(i)))
        bbMergeAnimation = i;
    if (ImGui::Button("Append to current animation")) {
      bbEdit("Merge animations", [] {
        if (bbMergeAnimation == bbAnimation || bbAnimation < 0 ||
            size_t(bbAnimation) >= bbProject.root["animations"].size())
          throw std::runtime_error("Choose a different source animation");
        auto source = bbProject.root["animations"][bbMergeAnimation];
        auto &target = bbProject.root["animations"][bbAnimation];
        double offset = bb::animationLength(target);
        for (auto it = source["animators"].begin(); it != source["animators"].end(); ++it) {
          auto &track = target["animators"][it.key()];
          if (track.is_null())
            track = {{"type", "bone"}, {"keyframes", bb::Json::array()}};
          for (auto key : it.value()["keyframes"]) {
            key["uuid"] = bb::uuid();
            key["time"] = key.value("time", 0.) + offset;
            track["keyframes"].push_back(key);
          }
        }
        target["length"] = offset + bb::animationLength(source);
      });
      bbMergeAnimationOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbMergeAnimationOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (bbAdjustOpen)
    ImGui::OpenPopup("Adjust texture colors");
  if (ImGui::BeginPopupModal("Adjust texture colors", &bbAdjustOpen,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::SliderFloat("Brightness", &bbAdjust[0], -1, 1);
    ImGui::SliderFloat("Contrast", &bbAdjust[1], 0, 3);
    ImGui::SliderFloat("Hue", &bbAdjust[2], -1, 1);
    ImGui::SliderFloat("Saturation", &bbAdjust[3], 0, 3);
    ImGui::SliderFloat("Opacity", &bbAdjust[4], 0, 1);
    if (ImGui::Button("Apply")) {
      if (bbTexture >= 0 && size_t(bbTexture) < bbProject.textures.size())
        bbEdit("Adjust texture",
               [] {
                 bb::adjustImage(bbProject.textures[bbTexture], bbAdjust[0], bbAdjust[1],
                                 bbAdjust[2], bbAdjust[3], bbAdjust[4]);
               },
               {size_t(bbTexture)});
      bbAdjustOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbAdjustOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (bbChooseOpen)
    ImGui::OpenPopup("Model files");
  ImGui::SetNextWindowSize({760 * uiScale(), 580 * uiScale()}, ImGuiCond_FirstUseEver);
  if (ImGui::BeginPopupModal("Model files", &bbChooseOpen)) {
    if (ImGui::Button("Up")) {
      bbChooseFolder = bbChooseFolder.parent_path();
      std::snprintf(bbChoosePath, sizeof bbChoosePath, "%s", bbChooseFolder.u8string().c_str());
    }
    ImGui::SameLine();
    if (ImGui::InputText("Directory", bbChoosePath, sizeof bbChoosePath,
                         ImGuiInputTextFlags_EnterReturnsTrue))
      bbChooseFolder = std::filesystem::u8path(bbChoosePath);
    ImGui::BeginChild("files", {0, -75 * uiScale()}, ImGuiChildFlags_Borders);
    try {
      auto entries = modFileList(bbChooseFolder);
      for (auto &[directory, path] : entries) {
        ImGui::PushID(path.u8string().c_str());
        auto name = path.filename().u8string();
        if (ImGui::Selectable(((directory ? "[Folder] " : "") + name).c_str(),
                              std::string(bbChooseName) == name,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
          if (directory) {
            bbChooseFolder = path;
            std::snprintf(bbChoosePath, sizeof bbChoosePath, "%s", path.u8string().c_str());
          } else
            std::snprintf(bbChooseName, sizeof bbChooseName, "%s", name.c_str());
        }
        ImGui::PopID();
      }
    } catch (const std::exception &e) {
      ImGui::TextWrapped("%s", e.what());
    }
    ImGui::EndChild();
    ImGui::InputText("Filename", bbChooseName, sizeof bbChooseName);
    if (ImGui::Button(bbChooseSave ? "Save" : "Open") && *bbChooseName) {
      auto path = bbChooseFolder / std::filesystem::u8path(bbChooseName);
      auto callback = bbChooseCallback;
      bbChooseOpen = false;
      ImGui::CloseCurrentPopup();
      if (callback)
        callback(path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbChooseOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (bbGuardOpen)
    ImGui::OpenPopup("Unsaved model project");
  if (ImGui::BeginPopupModal("Unsaved model project", &bbGuardOpen,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("Save changes to the model project before continuing?");
    if (ImGui::Button("Save")) {
      auto pending = bbPending;
      bbGuardOpen = false;
      ImGui::CloseCurrentPopup();
      if (bbProject.path.empty())
        bbChoose(true, modFileRoot() / (bbProject.root.value("name", "untitled") + ".bbmodel"),
                 [pending](auto path) {
                   if (bbSave(path) && pending)
                     pending();
                 });
      else if (bbSave(std::filesystem::u8path(bbProject.path)) && pending)
        pending();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
      bbGuardOpen = false;
      ImGui::CloseCurrentPopup();
      auto next = std::move(bbPending);
      bbPending = {};
      if (next)
        next();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbGuardOpen = false;
      bbPending = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (bbNewOpen)
    ImGui::OpenPopup("New model");
  ImGui::SetNextWindowSize({650 * uiScale(), 430 * uiScale()}, ImGuiCond_FirstUseEver);
  if (ImGui::BeginPopupModal("New model", &bbNewOpen)) {
    ImGui::InputText("Name", bbName, sizeof bbName);
    ImGui::Combo("Template", &bbNewType,
                 "Empty\0Quake monster\0Weapon view model\0Item pickup\0Prop\0");
    ImGui::Combo("Target format", &bbTarget,
                 "Quake MDL\0Quake II MD2\0MD3\0GoldSrc studio MDL\0OBJ\0IQM\0MD5MESH\0");
    ImGui::InputText("Folder", bbFolder, sizeof bbFolder);
    if (ImGui::Button("Choose folder...")) {
      bbNewOpen = false;
      ImGui::CloseCurrentPopup();
      bbChoose(true, std::filesystem::u8path(bbFolder) / "project.bbmodel", [](auto path) {
        std::snprintf(bbFolder, sizeof bbFolder, "%s", path.parent_path().u8string().c_str());
        bbNewOpen = true;
      });
    }
    ImGui::InputInt("Texture resolution", &bbNewResolution);
    ImGui::Checkbox("Box UV", &bbNewBox);
    ImGui::Checkbox("Create texture", &bbNewTexture);
    if (ImGui::Button("Create")) {
      try {
        auto folder = std::filesystem::u8path(bbFolder);
        modFileValidate(folder);
        auto p = bb::makeTemplate(bbNewType, bbName, gameProfile);
        for (auto &animation : p.root["animations"])
          animation["snapping"] = bbAnimationSnap;
        p.root["meta"]["box_uv"] = bbNewBox;
        for (auto &e : p.root["elements"])
          if (e.value("type", "") == "cube")
            e["box_uv"] = bbNewBox;
        const char *formats[] = {"mdl", "md2", "md3", "studio", "obj", "iqm", "md5mesh"};
        p.root["reyondbroom"]["target_format"] = formats[bbTarget];
        p.path = (folder / (std::string(bbName) + ".bbmodel")).u8string();
        if (std::filesystem::exists(std::filesystem::u8path(p.path)))
          throw std::runtime_error("A project with this name already exists; choose another name");
        if (bbNewTexture)
          bb::textureTemplate(p, "skin.png", std::clamp(bbNewResolution, 16, 256), true, 0, true,
                              bbNewBox);
        bbNewOpen = false;
        ImGui::CloseCurrentPopup();
        bbLeave([p = std::move(p)]() mutable {
          bbActivate(std::move(p));
          bbSavedStamp = ~uint64_t(0);
        });
      } catch (const std::exception &e) {
        bbNotice(e);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbNewOpen = false;
      ImGui::CloseCurrentPopup();
    }
    if (!bbMessage.empty())
      ImGui::TextWrapped("%s", bbMessage.c_str());
    ImGui::EndPopup();
  }
  if (bbPrimitiveOpen)
    ImGui::OpenPopup("Add mesh");
  if (ImGui::BeginPopupModal("Add mesh", &bbPrimitiveOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
    static int kind = 0, sides = 12, minorSides = 8;
    static float diameter = 16, height = 8, minor = 4;
    static bool align = true;
    ImGui::Combo(
        "Shape", &kind,
        "Cuboid\0Beveled "
        "cuboid\0Pyramid\0Plane\0Circle\0Cylinder\0Tube\0Cone\0Sphere\0Icosphere\0Torus\0");
    ImGui::InputFloat("Diameter", &diameter);
    ImGui::InputFloat("Height", &height);
    ImGui::InputInt("Sides", &sides);
    ImGui::InputFloat("Minor diameter", &minor);
    ImGui::InputInt("Minor sides", &minorSides);
    ImGui::Checkbox("Align edges", &align);
    if (ImGui::Button("Create mesh")) {
      bbEdit("Add mesh", [&] {
        auto m = bb::primitive(kind, diameter, height, sides, minor, minorSides, align);
        if (auto group = bbProject.find(bbSelection.activeGroup))
          m["origin"] = bb::getv(*group, "origin");
        if (!bbProject.textures.empty())
          for (auto &f : m["faces"])
            f["texture"] = bbTexture >= 0 ? bbTexture : 0;
        bbProject.root["elements"].push_back(m);
        bb::appendNode(bbProject, m["uuid"], bbSelection.activeGroup);
        bbSelect(m["uuid"]);
      });
      bbPrimitiveOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbPrimitiveOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (bbTemplateOpen)
    ImGui::OpenPopup("Create texture");
  if (ImGui::BeginPopupModal("Create texture", &bbTemplateOpen,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name[128] = "skin.png";
    static int density = 64, fill = 0;
    static bool compress = true, outline = true, boxes = false, isTemplate = true;
    static float fillColor[4] = {.5, .5, .5, 1};
    ImGui::InputText("Name", name, sizeof name);
    ImGui::InputInt("Resolution per 16 units", &density);
    ImGui::Checkbox("Template", &isTemplate);
    ImGui::Checkbox("Box UV template", &boxes);
    ImGui::Checkbox("Compress", &compress);
    ImGui::Checkbox("Add outline", &outline);
    ImGui::Combo("Fill color", &fill, "Element marker\0Transparent\0Custom color\0");
    if (fill == 2)
      ImGui::ColorEdit4("Fill color RGBA", fillColor);
    if (ImGui::Button("Create texture")) {
      bbEdit("Create texture template", [&] {
        if (isTemplate)
          bb::textureTemplate(bbProject, name, std::clamp(density, 16, 256), compress, fill,
                              outline, boxes);
        else {
          bb::Texture t;
          t.id = bb::uuid();
          t.name = name;
          t.width = t.height = t.uvWidth = t.uvHeight = std::clamp(density, 16, 256);
          t.rgba.assign(size_t(t.width) * t.height * 4, 0);
          bb::addTexture(bbProject, t);
        }
        if (fill == 2) {
          auto &tex = bbProject.textures.back();
          for (size_t n = 0; n < tex.rgba.size(); n += 4)
            if (tex.rgba[n + 3])
              for (int a = 0; a < 4; a++)
                tex.rgba[n + a] = std::clamp(int(fillColor[a] * 255), 0, 255);
          tex.revision++;
        }
        bbTexture = bbProject.textures.size() - 1;
      });
      bbTemplateOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbTemplateOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (bbResizeOpen)
    ImGui::OpenPopup("Resize texture");
  if (ImGui::BeginPopupModal("Resize texture", &bbResizeOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputInt("Width", &bbResizeW);
    ImGui::InputInt("Height", &bbResizeH);
    ImGui::Checkbox("Recalculate UV", &bbResizeUV);
    if (ImGui::Button("Resize")) {
      bbEdit("Resize texture",
             [] {
               auto &t = bbProject.textures[bbTexture];
               auto old = t;
               t.width = std::clamp(bbResizeW, 1, 2048);
               t.height = std::clamp(bbResizeH, 1, 2048);
               t.rgba.resize(size_t(t.width) * t.height * 4);
               for (int y = 0; y < t.height; y++)
                 for (int x = 0; x < t.width; x++) {
                   auto c = bb::pixel(old, x * old.width / t.width, y * old.height / t.height);
                   std::copy(c.begin(), c.end(), t.rgba.begin() + (size_t(y) * t.width + x) * 4);
                 }
               if (bbResizeUV) {
                 double sx = double(t.width) / old.width, sy = double(t.height) / old.height;
                 for (auto &e : bbProject.root["elements"])
                   if (e.contains("faces"))
                     for (auto &f : e["faces"])
                       if (bb::textureIndex(f) == bbTexture) {
                         if (f["uv"].is_array())
                           for (int n = 0; n < 4; n++)
                             f["uv"][n] = f["uv"][n].get<double>() * (n % 2 ? sy : sx);
                         else
                           for (auto &v : f["uv"]) {
                             v[0] = v[0].get<double>() * sx;
                             v[1] = v[1].get<double>() * sy;
                           }
                       }
                 t.uvWidth = t.width;
                 t.uvHeight = t.height;
               }
               t.saved = false;
               t.revision++;
             },
             {size_t(bbTexture)});
      bbResizeOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      bbResizeOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (bbSettingsOpen)
    ImGui::OpenPopup("Model editor settings");
  if (ImGui::BeginPopupModal("Model editor settings", &bbSettingsOpen,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputFloat("Default cube size", &bbDefaultCube);
    ImGui::InputFloat("Grid snap", &bbGrid);
    ImGui::InputFloat("Rotation snap", &bbRotationSnap);
    ImGui::InputInt("Animation snap", &bbAnimationSnap);
    bbAnimationSnap = std::clamp(bbAnimationSnap, 1, 500);
    ImGui::InputFloat("Autosave minutes", &bbAutosaveMinutes);
    ImGui::SliderFloat("Undo history", &bbUndoSteps, 1, 128, "%.0f");
    ImGui::Checkbox("Show Quake axes in inspector", &bbQuakeInspector);
    ImGui::Checkbox("Embed textures in project", &bbEmbed);
    ImGui::Checkbox("Pixel grid", &bbPixelGrid);
    ImGui::Checkbox("Brush outline", &bbBrushOutline);
    if (ImGui::Checkbox("Limit painting to palette", &bbLimitPalette))
      bbPalettePreference = bbLimitPalette ? 1 : 0;
    if (ImGui::Button("Done")) {
      bbSettingsOpen = false;
      bbHistory.steps = bbUndoSteps;
      bbSettingsSave();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (bbProjectSettingsOpen)
    ImGui::OpenPopup("Model project settings");
  if (ImGui::BeginPopupModal("Model project settings", &bbProjectSettingsOpen,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    auto &settings = bbProject.root["reyondbroom"];
    float scale = settings.value("export_scale", 1.f);
    if (ImGui::InputFloat("Export scale", &scale))
      bbEdit("Export scale", [&] { settings["export_scale"] = std::clamp(scale, .001f, 10000.f); });
    int front = bbProject.front();
    if (ImGui::Combo("Front", &front, "North\0South\0East\0West\0"))
      bbEdit("Model front", [&] {
        settings["front"] = front == 1   ? "south"
                            : front == 2 ? "east"
                            : front == 3 ? "west"
                                         : "north";
      });
    bool base = settings.value("include_base_frame", true);
    if (ImGui::Checkbox("Include base frame", &base))
      bbEdit("Base frame export", [&] { settings["include_base_frame"] = base; });
    if (ImGui::Button("Done")) {
      bbProjectSettingsOpen = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (bbExportOpen)
    ImGui::OpenPopup("Export authoring model");
  ImGui::SetNextWindowSize({720 * uiScale(), 500 * uiScale()}, ImGuiCond_FirstUseEver);
  if (ImGui::BeginPopupModal("Export authoring model", &bbExportOpen)) {
    ImGui::Combo("Target format", &bbTarget,
                 "Quake MDL\0Quake II MD2\0MD3\0GoldSrc studio MDL\0OBJ\0IQM\0MD5MESH\0");
    if (bbTarget <= 1)
      ImGui::TextWrapped("The target stores one indexed skin: textures are resampled into padded "
                         "256-pixel atlas tiles. Palette quantization and alpha loss may occur. "
                         "Vertices use 8-bit position quantization.");
    ImGui::TextWrapped(
        "MD3: coordinate range -512 to 511.984; 1/64 precision. OBJ, IQM and MD5MESH export one "
        "static pose. Locator tags are not retained by these writers.");
    size_t approximateVertices = bbTriangles.size() * 3;
    if (bbTarget == 2 && approximateVertices > 1000)
      ImGui::TextWrapped(
          "Compatibility warning: this model exceeds the original Quake III limit of 1000 vertices "
          "per surface. FTE permits up to 4096 vertices / 8192 triangles per surface.");
    if (bbTarget <= 1) {
      bb::Vec3 lo{1e30, 1e30, 1e30}, hi{-1e30, -1e30, -1e30};
      for (auto &t : bbTriangles)
        for (auto v : t.p) {
          v = bb::bbToQuake(v, bbProject.front(), bbProject.exportScale());
          for (int a = 0; a < 3; a++) {
            lo[a] = std::min(lo[a], v[a]);
            hi[a] = std::max(hi[a], v[a]);
          }
        }
      if (lo[0] <= hi[0])
        ImGui::Text("Base-pose 8-bit quantization step: %.4f / %.4f / %.4f units",
                    (hi[0] - lo[0]) / 255, (hi[1] - lo[1]) / 255, (hi[2] - lo[2]) / 255);
    }
    int first = bbProject.root["reyondbroom"].value("include_base_frame", true) ? 1 : 0;
    for (auto &a : bbProject.root["animations"]) {
      bool include = a.value("reyondbroom", bb::Json::object()).value("export", true);
      auto name = a.value("name", "animation");
      ImGui::PushID(a.value("uuid", "").c_str());
      if (ImGui::Checkbox(name.c_str(), &include))
        bbEdit("Animation export selection", [&] { a["reyondbroom"]["export"] = include; });
      double fps = a.value("reyondbroom", bb::Json::object()).value("export_fps", 10.0);
      int count = std::max(1, int(std::ceil(bb::animationLength(a) * fps - 1e-8)) +
                                  (a.value("loop", "once") == "loop" ? 0 : 1));
      ImGui::SameLine();
      ImGui::Text("%d frames | %d - %d", count, first, first + count - 1);
      if (include)
        first += count;
      ImGui::PopID();
    }
    if (ImGui::Button("Copy frame list"))
      ImGui::SetClipboardText(bb::frameList(bbProject).c_str());
    if (ImGui::Button("Export model as...")) {
      const char *exts[] = {".mdl", ".md2", ".md3", ".mdl", ".obj", ".iqm", ".md5mesh"};
      auto setting = bbProject.root["reyondbroom"].value(
          "export_path",
          std::string("models/") + bbProject.root.value("name", "model") + exts[bbTarget]);
      auto path = bbPath(setting);
      path.replace_extension(exts[bbTarget]);
      bbExportOpen = false;
      ImGui::CloseCurrentPopup();
      bbChoose(true, path, [](auto target) {
        try {
          bbExportTo(target);
        } catch (const std::exception &e) {
          bbNotice(e);
        }
      });
    }
    if (!bbLastExport.empty() && ImGui::Button("Place in map"))
      try {
        auto view = viewRects[viewFocus];
        command(modelDropText(std::filesystem::u8path(bbLastExport), view.x + view.w * .5f,
                              view.y + view.h * .5f)
                    .c_str());
      } catch (const std::exception &e) {
        bbNotice(e);
      }
    ImGui::EndPopup();
  }
}
static void bbImportTick() {
  bbBridgeTick();
  bbNativeImportTick();
}
static void bbNewDialog() {
  bbSettingsLoad();
  bbImportTick();
  bbDialogs();
}
static void bbPanel() {
  auto authorBegan = std::chrono::steady_clock::now();
  modelEditorCameraStep();
  if (bbBridgeJob.valid()) {
    ImGui::SetNextWindowSize({420 * uiScale(), 110 * uiScale()}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Importing model")) {
      ImGui::TextWrapped("Preparing the authoring project. Source files are preserved.");
      unsigned long long done = 0, total = 0;
      std::string phase;
      std::ifstream progress(modelImportRuntimeRoot() / "progress.txt");
      if (progress >> done >> total)
        std::getline(progress, phase);
      ImGui::ProgressBar(total ? float(done) / total : -float(ImGui::GetTime()), {0, 0},
                         phase.empty() ? "Importing..." : phase.c_str());
    }
    ImGui::End();
  }
  bool open = true;
  ImGui::SetNextWindowSize({1400 * uiScale(), 850 * uiScale()}, ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Model editor", &open, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse)) {
    bbFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    bbMenus();
    auto dock = ImGui::GetID("Model authoring dockspace");
    if (!ImGui::DockBuilderGetNode(dock)) {
      ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dock, ImGui::GetContentRegionAvail());
      ImGuiID center = dock,
              left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .23f, nullptr, &center),
              right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, .26f, nullptr, &center),
              bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, .26f, nullptr, &center);
      ImGui::DockBuilderDockWindow("Model tools", left);
      ImGui::DockBuilderDockWindow("Model properties", right);
      ImGui::DockBuilderDockWindow("Model viewport", center);
      ImGui::DockBuilderDockWindow("Model timeline", bottom);
      ImGui::DockBuilderFinish(dock);
    }
    ImGui::DockSpace(dock, {0, -ImGui::GetTextLineHeightWithSpacing() * 2});
    size_t vertices = 0;
    for (auto &element : bbProject.root["elements"])
      vertices += element.value("type", "") == "cube"   ? 8
                  : element.value("type", "") == "mesh" ? element["vertices"].size()
                                                        : 0;
    ImGui::Text("%s%s | %zu elements | %zu vertices | %zu triangles",
                bbProject.root.value("name", "untitled").c_str(), bbDirty() ? " *" : "",
                bbProject.root["elements"].size(), vertices, bbTriangles.size());
    auto selected = bb::descendants(bbProject, bbIds());
    if (!selected.empty()) {
      bb::Vec3 low{1e30, 1e30, 1e30}, high{-1e30, -1e30, -1e30};
      for (auto &triangle : bbTriangles)
        if (selected.count(triangle.element))
          for (auto point : triangle.p)
            for (int a = 0; a < 3; a++) {
              low[a] = std::min(low[a], point[a]);
              high[a] = std::max(high[a], point[a]);
            }
      if (low[0] <= high[0]) {
        ImGui::SameLine();
        ImGui::Text("| Size %.2f x %.2f x %.2f", high[0] - low[0], high[1] - low[1],
                    high[2] - low[2]);
      }
    }
    if (!bbMessage.empty())
      ImGui::TextWrapped("%s", bbMessage.c_str());
  }
  ImGui::End();
  if (!open)
    bbAction("Close");
  if (!bbActive)
    return;
  if (ImGui::Begin("Model tools")) {
    bbFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    bbLeft();
  }
  ImGui::End();
  if (ImGui::Begin("Model properties")) {
    bbFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    bbInspector();
    bbOutliner();
  }
  ImGui::End();
  if (ImGui::Begin("Model viewport", nullptr,
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    bbFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    try {
      bbUpload();
      bbViewport();
    } catch (const std::exception &e) {
      bbNotice(e);
      ImGui::TextWrapped("%s", e.what());
    }
  }
  ImGui::End();
  bbTimelineFocused = false;
  if (bbMode == 2) {
    if (ImGui::Begin("Model timeline")) {
      bbTimelineFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
      bbFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
      bbTimeline();
    }
    ImGui::End();
  }
  if (bbPlaying && bbMode == 2 && bbAnimation >= 0 &&
      size_t(bbAnimation) < bbProject.root["animations"].size()) {
    double length = bb::animationLength(bbProject.root["animations"][bbAnimation]);
    bbPlayTime += ImGui::GetIO().DeltaTime * bbPlaySpeed;
    if (bbPlayTime > length) {
      if (bbLoopPreview && length > 0)
        bbPlayTime = std::fmod(bbPlayTime, length);
      else {
        bbPlayTime = length;
        bbPlaying = false;
      }
    }
  }
  cvars->SetFloat("ftebroom_modelviewer_open", 1);
  cvars->SetFloat("ftebroom_bb_active", 1);
  if (bbDirty() && bbAutosaveMinutes > 0 &&
      std::chrono::duration<double>(std::chrono::steady_clock::now() - bbAutosaveAt).count() >
          bbAutosaveMinutes * 60 &&
      !bbHistory.active()) {
    try {
      auto name =
          std::filesystem::u8path(bbProject.root.value("name", "model")).filename().u8string();
      auto folder = documentPath("data/model-autosave");
      std::filesystem::create_directories(folder);
      bb::atomicWrite(folder / (name + ".bbmodel"), bb::serialize(bbProject, true));
      bbAutosaveAt = std::chrono::steady_clock::now();
    } catch (const std::exception &e) {
      bbNotice(e);
      bbAutosaveAt = std::chrono::steady_clock::now();
    }
  }
  if (bbAuthorTiming)
    bbAuthorCpu +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - authorBegan)
            .count();
}
