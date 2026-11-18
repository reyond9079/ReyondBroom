// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "texture/legacy_codec.h"
#include "texture/jpeg.h"
#include "texture/catalog.h"
#include "texture/layers.h"
#include "texture/package.h"
#include "texture/preview.h"
#include <thread>

namespace rbTexture = ftebroom::textureStudio;

struct TextureStudioState {
  enum class Apply { none, capture, baking, verify, commit, model };
  enum Preview { flat, tiled, sphere, cube, target, surface };
  struct ModelTarget {
    uint64_t epoch = 0, revision = 0, textureRevision = 0;
    size_t index = 0;
    std::string id;
  };
  struct Result {
    unsigned revision = 0;
    bool fullSize = false;
    rbTexture::Evaluation image{};
    std::map<std::string,std::string> preview;
    std::vector<unsigned char> modelPixels;
  };

  bool open = false, focus = false, focused = false, scheduled = false, fullSize = false;
  bool playing = false;
  Preview previewMode = flat;
  unsigned revision = 0, imageRevision = 0, frame = 0;
  double deadline = 0, nextRender = 0, nextAutosave = 0;
  double playStarted = 0;
  double nextSurfaceQuery = 0;
  uint32_t variationBase = 0;
  int handle = 0, slot = 0, ticket = 0, epoch = 0, profile = 0, selectedNode = 0;
  std::string channel = "baseColor", message, error, material, wadReference;
  std::string savedSource, recoveredSource;
  std::string surfaceData;
  std::string movingNode, wireStart;
  std::set<std::string> groupSelection;
  std::array<char, 96> presetSearch{};
  int catalogVariation = 0;
  int libraryFilter = 0;
  bool libraryLoaded = false;
  std::set<std::string> favorites;
  std::vector<std::string> recentPresets;
  std::vector<std::pair<std::string, std::string>> userPresets;
  std::filesystem::path file, destination, recoveryFile, assetFolder;
  std::vector<std::filesystem::path> recoverable;
  std::optional<rbTexture::History> document;
  ModelTarget modelTarget;
  std::optional<rbTexture::Document> drag;
  rbTexture::Assets assets;
  std::shared_ptr<std::atomic<bool>> cancel;
  std::shared_ptr<rbTexture::NodeCache> previewCache = std::make_shared<rbTexture::NodeCache>(32 * 1024 * 1024);
  std::future<Result> job;
  std::optional<Result> result;
  std::vector<std::filesystem::path> created;
  Apply apply = Apply::none;
};

static TextureStudioState textureStudio;

static bool textureStudioFocused() {
  const auto& s = textureStudio;
  return s.open && (s.focused || s.focus) && !jamEditing();
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
  if (s.document && !textureStudioDoc().readOnly)
    s.frame = std::min(s.frame, textureStudioDoc().source.at("animation").at("frames").get<unsigned>() - 1);
  ++s.revision;
  s.error.clear();
  s.scheduled = true;
  s.fullSize = full;
  s.nextRender = ImGui::GetTime() + (full ? 0 : .10);
}

static void studioEdit(rbTexture::Document document) {
  rbTexture::compile(document, {}, textureStudio.assets);
  textureStudio.document->replace(std::move(document));
  textureStudio.drag.reset();
  studioSchedule();
  textureStudio.nextRender = ImGui::GetTime();
}

static void studioNewDocument(rbTexture::Document document) {
  auto& s = textureStudio;
  s.file.clear();
  s.drag.reset();
  s.movingNode.clear();
  s.wireStart.clear();
  s.groupSelection.clear();
  s.document.emplace(std::move(document));
  s.assets.clear();
  s.assetFolder.clear();
  s.savedSource = s.document->get().save();
  s.recoveredSource.clear();
  s.recoveryFile.clear();
  s.nextAutosave = ImGui::GetTime() + 60;
  s.variationBase = s.document->get().seed();
  s.selectedNode = 0;
  s.frame = 0;
  s.playing = false;
  s.channel = "baseColor";
  studioSchedule();
}

static void studioNew(unsigned preset = 0) {
  studioNewDocument(rbTexture::builtinPreset(preset, ftebroom::bb::uuid()));
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

static void studioCancel() {
  auto& s = textureStudio;
  s.playing = false;
  if (!s.movingNode.empty()) s.drag.reset();
  s.movingNode.clear();
  s.wireStart.clear();
  s.groupSelection.clear();
  if (s.cancel) *s.cancel = true;
  ++s.revision;
  s.scheduled = false;
  if (s.apply != TextureStudioState::Apply::commit) s.apply = TextureStudioState::Apply::none;
}

static void studioReleaseImage() {
  auto& s = textureStudio;
  if (s.handle) draw->UnloadImage(s.handle);
  s.handle = 0;
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
}

static void studioRollback() {
  auto& s = textureStudio;
  for (auto it = s.created.rbegin(); it != s.created.rend(); ++it) {
    std::error_code error;
    std::filesystem::remove(*it, error);
    if (error) s.error = "Unable to remove an incomplete generated asset: " + it->filename().u8string();
  }
  s.created.clear();
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

static void studioBeginApply() {
  auto& s = textureStudio;
  s.playing = false;
  if (!studioProfileAllowsApply()) throw std::runtime_error("Apply for this profile needs a profile-specific encoder.");
  if (documentFile.empty()) throw std::runtime_error("Open a project map before applying a texture.");
  s.destination = studioDestination();
  if (s.destination.empty() || !std::filesystem::is_directory(s.destination))
    throw std::runtime_error("Choose a writable game or mod folder in Game profile.");
  studioAutosave();
  s.epoch = organizationMapEpoch;
  s.profile = gameProfile;
  s.error.clear();
  s.apply = TextureStudioState::Apply::capture;
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
    const auto bytes = rbTexture::readOriginal(s.assetFolder, reference);
    rbTexture::require(bytes.size() <= studioModelAssetLimit - retainedBytes, "assets",
      "A model material can retain up to 16 MiB of original images. Reduce the source size before applying it.");
    retainedBytes += bytes.size();
    originals[rbTexture::idField(reference, "sha256", "assets")] = ftebroom::bb::base64(bytes);
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
  const auto& image = *s.result->image.outputs.at("baseColor");
  const auto source = graph.save();
  const auto digest = ftebroom::jam::sha256(source);
  const bool quake1 = s.profile == 2, quake2 = s.profile == 3, goldsrc = s.profile == 4;
  const bool wad = quake1 || goldsrc;
  std::string palette;
  if (quake1) palette = q1Palette();
  if (quake2) palette = q2Palette();
  if (goldsrc) palette = studioGoldSrcPalette();
  const auto paletteHash = palette.empty() ? std::string{} : ftebroom::sha256(palette);
  const auto hash = ftebroom::sha256(digest + "/frame/" + std::to_string(s.frame) + "/palette/" + paletteHash);
  const auto id = wad ? "rb_" + hash.substr(0, 12) : hash.substr(0, quake2 ? 16 : 20);
  const auto name = wad ? id : "reyond-studio/" + id;
  const auto format = quake1 ? rbTexture::IndexedFormat::wad2 :
    goldsrc ? rbTexture::IndexedFormat::wad3 : rbTexture::IndexedFormat::wal;
  const auto imageBytes = palette.empty() ? rbTexture::tga(image) : rbTexture::indexed(image, name, palette, format);
  s.material = "textures/" + name;
  const auto root = std::filesystem::canonical(s.destination);
  const std::filesystem::path relative = wad ? "texture-wads" : "textures/reyond-studio";
  const auto folder = root / relative;
  if (std::filesystem::weakly_canonical(folder) != folder)
    throw std::runtime_error("The generated-texture folder must be inside the real game texture directory.");
  std::filesystem::create_directories(folder);
  studioCopyOriginals(folder);
  const auto insert = [&](std::string suffix, const std::string& bytes) {
    const auto file = folder / (id + suffix);
    if (std::filesystem::weakly_canonical(file) != file)
      throw std::runtime_error("The generated asset path points through a filesystem link.");
    if (std::filesystem::exists(file)) {
      if (ftebroom::bb::readFile(file) != bytes)
        throw std::runtime_error("A different asset already uses this content name.");
      return;
    }
    ftebroom::bb::atomicWrite(file, bytes);
    s.created.push_back(file);
  };
  const auto manifest = rbTexture::Json{
    {"version", 1}, {"generatorVersion", rbTexture::generatorVersion}, {"profile", gameProfileName()},
    {"material", s.material}, {"sourceHash", digest}, {"frame", s.frame}, {"paletteSha256", paletteHash},
    {"identity", ftebroom::jam::sha256(rbTexture::renderIdentity(graph))}, {"runtimeHash", ftebroom::jam::sha256(imageBytes)}};
  insert(wad ? ".wad" : quake2 ? ".wal" : ".tga", imageBytes);
  insert(".rbtex.json", graph.save());
  insert(".reyond.json", manifest.dump(2) + "\n");
  s.wadReference.clear();
  if (wad) {
    const auto file = folder / (id + ".wad");
    auto reference = file.lexically_relative(documentFile.parent_path());
    if (reference.empty()) reference = relative / file.filename();
    s.wadReference = reference.generic_u8string();
    rbTexture::require(s.wadReference.find_first_of(";\"\r\n") == std::string::npos, "wad",
      "Choose a map location without WAD separators or quotes in its path.");
    q1IndexAssets();
    resetMaterialBrowser();
    publishMaterialTags();
    cvars->SetFloat("ftebroom_material_count", float(materials.size()));
  } else command("fs_flush\nftebroom_assets_ready\n");
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
    s.created.clear();
    throw std::runtime_error("Apply was not acknowledged. The generated asset remains available.");
  }
  char value[80]{};
  if (ack) cvars->GetString("ftebroom_texture_apply_state", value, sizeof value);
  const std::string status(value);
  if (s.apply == A::capture && status == "captured") {
    s.apply = A::baking;
    studioSchedule(true);
  } else if (s.apply == A::verify && status == "checked") {
    studioInstall();
    s.apply = A::commit;
    studioQCSend("commit", s.material);
  } else if (s.apply == A::commit && (status == "applied" || status == "unchanged")) {
    s.created.clear();
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
      s.job = std::async(std::launch::async, [doc = std::move(doc), assets = s.assets, revision, full, model, frame, mode,
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
              mode == TextureStudioState::cube, stop.get())));
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
        return TextureStudioState::Result{revision, full, std::move(result), std::move(pngs), std::move(modelPixels)};
      });
    }
  } catch (const std::exception& e) {
    s.error = e.what();
    s.apply = TextureStudioState::Apply::none;
    studioRollback();
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
    rbTexture::require(embedded.is_object() && refs.size() <= 64 && embedded.size() == refs.size(),
      "assets", "A model material is missing its original images.");
    const auto folder = studioRecoveryRoot();
    std::filesystem::create_directories(folder);
    size_t retainedBytes = 0;
    for (const auto& record : refs) {
      const auto hash = rbTexture::idField(record, "sha256", "assets");
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

static void studioSaveUserPreset(const std::string& label) {
  studioLoadLibrary();
  auto& s = textureStudio;
  rbTexture::require(s.userPresets.size() < 64, "library", "The user preset library holds up to 64 presets.");
  auto doc = textureStudioDoc();
  const auto id = ftebroom::bb::uuid();
  doc.source["name"] = label;
  doc.source["documentId"] = id;
  const auto folder = documentPath("data/texture-studio/user-presets");
  std::filesystem::create_directories(folder);
  rbTexture::compile(doc, {}, textureStudio.assets);
  studioCopyOriginals(folder);
  ftebroom::bb::atomicWrite(folder / (id + ".rbtex.json"), doc.save());
  s.userPresets.emplace_back(id, label);
  studioSaveLibrary();
  s.message = "Saved to your preset library.";
}

static void studioOpenFile() {
  auto& s = textureStudio;
  runUiChoose(s.file.u8string(), [](const std::string& name) {
    studioAction([&] {
      auto path = std::filesystem::u8path(name);
      auto folder = path.parent_path();
      rbTexture::Document doc;
      rbTexture::Assets assets;
      const bool archived = entityLower(path.extension().u8string()) == ".rbtexpkg";
      if (archived) {
        auto package = rbTexture::unpack(path);
        folder = studioRecoveryRoot();
        std::filesystem::create_directories(folder);
        for (const auto& [relative, bytes] : package.originals)
          studioWriteOriginal(folder, relative, bytes);
        doc = std::move(package.document);
        assets = std::move(package.assets);
      } else {
        doc = rbTexture::open(ftebroom::bb::readFile(path, rbTexture::maxSourceBytes));
        if (!doc.readOnly) assets = rbTexture::loadAssets(doc, folder);
      }
      studioCancel();
      textureStudio.drag.reset();
      textureStudio.file = archived ? std::filesystem::path{} : path;
      textureStudio.document.emplace(std::move(doc));
      textureStudio.assetFolder = std::move(folder);
      textureStudio.assets = std::move(assets);
      textureStudio.savedSource = textureStudioDoc().save();
      textureStudio.recoveryFile.clear();
      textureStudio.recoveredSource.clear();
      textureStudio.nextAutosave = ImGui::GetTime() + 60;
      textureStudio.selectedNode = 0;
      textureStudio.frame = 0;
      if (!textureStudioDoc().readOnly) {
        textureStudio.variationBase = textureStudioDoc().seed();
        studioSchedule();
      }
    });
  });
}

static void studioImportRaster(std::string license, std::string textureName) {
  for (char& c : textureName) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  runUiChoose(textureStudio.file.parent_path().u8string(), [license = std::move(license),
    textureName = std::move(textureName)](const std::string& name) {
    studioAction([&] {
      const auto file = std::filesystem::u8path(name);
      auto bytes = rbTexture::imageFileBytes(file);
      const auto digest = ftebroom::sha256(bytes);
      const auto relative = rbTexture::imageRelativePath(digest);
      rbTexture::Json reference{{"id", "original"}, {"path", relative}, {"sha256", digest}, {"license", license}};
      const auto encoding = rbTexture::sourceEncoding(bytes, file);
      if (encoding != "image") {
        reference["encoding"] = encoding;
        if (!textureName.empty()) reference["texture"] = textureName;
        if (encoding == "wad2") {
          rbTexture::require(gameProfile == 2, "palette", "Select the Quake 1 source game profile to import WAD2.");
          reference["palette"] = rbTexture::writePalette(q1Palette());
        } else if (encoding == "wal") {
          rbTexture::require(gameProfile == 3, "palette", "Select the Quake II source game profile to import a WAL texture.");
          reference["palette"] = rbTexture::writePalette(q2Palette());
        }
      }
      auto raster = rbTexture::decodeOriginal(bytes, reference);
      const auto root = studioRecoveryRoot();
      std::filesystem::create_directories(root);
      studioWriteOriginal(root, relative, bytes);
      reference["width"] = raster->size.width;
      reference["height"] = raster->size.height;
      auto doc = rbTexture::importedImage(ftebroom::bb::uuid(), *raster, std::move(reference));
      studioCancel();
      auto& s = textureStudio;
      s.file.clear();
      s.recoveryFile.clear();
      s.savedSource.clear();
      s.recoveredSource.clear();
      s.assetFolder = root;
      s.assets = {{"original", std::move(raster)}};
      s.drag.reset();
      s.document.emplace(std::move(doc));
      s.variationBase = 0;
      s.channel = "baseColor";
      s.selectedNode = 0;
      s.frame = 0;
      studioSchedule();
      studioAutosave();
    });
  });
}

static void studioSaveAs() {
  runUiChoose(documentPath("material.rbtex.json").u8string(), [](const std::string& name) {
    studioAction([&] { studioSave(std::filesystem::u8path(name)); });
  }, true);
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
        rbTexture::tga(*image.outputs.at("baseColor"))}};
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
  static char textureName[32] = "studio_brick";
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
  const bool outputsReady = ready && (target < F::jpeg || s.result->image.outputs.count("baseColor")) &&
    (target != F::fte || (s.result->image.outputs.count("normal") && s.result->image.outputs.count("emissive")));
  if (!paletteReady) ImGui::TextUnformatted("Select the matching game profile before exporting its indexed texture.");
  if (ready && !outputsReady) ImGui::TextUnformatted("Connect the output channels required by this format.");
  ImGui::BeginDisabled(!outputsReady || !paletteReady);
  const bool exportFile = ImGui::Button("Choose destination…");
  runUiRecordGlobal("texture-studio/export-destination");
  ImGui::SameLine();
  const bool dryRun = ImGui::Button("Dry run");
  runUiRecordGlobal("texture-studio/export-dry-run");
  ImGui::EndDisabled();
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
      }, true);
    });
  }
  if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
  ImGui::EndPopup();
}

static void studioParameter(const rbTexture::Json& binding) {
  auto& s = textureStudio;
  const auto id = binding.at("node").get<std::string>();
  const auto group = binding.value("group", "");
  const auto key = binding.at("key").get<std::string>();
  const auto label = binding.at("label").get<std::string>();
  auto document = textureStudioDoc();
  auto& scope = document.source.at("graph");
  auto& nodes = group.empty() ? scope.at("nodes") : scope.at("groups").at(group).at("nodes");
  auto node = std::find_if(nodes.begin(), nodes.end(), [&](const auto& entry) { return entry["id"] == id; });
  rbTexture::require(node != nodes.end(), "graph", "The parameter's node no longer exists.");
  auto& param = node->at("parameters").at(key);
  bool changed = false;
  ImGui::PushID((group + "/" + id + "/" + key).c_str());
  if (param.is_number_integer()) {
    int v = param.get<int>();
    const int minimum = int(binding.at("min").get<float>());
    const int maximum = int(binding.at("max").get<float>());
    const int step = int(binding.at("step").get<unsigned>());
    changed = ImGui::SliderInt(label.c_str(), &v, minimum, maximum);
    if (changed) {
      v = std::clamp(minimum + std::max(1, step) * ((v - minimum) / std::max(1, step)), minimum, maximum);
      param = v;
    }
  } else {
    float v = param.get<float>();
    changed = ImGui::SliderFloat(label.c_str(), &v, binding.at("min").get<float>(), binding.at("max").get<float>(), "%.3f");
    if (changed) param = v;
  }
  const bool completed = ImGui::IsItemDeactivatedAfterEdit();
  runUiRecordGlobal("texture-studio/parameter/" + id + "/" + key);
  if (changed) {
    s.drag = std::move(document);
    studioSchedule();
  }
  if (completed && s.drag) studioEdit(*s.drag);
  ImGui::PopID();
}

static void studioQuick() {
  auto& s = textureStudio;
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
    int mode = parameters.value("mode", 0);
    const bool modeChanged = ImGui::Combo("Blend mode", &mode, "Normal\0Multiply\0Screen\0Add\0Overlay\0");
    studioParameter(rbTexture::Json{{"node", id}, {"key", "opacity"}, {"label", "Opacity"},
                                   {"min", 0.f}, {"max", 1.f}, {"step", 0}});
    ImGui::PopID();
    if (!visibility && !up && !down && !remove && !modeChanged) continue;
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

static void studioNodes() {
  auto& s = textureStudio;
  auto doc = textureStudioDoc();
  auto& graph = doc.source.at("graph");
  auto& nodes = graph.at("nodes");
  auto& edges = graph.at("edges");
  static float zoom = .70f;
  static ImVec2 pan{12,12};
  if (ImGui::Button("Add node")) ImGui::OpenPopup("Add texture node");
  runUiRecordGlobal("texture-studio/graph-add");
  if (ImGui::BeginPopup("Add texture node")) {
    for (const auto& spec : rbTexture::contracts()) {
      if (spec.kernel == rbTexture::Kernel::image) continue;
      if (!ImGui::MenuItem(spec.type)) continue;
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
  ImGui::BeginDisabled(s.groupSelection.empty());
  if (ImGui::Button("Group nodes…")) ImGui::OpenPopup("Group texture nodes");
  ImGui::EndDisabled();
  if (ImGui::BeginPopupModal("Group texture nodes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name[80] = "Material group";
    ImGui::InputText("Name", name, sizeof(name));
    bool grouped = false;
    ImGui::BeginDisabled(!name[0] || s.groupSelection.empty());
    if (ImGui::Button("Create group")) studioAction([&] {
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
  if (hovered && ImGui::GetIO().MouseWheel) zoom = std::clamp(zoom + ImGui::GetIO().MouseWheel * .07f, .30f, 2.f);
  if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    pan.x += ImGui::GetIO().MouseDelta.x; pan.y += ImGui::GetIO().MouseDelta.y;
  }
  auto* painter = ImGui::GetWindowDrawList();
  const auto color = ImGui::GetColorU32(ImGuiCol_Border);
  const auto accent = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
  const ImVec2 card{200 * zoom, 68 * zoom};
  std::map<std::string, ImVec2> positions;
  struct CanvasPin { ImVec2 center; const char* name; rbTexture::Port type; };
  std::map<std::string, CanvasPin> sendPins;
  std::map<std::pair<std::string, std::string>, CanvasPin> receivePins;
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const auto nearPin = [&](ImVec2 center) {
    const auto dx = center.x - mouse.x, dy = center.y - mouse.y;
    return dx * dx + dy * dy <= 81.f;
  };
  for (size_t index = 0; index < nodes.size(); ++index) {
    const auto& n = nodes[index];
    auto point = n.value("position", rbTexture::Json::array());
    float x = float(index % 4) * 232, y = float(index / 4) * 112;
    if (point.is_array() && point.size() == 2 && point[0].is_number() && point[1].is_number()) {
      x = std::clamp(point[0].get<float>(), -30000.f, 30000.f);
      y = std::clamp(point[1].get<float>(), -30000.f, 30000.f);
    }
    const auto id = n.at("id").get<std::string>();
    const auto position = ImVec2{origin.x + pan.x + x * zoom, origin.y + pan.y + y * zoom};
    positions[id] = position;
    const auto* spec = rbTexture::contract(n.at("type").get<std::string>());
    if (!spec) continue;
    sendPins.emplace(id, CanvasPin{{position.x + card.x, position.y + card.y * .62f}, spec->output.name, spec->output.type});
    for (size_t i = 0; i < spec->inputs.size(); ++i) {
      const auto& pin = spec->inputs[i];
      const float offset = card.y * (.42f + .15f * i);
      receivePins.emplace(std::make_pair(id, pin.name), CanvasPin{{position.x, position.y + offset}, pin.name, pin.type});
    }
  }
  for (const auto& e : edges) {
    auto start = positions.find(e.at("fromNode").get<std::string>());
    auto end = positions.find(e.at("toNode").get<std::string>());
    if (start == positions.end() || end == positions.end()) continue;
    const auto from = sendPins.find(start->first);
    const auto to = receivePins.find({end->first, e.at("toPort").get<std::string>()});
    const ImVec2 a = from == sendPins.end() ? ImVec2{start->second.x + card.x, start->second.y + card.y * .5f} : from->second.center;
    const ImVec2 b = to == receivePins.end() ? ImVec2{end->second.x, end->second.y + card.y * .5f} : to->second.center;
    painter->AddBezierCubic(a, {a.x + card.x * .25f, a.y}, {b.x - card.x * .25f, b.y}, b, accent, 2.f);
  }
  for (size_t index = 0; index < nodes.size(); ++index) {
    const auto& n = nodes[index];
    auto id = n.at("id").get<std::string>();
    auto type = n.at("type").get<std::string>();
    const auto a = positions.at(id);
    const ImVec2 b{a.x + card.x, a.y + card.y};
    const bool selected = int(index) == s.selectedNode || s.groupSelection.count(id);
    painter->AddRectFilled(a, b, ImGui::GetColorU32(ImGuiCol_FrameBg), 4);
    painter->AddRect(a, b, selected ? accent : color, 4, 0, selected ? 2.f : 1.f);
    const auto label = n.value("label", type);
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
  for (const auto& [id, pin] : sendPins) {
    const bool over = hovered && nearPin(pin.center);
    painter->AddCircleFilled(pin.center, over ? 5.f : 3.5f, pinColor(pin.type));
    if (over && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      s.wireStart = id;
      s.movingNode.clear();
    }
  }
  std::optional<rbTexture::Json> newConnection;
  const auto sending = sendPins.find(s.wireStart);
  if (sending != sendPins.end())
    painter->AddLine(sending->second.center, mouse, pinColor(sending->second.type), 2.f);
  for (const auto& [key, pin] : receivePins) {
    const bool over = hovered && nearPin(pin.center);
    painter->AddCircleFilled(pin.center, over ? 5.f : 3.5f, pinColor(pin.type));
    if (!over) continue;
    ImGui::SetTooltip("%s", pin.name);
    if (sending != sendPins.end() && pin.type == sending->second.type && key.first != sending->first &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left))
      newConnection = rbTexture::wire(sending->first.c_str(), sending->second.name, key.first.c_str(), pin.name);
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) s.wireStart.clear();
  if (!s.movingNode.empty() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    const auto position = positions.at(s.movingNode);
    const auto delta = ImGui::GetIO().MouseDelta;
    rbTexture::findNode(doc, s.movingNode)["position"] = {
      (position.x + delta.x - origin.x - pan.x) / zoom, (position.y + delta.y - origin.y - pan.y) / zoom};
    s.drag = doc;
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
  const auto selected = nodes[size_t(s.selectedNode)].at("id").get<std::string>();
  if (ImGui::BeginCombo("Node", selected.c_str())) {
    for (size_t i = 0; i < nodes.size(); ++i) {
      const auto id = nodes[i].at("id").get<std::string>();
      if (ImGui::Selectable(id.c_str(), int(i) == s.selectedNode)) {
        s.selectedNode = int(i);
        const auto position = positions.at(id);
        pan = {pan.x + origin.x + (area.x - card.x) * .5f - position.x,
               pan.y + origin.y + (area.y - card.y) * .5f - position.y};
      }
    }
    ImGui::EndCombo();
  }
  const auto& n = nodes[size_t(s.selectedNode)];
  const auto id = n.at("id").get<std::string>();
  const auto type = n.at("type").get<std::string>();
  if (type != "Subgraph") {
    bool included = s.groupSelection.count(id) != 0;
    if (ImGui::Checkbox("Include selected node in group", &included)) {
      if (included) s.groupSelection.insert(id);
      else s.groupSelection.erase(id);
    }
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
      ImGui::InputTextMultiline("##definition", buffer.data(), buffer.size(), {620 * uiScale(), 350 * uiScale()});
      if (ImGui::Button("Apply")) studioAction([&] {
        definition = rbTexture::parseJson(buffer.data());
        studioEdit(std::move(doc));
        ImGui::CloseCurrentPopup();
      });
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
      if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
      ImGui::EndPopup();
    }
    return;
  }
  const auto* dst = rbTexture::contract(type);
  if (!dst) return;
  if (ImGui::BeginCombo("Bind output", dst->output.name)) {
    for (const auto& [channel, port] : rbTexture::channelTypes()) {
      if (dst->output.type != port || !ImGui::Selectable(channel.c_str())) continue;
      doc.source["outputs"][channel] = {{"node", id}, {"port", dst->output.name}};
      ImGui::EndCombo();
      studioEdit(std::move(doc));
      return;
    }
    ImGui::EndCombo();
  }
  if (ImGui::Button("Delete node")) {
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
  ImGui::SameLine();
  if (ImGui::Button("Parameters…")) {
    const auto input = n.at("parameters").dump(2);
    rbTexture::require(input.size() < parameters.size(), "graph", "These node parameters exceed the editing budget.");
    std::copy(input.c_str(), input.c_str() + input.size() + 1, parameters.begin());
    ImGui::OpenPopup("Edit node parameters");
  }
  if (ImGui::BeginPopupModal("Edit node parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(dst->type);
    ImGui::InputTextMultiline("##node-parameters", parameters.data(), parameters.size(), {440 * uiScale(), 180 * uiScale()});
    const bool attempted = ImGui::Button("Apply");
    if (attempted) {
      studioAction([&] {
        nodes[size_t(s.selectedNode)]["parameters"] = rbTexture::parseJson(parameters.data());
        studioEdit(std::move(doc));
        ImGui::CloseCurrentPopup();
      });
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
    ImGui::EndPopup();
    if (attempted) return;
  }
  for (const auto pin : dst->inputs) {
    auto edge = std::find_if(edges.begin(), edges.end(), [&](const auto& e) {
      return e.at("toNode") == id && e.at("toPort") == pin.name;
    });
    const auto from = edge == edges.end() ? std::string{} : edge->at("fromNode").template get<std::string>();
    bool change = false;
    if (ImGui::BeginCombo(pin.name, from.c_str())) {
      for (const auto& other : nodes) {
        const auto candidate = other.at("id").get<std::string>();
        const auto* src = rbTexture::contract(other.at("type").get<std::string>());
        if (!src || src->output.type != pin.type || candidate == id) continue;
        if (ImGui::Selectable(candidate.c_str(), candidate == from) && candidate != from) {
          auto connection = rbTexture::wire(candidate.c_str(), src->output.name, id.c_str(), pin.name);
          if (edge == edges.end()) edges.push_back(std::move(connection));
          else *edge = std::move(connection);
          change = true;
        }
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
  ImGui::BeginDisabled(s.apply != TextureStudioState::Apply::none);
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
  if (!s.handle || !s.result) return;
  const bool mesh = s.previewMode == Preview::sphere || s.previewMode == Preview::cube || s.previewMode == Preview::surface;
  const auto size = mesh ? rbTexture::Resolution{256, 256} : s.result->image.size;
  const float scale = std::min(std::max(1.f, ImGui::GetContentRegionAvail().x) / size.width,
    std::max(1.f, ImGui::GetContentRegionAvail().y - 32 * uiScale()) / size.height);
  const ImVec2 rect{size.width * scale, size.height * scale};
  if (s.previewMode == Preview::tiled) {
    auto* painter = ImGui::GetWindowDrawList();
    const auto at = ImGui::GetCursorScreenPos();
    const ImVec2 tile{rect.x / 3, rect.y / 3};
    for (int y = 0; y < 3; ++y)
      for (int x = 0; x < 3; ++x)
        painter->AddImage((ImTextureID)s.handle, {at.x + tile.x * x, at.y + tile.y * y},
          {at.x + tile.x * (x + 1), at.y + tile.y * (y + 1)});
    ImGui::Dummy(rect);
  } else ImGui::Image((ImTextureID)s.handle, rect);
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

static void textureStudioPanel() {
  auto& s = textureStudio;
  studioTick();
  if (!s.open) return;
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
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) s.wireStart.clear();
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
  const bool contentVisible = ImGui::Begin("Texture Studio", &s.open);
  s.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  if (contentVisible) {
    const auto nextButton = [](const char* label) {
      const auto& style = ImGui::GetStyle();
      const float nextRight = ImGui::GetItemRectMax().x + style.ItemSpacing.x +
        ImGui::CalcTextSize(label).x + 2 * style.FramePadding.x;
      if (nextRight <= ImGui::GetCurrentWindowRead()->WorkRect.Max.x) ImGui::SameLine();
    };
    if (runUiLive) {
      runUiGlobalRects["texture-studio/render-state"] = {
        {"revision", s.revision}, {"previewReady", s.handle != 0 && s.imageRevision == s.revision},
        {"theme", uiEffectiveTheme()}, {"scale", uiScale()},
        {"previewMode", int(s.previewMode)}, {"hasSurface", !s.surfaceData.empty()},
        {"fullSize", s.result && s.result->fullSize && s.result->revision == s.revision},
        {"busy", s.job.valid() || s.apply != TextureStudioState::Apply::none}, {"error", s.error}
      };
      const auto* window = ImGui::GetCurrentWindowRead();
      if (!window->DockIsActive) {
        const auto title = window->TitleBarRect();
        const auto padding = ImGui::GetStyle().FramePadding;
        const float button = ImGui::GetFontSize();
        runUiGlobalRects["texture-studio/close"] = {
          {"x", title.Max.x - padding.x - button}, {"y", title.Min.y + padding.y},
          {"width", button}, {"height", button}
        };
      }
    }
    const bool applying = s.apply != TextureStudioState::Apply::none;
    const bool writable = !textureStudioDoc().readOnly;
    const auto& io = ImGui::GetIO();
    const bool shortcuts = s.focused && !io.WantTextInput &&
      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    if (shortcuts && s.apply != TextureStudioState::Apply::commit && ImGui::IsKeyPressed(ImGuiKey_Escape) &&
        (applying || s.job.valid() || s.scheduled || s.drag || !s.wireStart.empty())) {
      studioCancel();
      s.drag.reset();
    }
    ImGui::BeginDisabled(applying);
    if (ImGui::Button("New")) {
      s.presetSearch.fill(0);
      s.catalogVariation = 0;
      studioAction(studioLoadLibrary);
      ImGui::OpenPopup("New source?");
    }
    runUiRecordGlobal("texture-studio/new");
    nextButton("Open…");
    if (ImGui::Button("Open…")) ImGui::OpenPopup("Open source?");
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
    if (ImGui::BeginPopupModal("Recover source", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      if (s.recoverable.empty()) ImGui::TextUnformatted("There are no recovery copies.");
      else ImGui::TextUnformatted("Save your current source before opening a recovery copy.");
      for (const auto& path : s.recoverable) {
        if (!ImGui::Selectable(path.stem().stem().u8string().c_str())) continue;
        studioAction([&] {
          auto doc = rbTexture::open(ftebroom::bb::readFile(path, rbTexture::maxSourceBytes));
          rbTexture::require(!doc.readOnly, "recovery", "Open a newer recovery copy using Open.");
          auto assets = rbTexture::loadAssets(doc, path.parent_path());
          rbTexture::compile(doc, {}, assets);
          doc.source["documentId"] = ftebroom::bb::uuid();
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
      ImGui::TextUnformatted("Choose a preset. Save your current source before replacing it.");
      ImGui::SetNextItemWidth(390);
      ImGui::InputTextWithHint("##preset-search", "Search presets", s.presetSearch.data(), s.presetSearch.size());
      runUiRecordGlobal("texture-studio/preset-search");
      ImGui::SetNextItemWidth(180);
      ImGui::Combo("Show", &s.libraryFilter, "All presets\0Favorites\0Recently used\0");
      ImGui::SetNextItemWidth(180);
      const auto choice = s.catalogVariation > 0 ? rbTexture::curatedVariations[s.catalogVariation - 1].name : "Default";
      if (ImGui::BeginCombo("Catalog variation", choice)) {
        if (ImGui::Selectable("Default", s.catalogVariation == 0)) s.catalogVariation = 0;
        for (size_t i = 0; i < std::size(rbTexture::curatedVariations); ++i)
          if (ImGui::Selectable(rbTexture::curatedVariations[i].name, s.catalogVariation == int(i + 1)))
            s.catalogVariation = int(i + 1);
        ImGui::EndCombo();
      }
      const auto lowercase = [](std::string value) {
        for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return value;
      };
      const auto query = lowercase(s.presetSearch.data());
      std::optional<rbTexture::Document> selected;
      rbTexture::Assets selectedAssets;
      std::filesystem::path selectedAssetRoot;
      std::string selectedId;
      if (ImGui::BeginChild("preset-library", ImVec2(390, 335), true)) {
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
        if (!query.empty() || s.libraryFilter || ImGui::CollapsingHeader("Featured", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (unsigned preset = 0; preset < rbTexture::presetNames.size(); ++preset) {
            const auto id = "featured-" + std::to_string(preset);
            if (!permitted(id) || lowercase(rbTexture::presetNames[preset]).find(query) == std::string::npos) continue;
            matched = true;
            if (ImGui::Selectable(rbTexture::presetNames[preset], false, 0, {320, 0})) {
              selected = rbTexture::builtinPreset(preset, ftebroom::bb::uuid());
              selectedId = id;
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
              if (ImGui::Selectable(name, false, 0, {320, 0})) {
                selected = rbTexture::catalogPreset(category, index, ftebroom::bb::uuid());
                selectedId = id;
                if (s.catalogVariation > 0)
                  rbTexture::applyCuratedVariation(*selected, rbTexture::curatedVariations[s.catalogVariation - 1]);
              }
              runUiRecordGlobal("texture-studio/recipe/" + id);
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
              const auto folder = documentPath("data/texture-studio/user-presets");
              auto doc = rbTexture::open(ftebroom::bb::readFile(folder / (id + ".rbtex.json"), rbTexture::maxSourceBytes));
              auto assets = rbTexture::loadAssets(doc, folder);
              rbTexture::compile(doc, {}, assets);
              doc.source["documentId"] = ftebroom::bb::uuid();
              selected = std::move(doc);
              selectedAssets = std::move(assets);
              selectedAssetRoot = folder;
              selectedId = id;
            });
            favorite(id);
            ImGui::PopID();
          }
        }
        if (!matched) ImGui::TextDisabled("No presets match this view.");
      }
      ImGui::EndChild();
      if (selected) {
        studioNewDocument(std::move(*selected));
        s.assets = std::move(selectedAssets);
        s.assetFolder = std::move(selectedAssetRoot);
        studioAction([&] { studioRecordPreset(selectedId); });
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("Open source?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Save your current source before replacing it.");
      if (ImGui::Button("Choose file…")) { ImGui::CloseCurrentPopup(); studioOpenFile(); }
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
      if (ImGui::BeginPopupModal("Save user preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char label[81]{};
        ImGui::InputText("Name", label, sizeof(label));
        ImGui::BeginDisabled(!label[0]);
        if (ImGui::Button("Save preset")) studioAction([&] {
          studioSaveUserPreset(label);
          ImGui::CloseCurrentPopup();
        });
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

    if (writable) ImGui::TextUnformatted(textureStudioDoc().source.at("name").get_ref<const std::string&>().c_str());
    ImGui::Text("Profile %s", gameProfileName());
    ImGui::BeginDisabled(!writable || applying);
    if (ImGui::Button("Generate")) studioAction([&] { s.playing = false; studioAutosave(); studioSchedule(true); });
    runUiRecordGlobal("texture-studio/generate");
    nextButton("Export…");
    ImGui::BeginDisabled(!s.result || s.result->revision != s.revision || !s.result->fullSize);
    if (ImGui::Button("Export…")) { s.playing = false; ImGui::OpenPopup("Export texture"); }
    runUiRecordGlobal("texture-studio/export");
    ImGui::EndDisabled();
    studioExportDialog();
    nextButton("Generate & Apply");
    const auto faces = cvars->GetFloat("ftebroom_face_count");
    ImGui::BeginDisabled(!studioProfileAllowsApply() || faces <= 0);
    if (ImGui::Button("Generate & Apply")) studioAction(studioBeginApply);
    runUiRecordGlobal("texture-studio/apply");
    ImGui::EndDisabled();
    if (bbActive) {
      nextButton("Apply to model texture");
      ImGui::BeginDisabled(!studioModelSlotAvailable());
      if (ImGui::Button("Apply to model texture")) studioAction(studioBeginModelApply);
      runUiRecordGlobal("texture-studio/apply-model");
      ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
    if (s.job.valid() || applying) {
      nextButton("Cancel");
      ImGui::BeginDisabled(s.apply == TextureStudioState::Apply::commit);
      if (ImGui::Button("Cancel")) studioCancel();
      runUiRecordGlobal("texture-studio/cancel");
      ImGui::EndDisabled();
      ImGui::TextUnformatted("Generating…");
    }
    if (!s.error.empty()) ImGui::TextWrapped("%s", s.error.c_str());
    else if (!s.message.empty()) ImGui::TextWrapped("%s", s.message.c_str());
    if (!writable) ImGui::TextWrapped("This source uses a newer format. Save As preserves its original bytes.");
    const bool stacked = ImGui::GetContentRegionAvail().x < 660 * uiScale();
    if (writable && stacked) {
      const float height = std::clamp(ImGui::GetContentRegionAvail().y * .45f, 130.f, std::max(130.f, 310 * uiScale()));
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
  }
  ImGui::End();
  if (!s.open) {
    studioCancel();
    studioReleaseImage();
    s.result.reset();
    s.previewCache = std::make_shared<rbTexture::NodeCache>(32 * 1024 * 1024);
  }
}

static void textureStudioShutdown() {
  studioCancel();
  if (textureStudio.job.valid()) textureStudio.job.wait();
  studioReleaseImage();
}
