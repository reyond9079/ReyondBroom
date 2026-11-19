// SPDX-License-Identifier: GPL-3.0-or-later
#include "legacy_codec.h"
#include "catalog.h"
#include "../ftebroom_pak.h"
#include "../ftebroom_sha256.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "jpeg.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "package.h"
#undef STB_IMAGE_IMPLEMENTATION

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>
#ifdef _WIN32
#include <chrono>
#include <shellapi.h>
#include <thread>
#endif

namespace ts = ftebroom::textureStudio;
namespace fs = std::filesystem;

std::string read(const fs::path& path, size_t limit = ts::maxSourceBytes) {
  if (!fs::is_regular_file(path) || fs::file_size(path) > limit)
    throw ts::Error("input", "A required file is missing or too large.");
  std::ifstream in(path, std::ios::binary);
  std::string data;
  std::array<char, 16384> block{};
  while (in.read(block.data(), block.size()) || in.gcount() > 0) {
    ts::require(size_t(in.gcount()) <= limit - data.size(), "input", "File exceeded its byte limit while being read.");
    data.append(block.data(), in.gcount());
  }
  ts::require(in.eof(), "input", "Cannot read the complete file.");
  return data;
}

void writeNew(const fs::path& path, const std::string& bytes) {
  ts::require(!fs::exists(fs::symlink_status(path)), "output", "Choose a new output path.");
  std::ofstream out(path, std::ios::binary);
  out.write(bytes.data(), bytes.size());
  out.close();
  ts::require(bool(out), "output", "The output file could not be written.");
}

fs::path path(const std::string& value) { return fs::u8path(value); }

class OutputFolder {
  fs::path target, staging;
  std::set<fs::path> directories;
  std::vector<fs::path> written;
  bool committed = false;

public:
  explicit OutputFolder(const fs::path& destination) {
    ts::require(destination.has_filename() && destination.filename() != "." && destination.filename() != "..",
      "output", "Choose a new output directory.");
    const auto base = fs::canonical(destination.parent_path().empty() ? "." : destination.parent_path());
    target = base / destination.filename().make_preferred();
    ts::require(!fs::exists(fs::symlink_status(target)), "output", "Choose a new output directory.");
    staging = target;
    staging += ".incoming";
    ts::require(fs::create_directory(staging), "output", "A staging directory with this name already exists.");
  }
  OutputFolder(const OutputFolder&) = delete;
  OutputFolder& operator=(const OutputFolder&) = delete;
  ~OutputFolder() {
    if (committed) return;
    std::error_code error;
    if (fs::weakly_canonical(staging, error) != staging || error) return;
    for (const auto& file : written)
      if (fs::weakly_canonical(file.parent_path(), error) == file.parent_path() && !error) fs::remove(file, error);
    for (auto dir = directories.rbegin(); dir != directories.rend(); ++dir)
      if (fs::weakly_canonical(dir->parent_path(), error) == dir->parent_path() && !error) fs::remove(*dir, error);
    fs::remove(staging, error);
  }
  void write(const std::string& name, const std::string& bytes) {
    const auto relative = path(name).make_preferred();
    ts::require(fs::weakly_canonical(staging) == staging, "output", "The staging directory changed during export.");
    auto directory = staging;
    for (const auto& segment : relative.parent_path()) {
      directory /= segment;
      if (fs::create_directory(directory)) directories.insert(directory);
      else ts::require(directories.count(directory) != 0 &&
        !fs::is_symlink(fs::symlink_status(directory)), "output", "The export directory changed during the write.");
    }
    const auto filename = staging / relative;
    ts::require(!fs::exists(fs::symlink_status(filename)), "output", "An export asset already exists in staging.");
    written.push_back(filename);
    writeNew(filename, bytes);
  }
  void commit() {
    ts::require(!fs::exists(fs::symlink_status(target)), "output", "The destination appeared during the export.");
#ifdef _WIN32
    for (unsigned attempt = 0;; ++attempt) {
      ts::require(fs::weakly_canonical(staging) == staging, "output", "The staging directory changed during export.");
      if (MoveFileExW(staging.c_str(), target.c_str(), 0)) break;
      const auto error = GetLastError();
      const bool canRetry = error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION || error == ERROR_ACCESS_DENIED;
      if (!canRetry || attempt == 8 || fs::exists(fs::symlink_status(target)))
        throw fs::filesystem_error("Cannot publish the Texture Studio output", staging, target,
          std::error_code(int(error), std::system_category()));
      std::this_thread::sleep_for(std::chrono::milliseconds(25 * (attempt + 1)));
    }
#else
    fs::rename(staging, target);
#endif
    committed = true;
  }
};

struct Options {
  std::string command, source, out, profile, palette, license, preset, texture, name = "studio_brick";
  bool masked = false, pk3 = false, dryRun = false, allFrames = false, acceptRate = false;
  unsigned threads = 1, jpegQuality = 0, frame = 0;
  unsigned variation = 0;
  unsigned exportFrames = 0;
  uint32_t seed = 24058;
  bool seedProvided = false;
  std::string familyInput;
  std::optional<uint32_t> familyShade, familyAccent, familyScale;
  std::optional<int> familyWear;
};

Options options(int argc, char** argv) {
  ts::require(argc >= 3, "command",
    "Use new, family, validate, render, export, bundle, unpack, or import IMAGE --license ID --out DIR.");
  Options out{argv[1], argv[2]};
  std::set<std::string> seen;
  for (int arg = 3; arg < argc; ++arg) {
    const std::string key(argv[arg]);
    ts::require(seen.insert(key).second, "command", "Duplicate command option.");
    if (key == "--masked") { out.masked = true; continue; }
    if (key == "--pk3") { out.pk3 = true; continue; }
    if (key == "--dry-run") { out.dryRun = true; continue; }
    if (key == "--all-frames") { out.allFrames = true; continue; }
    if (key == "--accept-rate-change") { out.acceptRate = true; continue; }
    ts::require(arg + 1 < argc, "command", "An option is missing its value.");
    const std::string value(argv[++arg]);
    if (key == "--out") out.out = value;
    else if (key == "--profile") out.profile = value;
    else if (key == "--from-family" || key == "--family-shade" || key == "--family-accent" ||
             key == "--family-scale" || key == "--family-wear") {
      ts::require(out.command == "family", "family", "Use family settings with the family command.");
      if (key == "--from-family") {
        ts::require(!value.empty(), "family", "Choose an existing family directory.");
        out.familyInput = value;
      } else {
        const bool color = key == "--family-shade" || key == "--family-accent";
        int setting = 0;
        const auto end = value.data() + value.size();
        const auto parsed = std::from_chars(value.data(), end, setting, color ? 16 : 10);
        ts::require(parsed.ec == std::errc{} && parsed.ptr == end, "family", "Expected a valid family setting.");
        if (color) {
          ts::require(value.size() == 6 && setting >= 0 && setting <= 0xffffff,
            "family", "Use a six-digit RGB hex color.");
          (key == "--family-shade" ? out.familyShade : out.familyAccent) = uint32_t(setting);
        } else if (key == "--family-scale") {
          ts::require(setting >= 25 && setting <= 200, "family", "Choose a pattern frequency from 25 to 200 percent.");
          out.familyScale = unsigned(setting);
        } else {
          ts::require(setting >= -50 && setting <= 50, "family", "Choose a wear adjustment from -50 to 50 percent.");
          out.familyWear = setting;
        }
      }
    }
    else if (key == "--palette") out.palette = value;
    else if (key == "--license") out.license = value;
    else if (key == "--preset") out.preset = value;
    else if (key == "--texture") {
      ts::require(out.command == "import" && value.size() <= 15, "texture", "Choose a WAD texture of at most 15 bytes.");
      out.texture = value;
      for (char& c : out.texture) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    else if (key == "--name") out.name = value;
    else if (key == "--threads" || key == "--jpeg" || key == "--frame" || key == "--seed" ||
             key == "--variation" || key == "--frames") {
      uint32_t number = 0;
      const auto end = value.data() + value.size();
      const auto parsed = std::from_chars(value.data(), end, number);
      ts::require(parsed.ec == std::errc{} && parsed.ptr == end, "command", "Expected an unsigned whole number.");
      if (key == "--frame") out.frame = number;
      else if (key == "--frames") {
        ts::require(number >= 2 && number <= 64, "frames", "Choose 2 to 64 frames.");
        out.exportFrames = number;
      }
      else if (key == "--threads") {
        ts::require(number > 0 && number <= ts::maxThreads, "threads", "Choose 1 to 16 CPU workers.");
        out.threads = number;
      } else if (key == "--jpeg") {
        ts::require(number > 0 && number <= 100, "jpeg", "Choose a JPEG quality from 1 to 100.");
        out.jpegQuality = number;
      } else if (key == "--seed") {
        ts::require(out.command == "new" || out.command == "family", "seed", "Use --seed with new or family.");
        out.seed = number;
        out.seedProvided = true;
      } else {
        ts::require(out.command == "new" && number > 0 && number <= ts::maxCatalogVariations,
          "variation", "Use --variation 1 to 4 with a catalog preset.");
        out.variation = number;
      }
    }
    else if (key == "--backend") ts::require(value == "cpu", "backend", "This build supports the CPU reference backend.");
    else throw ts::Error("command", "Unknown command option.");
  }
  return out;
}

void generateFamily(const Options& args) {
  ts::require(args.dryRun || !args.out.empty(), "output", "Choose a destination directory for the family.");
  const auto entry = std::find_if(std::begin(ts::textureFamilies), std::end(ts::textureFamilies), [&](const auto& family) {
    return args.source == family.id || args.source == family.name;
  });
  ts::require(entry != std::end(ts::textureFamilies), "family", "Choose the name or ID of a texture family.");
  const auto index = size_t(entry - std::begin(ts::textureFamilies));
  const auto existing = args.familyInput.empty() ? fs::path{} : fs::canonical(path(args.familyInput));
  const auto openMember = [&](size_t role) {
    const auto name = std::string(ts::familyRoles[role]) + ".rbtex.json";
    const auto source = existing / fs::u8path(name);
    ts::require(fs::canonical(source) == source, "family", "A family source points through a link.");
    auto doc = ts::open(read(source));
    const auto identity = ts::familyIdentity(doc);
    ts::require(!doc.readOnly && identity == std::make_pair(index, role), "family", "A source has the wrong family or role.");
    return doc;
  };
  auto controls = existing.empty() ? ts::defaultFamilyControls(index, args.seed) :
    ts::readFamilyControls(openMember(0), index);
  if (args.seedProvided) controls.seed = args.seed;
  if (args.familyShade) controls.shade = *args.familyShade;
  if (args.familyAccent) controls.accent = *args.familyAccent;
  if (args.familyScale) controls.scale = *args.familyScale;
  if (args.familyWear) controls.wear = float(*args.familyWear) / 100.f;
  ts::Json manifest{{"schemaVersion", 1}, {"family", entry->id}, {"name", entry->name}, {"seed", controls.seed},
    {"shared", {{"shade", controls.shade}, {"accent", controls.accent}, {"detail", entry->settings.detail},
      {"bump", entry->settings.bump}, {"roughness", entry->settings.roughness},
      {"scalePercent", controls.scale}, {"wear", controls.wear}}},
    {"materials", ts::Json::array()}};
  std::unique_ptr<OutputFolder> output;
  if (!args.dryRun) output = std::make_unique<OutputFolder>(path(args.out));
  std::set<std::string> copied;
  for (size_t role = 0; role < std::size(ts::familyRoles); ++role) {
    auto doc = existing.empty() ? ts::familyMaterial(index, role, controls) : openMember(role);
    if (!existing.empty()) {
      ts::refreshFamilyMember(doc, controls);
      const auto assets = ts::loadAssets(doc, existing);
      ts::compile(doc, {}, assets);
      if (output) for (const auto& reference : doc.source.at("assetReferences")) {
        const auto name = ts::imageRelativePath(ts::idField(reference, "sha256", "assets"));
        if (copied.insert(name).second) output->write(name, ts::readOriginal(existing, reference));
      }
    }
    const auto filename = std::string(ts::familyRoles[role]) + ".rbtex.json";
    const auto bytes = doc.save();
    if (output) output->write(filename, bytes);
    const auto& info = doc.source.at("provenance");
    manifest["materials"].push_back({{"role", ts::familyRoles[role]}, {"source", filename}, {"seed", doc.seed()},
      {"preset", info.at("preset")}, {"sha256", ftebroom::sha256(bytes)}});
  }
  if (output) {
    output->write("family.json", manifest.dump(2) + "\n");
    output->commit();
  }
  std::cout << ts::Json{{"state", args.dryRun ? "planned" : "written"}, {"manifest", manifest}}.dump() << '\n';
}

std::string texturePath(const std::string& name, const std::string& extension) {
  ts::require(ts::indexedName(name, true, false), "name",
    "Use a lowercase texture name with optional directory segments.");
  return "textures/" + name + extension;
}

void exportSequence(const Options& args, const ts::Document& doc, const ts::Plan& graph, const ts::Assets& assets) {
  ts::require(!args.frame && !args.masked && !args.jpegQuality && graph.loop && graph.frames >= 2,
    "animation", "Use an opaque, looping source with at least two frames for an animated game material.");
  const bool q1 = args.profile == "q1-baseline", q2 = args.profile == "q2-baseline";
  const bool hl = args.profile == "goldsrc-baseline";
  const bool shaderSequence = args.profile == "q3-baseline" || args.profile == "fte-diffuse";
  ts::require(q1 || q2 || shaderSequence || hl, "animation",
    "Choose an animation profile for Quake 1, Quake II, Quake III, GoldSrc or FTE diffuse.");
  const unsigned limit = shaderSequence ? 8 : q2 ? 64 : 10;
  const unsigned frames = args.exportFrames ? args.exportFrames : graph.frames;
  if (frames > limit || frames > graph.frames)
    throw ts::Error("frames",
      "Use --frames to select at most " + std::to_string(std::min(limit, graph.frames)) + " frames for this source and profile.");
  const unsigned sourceRate = doc.source.at("animation").at("fps").get<unsigned>();
  const double expected = double(sourceRate) * frames / graph.frames;
  const double rate = shaderSequence ? expected : q1 ? 5 : q2 ? 2 : 10;
  ts::require(args.acceptRate || rate == expected, "animation",
    "The target changes the loop duration. Choose a matching frame count or explicitly accept with --accept-rate-change.");
  const auto palette = shaderSequence ? std::string{} : read(path(args.palette), 768);
  const auto size = doc.resolution();
  ts::require(!shaderSequence || (size.width <= 2048 && size.height <= 2048), "resolution",
    "Choose an animated shader texture no larger than 2048 × 2048.");
  ts::require(q1 || hl || ts::indexedName(args.name, true, false), "name", "Choose a safe namespaced texture name.");
  const auto suffix = [&](unsigned frame) {
    if (q1 || hl) return "+" + std::to_string(frame) + args.name;
    return args.name + "_" + std::to_string(frame);
  };
  const auto textureName = shaderSequence ? args.name : suffix(0);
  ts::Json manifest{{"schemaVersion", 1}, {"generatorVersion", ts::generatorVersion}, {"profile", args.profile},
    {"sourceSha256", ftebroom::sha256(doc.save())}, {"paletteSha256", palette.empty() ? "" : ftebroom::sha256(palette)},
    {"primaryTexture", textureName}, {"seed", doc.seed()}, {"sourceFrames", graph.frames},
    {"frames", frames}, {"fps", rate}, {"sourceFps", sourceRate}, {"rateChanged", rate != expected},
    {"frameMap", ts::Json::array()}};
  std::map<std::string, std::string> files;
  std::vector<std::string> wads;
  std::string stages;
  ts::NodeCache cache(32 * 1024 * 1024);
  const auto plan = ts::compile(doc, {"baseColor"}, assets);
  for (unsigned frame = 0; frame < frames; ++frame) {
    const unsigned sourceFrame = frame * graph.frames / frames;
    manifest["frameMap"].push_back(sourceFrame);
    const auto result = ts::evaluate(plan, size, nullptr, ts::memoryBudget, args.threads, sourceFrame, &cache);
    const auto& color = *result.outputs.at("baseColor");
    const auto name = suffix(frame);
    if (shaderSequence) {
      const auto relative = texturePath(name, ".tga");
      ts::require(std::all_of(color.pixels.begin(), color.pixels.end(), [](const auto& p) { return p[3] == 1; }),
        "alpha", "This animation profile requires opaque Base Color.");
      stages += " " + relative;
      const auto origin = args.profile == "q3-baseline" ? ts::TgaOrigin::lowerLeft : ts::TgaOrigin::upperLeft;
      files.emplace(relative, ts::tga(color, origin));
    } else if (q2) {
      files.emplace(texturePath(name, ".wal"), ts::indexed(color, name, palette, ts::IndexedFormat::wal,
        false, suffix((frame + 1) % frames)));
    } else {
      wads.push_back(ts::indexed(color, name, palette, q1 ? ts::IndexedFormat::wad2 : ts::IndexedFormat::wad3));
    }
  }
  if (!wads.empty())
    files.emplace("texture-studio.wad", ts::combineAnimationWads(wads, q1 ? ts::IndexedFormat::wad2 : ts::IndexedFormat::wad3));
  if (shaderSequence) {
    const auto shader = "textures/" + args.name;
    const auto basename = "rbtex_" + ftebroom::sha256(shader).substr(0,12);
    std::ostringstream rateText;
    rateText.imbue(std::locale::classic());
    rateText << rate;
    files.emplace("scripts/" + basename + ".shader", shader +
      "\n{\n {\n  map $lightmap\n  rgbGen identity\n }\n {\n  animMap " + rateText.str() + stages +
      "\n  blendFunc GL_DST_COLOR GL_ZERO\n  rgbGen identity\n }\n}\n");
    manifest["shaderListEntry"] = basename;
  }
  if (args.pk3) {
    std::ostringstream archive(std::ios::binary | std::ios::out);
    std::map<std::string, std::string_view> items;
    for (const auto& item : files) items.emplace(item.first, item.second);
    ftebroom::pak::zipStore(archive, items);
    files = {{"texture-studio.pk3", archive.str()}};
  }
  files.emplace("source.rbtex.json", doc.save());
  for (const auto& asset : doc.source.at("assetReferences")) {
    const auto name = ts::imageRelativePath(ts::idField(asset, "sha256", "assets"));
    files.emplace(name, ts::readOriginal(path(args.source).parent_path(), asset));
  }
  size_t outputBytes = 0;
  auto& artifacts = manifest["artifacts"] = ts::Json::array();
  for (const auto& [name, bytes] : files) {
    ts::require(bytes.size() <= ts::packageByteLimit - outputBytes, "output", "The package exceeds 1 GiB.");
    outputBytes += bytes.size();
    artifacts.push_back({{"path", name}, {"bytes", bytes.size()}, {"sha256", ftebroom::sha256(bytes)}});
  }
  if (!args.dryRun) {
    OutputFolder output(path(args.out));
    for (const auto& [name, bytes] : files) output.write(name, bytes);
    output.write("manifest.json", manifest.dump(2) + "\n");
    output.commit();
  }
  std::cout << ts::Json{{"state", args.dryRun ? "planned" : "written"}, {"manifest", manifest}}.dump() << '\n';
}

void renderSequence(const Options& args, const ts::Document& doc, const ts::Plan& plan) {
  ts::require(args.command == "render" && args.frame == 0, "frame", "Use --all-frames with render and without --frame.");
  std::unique_ptr<OutputFolder> folder;
  if (!args.dryRun) folder = std::make_unique<OutputFolder>(path(args.out));
  auto manifest = ts::Json{{"schemaVersion", 1}, {"profile", "neutral-png-animation"},
    {"generatorVersion", ts::generatorVersion}, {"frames", plan.frames}, {"seed", doc.seed()},
    {"animation", doc.source.at("animation")}, {"artifacts", ts::Json::array()}};
  uint64_t outputBytes = 0;
  const auto store = [&](const std::string& name, const std::string& bytes) {
    ts::require(bytes.size() <= ts::packageByteLimit - outputBytes, "frame", "The animation export exceeds 1 GiB.");
    outputBytes += bytes.size();
    manifest["artifacts"].push_back({{"path", name}, {"bytes", bytes.size()}, {"sha256", ftebroom::sha256(bytes)}});
    if (folder) folder->write(name, bytes);
  };
  ts::NodeCache frameCache(32 * 1024 * 1024);
  for (unsigned frame = 0; frame < plan.frames; ++frame) {
    const auto sample = ts::evaluate(plan, doc.resolution(), nullptr, ts::memoryBudget, args.threads, frame, &frameCache);
    const std::string prefix = "frames/" + std::to_string(1000 + frame).substr(1) + "/";
    for (const auto& [channel, image] : sample.outputs)
      store(prefix + channel + ".png", ts::png(*image, channel == "height" ? 16 : 8));
  }
  store("source.rbtex.json", doc.save());
  for (const auto& asset : doc.source.at("assetReferences")) {
    const auto name = ts::imageRelativePath(ts::idField(asset, "sha256", "assets"));
    store(name, ts::readOriginal(path(args.source).parent_path(), asset));
  }
  if (folder) {
    folder->write("manifest.json", manifest.dump(2) + "\n");
    folder->commit();
  }
  std::cout << ts::Json{{"state", args.dryRun ? "planned" : "written"}, {"manifest", manifest}}.dump() << '\n';
}

int main(int argc, char** argv) try {
#ifdef _WIN32
  std::unique_ptr<wchar_t*, decltype(&LocalFree)> wide(CommandLineToArgvW(GetCommandLineW(), &argc), &LocalFree);
  ts::require(bool(wide), "command", "Cannot read the Windows command line.");
  std::vector<std::string> utf8;
  utf8.reserve(size_t(argc));
  for (int index = 0; index < argc; ++index) utf8.push_back(fs::path(wide.get()[index]).u8string());
  std::vector<char*> arguments;
  arguments.reserve(utf8.size());
  for (auto& value : utf8) arguments.push_back(value.data());
  argv = arguments.data();
#endif
  const auto args = options(argc, argv);
  if (args.command == "family") {
    generateFamily(args);
    return 0;
  }
  if (args.command == "new") {
    auto preset = [&] {
      if (args.preset.empty()) return ts::builtinPreset(0, "cli-preset-0");
      const auto featured = std::find(ts::presetNames.begin(), ts::presetNames.end(), args.preset);
      if (featured != ts::presetNames.end()) {
        const auto index = unsigned(featured - ts::presetNames.begin());
        return ts::builtinPreset(index, "cli-preset-" + std::to_string(index));
      }
      for (size_t category = 0; category < std::size(ts::catalogRecipes); ++category)
        for (size_t index = 0; index < std::size(ts::catalogRecipes[category]); ++index) {
          const std::string id = ts::recipeId(category, index);
          if (args.preset == id || args.preset == ts::catalogRecipes[category][index].name) {
            auto doc = ts::catalogPreset(category, index, "cli-" + id);
            if (args.variation) ts::applyCatalogVariation(doc, category, index, args.variation);
            return doc;
          }
        }
      throw ts::Error("preset", "Choose the name or ID of a built-in preset.");
    }();
    ts::require(!args.variation || preset.source["provenance"].contains("catalogVersion"),
      "variation", "Choose a catalog recipe for a curated variation.");
    preset.source["rootSeed"] = args.seed;
    ts::compile(preset);
    writeNew(path(args.source), preset.save());
    std::cout << "{\"state\":\"created\"}\n";
    return 0;
  }
  const bool importing = args.command == "import";
  const bool extracting = args.command == "unpack";
  ts::Assets assets;
  std::map<std::string, std::string> files;
  auto doc = [&] {
    if (extracting) {
      auto package = ts::unpack(path(args.source));
      files = std::move(package.originals);
      assets = std::move(package.assets);
      return std::move(package.document);
    }
    if (!importing) {
      auto existing = ts::open(read(path(args.source)));
      if (!existing.readOnly) assets = ts::loadAssets(existing, path(args.source).parent_path());
      return existing;
    }
    ts::require(ts::licenseId(args.license), "license", "Supply the original image's license with --license.");
    const auto bytes = ts::imageFileBytes(path(args.source));
    const auto digest = ftebroom::sha256(bytes);
    const auto name = ts::imageRelativePath(digest);
    const auto encoding = ts::sourceEncoding(bytes, path(args.source));
    ts::Json reference{{"id", "original"}, {"path", name}, {"sha256", digest}, {"license", args.license}};
    if (encoding != "image") {
      reference["encoding"] = encoding;
      if (encoding == "wal" || encoding == "wad2") {
        ts::require(!args.palette.empty(), "assets.palette", "Use --palette with the source game's 768-byte palette.");
        reference["palette"] = ts::writePalette(read(path(args.palette), 768));
      }
      if (!args.texture.empty()) reference["texture"] = args.texture;
    }
    auto raster = ts::decodeOriginal(bytes, reference);
    const auto imageSize = raster->size;
    files.emplace(name, bytes);
    assets.emplace("original", raster);
    reference["width"] = imageSize.width;
    reference["height"] = imageSize.height;
    return ts::importedImage("image-" + digest.substr(0, 32), *raster, std::move(reference));
  }();
  const auto graph = ts::compile(doc, {}, assets);
  if (args.command == "bundle") {
    ts::require(!args.out.empty() && !args.dryRun, "output", "Choose a new .rbtexpkg file with --out.");
    writeNew(path(args.out), ts::pack(doc, path(args.source).parent_path()));
    std::cout << "{\"state\":\"bundled\"}\n";
    return 0;
  }
  if (args.command == "validate") {
    std::cout << ts::Json{{"state", "valid"}, {"evaluatedNodes", graph.order.size()},
      {"channels", graph.outputs.size()}, {"validation", "source graph"}}.dump() << '\n';
    return 0;
  }
  ts::require(importing || extracting || args.command == "render" || args.command == "export", "command", "Unknown action.");
  ts::require(args.dryRun || !args.out.empty(), "output", "Choose an output directory.");
  ts::require(!args.pk3 || (args.command == "export" &&
    (args.profile == "q3-baseline" || (args.allFrames && args.profile == "fte-diffuse"))),
    "profile", "Choose Quake III or an animated FTE diffuse material for PK3 export.");
  ts::require(!args.jpegQuality || (args.command == "export" && args.profile == "q3-baseline"),
    "profile", "JPEG export requires the Quake III profile.");
  ts::require(!args.masked || (args.command == "export" && args.profile == "goldsrc-baseline"),
    "profile", "Masked indexed textures require the GoldSrc profile.");
  ts::require((!args.exportFrames && !args.acceptRate) || (args.command == "export" && args.allFrames),
    "animation", "Use --frames and --accept-rate-change with export --all-frames.");
  if (args.allFrames) {
    if (args.command == "export") exportSequence(args, doc, graph, assets);
    else renderSequence(args, doc, graph);
    return 0;
  }
  std::string paletteHash;
  if (!importing && !extracting) {
    const auto result = ts::evaluate(graph, doc.resolution(), nullptr, ts::memoryBudget, args.threads, args.frame);
    if (args.command == "render") {
      for (const auto& [channel, image] : result.outputs)
        files.emplace(channel + ".png", ts::png(*image, channel == "height" ? 16 : 8));
    } else {
      const auto& base = *result.outputs.at("baseColor");
      if (args.profile == "fte-rich") {
        texturePath(args.name, ".tga");
        files = ts::fteWallMaterial(result, args.name);
      } else if (args.profile == "q3-baseline" || args.profile == "fte-diffuse") {
        const auto name = texturePath(args.name, args.jpegQuality ? ".jpg" : ".tga");
        const auto origin = args.profile == "q3-baseline" ? ts::TgaOrigin::lowerLeft : ts::TgaOrigin::upperLeft;
        auto bytes = args.jpegQuality ? ts::jpeg(base, args.jpegQuality) : ts::tga(base, origin);
        if (args.pk3) {
          std::ostringstream archive(std::ios::binary | std::ios::out);
          ftebroom::pak::zipStore(archive, {{name, bytes}});
          files.emplace("texture-studio.pk3", archive.str());
        } else files.emplace(name, std::move(bytes));
      } else {
        ts::IndexedFormat type;
        if (args.profile == "q1-baseline") type = ts::IndexedFormat::wad2;
        else if (args.profile == "q2-baseline") type = ts::IndexedFormat::wal;
        else if (args.profile == "goldsrc-baseline") type = ts::IndexedFormat::wad3;
        else throw ts::Error("profile", "Choose q1-baseline, q2-baseline, q3-baseline, goldsrc-baseline, fte-diffuse or fte-rich.");
        const auto palette = read(path(args.palette), 768);
        paletteHash = ftebroom::sha256(palette);
        files.emplace(type == ts::IndexedFormat::wal ? texturePath(args.name, ".wal") : "texture-studio.wad",
          ts::indexed(base, args.name, palette, type, args.masked));
      }
    }
    for (const auto& asset : doc.source.at("assetReferences")) {
      const auto digest = ts::idField(asset, "sha256", "assets");
      const auto name = ts::imageRelativePath(digest);
      files.emplace(name, ts::readOriginal(path(args.source).parent_path(), asset));
    }
  }

  const auto source = doc.save();
  auto names = ts::Json::array(), generated = ts::Json::array();
  size_t outputBytes = 0;
  for (const auto& [name, bytes] : files) {
    names.push_back(name);
    generated.push_back({{"path", name}, {"sha256", ftebroom::sha256(bytes)}, {"bytes", bytes.size()}});
    ts::require(bytes.size() <= ts::memoryBudget - outputBytes, "output", "Generated files exceed the output budget.");
    outputBytes += bytes.size();
  }
  const auto size = doc.resolution();
  const auto fingerprint = (args.command == "render" ? "neutral-png" : args.profile) + std::string("/1");
  ts::Json manifest{{"schemaVersion", 1}, {"generatorVersion", ts::generatorVersion},
    {"profile", args.profile}, {"profileHash", ftebroom::sha256(fingerprint)}, {"seed", doc.seed()},
    {"frame", graph.loop ? args.frame % graph.frames : args.frame}, {"frameCount", graph.frames},
    {"width", size.width}, {"height", size.height}, {"files", names}, {"artifacts", generated},
    {"sourceSha256", ftebroom::sha256(source)},
    {"provenance", {{"kind", "texture-studio-source"}, {"assetCount", doc.source.at("assetReferences").size()}}},
    {"paletteSha256", paletteHash}, {"jpegQuality", args.jpegQuality}, {"masked", args.masked}};
  if (args.dryRun) {
    std::cout << ts::Json{{"state", "planned"}, {"manifest", manifest}}.dump() << '\n';
    return 0;
  }

  OutputFolder folder(path(args.out));
  for (const auto& [name, bytes] : files) folder.write(name, bytes);
  folder.write("source.rbtex.json", source);
  folder.write("manifest.json", manifest.dump(2) + "\n");
  folder.commit();
  std::cout << ts::Json{{"state", "written"}, {"files", names}}.dump() << '\n';
  return 0;
} catch (const std::exception& e) {
  std::cout << ts::Json{{"state", "error"}, {"message", e.what()}}.dump(-1, ' ', true,
    ts::Json::error_handler_t::replace) << '\n';
  std::cerr << e.what() << '\n';
  return 1;
}
