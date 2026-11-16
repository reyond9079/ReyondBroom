// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace {
static bb::Texture characterTexture(unsigned material,const cc::Document& doc,
                                    const std::string& map,const std::string& bytes) {
  bb::Texture texture;
  if (bytes.empty()) {
    texture.width = texture.height = 4;
    texture.rgba.resize(4*4*4,255);
  } else {
    int width = 0, height = 0, channels = 0;
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const auto size = int(bytes.size());
    if (!stbi_info_from_memory(data,size,&width,&height,&channels) ||
        width<1 || height<1 || width>2048 || height>2048)
      throw std::runtime_error("Invalid bundled character texture dimensions.");
    std::unique_ptr<stbi_uc,void(*)(void*)> pixels(
        stbi_load_from_memory(data,size,&width,&height,&channels,4),stbi_image_free);
    if (!pixels) throw std::runtime_error("Cannot decode a bundled character texture.");
    texture.width = width;
    texture.height = height;
    texture.rgba.assign(pixels.get(),pixels.get()+std::size_t(width)*height*4);
  }
  const auto tint = cc::surfaceTint(material,doc,map);
  for (std::size_t i = 0; i < texture.rgba.size(); i += 4)
    for (unsigned channel = 0; channel < 3; ++channel)
      texture.rgba[i+channel] = std::uint8_t(std::lround(std::min(255.,texture.rgba[i+channel]*tint[channel])));
  return texture;
}

static const char* characterExtension(int profile) {
  const char* names[]{".md3",".iqm",".mdl",".md2",".mdl"};
  if (profile < 0 || profile >= int(std::size(names))) throw std::runtime_error("Unknown game profile.");
  return names[profile];
}

static std::string characterJpeg(const bb::Texture& texture) {
  std::string bytes;
  if (!stbi_write_jpg_to_func(+[](void* context,void* data,int length) {
          static_cast<std::string*>(context)->append(static_cast<const char*>(data),length);
        },&bytes,texture.width,texture.height,4,texture.rgba.data(),96))
    throw std::runtime_error("Cannot encode a generated character texture.");
  return bytes;
}

static void characterRuntimeFiles(std::map<std::string,std::string>& bundle, const std::string& model,
                                  const std::string& textureBase, const cc::Humanoid& fitted,
                                  const cc::Document& doc, int profile, const std::string& palette,
                                  const cc::MaterialImages& images, const std::string& surfaceProgram,
                                  const std::string& detailTexture, const std::string& fabricTexture,
                                  const std::string& md2SkinPath = {},const cc::ImportedMotion* motion = nullptr) {
  using namespace ftebroom::modelExport;
  std::array<std::string,cc::materialCount> materials;
  std::array<Skin,cc::materialCount> skins;
  const auto maps = cc::surfaceMaps(doc,profile==1);
  std::string shaderText;
  for (std::size_t i = 0; i < materials.size(); ++i) {
    if (profile!=1 && profile!=4 && (i==cc::browMaterial || i==cc::lashMaterial)) continue;
    if (std::none_of(fitted.triangles.begin(),fitted.triangles.end(),[i](auto& face) {
          return face.material==i;
        })) continue;
    auto texture = characterTexture(unsigned(i),doc,maps[i],images[i]);
    if (profile <= 1) {
      materials[i] = textureBase+"-"+std::to_string(i);
      const bool transparent = cc::cutoutMaterial(unsigned(i));
      const auto image = materials[i]+(profile!=1 ? ".tga" : transparent ? ".png" : ".jpg");
      auto encoded = profile!=1 ? bb::tga(texture) : transparent ? bb::png(texture) : characterJpeg(texture);
      if (encoded.empty()) throw std::runtime_error("Cannot encode a generated character texture.");
      bundle[image] = std::move(encoded);
      if (const auto variant = profile==1 ? cc::surfaceVariant(unsigned(i)) : nullptr) {
        materials[i] += "-pbr";
        shaderText += materials[i]+"\n{\n program reyond_character_surface_v4#"+variant+
                      "\n diffusemap "+image+"\n";
        if (i==0) shaderText += std::string(" normalmap ")+cc::surfaceDetailPath+"\n";
        if (i==3 || i==4 || i==6) shaderText += std::string(" normalmap ")+cc::fabricDetailPath+"\n";
        if (i==5) shaderText += " cull none\n sort seethrough\n";
        shaderText += " {\n map "+image+"\n rgbGen exactVertex\n alphaGen vertex\n";
        if (i==5) shaderText += " blendFunc blend\n depthWrite\n";
        shaderText += " }\n}\n";
      } else if (profile==1 && cc::cutoutMaterial(unsigned(i))) {
        const bool hair = i==5;
        shaderText += materials[i]+"\n{\n cull none\n sort "+(hair ? "seethrough" : "blend")+
                      "\n {\n map "+image+(hair ? "\n blendFunc blend\n alphaFunc GT0\n depthWrite" :
                          "\n blendFunc blend\n alphaFunc GT0")+
                      "\n rgbGen lightingDiffuse\n }\n}\n";
      }
    } else {
      skins[i] = {texture.width,texture.height,std::move(texture.rgba)};
    }
  }
  if (profile == 4) {
    bundle[model] = cc::exportStudio(fitted,doc,&skins,motion);
    return;
  }
  if (profile == 1) {
    if (surfaceProgram.empty() || detailTexture.empty() || fabricTexture.empty())
      throw std::runtime_error("The FTE character surface assets are missing.");
    bundle[cc::surfaceProgramPath] = surfaceProgram;
    bundle[cc::surfaceDetailPath] = detailTexture;
    bundle[cc::fabricDetailPath] = fabricTexture;
    const auto leaf = std::filesystem::u8path(textureBase).filename().u8string();
    bundle["scripts/"+(leaf.rfind("character-",0)==0 ? leaf : "character-"+leaf)+".shader"] =
        std::move(shaderText);
    bundle[model] = cc::exportIqm(fitted,doc,materials,true,true,true,motion);
    return;
  }
  auto legacy = cc::bakeLegacy(fitted,doc,profile,materials,profile>=2 ? &skins : nullptr,motion);
  if (profile == 0) bundle[model] = md3(legacy.mesh);
  else if (profile == 2) bundle[model] = mdl(legacy.mesh,legacy.skins,palette);
  else {
    const auto skin = textureBase+".pcx";
    auto result = md2(legacy.mesh,legacy.skins,palette,md2SkinPath.empty() ? skin : md2SkinPath);
    bundle[model] = std::move(result.model);
    bundle[textureBase+".pcx"] = std::move(result.skin);
  }
  bundle[model+".framegroups"] = cc::engineFrameGroups(legacy.frameGroups).dump(2)+"\n";
}

static std::string characterPalette(int profile) {
  if (profile == 2) return q1Palette();
  if (profile == 3) return q2Palette();
  return {};
}

static void characterExportProfile(const std::filesystem::path& path,int profile) {
  characterRequireOffline();
  if (path.extension() != characterExtension(profile) || std::filesystem::exists(path))
    throw std::runtime_error("Choose a new filename with the selected profile extension.");
  const auto leaf = path.stem().u8string();
  if (leaf.empty() || leaf.size() > 32 ||
      leaf.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != leaf.npos)
    throw std::runtime_error("Use 1–32 ASCII letters, digits, hyphens or underscores for a game model name.");
  const auto& s = characterEditor;
  const auto fitted = cc::assemble(*s.rig,s.doc,[&](const auto& name) -> const auto& {
    return s.wardrobe.at(name);
  },profile==1);
  std::map<std::string,std::string> bundle;
  const auto modelName = path.filename().u8string();
  const auto images = characterExportImages(s.doc,s.materialManifest,profile==1);
  const auto shader = profile==1 ? characterAsset(cc::surfaceProgramPath,64*1024) : std::string{};
  const auto detail = profile==1 ? characterSurfaceDetail(s.materialManifest) : std::string{};
  const auto fabric = profile==1 ? characterSurfaceDetail(s.materialManifest,true) : std::string{};
  const auto textures = profile==1 ? "textures/reyond-plugins/"+leaf : leaf;
  // Quake II resolves the PCX path from the mod root. Keep the files adjacent
  // on disk and preserve a relative filename for exports outside a known mod.
  const auto md2SkinPath = profile==3 ?
      modFileModelPath(path.parent_path()/std::filesystem::u8path(leaf+".pcx")) : std::string{};
  const auto motion=characterImportedMotion(s.doc);
  characterRuntimeFiles(bundle,modelName,textures,fitted,s.doc,profile,characterPalette(profile),
                        images,shader,detail,fabric,md2SkinPath,motion.get());
  auto shared = [](const std::string& name) {
    return name==cc::surfaceProgramPath || name==cc::surfaceDetailPath || name==cc::fabricDetailPath;
  };
  for (const auto& [name,bytes] : bundle)
    if (const auto dest=path.parent_path()/std::filesystem::u8path(name);
        std::filesystem::exists(dest) && (!shared(name) || q1Read(dest)!=bytes))
      throw std::runtime_error("Export would overwrite an existing dependency: "+name);
  std::vector<std::filesystem::path> written;
  try {
    for (bool modelLast : {false,true}) for (const auto& [name,bytes] : bundle) {
      if ((name == modelName) != modelLast) continue;
      const auto dest = path.parent_path()/std::filesystem::u8path(name);
      if (shared(name) && std::filesystem::exists(dest)) continue;
      plantWriteNew(dest,bytes);
      written.push_back(dest);
    }
  } catch (...) {
    for (const auto& file : written) { std::error_code error; std::filesystem::remove(file,error); }
    throw;
  }
}

static void characterPublishPlacement() {
  characterRequireOffline();
  auto& s = characterEditor;
  auto& binding = s.placement;
  placementValid(binding);
  if (s.file.empty() || !binding.active) return;
  if (s.placementJob.valid()) throw std::runtime_error("The previous character export is still running.");
  auto palette = characterPalette(binding.profile);
  auto images = characterExportImages(s.doc,s.materialManifest,binding.profile==1);
  std::map<std::string,std::string> dependencies;
  if (!s.doc.animationAsset.empty())
    dependencies["models/reyond-plugins/rbchar-assets/"+s.doc.animationAsset+".glb"]=s.importedFiles.at(s.doc.animationAsset);
  if (!s.doc.customWardrobe.empty())
    dependencies["models/reyond-plugins/rbchar-assets/"+s.doc.customWardrobe+".rbw"]=s.importedFiles.at(s.doc.customWardrobe);
  const auto motion=characterImportedMotion(s.doc);
  s.placementJob = std::async(std::launch::async,
      [rig=&*s.rig, wardrobe=&s.wardrobe, doc=s.doc, source=s.saved,
       profile=binding.profile, bindingId=binding.id, palette=std::move(palette),
       images=std::move(images), visual=s.materialDigest, motion, dependencies=std::move(dependencies),
       shader=binding.profile==1 ? characterAsset(cc::surfaceProgramPath,64*1024) : std::string{},
       detail=binding.profile==1 ? characterSurfaceDetail(s.materialManifest) : std::string{},
       fabric=binding.profile==1 ? characterSurfaceDetail(s.materialManifest,true) : std::string{}] {
      CharacterEditorState::RuntimeAsset asset;
      const auto identity = ftebroom::jam::sha256(std::string(profile==1 ? "rbchar-runtime-v26\n" : "rbchar-runtime-v20\n") +
                                                  std::to_string(profile) +
                                                  source + palette + visual + shader).substr(0,24);
      const auto id = "character-" + identity;
      const auto model = "models/reyond-plugins/" + id + characterExtension(profile);
      const auto sourcePath = "models/reyond-plugins/" + id + ".rbchar";
      const auto manifest = model + ".reyond.json";
      auto& bundle = asset.files;
      const auto fitted = cc::assemble(*rig,doc,[&](const auto& name) -> const auto& {
        return wardrobe->at(name);
      },profile==1);
      const auto textures = std::string(profile == 3 ? "models/reyond-plugins/" : "textures/reyond-plugins/") + id;
      characterRuntimeFiles(bundle,model,textures,fitted,doc,profile,palette,images,shader,detail,fabric,{},motion.get());
      bundle[sourcePath] = source + "\n";
      bundle.insert(dependencies.begin(),dependencies.end());
      cc::Json deps = cc::Json::array();
      for (const auto& [name,bytes] : bundle) if (name != model) deps.push_back(name);
      cc::Vec3 low{1e6,1e6,1e6}, high{-1e6,-1e6,-1e6};
      for (const auto& vertex : fitted.vertices) {
        const auto point = bb::bbToQuake(vertex.position,0,4*cc::bodyScale(doc));
        for (int axis = 0; axis < 3; ++axis) {
          low[axis] = std::min(low[axis],point[axis]);
          high[axis] = std::max(high[axis],point[axis]);
        }
      }
      bundle[manifest] = cc::Json{{"kind","reyondbroom-authoring-model"},{"version",1},
          {"low",low},{"high",high},{"normalization",1},{"dependencies",deps}}.dump(2);
      asset.binding = bindingId;
      asset.model = model;
      asset.source = sourcePath;
      asset.low = low;
      asset.high = high;
      return asset;
  });
  s.status = "Source saved. Preparing the game model.";
}

static void characterFinishPlacement() {
  auto& s = characterEditor;
  if (!s.placementJob.valid() ||
      s.placementJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
  try {
    auto asset = s.placementJob.get();
    auto& binding = s.placement;
    if (!binding.active || binding.id != asset.binding) return;
    placementValid(binding);
    placementPublish(asset.files);
    placementSend(binding,asset.model,asset.source,asset.low,asset.high,1,false,plants::targetClass(binding.profile));
    s.status = "Game model exported. Applying it to the map.";
  } catch (const std::exception& error) {
    s.status = std::string("Character placement failed: ") + error.what();
  }
}
} // namespace
