// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "evaluate.h"

namespace ftebroom::textureStudio {

struct Parameter {
  const char* label;
  const char* node;
  const char* key;
  float min, max;
  unsigned step;
};

inline constexpr std::array<Parameter, 7> brickParameters{{
  {"Columns", "bond", "columns", 1, 32, 1},
  {"Rows", "bond", "rows", 2, 32, 2},
  {"Mortar width", "bond", "mortar", .01f, .4f, 0},
  {"Edge bevel", "bond", "bevel", .01f, .4f, 0},
  {"Surface wear", "bond", "wear", 0, .7f, 0},
  {"Detail scale", "wear", "period", 1, 128, 1},
  {"Bump strength", "normal", "strength", 0, 8, 0},
}};

inline Json node(const char* id, const char* type, Json parameters, int column, int row = 0) {
  return {{"id", id}, {"type", type}, {"version", 1}, {"parameters", std::move(parameters)},
    {"position", {column * 230 + 12, row * 125 + 12}}};
}

inline Json wire(const char* from, const char* output, const char* to, const char* input) {
  return {{"fromNode", from}, {"fromPort", output}, {"toNode", to}, {"toPort", input}};
}

inline Json& findNode(Document& doc, std::string_view id) {
  for (auto& item : doc.source.at("graph").at("nodes"))
    if (item.at("id").get_ref<const std::string&>() == id) return item;
  throw Error(std::string(id), "This document has no such node.");
}

inline Document runningBrick(std::string id, uint32_t seed = 24058) {
  Json nodes = Json::array({
    node("wear", "ValueNoise", {{"period", 36}, {"contrast", 1.25}}, 0),
    node("bond", "Bricks", {{"columns", 4}, {"rows", 8}, {"mortar", .1},
      {"bevel", .06}, {"wear", .28}}, 1),
    node("clay", "ColorRamp", {{"low", {.091, .081, .063, 1}},
      {"high", {.48, .12, .065, 1}}}, 2),
    node("normal", "HeightToNormal", {{"strength", 2.8}, {"invertY", false}}, 2, 1),
    node("ao", "HeightToAO", {{"strength", 2.5}}, 2, 2),
    node("rough", "ColorRamp", {{"low", {.29, .27, .23, 1}},
      {"high", {.55, .48, .39, 1}}}, 1, 1),
    node("soot", "Multiply", Json::object(), 1, 2),
    node("diffuse", "Blend", {{"opacity", .23}}, 3),
    node("roughness", "Constant", {{"value", .76}}, 3, 3),
  });
  Json edges = Json::array({
    wire("wear", "value", "bond", "variation"),
    wire("bond", "value", "clay", "value"),
    wire("bond", "value", "normal", "height"),
    wire("bond", "value", "ao", "height"),
    wire("wear", "value", "rough", "value"),
    wire("bond", "value", "soot", "a"),
    wire("wear", "value", "soot", "b"),
    wire("clay", "color", "diffuse", "base"),
    wire("rough", "color", "diffuse", "top"),
    wire("soot", "value", "diffuse", "mask"),
  });
  Json bindings = Json::array();
  for (const auto& p : brickParameters)
    bindings.push_back({{"node", p.node}, {"key", p.key}, {"label", p.label},
      {"min", p.min}, {"max", p.max}, {"step", p.step}});
  const auto output = [](const char* id, const char* port) {
    return Json{{"node", id}, {"port", port}};
  };
  Json source{{"schemaVersion", schemaVersion}, {"generatorVersion", generatorVersion},
    {"documentId", std::move(id)}, {"name", "Running Brick"}, {"rootSeed", seed},
    {"resolution", {{"width", 256}, {"height", 256}}},
    {"colorManagement", {{"working", "linear-srgb"}, {"display", "srgb"}}},
    {"graph", {{"nodes", std::move(nodes)}, {"edges", std::move(edges)}}},
    {"exposedParameters", std::move(bindings)}, {"layerBindings", Json::array()},
    {"assetReferences", Json::array()}, {"exportProfiles", Json::array()},
    {"animation", {{"frames", 1}, {"fps", 10}, {"loop", true}}},
    {"outputs", {{"baseColor", output("diffuse", "color")}, {"height", output("bond", "value")},
      {"normal", output("normal", "normal")}, {"ao", output("ao", "value")},
      {"roughness", output("roughness", "value")}}},
    {"provenance", {{"author", "ReyondBroom"}, {"preset", "masonry-running-brick"},
        {"license", "GPL-3.0-or-later"}, {"assets", Json::array()}}}};
  return {std::move(source), false, {}};
}

inline uint32_t variationSeed(uint32_t root, unsigned variation) {
  Pcg32 random(stableSeed(root, "variation", variation), 27);
  return random.next();
}

inline constexpr std::array<const char*, 14> presetNames{
  "Running Brick", "Ceramic Square", "Hazard Stripes", "Cast Concrete", "Pebble Gravel",
  "Luminous Panel", "Basalt Ridges", "Checked Rubber", "Water Ripples", "Toxic Foam",
  "Lava Crust", "Oil Sheen", "Energy Grid", "Pulsing Status"
};

inline Document builtinPreset(unsigned preset, std::string id) {
  require(preset < presetNames.size(), "preset", "Unknown built-in preset.");
  auto doc = runningBrick(std::move(id));
  if (preset == 0) return doc;
  doc.source["name"] = presetNames[preset];
  static constexpr const char* ids[]{"masonry-running-brick", "ceramic-square", "hazard-stripes",
    "cast-concrete", "pebble-gravel", "luminous-panel", "basalt-ridges", "checked-rubber", "water-ripples",
    "toxic-foam", "lava-crust", "oil-sheen", "energy-grid", "pulsing-status"};
  doc.source["provenance"]["preset"] = ids[preset];
  auto& edges = doc.source["graph"]["edges"];
  edges.erase(size_t(0)); // The replacement pattern has no variation input.
  auto& pattern = findNode(doc, "bond");
  auto& pigment = findNode(doc, "clay")["parameters"];
  auto& bindings = doc.source["exposedParameters"];
  bindings = Json::array();
  const auto control = [&](Parameter p) {
    bindings.push_back({{"node", p.node}, {"key", p.key}, {"label", p.label},
      {"min", p.min}, {"max", p.max}, {"step", p.step}});
  };
  if (preset == 1) {
    pattern = node("bond", "Tiles", {{"columns", 4}, {"rows", 4}, {"grout", .07}, {"bevel", .05}}, 1);
    pigment["low"] = {.055, .060, .055, 1};
    pigment["high"] = {.66, .72, .63, 1};
    findNode(doc, "roughness")["parameters"]["value"] = .3;
    control({"Columns", "bond", "columns", 1, 32, 1});
    control({"Rows", "bond", "rows", 1, 32, 1});
    control({"Grout width", "bond", "grout", .01f, .4f, 0});
    control({"Edge bevel", "bond", "bevel", .01f, .4f, 0});
  } else if (preset == 2) {
    pattern = node("bond", "Stripes", {{"period", 4}, {"duty", .48}, {"feather", .017}, {"slant", 4}}, 1);
    pigment["low"] = {.023, .020, .018, 1};
    pigment["high"] = {.88, .48, .035, 1};
    findNode(doc, "normal")["parameters"]["strength"] = .12;
    control({"Stripe count", "bond", "period", 1, 16, 1});
    control({"Paint coverage", "bond", "duty", .1f, .9f, 0});
    control({"Edge softness", "bond", "feather", .001f, .05f, 0});
    control({"Diagonal slope", "bond", "slant", 0, 16, 1});
  } else if (preset == 3 || preset == 6) {
    const bool concrete = preset == 3;
    pattern = node("bond", concrete ? "FBM" : "Ridged",
      {{"period", concrete ? 4 : 7}, {"octaves", concrete ? 5 : 6},
       {"gain", concrete ? .64 : .48}, {"lacunarity", 2}}, 1);
    pigment["low"] = concrete ? Json{.12,.115,.108,1} : Json{.038,.038,.037,1};
    pigment["high"] = concrete ? Json{.39,.37,.33,1} : Json{.18,.16,.145,1};
    findNode(doc, "roughness")["parameters"]["value"] = concrete ? .86 : .8;
    control({"Coarse scale", "bond", "period", 1, 64, 1});
    control({"Detail layers", "bond", "octaves", 1, 7, 1});
    control({"Detail gain", "bond", "gain", 0, 1, 0});
  } else if (preset == 4) {
    pattern = node("bond", "Worley", {{"period", 23}, {"jitter", .95}}, 1);
    pigment["low"] = {.42,.29,.20,1};
    pigment["high"] = {.055,.046,.041,1};
    findNode(doc, "roughness")["parameters"]["value"] = .92;
    control({"Pebble density", "bond", "period", 4, 64, 1});
    control({"Pebble dispersion", "bond", "jitter", 0, 1, 0});
  } else if (preset == 5) {
    pattern = node("bond", "Rectangle", {{"width", .72}, {"height", .56}, {"feather", .035}}, 1);
    pigment["low"] = {.071,.091,.09,1};
    pigment["high"] = {.023,.063,.065,1};
    auto& nodes = doc.source["graph"]["nodes"];
    nodes.push_back(node("panel-edge", "EdgeDetect", {{"strength", 5}}, 2, 3));
    nodes.push_back(node("panel-light", "ColorRamp", {{"low", {0,0,0,1}}, {"high", {.024,.8,.32,1}}}, 3, 1));
    edges.push_back(wire("bond", "value", "panel-edge", "value"));
    edges.push_back(wire("panel-edge", "value", "panel-light", "value"));
    doc.source["outputs"]["emissive"] = {{"node", "panel-light"}, {"port", "color"}};
    control({"Plate width", "bond", "width", .1f, .91f, 0});
    control({"Plate height", "bond", "height", .1f, .91f, 0});
    control({"Edge softness", "bond", "feather", .001f, .05f, 0});
    control({"Emission strength", "panel-edge", "strength", 0, 8, 0});
  } else if (preset == 7) {
    pattern = node("bond", "Checker", {{"columns", 6}, {"rows", 6}}, 1);
    pigment["low"] = {.033,.034,.032,1};
    pigment["high"] = {.093,.086,.075,1};
    findNode(doc, "normal")["parameters"]["strength"] = .22;
    control({"Columns", "bond", "columns", 2, 32, 2});
    control({"Rows", "bond", "rows", 2, 32, 2});
  } else {
    static constexpr std::array<Pixel, 6> shadows{{
      {.020f,.063f,.11f,1}, {.02f,.053f,.004f,1}, {.025f,.011f,.008f,1},
      {.013f,.009f,.036f,1}, {.016f,.032f,.059f,1}, {.019f,.020f,.025f,1}
    }};
    static constexpr std::array<Pixel, 6> highlights{{
      {.063f,.31f,.36f,1}, {.24f,.66f,.044f,1}, {.95f,.23f,.016f,1},
      {.61f,.19f,.34f,1}, {.047f,.56f,.95f,1}, {.54f,.72f,.10f,1}
    }};
    const unsigned style = preset - 8;
    const char* flow = style == 0 ? "bond" : "flow";
    const auto waves = Json{{"period", style == 5 ? 1 : style + 3}, {"cycles", 1}, {"amplitude", style == 0 ? .16 : .44}};
    if (style == 0) pattern = node(flow, "Flow", waves, 1);
    else pattern = node("bond", style == 2 || style == 4 ? "Add" : "Multiply", Json::object(), 2);
    pigment["low"] = shadows[style];
    pigment["high"] = highlights[style];
    findNode(doc, "roughness")["parameters"]["value"] = style == 3 ? .11 : .24;
    findNode(doc, "rough")["parameters"]["low"] = shadows[style];
    findNode(doc, "rough")["parameters"]["high"] = highlights[style];
    auto& nodes = doc.source["graph"]["nodes"];
    nodes.push_back(node("time", "Time", Json::object(), 0, 2));
    edges.push_back(wire("time", "value", flow, "phase"));
    if (style != 0) {
      nodes.push_back(node(flow, "Flow", waves, 1, 1));
      if (style == 1) {
        nodes.push_back(node("animated-detail", "Worley", {{"period", 26}, {"jitter", .72}}, 0, 3));
      } else if (style == 2) {
        nodes.push_back(node("lava-ridges", "Ridged", {{"period", 6}, {"octaves", 5}, {"gain", .62}, {"lacunarity", 2}}, 0, 3));
        nodes.push_back(node("animated-detail", "Invert", Json::object(), 1, 3));
        edges.push_back(wire("lava-ridges", "value", "animated-detail", "value"));
      } else if (style == 3) {
        nodes.push_back(node("animated-detail", "Flow", {{"period", 7}, {"cycles", 3}, {"amplitude", .62}}, 1, 3));
        edges.push_back(wire("time", "value", "animated-detail", "phase"));
      } else if (style == 4) {
        nodes.push_back(node("animated-detail", "Checker", {{"columns", 12}, {"rows", 12}}, 0, 3));
      } else {
        nodes.push_back(node("animated-detail", "Circle", {{"radius", .29}, {"feather", .027}}, 0, 3));
      }
      edges.push_back(wire(flow, "value", "bond", "a"));
      edges.push_back(wire("animated-detail", "value", "bond", "b"));
    }
    if (style == 2 || style == 4 || style == 5) {
      nodes.push_back(node("motion-emission", "ColorRamp",
        {{"low", {0,0,0,1}}, {"high", highlights[style]}}, 4, 2));
      edges.push_back(wire("bond", "value", "motion-emission", "value"));
      doc.source["outputs"]["emissive"] = {{"node", "motion-emission"}, {"port", "color"}};
    }
    doc.source["animation"] = {{"frames", 16}, {"fps", 12}, {"loop", true}};
    control({"Wave count", flow, "period", 1, 16, 1});
    control({"Cycles per loop", flow, "cycles", 1, 8, 1});
    control({"Wave distortion", flow, "amplitude", 0, 1, 0});
  }
  control({"Surface wear", "diffuse", "opacity", 0, 1, 0});
  control({"Detail scale", "wear", "period", 1, 128, 1});
  control({"Bump strength", "normal", "strength", 0, 8, 0});
  return doc;
}

inline Document importedImage(std::string id, const Raster& image, Json reference) {
  auto doc = runningBrick(std::move(id), 0);
  doc.source["name"] = "Imported image";
  doc.source["resolution"] = {{"width", image.size.width}, {"height", image.size.height}};
  doc.source["assetReferences"] = Json::array({std::move(reference)});
  doc.source["provenance"] = {{"kind", "imported-raster"}};
  doc.source["graph"]["nodes"] = Json::array({
    node("original", "ImageInput", {{"asset", "original"}}, 0),
    node("height", "ColorToHeight", Json::object(), 1),
    node("normal", "HeightToNormal", {{"strength", 1.2}, {"invertY", false}}, 2),
    node("ao", "HeightToAO", {{"strength", 1.4}}, 2, 1)
  });
  doc.source["graph"]["edges"] = Json::array({
    wire("original", "color", "height", "color"),
    wire("height", "value", "normal", "height"),
    wire("height", "value", "ao", "height")
  });
  doc.source["outputs"] = {
    {"baseColor", {{"node", "original"}, {"port", "color"}}},
    {"height", {{"node", "height"}, {"port", "value"}}},
    {"normal", {{"node", "normal"}, {"port", "normal"}}},
    {"ao", {{"node", "ao"}, {"port", "value"}}}
  };
  doc.source["exposedParameters"] = Json::array({
    {{"node", "normal"}, {"key", "strength"}, {"label", "Bump strength"}, {"min", 0}, {"max", 8}, {"step", 0}},
    {{"node", "ao"}, {"key", "strength"}, {"label", "Occlusion strength"}, {"min", 0}, {"max", 8}, {"step", 0}}
  });
  return doc;
}

} // namespace ftebroom::textureStudio
