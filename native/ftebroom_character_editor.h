// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "character/materials.h"
#include "character/presets.h"
#include "character/preview.h"
#include "character/iqm.h"
#include "character/studio.h"
#include "character/animation_import.h"

namespace {
namespace cc = ftebroom::character;
static void characterPublishPlacement();
static void characterFinishPlacement();
static void characterExportProfile(const std::filesystem::path& path,int profile);
static const char* characterExtension(int profile);

static cc::Document characterNewDocument() {
  cc::Document doc;
  doc.faceProfile = .55;
  doc.bodyProfile = .76;
  doc.faceTemplate = 1;
  doc.eyebrow = 2;
  doc.eyes = {.27,.22,.17};
  return doc;
}

struct CharacterEditorState {
  std::optional<cc::Humanoid> rig;
  cc::Document doc = characterNewDocument();
  std::filesystem::path file;
  std::filesystem::path readonly;
  std::string saved, error, status;
  std::function<void()> pendingDocumentAction, readyDocumentAction;
  std::string unsavedError;
  bool waitingForSavePath = false;
  AssetPlacement placement;
  std::vector<cc::Document> undo, redo;
  std::optional<cc::Document> gesture;
  std::optional<std::array<double,cc::morphCount>> sculptStart;
  struct MorphResult {
    std::uint64_t revision;
    cc::Humanoid fitted;
    std::optional<cc::Humanoid> reduced;
  };
  std::future<MorphResult> morphJob;
  struct RuntimeAsset {
    std::map<std::string,std::string> files;
    std::string binding, model, source;
    cc::Vec3 low, high;
  };
  std::future<RuntimeAsset> placementJob;
  cc::Humanoid fitted;
  std::optional<cc::Humanoid> reduced;
  // Jobs copy the lookup table and share immutable meshes with the editor.
  std::map<std::string,std::shared_ptr<const cc::Wardrobe>> wardrobe;
  std::map<std::string,std::string> importedFiles;
  std::map<std::string,std::shared_ptr<const cc::ImportedMotion>> importedMotions;
  std::size_t importedBytes=0;
  std::array<std::array<unsigned,2>,std::size(cc::faceRegions)> sculptAnchors{};
  cc::Json materialManifest;
  std::string materialDigest;
  std::set<std::string> materialUploads;
  std::vector<cc::Vec3> rest;
  std::vector<cc::Vec3> reducedRest;
  std::uint64_t revision = 1, submitted = 0, applied = 0;
  bool open = false, owner = false, uploaded = false, playing = false;
  bool wire = false, ortho = false, grid = true;
  cc::PreviewShading shading = cc::PreviewShading::Materials;
  bool skinShaderActive = false;
  bool pbrPreview = false;
  bool fitBodyRig = false;
  enum class Framing { Manual, Body, Portrait, UpperBody } framing = Framing::Body;
  enum class Tab { Body, Face, Appearance, Wardrobe, Rig, Export } tab = Tab::Body;
  int faceRegion = -1, sculptSide = 1, faceTypes = 0;
  bool sculptDepth = false;
  bool suppressFaceOrbit = false;
  bool bones = false, showSockets = false, blend = false, blendUpperBody = true;
  int detailLevel = 0;
  enum class Playback { ClipDefault, Loop, Once } playback = Playback::ClipDefault;
  bool separatePanes = false;
  std::size_t bone = 0;
  cc::Vec3 pan{};
  double time = 0, nextUpload = 0, nextAutosave = 0;
  float yaw = 12, pitch = 6, zoom = .79f;
  float speed = 1, blendWeight = .5f;
  std::size_t blendClip = 0;
  unsigned hairFilter = 0;
  std::vector<bool> upperBody;
};

static CharacterEditorState characterEditor;

static constexpr std::size_t characterMotionLimit=32*1024*1024, characterWardrobeLimit=4*1024*1024;

static std::shared_ptr<const cc::ImportedMotion> characterImportedMotion(const cc::Document& doc) {
  if (doc.animationAsset.empty()) return {};
  return characterEditor.importedMotions.at(doc.animationAsset);
}

static void characterRegisterImport(const std::string& id,std::string data,bool animation) {
  auto& s=characterEditor;
  if (ftebroom::jam::sha256(data)!=id) throw std::runtime_error("Imported character asset checksum mismatch.");
  if (s.importedFiles.count(id)) {
    if (animation!=bool(s.importedMotions.count(id)))
      throw std::runtime_error("Imported asset does not match its document slot.");
    return;
  }
  if (s.importedFiles.size()>=16 || data.size()>64*1024*1024-s.importedBytes)
    throw std::runtime_error("This Character Creator session has reached its import cache limit.");
  if (animation) {
    auto motion=cc::MotionGlb(data).retarget(*s.rig);
    s.importedMotions.emplace(id,std::make_shared<const cc::ImportedMotion>(std::move(motion)));
  } else {
    if (s.placementJob.valid()) throw std::runtime_error("Wait for the current map placement to finish.");
    auto clothing=cc::readWardrobe(data,*s.rig);
    for (const auto& entry : clothing.vertices)
      if (entry.vertex.position[1]<0 || entry.vertex.position[1]>15.2)
        throw std::runtime_error("Import a garment fitted to the torso or legs of the canonical body.");
    s.wardrobe.emplace(id,std::make_shared<const cc::Wardrobe>(std::move(clothing)));
  }
  s.importedBytes+=data.size();
  s.importedFiles.emplace(id,std::move(data));
}

static std::filesystem::path characterImportPath(const std::filesystem::path& document,
                                                 const std::string& id,bool animation) {
  const auto parent=std::filesystem::weakly_canonical(std::filesystem::absolute(document).parent_path());
  const auto folder=parent/"rbchar-assets";
  const auto path=folder/(id+(animation ? ".glb" : ".rbw"));
  if (std::filesystem::weakly_canonical(path)!=path)
    throw std::runtime_error("Character dependencies must stay inside the document asset folder.");
  return path;
}

static bool characterAvailable() {
  return !jamEditing() && !jamModeActive();
}

static void characterRequireOffline() {
  if (!characterAvailable()) throw std::runtime_error("Character Creator is available in offline projects.");
}

static std::string characterAsset(const std::string& name, std::size_t maximum) {
  std::size_t length = 0;
  const auto raw = static_cast<const char*>(files->LoadFile(name.c_str(),&length));
  if (!raw) throw std::runtime_error("The bundled character asset is missing: " + name);
  std::unique_ptr<const char, void(*)(const char*)> data(raw, [](const char* ptr) {
    core->Free(const_cast<char*>(ptr));
  });
  if (length > maximum) throw std::runtime_error("Character asset exceeds its size limit: " + name);
  return std::string(data.get(),length);
}

static std::string characterMaterialBytes(const std::string& id, const cc::Json& manifest) {
  const auto& material = manifest.at("textures").at(id);
  const auto file = material.at("file").get<std::string>();
  if (file != id+".jpg" && file != id+".png")
    throw std::runtime_error("Invalid filename in the bundled character material registry.");
  auto bytes = characterAsset("characters/materials/"+file,20*1024*1024);
  if (ftebroom::jam::sha256(bytes) != material.at("sha256"))
    throw std::runtime_error("A character texture failed verification: "+id);
  return bytes;
}

static cc::MaterialImages characterExportImages(const cc::Document& doc,const cc::Json& manifest,bool pbr=false) {
  cc::MaterialImages images;
  const auto maps = cc::surfaceMaps(doc,pbr);
  for (unsigned i = 0; i < maps.size(); ++i)
    if (!maps[i].empty()) images[i] = characterMaterialBytes(maps[i],manifest);
  return images;
}

static std::string characterSurfaceDetail(const cc::Json& manifest, bool fabric = false) {
  auto bytes = characterAsset(fabric ? cc::fabricDetailPath : cc::surfaceDetailPath,20*1024*1024);
  if (ftebroom::jam::sha256(bytes)!=manifest.at(fabric ? "pbrFabric" : "pbrDetail").at("sha256"))
    throw std::runtime_error("A character PBR detail texture failed verification.");
  return bytes;
}

static auto characterPreviewMaterials(CharacterEditorState& s,
    const std::array<std::string,cc::materialCount>& maps) {
  std::array<std::string,cc::materialCount> names;
  auto api = bbImageAPI();
  if (!api) throw std::runtime_error("The engine does not provide character texture uploads.");
  for (unsigned i = 0; i < maps.size(); ++i) {
    const auto& id = maps[i];
    if (id.empty()) continue;
    const bool fine = i==cc::browMaterial || i==cc::lashMaterial;
    const char* prefix = fine ? "bbtex/rbchar-fine-" : i==5 ? "bbtex/rbchar-hair-" :
        i==0 && s.skinShaderActive ? "bbtex/rbchar-skin-" : "bbtex/rbchar-";
    if (s.pbrPreview && cc::surfaceVariant(i)) {
      prefix = i==0 ? "bbtex/rbchar-pbr-skin-" : i==1 ? "bbtex/rbchar-pbr-eye-" :
               i==5 ? "bbtex/rbchar-pbr-hair-" : "bbtex/rbchar-pbr-cloth-";
    }
    names[i] = std::string(prefix)+id+".tga";
  }
  std::size_t missing = 0;
  for (const auto& name : names) if (!name.empty() && !s.materialUploads.count(name)) ++missing;
  if (s.pbrPreview && s.materialUploads.size()+missing>12) {
    if (auto mesh = bbAPI()) mesh->clear();
    api->clear();
    s.materialUploads.clear();
  }
  for (unsigned i=0; i<names.size(); ++i) {
    const auto& name = names[i];
    if (name.empty() || s.materialUploads.count(name)) continue;
    const auto image = bb::decodeImage(characterMaterialBytes(maps[i],s.materialManifest));
    if (!api->upload(name.c_str(),image.rgba.data(),image.width,image.height))
      throw std::runtime_error("The engine rejected a character texture: "+maps[i]);
    s.materialUploads.insert(name);
  }
  return names;
}

static void characterReleasePreview() {
  auto& s = characterEditor;
  if (!s.owner) return;
  s.owner = s.uploaded = false;
  s.materialUploads = {};
  if (bbActive || modelViewerOpen || plantState.owner) return;
  cvars->SetFloat("ftebroom_bb_active",0);
  cvars->SetFloat("ftebroom_modelviewer_open",0);
  cvars->SetFloat("ftebroom_modeleditor_active",0);
  if (auto api = bbAPI()) api->clear();
  if (auto api = bbImageAPI()) api->clear();
}

static void characterOpen() {
  characterRequireOffline();
  auto& s = characterEditor;
  if (!s.rig) {
    const auto materials = characterAsset("characters/materials/provenance.json",128*1024);
    s.materialManifest = cc::Json::parse(materials);
    s.materialDigest = ftebroom::jam::sha256(materials);
    const auto bytes = characterAsset("characters/humanoid-v1.rbh",2*1024*1024);
    if (ftebroom::jam::sha256(bytes) != "37b57d5a27cfa6707466aaf8875073d59f276e41c08f5e20ce1012d7ccd3c2f3")
      throw std::runtime_error("The bundled humanoid asset failed verification.");
    auto rig = cc::readHumanoid(bytes);
    const auto shapesManifest = cc::Json::parse(characterAsset("characters/face-shapes-provenance.json",8*1024));
    auto shapes = characterAsset("characters/adult-face-shapes.rbf",512*1024);
    if (ftebroom::jam::sha256(shapes)!=shapesManifest.at("files").at("adult-face-shapes.rbf"))
      throw std::runtime_error("The bundled face shape asset failed verification.");
    cc::readFaceShapes(rig,shapes);
    const auto bodyManifest = cc::Json::parse(characterAsset("characters/body-shapes-provenance.json",8*1024));
    const auto body = characterAsset("characters/adult-body-shapes.rbs",2*1024*1024);
    if (ftebroom::jam::sha256(body)!=bodyManifest.at("files").at("adult-body-shapes.rbs"))
      throw std::runtime_error("The bundled body shape asset failed verification.");
    cc::readBodyShapes(rig,body);
    std::vector<bool> skinVertices(rig.vertices.size());
    for (const auto& face : rig.triangles)
      if (face.material==0)
        for (auto id : face.indices) skinVertices[id] = true;
    for (unsigned i=0; i<std::size(cc::faceRegions); ++i) {
      for (unsigned side=0; side<2; ++side) {
        auto point = cc::faceRegions[i].point;
        if (side) point[0] = -point[0];
        double best = 1e9;
        for (unsigned v=0; v<rig.vertices.size(); ++v) {
          if (!skinVertices[v]) continue;
          const auto delta = cc::sub(point,rig.vertices[v].position);
          const auto distance = cc::dot(delta,delta);
          if (distance>=best) continue;
          best = distance;
          s.sculptAnchors[i][side] = v;
        }
      }
    }
    const auto manifest = cc::Json::parse(characterAsset("characters/wardrobe/provenance.json",64*1024));
    auto install = [&](const char* id) {
      if (!*id || s.wardrobe.count(id)) return;
      const auto& assets = manifest.at("assets");
      const auto it = std::find_if(assets.begin(),assets.end(),[&](const auto& asset) {
        return asset.at("id") == id;
      });
      if (it == assets.end()) throw std::runtime_error("Missing wardrobe manifest entry.");
      const auto item = characterAsset("characters/wardrobe/" + std::string(id) + ".rbw",4*1024*1024);
      if (ftebroom::jam::sha256(item) != it->at("sha256"))
        throw std::runtime_error("A wardrobe asset failed verification.");
      s.wardrobe.emplace(id,std::make_shared<const cc::Wardrobe>(cc::readWardrobe(item,rig,id)));
    };
    for (const auto& outfit : cc::outfits) {
      install(outfit.clothes);
      install(outfit.shoes);
      install(outfit.hat);
    }
    for (const auto& hair : cc::hairstyles) install(hair.mesh);
    for (const auto brow : cc::eyebrowMeshes) install(brow);
    install("eyelashes01");
    s.upperBody = cc::upperBodyMask(rig);
    s.rig = std::move(rig);
    s.saved = cc::writeDocument(s.doc,*s.rig).dump();
    constexpr unsigned archiveFlag = 1;
    cvars->GetNVFDG("ftebroom_character_dock_panes","0",archiveFlag,
                    "Use separate dockable Character Creator panels","ReyondBroom");
    s.separatePanes = cvars->GetFloat("ftebroom_character_dock_panes") != 0;
  }
  s.open = true;
  s.error.clear();
  ImGui::SetWindowFocus("Character Creator");
  input->SetMenuFocus(qtrue,"",0,0,1);
}

static void characterTickMorph() {
  auto& s = characterEditor;
  if (s.fitBodyRig != (gameProfile==1)) {
    s.fitBodyRig = gameProfile==1;
    ++s.revision;
  }
  if (s.morphJob.valid() && s.morphJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    auto result = s.morphJob.get();
    if (result.revision == s.revision || s.rest.empty() || s.sculptStart) {
      s.applied = result.revision;
      s.fitted = std::move(result.fitted);
      s.reduced = std::move(result.reduced);
      s.rest.clear();
      s.rest.reserve(s.fitted.vertices.size());
      for (const auto& vertex : s.fitted.vertices) s.rest.push_back(vertex.position);
      s.reducedRest.clear();
      if (s.reduced) {
        s.reducedRest.reserve(s.reduced->vertices.size());
        for (const auto& vertex : s.reduced->vertices) s.reducedRest.push_back(vertex.position);
      }
      s.uploaded = false;
    }
  }
  if (s.morphJob.valid() || s.submitted == s.revision) return;
  const auto* rig = &*s.rig;
  s.morphJob = std::async(std::launch::async, [rig,clothes=s.wardrobe,doc=s.doc,revision=s.revision,fit=s.fitBodyRig,
                                             detail=s.detailLevel] {
    CharacterEditorState::MorphResult result{revision, cc::assemble(*rig,doc,[&](const auto& id) -> const auto& {
      return *clothes.at(id);
    },fit),{}};
    if (detail) result.reduced=cc::previewLod(result.fitted,detail==1 ? 15000 : 9000);
    return result;
  });
  s.submitted = s.revision;
}

static bool characterDirty() {
  const auto& s = characterEditor;
  return cc::writeDocument(s.doc,*s.rig).dump() != s.saved;
}

static void characterReset(cc::Document doc, std::filesystem::path path = {}) {
  auto& s = characterEditor;
  s.doc = std::move(doc);
  s.file = std::move(path);
  s.readonly.clear();
  s.saved = cc::writeDocument(s.doc,*s.rig).dump();
  s.undo.clear();
  s.redo.clear();
  s.placement = {};
  s.status.clear();
  s.gesture.reset();
  s.sculptStart.reset();
  s.suppressFaceOrbit = false;
  s.playing = false;
  s.blend = false;
  s.time = 0;
  ++s.revision;
}

static void characterSaveDependencies(const std::filesystem::path& path,const CharacterEditorState& s) {
  for (const bool animation : {true,false}) {
    const auto& id=animation ? s.doc.animationAsset : s.doc.customWardrobe;
    if (id.empty()) continue;
    const auto dependency=characterImportPath(path,id,animation);
    const auto& bytes=s.importedFiles.at(id);
    if (!std::filesystem::exists(dependency)) {
      std::filesystem::create_directories(dependency.parent_path());
      bb::atomicWrite(dependency,bytes);
    }
    else if (ftebroom::jam::fileBytes(dependency,animation ? characterMotionLimit : characterWardrobeLimit)!=bytes)
      throw std::runtime_error("Conflicting imported asset beside the character document.");
  }
}

static void characterSave(const std::filesystem::path& path) {
  characterRequireOffline();
  auto& s = characterEditor;
  if (s.placement.active && (s.placement.awaiting || s.placementJob.valid()))
    throw std::runtime_error("Wait for the current character placement to finish before saving again.");
  if (path.extension() != ".rbchar")
    throw std::runtime_error("Choose a filename with the .rbchar extension.");
  if (!s.readonly.empty()) throw std::runtime_error("The future document format is read-only.");
  if (path != s.file && std::filesystem::exists(path))
    throw std::runtime_error("Choose a new filename or open the existing character first.");
  auto json = cc::writeDocument(s.doc,*s.rig);
  cc::readDocument(json.dump(),*s.rig);
  characterSaveDependencies(path,s);
  if (path == s.file && std::filesystem::is_regular_file(path)) {
    auto backup = path;
    backup += ".bak";
    bb::atomicWrite(backup,ftebroom::jam::fileBytes(path,65536));
  }
  bb::atomicWrite(path,json.dump(2) + "\n");
  s.saved = json.dump();
  s.file = path;
  if (s.placement.active) characterPublishPlacement();
  s.error.clear();
}

static void characterChooseSave(std::function<void()> saved = {}) {
  const auto& s = characterEditor;
  runUiChoose((s.file.empty() ? documentDefaultFolder() / "human.rbchar" : s.file).u8string(),
              [saved=std::move(saved)](const std::string& path) {
                characterSave(std::filesystem::u8path(path));
                if (saved) saved();
              },true);
}

static void characterRequestDocumentAction(std::function<void()> action) {
  auto& s = characterEditor;
  if (s.pendingDocumentAction || s.readyDocumentAction || s.waitingForSavePath) return;
  if (!characterDirty()) {
    if (s.placement.awaiting || s.placementJob.valid()) s.readyDocumentAction = std::move(action);
    else action();
    return;
  }
  s.unsavedError.clear();
  s.pendingDocumentAction = std::move(action);
}

static void characterReadFile(const std::filesystem::path& path) {
  characterRequireOffline();
  if (path.extension() != ".rbchar")
    throw std::runtime_error("Choose an editable .rbchar file.");
  const auto bytes = ftebroom::jam::fileBytes(path,65536);
  const auto header = cc::parseDocument(bytes);
  if (header.is_object() && header.value("format","") == "rbchar" &&
      header.contains("schemaVersion") && header["schemaVersion"].is_number_unsigned() &&
      header["schemaVersion"] > 1) {
    characterReset({});
    characterEditor.readonly = path;
    characterEditor.status = "This document uses a newer schema. Open it in a newer editor to edit its contents.";
    return;
  }
  const auto doc=cc::decodeDocument(header,*characterEditor.rig);
  for (const bool animation : {true,false}) {
    const auto& id=animation ? doc.animationAsset : doc.customWardrobe;
    if (id.empty()) continue;
    auto dependency=characterImportPath(path,id,animation);
    auto data=ftebroom::jam::fileBytes(dependency,animation ? characterMotionLimit : characterWardrobeLimit);
    characterRegisterImport(id,std::move(data),animation);
  }
  characterReset(doc,placementSnapshotPath(path) ? std::filesystem::path{} : path);
}

static void characterOpenFile(const std::filesystem::path& path) {
  characterOpen();
  characterRequestDocumentAction([path] { characterReadFile(path); });
}

static void characterClose() {
  auto& s = characterEditor;
  if (characterDirty()) characterReset(cc::readDocument(s.saved,*s.rig),s.file);
  s.open = false;
  characterReleasePreview();
}

static bool characterBeforeDocument(const std::string& next) {
  auto& s = characterEditor;
  if (!s.open || !s.rig || !characterAvailable()) return false;
  if (!characterDirty() && !s.placement.awaiting && !s.placementJob.valid()) return false;
  ImGui::SetWindowFocus("Character Creator");
  characterRequestDocumentAction([next] {
    characterClose();
    documentRequest(next);
  });
  return true;
}

static void characterAddToMap() {
  characterRequireOffline();
  auto& s = characterEditor;
  if (!s.placement.active || s.placement.awaiting) return;
  if (s.file.empty() || characterDirty()) characterChooseSave();
  else characterPublishPlacement();
}

static void characterCreateHere() {
  try {
    characterOpen();
    auto captured = contextPlacement;
    characterRequestDocumentAction([captured]() mutable {
      placementValid(captured);
      characterReset(characterNewDocument());
      characterEditor.placement = captured;
    });
  } catch (const std::exception& error) {
    characterEditor.error = error.what();
  }
}

static void characterCheckpoint(const cc::Document& doc) {
  auto& s = characterEditor;
  if (s.undo.size() == 128) s.undo.erase(s.undo.begin());
  s.undo.push_back(doc);
  s.redo.clear();
}

static void characterChooseImport(bool animation) {
  runUiChoose((documentDefaultFolder()/(animation ? "motion.glb" : "garment.rbw")).u8string(),
      [animation](const std::string& selected) {
        characterRequireOffline();
        const auto path=std::filesystem::u8path(selected);
        if (path.extension()!=(animation ? ".glb" : ".rbw"))
          throw std::runtime_error(animation ? "Choose a GLB animation." : "Choose a rig-matched RBW garment.");
        auto bytes=ftebroom::jam::fileBytes(path,animation ? characterMotionLimit : characterWardrobeLimit);
        const auto id=ftebroom::jam::sha256(bytes);
        characterRegisterImport(id,std::move(bytes),animation);
        auto& s=characterEditor;
        characterCheckpoint(s.doc);
        if (animation) {
          s.doc.animationAsset=id;
          s.doc.clip=cc::importedClipId;
          s.time=0;
        } else s.doc.customWardrobe=id;
        s.uploaded=false;
        ++s.revision;
      },false);
}

static void characterHistory(bool redo) {
  auto& s = characterEditor;
  auto& from = redo ? s.redo : s.undo;
  auto& to = redo ? s.undo : s.redo;
  if (from.empty()) return;
  to.push_back(s.doc);
  s.doc = std::move(from.back());
  from.pop_back();
  ++s.revision;
}

static void characterFrame(CharacterEditorState::Framing framing) {
  auto& s = characterEditor;
  s.framing = framing;
  s.pan = {};
  s.yaw = framing==CharacterEditorState::Framing::Portrait ? 0.f : 12.f;
  s.pitch = framing==CharacterEditorState::Framing::Portrait ? 0.f : 6.f;
  if (framing==CharacterEditorState::Framing::UpperBody) {
    s.yaw = -16;
    s.pitch = 4;
  }
  s.zoom = framing==CharacterEditorState::Framing::Portrait ? .195f :
           framing==CharacterEditorState::Framing::UpperBody ? .36f : .79f;
  s.uploaded = false;
}

static void characterFinishSculpt(bool cancel = false) {
  auto& s = characterEditor;
  if (!s.sculptStart) return;
  if (s.doc.values!=*s.sculptStart) {
    if (cancel) {
      s.doc.values = *s.sculptStart;
      ++s.revision;
    } else {
      auto before = s.doc;
      before.values = *s.sculptStart;
      characterCheckpoint(before);
    }
  }
  s.sculptStart.reset();
}

struct CharacterProjection {
  ImVec2 pos, size;
  cc::Vec3 forward, up, right, eye;
  double orthoScale = 0;
  ImVec2 point(const cc::Vec3& world) const {
    const auto delta = cc::sub(world,eye);
    const auto scale = orthoScale ? orthoScale :
        size.y/(2*std::tan(22.5*bb::pi/180)*std::max(.1,cc::dot(delta,forward)));
    return {pos.x+size.x*.5f+float(scale*cc::dot(delta,right)),
            pos.y+size.y*.5f-float(scale*cc::dot(delta,up))};
  }
};

static CharacterProjection characterProjection(ImVec2 pos, ImVec2 size) {
  const auto& s = characterEditor;
  const double yaw = s.yaw*bb::pi/180, pitch = s.pitch*bb::pi/180;
  const cc::Vec3 forward{std::cos(yaw)*std::cos(pitch),std::sin(yaw)*std::cos(pitch),-std::sin(pitch)};
  const cc::Vec3 right{std::sin(yaw),-std::cos(yaw),0};
  const auto extent = bb::length({20,46,70});
  const auto eye = cc::sub(cc::add(s.pan,{0,0,35}),cc::mul(forward,extent*1.4*s.zoom));
  return {pos,size,forward,cc::cross(right,forward),right,eye,s.ortho ? size.x/(extent*s.zoom) : 0};
}

static bool characterSculpt(const CharacterProjection& projection,const std::vector<cc::Mat4>& skin) {
  auto& s = characterEditor;
  auto& io = ImGui::GetIO();
  if (s.tab!=CharacterEditorState::Tab::Face || s.playing || s.wire) {
    characterFinishSculpt();
    return false;
  }
  if (s.sculptStart && (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                        ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
    characterFinishSculpt(true);
    s.suppressFaceOrbit = true;
    return true;
  }
  if (s.sculptStart && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) characterFinishSculpt();
  // Only offer points on the visible side when orbiting away from the front.
  const auto angle = std::remainder(s.yaw,360.f);
  if (std::abs(angle)>82.f || std::abs(s.pitch)>50.f) return false;
  const auto joint = std::find_if(s.fitted.joints.begin(),s.fitted.joints.end(),
                                  [](const auto& bone) { return bone.name=="head"; });
  const auto& matrix = skin.at(joint-s.fitted.joints.begin());
  auto point = [&](const cc::Vec3& p) {
    return projection.point(bb::bbToQuake(matrix.point(cc::adultProportions(p)),0,4*cc::bodyScale(s.doc)));
  };
  int hovered = -1, side = 1;
  ImVec2 target{};
  float targetRadius = 0, best = 1.1f;
  auto list = ImGui::GetWindowDrawList();
  if (ImGui::IsItemHovered() || s.sculptStart || placementQa()) {
    for (unsigned i=0; i<std::size(cc::faceRegions); ++i) {
      const auto& region = cc::faceRegions[i];
      for (int sign : {1,-1}) {
        if (sign==-1 && !region.paired) continue;
        if (region.paired && std::abs(angle)>43.f && sign*angle<0) continue;
        const auto& anchor = s.rig->vertices[s.sculptAnchors[i][sign==-1]];
        const auto p = cc::add(cc::morphPoint(s.doc,anchor.position),cc::faceShapeDelta(*s.rig,s.doc,anchor.source));
        auto edge = p;
        edge[1] += region.radius;
        const auto center = point(p);
        const auto radius = std::max(8.f,std::abs(center.y-point(edge).y));
        if (placementQa() && sign==1) {
          auto key = std::string("character/region/")+region.label;
          placementWidgets[key]={{"x",center.x-radius},{"y",center.y-radius},
                                 {"width",radius*2},{"height",radius*2}};
        }
        const auto dx = (io.MousePos.x-center.x)/radius, dy = (io.MousePos.y-center.y)/radius;
        const auto distance = dx*dx+dy*dy;
        const bool dragging = s.sculptStart && s.faceRegion==int(i) && s.sculptSide==sign;
        if (dragging || (ImGui::IsItemHovered() && !s.sculptStart && distance<best && distance<=1)) {
          hovered = int(i);
          side = sign;
          best = distance;
          target = center;
          targetRadius = radius;
        }
      }
    }
  }
  if (hovered>=0) {
    list->PushClipRect(projection.pos,{projection.pos.x+projection.size.x,projection.pos.y+projection.size.y},true);
    list->AddCircleFilled(target,targetRadius,IM_COL32(69,216,196,43));
    list->AddCircle(target,targetRadius,IM_COL32(139,239,220,240),24,1.4f);
    list->PopClipRect();
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      s.faceRegion = hovered;
      s.faceTypes = 0;
      s.sculptSide = side;
      s.sculptDepth = io.KeyShift;
      s.sculptStart = s.doc.values;
    }
  }
  if (!s.sculptStart) return false;
  const auto& region = cc::faceRegions[s.faceRegion];
  const auto motion = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left,1);
  const double sensitivity = 3.0/std::max(projection.size.y,240.f);
  auto values = *s.sculptStart;
  if (s.sculptDepth) {
    values[region.depth] = std::clamp(values[region.depth]-motion.y*sensitivity,-1.,1.);
  } else {
    const auto sign = region.paired ? s.sculptSide : 1;
    values[region.across] = std::clamp(values[region.across]+motion.x*sensitivity*sign,-1.,1.);
    values[region.vertical] = std::clamp(values[region.vertical]-motion.y*sensitivity,-1.,1.);
  }
  if (values!=s.doc.values) {
    s.doc.values = values;
    ++s.revision;
  }
  return true;
}

static bool characterViewport(const cc::Clip& clip) {
  auto& s = characterEditor;
  if (bbActive || modelViewerOpen || plantState.owner) {
    characterReleasePreview();
    ImGui::TextWrapped("The shared preview is in use.");
    if (ImGui::Button("Switch preview to Character Creator")) {
      bbLeave([] {
        plantReleasePreview();
        modelViewerOpen = modelEditorActive = false;
        modelEditorCameraEnd();
      });
    }
    return false;
  }
  if (!s.owner) {
    bbRegister();
    s.owner = true;
  }
  if (s.rest.empty()) {
    ImGui::TextUnformatted("Preparing the humanoid mesh...");
    return false;
  }
  const bool comparison = placementQa() && cvars->GetFloat("ftebroom_character_compare_pose")!=0;
  // Keep the render target usable when scaled controls would take all its height.
  const bool compact = ImGui::GetContentRegionAvail().y < 64 + 3*ImGui::GetFrameHeightWithSpacing();
  if (compact && ImGui::Button("View options")) ImGui::OpenPopup("Character view options");
  const bool optionsOpen = compact && ImGui::BeginPopup("Character view options");
  if (!compact || optionsOpen) {
    ImGui::Checkbox("Grid",&s.grid); ImGui::SameLine();
    ImGui::Checkbox("Wireframe",&s.wire); ImGui::SameLine();
    ImGui::Checkbox("Orthographic",&s.ortho);
    if (ImGui::GetContentRegionAvail().x>=470*uiScale()) ImGui::SameLine();
    ImGui::SetNextItemWidth(128*uiScale());
    const char* detailLevels[]{"Full detail","Balanced (15k)","Light (9k)"};
    const bool choosingDetail=ImGui::BeginCombo("Preview detail",detailLevels[s.detailLevel]);
    placementRect("character/preview-detail");
    if (choosingDetail) {
      for (unsigned index=0; index<IM_ARRAYSIZE(detailLevels); ++index) {
        if (ImGui::Selectable(detailLevels[index],s.detailLevel==int(index)) && s.detailLevel!=int(index)) {
          s.detailLevel=int(index);
          s.reduced.reset();
          s.uploaded=false;
          ++s.revision;
        }
        placementRect(("character/preview-detail/"+std::to_string(index)).c_str());
      }
      ImGui::EndCombo();
    }
    if (ImGui::Button("Front")) { s.yaw = s.pitch = 0; s.uploaded = false; }
    ImGui::SameLine();
    if (ImGui::Button("Side")) { s.yaw = 90; s.pitch = 0; s.uploaded = false; }
    placementRect("character/side");
    ImGui::SameLine();
    const bool resetFrame = ImGui::Button("Frame");
    placementRect("character/frame");
    if (resetFrame) characterFrame(CharacterEditorState::Framing::Body);
    ImGui::SameLine();
    const bool portrait = ImGui::Button("Portrait");
    placementRect("character/portrait");
    if (portrait) characterFrame(CharacterEditorState::Framing::Portrait);
    ImGui::SameLine();
    const bool upperBody = ImGui::Button("Upper body");
    placementRect("character/upper-body");
    if (upperBody) characterFrame(CharacterEditorState::Framing::UpperBody);
  }
  if (optionsOpen) ImGui::EndPopup();
  if (s.framing==CharacterEditorState::Framing::Portrait ||
      s.framing==CharacterEditorState::Framing::UpperBody) {
    const bool head = s.framing == CharacterEditorState::Framing::Portrait;
    const auto height = cc::bodyScale(s.doc);
    s.pan = {-3*height,0,(head ? 61.5 : 50.5)*height-35};
  }
  const auto size = ImGui::GetContentRegionAvail();
  if (size.x < 64 || size.y < 64) return false;
  const auto pos = ImGui::GetCursorScreenPos();
  if (!modelViewerImage)
    modelViewerImage = draw->LoadImageShader("ftebroom/modelviewer-image",
        "{ program default2d { map $rt:ftebroom_modelviewer rgbgen vertex alphagen vertex } }");
  ImGui::GetWindowDrawList()->AddImage((ImTextureID)modelViewerImage,pos,{pos.x+size.x,pos.y+size.y});
  ImGui::InvisibleButton("character-viewport",size,
      ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
  placementRect("character/viewport");
  auto& io = ImGui::GetIO();
  const auto previewPose = [&] {
    const bool useCapture = gameProfile==1;
    const cc::Animator animator(s.fitted,useCapture);
    if (comparison) return animator.comparisonPose();
    const auto imported=characterImportedMotion(s.doc);
    const auto primary = cc::sampleCharacterClip(animator,clip,s.time,imported.get());
    if (!s.blend) return primary;
    const auto layer = animator.sample(cc::captureTiming(cc::clips[s.blendClip],useCapture),s.time);
    if (s.blendUpperBody) return cc::blendAnimation(primary,layer,s.blendWeight,s.upperBody);
    return cc::blendAnimation(primary,layer,s.blendWeight);
  };
  auto projection = characterProjection(pos,size);
  bool sculpting = false;
  if (s.tab==CharacterEditorState::Tab::Face && (ImGui::IsItemHovered() || s.sculptStart || placementQa())) {
    const auto activePose = previewPose();
    sculpting = characterSculpt(projection,cc::skinMatrices(s.fitted,activePose.rotations,activePose.root));
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) s.suppressFaceOrbit = false;
  if (!sculpting && !s.suppressFaceOrbit && ImGui::IsItemActive() &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    s.yaw -= io.MouseDelta.x * .4f;
    s.uploaded = false;
    s.pitch = std::clamp(s.pitch - io.MouseDelta.y * .4f,-85.f,85.f);
  }
  if (!sculpting && ImGui::IsItemActive() &&
      (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))) {
    const auto yaw = s.yaw * bb::pi / 180;
    const auto pitch = s.pitch * bb::pi / 180;
    const cc::Vec3 right{std::sin(yaw),-std::cos(yaw),0};
    const cc::Vec3 up{std::sin(pitch)*std::cos(yaw),std::sin(pitch)*std::sin(yaw),std::cos(pitch)};
    s.framing = CharacterEditorState::Framing::Manual;
    s.pan = cc::add(s.pan,cc::mul(cc::add(cc::mul(right,-io.MouseDelta.x),cc::mul(up,io.MouseDelta.y)),
                                  82 * s.zoom / size.y));
  }
  if (!sculpting && ImGui::IsItemHovered() && io.MouseWheel) {
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    s.zoom = std::clamp(s.zoom * std::pow(1.12f,-io.MouseWheel),.18f,4.f);
  }
  const bool useMaterials = s.shading == cc::PreviewShading::Materials;
  const bool skinShader = useMaterials && gameProfile==1 && !s.wire;
  const bool pbr = skinShader && bbSmoothNormals();
  if (skinShader!=s.skinShaderActive || pbr!=s.pbrPreview) {
    if (!bbActive && !plantState.owner) {
      if (auto mesh = bbAPI()) mesh->clear();
      if (auto images = bbImageAPI()) images->clear();
      s.materialUploads.clear();
    }
    s.skinShaderActive = skinShader;
    s.pbrPreview = pbr;
    s.uploaded = false;
  }
  if (!s.uploaded || (s.playing && ImGui::GetTime() >= s.nextUpload)) {
    auto api = bbAPI();
    if (!api) throw std::runtime_error("The engine does not provide the native mesh preview.");
    const auto pose = previewPose();
    const auto maps = cc::surfaceMaps(s.doc,s.pbrPreview);
    const auto textures = useMaterials ? characterPreviewMaterials(s,maps)
                                       : std::array<std::string,cc::materialCount>{};
    std::array<cc::Vec3,cc::materialCount> tints;
    for (unsigned i = 0; i < maps.size(); ++i)
      tints[i] = cc::surfaceTint(i,s.doc,maps[i]);
    const auto yaw = s.yaw*bb::pi/180, pitch = s.pitch*bb::pi/180;
    const cc::Vec3 view{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
    const bool useLod=s.reduced && s.applied==s.revision &&
        s.tab!=CharacterEditorState::Tab::Face && s.framing!=CharacterEditorState::Framing::Portrait;
    const auto mesh = cc::previewMesh(useLod ? *s.reduced : s.fitted,s.doc,
        useLod ? s.reducedRest : s.rest,pose,textures,tints,view,s.pbrPreview,s.shading,s.bone);
    if (!api->upload(mesh.data(),mesh.size()))
      throw std::runtime_error("The engine rejected the character mesh.");
    s.uploaded = true;
    s.nextUpload = ImGui::GetTime() + 1. / 30;
  }
  if ((s.bones || s.showSockets) && !s.fitted.joints.empty()) {
    const auto pose = previewPose();
    const auto skin = cc::skinMatrices(s.fitted,pose.rotations,pose.root);
    projection = characterProjection(pos,size);
    std::vector<ImVec2> marks;
    marks.reserve(skin.size());
    for (std::size_t i = 0; i < skin.size(); ++i) {
      const auto point = bb::bbToQuake(skin[i].point(s.fitted.joints[i].bindPosition),0,4*cc::bodyScale(s.doc));
      marks.push_back(projection.point(point));
    }
    auto list = ImGui::GetWindowDrawList();
    list->PushClipRect(pos,{pos.x+size.x,pos.y+size.y},true);
    for (std::size_t i = 0; s.bones && i < marks.size(); ++i) {
      const auto parent = s.rig->joints[i].parent;
      const auto color = s.bone == i ? IM_COL32(250,218,128,255) : IM_COL32(94,206,192,215);
      if (parent >= 0) list->AddLine(marks[parent],marks[i],color,1.3f);
      list->AddCircleFilled(marks[i],s.bone == i ? 4.f : 2.5f,color);
    }
    if (s.showSockets)
      for (const auto& socket : cc::gearSockets) {
        const auto index=cc::socketJoint(s.fitted.joints,socket);
        const auto point=skin[index].point(cc::add(s.fitted.joints[index].bindPosition,socket.offset));
        const auto screen=projection.point(bb::bbToQuake(point,0,4*cc::bodyScale(s.doc)));
        list->AddCircle(screen,5.f,IM_COL32(255,207,118,255),0,1.8f);
        list->AddText({screen.x+7,screen.y-9},IM_COL32(255,233,184,255),socket.id);
      }
    list->PopClipRect();
  }
  cvars->SetFloat("ftebroom_bb_active",1);
  cvars->SetFloat("ftebroom_modelviewer_open",1);
  cvars->SetFloat("ftebroom_modelviewer_width",size.x);
  cvars->SetFloat("ftebroom_modelviewer_height",size.y);
  cvars->SetFloat("ftebroom_modelviewer_yaw",s.yaw);
  cvars->SetFloat("ftebroom_modelviewer_pitch",s.pitch);
  cvars->SetFloat("ftebroom_modelviewer_zoom",comparison ?
      float(size.x/size.y*.98*cc::bodyScale(s.doc)) : s.zoom);
  auto vectorString = [](const cc::Vec3& point) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << point[0] << ' ' << point[1] << ' ' << point[2];
    return out.str();
  };
  cvars->SetString("ftebroom_bb_low",vectorString(cc::add(s.pan,{-10,-23,0})).c_str());
  cvars->SetString("ftebroom_bb_high",vectorString(cc::add(s.pan,{10,23,70})).c_str());
  cvars->SetFloat("ftebroom_bb_ortho",comparison || s.ortho);
  cvars->SetFloat("ftebroom_bb_display",s.wire ? 2 : 0);
  cvars->SetFloat("ftebroom_bb_cull",0);
  cvars->SetFloat("ftebroom_modeleditor_floor",s.grid && !comparison);
  cvars->SetFloat("ftebroom_modeleditor_axes",0);
  cvars->SetFloat("ftebroom_modeleditor_player",0);
  cvars->SetFloat("ftebroom_modeleditor_freecamera",0);
  cvars->SetFloat("ftebroom_modeleditor_active",1);
  return true;
}

static void characterMorphSliders(std::size_t first, std::size_t end) {
  auto& s = characterEditor;
  const auto reset = [&] {
    const bool body = first==0 && gameProfile==1;
    if ((!body || s.doc.bodyProfile==0.) &&
        std::all_of(s.doc.values.begin()+first,s.doc.values.begin()+end,
                    [](double value) { return value==0.; })) return;
    characterCheckpoint(s.doc);
    if (body) s.doc.bodyProfile = 0.;
    std::fill(s.doc.values.begin()+first,s.doc.values.begin()+end,0.);
    ++s.revision;
    s.uploaded = false;
  };
  if (ImGui::SmallButton(first==0 ? "Reset body proportions" : "Reset facial proportions")) reset();
  for (auto i = first; i < end; ++i) {
    auto amount = float(s.doc.values[i]);
    auto before = s.doc;
    const auto* label = cc::morphs[i].label;
    const auto width = ImGui::GetContentRegionAvail().x;
    const auto fieldRoom = width-ImGui::CalcTextSize(label).x-ImGui::GetStyle().ItemInnerSpacing.x;
    const bool stacked = fieldRoom < 55*uiScale();
    ImGui::PushID(label);
    if (stacked) ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(stacked ? -FLT_MIN : std::min({135*uiScale(),width*.52f,fieldRoom}));
    const auto sliderWidth = ImGui::CalcItemWidth();
    const bool changed = ImGui::SliderFloat(stacked ? "##value" : label,&amount,-1,1,"%.2f",
                                          ImGuiSliderFlags_AlwaysClamp);
    if (placementQa()) {
      const auto id = std::string("character/morph/")+cc::morphs[i].id;
      placementRect(id.c_str());
      placementWidgets[id]["width"] = sliderWidth;
      placementWidgets[id]["value"] = amount;
    }
    if (ImGui::IsItemActivated()) s.gesture = before;
    if (changed) {
      s.doc.values[i] = amount;
      ++s.revision;
      if (!i) s.uploaded = false;
    }
    if (ImGui::IsItemDeactivated()) {
      if (ImGui::IsItemDeactivatedAfterEdit() && s.gesture) characterCheckpoint(*s.gesture);
      s.gesture.reset();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl-click to type a value. Right-click to reset.");
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Reset",nullptr,false,s.doc.values[i]!=0.)) {
        characterCheckpoint(s.doc);
        s.doc.values[i] = 0.;
        ++s.revision;
        s.uploaded = false;
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }
}

static void characterAdjustShape(const char* label,double& value,const char* qa = nullptr) {
  auto& s = characterEditor;
  const auto before = s.doc;
  auto amount = float(value);
  const auto width = ImGui::GetContentRegionAvail().x;
  const auto fieldRoom = width-ImGui::CalcTextSize(label).x-ImGui::GetStyle().ItemInnerSpacing.x;
  const bool stacked = fieldRoom < 48*uiScale();
  ImGui::PushID(label);
  if (stacked) ImGui::TextUnformatted(label);
  ImGui::SetNextItemWidth(stacked ? -FLT_MIN : std::min(155*uiScale(),fieldRoom));
  const bool changed = ImGui::SliderFloat(stacked ? "##value" : label,&amount,-1,1,"%.2f",
                                         ImGuiSliderFlags_AlwaysClamp);
  if (qa) placementRect(qa);
  if (ImGui::IsItemActivated()) s.gesture = before;
  if (changed) {
    value = amount;
    ++s.revision;
  }
  if (ImGui::IsItemDeactivated()) {
    if (ImGui::IsItemDeactivatedAfterEdit() && s.gesture) characterCheckpoint(*s.gesture);
    s.gesture.reset();
  }
  ImGui::PopID();
}

static bool characterPropertyCombo(const char* label,const char* selection,ImGuiComboFlags flags = 0) {
  const auto width = ImGui::GetContentRegionAvail().x;
  const auto fieldRoom = width-ImGui::CalcTextSize(label).x-ImGui::GetStyle().ItemInnerSpacing.x;
  const auto minField = std::min(145*uiScale(),ImGui::CalcTextSize(selection).x+
      ImGui::GetFrameHeight()+2*ImGui::GetStyle().FramePadding.x);
  if (fieldRoom < minField) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    return ImGui::BeginCombo((std::string("##")+label).c_str(),selection,flags);
  }
  ImGui::SetNextItemWidth(std::min(width*.66f,fieldRoom));
  return ImGui::BeginCombo(label,selection,flags);
}

static void characterFaceTools() {
  auto& s = characterEditor;
  ImGui::TextWrapped("Drag a feature on the face. Shift-drag changes depth. Right-click cancels a drag.");
  if (characterPropertyCombo("Feature",s.faceRegion<0 ? "Select a feature" : cc::faceRegions[s.faceRegion].label)) {
    for (unsigned i=0; i<std::size(cc::faceRegions); ++i) {
      if (!ImGui::Selectable(cc::faceRegions[i].label,s.faceRegion==int(i))) continue;
      s.faceRegion = int(i);
      s.faceTypes = 0;
    }
    ImGui::EndCombo();
  }
  if (s.faceRegion<0) return;
  const auto& region = cc::faceRegions[s.faceRegion];
  constexpr const char* shapes[]{"Custom","Balanced","Narrow","Wide","Recessed","Pronounced"};
  if (characterPropertyCombo("Feature shape",shapes[s.faceTypes])) {
    for (int i=1; i<int(std::size(shapes)); ++i) {
      if (!ImGui::Selectable(shapes[i],s.faceTypes==i)) continue;
      constexpr double width[]{0,0,-.61,.61,0,0}, depth[]{0,0,0,0,-.65,.65};
      characterCheckpoint(s.doc);
      s.doc.values[region.across] = width[i];
      s.doc.values[region.vertical] = 0;
      s.doc.values[region.depth] = depth[i];
      s.faceTypes = i;
      ++s.revision;
    }
    ImGui::EndCombo();
  }
  const auto before = s.revision;
  const cc::MorphIndex indexes[]{region.across,region.vertical,region.depth,region.extra};
  for (unsigned i=0; i<std::size(indexes); ++i) {
    const auto index = indexes[i];
    if (std::find(indexes,indexes+i,index)!=indexes+i) continue;
    characterAdjustShape(cc::morphs[index].label,s.doc.values[index]);
  }
  if (s.revision!=before) s.faceTypes = 0;
  ImGui::Separator();
}

static void characterChangeTab(CharacterEditorState::Tab tab) {
  auto& s = characterEditor;
  if (tab==s.tab) return;
  characterFinishSculpt();
  s.tab = tab;
  using Framing = CharacterEditorState::Framing;
  using Tab = CharacterEditorState::Tab;
  if (tab==Tab::Face || tab==Tab::Appearance || tab==Tab::Wardrobe) {
    if (tab==Tab::Face) {
      s.playing = false;
      s.time = 0;
    }
    characterFrame(Framing::Portrait);
  } else {
    characterFrame(Framing::Body);
  }
}

static void characterChooseProfileExport(int profile) {
  const auto mapProfile = gameProfile;
  const auto filename = std::string("human")+characterExtension(profile);
  runUiChoose((documentDefaultFolder()/filename).u8string(),[profile,mapProfile](const std::string& path) {
    characterRequireOffline();
    if (gameProfile!=mapProfile) throw std::runtime_error("The game profile changed.");
    characterExportProfile(std::filesystem::u8path(path),profile);
  },true);
}

static void characterExportSettings() {
  auto& s = characterEditor;
  constexpr const char* profiles[]{"Use active map profile","Quake III Arena","FTE Custom game",
                                    "Quake 1","Quake II","Half-Life"};
  static_assert(std::size(profiles)==std::size(cc::exportProfileIds)+1);
  const bool choose = characterPropertyCombo("Target",profiles[s.doc.exportProfile+1]);
  placementRect("character/export-target");
  if (choose) {
    for (int i=0; i<int(std::size(profiles)); ++i) {
      const bool selected = ImGui::Selectable(profiles[i],i==s.doc.exportProfile+1);
      placementRect(("character/export-target/"+std::to_string(i)).c_str());
      if (selected && i!=s.doc.exportProfile+1) {
        characterCheckpoint(s.doc);
        s.doc.exportProfile = i-1;
      }
    }
    ImGui::EndCombo();
  }
  const int target = s.doc.exportProfile<0 ? gameProfile : s.doc.exportProfile;
  const bool needsPalette = (target==2 || target==3) && target!=gameProfile;
  if (target==1) ImGui::TextWrapped("IQM with a weighted skeleton, named clips and material files.");
  else if (target==4) ImGui::TextWrapped("Studio MDL with a skeleton and named sequences.");
  else ImGui::TextWrapped("Vertex animation. The game or mod must advance the frames.");
  if (needsPalette) ImGui::TextWrapped("Select the matching game profile to supply its palette.");
  ImGui::BeginDisabled(needsPalette);
  const bool exportModel = placementButton("Export model...","character/export-selected");
  ImGui::EndDisabled();
  if (exportModel) {
    try { characterChooseProfileExport(target); }
    catch (const std::exception& error) { s.error = error.what(); }
  }
}

static void characterProperties() {
  auto& s = characterEditor;
  using Tab = CharacterEditorState::Tab;
  constexpr const char* sections[]{"Body","Face","Appearance","Wardrobe","Rig","Export"};
  constexpr const char* ids[]{"character/tab-body","character/tab-face","character/tab-appearance",
                             "character/tab-wardrobe","character/tab-rig","character/tab-export"};
  // Keep a single selection when docking changes the available width.
  const auto& style = ImGui::GetStyle();
  float remaining = ImGui::GetContentRegionAvail().x;
  for (int i=0; i<int(std::size(sections)); ++i) {
    const auto tab = static_cast<Tab>(i);
    const float width = ImGui::CalcTextSize(sections[i]).x + 2*style.FramePadding.x;
    const float gap = style.ItemSpacing.x;
    if (i>0 && remaining >= gap+width) {
      ImGui::SameLine();
      remaining -= gap;
    } else {
      remaining = ImGui::GetContentRegionAvail().x;
    }
    const bool active = s.tab==tab;
    if (active) ImGui::PushStyleColor(ImGuiCol_Button,style.Colors[ImGuiCol_HeaderActive]);
    const bool selected = ImGui::ButtonEx(sections[i],{},ImGuiButtonFlags_PressedOnClick);
    placementRect(ids[i]);
    if (selected) characterChangeTab(tab);
    if (active) ImGui::PopStyleColor();
    remaining -= width;
  }

  if (s.tab==Tab::Body) {
    if (gameProfile==1) {
      characterAdjustShape("Body structure",s.doc.bodyProfile,"character/body-profile");
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Feminine (-1), neutral (0), masculine (+1). Independent of facial structure.");
    }
    characterMorphSliders(0,cc::bodyMorphCount);
  }
  if (s.tab==Tab::Face) {
    characterFaceTools();
    const bool chooseFaceTemplate = characterPropertyCombo("Face template",cc::faceTemplateLabel(s.doc.faceTemplate));
    placementRect("character/face-template");
    if (chooseFaceTemplate) {
      for (unsigned i=0; i<cc::selectableFaceTemplateCount; ++i) {
        const bool selected = ImGui::Selectable(cc::faceTemplateNames[i],i==s.doc.faceTemplate);
        const auto item = std::string("character/face-template/")+std::to_string(i);
        placementRect(item.c_str());
        if (!selected || i==s.doc.faceTemplate) continue;
        characterCheckpoint(s.doc);
        s.doc.faceTemplate = i;
        ++s.revision;
      }
      ImGui::EndCombo();
    }
    characterAdjustShape("Facial structure",s.doc.faceProfile,"character/face-profile");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Blend from feminine (-1) through neutral to masculine (+1).");
    characterAdjustShape("Asymmetry",s.doc.faceAsymmetry);
    if (characterPropertyCombo("Eyebrows",cc::eyebrowNames[s.doc.eyebrow])) {
      for (unsigned i=0; i<cc::eyebrowCount; ++i) {
        if (!ImGui::Selectable(cc::eyebrowNames[i],i==s.doc.eyebrow) || i==s.doc.eyebrow) continue;
        characterCheckpoint(s.doc);
        s.doc.eyebrow = i;
        ++s.revision;
      }
      ImGui::EndCombo();
    }

    if (characterPropertyCombo("Expression",cc::expressions[s.doc.expression])) {
      for (unsigned i = 0; i < std::size(cc::expressions); ++i)
        if (ImGui::Selectable(cc::expressions[i],i == s.doc.expression)) {
          characterCheckpoint(s.doc);
          s.doc.expression = i;
          ++s.revision;
        }
      ImGui::EndCombo();
    }
    const bool allDetails = ImGui::CollapsingHeader("All facial details");
    placementRect("character/all-face-details");
    if (allDetails) {
      ImGui::PushID("all facial details");
      characterMorphSliders(cc::bodyMorphCount,cc::morphCount);
      ImGui::PopID();
    }
  }
  if (s.tab==Tab::Appearance) {
    if (characterPropertyCombo("Complexion",cc::complexionNames[s.doc.complexion])) {
      for (unsigned i = 0; i < cc::complexionCount; ++i) {
        if (ImGui::Selectable(cc::complexionNames[i],i==s.doc.complexion) && i!=s.doc.complexion) {
          characterCheckpoint(s.doc);
          s.doc.complexion = i;
          s.uploaded = false;
        }
      }
      ImGui::EndCombo();
    }
    for (const char* title : {"Skin","Eyes","Hair tint"}) {
      auto& values = title[0] == 'S' ? s.doc.skin : title[0] == 'E' ? s.doc.eyes : s.doc.hairColor;
      float color[]{float(values[0]),float(values[1]),float(values[2])};
      const auto before = s.doc;
      const auto changed = ImGui::ColorEdit3(title,color);
      if (ImGui::IsItemActivated()) s.gesture = before;
      if (changed) {
        values = {color[0],color[1],color[2]};
        s.uploaded = false;
      }
      if (ImGui::IsItemDeactivated()) {
        if (ImGui::IsItemDeactivatedAfterEdit() && s.gesture) characterCheckpoint(*s.gesture);
        s.gesture.reset();
      }
    }
  }
  if (s.tab==Tab::Wardrobe) {
    const bool chooseOutfit=characterPropertyCombo("Outfit",cc::outfits[s.doc.outfit].label,
                                                    ImGuiComboFlags_HeightLargest);
    placementRect("character/outfit");
    if (chooseOutfit) {
      for (unsigned i = 0; i < std::size(cc::outfits); ++i) {
        const bool selected=ImGui::Selectable(cc::outfits[i].label,i == s.doc.outfit);
        placementRect((std::string("character/outfit/")+std::to_string(i)).c_str());
        if (!selected || i==s.doc.outfit) continue;
        characterCheckpoint(s.doc);
        s.doc.outfit = i;
        ++s.revision;
      }
      ImGui::EndCombo();
    }
    const bool chooseHairTexture = characterPropertyCombo("Hair texture",cc::hairTextureNames[s.hairFilter]);
    placementRect("character/hair-texture");
    if (chooseHairTexture) {
      for (unsigned i=0; i<std::size(cc::hairTextureNames); ++i) {
        const bool selected = ImGui::Selectable(cc::hairTextureNames[i],i==s.hairFilter);
        const auto item = std::string("character/hair-texture/")+std::to_string(i);
        placementRect(item.c_str());
        if (selected) s.hairFilter = i;
      }
      ImGui::EndCombo();
    }
    const bool chooseHair = characterPropertyCombo("Hairstyle",cc::hairstyles[s.doc.hairstyle].label,
                                             ImGuiComboFlags_HeightLargest);
    placementRect("character/hairstyle");
    if (chooseHair) {
      for (unsigned i=0; i<cc::hairstyleCount; ++i) {
        const auto& style = cc::hairstyles[i];
        if (s.hairFilter && i && unsigned(style.texture)!=s.hairFilter) continue;
        const bool selected = ImGui::Selectable(style.label,i==s.doc.hairstyle);
        const auto item = std::string("character/hairstyle/")+std::to_string(i);
        placementRect(item.c_str());
        if (!selected || i==s.doc.hairstyle) continue;
        characterCheckpoint(s.doc);
        s.doc.hairstyle = i;
        s.doc.headwear = 1;
        ++s.revision;
      }
      ImGui::EndCombo();
    }
    if (s.doc.hairstyle) {
      characterAdjustShape("Hair volume",s.doc.hairVolume);
      characterAdjustShape("Hair length",s.doc.hairLength);
      characterAdjustShape("Hairline",s.doc.hairline);
    }
    const bool chooseBeard=characterPropertyCombo("Facial hair",cc::beards[s.doc.beard]);
    placementRect("character/beard");
    if (chooseBeard) {
      for (unsigned i = 0; i < std::size(cc::beards); ++i) {
        const bool selected=ImGui::Selectable(cc::beards[i],i == s.doc.beard);
        placementRect((std::string("character/beard/")+std::to_string(i)).c_str());
        if (!selected || i==s.doc.beard) continue;
        characterCheckpoint(s.doc);
        s.doc.beard = i;
        ++s.revision;
      }
      ImGui::EndCombo();
    }
    auto chooseLayer = [&](const char* label, const auto& choices, unsigned& selection) {
      if (!characterPropertyCombo(label,choices[selection])) return;
      for (unsigned i = 0; i < std::size(choices); ++i)
        if (ImGui::Selectable(choices[i],i == selection) && i != selection) {
          characterCheckpoint(s.doc);
          selection = i;
          ++s.revision;
        }
      ImGui::EndCombo();
    };
    chooseLayer("Footwear",cc::footwearLabels,s.doc.footwear);
    chooseLayer("Headwear",cc::headwearLabels,s.doc.headwear);
    if (placementButton("Import fitted garment...","character/import-garment")) characterChooseImport(false);
    if (!s.doc.customWardrobe.empty()) {
      ImGui::TextDisabled("Imported garment is active");
      if (ImGui::Button("Restore outfit garment")) {
        characterCheckpoint(s.doc);
        s.doc.customWardrobe.clear();
        ++s.revision;
      }
    }
  }
  if (s.tab==Tab::Rig) {
    ImGui::Text("RB_Humanoid_v1 | %zu joints",s.rig->joints.size());
    const char* shadingModes[]{"Materials","Normals","Skin weights"};
    const bool choosingShading = characterPropertyCombo("Shading",shadingModes[int(s.shading)]);
    placementRect("character/shading");
    if (choosingShading) {
      for (int index=0; index<IM_ARRAYSIZE(shadingModes); ++index) {
        if (ImGui::Selectable(shadingModes[index],index==int(s.shading))) {
          s.shading = static_cast<cc::PreviewShading>(index);
          s.uploaded = false;
        }
        placementRect(("character/shading/"+std::to_string(index)).c_str());
      }
      ImGui::EndCombo();
    }
    ImGui::Checkbox("Bone overlay",&s.bones);
    ImGui::Checkbox("Gear sockets",&s.showSockets);
    placementRect("character/gear-sockets");
    for (std::size_t i = 0; i < s.rig->joints.size(); ++i) {
      const auto& joint = s.rig->joints[i];
      auto label = joint.name;
      if (joint.parent >= 0) label += " / " + s.rig->joints[joint.parent].name;
      if (ImGui::Selectable(label.c_str(),s.bone == i)) {
        s.bone = i;
        s.bones = true;
        if (s.shading == cc::PreviewShading::SkinWeights) s.uploaded = false;
      }
    }
  }
  if (s.tab==Tab::Export) characterExportSettings();
}

static void characterTimeline(bool compact = false) {
  auto& s = characterEditor;
  const auto imported=characterImportedMotion(s.doc);
  const auto available=cc::characterClips(imported.get());
  const auto width = ImGui::GetContentRegionAvail().x;
  const auto scale = uiScale();
  const bool wrap = width < 465*scale;
  auto clip = std::find_if(available.begin(),available.end(),
                                  [&](const cc::Clip& c) { return s.doc.clip == c.id; });
  if (clip == available.end()) clip = available.begin();
  ImGui::SetNextItemWidth(wrap ? -FLT_MIN : 185*scale);
  const bool chooseClip = ImGui::BeginCombo("##animation",clip->id);
  placementRect("character/clip");
  if (chooseClip) {
    for (auto choice=available.begin(); choice!=available.end(); ++choice) {
      const bool selected = ImGui::Selectable(choice->id,choice == clip);
      placementRect((std::string("character/clip/")+choice->id).c_str());
      if (selected) {
        characterCheckpoint(s.doc);
        s.doc.clip = choice->id;
        clip = choice;
        s.time = 0;
        s.uploaded = false;
      }
    }
    ImGui::EndCombo();
  }
  const auto duration = cc::captureTiming(*clip,gameProfile==1).seconds;
  if (!wrap) ImGui::SameLine();
  if (placementButton(s.playing ? "Pause" : "Play","character/play")) {
    s.playing = !s.playing;
    if (s.playing && s.time >= duration) s.time = 0;
  }
  ImGui::SameLine();
  if (placementButton("<","character/frame-back")) {
    s.playing = false;
    s.time = std::max(0.,s.time-1./30);
    s.uploaded = false;
  }
  ImGui::SameLine();
  if (placementButton(">","character/frame-forward")) {
    s.playing = false;
    s.time = std::min(duration,s.time+1./30);
    s.uploaded = false;
  }
  if (width >= 267*scale) ImGui::SameLine();
  ImGui::SetNextItemWidth(std::min(110*scale,
      std::max(45*scale,width-ImGui::CalcTextSize("Speed").x-ImGui::GetStyle().ItemInnerSpacing.x)));
  ImGui::SliderFloat("Speed",&s.speed,.25f,2.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
  placementRect("character/speed");
  const auto drawPlayback = [&] {
    const char* playbackModes[]{"Clip default","Loop","Once"};
    int playback = static_cast<int>(s.playback);
    ImGui::SetNextItemWidth(std::min(116*scale,
        std::max(45*scale,width-ImGui::CalcTextSize("Playback").x-ImGui::GetStyle().ItemInnerSpacing.x)));
    const bool choosePlayback = ImGui::BeginCombo("Playback",playbackModes[playback]);
    placementRect("character/playback");
    if (choosePlayback) {
      for (int i = 0; i < int(std::size(playbackModes)); ++i) {
        const bool selected = ImGui::Selectable(playbackModes[i],i==playback);
        placementRect((std::string("character/playback/")+playbackModes[i]).c_str());
        if (selected) {
          s.playback = static_cast<CharacterEditorState::Playback>(i);
          s.uploaded = false;
        }
      }
      ImGui::EndCombo();
    }
  };
  if (!compact) drawPlayback();
  if (s.playing) {
    s.time += ImGui::GetIO().DeltaTime*s.speed;
    if (s.time >= duration) {
      const bool loop = s.playback==CharacterEditorState::Playback::Loop ||
                        (s.playback==CharacterEditorState::Playback::ClipDefault && clip->loop);
      s.time = loop ? std::fmod(s.time,duration) : duration;
      if (!loop) { s.playing = false; s.uploaded = false; }
    }
  }
  auto t = float(s.time);
  ImGui::SetNextItemWidth(-1);
  if (ImGui::SliderFloat("##animation-time",&t,0,float(duration),"%.2f s",
                          ImGuiSliderFlags_AlwaysClamp)) {
    s.time = t;
    s.uploaded = false;
  }
  placementRect("character/scrub");
  // In the short timeline, keep the scrubber in the first two visible rows.
  if (compact) drawPlayback();
  if (placementButton("Import GLB animation...","character/import-motion")) characterChooseImport(true);
  if (imported) {
    if (!wrap) ImGui::SameLine();
    if (ImGui::Button("Remove imported animation")) {
      characterCheckpoint(s.doc);
      if (s.doc.clip==cc::importedClipId) s.doc.clip=cc::clips[0].id;
      s.doc.animationAsset.clear();
      s.time=0;
      s.uploaded=false;
    }
  }
  if (!wrap) ImGui::SameLine();
  if (ImGui::Checkbox("Preview blend",&s.blend)) s.uploaded = false;
  if (s.blend) {
    if (!wrap) ImGui::SameLine();
    ImGui::SetNextItemWidth(wrap ? -FLT_MIN : 150*scale);
    if (ImGui::BeginCombo("##blend-clip",cc::clips[s.blendClip].id)) {
      for (std::size_t i = 0; i < std::size(cc::clips); ++i)
        if (ImGui::Selectable(cc::clips[i].id,i==s.blendClip)) {
          s.blendClip = i;
          s.uploaded = false;
        }
      ImGui::EndCombo();
    }
    if (!wrap) ImGui::SameLine();
    ImGui::SetNextItemWidth(104*uiScale());
    if (ImGui::SliderFloat("Weight",&s.blendWeight,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp)) s.uploaded = false;
    ImGui::SameLine();
    if (ImGui::Checkbox("Upper body",&s.blendUpperBody)) s.uploaded = false;
  }
}

static cc::Clip characterSelectedClip() {
  const auto& id = characterEditor.doc.clip;
  if (id==cc::importedClipId)
    if (const auto imported=characterImportedMotion(characterEditor.doc)) return imported->definition();
  const auto clip = std::find_if(std::begin(cc::clips),std::end(cc::clips),
                                [&](const auto& item){ return item.id==id; });
  return cc::captureTiming(clip==std::end(cc::clips) ? cc::clips[0] : *clip,gameProfile==1);
}

static bool characterDrawPreview() {
  try {
    auto clip = characterSelectedClip();
    const auto mode = characterEditor.playback;
    if (mode!=CharacterEditorState::Playback::ClipDefault)
      clip.loop = mode==CharacterEditorState::Playback::Loop;
    return characterViewport(clip);
  } catch (const std::exception& error) {
    characterEditor.error = error.what();
    ImGui::TextWrapped("%s",error.what());
    return false;
  }
}

static void characterDrawSettings() {
  const auto& s = characterEditor;
  if (!s.error.empty()) ImGui::TextWrapped("%s",s.error.c_str());
  if (!s.status.empty()) ImGui::TextWrapped("%s",s.status.c_str());
  characterProperties();
}

static void characterDockPanes(ImGuiID dock) {
  if (!ImGui::DockBuilderGetNode(dock)) {
    ImGui::DockBuilderAddNode(dock,ImGuiDockNodeFlags_DockSpace);
    const auto room = ImGui::GetContentRegionAvail();
    ImGui::DockBuilderSetNodeSize(dock,room);
    ImGuiID main = dock, inspector = 0, timeline = 0;
    ImGui::DockBuilderSplitNode(main,ImGuiDir_Right,.30f,&inspector,&main);
    const auto timelineHeight = 6*ImGui::GetFrameHeightWithSpacing();
    ImGui::DockBuilderSplitNode(main,ImGuiDir_Down,std::clamp(timelineHeight/room.y,.24f,.43f),
                               &timeline,&main);
    ImGui::DockBuilderDockWindow("Preview###rb21-preview",main);
    ImGui::DockBuilderDockWindow("Character Properties###rb21-properties",inspector);
    ImGui::DockBuilderDockWindow("Animation Timeline###rb21-timeline",timeline);
    ImGui::DockBuilderFinish(dock);
  }
  ImGui::DockSpace(dock);
}

static void characterUnsavedDialog() {
  auto& s = characterEditor;
  if (s.pendingDocumentAction && !s.waitingForSavePath)
    ImGui::OpenPopup("Unsaved character changes");
  if (!ImGui::BeginPopupModal("Unsaved character changes",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) return;
  ImGui::TextUnformatted("Save changes to this character before continuing?");
  if (!s.unsavedError.empty()) ImGui::TextWrapped("%s",s.unsavedError.c_str());
  if (placementButton("Save","character/unsaved-save")) {
    try {
      if (s.file.empty()) {
        s.waitingForSavePath = true;
        characterChooseSave([] {
          auto& state = characterEditor;
          state.readyDocumentAction = std::move(state.pendingDocumentAction);
          state.waitingForSavePath = false;
        });
      } else {
        characterSave(s.file);
        s.readyDocumentAction = std::move(s.pendingDocumentAction);
      }
      ImGui::CloseCurrentPopup();
    } catch (const std::exception& error) {
      s.waitingForSavePath = false;
      s.unsavedError = error.what();
    }
  }
  ImGui::SameLine();
  if (placementButton("Discard","character/unsaved-discard")) {
    s.readyDocumentAction = std::move(s.pendingDocumentAction);
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (placementButton("Cancel","character/unsaved-cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    s.pendingDocumentAction = {};
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

static void characterPanel() {
  auto& s = characterEditor;
  if (!s.open || !characterAvailable()) return;
  const auto display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowSize({std::min(display.x * .91f,1120 * uiScale()),
                           std::min(display.y * .88f,770 * uiScale())},ImGuiCond_FirstUseEver);
  bool keep = true, rendered = false;
  bool drawPanes = false;
  if (ImGui::Begin("Character Creator",&keep)) {
    try {
      if (!s.readonly.empty()) {
        ImGui::TextUnformatted(s.readonly.filename().u8string().c_str());
        ImGui::TextWrapped("%s",s.status.c_str());
        if (ImGui::Button("Close read-only document")) s.open = false;
        if (!keep || !s.open) {
          s.open = false;
          s.readonly.clear();
        }
        ImGui::End();
        characterReleasePreview();
        return;
      }
      characterTickMorph();
      if (characterDirty() && ImGui::GetTime() >= s.nextAutosave) {
        s.nextAutosave = ImGui::GetTime()+30;
        const auto file = documentPath("characters/autosave/latest.rbchar");
        if (!file.empty()) {
          try {
            std::filesystem::create_directories(file.parent_path());
            characterSaveDependencies(file,s);
            bb::atomicWrite(file,cc::writeDocument(s.doc,*s.rig).dump(2)+"\n");
          } catch (const std::exception& error) {
            s.error = std::string("Character autosave failed: ")+error.what();
          }
        }
      }
      const bool dirty = characterDirty();
      if (placementButton("New","character/new"))
        characterRequestDocumentAction([] { characterReset(characterNewDocument()); });
      ImGui::SameLine();
      if (placementButton("Open...","character/open"))
        runUiChoose(documentDefaultFolder().u8string(),[](const std::string& source) {
          characterOpenFile(std::filesystem::u8path(source));
        });
      ImGui::SameLine();
      ImGui::BeginDisabled(s.placement.active && (s.placement.awaiting || s.placementJob.valid()));
      const bool save = ImGui::Button("Save");
      placementRect("character/save");
      ImGui::SameLine();
      const bool saveAs = ImGui::Button("Save As...");
      ImGui::EndDisabled();
      if (save || saveAs) {
        try {
          if (saveAs || s.file.empty()) characterChooseSave();
          else characterSave(s.file);
        } catch (const std::exception& error) {
          s.error = error.what();
        }
      }
      ImGui::SameLine();
      const bool exportGlb = ImGui::Button("Export GLB...");
      placementRect("character/export-glb");
      if (exportGlb)
        runUiChoose((documentDefaultFolder()/"human.glb").u8string(),[](const std::string& destination) {
          characterRequireOffline();
          const auto path = std::filesystem::u8path(destination);
          if (path.extension() != ".glb" || std::filesystem::exists(path))
            throw std::runtime_error("Choose a new filename with the .glb extension.");
          const auto& state = characterEditor;
          const cc::WardrobeReader wardrobe=[&](const auto& id) -> const auto& {
            return *state.wardrobe.at(id);
          };
          auto neutral=state.doc;
          neutral.expression=0;
          const auto fitted=cc::assemble(*state.rig,neutral,wardrobe,gameProfile==1);
          const auto targets=cc::glbExpressionTargets(*state.rig,fitted,neutral,wardrobe,gameProfile==1);
          const auto images = characterExportImages(state.doc,state.materialManifest,gameProfile==1);
          const auto motion=characterImportedMotion(state.doc);
          bb::atomicWrite(path,cc::exportGlb(fitted,state.doc,true,images,gameProfile==1,motion.get(),&targets));
        },true);
      ImGui::SameLine();
      if (ImGui::Button("Export IQM..."))
        runUiChoose((documentDefaultFolder()/"human.iqm").u8string(),[](const std::string& destination) {
          characterExportProfile(std::filesystem::u8path(destination),1);
        },true);
      const bool randomize = ImGui::Button("Randomize");
      placementRect("character/randomize");
      if (randomize) {
        characterCheckpoint(s.doc);
        cc::randomize(s.doc,s.doc.seed + 1);
        ++s.revision;
      }
      ImGui::SameLine();
      if (placementButton("Undo","character/undo")) characterHistory(false);
      ImGui::SameLine();
      if (ImGui::Button("Redo")) characterHistory(true);
      ImGui::SameLine();
      ImGui::BeginDisabled(!s.placement.active || s.placement.awaiting || s.placementJob.valid());
      const bool add = ImGui::Button("Add to Map");
      ImGui::EndDisabled();
      if (add) characterAddToMap();
      if (!s.placement.active && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Use Create > Model > Character in the viewport to choose a position.");
      ImGui::SameLine();
      ImGui::TextDisabled("Seed %u%s",s.doc.seed,dirty ? "  |  Unsaved changes" : "");
      ImGui::SetNextItemWidth(180*uiScale());
      const bool pickPreset = ImGui::BeginCombo("Character preset",s.doc.name.c_str(),ImGuiComboFlags_HeightLargest);
      placementRect("character/preset");
      if (pickPreset) {
        for (unsigned i=0; i<std::size(cc::presets); ++i) {
          const auto& preset = cc::presets[i];
          char label[64]{};
          std::snprintf(label,sizeof(label),"%02u  %s",i+1,preset.name);
          const bool selected = ImGui::Selectable(label);
          const auto item = std::string("character/preset/")+preset.name;
          placementRect(item.c_str());
          if (!selected) continue;
          characterCheckpoint(s.doc);
          s.doc = cc::applyPreset(s.doc,preset);
          s.faceTypes = 0;
          s.hairFilter = 0;
          ++s.revision;
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      const bool exportProfile = ImGui::Button("Export for game profile...");
      placementRect("character/export-profile");
      if (exportProfile) characterChooseProfileExport(gameProfile);
      ImGui::SameLine();
      if (ImGui::Checkbox("Dockable panes",&s.separatePanes))
        cvars->SetFloat("ftebroom_character_dock_panes",s.separatePanes);
      placementRect("character/dock-panes");
      const auto room = ImGui::GetContentRegionAvail();
      if (s.separatePanes && room.x >= 480*uiScale() && room.y >= 260*uiScale()) {
        characterDockPanes(ImGui::GetID("Character dockspace"));
        drawPanes = true;
      } else {
        const bool compactTimeline = room.y < 270*uiScale();
        if (!compactTimeline) characterTimeline();
        const float timelineRow = ImGui::GetFrameHeightWithSpacing();
        const float timelineHeight = std::clamp(room.y*.32f,2*timelineRow,4*timelineRow);
        // A compact window gives the preview and inspector their own region.
        // This leaves the timeline scrollable without moving the model.
        const bool showWorkspace = !compactTimeline ||
            ImGui::BeginChild("Character workspace region",{0,-timelineHeight-ImGui::GetStyle().ItemSpacing.y});
        if (showWorkspace && ImGui::BeginTable("Character workspace",2,ImGuiTableFlags_Resizable)) {
          ImGui::TableSetupColumn("Preview",ImGuiTableColumnFlags_WidthStretch);
          ImGui::TableSetupColumn("Properties",ImGuiTableColumnFlags_WidthFixed,
              std::min(320 * uiScale(),ImGui::GetContentRegionAvail().x * .46f));
          ImGui::TableNextColumn();
          if (ImGui::BeginChild("Character preview"))
            rendered = characterDrawPreview();
          ImGui::EndChild();
          ImGui::TableNextColumn();
          if (ImGui::BeginChild("Character settings"))
            characterDrawSettings();
          ImGui::EndChild();
          ImGui::EndTable();
        }
        if (compactTimeline) {
          ImGui::EndChild();
          if (ImGui::BeginChild("Character timeline region"))
            characterTimeline(true);
          ImGui::EndChild();
        }
      }
    } catch (const std::exception& error) {
      s.error = error.what();
      ImGui::TextWrapped("%s",s.error.c_str());
    }
    if (!keep) characterRequestDocumentAction(characterClose);
    characterUnsavedDialog();
  }
  ImGui::End();
  if (drawPanes && keep && s.open) {
    if (ImGui::Begin("Animation Timeline###rb21-timeline",nullptr,ImGuiWindowFlags_AlwaysVerticalScrollbar))
      characterTimeline();
    ImGui::End();
    if (ImGui::Begin("Preview###rb21-preview")) rendered = characterDrawPreview();
    ImGui::End();
    if (ImGui::Begin("Character Properties###rb21-properties")) characterDrawSettings();
    ImGui::End();
  }
  if (!s.open || !rendered) characterReleasePreview();
}

static void QDECL characterCommand() {
  initialize();
  try {
    char path[4096]{};
    commands->Argv(1,path,sizeof path);
    if (*path) characterOpenFile(std::filesystem::u8path(path));
    else characterOpen();
  }
  catch (const std::exception& error) { core->Print((std::string(error.what())+"\n").c_str()); }
}

static void characterModeTick() {
  auto& s = characterEditor;
  if (!characterAvailable()) {
    characterReleasePreview();
    s.placement = {};
    s.pendingDocumentAction = {};
    s.readyDocumentAction = {};
    s.waitingForSavePath = false;
    return;
  }
  if (s.waitingForSavePath && !runUiBrowseOpen) {
    s.waitingForSavePath = false;
    s.pendingDocumentAction = {};
  }
  auto& placement = s.placement;
  characterFinishPlacement();
  if (!placement.active) return;
  if (placement.epoch != organizationMapEpoch || placement.profile != gameProfile ||
      placement.sourceRoot != std::string(baseq3) || placement.runtimeRoot != gameRuntime()) {
    placement = {};
    return;
  }
  if (!placement.awaiting) return;
  char ack[200]{}, status[64]{};
  cvars->GetString("ftebroom_placement_ack",ack,sizeof ack);
  if (placement.request != ack) return;
  cvars->GetString("ftebroom_placement_status",status,sizeof status);
  placement.awaiting = false;
  const std::string_view result(status);
  if (result == "ok") s.status = "Saved the linked character model.";
  else {
    if (result=="invalid-position" || result=="invalid-layer") {
      s.status = result=="invalid-position" ? "The placement point is outside the editor coordinate limits."
                                           : "The target layer is no longer available.";
    } else if (result=="stale") {
      s.status = "The map changed before the character could be placed.";
    } else {
      s.status = "The linked entity or its map placement is no longer available.";
    }
    placement = {};
  }
}

static void characterDocumentActionsTick() {
  auto& s = characterEditor;
  if (!characterAvailable() || !s.readyDocumentAction ||
      s.placementJob.valid() || s.placement.awaiting) return;
  auto action = std::move(s.readyDocumentAction);
  s.readyDocumentAction = {};
  try {
    action();
  } catch (const std::exception& error) {
    s.error = error.what();
    s.open = true;
  }
}

static void characterShutdown() {
  characterReleasePreview();
  if (characterEditor.morphJob.valid()) characterEditor.morphJob.wait();
  if (characterEditor.placementJob.valid()) characterEditor.placementJob.wait();
}
} // namespace
