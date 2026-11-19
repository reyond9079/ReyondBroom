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

struct SurfaceFinish {
  GrainKind grain;
  WearKind wear;
  float density;
  float roughness;
  float relief;
  float wearContrast;
  uint32_t wearColor;
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
    {"board-formed", "Board-formed Concrete", Pattern::boardJoints, Pattern::streaks, Pattern::scalarMix, 8, 43, 0x626362, 0xa6a69b},
    {"cracked-slab", "Cracked Slab", Pattern::cracks, Pattern::fbm, Pattern::multiply, 6, 21, 0x292a2c, 0xb1a99b},
    {"chipped-panel", "Chipped Concrete Panel", Pattern::tiles, Pattern::worley, Pattern::minimum, 4, 44, 0x4c4946, 0xb4aba0},
    {"wet-bunker", "Wet Bunker", Pattern::fbm, Pattern::veins, Pattern::minimum, 16, 10, 0x252e2c, 0x66706b},
    {"aggregate", "Exposed Aggregate", Pattern::materialGrain, Pattern::white, Pattern::scalarMix, 32, 61, 0x58534d, 0xc3b8a9},
    {"patched", "Patched Concrete", Pattern::fbm, Pattern::ridged, Pattern::scalarMix, 9, 13, 0x626361, 0xb4b1a6},
    {"stained-ceiling", "Stained Concrete Ceiling", Pattern::gradient, Pattern::worley, Pattern::scalarMix, 8, 16, 0x77715f, 0xc8bc9e},
    {"expansion-floor", "Expansion-joint Floor", Pattern::tiles, Pattern::gradient, Pattern::scalarMix, 3, 29, 0x545350, 0x9e9791},
    {"reinforced-damage", "Reinforced Concrete Damage", Pattern::materialGrain, Pattern::grate, Pattern::scalarMix, 20, 13, 0x504c45, 0xa79887}
  },
  {
    {"running-brick", "Warm Running Brick", Pattern::bricks, Pattern::gradient, Pattern::multiply, 4, 43, 0x4d3025, 0xba704c},
    {"flemish-brick", "Flemish Brick", Pattern::flemish, Pattern::streaks, Pattern::scalarMix, 6, 22, 0x5b3627, 0xc88860},
    {"broken-brick", "Broken Brick", Pattern::bricks, Pattern::cracks, Pattern::minimum, 8, 13, 0x4d3331, 0xa8766b},
    {"rough-stone", "Rough Stonework", Pattern::stonework, Pattern::ridged, Pattern::minimum, 7, 10, 0x4b4b45, 0xada99c},
    {"ashlar", "Ashlar Blocks", Pattern::tiles, Pattern::veins, Pattern::difference, 4, 34, 0x797161, 0xc1bc98},
    {"cobblestone", "Rounded Cobblestone", Pattern::stonework, Pattern::worley, Pattern::multiply, 7, 18, 0x424548, 0xaaa08d},
    {"mortar-rubble", "Mortar and Rubble", Pattern::stonework, Pattern::cracks, Pattern::minimum, 18, 8, 0x4c4c43, 0xa5a994},
    {"gothic-block", "Gothic Block", Pattern::flemish, Pattern::rings, Pattern::minimum, 4, 11, 0x353841, 0x9f9ba3},
    {"basalt-blocks", "Basalt Blocks", Pattern::tiles, Pattern::ridged, Pattern::multiply, 9, 17, 0x292b31, 0x77747b},
    {"arch-trim", "Worn Arch Trim", Pattern::archBand, Pattern::cracks, Pattern::multiply, 11, 16, 0x504740, 0xb3a89a}
  },
  {
    {"peeling", "Peeling Plaster", Pattern::fbm, Pattern::ridged, Pattern::scalarMix, 7, 39, 0xb0a88a, 0xe4d9bb},
    {"cracked-paint", "Cracked Paint", Pattern::cracks, Pattern::gradient, Pattern::minimum, 24, 14, 0x56515a, 0xd8d1d4},
    {"square-ceramic", "Porcelain Squares", Pattern::tiles, Pattern::gradient, Pattern::maximum, 6, 41, 0x455d56, 0xb7ddd2},
    {"subway", "Subway Tile", Pattern::bricks, Pattern::veins, Pattern::maximum, 5, 20, 0x625b55, 0xeee4d7},
    {"mosaic", "Mosaic Tile", Pattern::tiles, Pattern::checker, Pattern::multiply, 24, 24, 0x426365, 0x8fc1b8},
    {"stained-grout", "Stained Grout", Pattern::tiles, Pattern::worley, Pattern::difference, 7, 31, 0x526766, 0xa3c0b1},
    {"painted-brick", "Painted Brick", Pattern::bricks, Pattern::streaks, Pattern::maximum, 7, 36, 0x475c5d, 0x8ababa},
    {"terrazzo", "Terrazzo", Pattern::materialGrain, Pattern::white, Pattern::scalarMix, 35, 47, 0x8e8075, 0xd8cfad},
    {"acoustic", "Acoustic Tile", Pattern::tiles, Pattern::rivets, Pattern::difference, 5, 42, 0x5f5a54, 0xd3ccb7},
    {"insulation", "Damaged Insulation", Pattern::streaks, Pattern::cracks, Pattern::maximum, 35, 9, 0x78593d, 0xe2ba7d}
  },
  {
    {"steel", "Brushed Steel", Pattern::materialGrain, Pattern::gradient, Pattern::scalarMix, 56, 21, 0x818487, 0xc0c4c4},
    {"oxidized", "Oxidized Iron", Pattern::materialGrain, Pattern::gradient, Pattern::scalarMix, 15, 32, 0x4d3d33, 0x8c6850},
    {"chipped-steel", "Painted Chipped Steel", Pattern::gradient, Pattern::streaks, Pattern::scalarMix, 7, 30, 0x3a565a, 0x779594},
    {"diamond-plate", "Diamond Plate", Pattern::diamond, Pattern::fibers, Pattern::minimum, 9, 23, 0x2b2c33, 0xbbc0c5},
    {"perforated-sheet", "Perforated Sheet", Pattern::rivets, Pattern::gradient, Pattern::difference, 25, 35, 0x151d20, 0xa0b4bb},
    {"corrugated-sheet", "Corrugated Sheet", Pattern::stripes, Pattern::fbm, Pattern::maximum, 14, 17, 0x303738, 0xa5b2a6},
    {"riveted", "Riveted Plate", Pattern::grate, Pattern::rivets, Pattern::maximum, 4, 9, 0x4c4841, 0x9c9a88},
    {"welded", "Welded Seam", Pattern::stripes, Pattern::streaks, Pattern::multiply, 1, 41, 0x5f5a58, 0xa8a5a0},
    {"cast-metal", "Cast Metal", Pattern::materialGrain, Pattern::white, Pattern::scalarMix, 24, 52, 0x494744, 0x8d8982},
    {"rust-flakes", "Rust Flakes", Pattern::materialGrain, Pattern::fbm, Pattern::scalarMix, 27, 28, 0x522f22, 0xb77b51}
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
    {"planks", "Timber Planks", Pattern::boardJoints, Pattern::fibers, Pattern::multiply, 6, 38, 0x584027, 0xbe9161},
    {"parquet", "Parquet", Pattern::checker, Pattern::streaks, Pattern::difference, 12, 32, 0x54301f, 0xb16d39},
    {"plywood", "Plywood", Pattern::materialGrain, Pattern::ridged, Pattern::minimum, 37, 12, 0x735331, 0xd1a977},
    {"end-grain", "End Grain", Pattern::materialGrain, Pattern::fbm, Pattern::scalarMix, 28, 23, 0x65432e, 0xd1ad7e},
    {"weathered", "Weathered Boards", Pattern::boardJoints, Pattern::cracks, Pattern::multiply, 8, 24, 0x49443e, 0x9c9083},
    {"painted-boards", "Painted Boards", Pattern::boardJoints, Pattern::worley, Pattern::maximum, 9, 26, 0x3b5257, 0x769098},
    {"burned", "Burned Wood", Pattern::cracks, Pattern::streaks, Pattern::minimum, 17, 46, 0x211e20, 0x68564d},
    {"bark", "Bark Slabs", Pattern::materialGrain, Pattern::veins, Pattern::multiply, 32, 10, 0x453020, 0x9d7658},
    {"crate", "Crate Panel", Pattern::rectangle, Pattern::streaks, Pattern::maximum, 32, 39, 0x634624, 0xb58a54},
    {"split", "Split Timber", Pattern::veins, Pattern::cracks, Pattern::minimum, 19, 6, 0x5a3822, 0xb58c6f}
  },
  {
    {"soil", "Loose Soil", Pattern::fbm, Pattern::worley, Pattern::difference, 17, 35, 0x3c2c24, 0x997256},
    {"gravel", "Mixed Gravel", Pattern::worley, Pattern::ridged, Pattern::multiply, 24, 11, 0x423d40, 0x9d8e81},
    {"sand", "Sand Ripples", Pattern::veins, Pattern::white, Pattern::scalarMix, 13, 70, 0x92724d, 0xdcc89a},
    {"mud", "Muddy Ground", Pattern::worley, Pattern::fbm, Pattern::minimum, 13, 9, 0x352821, 0x837251},
    {"cracked-earth", "Cracked Earth", Pattern::cracks, Pattern::ridged, Pattern::difference, 13, 22, 0x593f29, 0xbc8c63},
    {"asphalt", "Dense Asphalt", Pattern::materialGrain, Pattern::ridged, Pattern::minimum, 43, 23, 0x35353b, 0x6c6e76},
    {"damaged-asphalt", "Damaged Asphalt", Pattern::cracks, Pattern::white, Pattern::multiply, 11, 53, 0x282a2c, 0x777277},
    {"snow", "Snow Crust", Pattern::worley, Pattern::white, Pattern::minimum, 22, 52, 0xa9b4bb, 0xe9e9ed},
    {"leaf-litter", "Leaf Litter", Pattern::spots, Pattern::fibers, Pattern::difference, 19, 33, 0x4f392b, 0xa97f44},
    {"rock-scree", "Rock Scree", Pattern::cracks, Pattern::worley, Pattern::maximum, 34, 14, 0x535058, 0xa89e93}
  },
  {
    {"granite", "Speckled Granite", Pattern::materialGrain, Pattern::white, Pattern::scalarMix, 38, 52, 0x5c5e62, 0xcbbdba},
    {"stratified", "Stratified Rock", Pattern::materialGrain, Pattern::ridged, Pattern::scalarMix, 22, 14, 0x4e4a4c, 0xba9277},
    {"sandstone", "Layered Sandstone", Pattern::materialGrain, Pattern::ridged, Pattern::multiply, 34, 21, 0x695444, 0xc6a77d},
    {"marble", "Marble Veins", Pattern::materialGrain, Pattern::fbm, Pattern::scalarMix, 13, 14, 0x515558, 0xe5e6ec},
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
    {"water", "Woven Water Ripples", Pattern::flow, Pattern::gradient, Pattern::scalarMix, 4, 7, 0x214863, 0x6fa2b3},
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
    {"gothic-trim", "Gothic Trim", Pattern::archBand, Pattern::flemish, Pattern::maximum, 15, 4, 0x4d4645, 0xaa9687},
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

inline SurfaceFinish surfaceFinish(size_t category, size_t index) {
  using G = GrainKind;
  using W = WearKind;
  switch (category) {
  case 0:
    switch (index) {
    case 1: return {G::fibers, W::pits, .68f, .77f, .68f, .55f, 0x58554d};
    case 3: return {G::stone, W::chips, .92f, .82f, .78f, .86f, 0xc7b7a0};
    case 4: return {G::mineral, W::runoff, .93f, .52f, .37f, .72f, 0x202823};
    case 5: return {G::stone, W::pits, .72f, .87f, .92f, .52f, 0x585049};
    case 6: return {G::mineral, W::chips, .99f, .83f, .45f, .86f, 0x7a786e};
    case 7: return {G::mineral, W::deposits, .99f, .79f, .33f, .92f, 0x796342};
    case 8: return {G::mineral, W::scratches, .77f, .64f, .49f, .85f, 0xb3a99d};
    case 2:
    case 9: return {G::stone, W::chips, .96f, .85f, .87f, .83f, 0x696257};
    default: return {G::mineral, W::pits, .74f, .77f, .51f, .63f, 0x6d675e};
    }
  case 1:
    return {(index < 3 ? G::mineral : G::stone), (index == 2 || index == 7 ? W::chips : W::pits),
      index == 2 ? .98f : .77f, .85f, index < 3 ? .8f : .97f, .72f, index < 3 ? 0xcd8861u : 0xc0b6a3u};
  case 2:
    if (index == 0 || index == 1 || index == 6)
      return {G::mineral, W::chips, .98f, .69f, .61f, .95f, index == 6 ? 0xa96648u : 0x857964u};
    if (index == 7) return {G::stone, W::pits, .73f, .53f, .36f, .47f, 0xa29285};
    if (index >= 8) return {G::fibers, W::chips, .89f, .86f, .63f, .75f, 0x8d7757};
    return {G::polished, index == 5 ? W::deposits : W::scratches, .92f, .32f, .42f, .72f, 0xa7a38f};
  case 3:
    if (index == 1 || index == 9)
      return {G::stone, W::chips, .99f, .85f, .67f, .93f, index == 1 ? 0x312d29u : 0x593b2au};
    if (index == 2) return {G::brushed, W::chips, .99f, .52f, .44f, .95f, 0xb2a9a0};
    if (index == 8) return {G::stone, W::pits, .84f, .63f, .69f, .57f, 0x524c45};
    return {G::brushed, index == 5 || index == 6 ? W::runoff : W::scratches,
      .76f, index == 0 ? .29f : .46f, index == 0 ? .31f : .57f, .75f, index == 5 ? 0x665a4eu : 0xbeb9b1u};
  case 4:
    return {G::brushed, index == 9 ? W::scratches : W::chips, index == 9 ? .6f : .92f, .55f, .55f, .80f, 0x9b9182};
  case 5:
    if (index == 3) return {G::endGrain, W::scratches, .63f, .67f, .67f, .44f, 0xac8b68};
    if (index == 5) return {G::wood, W::chips, .98f, .64f, .64f, .92f, 0xb09a7c};
    if (index == 6) return {G::fibers, W::chips, .95f, .93f, .81f, .82f, 0x8a6954};
    if (index == 7) return {G::fibers, W::pits, .78f, .90f, .97f, .66f, 0x604733};
    return {G::wood, index == 4 ? W::deposits : index == 9 ? W::chips : W::scratches,
      .82f, index == 4 ? .84f : .64f, .75f, .78f, index == 4 ? 0x514438u : 0xbc9366u};
  case 6:
    return {index == 2 ? G::sand : G::stone, index == 3 || index == 8 ? W::deposits : W::pits,
      .83f, index == 3 ? .56f : .88f, .85f, .6f, index == 7 ? 0xafb4b4u : 0x3a322a};
  case 7:
    if (index == 3) return {G::marble, W::scratches, .55f, .34f, .35f, .40f, 0xb6b5b0};
    return {index == 4 ? G::polished : index == 1 || index == 2 ? G::bedded : G::stone,
      index == 5 || index == 6 ? W::deposits : W::pits,
      .84f, index == 3 || index == 4 ? .41f : .87f, .79f, .68f, index == 6 ? 0xa5a480u : 0x756858u};
  case 8:
    return {index == 1 || index == 7 ? G::fibers : G::mineral, W::pits, .72f, .53f, .68f, .5f, 0x43312a};
  case 9:
    return {G::polished, W::deposits, .18f, index == 0 || index == 4 ? .18f : .32f, .65f, .35f, 0x233326};
  case 10:
    if (index == 4 || index == 5)
      return {G::mineral, W::runoff, .92f, .64f, .26f, .87f, index == 4 ? 0x87472du : 0x454e40u};
    if (index == 8)
      return {G::mineral, W::deposits, 1.f, .67f, .26f, .86f, 0x7d674b};
    return {G::mineral, W::chips, .84f, .72f, .31f, .76f, 0x544e43};
  default:
    return {G::brushed, W::scratches, .79f, .53f, .59f, .66f, 0xaea392};
  }
}

inline Document catalogPreset(size_t category, size_t index, std::string id) {
  require(category < std::size(catalogRecipes) && index < std::size(catalogRecipes[0]),
    "preset", "Unknown catalog recipe.");
  const auto& recipe = catalogRecipes[category][index];
  const auto finish = surfaceFinish(category, index);
  auto doc = runningBrick(std::move(id));
  auto& source = doc.source;
  source["name"] = recipe.name;
  const auto preset = recipeId(category, index);
  source["provenance"]["preset"] = preset;
  const bool liquid = category == 9;
  const bool marking = category == 10;
  const bool activeDisplay = category == 11 && (index == 6 || index == 7);
  const bool naturalDetail = !liquid && !marking && !activeDisplay;
  source["provenance"]["catalogVersion"] = naturalDetail ? 3 : 2;
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
    if (pattern == Pattern::materialGrain) {
      parameters["kind"] = unsigned(finish.grain);
      if (naturalDetail) parameters["naturalDetail"] = 1;
    }
    if (pattern == Pattern::boardJoints) {
      parameters["seams"] = category == 0 ? 0 : 1;
      parameters["width"] = category == 0 ? .067 : .064;
      parameters["irregularity"] = category == 0 ? .37 : .70;
    }
    if (pattern == Pattern::stonework) {
      parameters["roundness"] = index == 5 ? .88 : .26;
      parameters["mortar"] = index == 6 ? .13 : .073;
      parameters["irregularity"] = index == 5 ? .83 : .67;
    }
    if (pattern == Pattern::stripes && category == 3 && index == 7) {
      parameters["duty"] = .11;
      parameters["feather"] = .042;
      parameters["slant"] = 0;
    }
    if (pattern == Pattern::tiles && category == 0 && index == 8) {
      parameters["grout"] = .032;
      parameters["bevel"] = .023;
      parameters["rows"] = 2;
    }
    if (pattern == Pattern::bricks || pattern == Pattern::flemish) {
      parameters["irregularity"] = category == 2 && index == 3 ? .16 : index == 2 ? 1.0 : .70;
      if (!std::any_of(nodes.begin(), nodes.end(), [](const Json& entry) { return entry["id"] == "wear"; }))
        nodes.push_back(node("wear", "GradientNoise", recipePattern(Pattern::gradient, 37), 0));
      edges.push_back(wire("wear", "value", id, "variation"));
    }
    if (pattern == Pattern::flow) {
      static constexpr unsigned liquidStyles[]{4, 1, 2, 1, 1, 2, 3, 3, 2, 1};
      parameters["style"] = category == 9 ? liquidStyles[index] : 3;
      if (source["animation"]["frames"] == 1) {
        source["animation"]["frames"] = 16;
        nodes.push_back(node("time", "Time", Json::object(), 0, 1));
      }
      edges.push_back(wire("time", "value", id, "phase"));
    }
    if (parameters.contains("period"))
      expose({row ? "Detail frequency" : "Pattern frequency", id, "period", pattern == Pattern::materialGrain ? 2.f : 1.f,
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
  const auto connect = [&](const char* from, const char* to, const char* port) {
    edges.push_back(wire(from, "value", to, port));
  };
  const auto mixScalar = [&](const char* id, const char* a, const char* b, float weight, int col, int row) {
    nodes.push_back(node(id, "ScalarMix", {{"weight", weight}}, col, row));
    connect(a, id, "a");
    connect(b, id, "b");
  };
  const auto constant = [&](const char* id, float value, int col, int row) {
    nodes.push_back(node(id, "Constant", {{"value", value}}, col, row));
  };
  const auto maskedScalar = [&](const char* id, const char* a, const char* b, const char* mask, int col, int row) {
    nodes.push_back(node(id, "ScalarBlend", Json::object(), col, row));
    connect(a, id, "a");
    connect(b, id, "b");
    connect(mask, id, "mask");
  };
  const auto combine = [&](const char* id, const char* type, const char* a, const char* b, int col, int row) {
    nodes.push_back(node(id, type, Json::object(), col, row));
    connect(a, id, "a");
    connect(b, id, "b");
  };
  const auto levels = [&](const char* id, const char* input, float low, float high, int col, int row) {
    nodes.push_back(node(id, "Levels", {{"low", low}, {"high", high}, {"gamma", 1}}, col, row));
    connect(input, id, "value");
  };
  Json grain = {{"kind", unsigned(finish.grain)}, {"period", std::min(recipe.detailScale, 60u)}};
  if (naturalDetail) grain["naturalDetail"] = 1;
  nodes.push_back(node("grain", "MaterialGrain", std::move(grain), 1, 2));
  const bool paintedSteel = category == 3 && index == 2;
  const unsigned wearFrequency = paintedSteel ? 19u : finish.wear == WearKind::chips ? 9u + recipe.scale % 10u :
    finish.wear == WearKind::deposits ? 8u : std::min(52u, recipe.detailScale);
  Json wear = {{"kind", unsigned(finish.wear)}, {"period", wearFrequency}, {"density", finish.density}};
  if (naturalDetail) wear["naturalDetail"] = 1;
  auto damage = node("damage", "SurfaceWear", std::move(wear), 1, 3);
  if (naturalDetail) damage["seedIdentity"] = preset + "-wear";
  nodes.push_back(std::move(damage));
  nodes.push_back(node("intact", "Invert", Json::object(), 2, 3));
  connect("damage", "intact", "value");
  const bool physicalJoint = recipe.pattern == Pattern::bricks || recipe.pattern == Pattern::flemish ||
    recipe.pattern == Pattern::tiles || recipe.pattern == Pattern::boardJoints || recipe.pattern == Pattern::panels ||
    recipe.pattern == Pattern::stonework || (category == 0 && index == 2);
  const bool gentleStructure = recipe.pattern == Pattern::materialGrain || recipe.pattern == Pattern::gradient ||
    recipe.pattern == Pattern::fbm || recipe.pattern == Pattern::ridged || recipe.pattern == Pattern::streaks;
  const bool woodBoards = category == 5 && recipe.pattern == Pattern::boardJoints;
  const bool stone = recipe.pattern == Pattern::stonework;
  const bool wetStone = category == 1 && index == 5;
  const bool peeledPlaster = category == 2 && (index == 0 || index == 1 || index == 6);
  const bool boardConcrete = category == 0 && index == 1;
  const float structuralWeight = category == 0 && index == 9 ? 0.f :
    recipe.pattern == Pattern::archBand ? .15f : stone ? .12f : gentleStructure ? .27f : physicalJoint ? .32f : .62f;
  mixScalar("structure", "pattern", "composite", liquid || activeDisplay ? .38f : structuralWeight, 3, 0);
  mixScalar("surface", "structure", "grain", liquid || activeDisplay ? .12f :
    woodBoards ? .47f : stone ? .22f : gentleStructure ? .62f : .40f, 4, 1);
  mixScalar("worn-relief", "surface", "intact", finish.relief * (liquid || marking ? .06f : .22f), 5, 1);
  const auto rgb = [](uint32_t hex, float alpha) {
    return Json{fromSrgb(float((hex >> 16) & 255) / 255), fromSrgb(float((hex >> 8) & 255) / 255),
      fromSrgb(float(hex & 255) / 255), alpha};
  };
  const bool decal = marking && index != 0;
  const bool tilePigment = category == 2 && index == 4;
  const bool cementJoint = ((category == 1 || category == 2) && physicalJoint) ||
    (category == 0 && (index == 1 || index == 3 || index == 8));
  const bool firedClay = category == 1 && index < 3;
  const bool wearDecal = decal && (index == 4 || index == 5 || index == 8);
  if (firedClay || stone || woodBoards || boardConcrete)
    levels("piece-tone", "pattern", firedClay ? .60f : stone ? .59f : .78f,
      firedClay ? .91f : stone ? .92f : .99f, 3, 3);
  const bool pieceTone = firedClay || stone || woodBoards || boardConcrete;
  const char* tonalStructure = tilePigment ? "detail" : pieceTone ? "piece-tone" : "structure";
  mixScalar("tone", "grain", tonalStructure, liquid || marking || activeDisplay || tilePigment ? .90f :
    firedClay ? .61f : stone ? .47f : woodBoards ? .25f : boardConcrete ? .38f :
    finish.grain == GrainKind::marble ? .14f : gentleStructure || cementJoint ? .22f : category == 5 ? .28f : .48f, 4, 0);
  nodes.push_back(node("pigment", "ColorRamp",
    {{"low", rgb(recipe.shadow, 1)}, {"high", rgb(recipe.highlight, 1)}}, 5, 0));
  connect("tone", "pigment", "value");
  const auto blend = [&](const char* id, const char* base, const char* top, const char* mask, float opacity, int row) {
    nodes.push_back(node(id, "Blend", {{"opacity", opacity}, {"enabled", true}, {"mode", 0}}, 7, row));
    edges.push_back(wire(base, "color", id, "base"));
    edges.push_back(wire(top, "color", id, "top"));
    connect(mask, id, "mask");
  };
  const char* colorLayer = "pigment";
  if (firedClay || woodBoards || boardConcrete) {
    auto age = node("age", "SurfaceWear", {{"kind", unsigned(WearKind::deposits)},
      {"period", firedClay ? 14u : 10u}, {"density", .85}, {"naturalDetail", 1}}, 1, 6);
    age["seedIdentity"] = preset + "-age";
    nodes.push_back(std::move(age));
    const auto low = firedClay ? 0x655b4du : woodBoards ? 0x4d4337u : 0x5d5d54u;
    const auto high = firedClay ? 0xc5a589u : woodBoards ? 0xa09380u : 0xb1a99au;
    nodes.push_back(node("aged-pigment", "ColorRamp",
      {{"low", rgb(low, 1)}, {"high", rgb(high, 1)}}, 5, 8));
    connect("grain", "aged-pigment", "value");
    blend("with-age", colorLayer, "aged-pigment", "age", woodBoards ? .76f : .67f, 8);
    colorLayer = "with-age";
  }
  if (cementJoint) {
    const uint32_t mortarLow = wetStone ? 0x3a362bu : category == 0 ? 0x484740u :
      category == 1 && index >= 3 ? 0x454238u : 0x51483cu;
    const uint32_t mortarHigh = wetStone ? 0x676252u : category == 0 ? 0x77756au :
      category == 1 && index >= 3 ? 0x7c7366u : 0x978875u;
    nodes.push_back(node("mortar", "ColorRamp", {{"low", rgb(mortarLow, 1)}, {"high", rgb(mortarHigh, 1)}}, 5, 2));
    connect("grain", "mortar", "value");
    nodes.push_back(node("grooves", "Invert", Json::object(), 2, 4));
    connect("pattern", "grooves", "value");
    levels("joint-mask", "grooves", stone ? .68f : .57f, stone ? .95f : .90f, 3, 4);
    blend("with-joints", colorLayer, "mortar", "joint-mask", 1, 0);
    colorLayer = "with-joints";
  }
  const bool layeredWear = paintedSteel || peeledPlaster;
  if (layeredWear) {
    const uint32_t edge = paintedSteel ? 0x58402eu : 0x74614cu;
    const uint32_t substrateLow = paintedSteel ? 0x343839u : 0x847662u;
    const uint32_t substrateHigh = paintedSteel ? 0x939498u : 0xc1ac8fu;
    nodes.push_back(node("wear-edge", "ConstantColor", {{"rgba", rgb(edge, 1)}}, 5, 9));
    levels("wear-halo", "damage", .12f, .52f, 3, 5);
    levels("open-damage", "damage", .48f, .89f, 4, 6);
    blend("with-wear-edge", colorLayer, "wear-edge", "wear-halo", paintedSteel ? .78f : .45f, 9);
    colorLayer = "with-wear-edge";
    nodes.push_back(node("worn-pigment", "ColorRamp",
      {{"low", rgb(substrateLow, 1)}, {"high", rgb(substrateHigh, 1)}}, 5, 3));
    connect("grain", "worn-pigment", "value");
  } else {
    nodes.push_back(node("worn-pigment", "ConstantColor", {{"rgba", rgb(finish.wearColor, 1)}}, 5, 3));
  }
  const float wearContrast = firedClay ? .28f : stone ? .32f : woodBoards ? .44f : finish.wearContrast;
  blend("with-wear", colorLayer, "worn-pigment", layeredWear ? "open-damage" : "damage", wearContrast, 1);
  colorLayer = "with-wear";
  if (category == 0 && index == 9) {
    combine("exposed-rebar", "Multiply", "detail", "damage", 4, 3);
    nodes.push_back(node("rebar-color", "ConstantColor", {{"rgba", rgb(0x635143, 1)}}, 5, 5));
    blend("with-rebar", colorLayer, "rebar-color", "exposed-rebar", .93f, 2);
    colorLayer = "with-rebar";
  }
  if (category == 4 || (category == 3 && index == 2) || (category == 5 && index == 5)) {
    nodes.push_back(node("scuffs", "SurfaceWear", {{"kind", unsigned(WearKind::scratches)},
      {"period", std::min(36u, recipe.detailScale)}, {"density", .68}}, 1, 5));
    nodes.push_back(node("scuff-pigment", "ConstantColor", {{"rgba", rgb(0xbdb29e, 1)}}, 5, 6));
    blend("with-scuffs", colorLayer, "scuff-pigment", "scuffs", .48f, 6);
    colorLayer = "with-scuffs";
  }
  if (decal) {
    const char* mask = "pattern";
    if (recipe.pattern == Pattern::cracks) {
      nodes.push_back(node("crack-mask", "Invert", Json::object(), 3, 2));
      connect("pattern", "crack-mask", "value");
      mask = "crack-mask";
    } else if (wearDecal) {
      mask = "damage";
    }
    if (!wearDecal) {
      combine("marking-opacity", "Multiply", mask, "intact", 6, 2);
      mask = "marking-opacity";
    }
    nodes.push_back(node("transparent", "ConstantColor", {{"rgba", {0, 0, 0, 0}}}, 6, 3));
    blend("decal", "transparent", colorLayer, mask, 1, 2);
    channel("baseColor", "decal", "color");
    channel("opacity", mask, "value");
  } else channel("baseColor", colorLayer, "color");
  channel("height", "worn-relief", "value");
  const bool metal = category == 3 || category == 4;
  nodes.push_back(node("normal", "HeightToNormal", {{"strength", .65f + 1.7f * finish.relief}, {"invertY", false}}, 7, 3));
  nodes.push_back(node("ao", "HeightToAO", {{"strength", .8f + finish.relief}}, 7, 4));
  constant("roughness", wetStone ? .38f : finish.roughness, 4, 4);
  mixScalar("micro-roughness", "roughness", "grain", metal ? .22f : .16f, 5, 4);
  if (naturalDetail) {
    const bool damp = category == 0 && index == 4;
    constant("substrate-roughness", paintedSteel ? .38f : damp ? .19f : .89f, 5, 11);
    const char* mask = layeredWear ? "open-damage" : "damage";
    if (wetStone) {
      mask = "joint-mask";
    } else if (paintedSteel) {
      constant("oxidized-roughness", .83f, 5, 10);
      maskedScalar("weathered-roughness", "micro-roughness", "oxidized-roughness", "wear-halo", 6, 6);
    }
    maskedScalar("worn-roughness", paintedSteel ? "weathered-roughness" : "micro-roughness",
      "substrate-roughness", mask, 6, 4);
  } else {
    mixScalar("worn-roughness", "micro-roughness", "damage", .22f, 6, 4);
  }
  connect("worn-relief", "normal", "height");
  connect("worn-relief", "ao", "height");
  channel("normal", "normal", "normal");
  channel("ao", "ao", "value");
  channel("roughness", "worn-roughness", "value");
  if (metal) {
    const bool painted = category == 4 || index == 2;
    const bool oxidized = category == 3 && (index == 1 || index == 9);
    nodes.push_back(node("metalness", "Constant", {{"value", painted ? .04 : oxidized ? .21 : .94}}, 4, 5));
    if (painted || oxidized) {
      constant("substrate-metalness", painted ? .96f : .015f, 5, 7);
      maskedScalar("worn-metalness", "metalness", "substrate-metalness",
        paintedSteel ? "open-damage" : "damage", 6, 5);
    } else {
      mixScalar("worn-metalness", "metalness", "intact", .14f, 6, 5);
    }
    channel("metalness", "worn-metalness", "value");
    expose({"Metalness", "metalness", "value", 0, 1, 0});
  }
  if ((category == 4 && index >= 6) || (liquid && index >= 5) || (category == 11 && (index == 6 || index == 7))) {
    nodes.push_back(node("glow", "ColorRamp",
      {{"low", {0,0,0,1}}, {"high", rgb(recipe.highlight, 1)}}, 4, 5));
    edges.push_back(wire("composite", "value", "glow", "value"));
    channel("emissive", "glow", "color");
  }
  expose({"Detail influence", "surface", "weight", 0, 1, 0});
  expose({"Wear density", "damage", "density", 0, 1, 0});
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
  auto& damage = findNode(doc, "damage")["parameters"]["density"];
  damage = std::clamp(damage.get<float>() + .36f * (settings.detail - .65f), 0.f, 1.f);
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
  doc.source["provenance"]["family"] = {{"id", family.id}, {"role", familyRoles[role]}, {"seed", root}, {"version", 2}};
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
