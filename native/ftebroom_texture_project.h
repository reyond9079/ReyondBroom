// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

static std::filesystem::path studioProjectPath(const std::filesystem::path& root, const std::string& relative) {
  const auto path = root / std::filesystem::u8path(relative);
  rbTexture::require(std::filesystem::weakly_canonical(path) == path, "project",
    "A Texture Studio project file points through a filesystem link.");
  return path;
}

static std::string studioProjectManifestLocation(int profile) {
  return rbTexture::projectPresetStorage(profile) + "manifest.json";
}

static std::string studioProjectManifestBytes(const std::filesystem::path& root, int profile) {
  const auto path = studioProjectPath(root, studioProjectManifestLocation(profile));
  return std::filesystem::exists(path) ? ftebroom::bb::readFile(path, 256 * 1024) : std::string{};
}

static rbTexture::Json studioParseProjectManifest(const std::string& bytes, int profile) {
  if (bytes.empty()) return {{"version", 1}, {"profile", rbTexture::projectProfileId(profile)},
                             {"files", rbTexture::Json::object()}};
  auto manifest = rbTexture::parseJson(bytes);
  rbTexture::require(manifest.value("version", 0) == 1 &&
    manifest.value("profile", "") == rbTexture::projectProfileId(profile) &&
    manifest.at("files").is_object(), "project", "The Texture Studio project manifest is invalid.");
  return manifest;
}

static rbTexture::Json studioProjectManifest(const std::filesystem::path& root, int profile) {
  return studioParseProjectManifest(studioProjectManifestBytes(root, profile), profile);
}

static std::string studioProjectOwnedFile(const std::filesystem::path& path, const rbTexture::Json& checksum) {
  auto bytes = ftebroom::bb::readFile(path);
  if (!checksum.is_string() || ftebroom::sha256(bytes) != checksum.get_ref<const std::string&>())
    throw rbTexture::Error("project", "A Texture Studio project file changed outside the editor: " + path.filename().u8string());
  return bytes;
}

static void studioRefreshProjectScope(bool force = false) {
  namespace fs = std::filesystem;
  auto& s = textureStudio;
  auto root = studioProfileAllowsApply() ? studioDestination() : fs::path{};
  const bool changed = root != s.projectDestination || gameProfile != s.projectProfile;
  if (!force && !changed) return;
  s.projectDestination = root;
  if (!root.empty()) {
    std::error_code error;
    root = fs::canonical(root, error);
    if (error || !fs::is_directory(root, error)) root.clear();
  }
  if (changed && s.projectProgress) s.projectProgress->cancel = true;
  if (changed && s.projectProfile != -1) {
    s.projectPreset.clear();
    if (s.inputCancel) *s.inputCancel = true;
  }
  s.projectRoot = std::move(root);
  s.projectProfile = gameProfile;
  s.projectOffer = true;
  s.projectImported = false;
  if (!s.projectRoot.empty()) {
    const auto bytes = studioProjectManifestBytes(s.projectRoot, gameProfile);
    if (!bytes.empty()) {
      studioParseProjectManifest(bytes, gameProfile);
      s.projectImported = true;
    }
  }
}

static const rbTexture::ProjectPreset* studioProjectPresetByKey(const std::string& key) {
  const auto& presets = rbTexture::projectPresets();
  const auto found = std::find_if(presets.begin(), presets.end(), [&](const auto& preset) { return preset.key == key; });
  return found == presets.end() ? nullptr : &*found;
}

static const rbTexture::ProjectPreset* studioProjectPresetForMaterial(std::string_view material) {
  if (!material.compare(0, 9, "textures/")) material.remove_prefix(9);
  if (material.substr(0, 4) != "rbs_" && material.substr(0, 15) != "reyond-studio/p") return nullptr;
  static const auto catalogs = [] {
    std::array<std::map<std::string, const rbTexture::ProjectPreset*, std::less<>>, 5> result;
    for (int profile = 0; profile < int(result.size()); ++profile)
      for (const auto& preset : rbTexture::projectPresets())
        result[profile].emplace(rbTexture::projectPresetMaterial(profile, preset), &preset);
    return result;
  }();
  const auto& catalog = catalogs.at(gameProfile);
  const auto found = catalog.find(material);
  return found == catalog.end() ? nullptr : found->second;
}

static std::string studioProjectWadReference(const std::filesystem::path& root, int profile) {
  const auto relative = rbTexture::projectPresetWad(profile);
  auto reference = (root / relative).lexically_relative(documentFile.parent_path());
  if (reference.empty()) reference = root / relative;
  const auto text = reference.generic_u8string();
  rbTexture::require(text.find_first_of(";\"\r\n") == std::string::npos, "wad",
    "Choose a map location without WAD separators or quotes in its path.");
  return text;
}

static void studioBeginProjectBake(const rbTexture::ProjectPreset* selected = nullptr) {
  auto& s = textureStudio;
  studioRefreshProjectScope();
  rbTexture::require(!s.projectRoot.empty(), "project", "Choose a writable game or mod folder in Game profile.");
  rbTexture::require(!s.projectJob.valid(), "project", "Finish or cancel the current project operation.");
  const auto profile = s.projectProfile;
  auto bytes = studioProjectManifestBytes(s.projectRoot, profile);
  rbTexture::require(selected ? !bytes.empty() : bytes.empty(), "project",
    selected ? "Import the presets before updating a material." : "The presets are already imported.");
  if (!selected && rbTexture::projectUsesWad(profile) && !documentFile.empty())
    studioProjectWadReference(s.projectRoot, profile);
  std::string palette = profile == 2 ? q1Palette() : profile == 3 ? q2Palette() :
                        profile == 4 ? studioGoldSrcPalette() : std::string{};
  auto progress = std::make_shared<TextureStudioState::ProjectProgress>();
  s.projectProgress = progress;
  std::optional<rbTexture::Document> doc;
  if (selected) doc = textureStudioDoc();
  s.error.clear();
  s.message.clear();
  s.playing = false;
  s.projectJob = std::async(std::launch::async, [root = s.projectRoot, profile, epoch = organizationMapEpoch,
      selected, doc = std::move(doc), existing = std::move(bytes), palette = std::move(palette), progress,
      assets = selected ? s.assets : rbTexture::Assets{}, originals = s.assetFolder, frame = s.frame]() {
    TextureStudioState::ProjectBake result{root, profile, epoch, existing, selected ? selected->key : "", {}};
    auto manifest = studioParseProjectManifest(existing, profile);
    const auto paletteHash = palette.empty() ? std::string{} : ftebroom::sha256(palette);
    rbTexture::require(!selected || manifest.value("palette", "") == paletteHash, "project",
      "The game palette changed since the presets were imported.");
    const auto wad = rbTexture::projectPresetWad(profile);
    const bool usesWad = rbTexture::projectUsesWad(profile);
    rbTexture::ProjectPresetFiles lumps;
    if (selected && usesWad)
      lumps = rbTexture::projectWadLumps(studioProjectOwnedFile(studioProjectPath(root, wad),
        manifest.at("files").at(wad)), profile);
    const auto evaluate = [&](const rbTexture::ProjectPreset& preset, const rbTexture::Document& source,
        const rbTexture::Assets& inputs, unsigned imageFrame) {
      rbTexture::require(!progress->cancel, "project", "Preset generation cancelled.");
      const auto plan = rbTexture::compile(source, {}, inputs);
      const auto threads = std::clamp(std::thread::hardware_concurrency(), 1u, 4u);
      const auto evaluation = rbTexture::evaluate(plan, source.resolution(), &progress->cancel,
        rbTexture::memoryBudget, selected ? threads : 1, imageFrame);
      rbTexture::projectEncodePreset(result.files, lumps, profile, preset, evaluation, palette);
      ++progress->finished;
    };
    if (selected) {
      evaluate(*selected, *doc, assets, frame);
      const auto folder = rbTexture::projectPresetStorage(profile);
      result.files.emplace(folder + selected->key + ".rbtex.json", doc->save());
      for (const auto& reference : doc->source.at("assetReferences")) {
        const auto hash = rbTexture::idField(reference, "sha256", "assets");
        result.files.emplace(folder + rbTexture::imageRelativePath(hash), rbTexture::readOriginal(originals, reference));
      }
      manifest["frames"][selected->key] = frame;
    } else {
      for (const auto& preset : rbTexture::projectPresets()) evaluate(preset, preset.document(), {}, 0);
      if (profile <= 1)
        result.files.emplace("scripts/" + rbTexture::projectPresetShaders(profile) + ".shader",
          rbTexture::projectShaderText(profile));
    }
    if (usesWad) result.files.emplace(wad, ftebroom::quake1::mipWad(lumps, profile == 4));
    size_t totalBytes = 0;
    for (const auto& [path, content] : result.files) {
      totalBytes += content.size();
      rbTexture::require(totalBytes <= 192 * 1024 * 1024, "project", "The preset library exceeds 192 MiB.");
      manifest["files"][path] = ftebroom::sha256(content);
    }
    manifest["palette"] = paletteHash;
    result.files.emplace(studioProjectManifestLocation(profile), manifest.dump(2) + "\n");
    return result;
  });
}

static void studioInstallProjectBake(TextureStudioState::ProjectBake& bake) {
  namespace fs = std::filesystem;
  const auto manifestPath = studioProjectManifestLocation(bake.profile);
  const auto lockPath = studioProjectPath(bake.root, rbTexture::projectPresetStorage(bake.profile) + ".lock");
  fs::create_directories(lockPath.parent_path());
  // The manifest check and installation must be serialized across windows
  // and editor processes. The OS releases the lock if an editor exits.
  struct ProjectLock {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~ProjectLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
#else
    int handle = -1;
    ~ProjectLock() { if (handle >= 0) close(handle); }
#endif
  } lock;
#ifdef _WIN32
  lock.handle = CreateFileW(ftebroom::filesystemPath(lockPath).c_str(), GENERIC_WRITE, 0, nullptr,
    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  rbTexture::require(lock.handle != INVALID_HANDLE_VALUE, "project",
    "Cannot acquire the preset library lock. Check project permissions and other editor windows.");
#else
  lock.handle = open(lockPath.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
  rbTexture::require(lock.handle >= 0 && flock(lock.handle, LOCK_EX | LOCK_NB) == 0, "project",
    "Cannot acquire the preset library lock. Check project permissions and other editor windows.");
#endif
  const auto before = studioProjectManifestBytes(bake.root, bake.profile);
  rbTexture::require(before == bake.manifest, "project", "The project preset library changed during generation.");
  const auto owned = studioParseProjectManifest(before, bake.profile).at("files");
  if (bake.preset.empty() && bake.profile <= 1) {
    const auto name = std::string("scripts/shaderlist.txt");
    const auto path = studioProjectPath(bake.root, name);
    std::string list;
    if (fs::exists(path)) list = ftebroom::bb::readFile(path, 4 * 1024 * 1024);
    else {
      size_t length = 0;
      auto data = static_cast<char*>(files->LoadFile(name.c_str(), &length));
      if (data) {
        if (length <= 4 * 1024 * 1024) list.assign(data, length);
        core->Free(data);
        rbTexture::require(length <= 4 * 1024 * 1024, "project", "The project shader list is too large.");
      }
    }
    bake.files.emplace(name, ftebroom::appendShaderList(list, {rbTexture::projectPresetShaders(bake.profile)}));
  }
  struct Write {
    fs::path path;
    const std::string* bytes;
    std::optional<std::string> previous;
  };
  std::vector<Write> writes;
  for (const auto& [name, bytes] : bake.files) {
    if (name == manifestPath) continue;
    const auto path = studioProjectPath(bake.root, name);
    std::optional<std::string> previous;
    if (fs::exists(path)) {
      if (owned.contains(name)) previous = studioProjectOwnedFile(path, owned.at(name));
      else {
        previous = ftebroom::bb::readFile(path);
        if (*previous != bytes && name != "scripts/shaderlist.txt")
          throw rbTexture::Error("project", "The project already has a different file named " + path.filename().u8string() + ".");
      }
      if (*previous == bytes) continue;
    }
    writes.push_back({path, &bytes, std::move(previous)});
  }
  size_t written = 0;
  try {
    for (const auto& item : writes) {
      fs::create_directories(item.path.parent_path());
      ftebroom::bb::atomicWrite(item.path, *item.bytes);
      ++written;
    }
    const auto manifest = studioProjectPath(bake.root, manifestPath);
    fs::create_directories(manifest.parent_path());
    ftebroom::bb::atomicWrite(manifest, bake.files.at(manifestPath));
  } catch (...) {
    std::string rollbackError;
    while (written) {
      const auto& item = writes[--written];
      try {
        if (item.previous) ftebroom::bb::atomicWrite(item.path, *item.previous);
        else fs::remove(item.path);
      } catch (const std::exception& e) {
        if (rollbackError.empty()) rollbackError = e.what();
      }
    }
    if (!rollbackError.empty())
      throw rbTexture::Error("project", "Preset installation failed and some files could not be restored: " + rollbackError);
    throw;
  }
}

static void studioProjectTick() {
  auto& s = textureStudio;
  if (!s.projectJob.valid() || s.projectJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
  const bool cancelled = s.projectProgress->cancel;
  s.projectProgress.reset();
  try {
    auto bake = s.projectJob.get();
    if (cancelled || !s.open || jamEditing() || bake.root != s.projectRoot || bake.profile != gameProfile) return;
    studioInstallProjectBake(bake);
    s.projectImported = true;
    s.projectOffer = false;
    if (rbTexture::projectUsesWad(bake.profile)) {
      if (bake.preset.empty() && !documentFile.empty() && bake.mapEpoch == organizationMapEpoch)
        documentAttachWad(std::filesystem::u8path(studioProjectWadReference(bake.root, bake.profile)));
      char currentTexture[1024]{};
      cvars->GetString("ca_newbrushtexture", currentTexture, sizeof currentTexture);
      q1IndexAssets();
      if (currentTexture[0]) cvars->SetString("ca_newbrushtexture", currentTexture);
      resetMaterialBrowser();
      publishMaterialTags();
      cvars->SetFloat("ftebroom_material_count", float(materials.size()));
    } else command("fs_restart\nvid_reload\nftebroom_assets_ready\n");
    modFilesRefresh = true;
    if (bake.preset.empty())
      s.message = "Imported " + std::to_string(rbTexture::projectPresets().size()) + " presets into the game project.";
    else
    {
      s.savedSource = textureStudioDoc().save();
      s.message = "Updated the project material. All faces using it will show the new texture.";
    }
  } catch (const std::exception& e) {
    if (!cancelled) s.error = e.what();
  }
}

static void studioApplyProjectPreset(const rbTexture::ProjectPreset& preset) {
  const auto& s = textureStudio;
  const auto profile = gameProfile;
  const auto material = rbTexture::projectPresetMaterial(profile, preset);
  const bool wad = rbTexture::projectUsesWad(profile);
  const auto relative = wad ? rbTexture::projectPresetWad(profile) : rbTexture::projectPresetImagePath(profile, material);
  rbTexture::require(std::filesystem::is_regular_file(studioProjectPath(s.projectRoot, relative)), "project",
    "The installed material is missing from the game project.");
  studioBeginApply(false, "textures/" + material, wad ? studioProjectWadReference(s.projectRoot, profile) : "");
}
