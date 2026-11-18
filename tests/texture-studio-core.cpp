// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/texture/legacy_codec.h"
#include "../native/texture/catalog.h"
#include "../native/texture/layers.h"
#include "../native/texture/preview.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace ts = ftebroom::textureStudio;
namespace fs = std::filesystem;

void check(bool ok, const char* label) {
  if (!ok) throw std::runtime_error(label);
}

template<class F> void reject(F action, std::string_view expected) {
  try {
    action();
  } catch (const ts::Error& e) {
    check(std::string_view(e.what()).find(expected) != std::string_view::npos, e.what());
    return;
  }
  throw std::runtime_error("Expected a validation error.");
}

void write(const fs::path& file, const std::string& value) {
  std::ofstream out(file, std::ios::binary);
  out.write(value.data(), value.size());
  check(bool(out), "Cannot write generated fixture.");
}

void checkCatalog(const fs::path& previews) {
  using namespace ts;
  const bool thorough = !previews.empty();
  if (thorough) fs::create_directories(previews);
  std::set<std::string> recipeIds, recipeShapes, images;
  unsigned animations = 0;
  for (size_t category = 0; category < std::size(catalogRecipes); ++category) {
    for (size_t index = 0; index < std::size(catalogRecipes[category]); ++index) {
      const auto id = recipeId(category, index);
      auto doc = catalogPreset(category, index, id);
      check(recipeIds.emplace(id).second, "Duplicate recipe ID");
      auto graph = doc.source["graph"];
      for (auto& item : graph["nodes"]) {
        item.erase("position");
        const auto type = item["type"];
        if (type == "ColorRamp") item.erase("parameters");
      }
      if (!recipeShapes.emplace(graph.dump()).second)
        throw std::runtime_error(id + " duplicates a recipe shape.");
      for (uint32_t seed : {0u, 1u, 42u, 24058u, UINT32_MAX}) {
        doc.source["rootSeed"] = seed;
        const auto plan = compile(doc);
        const std::vector<unsigned> sizes = !thorough ? std::vector<unsigned>{32} :
          seed == 24058 ? std::vector<unsigned>{64, 128, 256, 512} : std::vector<unsigned>{128, 256};
        for (const auto side : sizes) {
          const auto result = evaluate(plan, {side,side}, nullptr, memoryBudget, thorough ? 4 : 1);
          for (const auto& [channel, image] : result.outputs) {
            const auto first = image->pixels.front();
            bool varies = false;
            for (const auto& pixel : image->pixels) {
              varies |= pixel != first;
              for (const float value : pixel)
                check(std::isfinite(value) && value >= (channel == "normal" ? -1 : 0) && value <= 1, id.c_str());
            }
            if ((channel == "baseColor" || channel == "height") && !varies)
              throw std::runtime_error(id + " has constant " + channel + " at " +
                std::to_string(side) + " px, seed " + std::to_string(seed));
          }
          if (seed == 24058 && side == (thorough ? 128u : 32u)) {
            const auto preview = png(*result.outputs.at("baseColor"));
            if (!images.emplace(ftebroom::sha256(preview)).second)
              throw std::runtime_error(id + " duplicates a thumbnail.");
            if (thorough) write(previews / (id + ".png"), preview);
          }
        }
        if (seed == 24058 && plan.frames > 1) {
          ++animations;
          const Resolution size{32,32};
          const auto first = evaluate(plan, size, nullptr, memoryBudget, 1, 0);
          const auto next = evaluate(plan, size, nullptr, memoryBudget, 1, plan.frames / 3);
          const auto loop = evaluate(plan, size, nullptr, memoryBudget, 1, plan.frames);
          if (first.outputs.at("baseColor")->pixels == next.outputs.at("baseColor")->pixels)
            throw std::runtime_error(id + " has static animation frames.");
          if (first.outputs.at("baseColor")->pixels != loop.outputs.at("baseColor")->pixels)
            throw std::runtime_error(id + " animation does not wrap.");
        }
      }
    }
  }
  check(recipeIds.size() == 120 && animations >= 6, "Catalog size and animation coverage");
  std::cout << "PASS 120 distinct catalog recipes at five seeds, including " << animations << " looping recipes\n";
}

int main(int argc, char** argv) try {
  if (argc == 3 && std::string_view(argv[1]) == "--catalog-matrix") {
    checkCatalog(fs::u8path(argv[2]));
    return 0;
  }
  if (argc != 2) return 2;
  using namespace ts;
  Pcg32 pcg(42, 54);
  for (uint32_t expected : {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu})
    check(pcg.next() == expected, "PCG32 reference vector");
  for (unsigned i = 0; i <= 1000; ++i) {
    const float v = float(i) / 1000;
    check(std::abs(toSrgb(fromSrgb(v)) - v) <= 1e-6f, "sRGB round trip");
  }
  check(std::abs(fromSrgb(.04045f) - .003130805f) < 1e-7f, "sRGB break point");
  check(lerpColor({1,0,0,0}, {0,1,0,1}, .5f) == Pixel{0,1,0,.5f}, "Premultiplied interpolation");
  const auto noiseSeed = stableSeed(42, "noise");
  for (int n = 0; n < 10000; ++n) {
    const float u = float(n % 127) / 256, v = float(n / 127) / 256;
    const float value = valueNoise(noiseSeed, 37, u, v);
    check(value == valueNoise(noiseSeed, 37, u + 1, v) &&
      value == valueNoise(noiseSeed, 37, u, v + 1), "Periodic reference sampling");
    const auto gradient = gradientNoise(noiseSeed, 37, u, v);
    check(gradient == gradientNoise(noiseSeed, 37, u + 1, v) &&
      gradient == gradientNoise(noiseSeed, 37, u, v + 1), "Periodic Gradient Noise");
    const auto cellular = cellularDistance(noiseSeed, 37, .9f, u, v);
    check(cellular == cellularDistance(noiseSeed, 37, .9f, u + 1, v) &&
      cellular == cellularDistance(noiseSeed, 37, .9f, u, v + 1), "Periodic Worley distances");
  }

  auto brick = runningBrick("fixture");
  auto roundTrip = open(brick.save());
  check(roundTrip.source == brick.source, "Source round trip");
  auto newer = open("{\"schemaVersion\":3,\"foreign\":[1,2,3]}");
  check(newer.readOnly && newer.save() == "{\"schemaVersion\":3,\"foreign\":[1,2,3]}", "Future input preserved");
  reject([&]{open("{\"schemaVersion\":1,\"schemaVersion\":3}");}, "Duplicate object key");
  auto invalid = brick;
  invalid.source["graph"]["edges"].push_back(wire("soot", "value", "wear", "input"));
  reject([&]{compile(invalid);}, "Input port does not exist");
  invalid = brick;
  invalid.source["graph"]["edges"][0] = wire("soot", "value", "bond", "variation");
  reject([&]{compile(invalid);}, "cycle");
  invalid.source["graph"]["nodes"].push_back(invalid.source["graph"]["nodes"][0]);
  reject([&]{compile(invalid);}, "Duplicate node ID");
  invalid = brick;
  invalid.source["graph"]["edges"][0] = wire("rough", "color", "bond", "variation");
  reject([&]{compile(invalid);}, "types are incompatible");
  auto foreign = brick;
  foreign.source["graph"]["nodes"].push_back(node("extension", "UnknownVendor", {{"raw", {7,8,9}}}, 1));
  check(open(foreign.save()).source == foreign.source, "Opaque node preserved");
  foreign.source["outputs"]["height"] = {{"node", "extension"}, {"port", "value"}};
  reject([&]{compile(foreign);}, "unknown node");
  check(compile(foreign, {"baseColor"}).order.size() == 6, "Only requested output ancestors visited");
  auto grouped = runningBrick("subgraph-test");
  grouped.source["graph"] = {
    {"nodes", Json::array({node("strength", "Constant", {{"value", .5}}, 0),
      node("exterior", "Subgraph", {{"group", "outer"}}, 1)})},
    {"edges", Json::array({wire("strength", "value", "exterior", "amount")})},
    {"groups", {
      {"inner", {
        {"nodes", Json::array({node("quarter", "Constant", {{"value", .25}}, 0),
          node("product", "Multiply", Json::object(), 1)})},
        {"edges", Json::array({wire("quarter", "value", "product", "b")})},
        {"inputs", Json::array({Json{{"name", "amount"}, {"node", "product"}, {"port", "a"}}})},
        {"outputs", {{"result", {{"node", "product"}, {"port", "value"}}}}}
      }},
      {"outer", {
        {"nodes", Json::array({node("inside", "Subgraph", {{"group", "inner"}}, 0)})},
        {"edges", Json::array()},
        {"inputs", Json::array({Json{{"name", "amount"}, {"node", "inside"}, {"port", "amount"}}})},
        {"outputs", {{"result", {{"node", "inside"}, {"port", "result"}}}}}
      }}
    }}
  };
  grouped.source["outputs"] = {{"height", {{"node", "exterior"}, {"port", "result"}}}};
  const auto groupPlan = compile(open(grouped.save()));
  check(evaluate(groupPlan, {32,32}).outputs.at("height")->pixels.front()[0] == .125f,
    "Nested subgraph input and output aliases");
  auto revisedGroup = grouped;
  revisedGroup.source["graph"]["groups"]["inner"]["nodes"][0]["position"] = {1000, 2000};
  check(renderIdentity(revisedGroup) == renderIdentity(grouped), "Subgraph layout does not invalidate the render");
  revisedGroup.source["graph"]["groups"]["inner"]["nodes"][0]["parameters"]["value"] = .75;
  check(renderIdentity(revisedGroup) != renderIdentity(grouped), "Subgraph semantic changes invalidate the render");
  revisedGroup = grouped;
  revisedGroup.source["graph"]["groups"]["outer"]["nodes"][0]["parameters"]["group"] = "outer";
  reject([&]{compile(revisedGroup);}, "Recursive subgraph");
  revisedGroup = grouped;
  revisedGroup.source["graph"]["edges"][0]["toPort"] = "bogus";
  reject([&]{compile(revisedGroup);}, "no such input");
  revisedGroup = grouped;
  revisedGroup.source["graph"]["nodes"][0] = node("strength", "ConstantColor", defaultParameters(Kernel::color), 0);
  revisedGroup.source["graph"]["edges"][0]["fromPort"] = "color";
  reject([&]{compile(revisedGroup);}, "types are incompatible");
  std::set<std::string> recipeImages;
  for (unsigned i = 0; i < presetNames.size(); ++i) {
    const auto doc = builtinPreset(i, "preset-test");
    for (uint32_t seed : {0u, 1u, 42u, 24058u, UINT32_MAX}) {
      auto variant = doc;
      variant.source["rootSeed"] = seed;
      const auto rendered = evaluate(compile(variant), {64,64});
      if (seed == 42)
        check(recipeImages.emplace(png(*rendered.outputs.at("baseColor"))).second, "Distinct built-in recipe output");
    }
  }
  checkCatalog({});
  for (const auto& spec : contracts()) {
    if (spec.kernel == Kernel::image) continue; // Exercised with encoded originals in the CLI suite.
    auto doc = runningBrick("node-check");
    auto& nodes = doc.source["graph"]["nodes"] = Json::array({
      node("test", spec.type, defaultParameters(spec.kernel), 1),
      node("scalar", "Constant", {{"value", .5}}, 0, 0),
      node("color", "ConstantColor", {{"rgba", {.48, .2, .09, 1}}}, 0, 1),
      node("flat", "HeightToNormal", defaultParameters(Kernel::normal), 0, 2),
      node("uv", "UV", Json::object(), 0, 3),
      node("data", "ChannelPack", Json::object(), 0, 4)
    });
    auto& edges = doc.source["graph"]["edges"] = Json::array({wire("scalar", "value", "flat", "height"),
      wire("scalar", "value", "data", "red"), wire("scalar", "value", "data", "green"),
      wire("scalar", "value", "data", "blue")});
    for (const auto& pin : spec.inputs) {
      if (pin.type == Port::color) edges.push_back(wire("color", "color", "test", pin.name));
      else if (pin.type == Port::normal) edges.push_back(wire("flat", "normal", "test", pin.name));
      else if (pin.type == Port::uv) edges.push_back(wire("uv", "uv", "test", pin.name));
      else if (pin.type == Port::packed) edges.push_back(wire("data", "data", "test", pin.name));
      else edges.push_back(wire("scalar", "value", "test", pin.name));
    }
    const char* channel = "height";
    std::string outputNode = "test", outputPort = spec.output.name;
    switch (spec.output.type) {
      case Port::normal: channel = "normal"; break;
      case Port::packed: channel = "packedData"; break;
      case Port::color: channel = "baseColor"; break;
      case Port::uv:
        channel = "baseColor";
        outputNode = "projected";
        outputPort = "color";
        nodes.push_back(node("projected", "SampleUV", Json::object(), 2));
        edges.push_back(wire("color", "color", "projected", "color"));
        edges.push_back(wire("test", spec.output.name, "projected", "coordinates"));
        break;
      case Port::scalar: break;
    }
    doc.source["outputs"] = {{channel, {{"node", outputNode}, {"port", outputPort}}}};
    const auto image = evaluate(compile(doc), {33,25}).outputs.at(channel);
    for (const auto& pixel : image->pixels)
      for (float sample : pixel)
        check(std::isfinite(sample) && sample >= -1 && sample <= 1, spec.type);
    check(image->pixels.size() == 825, "Every declared node executes against its default contract");
  }
  const auto motion = compile(builtinPreset(unsigned(presetNames.size() - 1), "animated-flow"));
  const auto first = evaluate(motion, {64,64}, nullptr, memoryBudget, 1, 0);
  const auto middle = evaluate(motion, {64,64}, nullptr, memoryBudget, 1, motion.frames / 3);
  const auto wrapped = evaluate(motion, {64,64}, nullptr, memoryBudget, 4, motion.frames);
  check(first.outputs.at("baseColor")->pixels != middle.outputs.at("baseColor")->pixels, "Animated material changes frames");
  for (const auto& [channel, image] : first.outputs)
    check(image->pixels == wrapped.outputs.at(channel)->pixels, "Animation wraps without a discontinuity");

  auto a = evaluate(compile(brick), {128,128});
  auto b = evaluate(compile(roundTrip), {128,128});
  check(a.outputs.at("baseColor")->pixels == b.outputs.at("baseColor")->pixels, "Stable reference output");
  auto wrappedGroup = brick;
  groupNodes(wrappedGroup, {"wear", "bond", "clay"}, "room-masonry", "Room masonry");
  const auto afterGroup = evaluate(compile(open(wrappedGroup.save())), {128,128});
  for (const auto& [channel, image] : a.outputs)
    check(image->pixels == afterGroup.outputs.at(channel)->pixels, "Grouping preserves render output and the existing seed stream");
  check(wrappedGroup.source["exposedParameters"][0]["group"] == "room-masonry",
    "Grouping retains the Quick parameter binding");
  auto filtered = brick;
  auto& filterGraph = filtered.source["graph"];
  filterGraph["nodes"].push_back(node("spread", "Dilate", {{"radius", 3}}, 5));
  filterGraph["nodes"].push_back(node("soften", "DirectionalBlur", {{"radius", 4}, {"axis", 3}}, 6));
  filterGraph["edges"].push_back(wire("bond", "value", "spread", "value"));
  filterGraph["edges"].push_back(wire("spread", "value", "soften", "value"));
  filtered.source["outputs"]["height"] = {{"node", "soften"}, {"port", "value"}};
  const auto haloPlan = compile(filtered);
  const Resolution oddSize{549, 511};
  NodeCache referenceCache(1);
  const auto fullImage = evaluate(haloPlan, oddSize, nullptr, memoryBudget, 2, 0, &referenceCache);
  const auto tiles = evaluateTiles(haloPlan, oddSize, memoryBudget, 0, 4, nullptr);
  check(tiles.has_value(), "Tile evaluator supports filters with multi-level halos");
  for (const auto& [channel, image] : fullImage.outputs)
    check(image->pixels == tiles->outputs.at(channel)->pixels, "Tiles and whole-image evaluator agree at both wrap seams");
  const auto sphere = materialPreview(a, false);
  const auto cube = materialPreview(a, true);
  check(sphere.pixels.front()[3] == 0 && cube.pixels.front()[3] == 0,
    "Transparent background in geometry previews");
  check(sphere.sample(128, 128)[3] == 1 && cube.sample(128, 128)[3] == 1 &&
    sphere.pixels != cube.pixels, "Preview mesh surface and UV mapping");
  auto flatNormal = a;
  flatNormal.outputs.erase("normal");
  check(materialPreview(flatNormal, false).pixels != sphere.pixels,
    "Lit preview uses the material normal channel");
  auto layered = brick;
  appendSolidLayer(layered, layers(layered), "blue-layer", {.04f,.09f,.65f,1});
  appendSolidLayer(layered, layers(layered), "amber-layer", {.8f,.4f,.07f,1});
  const auto originalLayers = evaluate(compile(layered), {32,32}).outputs.at("baseColor");
  auto stack = layers(layered);
  check(stack.order == std::vector<std::string>{"diffuse", "blue-layer", "amber-layer"}, "Layers use the graph order");
  std::swap(stack.order[1], stack.order[2]);
  bindLayers(layered, stack);
  const auto reordered = evaluate(compile(layered), {32,32}).outputs.at("baseColor");
  check(originalLayers->pixels != reordered->pixels, "Layer order changes the compiled material");
  findNode(layered, "blue-layer")["parameters"]["enabled"] = false;
  const auto hidden = evaluate(compile(layered), {32,32}).outputs.at("baseColor");
  stack.order.pop_back();
  bindLayers(layered, stack);
  check(hidden->pixels == evaluate(compile(layered), {32,32}).outputs.at("baseColor")->pixels,
    "A hidden layer leaves underlying pixels unchanged");
  NodeCache cache(4 * 1024 * 1024);
  auto preview = evaluate(compile(brick), {128,128}, nullptr, memoryBudget, 1, 0, &cache);
  auto adjusted = brick;
  findNode(adjusted, "normal")["parameters"]["strength"] = 0;
  auto revised = evaluate(compile(adjusted), {128,128}, nullptr, memoryBudget, 1, 0, &cache);
  check(revised.outputs.at("baseColor") == preview.outputs.at("baseColor") &&
    revised.outputs.at("normal") != preview.outputs.at("normal") && cache.hitCount() != 0 && cache.bytes() <= cache.limit(),
    "Independent branches remain cached after an adjustment");
  const auto plan = compile(brick);
  auto single = evaluate(plan, {257,129});
  for (unsigned workers : {2u, 4u, 16u}) {
    auto parallel = evaluate(plan, {257,129}, nullptr, memoryBudget, workers);
    for (const auto& [channel, image] : single.outputs)
      check(image->pixels == parallel.outputs.at(channel)->pixels, "Thread-invariant output across tile seams");
  }
  auto changed = brick;
  changed.source["rootSeed"] = UINT32_MAX;
  auto c = evaluate(compile(changed), {128,128});
  check(a.outputs.at("baseColor")->pixels != c.outputs.at("baseColor")->pixels, "Seed changes reference map");
  std::reverse(changed.source["graph"]["nodes"].begin(), changed.source["graph"]["nodes"].end());
  check(renderIdentity(changed) == renderIdentity(open(changed.save())), "Stable canonical identity");
  changed.source["rootSeed"] = brick.seed();
  changed.source["graph"]["nodes"][0]["position"] = {9,1};
  check(renderIdentity(changed) == renderIdentity(brick), "Layout is excluded from render identity");
  History undo(brick);
  undo.replace(changed);
  check(undo.undo() && undo.get().source == brick.source && undo.redo() &&
    undo.get().source == changed.source, "Document undo and redo preserve graph");
  for (const auto& p : a.outputs.at("normal")->pixels)
    check(std::abs(p[0]*p[0] + p[1]*p[1] + p[2]*p[2] - 1) < 1e-5f, "Normalized generated normal");
  reject([&]{evaluate(compile(brick), {8192,8192});}, "RAM budget");
  std::atomic<bool> stop{true};
  reject([&]{evaluate(compile(brick), {4,4}, &stop);}, "cancelled");

  const auto path = fs::u8path(argv[1]);
  fs::create_directories(path);
  write(path / "brick.png", png(*a.outputs.at("baseColor")));
  write(path / "height16.png", png(*a.outputs.at("height"), 16));
  write(path / "normal.png", png(*a.outputs.at("normal")));
  write(path / "brick.rbtex.json", brick.save());
  std::string palette;
  for (int i = 0; i < 256; ++i) {
    palette += char(i); palette += char(i); palette += char(i);
  }
  const auto& albedo = *a.outputs.at("baseColor");
  write(path / "brick.tga", tga(albedo));
  write(path / "brick.wad2", indexed(albedo, "brick", palette, IndexedFormat::wad2));
  write(path / "brick.wal", indexed(albedo, "studio/brick", palette, IndexedFormat::wal));
  auto white = albedo;
  std::fill(white.pixels.begin(), white.pixels.end(), Pixel{1,1,1,1});
  const auto whiteWal = indexed(white, "studio/white", palette, IndexedFormat::wal);
  check(std::all_of(whiteWal.begin() + 100, whiteWal.end(), [](unsigned char index) {
    return index == 254;
  }), "Opaque WAL must not use transparent index 255");
  auto cutout = albedo;
  for (unsigned y = 0; y < 128; ++y)
    for (unsigned x = 0; x < 64; ++x) cutout.pixels[y * 128 + x][3] = 0;
  write(path / "brick.wad3", indexed(cutout, "{brick", palette, IndexedFormat::wad3, true));
  reject([&] { indexed(cutout, "brick", palette, IndexedFormat::wad2); }, "opaque");
  reject([&] { indexed(albedo, "../escape", palette, IndexedFormat::wal); }, "name");
  std::cout << "PASS Texture Studio seed, graph, sampling, color, history, memory, and PNG fixtures\n";
  return 0;
} catch (const std::exception& e) {
  std::cerr << e.what() << '\n';
  return 1;
}
