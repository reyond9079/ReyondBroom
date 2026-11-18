// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "presets.h"

namespace ftebroom::textureStudio {

using Pattern = Kernel;

struct MaterialRecipe {
  const char* id;
  const char* name;
  Pattern pattern, detail, composition;
  unsigned scale, detailScale;
  uint32_t shadow, highlight;
};

inline constexpr const char* recipeCategories[]{
  "Concrete", "Masonry", "Plaster and Tiles", "Metal", "Tech Panels", "Wood",
  "Ground", "Natural Surfaces", "Organic Horror", "Liquids and Energy", "Decals and Markings", "Trims and Displays"
};

inline constexpr const char* recipeCategoryIds[]{
  "concrete", "masonry", "plaster", "metal", "tech", "wood", "ground",
  "natural", "organic", "liquids", "decals", "trims"
};

inline constexpr MaterialRecipe catalogRecipes[][10]{
  {
    {"poured", "Poured Concrete", Pattern::fbm, Pattern::gradient, Pattern::scalarMix, 6, 36, 0x555554, 0xaaa69b},
    {"board-formed", "Board-formed Concrete", Pattern::stripes, Pattern::streaks, Pattern::difference, 8, 43, 0x454a4b, 0xa1a7a0},
    {"cracked-slab", "Cracked Slab", Pattern::cracks, Pattern::fbm, Pattern::multiply, 6, 21, 0x292a2c, 0xb1a99b},
    {"chipped-panel", "Chipped Concrete Panel", Pattern::tiles, Pattern::worley, Pattern::minimum, 4, 44, 0x4c4946, 0xb4aba0},
    {"wet-bunker", "Wet Bunker", Pattern::fbm, Pattern::veins, Pattern::minimum, 16, 10, 0x252e2c, 0x66706b},
    {"aggregate", "Exposed Aggregate", Pattern::worley, Pattern::white, Pattern::maximum, 32, 61, 0x585359, 0xc3acab},
    {"patched", "Patched Concrete", Pattern::rectangle, Pattern::ridged, Pattern::maximum, 9, 13, 0x494d50, 0xc6c1aa},
    {"stained-ceiling", "Stained Concrete Ceiling", Pattern::streaks, Pattern::worley, Pattern::scalarMix, 23, 16, 0x6b6250, 0xc8bc9e},
    {"expansion-floor", "Expansion-joint Floor", Pattern::grate, Pattern::gradient, Pattern::difference, 3, 29, 0x363532, 0x9e9791},
    {"reinforced-damage", "Reinforced Concrete Damage", Pattern::cracks, Pattern::grate, Pattern::maximum, 14, 19, 0x403a35, 0xa98365}
  },
  {
    {"running-brick", "Warm Running Brick", Pattern::bricks, Pattern::gradient, Pattern::multiply, 4, 43, 0x4d3025, 0xba704c},
    {"flemish-brick", "Flemish Brick", Pattern::flemish, Pattern::streaks, Pattern::scalarMix, 6, 22, 0x5b3627, 0xc88860},
    {"broken-brick", "Broken Brick", Pattern::bricks, Pattern::cracks, Pattern::minimum, 8, 13, 0x4d3331, 0xa8766b},
    {"rough-stone", "Rough Stonework", Pattern::cracks, Pattern::ridged, Pattern::minimum, 7, 10, 0x434644, 0xada99c},
    {"ashlar", "Ashlar Blocks", Pattern::tiles, Pattern::veins, Pattern::difference, 4, 34, 0x797161, 0xc1bc98},
    {"cobblestone", "Rounded Cobblestone", Pattern::cracks, Pattern::worley, Pattern::multiply, 12, 15, 0x303337, 0xaca89f},
    {"mortar-rubble", "Mortar and Rubble", Pattern::worley, Pattern::cracks, Pattern::difference, 27, 8, 0x4c4c43, 0xa5a994},
    {"gothic-block", "Gothic Block", Pattern::flemish, Pattern::rings, Pattern::minimum, 4, 11, 0x353841, 0x9f9ba3},
    {"basalt-blocks", "Basalt Blocks", Pattern::tiles, Pattern::ridged, Pattern::multiply, 9, 17, 0x292b31, 0x77747b},
    {"arch-trim", "Worn Arch Trim", Pattern::rings, Pattern::cracks, Pattern::multiply, 10, 11, 0x463f40, 0xb3a89a}
  },
  {
    {"peeling", "Peeling Plaster", Pattern::worley, Pattern::ridged, Pattern::maximum, 13, 39, 0x78775c, 0xe4d9bb},
    {"cracked-paint", "Cracked Paint", Pattern::cracks, Pattern::gradient, Pattern::minimum, 24, 14, 0x56515a, 0xd8d1d4},
    {"square-ceramic", "Porcelain Squares", Pattern::tiles, Pattern::gradient, Pattern::maximum, 6, 41, 0x455d56, 0xb7ddd2},
    {"subway", "Subway Tile", Pattern::bricks, Pattern::veins, Pattern::maximum, 5, 20, 0x625b55, 0xeee4d7},
    {"mosaic", "Mosaic Tile", Pattern::checker, Pattern::tiles, Pattern::multiply, 24, 24, 0x233b4c, 0x8fc1b8},
    {"stained-grout", "Stained Grout", Pattern::tiles, Pattern::worley, Pattern::difference, 7, 31, 0x526766, 0xa3c0b1},
    {"painted-brick", "Painted Brick", Pattern::bricks, Pattern::streaks, Pattern::maximum, 7, 36, 0x475c5d, 0x8ababa},
    {"terrazzo", "Terrazzo", Pattern::cracks, Pattern::white, Pattern::difference, 35, 77, 0x8b8191, 0xd8cfad},
    {"acoustic", "Acoustic Tile", Pattern::tiles, Pattern::rivets, Pattern::difference, 5, 42, 0x5f5a54, 0xd3ccb7},
    {"insulation", "Damaged Insulation", Pattern::streaks, Pattern::cracks, Pattern::maximum, 35, 9, 0x78593d, 0xe2ba7d}
  },
  {
    {"steel", "Brushed Steel", Pattern::streaks, Pattern::gradient, Pattern::maximum, 56, 21, 0x555961, 0xcccfd5},
    {"oxidized", "Oxidized Iron", Pattern::worley, Pattern::gradient, Pattern::multiply, 15, 32, 0x3d312a, 0xa76b4e},
    {"chipped-steel", "Painted Chipped Steel", Pattern::cracks, Pattern::streaks, Pattern::difference, 24, 30, 0x303c47, 0x7b9b9a},
    {"diamond-plate", "Diamond Plate", Pattern::diamond, Pattern::fibers, Pattern::minimum, 9, 23, 0x2b2c33, 0xbbc0c5},
    {"perforated-sheet", "Perforated Sheet", Pattern::rivets, Pattern::gradient, Pattern::difference, 25, 35, 0x151d20, 0xa0b4bb},
    {"corrugated-sheet", "Corrugated Sheet", Pattern::stripes, Pattern::fbm, Pattern::maximum, 14, 17, 0x303738, 0xa5b2a6},
    {"riveted", "Riveted Plate", Pattern::grate, Pattern::rivets, Pattern::maximum, 4, 9, 0x4c4841, 0x9c9a88},
    {"welded", "Welded Seam", Pattern::veins, Pattern::streaks, Pattern::maximum, 4, 41, 0x4b454d, 0xafa1a0},
    {"cast-metal", "Cast Metal", Pattern::ridged, Pattern::white, Pattern::multiply, 24, 52, 0x494341, 0x9d8d81},
    {"rust-flakes", "Rust Flakes", Pattern::cracks, Pattern::fbm, Pattern::difference, 27, 28, 0x4e2718, 0xd59655}
  },
  {
    {"modular-wall", "Modular Wall", Pattern::panels, Pattern::grate, Pattern::difference, 3, 20, 0x2c3943, 0x6c8d9d},
    {"service-hatch", "Service Hatch", Pattern::rectangle, Pattern::rivets, Pattern::maximum, 12, 6, 0x343537, 0x8c8f92},
    {"recessed-panel", "Recessed Panel", Pattern::rectangle, Pattern::grate, Pattern::minimum, 27, 11, 0x232830, 0x616c77},
    {"vent-grille", "Vent Grille", Pattern::grate, Pattern::streaks, Pattern::minimum, 18, 44, 0x262e30, 0x93a09d},
    {"conduit-plate", "Conduit Plate", Pattern::veins, Pattern::rectangle, Pattern::maximum, 5, 21, 0x4f4132, 0x8aa8a0},
    {"cable-tray", "Cable Tray", Pattern::stripes, Pattern::grate, Pattern::minimum, 11, 19, 0x23252f, 0x6c78a9},
    {"access-door", "Access Door", Pattern::rectangle, Pattern::arrow, Pattern::difference, 19, 22, 0x37483e, 0x7a9d69},
    {"armored-floor", "Armored Floor", Pattern::tiles, Pattern::rivets, Pattern::maximum, 6, 33, 0x2c2929, 0x7a8285},
    {"ceiling-module", "Ceiling Module", Pattern::checker, Pattern::grate, Pattern::maximum, 8, 23, 0x2b353d, 0x819390},
    {"laboratory", "Laboratory Panel", Pattern::tiles, Pattern::streaks, Pattern::scalarMix, 4, 45, 0x606e77, 0xc5d4d6}
  },
  {
    {"planks", "Timber Planks", Pattern::stripes, Pattern::fibers, Pattern::multiply, 6, 38, 0x584027, 0xbe9161},
    {"parquet", "Parquet", Pattern::checker, Pattern::streaks, Pattern::difference, 12, 32, 0x54301f, 0xb16d39},
    {"plywood", "Plywood", Pattern::fibers, Pattern::ridged, Pattern::minimum, 37, 12, 0x62432b, 0xd1a977},
    {"end-grain", "End Grain", Pattern::rings, Pattern::veins, Pattern::difference, 20, 13, 0x503828, 0xc8a574},
    {"weathered", "Weathered Boards", Pattern::stripes, Pattern::cracks, Pattern::multiply, 8, 24, 0x383638, 0x9c9083},
    {"painted-boards", "Painted Boards", Pattern::stripes, Pattern::worley, Pattern::maximum, 9, 26, 0x363d48, 0x6d8b91},
    {"burned", "Burned Wood", Pattern::cracks, Pattern::streaks, Pattern::minimum, 17, 46, 0x211e20, 0x68564d},
    {"bark", "Bark Slabs", Pattern::streaks, Pattern::veins, Pattern::multiply, 32, 10, 0x453020, 0x9d7658},
    {"crate", "Crate Panel", Pattern::rectangle, Pattern::streaks, Pattern::maximum, 32, 39, 0x634624, 0xb58a54},
    {"split", "Split Timber", Pattern::veins, Pattern::cracks, Pattern::minimum, 19, 6, 0x5a3822, 0xb58c6f}
  },
  {
    {"soil", "Loose Soil", Pattern::fbm, Pattern::worley, Pattern::difference, 17, 35, 0x3c2c24, 0x997256},
    {"gravel", "Mixed Gravel", Pattern::worley, Pattern::ridged, Pattern::multiply, 24, 11, 0x423d40, 0x9d8e81},
    {"sand", "Sand Ripples", Pattern::veins, Pattern::white, Pattern::scalarMix, 13, 70, 0x92724d, 0xdcc89a},
    {"mud", "Muddy Ground", Pattern::worley, Pattern::fbm, Pattern::minimum, 13, 9, 0x352821, 0x837251},
    {"cracked-earth", "Cracked Earth", Pattern::cracks, Pattern::ridged, Pattern::difference, 13, 22, 0x593f29, 0xbc8c63},
    {"asphalt", "Dense Asphalt", Pattern::white, Pattern::ridged, Pattern::minimum, 43, 23, 0x35353b, 0x6c6e76},
    {"damaged-asphalt", "Damaged Asphalt", Pattern::cracks, Pattern::white, Pattern::multiply, 11, 53, 0x282a2c, 0x777277},
    {"snow", "Snow Crust", Pattern::worley, Pattern::white, Pattern::minimum, 22, 52, 0xa9b4bb, 0xe9e9ed},
    {"leaf-litter", "Leaf Litter", Pattern::spots, Pattern::fibers, Pattern::difference, 19, 33, 0x4f392b, 0xa97f44},
    {"rock-scree", "Rock Scree", Pattern::cracks, Pattern::worley, Pattern::maximum, 34, 14, 0x535058, 0xa89e93}
  },
  {
    {"granite", "Speckled Granite", Pattern::worley, Pattern::white, Pattern::multiply, 38, 80, 0x5c5e62, 0xcbbdba},
    {"stratified", "Stratified Rock", Pattern::veins, Pattern::ridged, Pattern::maximum, 12, 22, 0x4e4a4c, 0xba9277},
    {"sandstone", "Layered Sandstone", Pattern::streaks, Pattern::ridged, Pattern::difference, 34, 13, 0x695444, 0xc6a77d},
    {"marble", "Marble Veins", Pattern::veins, Pattern::fbm, Pattern::difference, 5, 23, 0x767d8a, 0xe5e6ec},
    {"ice", "Fractured Ice", Pattern::cracks, Pattern::veins, Pattern::maximum, 12, 19, 0x3a7388, 0xb4dede},
    {"moss", "Cushion Moss", Pattern::worley, Pattern::fbm, Pattern::maximum, 19, 29, 0x343a26, 0x72984c},
    {"lichen", "Lichen Patches", Pattern::worley, Pattern::cracks, Pattern::maximum, 31, 19, 0x444b35, 0x9ea17c},
    {"roots", "Exposed Roots", Pattern::veins, Pattern::streaks, Pattern::difference, 7, 25, 0x342d27, 0x7a624e},
    {"cliff", "Cliff Face", Pattern::ridged, Pattern::cracks, Pattern::difference, 11, 19, 0x474349, 0xa99b8a},
    {"salt", "Salt Crust", Pattern::cracks, Pattern::white, Pattern::scalarMix, 19, 54, 0x837c7c, 0xe4dbca}
  },
  {
    {"pores", "Skin Pores", Pattern::worley, Pattern::fbm, Pattern::scalarMix, 42, 16, 0x724739, 0xc89276},
    {"flesh", "Flesh Veins", Pattern::veins, Pattern::cracks, Pattern::difference, 5, 26, 0x612930, 0xbc7a73},
    {"scar", "Scar Tissue", Pattern::streaks, Pattern::veins, Pattern::maximum, 31, 14, 0x743638, 0xc18b84},
    {"bone", "Bone Plates", Pattern::hex, Pattern::cracks, Pattern::multiply, 6, 18, 0x7b6655, 0xd7c7a4},
    {"chitin", "Chitin", Pattern::rings, Pattern::cracks, Pattern::difference, 20, 23, 0x3a2829, 0x794435},
    {"infestation", "Root Infestation", Pattern::veins, Pattern::worley, Pattern::multiply, 9, 22, 0x372b33, 0x72644f},
    {"slime-membrane", "Slime Membrane", Pattern::worley, Pattern::ridged, Pattern::difference, 19, 31, 0x2e5048, 0x7ba581},
    {"tendons", "Tendons", Pattern::veins, Pattern::streaks, Pattern::maximum, 11, 41, 0x592932, 0xb98270},
    {"cells", "Alien Cells", Pattern::hex, Pattern::spots, Pattern::difference, 8, 12, 0x4b3250, 0xa181aa},
    {"biomechanical", "Biomechanical Wall", Pattern::grate, Pattern::veins, Pattern::difference, 7, 15, 0x2e2935, 0x7e7184}
  },
  {
    {"water", "Woven Water Ripples", Pattern::flow, Pattern::veins, Pattern::scalarMix, 4, 9, 0x173b61, 0x4e9bb5},
    {"toxic", "Toxic Fluid", Pattern::flow, Pattern::worley, Pattern::multiply, 8, 17, 0x253924, 0x9ac04d},
    {"lava", "Veined Lava Crust", Pattern::flow, Pattern::cracks, Pattern::difference, 6, 12, 0x4b2d23, 0xe58d37},
    {"sludge", "Deep Sludge", Pattern::flow, Pattern::ridged, Pattern::minimum, 5, 12, 0x323f2d, 0x6e7948},
    {"oil", "Oil Film", Pattern::flow, Pattern::streaks, Pattern::maximum, 5, 35, 0x282235, 0xa17998},
    {"acid", "Acid Bubbles", Pattern::flow, Pattern::rivets, Pattern::difference, 7, 21, 0x315047, 0xb5dd88},
    {"field", "Energy Field", Pattern::flow, Pattern::checker, Pattern::maximum, 11, 16, 0x283b72, 0x5db5c6},
    {"plasma", "Plasma Grid", Pattern::flow, Pattern::grate, Pattern::scalarMix, 10, 12, 0x492b55, 0xbb81dd},
    {"molten", "Molten Metal", Pattern::flow, Pattern::fbm, Pattern::minimum, 7, 20, 0x5a3833, 0xedbe5b},
    {"flowing", "Flowing Slime", Pattern::flow, Pattern::veins, Pattern::difference, 6, 17, 0x36431b, 0x79b056}
  },
  {
    {"hazard", "Industrial Hazard Stripes", Pattern::stripes, Pattern::cracks, Pattern::minimum, 8, 27, 0x232424, 0xeab944},
    {"number", "Room Number", Pattern::digit, Pattern::streaks, Pattern::multiply, 7, 28, 0x303238, 0xe6e4cb},
    {"arrow", "Direction Arrow", Pattern::arrow, Pattern::gradient, Pattern::minimum, 15, 26, 0x263640, 0xa9d8d8},
    {"warning", "Warning Panel", Pattern::rectangle, Pattern::stripes, Pattern::multiply, 20, 10, 0x30342e, 0xf0c351},
    {"rust-drip", "Rust Drip", Pattern::streaks, Pattern::worley, Pattern::minimum, 51, 16, 0x573320, 0xb77f54},
    {"water-streak", "Water Streak", Pattern::streaks, Pattern::gradient, Pattern::multiply, 16, 20, 0x3e4645, 0x5f6d5d},
    {"impact", "Impact Mark", Pattern::polygon, Pattern::cracks, Pattern::minimum, 13, 15, 0x3a383b, 0x838089},
    {"crack-overlay", "Crack Overlay", Pattern::cracks, Pattern::veins, Pattern::minimum, 21, 11, 0x3d4042, 0x71727b},
    {"leak", "Leak Stain", Pattern::worley, Pattern::streaks, Pattern::multiply, 9, 34, 0x484238, 0x82715f},
    {"grime", "Grime Edge", Pattern::rectangle, Pattern::worley, Pattern::difference, 11, 25, 0x343e37, 0x747c64}
  },
  {
    {"industrial-trim", "Industrial Trim", Pattern::grate, Pattern::streaks, Pattern::maximum, 6, 30, 0x333d3e, 0x839b9b},
    {"gothic-trim", "Gothic Trim", Pattern::rings, Pattern::flemish, Pattern::minimum, 15, 4, 0x51454d, 0xa99092},
    {"pipe-border", "Pipe Border", Pattern::stripes, Pattern::rivets, Pattern::multiply, 5, 10, 0x344448, 0x849e9e},
    {"cable-border", "Cable Border", Pattern::veins, Pattern::grate, Pattern::multiply, 10, 8, 0x414052, 0x868195},
    {"floor-border", "Floor Border", Pattern::stripes, Pattern::tiles, Pattern::minimum, 3, 12, 0x4b4444, 0xaa9e8c},
    {"door-frame", "Door Frame", Pattern::rectangle, Pattern::tiles, Pattern::difference, 18, 6, 0x343e46, 0x8798a0},
    {"terminal", "Terminal Display", Pattern::digit, Pattern::flow, Pattern::multiply, 2, 20, 0x293235, 0x86ceab},
    {"status", "Status Lights", Pattern::rivets, Pattern::flow, Pattern::multiply, 8, 6, 0x344137, 0x90b955},
    {"grille-strip", "Grille Strip", Pattern::grate, Pattern::stripes, Pattern::difference, 21, 7, 0x393739, 0x969298},
    {"beam-trim", "Structural Beam Trim", Pattern::rectangle, Pattern::rivets, Pattern::minimum, 29, 18, 0x444e55, 0x9da7a1}
  }
};

static_assert(std::size(catalogRecipes) == std::size(recipeCategories));
static_assert(std::size(catalogRecipes) == std::size(recipeCategoryIds));

inline const Contract& recipeContract(Pattern pattern) {
  for (const auto& type : contracts()) if (type.kernel == pattern) return type;
  throw Error("preset", "Unknown recipe node.");
}

inline Json recipePattern(Pattern pattern, unsigned scale) {
  auto params = defaultParameters(pattern);
  if (params.contains("period")) {
    const auto limit = pattern == Pattern::flow ? 32u : 64u;
    params["period"] = std::min(scale, limit);
  }
  if (params.contains("columns")) {
    const auto even = [](unsigned value) { return std::max(2u, value & ~1u); };
    const auto columns = std::min(64u, scale);
    const auto rows = std::min(64u, scale * 2);
    params["columns"] = pattern == Pattern::checker ? even(columns) : columns;
    if (pattern == Pattern::bricks || pattern == Pattern::flemish || pattern == Pattern::checker ||
        pattern == Pattern::hex || pattern == Pattern::diamond)
      params["rows"] = even(rows);
    else if (pattern == Pattern::streaks || pattern == Pattern::fibers) params["rows"] = std::max(2u, scale / 6);
    else if (pattern == Pattern::grate) params["rows"] = std::max(1u, scale / 3);
    else params["rows"] = std::min(64u, scale + scale % 3);
  }
  switch (pattern) {
    case Pattern::stripes: params["duty"] = .25 + .05 * (scale % 9); break;
    case Pattern::rectangle:
      params["width"] = .25 + .035 * (scale % 17);
      params["height"] = .35 + .04 * (scale % 13);
      break;
    case Pattern::circle: params["radius"] = .18 + .012 * (scale % 20); break;
    case Pattern::polygon:
      params["radius"] = .18 + .012 * (scale % 20);
      params["sides"] = 5 + scale % 5;
      break;
    case Pattern::digit: params["digit"] = scale % 10; break;
    case Pattern::arrow: params["length"] = .48 + .03 * (scale % 12); break;
    default: break;
  }
  return params;
}

inline std::string recipeId(size_t category, size_t index) {
  return std::string(recipeCategoryIds[category]) + "-" + catalogRecipes[category][index].id;
}

inline Document catalogPreset(size_t category, size_t index, std::string id) {
  require(category < std::size(catalogRecipes) && index < std::size(catalogRecipes[0]),
    "preset", "Unknown catalog recipe.");
  const auto& recipe = catalogRecipes[category][index];
  auto doc = runningBrick(std::move(id));
  auto& source = doc.source;
  source["name"] = recipe.name;
  source["provenance"]["preset"] = recipeId(category, index);
  source["provenance"]["catalogVersion"] = 1;
  auto& nodes = source["graph"]["nodes"] = Json::array();
  auto& edges = source["graph"]["edges"] = Json::array();
  auto& outputs = source["outputs"] = Json::object();
  auto& controls = source["exposedParameters"] = Json::array();
  const auto expose = [&](Parameter p) {
    controls.push_back({{"node", p.node}, {"key", p.key}, {"label", p.label},
      {"min", p.min}, {"max", p.max}, {"step", p.step}});
  };
  const auto channel = [&](const char* name, const char* id, const char* port) {
    outputs[name] = {{"node", id}, {"port", port}};
  };
  const auto add = [&](const char* id, Pattern pattern, unsigned scale, int row) {
    const auto& type = recipeContract(pattern);
    auto parameters = recipePattern(pattern, scale);
    if (pattern == Pattern::bricks || pattern == Pattern::flemish) {
      if (!std::any_of(nodes.begin(), nodes.end(), [](const Json& entry) { return entry["id"] == "wear"; }))
        nodes.push_back(node("wear", "GradientNoise", recipePattern(Pattern::gradient, 37), 0));
      edges.push_back(wire("wear", "value", id, "variation"));
    }
    if (pattern == Pattern::flow) {
      static constexpr unsigned liquidStyles[]{0, 1, 2, 1, 1, 2, 3, 3, 2, 1};
      parameters["style"] = category == 9 ? liquidStyles[index] : 3;
      if (source["animation"]["frames"] == 1) {
        source["animation"]["frames"] = 16;
        nodes.push_back(node("time", "Time", Json::object(), 0, 1));
      }
      edges.push_back(wire("time", "value", id, "phase"));
    }
    if (parameters.contains("period"))
      expose({row ? "Detail frequency" : "Pattern frequency", id, "period", 1,
        pattern == Pattern::flow ? 32.f : 64.f, 1});
    else if (parameters.contains("columns")) {
      const bool checker = pattern == Pattern::checker;
      expose({row ? "Detail columns" : "Pattern columns", id, "columns", checker ? 2.f : 1.f, 64, checker ? 2u : 1u});
    } else if (pattern == Pattern::rectangle)
      expose({row ? "Detail width" : "Pattern width", id, "width", .025f, .95f, 0});
    else if (pattern == Pattern::circle || pattern == Pattern::polygon)
      expose({row ? "Detail radius" : "Pattern radius", id, "radius", .015f, .475f, 0});
    else if (pattern == Pattern::digit)
      expose({row ? "Detail digit" : "Digit", id, "digit", 0, 9, 1});
    else if (pattern == Pattern::arrow)
      expose({row ? "Detail length" : "Arrow length", id, "length", .2f, .9f, 0});
    nodes.push_back(node(id, type.type, std::move(parameters), 1, row));
  };
  add("pattern", recipe.pattern, recipe.scale, 0);
  add("detail", recipe.detail, recipe.detailScale, 1);
  nodes.push_back(node("composite", recipeContract(recipe.composition).type,
    defaultParameters(recipe.composition), 2, 1));
  edges.push_back(wire("pattern", "value", "composite", "a"));
  edges.push_back(wire("detail", "value", "composite", "b"));
  nodes.push_back(node("surface", "ScalarMix", {{"weight", .64}}, 3));
  edges.push_back(wire("pattern", "value", "surface", "a"));
  edges.push_back(wire("composite", "value", "surface", "b"));
  const auto rgb = [](uint32_t hex, float alpha) {
    return Json{fromSrgb(float((hex >> 16) & 255) / 255), fromSrgb(float((hex >> 8) & 255) / 255),
      fromSrgb(float(hex & 255) / 255), alpha};
  };
  const bool decal = category == 10 && index != 0;
  nodes.push_back(node("pigment", "ColorRamp",
    {{"low", rgb(recipe.shadow, decal ? 0.f : 1.f)}, {"high", rgb(recipe.highlight, 1)}}, 4));
  edges.push_back(wire("surface", "value", "pigment", "value"));
  channel("baseColor", "pigment", "color");
  channel("height", "surface", "value");
  if (decal) channel("opacity", "surface", "value");
  const bool metal = category == 3 || category == 4;
  const bool liquid = category == 9;
  nodes.push_back(node("normal", "HeightToNormal", {{"strength", liquid ? .85 : 2.2}, {"invertY", false}}, 4, 1));
  nodes.push_back(node("ao", "HeightToAO", {{"strength", 1.8}}, 4, 2));
  nodes.push_back(node("roughness", "Constant", {{"value", metal ? .4 : liquid ? .25 : .74}}, 4, 3));
  edges.push_back(wire("surface", "value", "normal", "height"));
  edges.push_back(wire("surface", "value", "ao", "height"));
  channel("normal", "normal", "normal");
  channel("ao", "ao", "value");
  channel("roughness", "roughness", "value");
  if (metal) {
    nodes.push_back(node("metalness", "Constant", {{"value", category == 3 ? .86 : .6}}, 4, 4));
    channel("metalness", "metalness", "value");
    expose({"Metalness", "metalness", "value", 0, 1, 0});
  }
  if ((category == 4 && index >= 6) || (liquid && index >= 5) || (category == 11 && (index == 6 || index == 7))) {
    nodes.push_back(node("glow", "ColorRamp",
      {{"low", {0,0,0,1}}, {"high", rgb(recipe.highlight, 1)}}, 4, 5));
    edges.push_back(wire("composite", "value", "glow", "value"));
    channel("emissive", "glow", "color");
  }
  expose({"Detail influence", "surface", "weight", 0, 1, 0});
  expose({"Bump strength", "normal", "strength", 0, 8, 0});
  expose({"Roughness", "roughness", "value", 0, 1, 0});
  return doc;
}

struct RecipeVariation {
  const char* name;
  float detail, bump, roughness;
  unsigned scalePercent;
};

inline constexpr RecipeVariation curatedVariations[]{
  {"Clean and fine", .29f, .9f, .32f, 135},
  {"Deep relief", .74f, 3.6f, .61f, 78},
  {"Weathered", .89f, 2.7f, .91f, 115},
  {"Broad and soft", .46f, .5f, .69f, 63}
};

inline void applyCuratedVariation(Document& doc, const RecipeVariation& settings) {
  auto& surface = findNode(doc, "surface")["parameters"];
  auto& normal = findNode(doc, "normal")["parameters"];
  auto& roughness = findNode(doc, "roughness")["parameters"];
  surface["weight"] = settings.detail;
  normal["strength"] = settings.bump;
  roughness["value"] = settings.roughness;
  for (const auto& binding : doc.source["exposedParameters"]) {
    const auto key = binding["key"].get<std::string>();
    const auto id = binding["node"].get<std::string>();
    if ((id != "pattern" && id != "detail") || key == "digit") continue;
    auto& value = findNode(doc, id)["parameters"][key];
    const double low = binding["min"], high = binding["max"];
    const unsigned step = binding["step"];
    double scaled = value.get<double>() * settings.scalePercent / 100;
    if (step) {
      scaled = low + std::round((scaled - low) / step) * step;
      value = unsigned(std::clamp(scaled, low, high));
    } else value = std::clamp(scaled, low, high);
  }
}

struct TextureFamily {
  const char* id;
  const char* name;
  uint32_t shade, accent;
  RecipeVariation settings;
  std::array<std::array<unsigned, 2>, 8> recipes;
};

inline constexpr const char* familyRoles[]{
  "wall", "floor", "ceiling", "panel", "trim", "door", "damaged", "marking"
};

inline constexpr TextureFamily textureFamilies[]{
  {"gothic-fortress", "Gothic Fortress", 0x38383d, 0xb4a091, {"Stone", .68f, 2.9f, .82f, 92},
    {{{1,7},{1,5},{1,4},{1,8},{11,1},{11,5},{1,3},{10,3}}}},
  {"industrial-bunker", "Industrial Bunker", 0x313638, 0xa9a18b, {"Shop finish", .65f, 1.8f, .63f, 105},
    {{{4,0},{3,3},{4,8},{4,3},{11,0},{4,6},{3,2},{10,0}}}},
  {"retro-laboratory", "Retro Laboratory", 0x3a5354, 0xb6d0c2, {"Coated finish", .36f, 1.1f, .39f, 125},
    {{{2,2},{2,4},{4,9},{4,2},{11,4},{4,6},{2,1},{10,2}}}},
  {"brutalist-city", "Brutalist City", 0x373837, 0xa39b91, {"Masonry", .72f, 2.6f, .84f, 82},
    {{{0,1},{0,8},{0,7},{0,3},{11,9},{3,5},{0,9},{10,1}}}},
  {"abandoned-facility", "Abandoned Facility", 0x44403b, 0xb7a079, {"Weathered paint", .87f, 2.3f, .81f, 93},
    {{{2,0},{6,6},{2,9},{4,1},{11,8},{3,6},{3,9},{10,4}}}},
  {"alien-infestation", "Alien Infestation", 0x382a40, 0xa97c92, {"Organic finish", .72f, 2.45f, .51f, 108},
    {{{8,9},{8,3},{8,7},{8,4},{11,3},{8,8},{8,5},{9,6}}}},
};

inline Document familyMaterial(size_t index, size_t role, uint32_t root) {
  require(index < std::size(textureFamilies) && role < std::size(familyRoles), "family", "Unknown family member.");
  const auto& family = textureFamilies[index];
  const auto& [category, recipe] = family.recipes[role];
  const auto name = std::string(family.id) + "-" + familyRoles[role];
  auto doc = catalogPreset(category, recipe, name);
  applyCuratedVariation(doc, family.settings);
  doc.source["name"] = std::string(family.name) + " / " + familyRoles[role];
  doc.source["rootSeed"] = uint32_t(stableSeed(root, name));
  doc.source["provenance"]["family"] = {{"id", family.id}, {"role", familyRoles[role]}, {"seed", root}, {"version", 1}};
  auto& pigment = findNode(doc, "pigment")["parameters"];
  const auto tint = [&](const char* key, uint32_t hex, float weight) {
    auto rgba = pigment[key].get<Pixel>();
    for (unsigned channel = 0; channel < 3; ++channel) {
      const float target = fromSrgb(float((hex >> (16 - channel * 8)) & 255) / 255);
      rgba[channel] += weight * (target - rgba[channel]);
    }
    pigment[key] = rgba;
  };
  tint("low", family.shade, .72f);
  tint("high", family.accent, role == 7 ? .4f : .72f);
  return doc;
}

} // namespace ftebroom::textureStudio
