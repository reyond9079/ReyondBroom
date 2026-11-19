// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "subgraphs.h"
#include <memory>

namespace ftebroom::textureStudio {

enum class Port { scalar, color, normal, uv, packed };
enum class Kernel { unknown, constant, color, image, noise, bricks, tiles, checker, stripes, ramp, blend, add,
                    multiply, invert, clamp, luminance, normal, ao, gradient, worley, fbm, ridged, white,
                    rectangle, circle, blur, dilate, erode, edge, time, seed, flow, cracks, streaks, veins, grate, rivets,
                    flemish, rings, digit, arrow, scalarMix, minimum, maximum, difference,
                    diamond, hex, polygon, fibers, spots, levels, transform, tile, mirror,
                    rotate90, directionalBlur, hsv, posterize, pixelate, dither, normalBlend,
                    uv, uvSample, warp, directionalWarp, histogram, channelPack, channelUnpack, retroShade,
                    panels, distance, slopeBlur, colorize, palette, quantize, materialGrain, surfaceWear, boardJoints,
                    stonework, archBand, scalarBlend, brickLayout, flemishLayout, boardLayout, stoneBrickLayout };

enum class GrainKind { mineral, stone, wood, brushed, sand, fibers, polished, endGrain, bedded, marble, count };
enum class WearKind { pits, chips, runoff, scratches, deposits, count };

struct Pin { const char* name; Port type; };
struct Contract {
  const char* type;
  Kernel kernel;
  Pin output;
  std::vector<Pin> inputs;
};

inline const std::vector<Contract>& contracts() {
  static const std::vector<Contract> table{
    {"Constant", Kernel::constant, {"value", Port::scalar}, {}},
    {"ConstantColor", Kernel::color, {"color", Port::color}, {}},
    {"Time", Kernel::time, {"value", Port::scalar}, {}},
    {"Seed", Kernel::seed, {"value", Port::scalar}, {}},
    {"UV", Kernel::uv, {"uv", Port::uv}, {}},
    {"ImageInput", Kernel::image, {"color", Port::color}, {}},
    {"ValueNoise", Kernel::noise, {"value", Port::scalar}, {}},
    {"GradientNoise", Kernel::gradient, {"value", Port::scalar}, {}},
    {"Worley", Kernel::worley, {"value", Port::scalar}, {}},
    {"FBM", Kernel::fbm, {"value", Port::scalar}, {}},
    {"Ridged", Kernel::ridged, {"value", Port::scalar}, {}},
    {"WhiteNoise", Kernel::white, {"value", Port::scalar}, {}},
    {"Flow", Kernel::flow, {"value", Port::scalar}, {{"phase", Port::scalar}}},
    {"Cracks", Kernel::cracks, {"value", Port::scalar}, {}},
    {"Scratches", Kernel::streaks, {"value", Port::scalar}, {}},
    {"Fibers", Kernel::fibers, {"value", Port::scalar}, {}},
    {"Spots", Kernel::spots, {"value", Port::scalar}, {}},
    {"Veins", Kernel::veins, {"value", Port::scalar}, {}},
    {"MaterialGrain", Kernel::materialGrain, {"value", Port::scalar}, {}},
    {"SurfaceWear", Kernel::surfaceWear, {"value", Port::scalar}, {}},
    {"BoardJoints", Kernel::boardJoints, {"value", Port::scalar}, {}},
    {"Stonework", Kernel::stonework, {"value", Port::scalar}, {}},
    {"ArchBand", Kernel::archBand, {"value", Port::scalar}, {}},
    {"Bricks", Kernel::bricks, {"value", Port::scalar}, {{"variation", Port::scalar}}},
    {"FlemishBricks", Kernel::flemish, {"value", Port::scalar}, {{"variation", Port::scalar}}},
    {"BrickLayout", Kernel::brickLayout, {"data", Port::packed}, {{"variation", Port::scalar}}},
    {"FlemishLayout", Kernel::flemishLayout, {"data", Port::packed}, {{"variation", Port::scalar}}},
    {"StoneBrickLayout", Kernel::stoneBrickLayout, {"data", Port::packed}, {{"variation", Port::scalar}}},
    {"BoardLayout", Kernel::boardLayout, {"data", Port::packed}, {}},
    {"Tiles", Kernel::tiles, {"value", Port::scalar}, {}},
    {"Checker", Kernel::checker, {"value", Port::scalar}, {}},
    {"Stripes", Kernel::stripes, {"value", Port::scalar}, {}},
    {"Grate", Kernel::grate, {"value", Port::scalar}, {}},
    {"Rivets", Kernel::rivets, {"value", Port::scalar}, {}},
    {"DiamondPlate", Kernel::diamond, {"value", Port::scalar}, {}},
    {"Hex", Kernel::hex, {"value", Port::scalar}, {}},
    {"Panels", Kernel::panels, {"value", Port::scalar}, {}},
    {"Rectangle", Kernel::rectangle, {"value", Port::scalar}, {}},
    {"Circle", Kernel::circle, {"value", Port::scalar}, {}},
    {"Polygon", Kernel::polygon, {"value", Port::scalar}, {}},
    {"Rings", Kernel::rings, {"value", Port::scalar}, {}},
    {"Digit", Kernel::digit, {"value", Port::scalar}, {}},
    {"Arrow", Kernel::arrow, {"value", Port::scalar}, {}},
    {"DistanceField", Kernel::distance, {"value", Port::scalar}, {{"mask", Port::scalar}}},
    {"ColorRamp", Kernel::ramp, {"color", Port::color}, {{"value", Port::scalar}}},
    {"Blend", Kernel::blend, {"color", Port::color},
      {{"base", Port::color}, {"top", Port::color}, {"mask", Port::scalar}}},
    {"Multiply", Kernel::multiply, {"value", Port::scalar},
      {{"a", Port::scalar}, {"b", Port::scalar}}},
    {"Add", Kernel::add, {"value", Port::scalar}, {{"a", Port::scalar}, {"b", Port::scalar}}},
    {"ScalarMix", Kernel::scalarMix, {"value", Port::scalar}, {{"a", Port::scalar}, {"b", Port::scalar}}},
    {"ScalarBlend", Kernel::scalarBlend, {"value", Port::scalar},
      {{"a", Port::scalar}, {"b", Port::scalar}, {"mask", Port::scalar}}},
    {"Minimum", Kernel::minimum, {"value", Port::scalar}, {{"a", Port::scalar}, {"b", Port::scalar}}},
    {"Maximum", Kernel::maximum, {"value", Port::scalar}, {{"a", Port::scalar}, {"b", Port::scalar}}},
    {"Difference", Kernel::difference, {"value", Port::scalar}, {{"a", Port::scalar}, {"b", Port::scalar}}},
    {"Invert", Kernel::invert, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"Clamp", Kernel::clamp, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"Levels", Kernel::levels, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"HistogramRange", Kernel::histogram, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"Transform2D", Kernel::transform, {"color", Port::color}, {{"color", Port::color}}},
    {"Tile", Kernel::tile, {"color", Port::color}, {{"color", Port::color}}},
    {"Mirror", Kernel::mirror, {"color", Port::color}, {{"color", Port::color}}},
    {"Rotate90", Kernel::rotate90, {"color", Port::color}, {{"color", Port::color}}},
    {"SampleUV", Kernel::uvSample, {"color", Port::color}, {{"color", Port::color}, {"coordinates", Port::uv}}},
    {"Warp", Kernel::warp, {"uv", Port::uv},
      {{"coordinates", Port::uv}, {"alongU", Port::scalar}, {"alongV", Port::scalar}}},
    {"DirectionalWarp", Kernel::directionalWarp, {"uv", Port::uv},
      {{"coordinates", Port::uv}, {"displacement", Port::scalar}}},
    {"HSVAdjust", Kernel::hsv, {"color", Port::color}, {{"color", Port::color}}},
    {"Colorize", Kernel::colorize, {"color", Port::color}, {{"color", Port::color}}},
    {"PaletteMap", Kernel::palette, {"color", Port::color}, {{"color", Port::color}}},
    {"Posterize", Kernel::posterize, {"color", Port::color}, {{"color", Port::color}}},
    {"Quantize", Kernel::quantize, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"Pixelate", Kernel::pixelate, {"color", Port::color}, {{"color", Port::color}}},
    {"OrderedDither", Kernel::dither, {"color", Port::color}, {{"color", Port::color}}},
    {"RetroShade", Kernel::retroShade, {"color", Port::color},
      {{"color", Port::color}, {"normal", Port::normal}}},
    {"ChannelPack", Kernel::channelPack, {"data", Port::packed},
      {{"red", Port::scalar}, {"green", Port::scalar}, {"blue", Port::scalar}}},
    {"ChannelUnpack", Kernel::channelUnpack, {"value", Port::scalar}, {{"data", Port::packed}}},
    {"ColorToHeight", Kernel::luminance, {"value", Port::scalar}, {{"color", Port::color}}},
    {"Blur", Kernel::blur, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"DirectionalBlur", Kernel::directionalBlur, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"SlopeBlur", Kernel::slopeBlur, {"value", Port::scalar}, {{"value", Port::scalar}, {"slope", Port::scalar}}},
    {"Dilate", Kernel::dilate, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"Erode", Kernel::erode, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"EdgeDetect", Kernel::edge, {"value", Port::scalar}, {{"value", Port::scalar}}},
    {"HeightToNormal", Kernel::normal, {"normal", Port::normal}, {{"height", Port::scalar}}},
    {"HeightToAO", Kernel::ao, {"value", Port::scalar}, {{"height", Port::scalar}}},
    {"NormalBlend", Kernel::normalBlend, {"normal", Port::normal},
      {{"base", Port::normal}, {"detail", Port::normal}}},
  };
  return table;
}

inline const Contract* contract(std::string_view type) {
  const auto& table = contracts();
  auto found = std::find_if(table.begin(), table.end(), [type](const Contract& item) {
    return item.type == type;
  });
  return found == table.end() ? nullptr : &*found;
}

inline Json defaultParameters(Kernel kernel) {
  switch (kernel) {
    case Kernel::constant: return {{"value", .5}};
    case Kernel::color: return {{"rgba", {.18, .18, .18, 1}}};
    case Kernel::noise:
    case Kernel::gradient: return {{"period", 8}, {"contrast", 1}};
    case Kernel::worley: return {{"period", 8}, {"jitter", .8}};
    case Kernel::flow: return {{"period", 4}, {"cycles", 1}, {"amplitude", .75}, {"style", 0}};
    case Kernel::cracks: return {{"period", 8}, {"jitter", .8}, {"width", .035}};
    case Kernel::streaks: return {{"columns", 32}, {"rows", 4}, {"contrast", 2}};
    case Kernel::materialGrain: return {{"kind", 0}, {"period", 24}};
    case Kernel::surfaceWear: return {{"kind", 0}, {"period", 16}, {"density", .4}};
    case Kernel::boardJoints: return {{"columns", 6}, {"seams", 1}, {"width", .025}};
    case Kernel::boardLayout: return {{"columns", 6}, {"seams", 1}, {"width", .064}, {"irregularity", .55}};
    case Kernel::stonework: return {{"period", 9}, {"mortar", .08}, {"roundness", .5}};
    case Kernel::archBand: return {{"period", 11}, {"width", .115}};
    case Kernel::fibers: return {{"columns", 45}, {"rows", 5}, {"contrast", 2}};
    case Kernel::spots: return {{"period", 12}, {"radius", .2}, {"jitter", .6}};
    case Kernel::veins: return {{"period", 8}, {"width", .055}, {"distortion", .44}};
    case Kernel::fbm:
    case Kernel::ridged: return {{"period", 4}, {"octaves", 4}, {"gain", .5}, {"lacunarity", 2}};
    case Kernel::bricks:
    case Kernel::flemish: return {{"columns", 4}, {"rows", 8}, {"mortar", .1}, {"bevel", .06}, {"wear", .28}};
    case Kernel::stoneBrickLayout:
      return {{"columns", 4}, {"rows", 8}, {"mortar", .064}, {"bevel", .145}, {"wear", .43}, {"irregularity", .82}};
    case Kernel::brickLayout:
    case Kernel::flemishLayout:
      return {{"columns", 4}, {"rows", 8}, {"mortar", .075}, {"bevel", .035}, {"wear", .28}, {"irregularity", .55}};
    case Kernel::tiles: return {{"columns", 4}, {"rows", 4}, {"grout", .08}, {"bevel", .04}};
    case Kernel::checker: return {{"columns", 8}, {"rows", 8}};
    case Kernel::stripes: return {{"period", 4}, {"duty", .5}, {"feather", .015}, {"slant", 0}};
    case Kernel::grate: return {{"columns", 12}, {"rows", 4}, {"width", .18}, {"bevel", .08}};
    case Kernel::rivets: return {{"columns", 5}, {"rows", 5}, {"radius", .22}, {"bevel", .10}};
    case Kernel::diamond: return {{"columns", 6}, {"rows", 8}, {"width", .28}, {"bevel", .10}};
    case Kernel::hex: return {{"columns", 6}, {"rows", 8}, {"grout", .074}, {"bevel", .04}};
    case Kernel::panels: return {{"columns", 3}, {"rows", 3}, {"grout", .11}, {"bevel", .055}};
    case Kernel::rectangle: return {{"width", .45}, {"height", .6}, {"feather", .018}};
    case Kernel::circle: return {{"radius", .35}, {"feather", .018}};
    case Kernel::polygon: return {{"radius", .35}, {"feather", .018}, {"sides", 6}};
    case Kernel::rings: return {{"period", 12}, {"distortion", .16}};
    case Kernel::digit: return {{"digit", 8}, {"scale", .66}};
    case Kernel::arrow: return {{"width", .22}, {"length", .69}};
    case Kernel::distance: return {{"radius", 6}, {"threshold", .5}};
    case Kernel::ramp: return {{"low", {0, 0, 0, 1}}, {"high", {1, 1, 1, 1}}};
    case Kernel::blend: return {{"opacity", .5}, {"enabled", true}, {"mode", 0}};
    case Kernel::scalarMix: return {{"weight", .35}};
    case Kernel::clamp: return {{"low", .1}, {"high", .9}};
    case Kernel::levels: return {{"low", .05}, {"high", .95}, {"gamma", 1}};
    case Kernel::histogram: return {{"lowPercentile", .02}, {"highPercentile", .98}};
    case Kernel::transform: return {{"scaleU", 1}, {"scaleV", 1}, {"offsetU", 0}, {"offsetV", 0}, {"rotation", 0}};
    case Kernel::tile: return {{"columns", 2}, {"rows", 2}};
    case Kernel::mirror: return {{"axis", 0}};
    case Kernel::rotate90: return {{"steps", 1}};
    case Kernel::hsv: return {{"hue", 0}, {"saturation", 1}, {"value", 1}};
    case Kernel::colorize: return {{"shadow", {.04,.06,.08,1}}, {"highlight", {.65,.54,.31,1}}, {"mix", .65}};
    case Kernel::palette: return {{"swatches", {{.018,.014,.012,1}, {.096,.082,.066,1},
                                                {.36,.32,.245,1}, {.79,.76,.62,1}}}};
    case Kernel::posterize:
    case Kernel::quantize:
    case Kernel::dither: return {{"levels", 8}};
    case Kernel::pixelate: return {{"columns", 32}, {"rows", 32}};
    case Kernel::directionalBlur: return {{"radius", 3}, {"axis", 0}};
    case Kernel::slopeBlur: return {{"distance", 8}, {"samples", 8}};
    case Kernel::normalBlend: return {{"strength", .75}};
    case Kernel::warp: return {{"strength", .12}};
    case Kernel::directionalWarp: return {{"strength", .12}, {"angle", 45}};
    case Kernel::channelUnpack: return {{"channel", 0}};
    case Kernel::retroShade: return {{"enabled", false}, {"steps", 6}, {"ambient", .35}};
    case Kernel::blur:
    case Kernel::dilate:
    case Kernel::erode: return {{"radius", 2}};
    case Kernel::edge: return {{"strength", 1}};
    case Kernel::normal: return {{"strength", 1}, {"invertY", false}};
    case Kernel::ao: return {{"strength", 1}};
    default: return Json::object();
  }
}

inline const std::map<std::string, Port>& channelTypes() {
  static const std::map<std::string, Port> types{
    {"baseColor", Port::color}, {"emissive", Port::color}, {"normal", Port::normal},
    {"height", Port::scalar}, {"roughness", Port::scalar},
    {"metalness", Port::scalar}, {"ao", Port::scalar}, {"opacity", Port::scalar},
    {"packedData", Port::packed}
  };
  return types;
}

using Pixel = std::array<float, 4>;

struct Raster {
  Resolution size;
  std::vector<unsigned char> rgba;
};

using Assets = std::map<std::string, std::shared_ptr<const Raster>>;

inline Pixel rgba(const Json& value, const std::string& where) {
  require(value.is_array() && value.size() == 4, where, "Expected linear RGBA with four channels.");
  Pixel result{};
  for (size_t i = 0; i < result.size(); ++i)
    result[i] = number(value[i], 0, 1, where + "[" + std::to_string(i) + "]");
  return result;
}

struct Node {
  std::string id;
  const Contract* spec;
  std::string seedIdentity;
  std::array<float, 6> p{};
  Pixel first{}, last{};
  std::vector<Pixel> palette;
  std::array<int, 3> inputs{-1, -1, -1};
  std::string asset;
  std::shared_ptr<const Raster> raster;
};

struct Plan {
  std::vector<Node> nodes;
  std::map<std::string, int> outputs;
  std::vector<int> order;
  uint32_t seed = 0;
  unsigned frames = 1;
  bool loop = true;
};

inline void parameters(Node& node, const Json& params) {
  const auto path = "graph.nodes." + node.id + ".parameters";
  const auto range = [&](const char* name, float low, float high) {
    return number(member(params, name, path), low, high, path + "." + name);
  };
  const auto count = [&](const char* name, unsigned low, unsigned high) {
    return static_cast<float>(integer(member(params, name, path), low, high, path + "." + name));
  };
  switch (node.spec->kernel) {
  case Kernel::constant: node.p[0] = range("value", 0, 1); break;
  case Kernel::color: node.first = rgba(member(params, "rgba", path), path + ".rgba"); break;
  case Kernel::image: node.asset = idField(params, "asset", path); break;
  case Kernel::noise:
  case Kernel::gradient:
    node.p[0] = count("period", 1, 128);
    node.p[1] = range("contrast", 0, 2);
    break;
  case Kernel::worley:
    node.p[0] = count("period", 1, 128);
    node.p[1] = range("jitter", 0, 1);
    break;
  case Kernel::cracks:
    node.p[0] = count("period", 1, 128);
    node.p[1] = range("jitter", 0, 1);
    node.p[2] = range("width", .005f, .25f);
    break;
  case Kernel::streaks:
  case Kernel::fibers:
    node.p[0] = count("columns", 1, 128);
    node.p[1] = count("rows", 1, 128);
    node.p[2] = range("contrast", .1f, 5);
    break;
  case Kernel::materialGrain:
    node.p[0] = count("kind", 0, unsigned(GrainKind::count) - 1);
    node.p[1] = count("period", 2, 64);
    if (params.contains("naturalDetail")) node.p[2] = range("naturalDetail", 0, 1);
    break;
  case Kernel::surfaceWear:
    node.p[0] = count("kind", 0, unsigned(WearKind::count) - 1);
    node.p[1] = count("period", 2, 64);
    node.p[2] = range("density", 0, 1);
    if (params.contains("naturalDetail")) node.p[3] = range("naturalDetail", 0, 1);
    break;
  case Kernel::boardJoints:
  case Kernel::boardLayout:
    node.p[0] = count("columns", 1, 64);
    node.p[1] = count("seams", 0, 4);
    node.p[2] = range("width", .005f, .12f);
    if (params.contains("irregularity")) node.p[3] = range("irregularity", 0, 1);
    break;
  case Kernel::stonework:
    node.p[0] = count("period", 1, 64);
    node.p[1] = range("mortar", .02f, .25f);
    node.p[2] = range("roundness", 0, 1);
    if (params.contains("irregularity")) node.p[3] = range("irregularity", 0, 1);
    break;
  case Kernel::archBand:
    node.p[0] = count("period", 1, 64);
    node.p[1] = range("width", .04f, .24f);
    break;
  case Kernel::spots:
    node.p[0] = count("period", 1, 64);
    node.p[1] = range("radius", .045f, .6f);
    node.p[2] = range("jitter", 0, 1);
    break;
  case Kernel::veins:
    node.p[0] = count("period", 1, 64);
    node.p[1] = range("width", .01f, .35f);
    node.p[2] = range("distortion", 0, 2);
    break;
  case Kernel::fbm:
  case Kernel::ridged: {
    node.p[0] = count("period", 1, 128);
    node.p[1] = count("octaves", 1, 12);
    node.p[2] = range("gain", 0, 1);
    node.p[3] = count("lacunarity", 1, 4);
    unsigned frequency = unsigned(node.p[0]);
    for (int octave = 1; octave < int(node.p[1]); ++octave) {
      require(frequency <= 8192 / unsigned(node.p[3]), path + ".octaves", "The fractal frequency exceeds 8192.");
      frequency *= unsigned(node.p[3]);
    }
    break;
  }
  case Kernel::bricks:
  case Kernel::flemish:
  case Kernel::brickLayout:
  case Kernel::flemishLayout:
  case Kernel::stoneBrickLayout:
    node.p[0] = count("columns", 1, 64);
    node.p[1] = count("rows", 2, 64);
    node.p[2] = range("mortar", 0.01f, 0.4f);
    node.p[3] = range("bevel", 0.01f, 0.4f);
    node.p[4] = range("wear", 0, 0.7f);
    if (params.contains("irregularity")) node.p[5] = range("irregularity", 0, 1);
    require(static_cast<unsigned>(node.p[1]) % 2 == 0, path + ".rows",
      "Use an even row count to tile the running bond across the seam.");
    break;
  case Kernel::tiles:
  case Kernel::hex:
  case Kernel::panels:
    node.p[0] = count("columns", 1, 64);
    node.p[1] = count("rows", 1, 64);
    node.p[2] = range("grout", .01f, .4f);
    node.p[3] = range("bevel", .01f, .4f);
    if (node.spec->kernel == Kernel::hex)
      require(int(node.p[1]) % 2 == 0, path + ".rows", "Use an even row count for hexagonal repetition.");
    break;
  case Kernel::checker:
    node.p[0] = count("columns", 2, 64);
    node.p[1] = count("rows", 2, 64);
    require(int(node.p[0]) % 2 == 0 && int(node.p[1]) % 2 == 0, path,
      "Choose even checker counts so the pattern matches across both seams.");
    break;
  case Kernel::stripes:
    node.p[0] = count("period", 1, 64);
    node.p[1] = range("duty", .1f, .9f);
    node.p[2] = range("feather", .001f, .05f);
    node.p[3] = range("slant", -64, 64);
    require(node.p[3] == std::floor(node.p[3]), path + ".slant", "Use an integer slant to tile across both seams.");
    break;
  case Kernel::grate:
  case Kernel::diamond:
  case Kernel::rivets:
    node.p[0] = count("columns", 1, 64);
    node.p[1] = count("rows", 1, 64);
    node.p[2] = range(node.spec->kernel == Kernel::rivets ? "radius" : "width", .02f, .38f);
    node.p[3] = range("bevel", .005f, .22f);
    if (node.spec->kernel == Kernel::diamond)
      require(int(node.p[1]) % 2 == 0, path + ".rows", "Use an even row count for alternating treads.");
    break;
  case Kernel::rectangle:
    node.p[0] = range("width", .025f, .95f);
    node.p[1] = range("height", .025f, .95f);
    node.p[2] = range("feather", .001f, .05f);
    break;
  case Kernel::circle:
  case Kernel::polygon:
    node.p[0] = range("radius", .015f, .475f);
    node.p[1] = range("feather", .001f, .05f);
    if (node.spec->kernel == Kernel::polygon) node.p[2] = count("sides", 3, 12);
    break;
  case Kernel::rings:
    node.p[0] = count("period", 1, 64);
    node.p[1] = range("distortion", 0, 1);
    break;
  case Kernel::digit:
    node.p[0] = count("digit", 0, 9);
    node.p[1] = range("scale", .1f, .95f);
    break;
  case Kernel::arrow:
    node.p[0] = range("width", .1f, .45f);
    node.p[1] = range("length", .2f, .9f);
    break;
  case Kernel::distance:
    node.p[0] = count("radius", 1, 12);
    node.p[1] = range("threshold", 0, 1);
    break;
  case Kernel::ramp:
    node.first = rgba(member(params, "low", path), path + ".low");
    node.last = rgba(member(params, "high", path), path + ".high");
    break;
  case Kernel::blend: {
    node.p[0] = range("opacity", 0, 1);
    auto enabled = params.find("enabled");
    require(enabled == params.end() || enabled->is_boolean(), path + ".enabled", "Expected a Boolean.");
    node.p[1] = enabled == params.end() || enabled->get<bool>() ? 1.f : 0.f;
    if (params.contains("mode")) node.p[2] = count("mode", 0, 4);
    break;
  }
  case Kernel::scalarBlend:
  case Kernel::add:
  case Kernel::multiply:
  case Kernel::minimum:
  case Kernel::maximum:
  case Kernel::difference:
  case Kernel::invert:
  case Kernel::luminance:
  case Kernel::time:
  case Kernel::seed:
  case Kernel::uv:
  case Kernel::uvSample:
  case Kernel::channelPack:
  case Kernel::white: break;
  case Kernel::scalarMix: node.p[0] = range("weight", 0, 1); break;
  case Kernel::flow:
    node.p[0] = count("period", 1, 32);
    node.p[1] = count("cycles", 1, 8);
    node.p[2] = range("amplitude", 0, 1);
    if (params.contains("style")) node.p[3] = count("style", 0, 4);
    break;
  case Kernel::clamp:
    node.p[0] = range("low", 0, 1);
    node.p[1] = range("high", 0, 1);
    require(node.p[0] <= node.p[1], path, "The low bound must be at most the high bound.");
    break;
  case Kernel::levels:
    node.p[0] = range("low", 0, 1);
    node.p[1] = range("high", 0, 1);
    node.p[2] = range("gamma", .125f, 8);
    require(node.p[0] < node.p[1], path, "Levels require a nonempty input range.");
    break;
  case Kernel::histogram:
    node.p[0] = range("lowPercentile", 0, .9f);
    node.p[1] = range("highPercentile", .1f, 1);
    require(node.p[0] < node.p[1], path, "Choose the lower percentile before the higher percentile.");
    break;
  case Kernel::transform:
    node.p[0] = range("scaleU", .03125f, 32);
    node.p[1] = range("scaleV", .03125f, 32);
    node.p[2] = range("offsetU", -1, 1);
    node.p[3] = range("offsetV", -1, 1);
    node.p[4] = range("rotation", -180, 180);
    break;
  case Kernel::tile:
  case Kernel::pixelate:
    node.p[0] = count("columns", 1, 128);
    node.p[1] = count("rows", 1, 128);
    break;
  case Kernel::mirror: node.p[0] = count("axis", 0, 2); break;
  case Kernel::rotate90: node.p[0] = count("steps", 0, 3); break;
  case Kernel::hsv:
    node.p[0] = range("hue", -180, 180);
    node.p[1] = range("saturation", 0, 2);
    node.p[2] = range("value", 0, 2);
    break;
  case Kernel::colorize:
    node.first = rgba(member(params, "shadow", path), path + ".shadow");
    node.last = rgba(member(params, "highlight", path), path + ".highlight");
    node.p[0] = range("mix", 0, 1);
    break;
  case Kernel::palette: {
    const auto& samples = member(params, "swatches", path);
    require(samples.is_array() && samples.size() >= 2 && samples.size() <= 256, path,
      "Provide 2 to 256 linear RGBA palette entries.");
    for (const auto& sample : samples) node.palette.push_back(rgba(sample, path + ".swatches"));
    break;
  }
  case Kernel::posterize:
  case Kernel::quantize:
  case Kernel::dither: node.p[0] = count("levels", 2, 256); break;
  case Kernel::directionalBlur:
    node.p[0] = count("radius", 1, 16);
    node.p[1] = count("axis", 0, 3);
    break;
  case Kernel::slopeBlur:
    node.p[0] = range("distance", 0, 32);
    node.p[1] = count("samples", 2, 16);
    break;
  case Kernel::normalBlend: node.p[0] = range("strength", 0, 1); break;
  case Kernel::warp:
  case Kernel::directionalWarp:
    node.p[0] = range("strength", 0, 1);
    if (node.spec->kernel == Kernel::directionalWarp) node.p[1] = range("angle", -180, 180);
    break;
  case Kernel::channelUnpack: node.p[0] = count("channel", 0, 3); break;
  case Kernel::retroShade:
    require(member(params, "enabled", path).is_boolean(), path + ".enabled", "Expected a Boolean.");
    node.p[0] = params["enabled"].get<bool>() ? 1.f : 0.f;
    node.p[1] = count("steps", 2, 32);
    node.p[2] = range("ambient", 0, 1);
    break;
  case Kernel::blur:
  case Kernel::dilate:
  case Kernel::erode: node.p[0] = count("radius", 1, 8); break;
  case Kernel::edge: node.p[0] = range("strength", 0, 8); break;
  case Kernel::normal: {
    node.p[0] = range("strength", 0, 8);
    auto& value = member(params, "invertY", path);
    require(value.is_boolean(), path + ".invertY", "Expected a Boolean.");
    node.p[1] = value.get<bool>() ? -1.f : 1.f;
    break;
  }
  case Kernel::ao:
    node.p[0] = range("strength", 0, 8);
    if (params.contains("radius")) node.p[1] = count("radius", 1, 16);
    break;
  case Kernel::unknown: break;
  }
}

inline Plan compile(const Document& document, const std::vector<std::string>& requested = {}, const Assets& assets = {}) {
  require(!document.readOnly, "document", "A newer source version is read-only.");
  Plan plan;
  plan.seed = document.seed();
  const auto& animation = document.source.at("animation");
  plan.frames = integer(member(animation, "frames", "animation"), 1, 64, "animation.frames");
  plan.loop = member(animation, "loop", "animation").get<bool>();
  const auto& graph = document.source.at("graph");
  const auto& targets = document.source.at("outputs");
  std::optional<SubgraphExpander::Expanded> expanded;
  if (graph.contains("groups"))
    expanded = SubgraphExpander(graph.at("groups")).finish(graph, targets);
  const auto& objects = expanded ? expanded->nodes : graph.at("nodes");
  const auto& edges = expanded ? expanded->edges : graph.at("edges");
  require(objects.is_array() && objects.size() <= maxNodes, "graph.nodes", "Node budget exceeded.");
  require(edges.is_array() && edges.size() <= maxEdges, "graph.edges", "Edge budget exceeded.");
  std::map<std::string, int> byId;
  for (const auto& object : objects) {
    const auto id = idField(object, "id", "graph.nodes");
    const auto path = "graph.nodes." + id;
    const auto type = idField(object, "type", path);
    require(byId.emplace(id, static_cast<int>(plan.nodes.size())).second, path, "Duplicate node ID.");
    const auto version = integer(member(object, "version", path), 1, 65535, path + ".version");
    Node node{id, version == 1 ? contract(type) : nullptr};
    node.seedIdentity = object.contains("seedIdentity") ? idField(object, "seedIdentity", path) : id;
    if (node.spec) parameters(node, member(object, "parameters", path));
    plan.nodes.push_back(std::move(node));
  }

  const auto lookup = [&](const std::string& id, const std::string& path) {
    auto match = byId.find(id);
    require(match != byId.end(), path, "Referenced node does not exist.");
    return match->second;
  };
  std::vector<std::vector<int>> consumers(plan.nodes.size()), predecessors(plan.nodes.size());
  std::vector<unsigned> degree(plan.nodes.size());
  std::set<std::pair<int, std::string>> occupied;
  for (size_t i = 0; i < edges.size(); ++i) {
    const auto path = "graph.edges[" + std::to_string(i) + "]";
    auto& edge = edges[i];
    const auto from = lookup(idField(edge, "fromNode", path), path + ".fromNode");
    const auto to = lookup(idField(edge, "toNode", path), path + ".toNode");
    const auto fromPort = idField(edge, "fromPort", path);
    const auto toPort = idField(edge, "toPort", path);
    const auto& src = plan.nodes[from];
    auto& dst = plan.nodes[to];
    require(occupied.emplace(to, toPort).second, path, "The input already has a connection.");
    if (src.spec)
      require(fromPort == src.spec->output.name, path + ".fromPort", "Output port does not exist.");
    if (dst.spec) {
      auto pin = std::find_if(dst.spec->inputs.begin(), dst.spec->inputs.end(),
        [&](Pin candidate) { return candidate.name == toPort; });
      require(pin != dst.spec->inputs.end(), path + ".toPort", "Input port does not exist.");
      require(!src.spec || src.spec->output.type == pin->type, path, "Port types are incompatible.");
      dst.inputs[size_t(pin - dst.spec->inputs.begin())] = from;
    }
    consumers[from].push_back(to);
    predecessors[to].push_back(from);
    ++degree[to];
  }

  std::set<std::pair<std::string, int>> ready;
  for (const auto& [id, index] : byId) if (!degree[index]) ready.emplace(id, index);
  std::vector<int> topological;
  while (!ready.empty()) {
    const auto index = ready.begin()->second;
    ready.erase(ready.begin());
    topological.push_back(index);
    for (int follower : consumers[index])
      if (!--degree[follower]) ready.emplace(plan.nodes[follower].id, follower);
  }
  require(topological.size() == plan.nodes.size(), "graph.edges", "The graph has a cycle.");

  const auto& outputs = expanded ? expanded->outputs : targets;
  require(outputs.is_object() && !outputs.empty(), "outputs", "Connect at least one output.");
  for (auto output = outputs.begin(); output != outputs.end(); ++output) {
    const auto path = "outputs." + output.key();
    const auto type = channelTypes().find(output.key());
    require(type != channelTypes().end(), path, "Unknown output channel.");
    const int index = lookup(idField(*output, "node", path), path + ".node");
    const auto port = idField(*output, "port", path);
    const auto* spec = plan.nodes[index].spec;
    require(!spec || (spec->output.name == port && spec->output.type == type->second),
      path, "The output channel requires another port type.");
    if (requested.empty() || std::find(requested.begin(), requested.end(), output.key()) != requested.end())
      plan.outputs.emplace(output.key(), index);
  }
  for (const auto& name : requested)
    require(plan.outputs.count(name) != 0, "outputs." + name, "Requested output is missing.");

  std::vector<bool> needed(plan.nodes.size());
  std::vector<int> visit;
  for (const auto& [name, index] : plan.outputs) visit.push_back(index);
  while (!visit.empty()) {
    const int index = visit.back();
    visit.pop_back();
    if (needed[index]) continue;
    needed[index] = true;
    auto& node = plan.nodes[index];
    const auto path = "graph.nodes." + node.id;
    require(node.spec != nullptr, path, "This output depends on an unknown node type or version.");
    if (node.spec->kernel == Kernel::image) {
      const auto found = assets.find(node.asset);
      require(found != assets.end(), path + ".asset", "The referenced image is missing.");
      node.raster = found->second;
      require(node.raster && node.raster->size.width && node.raster->size.height &&
        node.raster->size.width <= maxDimension && node.raster->size.height <= maxDimension &&
        node.raster->rgba.size() == size_t(node.raster->size.width) * node.raster->size.height * 4,
        path + ".asset", "The referenced image has invalid dimensions or pixels.");
    }
    for (size_t slot = 0; slot < node.spec->inputs.size(); ++slot)
      require(node.inputs[slot] >= 0, path + "." + node.spec->inputs[slot].name, "Connect this required input.");
    visit.insert(visit.end(), predecessors[index].begin(), predecessors[index].end());
  }
  for (int index : topological) if (needed[index]) plan.order.push_back(index);
  return plan;
}

} // namespace ftebroom::textureStudio
