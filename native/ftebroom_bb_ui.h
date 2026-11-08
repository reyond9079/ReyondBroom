// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_export.h"
#include "ftebroom_bb_image_ops.h"
#include "ftebroom_bb_obj.h"
#include "ftebroom_bb_paint.h"
#include "ftebroom_bb_pick.h"
#include "ftebroom_bb_tools.h"
#include "ftebroom_bb_uv.h"
namespace bb = ftebroom::bb;
static bb::Project bbProject;
static bb::Selection bbSelection;
static bb::History bbHistory;
static std::vector<std::pair<std::string, ImVec2>> bbTimelineProbe;
static bool bbTimelineFocused = false;
static std::string bbLastVertex;
static bool bbViewSaveOpen = false;
static char bbViewName[128] = "View";
static bool bbFocused = false, bbPlaying = false, bbOrtho = false, bbSilhouette = true,
            bbShowBones = true, bbPaint2D = false, bbMirrorPaint = false,
            bbOnlySelectedPaint = false, bbLimitPalette = false;
static int bbPalettePreference = -1;
static int bbKeyChannel = 0, bbSnapMode = 0, bbAnimationSnap = 24;
static bool bbRenameOpen = false;
static char bbRenameText[256] = {};
static std::vector<std::string> bbOutlinerOrder, bbOutlinerBuild;
static std::set<std::string> bbCollapseNodes;
static bool bbAutoKey = true;
static float bbAnimationSpeedFactor = 1;
static bool bbAnimationSpeedOpen = false, bbMergeAnimationOpen = false;
static int bbMergeAnimation = 0;
static bb::Json bbPoseClipboard;
static int bbMode = 0, bbTool = 0, bbSelectionMode = 0, bbSpace = 0, bbTexture = 0, bbFace = 0,
           bbAnimation = 0, bbDisplay = 0, bbFillMode = 3;
static float bbGrid = 1, bbRotationSnap = 15, bbPlayTime = 0, bbPlaySpeed = 1, bbUndoSteps = 128,
             bbDefaultCube = 2, bbAutosaveMinutes = 2;
static bool bbNewFteQ1=false;
static bool bbEmbed = true, bbQuakeInspector = false, bbPixelGrid = true, bbBrushOutline = true,
            bbLoopPreview = true;
static uint64_t bbSavedStamp = 0;
static std::string bbSaved, bbMessage, bbLastExport, bbClipboard;
static bb::Json bbClipboardData;
struct BbPreviewEdge {
  std::string element, face, aKey, bKey;
  bb::Vec3 a, b;
  bb::Vec2 u, v;
  int texture = -1;
};
static std::vector<BbPreviewEdge> bbPreviewEdges;
static double bbLastUpload = -1000;
static std::map<std::string, std::vector<std::array<bb::Vec2, 3>>> bbSurfaceUV;
static std::vector<bb::Triangle> bbTriangles;
static bb::VisibilityTree bbVisibility;
static uint64_t bbVisibilityEpoch = 0;
static uint64_t bbRenderRevision = 0;
static double bbLastPose = -1;
static std::map<std::string, qhandle_t> bbImages;
static std::map<std::string, uint64_t> bbImageRevisions;
static std::map<std::string, double> bbImageTimes;
static std::chrono::steady_clock::time_point bbAutosaveAt = std::chrono::steady_clock::now();
static bb::PaintOptions bbBrush;
static bb::Pixel bbSecondary{30, 40, 70, 255};
static float bbColor[4] = {.7f, .5f, .35f, 1};
static std::vector<bb::Pixel> bbPalette, bbGamePalette;
static bb::Texture bbPixelClipboard;
static int bbFillTolerance = 0;
static bool bbAdjustOpen = false;
static float bbAdjust[5] = {0, 1, 0, 1, 1};
static bool bbNewOpen = false, bbSettingsOpen = false, bbProjectSettingsOpen = false,
            bbPrimitiveOpen = false, bbTemplateOpen = false, bbExportOpen = false,
            bbGuardOpen = false;
static std::function<void()> bbPending;
static char bbName[256] = "untitled", bbFolder[4096] = {};
static int bbNewType = 1, bbNewResolution = 64, bbTarget = 0;
static bool bbNewTexture = true, bbNewBox = true;
static char bbOutlinerSearch[256] = {};
static float bbUVZoom = 4;
static ImVec2 bbUVPan;
static float bbTimelineZoom = 100;
static std::set<std::string> bbUVVertices;
static void bbAction(const std::string &name);
static void bbResetInteraction();
static void bbViewport();
static void bbLeft();
static void bbInspector();
static void bbOutliner();
static void bbTimeline();
static void bbMenus();
static void bbDialogs();
static void bbTexturePanel();
static void bbUVPanel(bool paint = false);
static bool bbDirty() {
  return bbActive && bbHistory.state() != bbSavedStamp;
}
static void bbNotice(const std::exception &e) {
  bbMessage = e.what();
}
static std::vector<size_t> bbTextureIndices() {
  std::vector<size_t> ids;
  for (size_t i = 0; i < bbProject.textures.size(); i++)
    ids.push_back(i);
  return ids;
}
template <class F> static void bbEdit(const char *label, F fn, std::vector<size_t> images = {}) {
  try {
    bbHistory.begin(bbProject, bbSelection, images);
    fn();
    bbHistory.finish(bbProject, bbSelection, label);
  } catch (const std::exception &e) {
    bbHistory.cancel(bbProject, bbSelection);
    bbNotice(e);
  }
}
static bool bbCheckerReady = false;
static void bbReleaseImages() {
  bbCheckerReady = false;
  if (auto api = bbImageAPI())
    api->clear();
  for (auto [id, image] : bbImages)
    if (image)
      draw->UnloadImage(image);
  bbImages.clear();
  bbImageRevisions.clear();
  bbImageTimes.clear();
}
static void bbRegister() {
  for (auto n : {"ftebroom_bb_active", "ftebroom_bb_benchmark_b", "ftebroom_bb_benchmark_vertices",
                 "ftebroom_bb_low", "ftebroom_bb_high", "ftebroom_bb_ortho", "ftebroom_bb_display",
                 "ftebroom_bb_cull"})
    cvars->GetNVFDG(n, "0", 0, "Model authoring preview", "ReyondBroom");
}
static void bbPaletteLoad() {
  bbPalette.clear();
  if (gameProfile == 2)
    try {
      auto bytes = modelResource("gfx/palette.lmp");
      if (bytes.size() == 768)
        for (int i = 0; i < 256; i++)
          bbPalette.push_back({(unsigned char)bytes[i * 3], (unsigned char)bytes[i * 3 + 1],
                               (unsigned char)bytes[i * 3 + 2], 255});
    } catch (...) {
    }
  if (bbPalette.empty())
    for (int i = 0; i < 32; i++) {
      int value = (i % 8) * 31 + 20;
      bbPalette.push_back({(unsigned char)(i / 8 == 1 ? value / 2 : value),
                           (unsigned char)(i / 8 == 2 ? value / 2 : value),
                           (unsigned char)(i / 8 == 3 ? value / 2 : value), 255});
    }
  bbGamePalette = bbPalette;
  bbLimitPalette = bbPalettePreference < 0 ? gameProfile == 2 : bbPalettePreference != 0;
}
static std::filesystem::path bbPath(const std::string &path) {
  auto p = std::filesystem::u8path(path);
  return p.is_absolute() ? p : modFileRoot() / p;
}
static bool bbSettingsLoaded = false;
static void bbSettingsLoad() {
  if (bbSettingsLoaded)
    return;
  bbSettingsLoaded = true;
  try {
    auto path = documentPath("data/model-editor-settings.json");
    if (!std::filesystem::is_regular_file(path))
      return;
    auto j = bb::Json::parse(bb::readFile(path));
    bbDefaultCube = std::clamp(j.value("cube_size", 2.f), .01f, 1024.f);
    bbGrid = std::clamp(j.value("grid_snap", 1.f), .001f, 1024.f);
    bbRotationSnap = std::clamp(j.value("rotation_snap", 15.f), 0.f, 180.f);
    bbAnimationSnap = std::clamp(j.value("animation_snap", 24), 1, 500);
    bbAutosaveMinutes = std::clamp(j.value("autosave_minutes", 2.f), 0.f, 60.f);
    bbUndoSteps = std::clamp(j.value("undo_steps", 128.f), 1.f, 128.f);
    bbQuakeInspector = j.value("quake_axes", false);
    bbEmbed = j.value("embed_textures", true);
    bbPixelGrid = j.value("pixel_grid", true);
    bbBrushOutline = j.value("brush_outline", true);
    if (j.contains("limit_palette"))
      bbPalettePreference = j.value("limit_palette", false) ? 1 : 0;
  } catch (const std::exception &e) {
    bbNotice(e);
  }
}
static void bbSettingsSave() {
  try {
    auto path = documentPath("data/model-editor-settings.json");
    std::filesystem::create_directories(path.parent_path());
    bb::Json j = {{"cube_size", bbDefaultCube},
                  {"grid_snap", bbGrid},
                  {"animation_snap", bbAnimationSnap},
                  {"rotation_snap", bbRotationSnap},
                  {"autosave_minutes", bbAutosaveMinutes},
                  {"undo_steps", bbUndoSteps},
                  {"quake_axes", bbQuakeInspector},
                  {"embed_textures", bbEmbed},
                  {"pixel_grid", bbPixelGrid},
                  {"brush_outline", bbBrushOutline},
                  {"limit_palette", bbLimitPalette}};
    bb::atomicWrite(path, j.dump(2));
  } catch (const std::exception &e) {
    bbNotice(e);
  }
}
static void bbActivate(bb::Project project) {
  placementCancel(modelPlacement);placementCancel(modelNewPlacement);
  bbSettingsLoad();
  bbResetInteraction();
  bbOutlinerOrder.clear();
  bbOutlinerBuild.clear();
  bbCollapseNodes.clear();
  bbReleaseImages();
  if (auto api = bbAPI())
    api->clear();
  bbProject = std::move(project);
  bbSelection = {};
  bbHistory.clear();
  bbHistory.steps = bbUndoSteps;
  bbActive = true;
  modelViewerOpen = modelEditorActive = true;
  modelEditDirty = false;
  modelEditCameraFree = false;
  modelEditCameraEye = {};
  modelEditorCameraEnd();
  modelViewerYaw = 135;
  modelViewerPitch = 15;
  modelViewerZoom = 1;
  modelEditPlayer = bbProject.root.value("name", "").rfind("v_", 0) == 0;
  auto target = bbProject.root["reyondbroom"].value("target_format", "md3");
  bbTarget = target == "mdl"       ? 0
             : target == "md2"     ? 1
             : target == "studio"  ? 3
             : target == "obj"     ? 4
             : target == "iqm"     ? 5
             : target == "md5mesh" ? 6
                                   : 2;
  bbRenderRevision = 0;
  bbLastUpload = -1000;
  bbLastPose = -1;
  bbTexture = bbAnimation = 0;
  bbPlayTime = 0;
  bbPlaying = false;
  auto mode = bbProject.root.value("editor_state", bb::Json::object()).value("mode", "edit");
  bbMode = mode == "paint" ? 1 : mode == "animate" ? 2 : 0;
  bbSavedStamp = bbHistory.state();
  bbSaved = bb::serialize(bbProject, bbEmbed);
  bbMessage = bbProject.warnings.empty() ? "" : bbProject.warnings.front();
  bbPaletteLoad();
  bbRegister();
  cvars->SetFloat("ftebroom_bb_active", 1);
  bbAutosaveAt = std::chrono::steady_clock::now();
}
static void bbLeave(std::function<void()> next) {
  if (bbDirty()) {
    bbPending = std::move(next);
    bbGuardOpen = true;
  } else
    next();
}
static void bbOpen(const std::string &path) {
  bbLeave([path]() {
    try {
      auto file = bbPath(path);
      auto bytes = std::filesystem::is_regular_file(file) || modFilePacked(file)
                       ? modFileBytes(file)
                       : modelResource(path);
      auto parent = file.parent_path();
      auto p = bb::parseProject(bytes, [parent](const std::string &texture) {
        auto path = parent / std::filesystem::u8path(texture);
        if (std::filesystem::is_regular_file(path))
          return modFileBytes(path);
        return modelImportTexture(texture);
      });
      bool snapshot=!modFilePacked(file)&&placementSnapshotPath(file);
      if (!modFilePacked(file)&&!snapshot)p.path = file.u8string();else p.path.clear();
      bbActivate(std::move(p));
      if(snapshot)bbMessage="Opened an immutable source snapshot. Save As creates an editable copy; history assets stay intact.";
    } catch (const std::exception &e) {
      bbNotice(e);
    }
  });
}
static void bbNew(const std::filesystem::path &folder) {
  placementCancel(modelNewPlacement);bbNewFteQ1=false;
  bbLeave([folder] {
    bbNewOpen = true;
    auto path = folder.empty() ? modFileRoot() : folder;
    if(folder.empty()&&!std::filesystem::is_directory(path)&&gameProfile==1)path=std::filesystem::u8path(baseq3);
    std::snprintf(bbFolder, sizeof bbFolder, "%s", path.u8string().c_str());
    bbTarget = gameProfile == 2 ? 0 : gameProfile == 3 ? 1 : gameProfile == 4 ? 3 : 2;
  });
}
static bool bbSave(const std::filesystem::path &path) {
  try {
    if (modFilePacked(path) || path.extension() != ".bbmodel")
      throw std::runtime_error("Choose a loose .bbmodel project inside the mod directory");
    modFileValidate(path);
    std::filesystem::create_directories(path.parent_path());
    bb::atomicWrite(path, bb::serialize(bbProject, bbEmbed));
    bbProject.path = path.u8string();
    for (auto &t : bbProject.textures)
      t.saved = true;
    bbSavedStamp = bbHistory.state();
    bbSaved = bb::serialize(bbProject, bbEmbed);
    bbMessage = "Project saved";
    placementModelSaved();
    modFilesRefresh = true;
    return true;
  } catch (const std::exception &e) {
    bbNotice(e);
    return false;
  }
}
static bool bbChooseOpen = false, bbChooseSave = false;
static std::filesystem::path bbChooseFolder;
static char bbChoosePath[4096] = {}, bbChooseName[1024] = {};
static std::function<void(std::filesystem::path)> bbChooseCallback;
static void bbChoose(bool save, std::filesystem::path initial,
                     std::function<void(std::filesystem::path)> callback) {
  bbChooseSave = save;
  bbChooseFolder = initial.has_extension() ? initial.parent_path() : initial;
  if (!std::filesystem::is_directory(bbChooseFolder))
    bbChooseFolder = modFileRoot();
  if (!std::filesystem::is_directory(bbChooseFolder)&&gameProfile==1)bbChooseFolder=std::filesystem::u8path(baseq3);
  if (!std::filesystem::is_directory(bbChooseFolder))
    bbChooseFolder = documentPath("").parent_path();
  std::snprintf(bbChoosePath, sizeof bbChoosePath, "%s", bbChooseFolder.u8string().c_str());
  std::snprintf(bbChooseName, sizeof bbChooseName, "%s",
                initial.has_extension() ? initial.filename().u8string().c_str() : "");
  bbChooseCallback = std::move(callback);
  bbChooseOpen = true;
}
static void bbSaveAs() {
  auto path = bbProject.path.empty()
                  ? modFileRoot() / (bbProject.root.value("name", "untitled") + ".bbmodel")
                  : std::filesystem::u8path(bbProject.path);
  bbChoose(true, path, [](auto target) {
    if (std::filesystem::exists(target) &&
        (bbProject.path.empty() ||
         !std::filesystem::exists(std::filesystem::u8path(bbProject.path)) ||
         !std::filesystem::equivalent(target, std::filesystem::u8path(bbProject.path)))) {
      bbMessage = "Choose a new project filename; another project already exists there";
      return;
    }
    bbSave(target);
  });
}
static qhandle_t bbImage(int index) {
  if (index < 0 || size_t(index) >= bbProject.textures.size())
    return 0;
  auto &t = bbProject.textures[index];
  double now = core->GetMilliseconds() / 1000.;
  if (bbImageRevisions[t.id] != t.revision &&
      (!bbImages[t.id] || now - bbImageTimes[t.id] >= 1. / 30)) {
    auto name = bb::textureName(t.id);
    if (auto api = bbImageAPI())
      bbImages[t.id] = api->upload(name.c_str(), t.rgba.data(), t.width, t.height);
    else {
      auto bytes = bb::tga(t);
      draw->LoadImageData(name.c_str(), "image/tga", bytes.data(), bytes.size());
      if (!bbImages[t.id])
        bbImages[t.id] = draw->LoadImage(name.c_str());
    }
    // A temporarily unavailable renderer must retry rather than mark an image clean.
    if (bbImages[t.id])
      bbImageRevisions[t.id] = t.revision;
    bbImageTimes[t.id] = now;
  }
  return bbImages[t.id];
}
static void bbUpload() {
  if (!bbCheckerReady)
    if (auto api = bbImageAPI()) {
      std::vector<unsigned char> pixels(64 * 64 * 4, 255);
      for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
          auto n = (y * 64 + x) * 4;
          bool dark = ((x / 4) + (y / 4)) % 2;
          pixels[n] = dark ? 60 : 210;
          pixels[n + 1] = dark ? 110 : 220;
          pixels[n + 2] = dark ? 150 : 235;
        }
      auto image = api->upload("bbtex/checker.tga", pixels.data(), 64, 64);
      bbImages["__checker"] = image;
      bbCheckerReady = image != 0;
    }
  cvars->SetFloat("ftebroom_bb_display", bbDisplay);
  cvars->SetFloat("ftebroom_bb_cull", modelEditCull);
  for (size_t i = 0; i < bbProject.textures.size(); i++)
    bbImage(i);
  double time = bbMode == 2 ? bbPlayTime : 0;
  double now = core->GetMilliseconds() / 1000.;
  if ((bbRenderRevision != bbProject.revision || bbLastPose != time) &&
      (bbRenderRevision == 0 || now - bbLastUpload >= 1. / 60)) {
    bbLastUpload = now;
    std::map<std::string, bb::Pose> pose;
    if (bbMode == 2 && bbAnimation >= 0 &&
        size_t(bbAnimation) < bbProject.root["animations"].size())
      pose = bb::animationPose(bbProject.root["animations"][bbAnimation], time);
    bbTriangles = bb::triangles(bbProject, false, pose);
    bbVisibility.rebuild(bbTriangles);
    bbVisibilityEpoch++;
    bbPreviewEdges.clear();
    std::map<std::tuple<std::string, std::string, std::string, std::string>,
             std::pair<int, BbPreviewEdge>>
        edges;
    for (auto &triangle : bbTriangles)
      for (int n = 0; n < 3; n++) {
        int next = (n + 1) % 3;
        auto key = std::make_tuple(triangle.element, triangle.face,
                                   std::min(triangle.keys[n], triangle.keys[next]),
                                   std::max(triangle.keys[n], triangle.keys[next]));
        auto &edge = edges[key];
        edge.first++;
        edge.second = {triangle.element,    triangle.face,     triangle.keys[n],
                       triangle.keys[next], triangle.p[n],     triangle.p[next],
                       triangle.uv[n],      triangle.uv[next], triangle.texture};
      }
    for (auto &item : edges)
      if (item.second.first == 1)
        bbPreviewEdges.push_back(item.second.second);
    bbSurfaceUV.clear();
    for (auto &triangle : bbTriangles) {
      auto prefix = triangle.element + "\n" + std::to_string(triangle.texture) + "\n";
      bbSurfaceUV[prefix + triangle.face].push_back(triangle.uv);
      bbSurfaceUV[prefix + "*"].push_back(triangle.uv);
    }
    auto data = bb::renderBuffer(bbProject, bbTriangles, bbDisplay);
    if (auto api = bbAPI()) {
      if (!api->upload(data.data(), data.size()))
        throw std::runtime_error("The preview mesh exceeds the renderer budget");
    } else
      throw std::runtime_error("This authoring project needs the matching editor engine build");
    bb::Vec3 low{-16, -16, -24}, high{16, 16, 32};
    bool first = true;
    for (auto &t : bbTriangles)
      for (auto v : t.p) {
        auto q = bb::bbToQuake(v, bbProject.front(), bbProject.exportScale());
        if (first) {
          low = high = q;
          first = false;
        }
        for (int a = 0; a < 3; a++) {
          low[a] = std::min(low[a], q[a]);
          high[a] = std::max(high[a], q[a]);
        }
      }
    modelEditSet("ftebroom_bb_low", {float(low[0]), float(low[1]), float(low[2])});
    modelEditSet("ftebroom_bb_high", {float(high[0]), float(high[1]), float(high[2])});
    bbRenderRevision = bbProject.revision;
    bbLastPose = time;
  }
}
static void bbExportTo(const std::filesystem::path &target) {
  modFileValidate(target);
  if (modFilePacked(target) || std::filesystem::exists(target))
    throw std::runtime_error(
        "Choose a new model filename; existing source models are not overwritten");
  auto ext = entityLower(target.extension().u8string());
  bool animated = ext == ".mdl" || ext == ".md2" || ext == ".md3";
  auto baked = bb::bake(bbProject, animated);
  auto mesh = baked.mesh;
  std::vector<ftebroom::modelExport::Skin> skins;
  auto folder = target.parent_path() / (target.stem().u8string() + "-skins");
  if (ext != ".mdl" && std::filesystem::exists(folder))
    throw std::runtime_error("The destination skin directory already exists");
  for (size_t n = 0; n < baked.textures.size(); n++) {
    int index = baked.textures[n];
    ftebroom::modelExport::Skin skin;
    if (index >= 0 && size_t(index) < bbProject.textures.size()) {
      auto &t = bbProject.textures[index];
      skin.width = t.width;
      skin.height = t.height;
      skin.rgba = t.rgba;
    } else {
      skin.width = skin.height = 16;
      skin.rgba.assign(16 * 16 * 4, 180);
      for (size_t i = 3; i < skin.rgba.size(); i += 4)
        skin.rgba[i] = 255;
    }
    skins.push_back(skin);
    auto path = folder / (std::to_string(n) + ".tga");
    auto material = modFileModelPath(path);
    mesh.surfaces[n].material =
        material.empty() ? path.lexically_relative(target.parent_path()).generic_u8string()
                         : material;
  }
  if (ext == ".md3")
    for (auto &surface : mesh.surfaces)
      if (surface.material.size() > 63)
        throw std::runtime_error("MD3 skin paths must fit 63 bytes; use a shorter export path");
  if (ext == ".md2" && modFileModelPath(folder / "skin.pcx").size() > 63)
    throw std::runtime_error("MD2 skin paths must fit 63 bytes; use a shorter export path");
  std::string output, pcx;
  bool embedded = ext == ".mdl";
  if (ext == ".md3")
    output = ftebroom::modelExport::md3(mesh);
  else if (ext == ".mdl") {
    if (bbTarget == 3)
      output = ftebroom::studioExport::bake(mesh, skins,
                                            bbProject.root["animations"].empty()
                                                ? 10.f
                                                : bbProject.root["animations"][0]
                                                      .value("reyondbroom", bb::Json::object())
                                                      .value("export_fps", 10.f));
    else {
      output = ftebroom::modelExport::mdl(mesh, skins, modelResource("gfx/palette.lmp"));
      if (bbProject.root["reyondbroom"].value("rotate_pickup", false) && output.size() > 80)
        output[76] = char((unsigned char)output[76] | 8);
    }
  } else if (ext == ".md2") {
    auto skinName = modFileModelPath(folder / "skin.pcx");
    auto result = ftebroom::modelExport::md2(mesh, skins, q2Palette(), skinName);
    output = std::move(result.model);
    pcx = std::move(result.skin);
  } else if (ext == ".obj")
    output = ftebroom::modelExport::obj(mesh);
  else if (ext == ".iqm")
    output = ftebroom::modelExport::iqm(mesh);
  else if (ext == ".md5mesh")
    output = ftebroom::modelExport::md5mesh(mesh);
  else
    throw std::runtime_error("Choose MDL, MD2, MD3, OBJ, IQM or MD5MESH");
  auto metadata = target;
  metadata += ".reyond.json";
  if (std::filesystem::exists(metadata))
    throw std::runtime_error("The destination authoring metadata already exists");
  auto mtl = target;
  mtl.replace_extension(".mtl");
  if (ext == ".obj" && std::filesystem::exists(mtl))
    throw std::runtime_error("The destination MTL already exists");
  std::vector<std::filesystem::path> owned;
  bool ownedFolder = false;
  try {
    std::filesystem::create_directories(target.parent_path());
    if (!embedded) {
      ownedFolder = std::filesystem::create_directory(folder);
      if (!ownedFolder)
        throw std::runtime_error("Cannot create skin directory");
      if (!pcx.empty()) {
        bb::atomicWrite(folder / "skin.pcx", pcx);
        owned.push_back(folder / "skin.pcx");
      } else
        for (size_t i = 0; i < skins.size(); i++) {
          bb::Texture t;
          t.width = skins[i].width;
          t.height = skins[i].height;
          t.rgba = skins[i].rgba;
          auto skinPath = folder / (std::to_string(i) + ".tga");
          bb::atomicWrite(skinPath, bb::tga(t));
          owned.push_back(skinPath);
        }
    }
    if (ext == ".obj") {
      auto mtl = target;
      mtl.replace_extension(".mtl");
      if (std::filesystem::exists(mtl))
        throw std::runtime_error("The destination MTL already exists");
      std::string text;
      for (size_t i = 0; i < skins.size(); i++)
        text += "newmtl surface" + std::to_string(i) + "\nmap_Kd " +
                (folder / (std::to_string(i) + ".tga"))
                    .lexically_relative(target.parent_path())
                    .generic_u8string() +
                "\n";
      bb::atomicWrite(mtl, text);
      owned.push_back(mtl);
      output = "mtllib " + mtl.filename().u8string() + "\n" + output;
    }
    bb::atomicWrite(target, output);
    owned.push_back(target);
    bb::Vec3 low{1e30, 1e30, 1e30}, high{-1e30, -1e30, -1e30};
    for (auto &surface : mesh.surfaces)
      for (auto &vertex : surface.frames[0])
        for (int a = 0; a < 3; a++) {
          low[a] = std::min(low[a], double(vertex.position[a]));
          high[a] = std::max(high[a], double(vertex.position[a]));
        }
    bb::Json manifest = {
        {"kind", "reyondbroom-authoring-model"},
        {"version", 1},
        {"low", low},
        {"high", high},
        {"frame_names", mesh.frameNames},
        {"source_project", std::filesystem::u8path(bbProject.path).filename().u8string()}};
    bb::atomicWrite(metadata, manifest.dump(2));
  } catch (...) {
    std::error_code ec;
    for (auto it = owned.rbegin(); it != owned.rend(); ++it)
      std::filesystem::remove(*it, ec);
    if (ownedFolder)
      std::filesystem::remove(folder, ec);
    throw;
  }
  bbLastExport = target.u8string();
  bbMessage = "Exported model and textures";
  if (!baked.warnings.empty())
    bbMessage += ". " + baked.warnings.front();
  modFilesRefresh = true;
  command("fs_flush\n");
}
static void bbFrameSelection() {
  modelEditorCameraEnd();
  modelEditCameraFree = false;
  modelViewerZoom = 1;
  cvars->SetFloat("ftebroom_modeleditor_freecamera", 0);
  bb::Vec3 low{1e30, 1e30, 1e30}, high{-1e30, -1e30, -1e30};
  auto selected = bb::descendants(bbProject, bb::selectedTop(bbProject, bbSelection));
  for (auto &t : bbTriangles)
    if (selected.empty() || selected.count(t.element))
      for (auto v : t.p) {
        v = bb::bbToQuake(v, bbProject.front(), bbProject.exportScale());
        for (int a = 0; a < 3; a++) {
          low[a] = std::min(low[a], v[a]);
          high[a] = std::max(high[a], v[a]);
        }
      }
  if (low[0] <= high[0]) {
    auto center = bb::mul(bb::add(low, high), .5);
    double yaw = modelViewerYaw * bb::pi / 180, pitch = modelViewerPitch * bb::pi / 180,
           distance = std::max(16., bb::length(bb::sub(high, low)) * 1.4);
    auto eye = bb::sub(center, bb::mul({std::cos(pitch) * std::cos(yaw),
                                        std::cos(pitch) * std::sin(yaw), -std::sin(pitch)},
                                       distance));
    modelEditCameraEye = {float(eye[0]), float(eye[1]), float(eye[2])};
    modelEditCameraFree = true;
  }
}
// Panels declare selection helpers used by the viewport, then actions use both.
// clang-format off
#include "ftebroom_bb_ui_panels.h"
#include "ftebroom_bb_ui_viewport.h"
#include "ftebroom_bb_ui_actions.h"
// clang-format on

static bool bbBeforeDocument(const std::string &next) {
  if (!bbActive)
    return false;
  bbLeave([next] {
    bbActive = false;
    modelViewerOpen = modelEditorActive = false;
    cvars->SetFloat("ftebroom_bb_active", 0);
    modelEditorCameraEnd();
    bbReleaseImages();
    if (auto api = bbAPI())
      api->clear();
    documentRequest(next);
  });
  return true;
}

static bool bbCameraVisible() {
  return !bbActive || !(bbMode == 1 && bbPaint2D);
}
