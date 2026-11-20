// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ftebroom_child_process.h"
#include "texture/legacy_codec.h"
#include "texture/jpeg.h"
#include "texture/catalog.h"
#include "texture/layers.h"
#include "texture/package.h"
#include "texture/preview.h"
#include "texture/project_presets.h"
#include <thread>

namespace rbTexture = ftebroom::textureStudio;

static constexpr size_t studioPreviewCacheBudget = 12 * 1024 * 1024;

struct TextureStudioState {
  enum class Apply { none, asset, capture, existing, baking, verify, commit, model };
  enum Preview { flat, tiled, sphere, cube, target, surface };
  struct ModelTarget {
    uint64_t epoch = 0, revision = 0, textureRevision = 0;
    size_t index = 0;
    std::string id;
  };
  struct Result {
    unsigned revision = 0;
    bool fullSize = false;
    rbTexture::Resolution previewSize{};
    rbTexture::Evaluation image{};
    std::map<std::string,std::string> preview;
    std::vector<unsigned char> modelPixels;
  };
  struct SourceInput {
    rbTexture::Document document;
    rbTexture::Assets assets;
    std::filesystem::path source, folder;
    bool imported = false;
    std::string projectPreset;
    unsigned projectFrame = 0;
  };
  struct ProjectProgress {
    size_t total = 0;
    std::atomic<bool> cancel{false};
    std::atomic<size_t> finished{0};
  };
  struct ProjectBake {
    std::filesystem::path root;
    int profile = -1, mapEpoch = 0;
    std::string manifest, preset;
    rbTexture::ProjectPresetFiles files;
  };

  bool open = false, focus = false, focused = false, scheduled = false, fullSize = false;
  bool playing = false;
  bool previewUploadFailed = false;
  bool actualPixels = false, previewGrid = false, previewSeams = false;
  Preview previewMode = flat;
  rbTexture::PreviewCamera previewCamera;
  unsigned revision = 0, imageRevision = 0, frame = 0;
  double deadline = 0, nextRender = 0, nextAutosave = 0;
  double playStarted = 0;
  double nextSurfaceQuery = 0;
  uint32_t variationBase = 0;
  int handle = 0, slot = 0, ticket = 0, epoch = 0, profile = 0, selectedNode = 0;
  std::string channel = "baseColor", message, error, errorNode, material, wadReference;
  std::string savedSource, recoveredSource;
  std::string surfaceData;
  std::string movingNode;
  std::pair<std::string, std::string> wireStart;
  std::set<std::string> groupSelection;
  std::set<std::string> parameterLocks;
  std::array<char, 96> presetSearch{};
  int libraryFilter = 0;
  bool libraryLoaded = false;
  std::set<std::string> favorites;
  std::vector<std::string> recentPresets;
  std::vector<std::pair<std::string, std::string>> userPresets;
  std::future<std::pair<std::string, std::string>> thumbnailJob;
  std::map<std::string, int> thumbnails;
  std::filesystem::path file, destination, recoveryFile, assetFolder;
  std::vector<std::filesystem::path> recoverable;
  std::optional<rbTexture::History> document;
  ModelTarget modelTarget;
  std::optional<rbTexture::Document> drag;
  rbTexture::Assets assets;
  std::shared_ptr<std::atomic<bool>> cancel;
  std::shared_ptr<rbTexture::NodeCache> previewCache = std::make_shared<rbTexture::NodeCache>(studioPreviewCacheBudget);
  std::future<Result> job;
  std::shared_ptr<std::atomic<bool>> inputCancel;
  std::future<SourceInput> inputJob;
  std::optional<Result> result;
  std::filesystem::path projectRoot, projectDestination;
  int projectProfile = -1, projectFamily = -2;
  bool projectOffer = true, projectImported = false;
  bool docked = false;
  std::set<std::pair<int, std::filesystem::path>> promptedProjects;
  rbTexture::Json projectNames = rbTexture::Json::object();
  bool projectLegacy = false;
  std::string projectPreset;
  std::shared_ptr<ProjectProgress> projectProgress;
  std::future<ProjectBake> projectJob;
  Apply apply = Apply::none;
};

static TextureStudioState textureStudio;

static bool textureStudioFocused() {
  const auto& s = textureStudio;
  return s.open && (s.focused || s.focus) && !jamEditing();
}

static bool textureStudioDocked() {
  return textureStudio.docked && !bbActive;
}

static const rbTexture::Document& textureStudioDoc() {
  return textureStudio.drag ? *textureStudio.drag : textureStudio.document->get();
}

static std::filesystem::path studioRecoveryRoot() {
  return documentPath("data/texture-studio/recovery");
}

static void studioWriteOriginal(const std::filesystem::path& folder, const std::string& relative,
                                const std::string& bytes) {
  namespace fs = std::filesystem;
  const auto root = fs::canonical(folder.empty() ? "." : folder);
  const auto target = root / fs::u8path(relative);
  rbTexture::require(fs::weakly_canonical(target) == target, "assets",
    "The original image destination points through a filesystem link.");
  if (fs::exists(target)) {
    rbTexture::require(rbTexture::imageFileBytes(target) == bytes, "assets",
      "A different file already uses the original image's name.");
    return;
  }
  fs::create_directories(target.parent_path());
  ftebroom::bb::atomicWrite(target, bytes);
}

static void studioCopyOriginals(const std::filesystem::path& destination) {
  for (const auto& reference : textureStudioDoc().source.at("assetReferences")) {
    const auto path = rbTexture::imageRelativePath(rbTexture::idField(reference, "sha256", "assets"));
    studioWriteOriginal(destination, path, rbTexture::readOriginal(textureStudio.assetFolder, reference));
  }
}

static void studioRecoverableFiles() {
  auto& s = textureStudio;
  s.recoverable.clear();
  const auto root = studioRecoveryRoot();
  if (!std::filesystem::is_directory(root)) return;
  for (const auto& item : std::filesystem::directory_iterator(root)) {
    if (s.recoverable.size() == 64) break;
    if (item.is_regular_file() && !item.is_symlink() &&
        item.path().filename().u8string().find(".rbtex.json") != std::string::npos &&
        item.file_size() <= rbTexture::maxSourceBytes)
      s.recoverable.push_back(item.path());
  }
  std::sort(s.recoverable.begin(), s.recoverable.end());
}

static void studioAutosave() {
  auto& s = textureStudio;
  s.nextAutosave = ImGui::GetTime() + 60;
  const auto& doc = textureStudioDoc();
  if (doc.readOnly) return;
  const auto source = doc.save();
  if (source == s.savedSource || source == s.recoveredSource) return;
  if (s.recoveryFile.empty())
    s.recoveryFile = studioRecoveryRoot() / (ftebroom::bb::uuid() + ".rbtex.json");
  std::filesystem::create_directories(s.recoveryFile.parent_path());
  studioCopyOriginals(s.recoveryFile.parent_path());
  ftebroom::bb::atomicWrite(s.recoveryFile, source);
  s.recoveredSource = source;
}

static void studioSchedule(bool full = false) {
  auto& s = textureStudio;
  if (s.cancel) *s.cancel = true;
  if (s.document) {
    if (textureStudioDoc().readOnly) {
      s.scheduled = false;
      return;
    }
    s.frame = std::min(s.frame, textureStudioDoc().source.at("animation").at("frames").get<unsigned>() - 1);
  }
  ++s.revision;
  s.error.clear();
  s.errorNode.clear();
  s.scheduled = true;
  s.fullSize = full;
  s.nextRender = ImGui::GetTime() + (full ? 0 : .10);
}

static void studioEdit(rbTexture::Document document) {
  auto& s = textureStudio;
  s.errorNode.clear();
  try {
    rbTexture::compile(document, {}, s.assets);
  } catch (const rbTexture::Error& error) {
    constexpr std::string_view prefix = "graph.nodes.";
    std::string_view location = error.location;
    if (location.substr(0, prefix.size()) == prefix) {
      location.remove_prefix(prefix.size());
      for (const auto& node : document.source.at("graph").at("nodes")) {
        const auto& id = node.at("id").get_ref<const std::string&>();
        if (location.substr(0, id.size()) == id && (location.size() == id.size() || location[id.size()] == '.') &&
            id.size() > s.errorNode.size()) s.errorNode = id;
      }
    }
    throw;
  }
  textureStudio.document->replace(std::move(document));
  textureStudio.drag.reset();
  studioSchedule();
  textureStudio.nextRender = ImGui::GetTime();
}

static void studioNewDocument(rbTexture::Document document) {
  auto& s = textureStudio;
  if (s.inputCancel) *s.inputCancel = true;
  s.file.clear();
  s.projectPreset.clear();
  s.drag.reset();
  s.movingNode.clear();
  s.wireStart = {};
  s.groupSelection.clear();
  s.parameterLocks.clear();
  s.document.emplace(std::move(document));
  s.assets.clear();
  s.assetFolder.clear();
  s.savedSource = s.document->get().save();
  s.recoveredSource.clear();
  s.recoveryFile.clear();
  s.nextAutosave = ImGui::GetTime() + 60;
  const bool readOnly = s.document->get().readOnly;
  s.variationBase = readOnly ? 0 : s.document->get().seed();
  s.selectedNode = 0;
  s.frame = 0;
  s.playing = false;
  s.channel = "baseColor";
  if (!readOnly) studioSchedule();
}

static void studioNew(unsigned preset = 0) {
  studioNewDocument(rbTexture::builtinPreset(preset, ftebroom::bb::uuid()));
  textureStudio.projectPreset = rbTexture::projectPresets().at(preset).key;
}

static void textureStudioShow() {
  if (jamEditing()) return;
  auto& s = textureStudio;
  if (!s.document) studioNew();
  else if (!s.open && !s.result) studioSchedule();
  s.open = true;
  s.focus = true;
}

static void textureStudioCommand() { textureStudioShow(); }

static void studioCancel(bool report = false) {
  auto& s = textureStudio;
  s.playing = false;
  if (!s.movingNode.empty()) s.drag.reset();
  s.movingNode.clear();
  s.wireStart = {};
  s.groupSelection.clear();
  if (s.inputCancel) *s.inputCancel = true;
  if (s.cancel) *s.cancel = true;
  if (s.projectProgress) s.projectProgress->cancel = true;
  ++s.revision;
  s.scheduled = false;
  if (s.apply != TextureStudioState::Apply::commit) s.apply = TextureStudioState::Apply::none;
  if (report) s.message = "Generation cancelled.";
}

static void studioReleaseImage() {
  auto& s = textureStudio;
  if (s.handle) draw->UnloadImage(s.handle);
  s.handle = 0;
  s.previewUploadFailed = false;
}

static void studioUpload() {
  auto& s = textureStudio;
  if (!s.result) return;
  const auto subject = s.previewMode >= TextureStudioState::sphere ? "material-preview" : s.channel;
  auto preview = s.result->preview.find(subject);
  if (preview == s.result->preview.end()) return;
  auto& png = preview->second;
  studioReleaseImage();
  s.slot ^= 1;
  const std::string path = "ftebroom/texture-studio-preview-" + std::to_string(s.slot) + ".png";
  draw->LoadImageData(path.c_str(), "image/png", png.data(), png.size());
  s.handle = draw->LoadImage(path.c_str());
  if (s.handle) s.imageRevision = s.result->revision;
  else s.previewUploadFailed = true;
}

static void studioVideoChanged() {
  auto& s = textureStudio;
  // Renderer-owned handles expired. Keep the CPU result and any in-flight bake.
  s.handle = 0;
  s.previewUploadFailed = false;
  s.thumbnails.clear();
  if (s.open && s.result && s.result->revision == s.revision) studioUpload();
}


static bool studioProfileAllowsApply() { return gameProfile >= 0 && gameProfile <= 4; }

static std::filesystem::path studioDestination() {
  auto root = modFileRoot();
  if (gameProfile == 1 && (root.empty() || !std::filesystem::is_directory(root)))
    return std::filesystem::u8path(baseq3);
  return root;
}

static bool studioContextMatches() {
  const auto& s = textureStudio;
  return !jamEditing() && s.epoch == organizationMapEpoch && s.profile == gameProfile &&
    s.destination == studioDestination();
}

static void studioQCSend(const char* step, const std::string& material = {}) {
  auto& s = textureStudio;
  cvars->SetFloat("ftebroom_texture_apply_ack", 0);
  s.deadline = ImGui::GetTime() + 4;
  std::string cmd = "ftebroom_texture_apply " + std::string(step) + " " + std::to_string(s.ticket);
  if (!material.empty()) cmd += " " + organizationQuote(material);
  if (!material.empty() && !s.wadReference.empty()) cmd += " " + organizationQuote(s.wadReference);
  command((cmd + "\n").c_str());
}

static void studioBeginApply(bool assetOnly, const std::string& installedMaterial = {},
                           const std::string& installedWad = {}) {
  auto& s = textureStudio;
  s.playing = false;
  if (!studioProfileAllowsApply()) throw std::runtime_error("Apply for this profile needs a profile-specific encoder.");
  if (!assetOnly && documentFile.empty()) throw std::runtime_error("Open a project map before applying a texture.");
  s.destination = studioDestination();
  if (s.destination.empty() || !std::filesystem::is_directory(s.destination))
    throw std::runtime_error("Choose a writable game or mod folder in Game profile.");
  studioAutosave();
  s.epoch = organizationMapEpoch;
  s.profile = gameProfile;
  s.error.clear();
  if (assetOnly) {
    s.apply = TextureStudioState::Apply::asset;
    studioSchedule(true);
    return;
  }
  s.apply = installedMaterial.empty() ? TextureStudioState::Apply::capture : TextureStudioState::Apply::existing;
  s.material = installedMaterial;
  s.wadReference = installedWad;
  s.ticket = s.ticket % 1000000 + 1;
  studioQCSend("capture");
}

static bool studioModelSlotAvailable() {
  return bbActive && bbTexture >= 0 && size_t(bbTexture) < bbProject.textures.size() && !bbHistory.active();
}

static constexpr size_t studioModelAssetLimit = 16 * 1024 * 1024;

static void studioBeginModelApply() {
  if (!studioModelSlotAvailable()) throw std::runtime_error("Select a texture in the Model Editor.");
  auto& s = textureStudio;
  s.playing = false;
  const auto size = textureStudioDoc().resolution();
  rbTexture::require(size.width <= 2048 && size.height <= 2048, "resolution",
    "Model texture slots support up to 2048 × 2048.");
  studioAutosave();
  const auto index = size_t(bbTexture);
  const auto& slot = bbProject.textures[index];
  s.modelTarget = {bbDocumentEpoch, bbProject.revision, slot.revision, index, slot.id};
  s.apply = TextureStudioState::Apply::model;
  studioSchedule(true);
}

static void studioApplyModel() {
  auto& s = textureStudio;
  const auto& target = s.modelTarget;
  if (!studioModelSlotAvailable() || bbDocumentEpoch != target.epoch || bbProject.revision != target.revision
      || size_t(bbTexture) != target.index || bbProject.textures[target.index].id != target.id
      || bbProject.textures[target.index].revision != target.textureRevision)
    throw std::runtime_error("The model or selected texture changed. Apply to the model was cancelled.");
  if (!s.result || s.result->revision != s.revision || !s.result->fullSize) return;
  auto& result = *s.result;
  auto& slot = bbProject.textures[target.index];
  const auto& graph = textureStudioDoc();
  auto originals = rbTexture::Json::object();
  size_t retainedBytes = 0;
  for (const auto& reference : graph.source.at("assetReferences")) {
    const auto hash = rbTexture::idField(reference, "sha256", "assets");
    if (originals.contains(hash)) continue;
    const auto bytes = rbTexture::readOriginal(s.assetFolder, reference);
    rbTexture::require(bytes.size() <= studioModelAssetLimit - retainedBytes, "assets",
      "A model material can retain up to 16 MiB of original images. Reduce the source size before applying it.");
    retainedBytes += bytes.size();
    originals[hash] = ftebroom::bb::base64(bytes);
  }
  size_t imageBytes = result.modelPixels.size();
  for (size_t i = 0; i < bbProject.textures.size(); ++i)
    if (i != target.index) imageBytes += bbProject.textures[i].rgba.size();
  rbTexture::require(imageBytes <= 256 * 1024 * 1024, "model", "The model would exceed its texture memory budget.");
  const auto& image = *result.image.outputs.at("baseColor");
  bbHistory.begin(bbProject, bbSelection, {target.index});
  try {
    auto& meta = bbProject.root["textures"][target.index];
    meta["reyond_texture_studio"] = graph.source;
    meta["reyond_texture_studio_frame"] = s.frame;
    if (originals.empty()) meta.erase("reyond_texture_studio_originals");
    else meta["reyond_texture_studio_originals"] = std::move(originals);
    slot.width = int(image.size.width);
    slot.height = int(image.size.height);
    slot.rgba = std::move(result.modelPixels);
    slot.path.clear();
    slot.saved = false;
    ++slot.revision;
    bbHistory.finish(bbProject, bbSelection, "Apply Texture Studio to model");
  } catch (...) {
    bbHistory.cancel(bbProject, bbSelection);
    throw;
  }
  s.apply = TextureStudioState::Apply::none;
  s.message = "Texture applied to " + slot.name + ". Undo restores the previous image.";
}

static std::string studioGoldSrcPalette() {
  std::string palette;
  palette.reserve(768);
  for (unsigned index = 0; index < 256; ++index) {
    palette += char((index >> 5) * 255 / 7);
    palette += char(((index >> 2) & 7) * 255 / 7);
    palette += char((index & 3) * 255 / 3);
  }
  return palette;
}

#include "ftebroom_texture_project.h"

static rbTexture::Image studioTargetPreview(const rbTexture::Image& image, int profile, const std::string& palette) {
  if (palette.empty()) return image;
  const auto format = profile == 2 ? rbTexture::IndexedFormat::wad2 :
    profile == 3 ? rbTexture::IndexedFormat::wal : rbTexture::IndexedFormat::wad3;
  const auto encoding = profile == 2 ? "wad2" : profile == 3 ? "wal" : "wad3";
  const auto container = rbTexture::indexed(image, "preview", palette, format);
  const auto raster = rbTexture::decodeOriginal(container,
    rbTexture::Json{{"encoding", encoding}, {"palette", rbTexture::writePalette(palette)}});
  rbTexture::Image result{raster->size, rbTexture::Port::color, {}};
  result.pixels.reserve(image.pixels.size());
  for (unsigned y = 0; y < result.size.height; ++y)
    for (unsigned x = 0; x < result.size.width; ++x)
      result.pixels.push_back(rbTexture::rasterPixel(*raster, int(x), int(y)));
  return result;
}

static rbTexture::Image studioSurfacePreview(const rbTexture::Image& color, rbTexture::Resolution textureSize,
                                               const std::string& data) {
  struct Corner { float x, y, s, t; };
  std::istringstream stream(data);
  stream.imbue(std::locale::classic());
  std::vector<Corner> face;
  size_t count = 0;
  rbTexture::require(bool(stream >> count) && count >= 3 && count <= 48, "preview", "Select a brush face to preview its texture projections.");
  for (size_t index = 0; index < count; ++index) {
    Corner corner{};
    rbTexture::require(bool(stream >> corner.x >> corner.y >> corner.s >> corner.t) &&
      std::isfinite(corner.x) && std::isfinite(corner.y) &&
      std::isfinite(corner.s) && std::isfinite(corner.t), "preview", "The selected face has invalid geometry.");
    face.push_back(corner);
  }
  auto low = face.front(), high = face.front();
  for (const auto& corner : face) {
    low.x = std::min(low.x, corner.x); low.y = std::min(low.y, corner.y);
    high.x = std::max(high.x, corner.x); high.y = std::max(high.y, corner.y);
  }
  const auto cross = [](float ax, float ay, float bx, float by) { return ax * by - ay * bx; };
  const auto a = face[0], b = face[1];
  size_t third = 2;
  float determinant = 0;
  for (; third < face.size(); ++third) {
    determinant = cross(b.x - a.x, b.y - a.y, face[third].x - a.x, face[third].y - a.y);
    if (std::abs(determinant) > .0001f) break;
  }
  rbTexture::require(third < face.size(), "preview", "The selected face has no area.");
  const auto c = face[third];
  constexpr unsigned edge = 256;
  const float scale = std::max(high.x - low.x, high.y - low.y) * 1.12f;
  rbTexture::require(std::isfinite(scale) && scale > 0 && scale <= 1e8f, "preview", "The selected face extent is unsupported.");
  rbTexture::Image result{{edge, edge}, rbTexture::Port::color, std::vector<rbTexture::Pixel>(edge * edge)};
  for (unsigned y = 0; y < edge; ++y) {
    const float py = (low.y + high.y) * .5f + (.5f - (float(y) + .5f) / edge) * scale;
    for (unsigned x = 0; x < edge; ++x) {
      const float px = (low.x + high.x) * .5f + ((float(x) + .5f) / edge - .5f) * scale;
      bool inside = true;
      for (size_t i = 0; i < face.size(); ++i) {
        const auto& p = face[i];
        const auto& q = face[(i + 1) % face.size()];
        if (cross(q.x - p.x, q.y - p.y, px - p.x, py - p.y) * determinant < 0) {
          inside = false;
          break;
        }
      }
      if (!inside) continue;
      const float wb = cross(px - a.x, py - a.y, c.x - a.x, c.y - a.y) / determinant;
      const float wc = cross(b.x - a.x, b.y - a.y, px - a.x, py - a.y) / determinant;
      const float s = (a.s + (b.s - a.s) * wb + (c.s - a.s) * wc) / textureSize.width;
      const float t = (a.t + (b.t - a.t) * wb + (c.t - a.t) * wc) / textureSize.height;
      rbTexture::require(std::isfinite(s) && std::isfinite(t), "preview", "The selected face has invalid texture coordinates.");
      result.pixels[y * edge + x] = rbTexture::sampleColor(color, s, t);
    }
  }
  return result;
}

static void studioInstall() {
  auto& s = textureStudio;
  const auto& graph = textureStudioDoc();
  const auto source = graph.save();
  const auto digest = ftebroom::jam::sha256(source);
  const int profile = s.profile;
  const bool wad = rbTexture::projectUsesWad(profile);
  const auto palette = profile == 2 ? q1Palette() : profile == 3 ? q2Palette() :
    profile == 4 ? studioGoldSrcPalette() : std::string{};
  const auto paletteHash = palette.empty() ? std::string{} : ftebroom::sha256(palette);
  const auto key = ftebroom::sha256(digest + "/frame/" + std::to_string(s.frame) + "/palette/" + paletteHash);
  const auto root = std::filesystem::canonical(s.destination);
  const auto previous = studioProjectManifestBytes(root, profile);
  auto library = studioParseProjectManifest(previous, profile);
  const bool legacy = studioLegacyCatalog(library);
  library["legacyCatalog"] = legacy;
  rbTexture::require(library.value("palette", paletteHash) == paletteHash, "project",
    "The game palette changed since the materials were imported.");
  auto generated = library.value("generated", rbTexture::Json::object());
  const auto archive = rbTexture::projectPresetWad(profile);
  const auto folder = std::string(wad ? "texture-wads/" : "textures/reyond-studio/");
  rbTexture::ProjectPresetFiles lumps;
  auto id = generated.value(key, std::string{});
  std::string unfinishedLump;
  const auto pendingMaterialMatches = [&](const std::string& name) {
    const auto path = studioProjectPath(root, folder + name + ".reyond.json");
    if (!std::filesystem::is_regular_file(path)) return false;
    const auto saved = rbTexture::parseJson(ftebroom::bb::readFile(path, 65536));
    return saved.value("sourceHash", "") == digest && saved.value("frame", 0u) == s.frame &&
      saved.value("paletteSha256", "") == paletteHash;
  };
  if (wad) {
    const auto archivePath = studioProjectPath(root, archive);
    if (std::filesystem::is_regular_file(archivePath)) {
      const auto bytes = ftebroom::bb::readFile(archivePath);
      const auto committedHash = library.at("files").value(archive, std::string{});
      lumps = rbTexture::projectWadLumps(bytes, profile);
      if (ftebroom::sha256(bytes) != committedHash) {
        // A previous Apply may have installed the WAD just before the editor stopped.
        for (const auto& [name, lump] : lumps) {
          if (!rbTexture::projectNumberedName(name) || !pendingMaterialMatches(name)) continue;
          auto committed = lumps;
          committed.erase(name);
          const bool original = committedHash.empty() ? committed.empty() :
            ftebroom::sha256(ftebroom::quake1::mipWad(committed, profile == 4)) == committedHash;
          if (!original) continue;
          id = name;
          unfinishedLump = lump;
          break;
        }
        rbTexture::require(!unfinishedLump.empty(), "project",
          "The Texture Studio WAD changed outside the editor.");
      }
    }
  }
  const auto nameExists = [&](const std::string& name) {
    return wad ? lumps.count(name) != 0 : std::filesystem::is_regular_file(studioProjectPath(root,
      rbTexture::projectPresetImagePath(profile, rbTexture::projectMaterialPath(profile, name))));
  };
  s.wadReference = wad && s.apply != TextureStudioState::Apply::asset ? studioProjectWadReference(root, profile) : "";
  if (unfinishedLump.empty() && rbTexture::projectNumberedName(id) && nameExists(id)) {
    const auto material = "textures/" + rbTexture::projectMaterialPath(profile, id);
    const auto installed = [&](const char* suffix) {
      const auto relative = material + suffix;
      return library.at("files").contains(relative) &&
        std::filesystem::is_regular_file(studioProjectPath(root, relative));
    };
    if (profile != 1 || (installed("_norm.tga") && installed("_gloss.tga"))) {
      s.material = material;
      return;
    }
  }
  if (!rbTexture::projectNumberedName(id)) {
    const auto category = rbTexture::projectMaterialWord(graph.source.value("name", std::string{}));
    auto used = studioProjectMaterialInventory();
    for (const auto& [name, content] : lumps) used.insert("textures/" + name);
    for (const auto& name : generated) if (name.is_string())
      used.insert("textures/" + rbTexture::projectMaterialPath(profile, name.get<std::string>()));
    for (;;) {
      id = studioNextMaterialName(profile, category, used);
      used.insert("textures/" + rbTexture::projectMaterialPath(profile, id));
      const auto pending = studioProjectPath(root, folder + id + ".reyond.json");
      if (std::filesystem::exists(pending)) {
        if (pendingMaterialMatches(id)) break;
      } else if (!nameExists(id)) break;
    }
  }
  generated[key] = id;
  library["generated"] = std::move(generated);
  library["palette"] = paletteHash;
  const auto name = rbTexture::projectMaterialPath(profile, id);
  s.material = "textures/" + name;
  TextureStudioState::ProjectBake bake{root, profile, s.epoch, previous, id, {}};
  rbTexture::projectEncodeMaterial(bake.files, lumps, profile, name, s.result->image, palette);
  rbTexture::require(unfinishedLump.empty() || unfinishedLump == lumps.at(name), "project",
    "The interrupted Texture Studio WAD entry does not match the regenerated texture.");
  const auto hash = ftebroom::sha256(wad ? lumps.at(name) :
    bake.files.at(rbTexture::projectPresetImagePath(profile, name)));
  const auto sidecar = rbTexture::Json{
    {"version", 1}, {"generatorVersion", rbTexture::generatorVersion}, {"profile", gameProfileName()},
    {"material", s.material}, {"sourceHash", digest}, {"frame", s.frame}, {"paletteSha256", paletteHash},
    {"identity", ftebroom::jam::sha256(rbTexture::renderIdentity(graph))}, {"runtimeHash", hash}};
  bake.files.emplace(folder + id + ".rbtex.json", source);
  bake.files.emplace(folder + id + ".reyond.json", sidecar.dump(2) + "\n");
  for (const auto& reference : graph.source.at("assetReferences")) {
    const auto sha = rbTexture::idField(reference, "sha256", "assets");
    bake.files.emplace(folder + rbTexture::imageRelativePath(sha), rbTexture::readOriginal(s.assetFolder, reference));
  }
  if (wad) bake.files.emplace(archive, ftebroom::quake1::mipWad(lumps, profile == 4));
  if (profile <= 1)
    bake.files.emplace("scripts/" + rbTexture::projectPresetShaders(profile) + ".shader",
      studioProjectShaderScript(profile, library));
  for (const auto& [path, bytes] : bake.files) library["files"][path] = ftebroom::sha256(bytes);
  bake.files.emplace(studioProjectManifestLocation(profile), library.dump(2) + "\n");
  studioInstallProjectBake(bake);
  if (s.projectRoot == root) studioProjectStatus(library);
  if (wad) {
    q1IndexAssets();
    resetMaterialBrowser();
    publishMaterialTags();
    cvars->SetFloat("ftebroom_material_count", float(materials.size()));
  } else command("fs_restart\nftebroom_assets_ready\n");
}

static void studioApplyTick() {
  auto& s = textureStudio;
  using A = TextureStudioState::Apply;
  if (s.apply == A::none) return;
  if (s.apply == A::model) {
    studioApplyModel();
    return;
  }
  if (!studioContextMatches() && s.apply != A::commit)
    throw std::runtime_error("The map or game profile changed. Apply was cancelled.");
  if (s.apply == A::asset) {
    if (s.result && s.result->revision == s.revision && s.result->fullSize) {
      studioInstall();
      s.apply = A::none;
      s.message = "Created " + s.material + ".";
    }
    return;
  }
  if (s.apply == A::baking) {
    if (s.result && s.result->revision == s.revision && s.result->fullSize) {
      s.apply = A::verify;
      studioQCSend("check");
    }
    return;
  }
  const bool ack = int(cvars->GetFloat("ftebroom_texture_apply_ack")) == s.ticket;
  if (!ack && ImGui::GetTime() < s.deadline) return;
  if (!ack && s.apply == A::commit) {
    throw std::runtime_error("Apply was not acknowledged. The generated asset remains available.");
  }
  char value[80]{};
  if (ack) cvars->GetString("ftebroom_texture_apply_state", value, sizeof value);
  const std::string status(value);
  if (s.apply == A::existing && status == "captured") {
    s.apply = A::commit;
    const auto material = s.profile == 1 ? s.material : s.material.substr(sizeof("textures/") - 1);
    studioQCSend("commit", material);
  } else if (s.apply == A::capture && status == "captured") {
    s.apply = A::baking;
    studioSchedule(true);
  } else if (s.apply == A::verify && status == "checked") {
    studioInstall();
    s.apply = A::commit;
    // Quake II and III resolve relative paths. Quake and GoldSrc use bare WAD names.
    const auto faceMaterial = s.profile == 1 ? s.material : s.material.substr(sizeof("textures/") - 1);
    studioQCSend("commit", faceMaterial);
  } else if (s.apply == A::commit && (status == "applied" || status == "unchanged")) {
    s.apply = A::none;
    s.message = "Material applied. Undo restores the previous face materials.";
  } else {
    throw std::runtime_error("Apply stopped. " + (status.empty() ? "The map did not acknowledge the operation." : status + ". Select the faces again."));
  }
}

static void studioTick() {
  auto& s = textureStudio;
  try {
    if (s.job.valid() && s.job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      try {
        auto result = s.job.get();
        if (result.revision == s.revision && s.open) {
          s.result = std::move(result);
          if (!s.result->image.outputs.count(s.channel))
            s.channel = s.result->image.outputs.begin()->first;
          studioUpload();
        }
      } catch (...) {
        if (!s.cancel || !s.cancel->load()) throw;
      }
    }
    studioApplyTick();
    if (!s.job.valid() && s.open && s.scheduled && ImGui::GetTime() >= s.nextRender) {
      auto doc = textureStudioDoc();
      const auto revision = s.revision;
      const auto frame = s.frame;
      const auto full = s.fullSize;
      const auto mode = s.previewMode;
      const auto camera = s.previewCamera;
      const auto targetProfile = gameProfile;
      const bool model = s.apply == TextureStudioState::Apply::model;
      s.scheduled = false;
      std::string targetPalette;
      if (mode == TextureStudioState::target) {
        if (targetProfile == 2) targetPalette = q1Palette();
        else if (targetProfile == 3) targetPalette = q2Palette();
        else if (targetProfile == 4) targetPalette = studioGoldSrcPalette();
      }
      s.cancel = std::make_shared<std::atomic<bool>>(false);
      s.job = std::async(std::launch::async, [doc = std::move(doc), assets = s.assets, revision, full, model, frame, mode, camera,
          targetProfile, palette = std::move(targetPalette), surfaceData = s.surfaceData,
          stop = s.cancel, cache = s.previewCache]() {
        const auto size = doc.resolution();
        auto previewSize = size;
        const auto longest = std::max(size.width, size.height);
        if (longest > 256)
          previewSize = {std::max(1u, size.width * 256 / longest), std::max(1u, size.height * 256 / longest)};
        const auto plan = rbTexture::compile(doc, {}, assets);
        auto preview = rbTexture::evaluate(plan, previewSize, stop.get(), rbTexture::memoryBudget, 1, frame, cache.get());
        std::map<std::string,std::string> pngs;
        for (const auto& [channel, image] : preview.outputs)
          pngs.emplace(channel, rbTexture::png(*image));
        if (preview.outputs.count("baseColor")) {
          if (mode == TextureStudioState::sphere || mode == TextureStudioState::cube)
            pngs.emplace("material-preview", rbTexture::png(rbTexture::materialPreview(preview,
              mode == TextureStudioState::cube, stop.get(), camera)));
          else if (mode == TextureStudioState::target)
            pngs.emplace("material-preview", rbTexture::png(studioTargetPreview(
              *preview.outputs.at("baseColor"), targetProfile, palette)));
          else if (mode == TextureStudioState::surface && !surfaceData.empty())
            pngs.emplace("material-preview", rbTexture::png(studioSurfacePreview(
              *preview.outputs.at("baseColor"), size, surfaceData)));
        }
        const unsigned workers = std::clamp(std::thread::hardware_concurrency(), 1u, 4u);
        auto result = full && longest > 256
          ? rbTexture::evaluate(plan, size, stop.get(), rbTexture::memoryBudget, workers, frame) : std::move(preview);
        std::vector<unsigned char> modelPixels;
        if (model) modelPixels = rbTexture::rgba8(*result.outputs.at("baseColor"));
        return TextureStudioState::Result{revision, full, previewSize, std::move(result), std::move(pngs), std::move(modelPixels)};
      });
    }
  } catch (const std::exception& e) {
    s.error = e.what();
    s.apply = TextureStudioState::Apply::none;
  }
  if (!s.inputJob.valid() || s.inputJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
  const bool cancelled = *s.inputCancel;
  s.inputCancel.reset();
  try {
    auto input = s.inputJob.get();
    if (cancelled || !s.open) return;
    studioAutosave();
    studioCancel();
    studioNewDocument(std::move(input.document));
    if (input.projectPreset.empty()) s.file = std::move(input.source);
    s.projectPreset = std::move(input.projectPreset);
    if (!s.projectPreset.empty()) {
      s.frame = std::min(input.projectFrame, textureStudioDoc().source.at("animation").at("frames").get<unsigned>() - 1);
      studioSchedule();
    }
    s.assetFolder = std::move(input.folder);
    s.assets = std::move(input.assets);
    s.message.clear();
    if (input.imported) {
      s.savedSource.clear();
      studioAutosave();
    }
  } catch (const std::exception& e) {
    if (!cancelled && s.open) s.error = e.what();
  }
}

static void studioSave(const std::filesystem::path& path) {
  auto& s = textureStudio;
  const auto& doc = textureStudioDoc();
  if (!doc.readOnly) {
    rbTexture::compile(doc, {}, s.assets);
    studioCopyOriginals(path.parent_path());
  }
  if (std::filesystem::is_regular_file(path))
    ftebroom::bb::atomicWrite(std::filesystem::u8path(path.u8string() + ".bak"),
      ftebroom::bb::readFile(path, rbTexture::maxSourceBytes));
  ftebroom::bb::atomicWrite(path, doc.save());
  s.file = path;
  if (!doc.readOnly) s.assetFolder = path.parent_path();
  s.savedSource = doc.save();
  if (!s.recoveryFile.empty()) {
    std::error_code error;
    std::filesystem::remove(s.recoveryFile, error);
    if (error) s.error = "Source saved. Its recovery copy could not be removed.";
    else s.recoveryFile.clear();
  }
  s.recoveredSource.clear();
  s.message = "Saved " + path.filename().u8string();
}

static void studioAction(const std::function<void()>& action) {
  auto& s = textureStudio;
  try {
    action();
  } catch (const std::exception& e) {
    s.error = e.what();
  }
}

static std::string studioMaterialId(std::string_view material) {
  constexpr std::string_view prefix = "textures/";
  if (material.substr(0, prefix.size()) == prefix) material.remove_prefix(prefix.size());
  const bool indexed = gameProfile == 2 || gameProfile == 4;
  if (indexed) {
    if (rbTexture::projectNumberedName(material)) return std::string(material);
    if (material.size() != 15 || material.substr(0, 3) != "rb_") return {};
  } else {
    constexpr std::string_view folder = "reyond-studio/";
    if (material.substr(0, folder.size()) != folder) return {};
    material.remove_prefix(folder.size());
    if (rbTexture::projectNumberedName(material)) return std::string(material);
    if (material.size() != (gameProfile == 3 ? 16 : 20)) return {};
  }
  const auto digest = material.substr(indexed ? 3 : 0);
  if (digest.find_first_not_of("0123456789abcdef") != std::string_view::npos) return {};
  return std::string(material);
}

static void studioChooseProjectPreset(const rbTexture::ProjectPreset& preset);

static bool textureStudioHasMaterialSource(std::string_view material) {
  if (jamEditing() || !studioProfileAllowsApply()) return false;
  try {
    studioRefreshProjectScope();
    if (studioProjectPresetForMaterial(material)) return true;
    const auto id = studioMaterialId(material);
    if (id.empty() || textureStudio.projectRoot.empty()) return false;
    const auto folder = rbTexture::projectUsesWad(gameProfile) ? "texture-wads/" : "textures/reyond-studio/";
    return std::filesystem::is_regular_file(studioProjectPath(textureStudio.projectRoot, folder + id + ".rbtex.json"));
  } catch (const std::exception&) { return false; }
}

static void textureStudioShowMaterialSource(std::string_view material) {
  if (!textureStudioHasMaterialSource(material)) return;
  textureStudioShow();
  studioAction([&] {
    auto& s = textureStudio;
    rbTexture::require(s.apply == TextureStudioState::Apply::none, "source", "Finish or cancel the current bake.");
    if (const auto* preset = studioProjectPresetForMaterial(material)) {
      studioChooseProjectPreset(*preset);
      return;
    }
    const auto id = studioMaterialId(material);
    const bool indexed = gameProfile == 2 || gameProfile == 4;
    const auto root = std::filesystem::canonical(studioDestination());
    const auto folder = root / (indexed ? "texture-wads" : "textures/reyond-studio");
    const auto source = folder / (id + ".rbtex.json");
    const auto sidecar = folder / (id + ".reyond.json");
    rbTexture::require(std::filesystem::weakly_canonical(source) == source &&
      std::filesystem::weakly_canonical(sidecar) == sidecar &&
      std::filesystem::is_regular_file(source) && std::filesystem::is_regular_file(sidecar), "source",
      "Keep the material's .rbtex.json and .reyond.json files beside its baked texture.");
    const auto manifest = rbTexture::parseJson(ftebroom::bb::readFile(sidecar, 65536));
    const auto bytes = ftebroom::bb::readFile(source, rbTexture::maxSourceBytes);
    const auto expected = "textures/" + std::string(indexed ? "" : "reyond-studio/") + id;
    rbTexture::require(manifest.value("material", "") == expected &&
      ftebroom::sha256(bytes) == manifest.value("sourceHash", ""), "source",
      "The saved source checksum differs from the texture manifest.");
    auto doc = rbTexture::open(bytes);
    rbTexture::require(!doc.readOnly, "source", "This material needs a newer Texture Studio.");
    auto assets = rbTexture::loadAssets(doc, folder);
    const auto plan = rbTexture::compile(doc, {}, assets);
    const auto frame = std::min(manifest.value("frame", 0u), plan.frames - 1u);
    studioAutosave();
    studioNewDocument(std::move(doc));
    s.assetFolder = folder;
    s.assets = std::move(assets);
    s.frame = frame;
    s.message = "Opened the source of " + expected + ". Save As creates an editable copy.";
  });
}

static void textureStudioShowModelTexture() {
  textureStudioShow();
  if (jamEditing() || !bbActive || bbTexture < 0 || size_t(bbTexture) >= bbProject.textures.size()) return;
  const auto entries = bbProject.root.find("textures");
  if (entries == bbProject.root.end() || !entries->is_array() || size_t(bbTexture) >= entries->size()) return;
  const auto& metadata = (*entries)[bbTexture];
  if (!metadata.contains("reyond_texture_studio")) return;
  studioAction([&] {
    auto doc = rbTexture::open(metadata.at("reyond_texture_studio").dump());
    rbTexture::require(!doc.readOnly, "source", "This model material needs a newer Texture Studio.");
    const auto& refs = doc.source.at("assetReferences");
    const auto& embedded = metadata.value("reyond_texture_studio_originals", rbTexture::Json::object());
    rbTexture::require(embedded.is_object() && refs.size() <= 64 && embedded.size() <= refs.size(),
      "assets", "A model material is missing its original images.");
    const auto folder = studioRecoveryRoot();
    std::filesystem::create_directories(folder);
    size_t retainedBytes = 0;
    std::set<std::string> restored;
    for (const auto& record : refs) {
      const auto hash = rbTexture::idField(record, "sha256", "assets");
      if (!restored.insert(hash).second) continue;
      const auto& original = rbTexture::member(embedded, hash.c_str(), "assets");
      rbTexture::require(original.is_string() && original.get_ref<const std::string&>().size() <=
        ((studioModelAssetLimit - retainedBytes + 2) / 3) * 4,
        "assets", "The model's encoded source exceeds its 16 MiB budget.");
      const auto bytes = ftebroom::bb::unbase64(original.get_ref<const std::string&>());
      rbTexture::require(bytes.size() <= studioModelAssetLimit - retainedBytes && ftebroom::sha256(bytes) == hash,
        "assets", "The model's original image checksum is invalid.");
      retainedBytes += bytes.size();
      studioWriteOriginal(folder, rbTexture::imageRelativePath(hash), bytes);
    }
    rbTexture::require(restored.size() == embedded.size(), "assets", "The model has unexpected embedded image data.");
    auto assets = rbTexture::loadAssets(doc, folder);
    const auto plan = rbTexture::compile(doc, {}, assets);
    const auto frame = std::min(metadata.value("reyond_texture_studio_frame", 0u), plan.frames - 1u);
    studioAutosave();
    studioNewDocument(std::move(doc));
    textureStudio.assetFolder = folder;
    textureStudio.assets = std::move(assets);
    textureStudio.frame = frame;
  });
}

static std::filesystem::path studioLibraryFile() {
  return documentPath("data/texture-studio/library.json");
}

static void studioLoadLibrary() {
  auto& s = textureStudio;
  if (std::exchange(s.libraryLoaded, true)) return;
  const auto file = studioLibraryFile();
  if (!std::filesystem::is_regular_file(file)) return;
  const auto settings = rbTexture::parseJson(ftebroom::bb::readFile(file, 32768));
  const auto readIds = [&](const char* key, auto use) {
    const auto entries = settings.value(key, rbTexture::Json::array());
    if (!entries.is_array() || entries.size() > 256) return;
    for (const auto& item : entries)
      if (item.is_string() && rbTexture::identifier(item.get_ref<const std::string&>())) use(item.get<std::string>());
  };
  readIds("favorites", [&](std::string id) { s.favorites.insert(std::move(id)); });
  readIds("recent", [&](std::string id) {
    if (s.recentPresets.size() < 12) s.recentPresets.push_back(std::move(id));
  });
  const auto saved = settings.value("userPresets", rbTexture::Json::array());
  if (saved.is_array() && saved.size() <= 64) {
    for (const auto& entry : saved) {
      if (!entry.is_array() || entry.size() != 2 || !entry[0].is_string() || !entry[1].is_string()) continue;
      const auto id = entry[0].get<std::string>(), label = entry[1].get<std::string>();
      if (rbTexture::identifier(id) && !label.empty() && label.size() <= 80) s.userPresets.emplace_back(id, label);
    }
  }
}

static void studioSaveLibrary() {
  auto& s = textureStudio;
  const auto file = studioLibraryFile();
  std::filesystem::create_directories(file.parent_path());
  const auto settings = rbTexture::Json{{"favorites", s.favorites}, {"recent", s.recentPresets}, {"userPresets", s.userPresets}};
  ftebroom::bb::atomicWrite(file, settings.dump());
}

static void studioRecordPreset(const std::string& id) {
  auto& recent = textureStudio.recentPresets;
  recent.erase(std::remove(recent.begin(), recent.end(), id), recent.end());
  recent.insert(recent.begin(), id);
  if (recent.size() > 12) recent.resize(12);
  studioSaveLibrary();
}

static void studioReleaseThumbnails() {
  for (const auto& [id, handle] : textureStudio.thumbnails) if (handle) draw->UnloadImage(handle);
  textureStudio.thumbnails.clear();
}

static void studioThumbnailTick() {
  auto& s = textureStudio;
  if (!s.thumbnailJob.valid() || s.thumbnailJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
  auto [id, bytes] = s.thumbnailJob.get();
  if (!s.open) return;
  int handle = 0;
  if (!bytes.empty()) {
    const auto path = "ftebroom/texture-studio/preset-" + id + ".png";
    draw->LoadImageData(path.c_str(), "image/png", bytes.data(), bytes.size());
    handle = draw->LoadImage(path.c_str());
  }
  s.thumbnails.emplace(id, handle);
}

template<class Recipe>
static int studioFindThumbnail(const std::string& id, Recipe recipe, bool visible) {
  auto& s = textureStudio;
  const auto found = s.thumbnails.find(id);
  if (found != s.thumbnails.end()) return found->second;
  if (visible && !s.thumbnailJob.valid()) {
    s.thumbnailJob = std::async(std::launch::async, [id, recipe] {
      try {
        const auto plan = rbTexture::compile(recipe());
        const auto result = rbTexture::evaluate(plan, {48, 48}, nullptr, rbTexture::memoryBudget, 1);
        return std::make_pair(id, rbTexture::png(*result.outputs.at("baseColor")));
      } catch (const std::exception&) {
        return std::make_pair(id, std::string{});
      }
    });
  }
  return 0;
}

template<class Recipe>
static float studioPresetThumbnail(const std::string& id, Recipe recipe) {
  const float side = 32 * uiScale();
  const ImVec2 size{side, side};
  const auto handle = studioFindThumbnail(id, recipe, ImGui::IsRectVisible(size));
  if (handle) ImGui::Image((ImTextureID)handle, size);
  else ImGui::Dummy(size);
  ImGui::SameLine();
  const auto& style = ImGui::GetStyle();
  return std::max(1.f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("☆").x -
    2 * style.FramePadding.x - 2 * style.ItemSpacing.x);
}

static std::filesystem::path studioUserPresetFolder() {
  return documentPath("data/texture-studio/user-presets");
}

static void studioSaveUserPreset(rbTexture::Document doc, const rbTexture::Assets& assets,
                                 const std::filesystem::path& originalFolder, const std::string& label) {
  studioLoadLibrary();
  auto& s = textureStudio;
  rbTexture::require(!label.empty() && label.size() <= 80, "library", "Enter a name up to 80 bytes.");
  rbTexture::require(s.userPresets.size() < 64, "library", "The user preset library holds up to 64 presets.");
  const auto id = ftebroom::bb::uuid();
  doc.source["name"] = label;
  doc.source["documentId"] = id;
  const auto folder = studioUserPresetFolder();
  rbTexture::compile(doc, {}, assets);
  std::filesystem::create_directories(folder);
  for (const auto& reference : doc.source.at("assetReferences")) {
    const auto relative = rbTexture::imageRelativePath(rbTexture::idField(reference, "sha256", "assets"));
    studioWriteOriginal(folder, relative, rbTexture::readOriginal(originalFolder, reference));
  }
  const auto file = folder / (id + ".rbtex.json");
  ftebroom::bb::atomicWrite(file, doc.save());
  s.userPresets.emplace_back(id, label);
  try {
    studioSaveLibrary();
  } catch (...) {
    s.userPresets.pop_back();
    std::error_code cleanup;
    std::filesystem::remove(file, cleanup);
    throw;
  }
  s.message = "Saved to your preset library.";
}

static rbTexture::Document studioReadUserPreset(const std::string& id) {
  return rbTexture::open(ftebroom::bb::readFile(studioUserPresetFolder() / (id + ".rbtex.json"),
    rbTexture::maxSourceBytes));
}

static void studioRemoveUserPreset(const std::string& id) {
  auto& s = textureStudio;
  auto presets = s.userPresets;
  auto favorites = s.favorites;
  auto recent = s.recentPresets;
  s.userPresets.erase(std::remove_if(s.userPresets.begin(), s.userPresets.end(),
    [&](const auto& preset) { return preset.first == id; }), s.userPresets.end());
  s.favorites.erase(id);
  s.recentPresets.erase(std::remove(s.recentPresets.begin(), s.recentPresets.end(), id), s.recentPresets.end());
  try {
    studioSaveLibrary();
  } catch (...) {
    s.userPresets = std::move(presets);
    s.favorites = std::move(favorites);
    s.recentPresets = std::move(recent);
    throw;
  }
  // Saved sources have their own graphs. Other presets and open sources may share the originals.
  std::error_code cleanup;
  std::filesystem::remove(studioUserPresetFolder() / (id + ".rbtex.json"), cleanup);
  s.message = cleanup ? "Removed from the library. The local preset file could not be deleted."
    : "Removed from your preset library.";
}

static TextureStudioState::SourceInput studioReadInput(const std::filesystem::path& tool,
    const std::filesystem::path& recovery, const std::filesystem::path& file, std::string license,
    std::string textureName, std::string palette, const std::shared_ptr<std::atomic<bool>>& stop) {
  namespace fs = std::filesystem;
  const bool importing = !license.empty();
  const bool archived = !importing && entityLower(file.extension().u8string()) == ".rbtexpkg";
  const auto temp = fs::temp_directory_path() / ("reyond-texture-input-" + ftebroom::bb::uuid());
  rbTexture::require(fs::create_directory(temp), "input", "Cannot create the Texture Studio input workspace.");
  struct Cleanup {
    fs::path folder;
    ~Cleanup() {
      std::error_code error;
      fs::remove_all(folder, error);
    }
  } cleanup{temp};

#ifndef _WIN32
  fs::permissions(temp, fs::perms::owner_all, fs::perm_options::replace);
#endif
  const auto output = temp / "output";
  const auto diagnostics = temp / "diagnostics.txt";
  std::vector<std::string> arguments{importing ? "import" : archived ? "unpack" : "open", file.u8string(),
    "--editor-input", "--out", output.u8string()};
  if (importing) {
    arguments.insert(arguments.end(), {"--license", std::move(license)});
    if (!textureName.empty())
      arguments.insert(arguments.end(), {"--texture", std::move(textureName)});
    if (palette.size() == 768) {
      const auto path = temp / "palette.lmp";
      ftebroom::bb::atomicWrite(path, palette);
      arguments.insert(arguments.end(), {"--palette", path.u8string()});
    }
  }
  const auto code = ftebroom::child::run(ftebroom::filesystemAccessPath(tool), arguments, diagnostics, stop.get(),
    std::chrono::seconds(30), "Texture Studio input decoder");
  rbTexture::require(!*stop, "input", "Opening cancelled.");
  // The helper atomically renames its staging directory only after writing a complete result.
  if ((code && *code != 0) || !fs::is_directory(output)) {
    const auto detail = ftebroom::bb::readFile(diagnostics, 4096);
    throw std::runtime_error(detail.empty() ? "The input decoder stopped. Try a smaller image or package." : detail);
  }

  auto doc = rbTexture::open(ftebroom::bb::readFile(output / rbTexture::packageSourceName, rbTexture::maxSourceBytes));
  rbTexture::Assets assets;
  if (!doc.readOnly) {
    std::map<std::string, const rbTexture::Json*> references;
    for (const auto& reference : doc.source.at("assetReferences")) {
      const auto id = rbTexture::idField(reference, "id", "assets");
      rbTexture::require(references.emplace(id, &reference).second, "assets", "The input worker repeated an image ID.");
    }
    rbTexture::require(references.size() <= 64, "assets", "The input worker returned too many images.");
    size_t index = 0, budget = rbTexture::decodedAssetLimit;
    for (const auto& [id, reference] : references) {
      rbTexture::require(!*stop, "input", "Opening cancelled.");
      auto raster = std::make_shared<rbTexture::Raster>();
      raster->size.width = rbTexture::integer(reference->at("width"), 1, rbTexture::maxDimension, "assets.width");
      raster->size.height = rbTexture::integer(reference->at("height"), 1, rbTexture::maxDimension, "assets.height");
      const size_t bytes = size_t(raster->size.width) * raster->size.height * 4;
      rbTexture::require(bytes <= budget, "assets", "The decoded images exceed their memory budget.");
      const auto decoded = output / ("raster-" + std::to_string(index++) + ".rgba");
      rbTexture::require(fs::file_size(decoded) == bytes, "assets", "The input worker returned an incomplete image.");
      raster->rgba.resize(bytes);
      std::ifstream stream(decoded, std::ios::binary);
      rbTexture::require(bool(stream.read(reinterpret_cast<char*>(raster->rgba.data()), bytes)), "assets",
        "Cannot read the decoded image.");
      assets.emplace(id, std::move(raster));
      budget -= bytes;

      if (importing || archived) {
        const auto path = rbTexture::imageRelativePath(rbTexture::idField(*reference, "sha256", "assets"));
        auto original = rbTexture::readOriginal(output, *reference);
        rbTexture::require(!*stop, "input", "Opening cancelled.");
        fs::create_directories(recovery);
        studioWriteOriginal(recovery, path, original);
      }
    }
  }
  if (importing) doc.source["documentId"] = ftebroom::bb::uuid();
  return {std::move(doc), std::move(assets), importing || archived ? fs::path{} : file,
    importing || archived ? recovery : file.parent_path(), importing};
}

static void studioQueueInput(const std::string& name, std::string license = {}, std::string textureName = {},
                             std::string projectPreset = {}, unsigned projectFrame = 0) {
  auto& s = textureStudio;
  rbTexture::require(!s.inputJob.valid(), "input", "Finish or cancel the current input first.");
  const auto root = std::filesystem::absolute(documentPath("ftebroom.cfg")).parent_path().parent_path();
  auto tool = root / "tools/texture-studio";
#ifdef _WIN32
  tool += ".exe";
#endif
  rbTexture::require(std::filesystem::is_regular_file(tool), "input",
    "The Texture Studio decoder is missing. Restore the tools directory from the editor package.");
  const auto recovery = std::filesystem::absolute(studioRecoveryRoot()).lexically_normal();
  const auto file = std::filesystem::u8path(name);
  std::string palette;
  if (!license.empty()) {
    // The helper also accepts modern raster files in the indexed game profiles.
    try {
      if (gameProfile == 2) palette = q1Palette();
      if (gameProfile == 3) palette = q2Palette();
    } catch (const std::exception&) {
      // An indexed import will diagnose the missing palette. Raster imports do not use it.
    }
  }
  s.playing = false;
  s.error.clear();
  s.message.clear();
  s.inputCancel = std::make_shared<std::atomic<bool>>(false);
  s.inputJob = std::async(std::launch::async, [tool = std::move(tool), recovery, file, license = std::move(license),
      textureName = std::move(textureName), palette = std::move(palette), key = std::move(projectPreset),
      projectFrame, stop = s.inputCancel] {
    auto input = studioReadInput(tool, recovery, file, license, textureName, palette, stop);
    input.projectPreset = key;
    input.projectFrame = projectFrame;
    return input;
  });
}

static void studioChooseProjectPreset(const rbTexture::ProjectPreset& preset) {
  auto& s = textureStudio;
  studioRefreshProjectScope();
  if (s.projectImported) {
    const auto source = rbTexture::projectPresetStorage(gameProfile) + preset.key + ".rbtex.json";
    const auto manifest = studioProjectManifest(s.projectRoot, gameProfile);
    if (manifest.at("files").contains(source)) {
      const auto path = studioProjectPath(s.projectRoot, source);
      studioProjectOwnedFile(path, manifest.at("files").at(source));
      const auto frame = manifest.contains("frames") ? manifest.at("frames").value(preset.key, 0u) : 0u;
      studioQueueInput(path.u8string(), {}, {}, preset.key, frame);
      return;
    }
  }
  studioAutosave();
  studioNewDocument(preset.document());
  s.projectPreset = preset.key;
}

static void studioOpenFile() {
  runUiChoose(textureStudio.file.u8string(), [](const std::string& name) {
    studioAction([&] { studioQueueInput(name); });
  });
}

static void studioImportRaster(std::string license, std::string textureName) {
  runUiChoose(textureStudio.file.parent_path().u8string(), [license = std::move(license),
    textureName = std::move(textureName)](const std::string& name) {
    studioAction([&] { studioQueueInput(name, license, textureName); });
  });
}

static void studioSaveAs() {
  runUiChoose(documentPath("material.rbtex.json").u8string(), [](const std::string& name) {
    studioAction([&] { studioSave(std::filesystem::u8path(name)); });
  }, true, true);
}

static void studioSavePackage() {
  runUiChoose(documentPath("material.rbtexpkg").u8string(), [](const std::string& name) {
    studioAction([&] {
      const auto path = std::filesystem::u8path(name);
      rbTexture::require(!std::filesystem::exists(std::filesystem::symlink_status(path)), "package",
        "Choose a new package file.");
      const auto& doc = textureStudioDoc();
      rbTexture::compile(doc, {}, textureStudio.assets);
      ftebroom::bb::atomicWrite(path, rbTexture::pack(doc, textureStudio.assetFolder));
      textureStudio.message = "Saved " + path.filename().u8string();
    });
  }, true);
}

enum class StudioExport { png8, png16, tga, jpeg, wad2, wad3, wal, pk3, fte };

static std::string studioExportBytes(const rbTexture::Evaluation& image, StudioExport format,
                                      const std::string& channel, const std::string& name,
                                      const std::string& palette, bool masked, unsigned quality) {
  using F = StudioExport;
  if (format == F::png8 || format == F::png16) return rbTexture::png(*image.outputs.at(channel), format == F::png16 ? 16 : 8);
  if (format == F::tga) return rbTexture::tga(*image.outputs.at(channel));
  if (format == F::jpeg) return rbTexture::jpeg(*image.outputs.at("baseColor"), quality);
  if (format >= F::wad2 && format <= F::wal) {
    const auto type = format == F::wad2 ? rbTexture::IndexedFormat::wad2 :
      format == F::wad3 ? rbTexture::IndexedFormat::wad3 : rbTexture::IndexedFormat::wal;
    return rbTexture::indexed(*image.outputs.at("baseColor"), name, palette, type, masked);
  }
  rbTexture::require(rbTexture::indexedName(name, false, false), "name", "Use a portable name of up to 15 characters.");
  const auto files = format == F::fte ? rbTexture::fteWallMaterial(image, "reyond-studio/" + name)
    : std::map<std::string, std::string>{{"textures/reyond-studio/" + name + ".tga",
        rbTexture::tga(*image.outputs.at("baseColor"), rbTexture::TgaOrigin::lowerLeft)}};
  std::map<std::string, std::string_view> members;
  for (const auto& [path, contents] : files) members.emplace(path, contents);
  std::ostringstream archive(std::ios::binary | std::ios::out);
  ftebroom::pak::zipStore(archive, members);
  return archive.str();
}

static void studioExportDialog() {
  if (!ImGui::BeginPopupModal("Export texture", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  auto& s = textureStudio;
  using F = StudioExport;
  static int format = int(F::png8), quality = 92;
  static bool masked = false;
  static char textureName[32]{};
  if (ImGui::IsWindowAppearing()) {
    const auto word = rbTexture::projectMaterialWord(textureStudioDoc().source.value("name", std::string{}));
    std::snprintf(textureName, sizeof textureName, "%s_1", word.c_str());
  }
  static constexpr const char* formatNames =
    "PNG 8-bit\0PNG 16-bit\0TGA\0JPEG\0Quake WAD2\0Half-Life WAD3\0Quake II WAL\0Quake III PK3\0FTE material PK3\0";
  ImGui::Combo("Format", &format, formatNames);
  const auto target = StudioExport(format);
  if (target == F::jpeg) {
    ImGui::SliderInt("JPEG quality", &quality, 1, 100);
    ImGui::TextUnformatted("Lossy color export. The image must be opaque.");
  }
  if (target >= F::wad2) ImGui::InputText("Texture name", textureName, sizeof textureName);
  if (target == F::wad3) {
    ImGui::Checkbox("Masked texture", &masked);
    ImGui::TextUnformatted("Uses a generated RGB 3-3-2 palette.");
  }
  if (target == F::wad2 || target == F::wal)
    ImGui::TextUnformatted("Uses the active game's palette.");
  const bool ready = s.result && s.result->revision == s.revision && s.result->fullSize;
  if (ready) ImGui::Text("%u × %u", s.result->image.size.width, s.result->image.size.height);
  const bool paletteReady = (target != F::wad2 || gameProfile == 2) && (target != F::wal || gameProfile == 3);
  const bool outputsReady = ready && (target < F::jpeg || s.result->image.outputs.count("baseColor"));
  if (!paletteReady) ImGui::TextUnformatted("Select the matching game profile before exporting its indexed texture.");
  if (ready && !outputsReady) ImGui::TextUnformatted("Connect the output channels required by this format.");
  ImGui::BeginDisabled(!outputsReady || !paletteReady);
  const bool exportFile = ImGui::Button("Choose destination…");
  runUiRecordGlobal("texture-studio/export-destination");
  ImGui::SameLine();
  const bool dryRun = ImGui::Button("Dry run");
  runUiRecordGlobal("texture-studio/export-dry-run");
  ImGui::EndDisabled();
  if (ready && !bbActive && !s.projectRoot.empty() && s.result->image.outputs.count("baseColor")) {
    ImGui::SameLine();
    if (ImGui::Button("Save to project")) {
      studioAction([&] {
        if (const auto* preset = studioProjectPresetByKey(s.projectPreset); preset && s.projectImported)
          studioBeginProjectBake(preset);
        else studioBeginApply(true);
      });
      if (s.apply != TextureStudioState::Apply::none || s.projectJob.valid()) ImGui::CloseCurrentPopup();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
  static std::string manifestStamp, manifestText, manifestHash;
  static size_t manifestSize = 0;
  const auto stamp = std::to_string(s.revision) + "/" + std::to_string(format) + "/" + std::to_string(quality) +
    "/" + std::to_string(gameProfile) + "/" + (masked ? "1" : "0") + "/" + textureName + "/" + s.channel;
  if (!manifestText.empty() && manifestStamp == stamp) {
    ImGui::TextWrapped("%s", manifestText.c_str());
    runUiRecordGlobal("texture-studio/export-plan");
    if (runUiLive) runUiGlobalRects["texture-studio/export-manifest"] = {{"sha256", manifestHash}, {"bytes", manifestSize}};
  }
  if (exportFile || dryRun) {
    const auto evaluation = s.result->image;
    const auto channel = s.channel;
    const std::string name = (target == F::wad3 && masked ? "{" : "") + std::string(textureName);
    const bool useMask = target == F::wad3 && masked;
    const std::string suffix = target <= F::png16 ? ".png" : target == F::tga ? ".tga" : target == F::jpeg ? ".jpg"
      : target == F::wal ? ".wal" : target == F::wad2 || target == F::wad3 ? ".wad" : ".pk3";
    studioAction([&] {
      std::string palette;
      s.error.clear();
      if (target == F::wad2) palette = q1Palette();
      else if (target == F::wal) palette = q2Palette();
      else if (target == F::wad3) palette = studioGoldSrcPalette();
      if (dryRun) {
        const auto bytes = studioExportBytes(evaluation, target, channel, name, palette, useMask, quality);
        manifestStamp = stamp;
        manifestSize = bytes.size();
        manifestHash = ftebroom::sha256(bytes);
        manifestText = std::to_string(manifestSize) + " bytes\nSHA-256 " + manifestHash;
        if (!palette.empty()) manifestText += "\nPalette SHA-256 " + ftebroom::sha256(palette);
        return;
      }
      ImGui::CloseCurrentPopup();
      runUiChoose(documentPath("texture-studio" + suffix).u8string(),
        [evaluation, channel, name, target, useMask, palette = std::move(palette), jpegQuality = quality](const std::string& pathText) {
        studioAction([&] {
          const auto filename = std::filesystem::u8path(pathText);
          const auto bytes = studioExportBytes(evaluation, target, channel, name, palette, useMask, jpegQuality);
          ftebroom::bb::atomicWrite(filename, bytes);
          textureStudio.message = "Exported " + filename.filename().u8string();
        });
      }, true, true);
    });
  }
  if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
  ImGui::EndPopup();
}

static std::string studioParameterId(const rbTexture::Json& binding) {
  return binding.value("group", "") + "/" + binding.at("node").get<std::string>() + "/" +
    binding.at("key").get<std::string>();
}

static rbTexture::Json& studioParameterValue(rbTexture::Document& document, const rbTexture::Json& binding) {
  const auto id = binding.at("node").get<std::string>();
  const auto group = binding.value("group", "");
  const auto key = binding.at("key").get<std::string>();
  auto& scope = document.source.at("graph");
  auto& nodes = group.empty() ? scope.at("nodes") : scope.at("groups").at(group).at("nodes");
  auto node = std::find_if(nodes.begin(), nodes.end(), [&](const auto& entry) { return entry["id"] == id; });
  rbTexture::require(node != nodes.end(), "graph", "The parameter's node no longer exists.");
  return node->at("parameters").at(key);
}

struct StudioParameterRange { float low, high; int step; };

static StudioParameterRange studioParameterRange(const rbTexture::Json& binding) {
  const auto low = binding.at("min").get<float>(), high = binding.at("max").get<float>();
  rbTexture::require(std::isfinite(low) && std::isfinite(high) && low >= -1000000 && high <= 1000000 && low <= high,
    "parameters", "The parameter has an invalid slider range.");
  const auto step = rbTexture::integer(binding.at("step"), 0, 1000000, "parameters.step");
  return {low, high, int(std::max(1u, step))};
}

static void studioParameter(const rbTexture::Json& binding) {
  auto& s = textureStudio;
  const auto id = binding.at("node").get<std::string>();
  const auto key = binding.at("key").get<std::string>();
  const auto label = binding.at("label").get<std::string>();
  auto document = textureStudioDoc();
  auto& param = studioParameterValue(document, binding);
  const auto range = studioParameterRange(binding);
  bool changed = false;
  ImGui::PushID(studioParameterId(binding).c_str());
  if (param.is_number_integer()) {
    int v = param.get<int>();
    const int minimum = int(range.low), maximum = int(range.high);
    changed = ImGui::SliderInt(label.c_str(), &v, minimum, maximum, "%d", ImGuiSliderFlags_AlwaysClamp);
    if (changed) {
      v = std::clamp(minimum + range.step * ((v - minimum) / range.step), minimum, maximum);
      param = v;
    }
  } else {
    float v = param.get<float>();
    changed = ImGui::SliderFloat(label.c_str(), &v, range.low, range.high,
      "%.3f", ImGuiSliderFlags_AlwaysClamp);
    if (changed) param = v;
  }
  const bool completed = ImGui::IsItemDeactivatedAfterEdit();
  runUiRecordGlobal("texture-studio/parameter/" + id + "/" + key);
  ImGui::PopID();
  if (changed) {
    s.drag = std::move(document);
    studioSchedule();
  }
  if (completed && s.drag) {
    auto edit = std::move(*s.drag);
    s.drag.reset();
    try { studioEdit(std::move(edit)); }
    catch (...) { studioSchedule(); throw; }
  }
}

static void studioFamilyControls() {
  const auto id = rbTexture::familyIdentity(textureStudioDoc());
  if (!id) return;
  const auto [family, role] = *id;
  static rbTexture::FamilyControls draft{};
  if (ImGui::Button("Family settings…")) {
    draft = rbTexture::readFamilyControls(textureStudioDoc(), family);
    ImGui::OpenPopup("Texture family settings");
  }
  runUiRecordGlobal("texture-studio/family-settings");
  if (!ImGui::BeginPopup("Texture family settings")) return;
  ImGui::Text("%s / %s", rbTexture::textureFamilies[family].name, rbTexture::familyRoles[role]);
  const auto colorControl = [](const char* name, uint32_t& hex) {
    float rgb[]{float((hex >> 16) & 255) / 255.f, float((hex >> 8) & 255) / 255.f, float(hex & 255) / 255.f};
    if (!ImGui::ColorEdit3(name, rgb)) return;
    hex = (uint32_t(std::round(255.f * std::clamp(rgb[0], 0.f, 1.f))) << 16) |
      (uint32_t(std::round(255.f * std::clamp(rgb[1], 0.f, 1.f))) << 8) |
      uint32_t(std::round(255.f * std::clamp(rgb[2], 0.f, 1.f)));
  };
  colorControl("Shadow color", draft.shade);
  colorControl("Accent color", draft.accent);
  ImGui::InputScalar("Family seed", ImGuiDataType_U32, &draft.seed, nullptr, nullptr, "%u");
  int scale = int(draft.scale);
  if (ImGui::SliderInt("Pattern frequency", &scale, 25, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp))
    draft.scale = unsigned(scale);
  ImGui::SliderFloat("Wear adjustment", &draft.wear, -.5f, .5f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
  const auto& info = textureStudioDoc().source["provenance"]["family"];
  if (info.contains("overrides") && info["overrides"].is_array() && !info["overrides"].empty()) {
    ImGui::TextUnformatted("Local overrides");
    for (size_t index = 0; index < std::min<size_t>(info["overrides"].size(), 20); ++index) {
      const auto& item = info["overrides"][index];
      if (item.is_string()) ImGui::BulletText("%s", item.get<std::string>().c_str());
    }
  }
  const bool update = ImGui::Button("Apply shared settings");
  runUiRecordGlobal("texture-studio/family-settings/apply");
  ImGui::SameLine();
  const bool reset = ImGui::Button("Reset local overrides");
  if (update || reset) {
    studioAction([&] {
      auto doc = textureStudioDoc();
      rbTexture::refreshFamilyMember(doc, draft, reset);
      studioEdit(std::move(doc));
      ImGui::CloseCurrentPopup();
    });
  }
  ImGui::EndPopup();
}

static void studioQuick() {
  auto& s = textureStudio;
  studioFamilyControls();
  const auto animation = textureStudioDoc().source.at("animation");
  const auto frames = animation.at("frames").get<int>();
  if (frames > 1) {
    if (ImGui::Button(s.playing ? "Pause preview" : "Play preview")) {
      s.playing = !s.playing;
      if (s.playing) {
        if (!animation.at("loop").get<bool>() && int(s.frame + 1) == frames) {
          s.frame = 0;
          studioSchedule();
        }
        s.playStarted = ImGui::GetTime() - double(s.frame) / animation.at("fps").get<unsigned>();
      }
    }
    ImGui::SameLine();
    int selected = int(s.frame) + 1;
    if (ImGui::SliderInt("Frame", &selected, 1, frames)) {
      s.playing = false;
      s.frame = unsigned(selected - 1);
      studioSchedule();
    }
  }
  if (ImGui::TreeNode("Animation settings")) {
    struct AnimationSlider { const char* key; const char* label; int upper; };
    const AnimationSlider sliders[]{{"frames", "Frame count", 64}, {"fps", "Frames per second", 30}};
    for (const auto& slider : sliders) {
      int value = animation.at(slider.key).get<int>();
      if (ImGui::SliderInt(slider.label, &value, 1, slider.upper)) {
        auto doc = textureStudioDoc();
        doc.source["animation"][slider.key] = value;
        s.drag = std::move(doc);
        s.playing = false;
        studioSchedule();
      }
      if (ImGui::IsItemDeactivatedAfterEdit() && s.drag) studioEdit(*s.drag);
    }
    bool looping = animation.at("loop").get<bool>();
    if (ImGui::Checkbox("Loop", &looping)) {
      auto doc = textureStudioDoc();
      doc.source["animation"]["loop"] = looping;
      s.playing = false;
      studioEdit(std::move(doc));
    }
    ImGui::TreePop();
  }
  const auto size = textureStudioDoc().resolution();
  const auto sizeLabel = std::to_string(size.width) + " × " + std::to_string(size.height);
  if (ImGui::BeginCombo("Resolution", sizeLabel.c_str())) {
    for (unsigned edge : {32u, 64u, 128u, 256u, 512u, 1024u, 2048u, 4096u}) {
      const auto label = std::to_string(edge) + " × " + std::to_string(edge);
      if (ImGui::Selectable(label.c_str(), size.width == edge && size.height == edge)) {
        auto doc = textureStudioDoc();
        doc.source["resolution"] = {{"width", edge}, {"height", edge}};
        studioEdit(std::move(doc));
      }
    }
    ImGui::EndCombo();
  }
  auto doc = textureStudioDoc();
  uint32_t seed = doc.seed();
  if (ImGui::InputScalar("Seed", ImGuiDataType_U32, &seed, nullptr, nullptr, "%u")) {
    doc.source["rootSeed"] = seed;
    s.drag = doc;
    studioSchedule();
  }
  if (ImGui::IsItemDeactivatedAfterEdit() && s.drag) {
    s.variationBase = s.drag->seed();
    studioEdit(*s.drag);
  }
  runUiRecordGlobal("texture-studio/seed");
  if (ImGui::Button("Randomize seed")) {
    const auto identity = ftebroom::bb::uuid();
    doc = textureStudioDoc();
    doc.source["rootSeed"] = uint32_t(rbTexture::stableSeed(seed, identity));
    s.variationBase = doc.seed();
    studioEdit(std::move(doc));
  }
  auto bindings = textureStudioDoc().source.at("exposedParameters");
  for (const auto& binding : bindings) studioParameter(binding);
  if (!bindings.empty()) {
    const auto unlocked = [&](const auto& binding) { return !s.parameterLocks.count(studioParameterId(binding)); };
    ImGui::BeginDisabled(std::none_of(bindings.begin(), bindings.end(), unlocked));
    const bool randomize = ImGui::Button("Randomize parameters");
    runUiRecordGlobal("texture-studio/randomize-parameters");
    ImGui::EndDisabled();
    if (randomize) {
      auto next = textureStudioDoc();
      rbTexture::Pcg32 random(rbTexture::stableSeed(next.seed(), ftebroom::bb::uuid()), 0);
      for (const auto& binding : bindings) {
        if (!unlocked(binding)) continue;
        auto& value = studioParameterValue(next, binding);
        const auto range = studioParameterRange(binding);
        const unsigned number = random.next();
        if (value.is_number_integer()) {
          const int slots = (int(range.high) - int(range.low)) / range.step + 1;
          value = int(range.low) + int(number % unsigned(slots)) * range.step;
        } else value = rbTexture::mix(range.low, range.high, float(number >> 8) / 16777216.f);
      }
      studioEdit(std::move(next));
    }
    if (ImGui::TreeNode("Parameter locks")) {
      for (const auto& binding : bindings) {
        const auto id = studioParameterId(binding);
        bool locked = s.parameterLocks.count(id) != 0;
        ImGui::PushID(id.c_str());
        if (ImGui::Checkbox(binding.at("label").get_ref<const std::string&>().c_str(), &locked)) {
          if (locked) s.parameterLocks.insert(id);
          else s.parameterLocks.erase(id);
        }
        ImGui::PopID();
      }
      ImGui::TreePop();
    }
  }
  ImGui::SeparatorText("Variations");
  for (unsigned variation = 0; variation < 16; ++variation) {
    if (variation % 4) ImGui::SameLine();
    ImGui::PushID(variation);
    if (ImGui::Button(std::to_string(variation + 1).c_str(), {35 * uiScale(), 0})) {
      auto next = textureStudioDoc();
      next.source["rootSeed"] = rbTexture::variationSeed(s.variationBase, variation);
      studioEdit(std::move(next));
    }
    runUiRecordGlobal("texture-studio/variation/" + std::to_string(variation + 1));
    ImGui::PopID();
  }
}

static void studioLayers() {
  auto doc = textureStudioDoc();
  if (!doc.source.at("outputs").contains("baseColor")) {
    ImGui::TextUnformatted("Connect Base Color in Node Graph to create a layer stack.");
    return;
  }
  auto stack = rbTexture::layers(doc);
  if (stack.order.empty()) ImGui::TextUnformatted("Base Color comes from the node graph.");
  static rbTexture::Pixel tint{.38f, .27f, .19f, 1.f};
  ImGui::ColorEdit4("Layer color", tint.data(), ImGuiColorEditFlags_Float);
  const bool added = ImGui::Button("Add color layer");
  runUiRecordGlobal("texture-studio/layer-add");
  if (added) {
    rbTexture::appendSolidLayer(doc, stack, ftebroom::bb::uuid(), tint);
    studioEdit(std::move(doc));
    return;
  }
  for (size_t index = stack.order.size(); index > 0; --index) {
    const size_t slot = index - 1;
    const auto& id = stack.order[slot];
    auto& layer = rbTexture::findNode(doc, id);
    auto& parameters = layer["parameters"];
    bool enabled = parameters.value("enabled", true);
    ImGui::PushID(id.c_str());
    const bool visibility = ImGui::Checkbox(layer.value("label", id).c_str(), &enabled);
    ImGui::SameLine();
    ImGui::BeginDisabled(index == stack.order.size());
    const bool up = ImGui::SmallButton("Up");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(slot == 0);
    const bool down = ImGui::SmallButton("Down");
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool remove = ImGui::SmallButton("Remove");
    const bool duplicate = ImGui::SmallButton("Duplicate");
    std::array<char, 81> label{};
    std::snprintf(label.data(), label.size(), "%s", layer.value("label", id).c_str());
    const bool rename = ImGui::InputText("Name", label.data(), label.size(), ImGuiInputTextFlags_EnterReturnsTrue) && label[0];
    int mode = parameters.value("mode", 0);
    const bool modeChanged = ImGui::Combo("Blend mode", &mode, "Normal\0Multiply\0Screen\0Add\0Overlay\0");
    studioParameter(rbTexture::Json{{"node", id}, {"key", "opacity"}, {"label", "Opacity"},
                                   {"min", 0.f}, {"max", 1.f}, {"step", 0}});
    const auto& edges = doc.source.at("graph").at("edges");
    const auto mask = std::find_if(edges.begin(), edges.end(), [&](const auto& edge) {
      return edge.at("toNode") == id && edge.at("toPort") == "mask";
    });
    if (mask != edges.end()) {
      const auto source = mask->at("fromNode").get<std::string>();
      if (rbTexture::findNode(doc, source).at("type") == "Constant")
        studioParameter(rbTexture::Json{{"node", source}, {"key", "value"}, {"label", "Mask coverage"},
                                       {"min", 0.f}, {"max", 1.f}, {"step", 0}});
      else ImGui::TextUnformatted("Mask comes from the node graph.");
    }
    ImGui::PopID();
    if (!visibility && !up && !down && !remove && !rename && !duplicate && !modeChanged) continue;
    if (duplicate) {
      rbTexture::duplicateLayer(doc, stack, slot, ftebroom::bb::uuid());
      studioEdit(std::move(doc));
      return;
    }
    if (rename) {
      layer["label"] = label.data();
      layer["studioLayer"] = true;
    }
    if (visibility) parameters["enabled"] = enabled;
    if (modeChanged) parameters["mode"] = mode;
    if (up) std::swap(stack.order[slot], stack.order[slot + 1]);
    if (down) std::swap(stack.order[slot], stack.order[slot - 1]);
    if (remove) stack.order.erase(stack.order.begin() + slot);
    rbTexture::bindLayers(doc, stack);
    studioEdit(std::move(doc));
    return;
  }
}

struct StudioGraphPins {
  using Pin = std::pair<std::string, rbTexture::Port>;
  std::vector<Pin> inputs, outputs;
};

static StudioGraphPins studioGraphPins(const rbTexture::Json& node, const rbTexture::Json& graph,
                                        std::map<std::string, StudioGraphPins>& definitions, unsigned depth = 0) {
  StudioGraphPins result;
  if (node.value("version", 0) != 1 || depth > 16) return result;
  const auto type = node.at("type").get<std::string>();
  if (const auto* spec = rbTexture::contract(type)) {
    for (const auto& pin : spec->inputs) result.inputs.emplace_back(pin.name, pin.type);
    result.outputs.emplace_back(spec->output.name, spec->output.type);
    return result;
  }
  if (type != "Subgraph") return result;
  const auto name = node.at("parameters").at("group").get<std::string>();
  const auto found = definitions.find(name);
  if (found != definitions.end()) return found->second;
  const auto& group = graph.at("groups").at(name);
  std::map<std::string, StudioGraphPins> internal;
  for (const auto& child : group.at("nodes"))
    internal.emplace(child.at("id").get<std::string>(), studioGraphPins(child, graph, definitions, depth + 1));
  const auto resolve = [&](const rbTexture::Json& binding, bool output) -> std::optional<rbTexture::Port> {
    const auto child = internal.find(binding.at("node").get<std::string>());
    if (child == internal.end()) return {};
    const auto& pins = output ? child->second.outputs : child->second.inputs;
    const auto pin = std::find_if(pins.begin(), pins.end(), [&](const auto& pair) {
      return pair.first == binding.at("port");
    });
    return pin == pins.end() ? std::nullopt : std::optional{pin->second};
  };
  std::map<std::string, std::optional<rbTexture::Port>> inputs;
  for (const auto& binding : group.at("inputs")) {
    const auto value = resolve(binding, false);
    const auto [entry, inserted] = inputs.try_emplace(binding.at("name").get<std::string>(), value);
    if (!inserted && entry->second != value) entry->second.reset();
  }
  for (const auto& [name, value] : inputs)
    if (value) result.inputs.emplace_back(name, *value);
  const auto& outputs = group.at("outputs");
  for (auto binding = outputs.begin(); binding != outputs.end(); ++binding)
    if (const auto type = resolve(*binding, true)) result.outputs.emplace_back(binding.key(), *type);
  definitions.emplace(name, result);
  return result;
}

static void studioNodes() {
  auto& s = textureStudio;
  auto doc = textureStudioDoc();
  auto& graph = doc.source.at("graph");
  auto& nodes = graph.at("nodes");
  auto& edges = graph.at("edges");
  static unsigned pinRevision = std::numeric_limits<unsigned>::max();
  static std::map<std::string, StudioGraphPins> nodePins;
  if (pinRevision != s.revision) {
    nodePins.clear();
    std::map<std::string, StudioGraphPins> definitions;
    for (const auto& node : nodes)
      nodePins.emplace(node.at("id").get<std::string>(), studioGraphPins(node, graph, definitions));
    pinRevision = s.revision;
  }
  static float zoom = .70f;
  static ImVec2 pan{12,12};
  static bool minimap = false;
  if (ImGui::Button("Add node")) ImGui::OpenPopup("Add texture node");
  runUiRecordGlobal("texture-studio/graph-add");
  if (ImGui::BeginPopup("Add texture node")) {
    for (const auto& spec : rbTexture::contracts()) {
      if (spec.kernel == rbTexture::Kernel::image) continue;
      const bool add = ImGui::MenuItem(spec.type);
      runUiRecordGlobal(std::string("texture-studio/graph/add/") + spec.type);
      if (!add) continue;
      const auto id = ftebroom::bb::uuid();
      const int index = int(nodes.size());
      nodes.push_back(rbTexture::node(id.c_str(), spec.type, rbTexture::defaultParameters(spec.kernel),
        index % 5, index / 5));
      ImGui::EndPopup();
      studioEdit(std::move(doc));
      s.selectedNode = index;
      return;
    }
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset graph view")) {
    pan = {12,12};
    zoom = .70f;
  }
  ImGui::SameLine();
  const bool zoomAll = ImGui::Button("Zoom to all");
  ImGui::SameLine();
  ImGui::Checkbox("Minimap", &minimap);
  ImGui::BeginDisabled(s.groupSelection.empty());
  if (ImGui::Button("Group nodes…")) ImGui::OpenPopup("Group texture nodes");
  runUiRecordGlobal("texture-studio/graph/group");
  ImGui::EndDisabled();
  std::string locate;
  if (!s.error.empty() && !s.errorNode.empty()) {
    if (ImGui::Button("Locate problem node")) locate = s.errorNode;
    runUiRecordGlobal("texture-studio/graph/locate-error");
  }
  if (ImGui::BeginPopupModal("Group texture nodes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name[80] = "Material group";
    ImGui::InputText("Name", name, sizeof(name));
    bool grouped = false;
    ImGui::BeginDisabled(!name[0] || s.groupSelection.empty());
    const bool create = ImGui::Button("Create group");
    runUiRecordGlobal("texture-studio/graph/group-create");
    if (create) studioAction([&] {
      rbTexture::groupNodes(doc, s.groupSelection, ftebroom::bb::uuid(), name);
      studioEdit(std::move(doc));
      s.selectedNode = int(textureStudioDoc().source.at("graph").at("nodes").size()) - 1;
      s.groupSelection.clear();
      grouped = true;
      ImGui::CloseCurrentPopup();
    });
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
    ImGui::EndPopup();
    if (grouped) return;
  }
  ImGui::BeginChild("Graph Canvas", {-1, 225 * uiScale()}, ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const auto origin = ImGui::GetCursorScreenPos();
  const auto area = ImGui::GetContentRegionAvail();
  const bool hovered = ImGui::IsWindowHovered();
  if (hovered && ImGui::GetIO().MouseWheel) {
    const float next = std::clamp(zoom * std::pow(1.15f, ImGui::GetIO().MouseWheel), .04f, 2.f);
    const auto mouse = ImGui::GetIO().MousePos;
    pan = {mouse.x-origin.x - (mouse.x-origin.x-pan.x)*next/zoom,
           mouse.y-origin.y - (mouse.y-origin.y-pan.y)*next/zoom};
    zoom = next;
  }
  if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    pan.x += ImGui::GetIO().MouseDelta.x; pan.y += ImGui::GetIO().MouseDelta.y;
  }
  auto* painter = ImGui::GetWindowDrawList();
  const auto color = ImGui::GetColorU32(ImGuiCol_Border);
  const auto accent = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
  const ImVec2 card{200 * zoom, 68 * zoom};
  std::map<std::string, ImVec2> positions;
  std::map<std::string, float> heights;
  struct CanvasPin { ImVec2 center; rbTexture::Port type; };
  std::map<std::pair<std::string, std::string>, CanvasPin> sendPins, receivePins;
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const auto nearPin = [&](ImVec2 center) {
    const auto dx = center.x - mouse.x, dy = center.y - mouse.y;
    return dx * dx + dy * dy <= 81.f;
  };
  const auto pinVisible = [&](ImVec2 center) {
    constexpr float margin = 9.f;
    return center.x >= origin.x - margin && center.x <= origin.x + area.x + margin &&
           center.y >= origin.y - margin && center.y <= origin.y + area.y + margin;
  };
  for (size_t index = 0; index < nodes.size(); ++index) {
    const auto& n = nodes[index];
    float x = float(index % 4) * 232, y = float(index / 4) * 112;
    const auto point = n.find("position");
    if (point != n.end() && point->is_array() && point->size() == 2 &&
        (*point)[0].is_number() && (*point)[1].is_number()) {
      x = std::clamp((*point)[0].get<float>(), -30000.f, 30000.f);
      y = std::clamp((*point)[1].get<float>(), -30000.f, 30000.f);
    }
    const auto& id = n.at("id").get_ref<const std::string&>();
    const auto position = ImVec2{origin.x + pan.x + x * zoom, origin.y + pan.y + y * zoom};
    positions[id] = position;
    const auto& ports = nodePins.at(id);
    const auto count = std::max(ports.inputs.size(), ports.outputs.size());
    const float step = std::max(15.f, 15 * zoom);
    heights[id] = std::max(card.y, 26 * zoom + (count + 1) * step);
    const auto place = [&](const auto& entries, float left, auto& destination) {
      for (size_t i = 0; i < entries.size(); ++i) {
        const auto& [name, type] = entries[i];
        const float top = position.y + 26 * zoom + (i + .5f) * step;
        destination.emplace(std::make_pair(id, name), CanvasPin{{left, top}, type});
      }
    };
    place(ports.outputs, position.x + card.x, sendPins);
    place(ports.inputs, position.x, receivePins);
  }
  for (const auto& e : edges) {
    const auto start = positions.find(e.at("fromNode").get_ref<const std::string&>());
    const auto end = positions.find(e.at("toNode").get_ref<const std::string&>());
    if (start == positions.end() || end == positions.end()) continue;
    const auto from = sendPins.find({start->first, e.at("fromPort").get<std::string>()});
    const auto to = receivePins.find({end->first, e.at("toPort").get<std::string>()});
    const ImVec2 a = from == sendPins.end() ? ImVec2{start->second.x + card.x, start->second.y + card.y * .5f} : from->second.center;
    const ImVec2 b = to == receivePins.end() ? ImVec2{end->second.x, end->second.y + card.y * .5f} : to->second.center;
    if (std::max(a.x,b.x) + card.x * .25f < origin.x || std::min(a.x,b.x) - card.x * .25f > origin.x + area.x ||
        std::max(a.y,b.y) < origin.y || std::min(a.y,b.y) > origin.y + area.y) continue;
    painter->AddBezierCubic(a, {a.x + card.x * .25f, a.y}, {b.x - card.x * .25f, b.y}, b, accent, 2.f);
  }
  for (size_t index = 0; index < nodes.size(); ++index) {
    const auto& n = nodes[index];
    const auto& id = n.at("id").get_ref<const std::string&>();
    const auto a = positions.at(id);
    const ImVec2 b{a.x + card.x, a.y + heights.at(id)};
    if (b.x < origin.x || b.y < origin.y || a.x > origin.x + area.x || a.y > origin.y + area.y) continue;
    const bool selected = int(index) == s.selectedNode || s.groupSelection.count(id);
    painter->AddRectFilled(a, b, ImGui::GetColorU32(ImGuiCol_FrameBg), 4);
    painter->AddRect(a, b, selected ? accent : color, 4, 0, selected ? 2.f : 1.f);
    const auto label = n.value("label", n.at("type").get_ref<const std::string&>());
    painter->AddText(ImGui::GetFont(), std::clamp(zoom * 20.f, 10.f, 22.f),
      {a.x + 6, a.y + 5}, ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        mouse.x >= a.x && mouse.x < b.x && mouse.y >= a.y && mouse.y < b.y) {
      s.selectedNode = int(index);
      if (ImGui::GetIO().KeyCtrl) {
        if (!s.groupSelection.erase(id)) s.groupSelection.insert(id);
      } else {
        s.groupSelection = {id};
        s.movingNode = id;
      }
    }
  }
  const auto pinColor = [&](rbTexture::Port type) {
    switch (type) {
      case rbTexture::Port::color: return IM_COL32(128, 188, 171, 255);
      case rbTexture::Port::normal: return IM_COL32(143, 181, 236, 255);
      case rbTexture::Port::uv: return IM_COL32(224, 162, 97, 255);
      case rbTexture::Port::packed: return IM_COL32(202, 152, 222, 255);
      case rbTexture::Port::scalar: return IM_COL32(198, 202, 209, 255);
    }
    return color;
  };
  for (const auto& [key, pin] : sendPins) {
    if (!pinVisible(pin.center)) continue;
    const bool over = hovered && nearPin(pin.center);
    painter->AddCircleFilled(pin.center, over ? 5.f : 3.5f, pinColor(pin.type));
    if (over) {
      ImGui::SetTooltip("Output %s", key.second.c_str());
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        s.wireStart = key;
        s.movingNode.clear();
      }
    }
  }
  std::optional<rbTexture::Json> newConnection;
  const auto sending = sendPins.find(s.wireStart);
  if (sending != sendPins.end())
    painter->AddLine(sending->second.center, mouse, pinColor(sending->second.type), 2.f);
  for (const auto& [key, pin] : receivePins) {
    if (!pinVisible(pin.center)) continue;
    const bool over = hovered && nearPin(pin.center);
    painter->AddCircleFilled(pin.center, over ? 5.f : 3.5f, pinColor(pin.type));
    if (!over) continue;
    const bool compatible = sending == sendPins.end() ||
      (pin.type == sending->second.type && key.first != sending->first.first);
    ImGui::SetTooltip("%s%s", key.second.c_str(), compatible ? "" : " · Incompatible output");
    if (sending != sendPins.end() && compatible &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left))
      newConnection = rbTexture::wire(sending->first.first.c_str(), sending->first.second.c_str(),
        key.first.c_str(), key.second.c_str());
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) s.wireStart = {};
  if (!s.movingNode.empty() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    const auto position = positions.at(s.movingNode);
    const auto delta = ImGui::GetIO().MouseDelta;
    rbTexture::findNode(doc, s.movingNode)["position"] = {
      (position.x + delta.x - origin.x - pan.x) / zoom, (position.y + delta.y - origin.y - pan.y) / zoom};
    s.drag = doc;
  }
  if ((zoomAll || minimap) && !positions.empty()) {
    ImVec2 low = positions.begin()->second, high = low;
    for (const auto& [id, point] : positions) {
      low = {std::min(low.x, point.x), std::min(low.y, point.y)};
      high = {std::max(high.x, point.x + card.x), std::max(high.y, point.y + heights.at(id))};
    }
    if (zoomAll) {
      const float next = std::clamp(zoom * std::min(std::max(1.f, area.x - 18) / (high.x-low.x),
        std::max(1.f, area.y - 18) / (high.y-low.y)), .04f, 2.f);
      pan = {area.x*.5f - ((low.x+high.x)*.5f-origin.x-pan.x)*next/zoom,
             area.y*.5f - ((low.y+high.y)*.5f-origin.y-pan.y)*next/zoom};
      zoom = next;
    }
    if (minimap) {
      const float scale = std::min(126.f/(high.x-low.x), 75.f/(high.y-low.y));
      const ImVec2 mount{origin.x+area.x-141, origin.y+area.y-90};
      const ImVec2 corner{mount.x+132, mount.y+81};
      painter->AddRectFilled(mount, corner, ImGui::GetColorU32(ImGuiCol_WindowBg), 3);
      painter->PushClipRect(mount, corner, true);
      const auto toMini = [&](ImVec2 p) {
        return ImVec2{mount.x+3+(p.x-low.x)*scale, mount.y+3+(p.y-low.y)*scale};
      };
      for (const auto& [id, point] : positions)
        painter->AddRectFilled(toMini(point), toMini({point.x+card.x, point.y+heights.at(id)}), accent);
      painter->AddRect(toMini(origin), toMini({origin.x+area.x, origin.y+area.y}), ImGui::GetColorU32(ImGuiCol_Text));
      painter->PopClipRect();
      painter->AddRect(mount, corner, color, 3);
    }
  }
  ImGui::Dummy({std::max(1.f, area.x), std::max(1.f, area.y)});
  ImGui::EndChild();
  if (newConnection) {
    const auto match = std::find_if(edges.begin(), edges.end(), [&](const auto& edge) {
      return edge["toNode"] == (*newConnection)["toNode"] && edge["toPort"] == (*newConnection)["toPort"];
    });
    if (match == edges.end()) edges.push_back(std::move(*newConnection));
    else *match = std::move(*newConnection);
    studioEdit(std::move(doc));
    return;
  }
  if (nodes.empty()) return;
  s.selectedNode = std::clamp(s.selectedNode, 0, int(nodes.size()) - 1);
  if (!locate.empty()) {
    for (size_t i = 0; i < nodes.size(); ++i)
      if (nodes[i].at("id") == locate) { s.selectedNode = int(i); break; }
  }
  const auto selected = nodes[size_t(s.selectedNode)].at("id").get<std::string>();
  const bool chooseNode = ImGui::BeginCombo("Node", selected.c_str());
  runUiRecordGlobal("texture-studio/graph/node");
  if (chooseNode) {
    for (size_t i = 0; i < nodes.size(); ++i) {
      const auto id = nodes[i].at("id").get<std::string>();
      const bool clicked = ImGui::Selectable(id.c_str(), int(i) == s.selectedNode);
      runUiRecordGlobal("texture-studio/graph/node/" + id);
      if (clicked) {
        s.selectedNode = int(i);
        locate = id;
      }
    }
    ImGui::EndCombo();
  }
  const auto& n = nodes[size_t(s.selectedNode)];
  const auto id = n.at("id").get<std::string>();
  if (locate == id) {
    const auto position = positions.at(id);
    pan = {pan.x + origin.x + (area.x - card.x) * .5f - position.x,
           pan.y + origin.y + (area.y - heights.at(id)) * .5f - position.y};
  }
  const auto type = n.at("type").get<std::string>();
  if (type != "Subgraph") {
    bool included = s.groupSelection.count(id) != 0;
    if (ImGui::Checkbox("Include selected node in group", &included)) {
      if (included) s.groupSelection.insert(id);
      else s.groupSelection.erase(id);
    }
    runUiRecordGlobal("texture-studio/graph/include");
  }
  if (type == "Subgraph") {
    const auto group = n.at("parameters").at("group").get<std::string>();
    auto& definition = graph.at("groups").at(group);
    static std::array<char, 65536> buffer;
    if (ImGui::Button("Edit subgraph…")) {
      const auto json = definition.dump(2);
      rbTexture::require(json.size() < buffer.size(), "graph", "Edit larger subgraph definitions in the source file.");
      std::copy(json.c_str(), json.c_str() + json.size() + 1, buffer.begin());
      ImGui::OpenPopup("Edit subgraph definition");
    }
    if (ImGui::BeginPopupModal("Edit subgraph definition", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      const auto window = ImGui::GetMainViewport()->WorkSize;
      ImGui::InputTextMultiline("##definition", buffer.data(), buffer.size(),
        {std::min(620 * uiScale(), window.x-72), std::min(350 * uiScale(), window.y-180)});
      const bool attempted = ImGui::Button("Apply");
      if (attempted) studioAction([&] {
        definition = rbTexture::parseJson(buffer.data());
        studioEdit(std::move(doc));
        ImGui::CloseCurrentPopup();
      });
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
      if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
      ImGui::EndPopup();
      if (attempted) return;
    }
  }
  const auto& ports = nodePins.at(id);
  for (const auto& [name, outputType] : ports.outputs) {
    const auto label = "Output: " + name;
    if (!ImGui::BeginCombo(label.c_str(), "Bind to material channel")) continue;
    for (const auto& [channel, port] : rbTexture::channelTypes()) {
      if (outputType != port || !ImGui::Selectable(channel.c_str())) continue;
      doc.source["outputs"][channel] = {{"node", id}, {"port", name}};
      ImGui::EndCombo();
      studioEdit(std::move(doc));
      return;
    }
    ImGui::EndCombo();
  }
  const bool deleteNode = ImGui::Button("Delete node");
  runUiRecordGlobal("texture-studio/graph/delete");
  if (deleteNode) {
    for (auto edge = edges.begin(); edge != edges.end();) {
      if (edge->at("toNode") == id || edge->at("fromNode") == id) edge = edges.erase(edge);
      else ++edge;
    }
    auto& outputs = doc.source["outputs"];
    for (auto output = outputs.begin(); output != outputs.end();) {
      if (output->at("node") == id) output = outputs.erase(output);
      else ++output;
    }
    for (auto key : {"exposedParameters", "layerBindings"}) {
      auto& bindings = doc.source[key];
      for (auto entry = bindings.begin(); entry != bindings.end();) {
        if (entry->is_object() && entry->value("node", "") == id) entry = bindings.erase(entry);
        else ++entry;
      }
    }
    nodes.erase(size_t(s.selectedNode));
    studioEdit(std::move(doc));
    s.groupSelection.erase(id);
    return;
  }
  static std::array<char, 4096> parameters;
  if (type != "Subgraph") ImGui::SameLine();
  if (type != "Subgraph") {
    const bool editParameters = ImGui::Button("Parameters…");
    runUiRecordGlobal("texture-studio/graph/parameters");
    if (editParameters) {
      const auto input = n.at("parameters").dump(2);
      rbTexture::require(input.size() < parameters.size(), "graph", "These node parameters exceed the editing budget.");
      std::copy(input.c_str(), input.c_str() + input.size() + 1, parameters.begin());
      ImGui::OpenPopup("Edit node parameters");
    }
  }
  if (ImGui::BeginPopupModal("Edit node parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(type.c_str());
    ImGui::InputTextMultiline("##node-parameters", parameters.data(), parameters.size(), {440 * uiScale(), 180 * uiScale()});
    runUiRecordGlobal("texture-studio/graph/parameter-input");
    if (runUiLive) {
      auto& field = runUiGlobalRects["texture-studio/graph/parameter-input"];
      field["active"] = ImGui::IsItemActive();
      field["value"] = parameters.data();
    }
    const bool attempted = ImGui::Button("Apply");
    runUiRecordGlobal("texture-studio/graph/parameters-apply");
    if (attempted) {
      studioAction([&] {
        nodes[size_t(s.selectedNode)]["parameters"] = rbTexture::parseJson(parameters.data());
        studioEdit(std::move(doc));
        ImGui::CloseCurrentPopup();
      });
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    runUiRecordGlobal("texture-studio/graph/parameters-cancel");
    if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
    ImGui::EndPopup();
    if (attempted) return;
  }
  for (const auto& [pinName, pinType] : ports.inputs) {
    auto edge = std::find_if(edges.begin(), edges.end(), [&](const auto& e) {
      return e.at("toNode") == id && e.at("toPort") == pinName;
    });
    const auto from = edge == edges.end() ? std::string{} : edge->at("fromNode").get<std::string>();
    const auto fromPin = edge == edges.end() ? std::string{} : edge->at("fromPort").get<std::string>();
    const auto text = from.empty() ? std::string("Unconnected") : from + " / " + fromPin;
    bool change = false;
    const auto label = "Input: " + pinName;
    const bool chooseSource = ImGui::BeginCombo(label.c_str(), text.c_str());
    runUiRecordGlobal("texture-studio/graph/input/" + pinName);
    if (chooseSource) {
      if (!from.empty() && ImGui::Selectable("Disconnect")) {
        edges.erase(edge);
        change = true;
      }
      if (!change) for (const auto& [candidate, sourcePins] : nodePins) {
        if (candidate == id) continue;
        for (const auto& [outputName, outputType] : sourcePins.outputs) {
          if (outputType != pinType) continue;
          const auto name = candidate + " / " + outputName;
          const bool active = candidate == from && outputName == fromPin;
          const bool clicked = ImGui::Selectable(name.c_str(), active);
          runUiRecordGlobal("texture-studio/graph/source/" + candidate + "/" + outputName);
          if (clicked && !active) {
            auto connection = rbTexture::wire(candidate.c_str(), outputName.c_str(), id.c_str(), pinName.c_str());
            if (edge == edges.end()) edges.push_back(std::move(connection));
            else *edge = std::move(connection);
            change = true;
            break;
          }
        }
        if (change) break;
      }
      ImGui::EndCombo();
    }
    if (change) {
      studioEdit(std::move(doc));
      return;
    }
  }
}

static void studioPreview() {
  auto& s = textureStudio;
  using Preview = TextureStudioState::Preview;
  static constexpr const char* subjects[]{"Flat", "Tiled 3 × 3", "Sphere", "Cube", "Game profile", "Selected surface"};
  int mode = s.previewMode;
  const auto& style = ImGui::GetStyle();
  const float previewLabel = ImGui::CalcTextSize("Preview").x + style.ItemInnerSpacing.x;
  const float channelLabel = ImGui::CalcTextSize("Channel").x + style.ItemInnerSpacing.x;
  const float previewField = ImGui::CalcTextSize("Selected surface").x + ImGui::GetFrameHeight() + 2 * style.FramePadding.x;
  const float channelField = ImGui::CalcTextSize("ambientOcclusion").x + ImGui::GetFrameHeight() + 2 * style.FramePadding.x;
  const bool showChannel = s.previewMode < Preview::sphere && s.result.has_value();
  const bool inlineChannel = showChannel && ImGui::GetContentRegionAvail().x >=
    previewLabel + previewField + style.ItemSpacing.x + channelLabel + channelField;
  ImGui::BeginDisabled(s.apply != TextureStudioState::Apply::none);
  if (inlineChannel) ImGui::SetNextItemWidth(previewField);
  const bool choice = ImGui::BeginCombo("Preview", subjects[mode]);
  runUiRecordGlobal("texture-studio/preview-mode");
  if (choice) {
    for (size_t i = 0; i < std::size(subjects); ++i) {
      if (ImGui::Selectable(subjects[i], int(i) == mode)) mode = int(i);
      runUiRecordGlobal("texture-studio/preview-mode/" + std::to_string(i));
    }
    ImGui::EndCombo();
  }
  if (mode != s.previewMode) {
    s.previewMode = Preview(mode);
    if (mode >= Preview::sphere) {
      studioReleaseImage();
      studioSchedule();
    } else if (!s.result || s.result->revision != s.revision) studioSchedule();
    else studioUpload();
  }
  ImGui::EndDisabled();
  if (inlineChannel && s.previewMode < Preview::sphere) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(channelField);
  }
  if (s.previewMode < Preview::sphere && s.result && ImGui::BeginCombo("Channel", s.channel.c_str())) {
    for (const auto& [name, data] : s.result->image.outputs) {
      if (ImGui::Selectable(name.c_str(), name == s.channel)) {
        s.channel = name;
        studioUpload();
      }
    }
    ImGui::EndCombo();
  }
  if (s.previewMode >= Preview::sphere && !textureStudioDoc().source.at("outputs").contains("baseColor"))
    ImGui::TextWrapped("Connect Base Color to preview the material.");
  if (s.previewMode == Preview::target)
    ImGui::TextUnformatted(gameProfileName());
  if (s.previewMode == Preview::surface && s.surfaceData.empty())
    ImGui::TextUnformatted("Select a brush face in the map.");
  const bool canvas3d = s.previewMode == Preview::sphere || s.previewMode == Preview::cube;
  const bool canvas2d = s.previewMode == Preview::flat || s.previewMode == Preview::tiled;
  if (canvas3d && ImGui::CollapsingHeader("Preview settings")) {
    ImGui::BeginDisabled(s.apply != TextureStudioState::Apply::none);
    auto& camera = s.previewCamera;
    int light = camera.softLight ? 1 : 0;
    bool changed = ImGui::Combo("Lighting", &light, "Directional\0Soft\0");
    camera.softLight = light != 0;
    changed |= ImGui::SliderFloat("Relief", &camera.relief, 0, 4, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    changed |= ImGui::SliderFloat("Zoom", &camera.zoom, .4f, 2.5f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::Button("Reset preview view")) { camera = {}; changed = true; }
    if (changed) studioSchedule();
    ImGui::EndDisabled();
  } else if (canvas2d) {
    ImGui::Checkbox("1:1 pixels", &s.actualPixels);
    ImGui::SameLine();
    ImGui::Checkbox("UV grid", &s.previewGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Seams", &s.previewSeams);
  }
  if (s.previewUploadFailed)
    ImGui::TextWrapped("The renderer could not display the preview. Generated textures are still available.");
  if (!s.handle || !s.result) return;
  const auto size = canvas3d || s.previewMode == Preview::surface
    ? rbTexture::Resolution{256, 256} : s.result->previewSize;
  const unsigned copies = s.previewMode == Preview::tiled ? 3 : 1;
  const float footerHeight = ImGui::GetTextLineHeightWithSpacing();
  const float scale = canvas2d && s.actualPixels ? 1.f : std::min(
    std::max(1.f, ImGui::GetContentRegionAvail().x) / (copies * size.width),
    std::max(1.f, ImGui::GetContentRegionAvail().y - footerHeight) / (copies * size.height));
  const ImVec2 tile{size.width * scale, size.height * scale};
  const ImVec2 rect{tile.x * copies, tile.y * copies};
  const auto at = ImGui::GetCursorScreenPos();
  auto* painter = ImGui::GetWindowDrawList();
  const float cell = std::max(12.f, std::max(rect.x, rect.y) / 32);
  for (int row = 0; row * cell < rect.y; ++row)
    for (int col = 0; col * cell < rect.x; ++col) {
      painter->AddRectFilled({at.x + col * cell, at.y + row * cell},
        {at.x + std::min(rect.x, (col + 1) * cell), at.y + std::min(rect.y, (row + 1) * cell)},
        ImGui::GetColorU32(((row ^ col) & 1) ? ImGuiCol_FrameBg : ImGuiCol_Button));
    }
  for (unsigned y = 0; y < copies; ++y)
    for (unsigned x = 0; x < copies; ++x) {
      const ImVec2 a{at.x + x * tile.x, at.y + y * tile.y}, b{a.x + tile.x, a.y + tile.y};
      painter->AddImage((ImTextureID)s.handle, a, b);
      if (canvas2d && s.previewGrid) {
        const auto color = IM_COL32(255, 255, 255, 75);
        for (int i = 1; i < 8; ++i) {
          const float dx = tile.x * i / 8, dy = tile.y * i / 8;
          painter->AddLine({a.x + dx, a.y}, {a.x + dx, b.y}, color);
          painter->AddLine({a.x, a.y + dy}, {b.x, a.y + dy}, color);
        }
      }
      if (canvas2d && s.previewSeams) painter->AddRect(a, b, IM_COL32(241, 173, 70, 230));
    }
  if (canvas3d) {
    ImGui::InvisibleButton("Preview camera", rect, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    runUiRecordGlobal("texture-studio/preview-image");
    if (s.apply == TextureStudioState::Apply::none && ImGui::IsItemActive()) {
      auto& camera = s.previewCamera;
      const auto delta = ImGui::GetIO().MouseDelta;
      if (delta.x != 0 || delta.y != 0) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          camera.yaw = std::remainder(camera.yaw + delta.x * .012f, 6.2831853f);
          camera.pitch = std::clamp(camera.pitch - delta.y * .012f, -1.6f, 1.6f);
          studioSchedule();
        } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
          camera.panX = std::clamp(camera.panX + delta.x / (rect.x * camera.zoom), -2.f, 2.f);
          camera.panY = std::clamp(camera.panY + delta.y / (rect.y * camera.zoom), -2.f, 2.f);
          studioSchedule();
        }
      }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Drag to orbit. Middle-drag to pan.");
  } else {
    ImGui::Dummy(rect);
    runUiRecordGlobal("texture-studio/preview-image");
    if (canvas2d && ImGui::IsItemHovered() && s.result->image.outputs.count(s.channel)) {
      const auto mouse = ImGui::GetIO().MousePos;
      const auto& channel = *s.result->image.outputs.at(s.channel);
      const unsigned x = unsigned(rbTexture::frac((mouse.x - at.x) / tile.x) * channel.size.width);
      const unsigned y = unsigned(rbTexture::frac((mouse.y - at.y) / tile.y) * channel.size.height);
      const auto& pixel = channel.sample(int(x), int(y));
      ImGui::BeginTooltip();
      ImGui::Text("Pixel %u, %u", x, y);
      ImGui::Text("%s %.3f, %.3f, %.3f, %.3f", channel.type == rbTexture::Port::color ? "Linear" : "Data",
                  pixel[0], pixel[1], pixel[2], pixel[3]);
      if (channel.type == rbTexture::Port::color) ImGui::Text("sRGB %.3f, %.3f, %.3f",
        rbTexture::toSrgb(pixel[0]), rbTexture::toSrgb(pixel[1]), rbTexture::toSrgb(pixel[2]));
      ImGui::EndTooltip();
    }
  }
  ImGui::Text("%u × %u%s", size.width, size.height, s.imageRevision != s.revision ? " · Previous result" : "");
}

static void studioModeTabs(bool applying) {
  ImGui::BeginDisabled(applying);
  if (ImGui::BeginTabBar("Editor modes")) {
    const bool quick = ImGui::BeginTabItem("Quick");
    runUiRecordGlobal("texture-studio/quick-tab");
    if (quick) { studioAction(studioQuick); ImGui::EndTabItem(); }
    const bool layers = ImGui::BeginTabItem("Layers");
    runUiRecordGlobal("texture-studio/layers-tab");
    if (layers) { studioAction(studioLayers); ImGui::EndTabItem(); }
    const bool graph = ImGui::BeginTabItem("Node Graph");
    runUiRecordGlobal("texture-studio/graph-tab");
    if (graph) { studioAction(studioNodes); ImGui::EndTabItem(); }
    ImGui::EndTabBar();
  }
  ImGui::EndDisabled();
}

static void studioProjectPrompt(bool busy) {
  auto& s = textureStudio;
  if (s.projectJob.valid()) {
    const auto total = s.projectProgress->total;
    ImGui::TextUnformatted(total == 1 ? "Updating project material…" : "Importing project presets…");
    ImGui::ProgressBar(float(s.projectProgress->finished) / float(total), {-1, 0});
    if (ImGui::Button("Cancel")) {
      s.projectProgress->cancel = true;
      s.message = "Project operation cancelled.";
    }
    runUiRecordGlobal("texture-studio/project/cancel");
    return;
  }
  if (s.projectRoot.empty()) return;
  constexpr const char* title = "Import Texture Studio presets";
  ImGui::BeginDisabled(busy);
  if (ImGui::Button("Import all presets…")) s.projectOffer = true;
  runUiRecordGlobal("texture-studio/project/offer");
  ImGui::EndDisabled();
  if (s.projectOffer && !busy &&
      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    ImGui::OpenPopup(title);
  if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    const auto dismissOffer = [&] {
      s.promptedProjects.insert({s.projectProfile, s.projectRoot});
      s.projectOffer = false;
      ImGui::CloseCurrentPopup();
    };
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 360 * uiScale());
    const auto format = s.projectProfile == 2 ? "one shared WAD2" : s.projectProfile == 4 ? "one shared WAD3" :
      s.projectProfile == 3 ? "WAL textures" : "TGA textures and shaders";
    ImGui::Text("Import all %zu presets into %s?", rbTexture::projectPresets().size(), gameProfileName());
    ImGui::TextUnformatted(format);
    if (s.projectImported) ImGui::TextUnformatted("Existing project edits will be kept.");
    ImGui::PopTextWrapPos();
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("Import all presets")) {
      studioAction([] { studioBeginProjectBake(); });
      if (s.projectJob.valid()) dismissOffer();
    }
    runUiRecordGlobal("texture-studio/project/import");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Later")) dismissOffer();
    runUiRecordGlobal("texture-studio/project/later");
    if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
    ImGui::EndPopup();
  }
}

static void studioProjectGallery(bool busy) {
  auto& s = textureStudio;
  ImGui::TextUnformatted("Project presets");
  const char* family = s.projectFamily == -2 ? "All presets" : s.projectFamily == -1 ? "Built-in" :
    rbTexture::recipeCategories[s.projectFamily];
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##project-preset-family", family)) {
    for (int group = -2; group < int(std::size(rbTexture::recipeCategories)); ++group) {
      const auto title = group == -2 ? "All presets" : group == -1 ? "Built-in" : rbTexture::recipeCategories[group];
      if (ImGui::Selectable(title, group == s.projectFamily)) s.projectFamily = group;
    }
    ImGui::EndCombo();
  }
  const auto* selected = studioProjectPresetByKey(s.projectPreset);
  if (selected) ImGui::TextWrapped("%s", selected->label);
  if (s.projectImported && selected) {
    ImGui::BeginDisabled(busy || cvars->GetFloat("ftebroom_face_count") <= 0);
    if (ImGui::Button("Apply material")) studioAction([&] { studioApplyProjectPreset(*selected); });
    runUiRecordGlobal("texture-studio/project/apply");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(busy || textureStudioDoc().readOnly);
    if (ImGui::Button("Apply changes")) studioAction([&] { studioBeginProjectBake(selected); });
    runUiRecordGlobal("texture-studio/project/update");
    ImGui::EndDisabled();
  }
  ImGui::BeginChild("Texture Studio preset tiles", {0, 0}, ImGuiChildFlags_Borders);
  const float side = 58 * uiScale(), padding = 3 * uiScale();
  const float rowHeight = side + ImGui::GetTextLineHeightWithSpacing() + padding;
  const auto width = ImGui::GetContentRegionAvail().x;
  const auto columns = std::max(1, int((width + padding) / (side + padding)));
  std::vector<const rbTexture::ProjectPreset*> visible;
  for (const auto& preset : rbTexture::projectPresets())
    if (s.projectFamily == -2 || preset.family == s.projectFamily) visible.push_back(&preset);
  ImGuiListClipper clipper;
  clipper.Begin(int((visible.size() + size_t(columns) - 1) / size_t(columns)),
    rowHeight + ImGui::GetStyle().ItemSpacing.y);
  ImGui::BeginDisabled(busy);
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const float y = ImGui::GetCursorPosY();
      for (int column = 0; column < columns && size_t(row * columns + column) < visible.size(); ++column) {
        const auto& preset = *visible[size_t(row * columns + column)];
        const float x = ImGui::GetStyle().WindowPadding.x + column * (side + padding);
        ImGui::SetCursorPos({x, y});
        ImGui::PushID(preset.key.c_str());
        const auto top = ImGui::GetCursorScreenPos();
        const bool click = ImGui::InvisibleButton("preset", {side, rowHeight});
        runUiRecordGlobal("texture-studio/project/preset/" + preset.key);
        const bool hovered = ImGui::IsItemHovered();
        if (click) studioAction([&] { studioChooseProjectPreset(preset); });
        const auto material = s.projectImported ? "textures/" + studioPresetMaterial(preset) : "";
        const auto handle = !material.empty() ? thumbnail(material) :
          studioFindThumbnail("project-" + preset.key, [&preset] { return preset.document(); }, ImGui::IsItemVisible());
        auto* drawList = ImGui::GetWindowDrawList();
        const ImVec2 imageEnd{top.x + side, top.y + side};
        drawList->AddRectFilled(top, imageEnd, ImGui::GetColorU32(hovered ? ImGuiCol_HeaderHovered : ImGuiCol_FrameBg));
        if (handle) drawList->AddImage((ImTextureID)handle, {top.x + padding, top.y + padding},
          {imageEnd.x - padding, imageEnd.y - padding});
        if (s.projectPreset == preset.key)
          drawList->AddRect(top, imageEnd, ImGui::GetColorU32(ImGuiCol_HeaderActive), 0, 0, 2 * uiScale());
        ImGui::RenderTextEllipsis(drawList, {top.x, imageEnd.y + padding}, {imageEnd.x, top.y + rowHeight},
          imageEnd.x, imageEnd.x, preset.label, nullptr, nullptr);
        if (hovered) ImGui::SetTooltip("%s", preset.label);
        ImGui::PopID();
      }
    }
  }
  ImGui::EndDisabled();
  ImGui::EndChild();
}

static void textureStudioPanel() {
  auto& s = textureStudio;
  studioTick();
  studioThumbnailTick();
  if (s.open) studioAction([&] { studioRefreshProjectScope(s.focus); });
  studioProjectTick();
  if (!s.open || jamEditing()) return;
  if (s.previewMode == TextureStudioState::surface && s.apply == TextureStudioState::Apply::none &&
      !(s.scheduled && s.fullSize)) {
    if (ImGui::GetTime() >= s.nextSurfaceQuery) {
      command("ftebroom_texture_apply surface\n");
      s.nextSurfaceQuery = ImGui::GetTime() + .20;
    }
    char data[8192]{};
    cvars->GetString("ftebroom_texture_surface", data, sizeof(data));
    if (s.surfaceData != data) {
      s.surfaceData = data;
      if (s.surfaceData.empty()) studioReleaseImage();
      studioSchedule();
    }
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) s.wireStart = {};
  if (!s.movingNode.empty() && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    if (s.drag) s.document->replace(std::move(*s.drag));
    s.drag.reset();
    s.movingNode.clear();
  }
  if (s.playing && !s.scheduled && !s.job.valid() && s.apply == TextureStudioState::Apply::none) {
    const auto& animation = textureStudioDoc().source.at("animation");
    const auto frames = animation.at("frames").get<unsigned>();
    const auto elapsed = std::max(0.0, ImGui::GetTime() - s.playStarted) * animation.at("fps").get<unsigned>();
    const bool looping = animation.at("loop").get<bool>();
    const auto next = looping ? unsigned(std::fmod(elapsed, frames)) : unsigned(std::min(elapsed, double(frames - 1)));
    if (!looping && next == frames - 1) s.playing = false;
    if (next != s.frame) {
      s.frame = next;
      studioSchedule();
      s.nextRender = ImGui::GetTime();
    }
  }
  if (!s.drag && ImGui::GetTime() >= s.nextAutosave) studioAction(studioAutosave);
  if (std::exchange(s.focus, false)) ImGui::SetNextWindowFocus();
  const auto* viewport = ImGui::GetMainViewport();
  const auto screen = viewport->WorkSize;
  const ImVec2 maximum{std::max(160.f, screen.x - 16), std::max(160.f, screen.y - 16)};
  ImGui::SetNextWindowSizeConstraints({std::min(360.f, maximum.x), std::min(250.f, maximum.y)}, maximum);
  ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_FirstUseEver, {.5f, .5f});
  ImGui::SetNextWindowSize({std::min(1030 * uiScale(), maximum.x), std::min(650 * uiScale(), maximum.y)},
    ImGuiCond_FirstUseEver);
  if (!bbActive) {
    for (const auto* name : {"Face", "Map", "Entity"}) {
      const auto* inspector = ImGui::FindWindowByName(name);
      if (inspector && inspector->DockId) {
        ImGui::SetNextWindowDockID(inspector->DockId, ImGuiCond_FirstUseEver);
        break;
      }
    }
  }
  // The model editor owns a separate workspace. Keep its texture dialog
  // above that workspace without moving the map editor's docked tab.
  const bool contentVisible = ImGui::Begin(bbActive ? "Texture Studio###Model Texture Studio" : "Texture Studio", &s.open,
    bbActive ? ImGuiWindowFlags_NoDocking : ImGuiWindowFlags_None);
  s.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  s.docked = ImGui::GetCurrentWindowRead()->DockIsActive;
  if (contentVisible) {
    const auto nextButton = [](const char* label) {
      const auto& style = ImGui::GetStyle();
      const float nextRight = ImGui::GetItemRectMax().x + style.ItemSpacing.x +
        ImGui::CalcTextSize(label).x + 2 * style.FramePadding.x;
      if (nextRight <= ImGui::GetCurrentWindowRead()->WorkRect.Max.x) ImGui::SameLine();
    };
    if (runUiLive) {
      const auto* window = ImGui::GetCurrentWindowRead();
      runUiGlobalRects["texture-studio/render-state"] = {
        {"revision", s.revision}, {"previewReady", s.handle != 0 && s.imageRevision == s.revision},
        {"theme", uiEffectiveTheme()}, {"scale", uiScale()},
        {"faceCount", int(cvars->GetFloat("ftebroom_face_count"))},
        {"catalogThumbnails", std::count_if(s.thumbnails.begin(), s.thumbnails.end(),
          [](const auto& entry) { return entry.second != 0 && entry.first.rfind("project-", 0) != 0; })},
        {"previewMode", int(s.previewMode)}, {"hasSurface", !s.surfaceData.empty()},
        {"fullSize", s.result && s.result->fullSize && s.result->revision == s.revision},
        {"busy", s.job.valid() || s.inputJob.valid() || s.projectJob.valid() || s.apply != TextureStudioState::Apply::none},
        {"projectImported", s.projectImported}, {"projectPreset", s.projectPreset},
        {"projectAvailable", !s.projectRoot.empty()}, {"projectOfferPending", s.projectOffer},
        {"cameraForward", cvars->GetFloat("ftebroom_forward")},
        {"projectMaterialReady", [&] {
          const auto* preset = studioProjectPresetByKey(s.projectPreset);
          return preset && materials.count("textures/" + studioPresetMaterial(*preset)) != 0;
        }()},
        {"docked", window->DockIsActive},
        {"readingSource", s.inputJob.valid()}, {"readOnly", textureStudioDoc().readOnly}, {"error", s.error}
      };
      if (!window->DockIsActive) {
        const auto title = window->TitleBarRect();
        const auto padding = ImGui::GetStyle().FramePadding;
        const float button = ImGui::GetFontSize();
        runUiGlobalRects["texture-studio/close"] = {
          {"x", title.Max.x - padding.x - button}, {"y", title.Min.y + padding.y},
          {"width", button}, {"height", button}
        };
      } else if (auto* tabs = window->DockNode->TabBar) {
        if (const auto* tab = ImGui::TabBarFindTabByID(tabs, window->TabId)) {
          const auto left = std::max(tabs->ScrollingRectMinX,
            tabs->BarRect.Min.x + tab->Offset - tabs->ScrollingAnim);
          const auto right = std::min(tabs->ScrollingRectMaxX,
            tabs->BarRect.Min.x + tab->Offset - tabs->ScrollingAnim + tab->Width);
          if (right > left) runUiGlobalRects["texture-studio/tab"] = {
            {"x", left}, {"y", tabs->BarRect.Min.y}, {"width", right - left}, {"height", tabs->BarRect.GetHeight()}
          };
        }
      }
    }
    const bool loading = s.inputJob.valid();
    const bool projectBusy = s.projectJob.valid();
    const bool applying = loading || projectBusy || s.apply != TextureStudioState::Apply::none;
    const bool writable = !textureStudioDoc().readOnly;
    const auto& io = ImGui::GetIO();
    const bool shortcuts = s.focused && !io.WantTextInput &&
      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    if (shortcuts && s.apply != TextureStudioState::Apply::commit && ImGui::IsKeyPressed(ImGuiKey_Escape) &&
        (applying || s.job.valid() || s.scheduled || s.drag || !s.wireStart.first.empty())) {
      if (loading) {
        *s.inputCancel = true;
        s.message = "Opening cancelled.";
      } else {
        studioCancel(true);
        s.drag.reset();
      }
    }
    const auto availableHeight = ImGui::GetContentRegionAvail().y;
    const auto galleryHeight = std::clamp(availableHeight * .36f, 102 * uiScale(), 280 * uiScale());
    ImGui::BeginChild("Texture Studio controls", {0, std::max(42.f, availableHeight - galleryHeight)});
    if (runUiLive) {
      const auto& clip = ImGui::GetCurrentWindowRead()->InnerClipRect;
      runUiGlobalRects["texture-studio/controls"] = {
        {"x", clip.Min.x}, {"y", clip.Min.y}, {"width", clip.GetWidth()}, {"height", clip.GetHeight()}
      };
    }
    if (!bbActive) studioProjectPrompt(applying);
    ImGui::BeginDisabled(applying);
    if (ImGui::Button("New")) {
      s.presetSearch.fill(0);
      studioAction(studioLoadLibrary);
      ImGui::OpenPopup("New source?");
    }
    runUiRecordGlobal("texture-studio/new");
    nextButton("Open…");
    if (ImGui::Button("Open…")) ImGui::OpenPopup("Open source?");
    runUiRecordGlobal("texture-studio/open");
    nextButton("Import Image…");
    if (ImGui::Button("Import Image…")) ImGui::OpenPopup("Import original image");
    runUiRecordGlobal("texture-studio/import");
    if (ImGui::BeginPopupModal("Import original image", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      static char license[129]{};
      static char textureName[16]{};
      ImGui::TextUnformatted("PNG, JPEG, TGA, WAD2, WAD3 or WAL. The original file is preserved.");
      ImGui::InputTextWithHint("License", "SPDX or private license identifier", license, sizeof(license));
      runUiRecordGlobal("texture-studio/import-license");
      ImGui::InputTextWithHint("WAD texture name", "Optional for single-texture WADs", textureName, sizeof(textureName));
      ImGui::BeginDisabled(!rbTexture::licenseId(license));
      if (ImGui::Button("Choose Image…")) {
        ImGui::CloseCurrentPopup();
        studioImportRaster(license, textureName);
      }
      runUiRecordGlobal("texture-studio/import-choose");
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    nextButton("Recover…");
    if (ImGui::Button("Recover…")) studioAction([&] {
      studioRecoverableFiles();
      ImGui::OpenPopup("Recover source");
    });
    runUiRecordGlobal("texture-studio/recover");
    if (ImGui::BeginPopupModal("Recover source", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      if (s.recoverable.empty()) ImGui::TextUnformatted("There are no recovery copies.");
      else ImGui::TextUnformatted("Save your current source before opening a recovery copy.");
      for (const auto& path : s.recoverable) {
        const auto id = path.stem().stem().u8string();
        const bool restore = ImGui::Selectable(id.c_str());
        runUiRecordGlobal("texture-studio/recover/" + id);
        if (!restore) continue;
        studioAction([&] {
          auto doc = rbTexture::open(ftebroom::bb::readFile(path, rbTexture::maxSourceBytes));
          rbTexture::require(!doc.readOnly, "recovery", "Open a newer recovery copy using Open.");
          auto assets = rbTexture::loadAssets(doc, path.parent_path());
          rbTexture::compile(doc, {}, assets);
          doc.source["documentId"] = ftebroom::bb::uuid();
          studioAutosave();
          studioCancel();
          s.file.clear();
          s.recoveryFile.clear();
          s.savedSource.clear();
          s.recoveredSource.clear();
          s.assetFolder = path.parent_path();
          s.assets = std::move(assets);
          s.drag.reset();
          s.document.emplace(std::move(doc));
          s.variationBase = textureStudioDoc().seed();
          s.channel = "baseColor";
          s.selectedNode = 0;
          s.parameterLocks.clear();
          s.frame = 0;
          studioSchedule();
          ImGui::CloseCurrentPopup();
        });
        break;
      }
      if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("New source?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      static std::string presetActionId, presetActionLabel;
      static std::array<char, 81> duplicateName{};
      const char* presetDialog = nullptr;
      const float listWidth = std::min(490 * uiScale(), screen.x - 60);
      const float listHeight = std::clamp(screen.y - 195 * uiScale(), 110.f, 345 * uiScale());
      ImGui::TextUnformatted("Choose a preset. Save your current source before replacing it.");
      ImGui::SetNextItemWidth(listWidth);
      ImGui::InputTextWithHint("##preset-search", "Search presets", s.presetSearch.data(), s.presetSearch.size());
      runUiRecordGlobal("texture-studio/preset-search");
      ImGui::SetNextItemWidth(180);
      ImGui::Combo("Show", &s.libraryFilter, "All presets\0Favorites\0Recently used\0");
      const auto lowercase = [](std::string value) {
        for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return value;
      };
      const auto query = lowercase(s.presetSearch.data());
      std::optional<rbTexture::Document> selected;
      rbTexture::Assets selectedAssets;
      std::filesystem::path selectedAssetRoot;
      std::string selectedId;
      std::string selectedProjectPreset;
      if (ImGui::BeginChild("preset-library", ImVec2(listWidth, listHeight), ImGuiChildFlags_Borders)) {
        bool matched = query.empty() && !s.libraryFilter;
        const auto permitted = [&](const std::string& id) {
          if (s.libraryFilter == 1) return s.favorites.count(id) != 0;
          if (s.libraryFilter == 2) return std::find(s.recentPresets.begin(), s.recentPresets.end(), id) != s.recentPresets.end();
          return true;
        };
        const auto favorite = [&](const std::string& id) {
          ImGui::SameLine();
          ImGui::PushID(id.c_str());
          const bool saved = s.favorites.count(id);
          if (ImGui::SmallButton(saved ? "★" : "☆")) studioAction([&] {
            if (saved) s.favorites.erase(id);
            else s.favorites.insert(id);
            studioSaveLibrary();
          });
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", saved ? "Remove from favorites" : "Add to favorites");
          ImGui::PopID();
        };
        const auto matchesRole = [&](const rbTexture::TextureFamily& entry, const char* role) {
          return permitted(std::string(entry.id) + "-" + role) &&
            lowercase(std::string(entry.name) + " " + role).find(query) != std::string::npos;
        };
        const auto matchesFamily = [&](const rbTexture::TextureFamily& entry) {
          return std::any_of(std::begin(rbTexture::familyRoles), std::end(rbTexture::familyRoles),
            [&](const char* role) { return matchesRole(entry, role); });
        };
        if (std::any_of(std::begin(rbTexture::textureFamilies), std::end(rbTexture::textureFamilies), matchesFamily) &&
            (!query.empty() || s.libraryFilter || ImGui::CollapsingHeader("Texture families"))) {
          static uint32_t familySeed = 24058;
          ImGui::SetNextItemWidth(140);
          ImGui::InputScalar("Family seed##new", ImGuiDataType_U32, &familySeed, nullptr, nullptr, "%u");
          runUiRecordGlobal("texture-studio/family-seed");
          for (size_t family = 0; family < std::size(rbTexture::textureFamilies); ++family) {
            const auto& entry = rbTexture::textureFamilies[family];
            if (!matchesFamily(entry)) continue;
            ImGui::PushID(entry.id);
            if (ImGui::TreeNodeEx(entry.name, query.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
              for (size_t role = 0; role < std::size(rbTexture::familyRoles); ++role) {
                const auto id = std::string(entry.id) + "-" + rbTexture::familyRoles[role];
                if (!matchesRole(entry, rbTexture::familyRoles[role])) continue;
                matched = true;
                const auto width = studioPresetThumbnail(id, [family, role] {
                  return rbTexture::familyMaterial(family, role, 24058u);
                });
                if (ImGui::Selectable(rbTexture::familyRoles[role], false, 0, {width, 32 * uiScale()})) {
                  selected = rbTexture::familyMaterial(family, role, familySeed);
                  selected->source["documentId"] = ftebroom::bb::uuid();
                  selectedId = id;
                }
                runUiRecordGlobal("texture-studio/family/" + id);
                favorite(id);
              }
              ImGui::TreePop();
            }
            ImGui::PopID();
          }
        }
        if (!query.empty() || s.libraryFilter || ImGui::CollapsingHeader("Featured", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (unsigned preset = 0; preset < rbTexture::presetNames.size(); ++preset) {
            const auto id = "featured-" + std::to_string(preset);
            if (!permitted(id) || lowercase(rbTexture::presetNames[preset]).find(query) == std::string::npos) continue;
            matched = true;
            const auto width = studioPresetThumbnail(id, [preset] { return rbTexture::builtinPreset(preset, "thumbnail"); });
            if (ImGui::Selectable(rbTexture::presetNames[preset], false, 0, {width, 32 * uiScale()})) {
              selected = rbTexture::builtinPreset(preset, ftebroom::bb::uuid());
              selectedId = id;
              selectedProjectPreset = rbTexture::projectPresets().at(preset).key;
            }
            runUiRecordGlobal("texture-studio/preset/" + std::to_string(preset));
            favorite(id);
          }
        }
        for (size_t category = 0; category < std::size(rbTexture::catalogRecipes); ++category) {
          ImGui::PushID(int(category));
          const auto title = rbTexture::recipeCategories[category];
          if (!query.empty() || s.libraryFilter || ImGui::CollapsingHeader(title)) {
            const bool categoryMatches = lowercase(title).find(query) != std::string::npos;
            for (size_t index = 0; index < std::size(rbTexture::catalogRecipes[category]); ++index) {
              const auto name = rbTexture::catalogRecipes[category][index].name;
              const auto id = rbTexture::recipeId(category, index);
              if (!permitted(id) || (!categoryMatches && lowercase(name).find(query) == std::string::npos)) continue;
              matched = true;
              const auto count = rbTexture::catalogVariationCount(category, index);
              const auto width = studioPresetThumbnail(id, [category, index] {
                return rbTexture::catalogPreset(category, index, "thumbnail");
              });
              const auto variantsWidth = count ? ImGui::CalcTextSize("Variants").x +
                2.f * ImGui::GetStyle().FramePadding.x + ImGui::GetStyle().ItemSpacing.x : 0.f;
              const auto choose = [&](size_t variation) {
                selected = rbTexture::catalogPreset(category, index, ftebroom::bb::uuid());
                if (variation) rbTexture::applyCatalogVariation(*selected, category, index, variation);
                selectedId = id;
                if (!variation) selectedProjectPreset = id;
              };
              if (ImGui::Selectable(name, false, 0, {std::max(1.f, width - variantsWidth), 32 * uiScale()}))
                choose(0);
              runUiRecordGlobal("texture-studio/recipe/" + id);
              if (count) {
                ImGui::SameLine();
                ImGui::PushID(id.c_str());
                if (ImGui::SmallButton("Variants")) ImGui::OpenPopup("Catalog variants");
                runUiRecordGlobal("texture-studio/recipe/" + id + "/variants");
                if (ImGui::BeginPopup("Catalog variants")) {
                  for (size_t variation = 1; variation <= count; ++variation) {
                    studioPresetThumbnail(id + "#" + std::to_string(variation), [category, index, variation] {
                      auto doc = rbTexture::catalogPreset(category, index, "thumbnail");
                      rbTexture::applyCatalogVariation(doc, category, index, variation);
                      return doc;
                    });
                    const float labelWidth = 160.f * uiScale();
                    if (ImGui::Selectable(rbTexture::catalogVariationName(category, index, variation),
                        false, 0, {labelWidth, 32 * uiScale()})) choose(variation);
                    runUiRecordGlobal("texture-studio/recipe/" + id + "/variants/" + std::to_string(variation));
                  }
                  ImGui::EndPopup();
                }
                ImGui::PopID();
              }
              favorite(id);
            }
          }
          ImGui::PopID();
        }
        if (!s.userPresets.empty() && (!query.empty() || s.libraryFilter || ImGui::CollapsingHeader("My presets"))) {
          for (const auto& [id, label] : s.userPresets) {
            if (!permitted(id) || lowercase(label).find(query) == std::string::npos) continue;
            matched = true;
            ImGui::PushID(id.c_str());
            if (ImGui::Selectable(label.c_str(), false, 0, {320, 0})) studioAction([&] {
              const auto folder = studioUserPresetFolder();
              auto doc = studioReadUserPreset(id);
              auto assets = rbTexture::loadAssets(doc, folder);
              rbTexture::compile(doc, {}, assets);
              doc.source["documentId"] = ftebroom::bb::uuid();
              selected = std::move(doc);
              selectedAssets = std::move(assets);
              selectedAssetRoot = folder;
              selectedId = id;
            });
            runUiRecordGlobal("texture-studio/user-preset/" + id);
            if (ImGui::BeginPopupContextItem()) {
              ImGui::BeginDisabled(s.userPresets.size() >= 64);
              const bool duplicate = ImGui::MenuItem("Duplicate preset…");
              runUiRecordGlobal("texture-studio/user-preset/duplicate");
              ImGui::EndDisabled();
              const bool remove = ImGui::MenuItem("Remove preset…");
              runUiRecordGlobal("texture-studio/user-preset/remove");
              if (duplicate || remove) {
                presetActionId = id;
                presetActionLabel = label;
                duplicateName.fill(0);
                presetDialog = duplicate ? "Duplicate user preset" : "Remove user preset";
              }
              ImGui::EndPopup();
            }
            favorite(id);
            ImGui::PopID();
          }
        }
        if (!matched) ImGui::TextDisabled("No presets match this view.");
      }
      ImGui::EndChild();
      if (presetDialog) ImGui::OpenPopup(presetDialog);
      if (ImGui::BeginPopupModal("Duplicate user preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputTextWithHint("Name", presetActionLabel.c_str(), duplicateName.data(), duplicateName.size());
        runUiRecordGlobal("texture-studio/user-preset/duplicate-name");
        ImGui::BeginDisabled(!duplicateName[0]);
        if (ImGui::Button("Duplicate")) studioAction([&] {
          auto doc = studioReadUserPreset(presetActionId);
          const auto folder = studioUserPresetFolder();
          auto assets = rbTexture::loadAssets(doc, folder);
          studioSaveUserPreset(std::move(doc), assets, folder, duplicateName.data());
          ImGui::CloseCurrentPopup();
        });
        runUiRecordGlobal("texture-studio/user-preset/duplicate-confirm");
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
        ImGui::EndPopup();
      }
      if (ImGui::BeginPopupModal("Remove user preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Remove %s from your library?", presetActionLabel.c_str());
        if (ImGui::Button("Remove")) studioAction([&] {
          studioRemoveUserPreset(presetActionId);
          ImGui::CloseCurrentPopup();
        });
        runUiRecordGlobal("texture-studio/user-preset/remove-confirm");
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
        ImGui::EndPopup();
      }
      if (selected) {
        studioAction([&] {
          if (const auto* preset = studioProjectPresetByKey(selectedProjectPreset))
            studioChooseProjectPreset(*preset);
          else {
            studioAutosave();
            studioNewDocument(std::move(*selected));
            s.assets = std::move(selectedAssets);
            s.assetFolder = std::move(selectedAssetRoot);
          }
          ImGui::CloseCurrentPopup();
          studioRecordPreset(selectedId);
        });
      }
      if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
      if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
      runUiRecordGlobal("texture-studio/catalog-cancel");
      ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("Open source?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Save your current source before replacing it.");
      if (ImGui::Button("Choose file…")) { ImGui::CloseCurrentPopup(); studioOpenFile(); }
      runUiRecordGlobal("texture-studio/open-choose");
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    nextButton("Save");
    if (ImGui::Button("Save")) {
      if (s.file.empty() || !writable) studioSaveAs();
      else studioAction([&] { studioSave(s.file); });
    }
    runUiRecordGlobal("texture-studio/save");
    nextButton("Save As…");
    if (ImGui::Button("Save As…")) studioSaveAs();
    if (writable) {
      nextButton("Save preset…");
      if (ImGui::Button("Save preset…")) ImGui::OpenPopup("Save user preset");
      runUiRecordGlobal("texture-studio/save-preset");
      if (ImGui::BeginPopupModal("Save user preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char label[81]{};
        ImGui::InputText("Name", label, sizeof(label));
        runUiRecordGlobal("texture-studio/save-preset-name");
        ImGui::BeginDisabled(!label[0]);
        if (ImGui::Button("Save preset")) studioAction([&] {
          studioSaveUserPreset(textureStudioDoc(), s.assets, s.assetFolder, label);
          ImGui::CloseCurrentPopup();
        });
        runUiRecordGlobal("texture-studio/save-preset-confirm");
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
        ImGui::EndPopup();
      }
    }
    nextButton("Save Package…");
    ImGui::BeginDisabled(!writable);
    if (ImGui::Button("Save Package…")) studioSavePackage();
    runUiRecordGlobal("texture-studio/save-package");
    ImGui::EndDisabled();
    nextButton("Undo");
    ImGui::BeginDisabled(!writable);
    const bool keyboardUndo = shortcuts && !applying && writable && !io.KeyShift &&
      ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z);
    if ((ImGui::Button("Undo") || keyboardUndo) && s.document->undo()) { s.drag.reset(); studioSchedule(); }
    nextButton("Redo");
    const bool keyboardRedo = shortcuts && !applying && writable &&
      (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z));
    if ((ImGui::Button("Redo") || keyboardRedo) && s.document->redo()) { s.drag.reset(); studioSchedule(); }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (writable) ImGui::TextWrapped("%s · %s", textureStudioDoc().source.at("name").get_ref<const std::string&>().c_str(),
                                    gameProfileName());
    else ImGui::TextUnformatted(gameProfileName());
    ImGui::BeginDisabled(!writable || applying);
    if (ImGui::Button("Generate")) studioAction([&] {
      s.playing = false;
      s.message.clear();
      studioAutosave();
      studioSchedule(true);
    });
    runUiRecordGlobal("texture-studio/generate");
    nextButton("Export…");
    ImGui::BeginDisabled(!s.result || s.result->revision != s.revision || !s.result->fullSize);
    if (ImGui::Button("Export…")) { s.playing = false; ImGui::OpenPopup("Export texture"); }
    runUiRecordGlobal("texture-studio/export");
    ImGui::EndDisabled();
    studioExportDialog();
    const auto faces = cvars->GetFloat("ftebroom_face_count");
    const bool assetOnly = faces <= 0;
    const char* generate = assetOnly ? "Generate Asset" : "Generate & Apply";
    nextButton(generate);
    ImGui::BeginDisabled(!studioProfileAllowsApply());
    if (!(s.projectImported && !s.projectPreset.empty())) {
      if (ImGui::Button(generate)) studioAction([&] { studioBeginApply(assetOnly); });
      runUiRecordGlobal("texture-studio/apply");
    }
    ImGui::EndDisabled();
    if (bbActive) {
      nextButton("Apply to model texture");
      ImGui::BeginDisabled(!studioModelSlotAvailable());
      if (ImGui::Button("Apply to model texture")) studioAction(studioBeginModelApply);
      runUiRecordGlobal("texture-studio/apply-model");
      ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
    const bool cancelling = s.job.valid() && s.cancel && s.cancel->load() && !s.scheduled;
    if (!projectBusy && loading) {
      if (!*s.inputCancel) {
        nextButton("Cancel");
        if (ImGui::Button("Cancel")) {
          *s.inputCancel = true;
          s.message = "Opening cancelled.";
        }
        runUiRecordGlobal("texture-studio/cancel-input");
        if (!*s.inputCancel) ImGui::TextUnformatted("Reading material source…");
      }
    } else if (!projectBusy && !cancelling && (s.job.valid() || s.apply != TextureStudioState::Apply::none)) {
      nextButton("Cancel");
      ImGui::BeginDisabled(s.apply == TextureStudioState::Apply::commit);
      const bool cancel = ImGui::Button("Cancel");
      runUiRecordGlobal("texture-studio/cancel");
      ImGui::EndDisabled();
      if (cancel) studioCancel(true);
      else ImGui::TextUnformatted("Generating…");
    }
    if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
    else if (!s.message.empty()) ImGui::TextWrapped("%s", s.message.c_str());
    if (!writable) ImGui::TextWrapped("This source uses a newer format. Save As preserves its original bytes.");
    const bool stacked = ImGui::GetContentRegionAvail().x < std::min(660 * uiScale(), 980.f);
    if (writable && stacked) {
      const float height = std::clamp(ImGui::GetContentRegionAvail().x, 220 * uiScale(), 310 * uiScale());
      ImGui::BeginChild("Texture Preview", {-1, height}, ImGuiChildFlags_Borders);
      studioAction(studioPreview);
      ImGui::EndChild();
      studioModeTabs(applying);
    } else if (writable && ImGui::BeginTable("Texture Studio Workspace", 2, ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 1.1f);
      ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch, 1.f);
      ImGui::TableNextColumn();
      studioAction(studioPreview);
      ImGui::TableNextColumn();
      studioModeTabs(applying);
      ImGui::EndTable();
    }
    ImGui::EndChild();
    studioProjectGallery(applying);
  }
  ImGui::End();
  if (!s.open) {
    studioCancel();
    if (s.document) studioAction(studioAutosave);
    studioReleaseImage();
    studioReleaseThumbnails();
    s.result.reset();
    s.previewCache = std::make_shared<rbTexture::NodeCache>(studioPreviewCacheBudget);
  }
}

static void textureStudioShutdown() {
  studioCancel();
  if (textureStudio.job.valid()) textureStudio.job.wait();
  if (textureStudio.inputJob.valid()) textureStudio.inputJob.wait();
  if (textureStudio.projectJob.valid()) textureStudio.projectJob.wait();
  if (textureStudio.thumbnailJob.valid()) textureStudio.thumbnailJob.wait();
  if (textureStudio.document && ImGui::GetCurrentContext())
    studioAction(studioAutosave);
  studioReleaseImage();
  studioReleaseThumbnails();
}
