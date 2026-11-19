// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "presets.h"
#include "catalog_stains.h"

namespace ftebroom::textureStudio {

using Pattern = Kernel;

struct MaterialRecipe {
  const char* id;
  const char* name;
  Pattern pattern, detail, composition;
  unsigned scale, detailScale;
  uint32_t shadow, highlight;
  unsigned surfaceKind = 0;
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
    {"poured", "Poured Concrete", Pattern::fbm, Pattern::gradient, Pattern::scalarMix, 6, 36, 0x62615d, 0xa49e93},
    {"board-formed", "Board-formed Concrete", Pattern::boardJoints, Pattern::streaks, Pattern::scalarMix, 8, 43, 0x62635f, 0xa29e91},
    {"cracked-slab", "Cracked Slab", Pattern::cracks, Pattern::fbm, Pattern::multiply, 6, 21, 0x292a2c, 0xb1a99b},
    {"chipped-panel", "Chipped Concrete Panel", Pattern::tiles, Pattern::worley, Pattern::minimum, 4, 44, 0x615b54, 0xaaa294},
    {"wet-bunker", "Wet Bunker", Pattern::fbm, Pattern::veins, Pattern::minimum, 16, 10, 0x303330, 0x666a60},
    {"aggregate", "Exposed Aggregate", Pattern::aggregate, Pattern::white, Pattern::scalarMix, 25, 61, 0x45403a, 0xc5b6a4, unsigned(AggregateKind::exposedConcrete)},
    {"patched", "Patched Concrete", Pattern::fbm, Pattern::ridged, Pattern::scalarMix, 9, 13, 0x626361, 0xb4b1a6},
    {"stained-ceiling", "Stained Concrete Ceiling", Pattern::gradient, Pattern::worley, Pattern::scalarMix, 8, 16, 0x787269, 0xc7bbaa},
    {"expansion-floor", "Expansion-joint Floor", Pattern::tiles, Pattern::gradient, Pattern::scalarMix, 3, 29, 0x545350, 0x9e9791},
    {"reinforced-damage", "Reinforced Concrete Damage", Pattern::installedSurface, Pattern::grate, Pattern::scalarMix, 4, 13, 0x3d352d, 0xb6ac99, unsigned(InstalledKind::spalledConcrete)}
  },
  {
    {"running-brick", "Warm Running Brick", Pattern::bricks, Pattern::gradient, Pattern::multiply, 6, 43, 0x272423, 0xb87950},
    {"flemish-brick", "Flemish Brick", Pattern::flemish, Pattern::streaks, Pattern::scalarMix, 6, 22, 0x704939, 0xb38b72},
    {"broken-brick", "Broken Brick", Pattern::bricks, Pattern::cracks, Pattern::minimum, 6, 13, 0x4e3e35, 0xbb896d},
    {"rough-stone", "Rough Stonework", Pattern::stonework, Pattern::ridged, Pattern::minimum, 7, 10, 0x4d4d46, 0xa29d8e},
    {"ashlar", "Ashlar Blocks", Pattern::installedSurface, Pattern::veins, Pattern::difference, 6, 34, 0x736b60, 0xd1c3ad, unsigned(InstalledKind::limestone)},
    {"cobblestone", "Rounded Cobblestone", Pattern::stonework, Pattern::worley, Pattern::multiply, 7, 18, 0x49453c, 0x8d8575},
    {"mortar-rubble", "Mortar and Rubble", Pattern::stonework, Pattern::cracks, Pattern::minimum, 18, 8, 0x4e5048, 0xa1a294},
    {"gothic-block", "Gothic Block", Pattern::flemish, Pattern::rings, Pattern::minimum, 4, 11, 0x424343, 0x8e8c83},
    {"basalt-blocks", "Basalt Blocks", Pattern::installedSurface, Pattern::ridged, Pattern::multiply, 9, 17, 0x282826, 0x888680, unsigned(InstalledKind::basalt)},
    {"arch-trim", "Worn Arch Trim", Pattern::archBand, Pattern::cracks, Pattern::multiply, 11, 16, 0x504740, 0xb3a89a}
  },
  {
    {"peeling", "Peeling Plaster", Pattern::fbm, Pattern::ridged, Pattern::scalarMix, 7, 39, 0xa49b89, 0xd5c9b8},
    {"cracked-paint", "Cracked Paint", Pattern::cracks, Pattern::gradient, Pattern::minimum, 24, 14, 0x706864, 0xc5bab0},
    {"square-ceramic", "Porcelain Squares", Pattern::tiles, Pattern::gradient, Pattern::maximum, 6, 41, 0x51675e, 0xaebfb3},
    {"subway", "Subway Tile", Pattern::bricks, Pattern::veins, Pattern::maximum, 5, 20, 0x625b55, 0xeee4d7},
    {"mosaic", "Mosaic Tile", Pattern::installedSurface, Pattern::checker, Pattern::multiply, 14, 24, 0x3b504a, 0x8fa499, unsigned(InstalledKind::mosaic)},
    {"stained-grout", "Stained Grout", Pattern::tiles, Pattern::worley, Pattern::difference, 7, 31, 0x5c6d69, 0xa9b8a9},
    {"painted-brick", "Painted Brick", Pattern::bricks, Pattern::streaks, Pattern::maximum, 7, 36, 0x576e63, 0xa1b1a4},
    {"terrazzo", "Terrazzo", Pattern::aggregate, Pattern::white, Pattern::scalarMix, 19, 47, 0x393637, 0xe0d2b8, unsigned(AggregateKind::terrazzo)},
    {"acoustic", "Acoustic Tile", Pattern::tiles, Pattern::rivets, Pattern::difference, 5, 42, 0x747068, 0xc8c1b5},
    {"insulation", "Damaged Insulation", Pattern::streaks, Pattern::cracks, Pattern::maximum, 35, 9, 0x806749, 0xbda47b}
  },
  {
    {"steel", "Brushed Steel", Pattern::materialGrain, Pattern::gradient, Pattern::scalarMix, 56, 21, 0x888c89, 0xb6bcba},
    {"oxidized", "Oxidized Iron", Pattern::materialGrain, Pattern::gradient, Pattern::scalarMix, 15, 32, 0x46392f, 0x83604c},
    {"chipped-steel", "Painted Chipped Steel", Pattern::gradient, Pattern::streaks, Pattern::scalarMix, 7, 30, 0x45595c, 0x78918b},
    {"diamond-plate", "Diamond Plate", Pattern::diamond, Pattern::fibers, Pattern::minimum, 9, 23, 0x515551, 0xa1a9a6},
    {"perforated-sheet", "Perforated Sheet", Pattern::rivets, Pattern::gradient, Pattern::difference, 25, 35, 0x303332, 0xa1a8a4},
    {"corrugated-sheet", "Corrugated Sheet", Pattern::stripes, Pattern::fbm, Pattern::maximum, 14, 17, 0x626963, 0xb7bdb9},
    {"riveted", "Riveted Plate", Pattern::formedMetal, Pattern::rivets, Pattern::maximum, 9, 9, 0x3e3d34, 0xa29d8c, unsigned(MetalForm::riveted)},
    {"welded", "Welded Seam", Pattern::formedMetal, Pattern::streaks, Pattern::multiply, 5, 41, 0x464542, 0xa6a7a4, unsigned(MetalForm::weld)},
    {"cast-metal", "Cast Metal", Pattern::materialGrain, Pattern::white, Pattern::scalarMix, 24, 52, 0x494744, 0x8d8982},
    {"rust-flakes", "Rust Flakes", Pattern::materialGrain, Pattern::fbm, Pattern::scalarMix, 27, 28, 0x412c1f, 0x9b633e}
  },
  {
    {"modular-wall", "Modular Wall", Pattern::panels, Pattern::grate, Pattern::difference, 3, 20, 0x2c3943, 0x6c8d9d},
    {"service-hatch", "Service Hatch", Pattern::formedMetal, Pattern::rivets, Pattern::maximum, 11, 6, 0x2e3435, 0xa4aaa6, unsigned(MetalForm::hatch)},
    {"recessed-panel", "Recessed Panel", Pattern::rectangle, Pattern::grate, Pattern::minimum, 27, 11, 0x232830, 0x616c77},
    {"vent-grille", "Vent Grille", Pattern::formedMetal, Pattern::streaks, Pattern::minimum, 9, 44, 0x242928, 0xa39f91, unsigned(MetalForm::slotted)},
    {"conduit-plate", "Conduit Plate", Pattern::formedMetal, Pattern::rectangle, Pattern::maximum, 4, 21, 0x383831, 0xa1aaa3, unsigned(MetalForm::conduit)},
    {"cable-tray", "Cable Tray", Pattern::formedMetal, Pattern::grate, Pattern::minimum, 7, 19, 0x343633, 0xb5bab6, unsigned(MetalForm::tray)},
    {"access-door", "Access Door", Pattern::formedMetal, Pattern::arrow, Pattern::difference, 5, 22, 0x343b31, 0x8c9b7e, unsigned(MetalForm::hatch)},
    {"armored-floor", "Armored Floor", Pattern::tiles, Pattern::rivets, Pattern::maximum, 6, 33, 0x2c2929, 0x7a8285},
    {"ceiling-module", "Ceiling Module", Pattern::formedMetal, Pattern::grate, Pattern::maximum, 14, 23, 0x2c302e, 0xb6b9b2, unsigned(MetalForm::slotted)},
    {"laboratory", "Laboratory Panel", Pattern::tiles, Pattern::streaks, Pattern::scalarMix, 4, 45, 0x606e77, 0xc5d4d6}
  },
  {
    {"planks", "Timber Planks", Pattern::boardJoints, Pattern::fibers, Pattern::multiply, 6, 38, 0x66482c, 0xbb986e},
    {"parquet", "Parquet", Pattern::installedSurface, Pattern::streaks, Pattern::difference, 4, 32, 0x594231, 0xbd9e72, unsigned(InstalledKind::parquet)},
    {"plywood", "Plywood", Pattern::materialGrain, Pattern::ridged, Pattern::minimum, 37, 12, 0x876c4c, 0xc5a782},
    {"end-grain", "End Grain", Pattern::materialGrain, Pattern::fbm, Pattern::scalarMix, 28, 23, 0x775637, 0xc3a17a},
    {"weathered", "Weathered Boards", Pattern::boardJoints, Pattern::cracks, Pattern::multiply, 8, 24, 0x433729, 0x867662},
    {"painted-boards", "Painted Boards", Pattern::boardJoints, Pattern::worley, Pattern::maximum, 9, 26, 0x445a57, 0x83918a},
    {"burned", "Burned Wood", Pattern::cracks, Pattern::streaks, Pattern::minimum, 17, 46, 0x211e20, 0x68564d},
    {"bark", "Bark Slabs", Pattern::fibrousSurface, Pattern::veins, Pattern::multiply, 14, 10, 0x322b25, 0x8b7864, unsigned(FibrousKind::bark)},
    {"crate", "Crate Panel", Pattern::installedSurface, Pattern::streaks, Pattern::maximum, 6, 39, 0x60452e, 0xc6a778, unsigned(InstalledKind::crate)},
    {"split", "Split Timber", Pattern::veins, Pattern::cracks, Pattern::minimum, 19, 6, 0x694d37, 0xa68a6d}
  },
  {
    {"soil", "Loose Soil", Pattern::fbm, Pattern::worley, Pattern::difference, 17, 35, 0x3c2c24, 0x997256},
    {"gravel", "Mixed Gravel", Pattern::aggregate, Pattern::ridged, Pattern::multiply, 19, 11, 0x3e372f, 0xb7aa98, unsigned(AggregateKind::gravel)},
    {"sand", "Sand Ripples", Pattern::aggregate, Pattern::white, Pattern::scalarMix, 9, 70, 0x8a7657, 0xd6c6aa, unsigned(AggregateKind::windSand)},
    {"mud", "Muddy Ground", Pattern::worley, Pattern::fbm, Pattern::minimum, 13, 9, 0x352821, 0x837251},
    {"cracked-earth", "Cracked Earth", Pattern::cracks, Pattern::ridged, Pattern::difference, 13, 22, 0x593f29, 0xbc8c63},
    {"asphalt", "Dense Asphalt", Pattern::aggregate, Pattern::ridged, Pattern::minimum, 51, 23, 0x242424, 0x777674, unsigned(AggregateKind::asphalt)},
    {"damaged-asphalt", "Damaged Asphalt", Pattern::aggregate, Pattern::white, Pattern::multiply, 41, 53, 0x242322, 0x77716b, unsigned(AggregateKind::damagedAsphalt)},
    {"snow", "Snow Crust", Pattern::worley, Pattern::white, Pattern::minimum, 22, 52, 0xadb6b6, 0xeeece5},
    {"leaf-litter", "Leaf Litter", Pattern::leafLitter, Pattern::fibers, Pattern::difference, 12, 33, 0x332a23, 0xc09658},
    {"rock-scree", "Rock Scree", Pattern::aggregate, Pattern::worley, Pattern::maximum, 10, 14, 0x363232, 0xb8aa9c, unsigned(AggregateKind::scree)}
  },
  {
    {"granite", "Speckled Granite", Pattern::materialGrain, Pattern::white, Pattern::scalarMix, 38, 52, 0x626261, 0xc6bdb2},
    {"stratified", "Stratified Rock", Pattern::materialGrain, Pattern::ridged, Pattern::scalarMix, 22, 14, 0x4e4a4c, 0xba9277},
    {"sandstone", "Layered Sandstone", Pattern::materialGrain, Pattern::ridged, Pattern::multiply, 34, 21, 0x695444, 0xc6a77d},
    {"marble", "Marble Veins", Pattern::fibrousSurface, Pattern::fbm, Pattern::scalarMix, 3, 14, 0x5c5b5b, 0xf0eeea, unsigned(FibrousKind::marble)},
    {"ice", "Fractured Ice", Pattern::cracks, Pattern::veins, Pattern::maximum, 12, 19, 0x64828b, 0xc5d3d0},
    {"moss", "Cushion Moss", Pattern::aggregate, Pattern::fbm, Pattern::maximum, 17, 29, 0x2e321c, 0x6f8149, unsigned(AggregateKind::moss)},
    {"lichen", "Lichen Patches", Pattern::worley, Pattern::cracks, Pattern::maximum, 31, 19, 0x4c5040, 0xa0a085},
    {"roots", "Exposed Roots", Pattern::fibrousSurface, Pattern::streaks, Pattern::difference, 4, 25, 0x302922, 0x947454, unsigned(FibrousKind::roots)},
    {"cliff", "Cliff Face", Pattern::ridged, Pattern::cracks, Pattern::difference, 11, 19, 0x474349, 0xa99b8a},
    {"salt", "Salt Crust", Pattern::cracks, Pattern::white, Pattern::scalarMix, 19, 54, 0x837c7c, 0xe4dbca}
  },
  {
    {"pores", "Skin Pores", Pattern::worley, Pattern::fbm, Pattern::scalarMix, 42, 16, 0x724739, 0xc89276},
    {"flesh", "Flesh Veins", Pattern::fibrousSurface, Pattern::cracks, Pattern::difference, 4, 26, 0x784d50, 0xca9690, unsigned(FibrousKind::veins)},
    {"scar", "Scar Tissue", Pattern::fibrousSurface, Pattern::veins, Pattern::maximum, 6, 14, 0x83524e, 0xd9a998, unsigned(FibrousKind::scar)},
    {"bone", "Bone Plates", Pattern::fibrousSurface, Pattern::cracks, Pattern::multiply, 5, 18, 0x5d4c3e, 0xd5c4a2, unsigned(FibrousKind::scutes)},
    {"chitin", "Chitin", Pattern::rings, Pattern::cracks, Pattern::difference, 20, 23, 0x3a2829, 0x794435},
    {"infestation", "Root Infestation", Pattern::fibrousSurface, Pattern::worley, Pattern::multiply, 7, 22, 0x2e2528, 0xa99b77, unsigned(FibrousKind::roots)},
    {"slime-membrane", "Slime Membrane", Pattern::worley, Pattern::ridged, Pattern::difference, 19, 31, 0x2e5048, 0x7ba581},
    {"tendons", "Tendons", Pattern::fibrousSurface, Pattern::streaks, Pattern::maximum, 16, 41, 0x624b47, 0xc5a58f, unsigned(FibrousKind::tendon)},
    {"cells", "Alien Cells", Pattern::fibrousSurface, Pattern::spots, Pattern::difference, 8, 12, 0x322942, 0xb089ad, unsigned(FibrousKind::cells)},
    {"biomechanical", "Biomechanical Wall", Pattern::fibrousSurface, Pattern::veins, Pattern::difference, 7, 15, 0x211b20, 0x897478, unsigned(FibrousKind::ribs)}
  },
  {
    {"water", "Woven Water Ripples", Pattern::flow, Pattern::gradient, Pattern::scalarMix, 4, 7, 0x214863, 0x6fa2b3},
    {"toxic", "Toxic Fluid", Pattern::flow, Pattern::worley, Pattern::multiply, 8, 17, 0x253924, 0x9ac04d},
    {"lava", "Veined Lava Crust", Pattern::flow, Pattern::cracks, Pattern::difference, 6, 12, 0x2e2622, 0xbf722e},
    {"sludge", "Deep Sludge", Pattern::flow, Pattern::ridged, Pattern::minimum, 5, 12, 0x323f2d, 0x6e7948},
    {"oil", "Oil Film", Pattern::flow, Pattern::streaks, Pattern::maximum, 5, 35, 0x282235, 0xa17998},
    {"acid", "Acid Bubbles", Pattern::flow, Pattern::rivets, Pattern::difference, 7, 21, 0x315047, 0xb5dd88},
    {"field", "Energy Field", Pattern::flow, Pattern::checker, Pattern::maximum, 11, 16, 0x283b72, 0x5db5c6},
    {"plasma", "Plasma Grid", Pattern::flow, Pattern::grate, Pattern::scalarMix, 10, 12, 0x492b55, 0xbb81dd},
    {"molten", "Molten Metal", Pattern::flow, Pattern::fbm, Pattern::minimum, 4, 20, 0x421f1a, 0xd59742},
    {"flowing", "Flowing Slime", Pattern::flow, Pattern::veins, Pattern::difference, 6, 17, 0x36431b, 0x79b056}
  },
  {
    {"hazard", "Industrial Hazard Stripes", Pattern::stripes, Pattern::cracks, Pattern::minimum, 8, 27, 0x232424, 0xeab944},
    {"number", "Room Number", Pattern::digit, Pattern::streaks, Pattern::multiply, 7, 28, 0x303238, 0xe6e4cb},
    {"arrow", "Direction Arrow", Pattern::arrow, Pattern::gradient, Pattern::minimum, 15, 26, 0x263640, 0xa9d8d8},
    {"warning", "Warning Panel", Pattern::rectangle, Pattern::stripes, Pattern::multiply, 20, 10, 0x30342e, 0xf0c351},
    {"rust-drip", "Rust Drip", Pattern::streaks, Pattern::worley, Pattern::minimum, 51, 16, 0x573320, 0xb77f54},
    {"water-streak", "Water Streak", Pattern::streaks, Pattern::gradient, Pattern::multiply, 16, 20, 0x3e4645, 0x5f6d5d},
    {"impact", "Impact Mark", Pattern::circle, Pattern::cracks, Pattern::minimum, 13, 15, 0x3a383b, 0x838089},
    {"crack-overlay", "Crack Overlay", Pattern::fibrousSurface, Pattern::veins, Pattern::minimum, 5, 11, 0x161412, 0x494039, unsigned(FibrousKind::fracture)},
    {"leak", "Leak Stain", Pattern::worley, Pattern::streaks, Pattern::multiply, 9, 34, 0x484238, 0x82715f},
    {"grime", "Grime Edge", Pattern::fibrousSurface, Pattern::worley, Pattern::difference, 11, 25, 0x273025, 0x68704f, unsigned(FibrousKind::grime)}
  },
  {
    {"industrial-trim", "Industrial Trim", Pattern::formedMetal, Pattern::streaks, Pattern::maximum, 6, 30, 0x353d3b, 0x9ba8a0, unsigned(MetalForm::slotted)},
    {"gothic-trim", "Gothic Trim", Pattern::installedSurface, Pattern::flemish, Pattern::maximum, 22, 4, 0x736654, 0xb6a189, unsigned(InstalledKind::voussoirs)},
    {"pipe-border", "Pipe Border", Pattern::formedMetal, Pattern::rivets, Pattern::multiply, 5, 10, 0x313838, 0x9fa9a7, unsigned(MetalForm::pipes)},
    {"cable-border", "Cable Border", Pattern::formedMetal, Pattern::grate, Pattern::multiply, 5, 8, 0x29282a, 0x998d78, unsigned(MetalForm::cables)},
    {"floor-border", "Floor Border", Pattern::stripes, Pattern::tiles, Pattern::minimum, 3, 12, 0x4b4444, 0xaa9e8c},
    {"door-frame", "Door Frame", Pattern::rectangle, Pattern::tiles, Pattern::difference, 18, 6, 0x343e46, 0x8798a0},
    {"terminal", "Terminal Display", Pattern::digit, Pattern::flow, Pattern::multiply, 2, 20, 0x293235, 0x86ceab},
    {"status", "Status Lights", Pattern::rivets, Pattern::flow, Pattern::multiply, 8, 6, 0x344137, 0x90b955},
    {"grille-strip", "Grille Strip", Pattern::formedMetal, Pattern::stripes, Pattern::difference, 18, 7, 0x33312f, 0xafa9a1, unsigned(MetalForm::slotted)},
    {"beam-trim", "Structural Beam Trim", Pattern::formedMetal, Pattern::rivets, Pattern::minimum, 5, 18, 0x343838, 0x9ca6a5, unsigned(MetalForm::beam)}
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
    case 7: return {G::mineral, W::deposits, .71f, .79f, .33f, .49f, 0x796342};
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
    return {G::brushed, index == 3 || index == 8 ? W::deposits : W::chips,
      index == 6 ? .78f : .56f, .55f, .55f, .62f, 0x8b8476};
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

static_assert(std::size(catalogStains::palettes) == std::size(catalogRecipes));
static_assert(std::size(catalogStains::palettes[0]) == std::size(catalogRecipes[0]));

inline Json catalogColor(uint32_t hex, float alpha) {
  return Json{fromSrgb(float((hex >> 16) & 255) / 255), fromSrgb(float((hex >> 8) & 255) / 255),
    fromSrgb(float(hex & 255) / 255), alpha};
}

inline void setCatalogStain(Document& doc, size_t category, size_t index, StainShape shape) {
  auto& mask = findNode(doc, "stain-mask")["parameters"];
  if (shape == catalogStains::clean) {
    mask["amount"] = 0.f;
    return;
  }
  const auto& palette = *catalogStains::palettes[category][index];
  const auto& treatment = catalogStains::treatments[unsigned(shape)];
  mask["shape"] = unsigned(shape);
  mask["period"] = std::clamp(int(palette.period) + int(index % 3) - 1 + treatment.periodOffset, 2, 32);
  mask["coverage"] = unit(palette.coverage + treatment.coverage - .52f);
  mask["amount"] = palette.strength * treatment.strength;
  mask["horizontal"] = palette.horizontal && !(category == 3 && index == 5);
  findNode(doc, "stain-pigment")["parameters"]["rgba"] = catalogColor(palette.pigments[treatment.pigment], 1);
  findNode(doc, "stain-edge-pigment")["parameters"]["rgba"] = catalogColor(palette.pigments[treatment.edge], 1);
  const float roughness = findNode(doc, "roughness")["parameters"]["value"].get<float>();
  findNode(doc, "stain-roughness")["parameters"]["value"] =
    std::clamp(roughness + treatment.roughnessOffset, .09f, .98f);
  // Push the existing relief gently toward either a recess or a raised deposit.
  findNode(doc, "stain-relief-target")["parameters"]["value"] = treatment.reliefSign > 0 ? 1.f : 0.f;
  findNode(doc, "stain-relief-offset")["parameters"]["weight"] = palette.relief * std::abs(treatment.reliefSign);
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
  const bool clearWater = liquid && index == 0;
  const bool lava = liquid && index == 2;
  const bool oilFilm = liquid && index == 4;
  const bool foam = liquid && index == 5;
  const bool discharge = liquid && (index == 6 || index == 7);
  const bool authoredSurface = recipe.pattern == Pattern::aggregate || recipe.pattern == Pattern::installedSurface ||
    recipe.pattern == Pattern::fibrousSurface || recipe.pattern == Pattern::formedMetal;
  const bool polishedMarble = category == 7 && index == 3;
  const bool profiledDecal = category == 10 && (index == 7 || index == 9);
  const bool clayCracks = category == 6 && index == 4;
  const auto& stains = catalogStains::selection[category][index];
  const bool stained = stains[0] != catalogStains::clean || stains[1] != catalogStains::clean;
  const bool marking = category == 10;
  const bool activeDisplay = category == 11 && (index == 6 || index == 7);
  const bool naturalDetail = !liquid && !marking && !activeDisplay;
  const bool weatheredBrick = category == 1 && (index == 0 || index == 2);
  const bool repointedBrick = weatheredBrick && index == 2;
  source["provenance"]["catalogVersion"] = authoredSurface || lava || foam || discharge || clayCracks || category == 4 ? 8 : 7;
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
    if (row == 0 && authoredSurface) parameters["kind"] = recipe.surfaceKind;
    if (pattern == Pattern::materialGrain) {
      parameters["kind"] = unsigned(finish.grain);
      if (naturalDetail) parameters["naturalDetail"] = 1;
    }
    if (pattern == Pattern::boardJoints) {
      parameters["seams"] = category == 0 ? 0 : 1;
      parameters["width"] = category == 0 ? .058 : .073;
      parameters["irregularity"] = category == 0 ? .26 : .56;
    }
    if (pattern == Pattern::stonework) {
      parameters["roundness"] = index == 5 ? .88 : .26;
      parameters["mortar"] = index == 6 ? .13 : .073;
      parameters["irregularity"] = index == 5 ? .83 : .67;
    }
    if (pattern == Pattern::circle && marking && index == 6) {
      parameters["radius"] = .25;
      parameters["feather"] = .048;
      parameters["edgeWear"] = .91;
    }
    if (pattern == Pattern::cracks && (clayCracks ||
        (category == 7 && index == 4) || (marking && index == 7))) {
      parameters["width"] = marking ? .12 : index == 4 ? .084 : .059;
      parameters["jitter"] = .91;
      if (clayCracks) parameters["erosion"] = .89;
    }
    if (pattern == Pattern::stripes && marking && index == 0) {
      parameters["slant"] = 6;
    }
    if (pattern == Pattern::tiles && category == 0 && index == 8) {
      parameters["grout"] = .032;
      parameters["bevel"] = .023;
      parameters["rows"] = 2;
    }
    if (pattern == Pattern::bricks || pattern == Pattern::flemish) {
      const bool ceramic = category == 2 && index == 3;
      if (row == 0 && weatheredBrick) {
        parameters["mortar"] = repointedBrick ? .127 : .104;
        parameters["bevel"] = repointedBrick ? .126 : .110;
        parameters["irregularity"] = repointedBrick ? .88 : .82;
        parameters["wear"] = repointedBrick ? .62 : .54;
        parameters["rows"] = repointedBrick ? 16 : 14;
        expose({"Joint width", id, "mortar", .02f, .17f, 0});
        expose({"Edge erosion", id, "wear", 0, .70f, 0});
      } else if (row == 0) {
        parameters["mortar"] = ceramic ? .056 : .080;
        parameters["bevel"] = ceramic ? .019 : .039;
        parameters["irregularity"] = ceramic ? .14 : .54;
      } else parameters["irregularity"] = .70;
      if (!std::any_of(nodes.begin(), nodes.end(), [](const Json& entry) { return entry["id"] == "wear"; }))
        nodes.push_back(node("wear", "GradientNoise", recipePattern(Pattern::gradient, 37), 0));
      edges.push_back(wire("wear", "value", id, "variation"));
    }
    if (pattern == Pattern::flow) {
      static constexpr unsigned liquidStyles[]{5, 1, 8, 1, 6, 7, 9, 10, 6, 1};
      parameters["style"] = category == 9 ? liquidStyles[index] : 3;
      if (clearWater) parameters["crossChop"] = .44f;
      if (source["animation"]["frames"] == 1) {
        source["animation"]["frames"] = 16;
        nodes.push_back(node("time", "Time", Json::object(), 0, 1));
      }
      edges.push_back(wire("time", "value", id, "phase"));
    }
    if (parameters.contains("period"))
      expose({row ? "Detail frequency" : "Pattern frequency", id, "period",
        pattern == Pattern::leafLitter ? 3.f : (row == 0 && authoredSurface) || pattern == Pattern::materialGrain ? 2.f : 1.f,
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
    const char* kernel = type.type;
    if (row == 0 && pattern == Pattern::bricks) kernel = weatheredBrick ? "MasonryBrickLayout" : "BrickLayout";
    if (row == 0 && pattern == Pattern::flemish) kernel = "FlemishLayout";
    if (row == 0 && pattern == Pattern::boardJoints) kernel = "BoardLayout";
    nodes.push_back(node(id, kernel, std::move(parameters), 1, row));
  };
  add("pattern", recipe.pattern, recipe.scale, 0);
  add("detail", recipe.detail, recipe.detailScale, 1);
  const bool leaves = recipe.pattern == Pattern::leafLitter;
  const bool pieceLayout = authoredSurface || leaves || recipe.pattern == Pattern::bricks || recipe.pattern == Pattern::flemish ||
    recipe.pattern == Pattern::boardJoints;
  if (pieceLayout) {
    constexpr std::array<const char*, 3> names{"piece-relief", "piece-face", "piece-tone"};
    constexpr std::array<int, 3> rows{0, 6, 7};
    for (unsigned i = 0; i < names.size(); ++i) {
      nodes.push_back(node(names[i], "ChannelUnpack", {{"channel", i}}, 2, rows[i]));
      edges.push_back(wire("pattern", "data", names[i], "data"));
    }
  }
  const char* relief = pieceLayout ? "piece-relief" : "pattern";
  nodes.push_back(node("composite", recipeContract(recipe.composition).type,
    defaultParameters(recipe.composition), 2, 1));
  edges.push_back(wire(relief, "value", "composite", "a"));
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
  const bool woodBoards = category == 5 && recipe.pattern == Pattern::boardJoints;
  if (woodBoards || weatheredBrick) {
    nodes.push_back(node("grain", "ChannelUnpack", {{"channel", 3}}, 2, 2));
    edges.push_back(wire("pattern", "data", "grain", "data"));
  } else {
    Json grain = {{"kind", unsigned(polishedMarble ? GrainKind::polished : finish.grain)}, {"period", std::min(recipe.detailScale, 60u)}};
    if (naturalDetail) grain["naturalDetail"] = 1;
    nodes.push_back(node("grain", "MaterialGrain", std::move(grain), 1, 2));
  }
  const bool paintedSteel = category == 3 && index == 2;
  const unsigned wearFrequency = paintedSteel ? 19u : finish.wear == WearKind::chips ? 9u + recipe.scale % 10u :
    finish.wear == WearKind::deposits ? 8u : std::min(52u, recipe.detailScale);
  const float wearDensity = clearWater || oilFilm || activeDisplay || (category == 4 && index == 9) ? 0.f : finish.density;
  Json wear = {{"kind", unsigned(finish.wear)}, {"period", wearFrequency}, {"density", wearDensity}};
  if (naturalDetail) wear["naturalDetail"] = 1;
  auto damage = node("damage", "SurfaceWear", std::move(wear), 1, 3);
  if (naturalDetail) damage["seedIdentity"] = preset + "-wear";
  nodes.push_back(std::move(damage));
  nodes.push_back(node("intact", "Invert", Json::object(), 2, 3));
  connect("damage", "intact", "value");
  const bool physicalJoint = recipe.pattern == Pattern::bricks || recipe.pattern == Pattern::flemish ||
    recipe.pattern == Pattern::tiles || recipe.pattern == Pattern::boardJoints || recipe.pattern == Pattern::panels ||
    recipe.pattern == Pattern::stonework || recipe.pattern == Pattern::installedSurface || (category == 0 && index == 2);
  const bool gentleStructure = !clayCracks && (recipe.pattern == Pattern::materialGrain || recipe.pattern == Pattern::gradient ||
    recipe.pattern == Pattern::fbm || recipe.pattern == Pattern::ridged || recipe.pattern == Pattern::streaks);
  const bool stone = recipe.pattern == Pattern::stonework;
  const bool wetStone = category == 1 && index == 5;
  const bool peeledPlaster = category == 2 && (index == 0 || index == 1 || index == 6);
  const bool boardConcrete = category == 0 && index == 1;
  const float structuralWeight = authoredSurface || clayCracks ? 0.f :
    recipe.pattern == Pattern::archBand ? .15f : pieceLayout || stone ? .12f :
    gentleStructure ? .27f : physicalJoint ? .32f : .62f;
  mixScalar("structure", relief, "composite", clearWater ? .20f : liquid || activeDisplay ? .38f : structuralWeight, 3, 0);
  mixScalar("surface", "structure", "grain", clearWater ? 0.f : liquid || activeDisplay ? .12f :
    polishedMarble ? .02f : authoredSurface ? .11f : weatheredBrick ? .11f : leaves ? .16f : woodBoards ? .47f : stone ? .22f :
    category == 6 && (index == 4 || index == 6) ? .24f : gentleStructure ? .62f : .40f, 4, 1);
  mixScalar("worn-relief", "surface", "intact", finish.relief * (clearWater || polishedMarble ? 0.f : authoredSurface || liquid || marking ? .06f : weatheredBrick ? .09f : .22f), 5, 1);
  const auto rgb = catalogColor;
  const bool decal = marking && index != 0;
  const bool cementJoint = ((category == 1 || category == 2) && physicalJoint) ||
    (category == 0 && (index == 1 || index == 3 || index == 8)) || (category == 5 && index == 1) ||
    (category == 11 && index == 1);
  const bool firedClay = category == 1 && index < 3;
  const bool wearDecal = decal && (index == 4 || index == 5 || index == 8);
  if (stone) levels("piece-tone", "pattern", .59f, .92f, 3, 3);
  if (lava) {
    nodes.push_back(node("lava-heat", "Invert", Json::object(), 3, 10));
    connect("pattern", "lava-heat", "value");
    levels("fissure-glow", "lava-heat", .87f, .998f, 4, 10);
  }
  if (discharge) levels("ionized-gas", "pattern", .14f, .92f, 4, 10);
  const char* tonalStructure = lava || oilFilm || foam || discharge ? "pattern" : pieceLayout || stone ? "piece-tone" : "structure";
  mixScalar("tone", "grain", tonalStructure, authoredSurface ? .97f : clayCracks ? .83f : liquid || marking || activeDisplay ? .90f :
    weatheredBrick ? .76f : leaves ? .93f : firedClay ? .60f : stone ? (category == 6 ? .78f : .44f) :
    woodBoards ? .55f : boardConcrete ? .28f :
    finish.grain == GrainKind::marble ? .14f : gentleStructure || cementJoint ? .22f : category == 5 ? .28f : .48f, 4, 0);
  Json pigment = {{"low", rgb(recipe.shadow, 1)}, {"high", rgb(recipe.highlight, 1)}};
  if (category == 2 && index == 7) {
    pigment["midtones"] = Json{rgb(0x675246, 1), rgb(0xa99786, 1), rgb(0x514d48, 1),
      rgb(0xd9cdb6, 1), rgb(0xbbaf99, 1)};
  }
  if (category == 8 && index == 1) {
    pigment["midtones"] = Json{rgb(0x6b5257, 1), rgb(0x8e5754, 1), rgb(0xbb7972, 1)};
  }
  if (category == 3 && index == 7) {
    pigment["midtones"] = Json{rgb(0x6c6055, 1), rgb(0x767a7d, 1), rgb(0x9d9b8e, 1)};
  }
  if (discharge) {
    pigment["low"] = rgb(index == 6 ? 0x151b2c : 0x201321, 1);
    pigment["high"] = rgb(index == 6 ? 0x376b91 : 0x744682, 1);
  }
  if (foam) {
    pigment["low"] = rgb(0x273e31, 1);
    pigment["midtones"] = Json{rgb(0x3f6650, 1), rgb(0x809867, 1)};
    pigment["high"] = rgb(0xc3cc9a, 1);
  }
  if (clearWater) {
    pigment["low"] = rgb(0x385a6b, 1);
    pigment["high"] = rgb(0x4b7181, 1);
  }
  if (lava) {
    pigment["low"] = rgb(0x2b2725, 1);
    pigment["high"] = rgb(0x63564d, 1);
  }
  if (oilFilm) {
    pigment["low"] = rgb(0x22232d, 1);
    pigment["midtones"] = Json{rgb(0x51445a, 1), rgb(0x586979, 1), rgb(0x728479, 1),
      rgb(0x9b865b, 1), rgb(0x765569, 1)};
    pigment["high"] = rgb(0x292a36, 1);
  }
  if (leaves) pigment["midtones"] = Json{rgb(0x67412a, 1), rgb(0x8f5834, 1), rgb(0x8d7748, 1)};
  if (weatheredBrick) {
    pigment["midtones"] = repointedBrick ?
      Json{rgb(0x865247u, 1), rgb(0xa66a50u, 1), rgb(0x8b7663u, 1)} :
      Json{rgb(0x563127u, 1), rgb(0xa4583cu, 1), rgb(0x775747u, 1)};
  }
  nodes.push_back(node("pigment", "ColorRamp", std::move(pigment), 5, 0));
  connect("tone", "pigment", "value");
  const auto blend = [&](const char* id, const char* base, const char* top, const char* mask, float opacity, int row, unsigned mode = 0) {
    nodes.push_back(node(id, "Blend", {{"opacity", opacity}, {"enabled", true}, {"mode", mode}}, 7, row));
    edges.push_back(wire(base, "color", id, "base"));
    edges.push_back(wire(top, "color", id, "top"));
    connect(mask, id, "mask");
  };
  const char* colorLayer = "pigment";
  if ((firedClay && !weatheredBrick) || woodBoards || boardConcrete) {
    auto age = node("age", "SurfaceWear", {{"kind", unsigned(WearKind::deposits)},
      {"period", firedClay ? 14u : 10u}, {"density", .85}, {"naturalDetail", 1}}, 1, 6);
    age["seedIdentity"] = preset + "-age";
    nodes.push_back(std::move(age));
    const auto low = firedClay ? 0x746150u : woodBoards ? 0x584e42u : 0x626056u;
    const auto high = firedClay ? 0xa28f7cu : woodBoards ? 0x968877u : 0xaaa092u;
    nodes.push_back(node("aged-pigment", "ColorRamp",
      {{"low", rgb(low, 1)}, {"high", rgb(high, 1)}}, 5, 8));
    connect("grain", "aged-pigment", "value");
    blend("with-age", colorLayer, "aged-pigment", "age", woodBoards ? .45f : .49f, 8);
    colorLayer = "with-age";
  }
  if (weatheredBrick) {
    nodes.push_back(node("clay-discoloration", "ColorRamp",
      {{"low", rgb(0x8c8277u, 1)}, {"high", rgb(0xffffffu, 1)}}, 5, 12));
    connect("grain", "clay-discoloration", "value");
    blend("stained-clay", colorLayer, "clay-discoloration", "piece-face", .67f, 12, 1);
    colorLayer = "stained-clay";
    levels("raised-stone", "piece-relief", .15f, .51f, 3, 5);
    nodes.push_back(node("recessed-stone", "Invert", Json::object(), 4, 5));
    connect("raised-stone", "recessed-stone", "value");
    nodes.push_back(node("chipped-rim", "ConstantColor", {{"rgba", rgb(repointedBrick ? 0x645447u : 0x422b21u, 1)}}, 5, 8));
    blend("with-rim", colorLayer, "chipped-rim", "recessed-stone", .57f, 8);
    colorLayer = "with-rim";
  }
  if (cementJoint || woodBoards) {
    const bool woodSeams = woodBoards || (category == 5 && index == 1);
    const uint32_t mortarLow = repointedBrick ? 0x675d4du : weatheredBrick ? 0x51483au : woodSeams ? 0x362b22u : wetStone ? 0x3a362bu : category == 0 ? 0x4a4841u :
      category == 1 && index >= 3 ? 0x454238u : 0x63594cu;
    const uint32_t mortarHigh = repointedBrick ? 0xc1b7a4u : weatheredBrick ? 0xb0a28au : woodSeams ? 0x5c4e40u : wetStone ? 0x676252u : category == 0 ? 0x747066u :
      category == 1 && index >= 3 ? 0x7c7366u : 0x958777u;
    nodes.push_back(node("mortar", "ColorRamp", {{"low", rgb(mortarLow, 1)}, {"high", rgb(mortarHigh, 1)}}, 5, 2));
    if (weatheredBrick) {
      nodes.push_back(node("mortar-grain", "MaterialGrain",
        {{"kind", unsigned(GrainKind::stone)}, {"period", 51}, {"naturalDetail", 1}}, 1, 7));
      connect("mortar-grain", "mortar", "value");
    } else connect("grain", "mortar", "value");
    if (pieceLayout) {
      nodes.push_back(node("joint-mask", "Invert", Json::object(), 3, 4));
      connect("piece-face", "joint-mask", "value");
    } else {
      nodes.push_back(node("grooves", "Invert", Json::object(), 2, 4));
      connect("pattern", "grooves", "value");
      levels("joint-mask", "grooves", stone ? .68f : .57f, stone ? .95f : .90f, 3, 4);
    }
    blend("with-joints", colorLayer, "mortar", "joint-mask", 1, 0);
    colorLayer = "with-joints";
  }
  const bool layeredWear = paintedSteel || peeledPlaster;
  if (layeredWear) {
    const uint32_t edge = paintedSteel ? 0x604b3eu : 0x827262u;
    const uint32_t substrateLow = paintedSteel ? 0x464947u : 0x877b70u;
    const uint32_t substrateHigh = paintedSteel ? 0x9fa29du : 0xb9aa96u;
    nodes.push_back(node("wear-edge", "ConstantColor", {{"rgba", rgb(edge, 1)}}, 5, 9));
    levels("wear-halo", "damage", .12f, .52f, 3, 5);
    levels("open-damage", "damage", .48f, .89f, 4, 6);
    blend("with-wear-edge", colorLayer, "wear-edge", "wear-halo", paintedSteel ? .78f : .45f, 9);
    colorLayer = "with-wear-edge";
    nodes.push_back(node("worn-pigment", "ColorRamp",
      {{"low", rgb(substrateLow, 1)}, {"high", rgb(substrateHigh, 1)}}, 5, 3));
    connect("grain", "worn-pigment", "value");
  } else {
    const auto wearColor = repointedBrick ? 0x7f6f61u : finish.wearColor;
    nodes.push_back(node("worn-pigment", "ConstantColor", {{"rgba", rgb(wearColor, 1)}}, 5, 3));
  }
  const float wearContrast = polishedMarble ? .08f : authoredSurface ? .32f * finish.wearContrast : weatheredBrick ? .14f : firedClay ? .28f : stone ? .32f : woodBoards ? .44f : finish.wearContrast;
  blend("with-wear", colorLayer, "worn-pigment", layeredWear ? "open-damage" : "damage", wearContrast, 1);
  colorLayer = "with-wear";
  if (category == 0 && index == 9) {
    nodes.push_back(node("rebar-color", "ConstantColor", {{"rgba", rgb(0x694739, 1)}}, 5, 5));
    blend("with-rebar", colorLayer, "rebar-color", "piece-face", .92f, 2);
    colorLayer = "with-rebar";
  }
  if ((category == 4 && index != 9) || (category == 3 && index == 2) || (category == 5 && index == 5)) {
    nodes.push_back(node("scuffs", "SurfaceWear", {{"kind", unsigned(WearKind::scratches)},
      {"period", std::min(36u, recipe.detailScale)}, {"density", .68}}, 1, 5));
    nodes.push_back(node("scuff-pigment", "ConstantColor", {{"rgba", rgb(0xbdb29e, 1)}}, 5, 6));
    blend("with-scuffs", colorLayer, "scuff-pigment", "scuffs", category == 4 ? .25f : .48f, 6);
    colorLayer = "with-scuffs";
  }
  const char* heightLayer = "worn-relief";
  if (weatheredBrick) {
    auto deposits = node("wall-weathering", "FBM",
      {{"period", repointedBrick ? 3 : 4}, {"octaves", 5}, {"gain", .72}, {"lacunarity", 2}}, 1, 8);
    deposits["seedIdentity"] = preset + "-wall";
    nodes.push_back(std::move(deposits));
    mixScalar("eroded-deposits", "wall-weathering", "mortar-grain", repointedBrick ? .21f : .13f, 2, 9);
    levels("wall-deposits", "eroded-deposits", repointedBrick ? .494f : .518f,
      repointedBrick ? .536f : .578f, 3, 7);
    nodes.push_back(node("wall-patina", "ColorRamp",
      {{"low", rgb(repointedBrick ? 0xa49682u : 0x342a23u, 1)},
       {"high", rgb(repointedBrick ? 0xd5cfc2u : 0x7d604bu, 1)}}, 5, 7));
    connect("mortar-grain", "wall-patina", "value");
    blend("with-patina", colorLayer, "wall-patina", "wall-deposits", repointedBrick ? .86f : .32f, 10);
    colorLayer = "with-patina";
    if (repointedBrick) {
      constant("repair-level", .52f, 4, 8);
      mixScalar("repair-relief", "worn-relief", "repair-level", .54f, 5, 10);
      maskedScalar("repointed-relief", "worn-relief", "repair-relief", "wall-deposits", 6, 10);
      heightLayer = "repointed-relief";
    }
  }
  if (stained) {
    auto stain = node("stain-mask", "Stain", defaultParameters(Kernel::stain), 3, 12);
    stain["seedIdentity"] = preset + "-stain";
    nodes.push_back(std::move(stain));
    if (liquid) {
      connect("time", "stain-mask", "phase");
    } else {
      constant("stain-phase", 0, 2, 12);
      connect("stain-phase", "stain-mask", "phase");
    }
    for (const char* id : {"stain-core", "stain-edge"}) {
      const bool core = std::string_view(id) == "stain-core";
      nodes.push_back(node(id, "ChannelUnpack", {{"channel", core ? 0 : 1}}, 4, core ? 12 : 13));
      edges.push_back(wire("stain-mask", "data", id, "data"));
    }
    const bool structural = category == 1 || category == 5 || (category == 2 && pieceLayout);
    const float stainOpacity = authoredSurface ? .59f : weatheredBrick ? .67f : leaves ? .61f : structural ? .73f : .92f;
    nodes.push_back(node("stain-pigment", "ConstantColor", defaultParameters(Kernel::color), 5, 12));
    nodes.push_back(node("stain-edge-pigment", "ConstantColor", defaultParameters(Kernel::color), 5, 13));
    const char* core = "stain-core";
    const char* rim = "stain-edge";
    if (category == 2 && index == 5) {
      // Grout wicks moisture. The glaze only catches a faint residue in comparison.
      constant("glaze-residue", .13f, 4, 14);
      mixScalar("near-joints", "glaze-residue", "joint-mask", .87f, 4, 15);
      combine("joint-stain-core", "Multiply", core, "near-joints", 5, 11);
      combine("joint-stain-rim", "Multiply", rim, "near-joints", 5, 12);
      core = "joint-stain-core";
      rim = "joint-stain-rim";
    }
    blend("with-stain-edge", colorLayer, "stain-edge-pigment", rim, stainOpacity * .62f, 12);
    blend("with-stains", "with-stain-edge", "stain-pigment", core, stainOpacity, 13);
    colorLayer = "with-stains";
    constant("stain-relief-target", 0, 5, 14);
    mixScalar("stain-relief-offset", heightLayer, "stain-relief-target", .1f, 6, 14);
    maskedScalar("stained-relief", heightLayer, "stain-relief-offset", core, 7, 14);
    heightLayer = "stained-relief";
  }
  if (decal) {
    const char* mask = profiledDecal ? "piece-face" : "pattern";
    if (recipe.pattern == Pattern::cracks) {
      nodes.push_back(node("crack-mask", "Invert", Json::object(), 3, 2));
      connect("pattern", "crack-mask", "value");
      mask = "crack-mask";
    } else if (wearDecal) {
      combine("stain-decal-opacity", "Maximum", "damage", "stain-core", 6, 2);
      combine("stain-decal-edge", "Maximum", "stain-decal-opacity", "stain-edge", 7, 2);
      mask = "stain-decal-edge";
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
  channel("height", heightLayer, "value");
  const bool metal = category == 3 || category == 4;
  Json normal = {{"strength", polishedMarble ? .46f : weatheredBrick ? 3.4f : authoredSurface ? 2.55f : .65f + 1.7f * finish.relief}, {"invertY", false}};
  if (weatheredBrick || clearWater || authoredSurface) normal["referenceSize"] = 256;
  nodes.push_back(node("normal", "HeightToNormal", std::move(normal), 7, 3));
  nodes.push_back(node("ao", "HeightToAO", {{"strength", .8f + finish.relief}}, 7, 4));
  constant("roughness", polishedMarble ? .27f : category == 2 && index == 7 ? .37f :
    wetStone ? .38f : finish.roughness, 4, 4);
  mixScalar("micro-roughness", "roughness", "grain", metal ? .22f : .16f, 5, 4);
  if (naturalDetail) {
    const bool damp = category == 0 && index == 4;
    constant("substrate-roughness", paintedSteel ? .38f : damp ? .19f : weatheredBrick ? .95f : .89f, 5, 11);
    const char* mask = layeredWear ? "open-damage" : "damage";
    if (wetStone || weatheredBrick) {
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
  connect(heightLayer, "normal", "height");
  connect(heightLayer, "ao", "height");
  channel("normal", "normal", "normal");
  if (weatheredBrick) {
    nodes.push_back(node("joint-ao", "HeightToAO", {{"strength", 1.45}, {"radius", 7}}, 7, 11));
    connect(heightLayer, "joint-ao", "height");
    combine("stone-ao", "Multiply", "ao", "joint-ao", 8, 9);
    channel("ao", "stone-ao", "value");
  } else channel("ao", "ao", "value");
  if (stained) {
    constant("stain-roughness", .5f, 5, 15);
    const char* roughnessMask = category == 2 && index == 5 ? "joint-stain-core" : "stain-core";
    maskedScalar("stained-roughness", "worn-roughness", "stain-roughness", roughnessMask, 7, 15);
    channel("roughness", "stained-roughness", "value");
  } else channel("roughness", clearWater ? "roughness" : "worn-roughness", "value");
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
  if (lava || (liquid && index >= 6) || activeDisplay) {
    nodes.push_back(node("glow", "ColorRamp",
      {{"low", {0,0,0,1}}, {"high", rgb(lava ? 0xff9837u : recipe.highlight, 1)}}, 4, 5));
    connect(lava ? "fissure-glow" : discharge ? "ionized-gas" : "composite", "glow", "value");
    channel("emissive", "glow", "color");
  }
  expose({"Detail influence", "surface", "weight", 0, 1, 0});
  if (wearDensity > 0.f) expose({"Wear density", "damage", "density", 0, 1, 0});
  if (stained) {
    expose({"Stain frequency", "stain-mask", "period", 2, 32, 1});
    expose({"Stain coverage", "stain-mask", "coverage", 0, 1, 0});
    expose({"Stain strength", "stain-mask", "amount", 0, 1, 0});
    setCatalogStain(doc, category, index, stains[0]);
  }
  expose({"Bump strength", "normal", "strength", 0, 8, 0});
  expose({"Roughness", "roughness", "value", 0, 1, 0});
  return doc;
}

struct RecipeVariation {
  const char* name;
  float detail, bump, roughness;
  unsigned scalePercent;
};

struct FlowVariation {
  const char* name;
  unsigned period;
  float amplitude, normalStrength, roughness, crossChop;
};

inline constexpr FlowVariation waterVariations[]{
  {"Fine wind ripples", 8, .49f, 2.1f, .19f, .30f},
  {"Long swell", 2, .62f, 2.9f, .12f, .16f},
  {"Crossing ripples", 4, .92f, 2.6f, .16f, .91f},
  {"Broken wind chop", 6, .98f, 3.15f, .18f, .64f},
};
inline constexpr FlowVariation oilVariations[]{
  {"Wide film bands", 3, .79f, 1.15f, .17f, 0.f},
  {"Broken film bands", 8, .91f, 1.8f, .22f, 0.f},
  {"Still film", 2, .35f, .75f, .11f, 0.f},
  {"Fine interference bands", 12, .60f, 1.25f, .18f, 0.f},
};

inline constexpr size_t maxCatalogVariations = catalogStains::Choices{}.size() - 1;
static_assert(std::size(waterVariations) == maxCatalogVariations && std::size(oilVariations) == maxCatalogVariations);

struct CatalogFinish {
  const char* name;
  unsigned frequency;
  float brightness, warmth, roughness;
};

// These finishes change the substrate or manufacture. They keep the recipe's
// existing damage and never add another stain mask.
inline constexpr CatalogFinish catalogFinishes[][4]{
  {{"Fine cast", 123, .98f, -.018f, -.05f}, {"Pale mineral mix", 100, 1.14f, .019f, .025f},
    {"Coarse cast", 81, .97f, .043f, .06f}, {"Dark mineral mix", 106, .88f, -.027f, .04f}},
  {{"Fine courses", 128, .99f, .027f, -.015f}, {"Cool stone batch", 100, 1.08f, -.054f, -.02f},
    {"Broad courses", 77, .95f, .017f, .035f}, {"Dark stone batch", 100, .86f, -.013f, .045f}},
  {{"Fine finish", 121, .98f, -.021f, -.04f}, {"Ivory finish", 100, 1.13f, .055f, -.055f},
    {"Paler cool finish", 105, 1.10f, -.039f, .025f}, {"Matte finish", 100, .92f, .011f, .10f}},
  {{"Fine machined finish", 125, 1.03f, -.020f, -.065f}, {"Satin steel", 100, 1.12f, -.04f, -.12f},
    {"Matte steel", 100, .92f, .019f, .12f}, {"Dark steel", 100, .83f, -.012f, .03f}},
  {{"Satin enamel", 100, 1.04f, -.016f, -.12f}, {"Warm enamel", 100, 1.11f, .054f, -.035f},
    {"Cool enamel", 100, 1.02f, -.062f, -.03f}, {"Matte enamel", 100, .90f, .006f, .12f}},
  {{"Fine grain", 126, 1.04f, .022f, -.04f}, {"Light timber", 100, 1.14f, .042f, -.035f},
    {"Mature timber", 100, .87f, .021f, .04f}, {"Dry timber", 92, .99f, -.049f, .08f}},
  {{"Finer surface", 129, 1.02f, -.012f, -.022f}, {"Paler sediment", 100, 1.13f, .024f, .038f},
    {"Coarser surface", 75, .93f, .028f, .075f}, {"Dark sediment", 100, .85f, -.026f, .025f}},
  {{"Fine structure", 124, 1.01f, -.020f, -.022f}, {"Pale surface", 100, 1.17f, .026f, -.015f},
    {"Broad structure", 75, .97f, .022f, .045f}, {"Cool surface", 100, .92f, -.051f, .03f}},
  {{"Fine structure", 128, 1.03f, -.024f, -.035f}, {"Muted pigment", 100, 1.11f, -.040f, .035f},
    {"Broad structure", 76, .97f, .031f, .065f}, {"Dark pigment", 100, .87f, .032f, -.025f}},
  {{"Finer flow", 128, .96f, -.025f, -.052f}, {"Slow broad flow", 76, 1.08f, .028f, -.033f},
    {"Bright surface", 108, 1.14f, -.034f, -.08f}, {"Dark surface", 100, .86f, .031f, .055f}},
  {{"Fine pigment", 119, .96f, -.017f, -.055f}, {"Pale pigment", 100, 1.17f, .016f, .035f},
    {"Dark pigment", 100, .83f, -.014f, .09f}, {"Warm pigment", 104, .98f, .057f, .025f}},
  {{"Satin finish", 100, 1.07f, -.016f, -.105f}, {"Warm finish", 100, 1.09f, .048f, -.040f},
    {"Cool finish", 100, 1.03f, -.052f, -.035f}, {"Matte finish", 100, .89f, .012f, .13f}},
};
static_assert(std::size(catalogFinishes) == std::size(catalogRecipes));

inline size_t catalogVariationCount(size_t, size_t) { return maxCatalogVariations; }

inline size_t stainVariationCount(size_t category, size_t index) {
  const auto& choices = catalogStains::selection[category][index];
  return std::count_if(choices.begin() + 1, choices.end(),
    [](StainShape shape) { return shape != catalogStains::clean; });
}

// Index zero opens the default recipe. Alternate indices are contiguous and one-based.
inline const char* catalogVariationName(size_t category, size_t index, size_t variation) {
  if (variation == 0) return "Default";
  if (category == 9 && index == 0) return waterVariations[variation - 1].name;
  if (category == 9 && index == 4) return oilVariations[variation - 1].name;
  const auto stains = stainVariationCount(category, index);
  if (variation > stains) return catalogFinishes[category][variation - stains - 1].name;
  return catalogStains::treatments[unsigned(catalogStains::selection[category][index][variation])].name;
}

inline void scaleExposedPattern(Document& doc, unsigned percent, bool includeLayout) {
  for (const auto& binding : doc.source["exposedParameters"]) {
    const auto key = binding["key"].get<std::string>();
    const auto id = binding["node"].get<std::string>();
    if ((id != "pattern" && id != "detail") || key == "digit") continue;
    if (!includeLayout && key != "period" && key != "columns") continue;
    auto& value = findNode(doc, id)["parameters"][key];
    const double low = binding["min"], high = binding["max"];
    const unsigned step = binding["step"];
    double scaled = value.get<double>() * percent / 100;
    if (step) {
      scaled = low + std::round((scaled - low) / step) * step;
      value = unsigned(std::clamp(scaled, low, high));
    } else value = std::clamp(scaled, low, high);
  }
}

inline void applyCatalogFinish(Document& doc, const CatalogFinish& finish) {
  auto& pigment = findNode(doc, "pigment")["parameters"];
  const auto tint = [&](Json& color) {
    auto rgba = color.get<Pixel>();
    const float balance[]{1.f + finish.warmth, 1.f, 1.f - finish.warmth};
    for (unsigned i = 0; i < 3; ++i) rgba[i] = unit(rgba[i] * finish.brightness * balance[i]);
    color = rgba;
  };
  tint(pigment["low"]);
  tint(pigment["high"]);
  if (pigment.contains("midtones")) for (auto& color : pigment["midtones"]) tint(color);
  auto& roughness = findNode(doc, "roughness")["parameters"]["value"];
  roughness = std::clamp(roughness.get<float>() + finish.roughness, .06f, .98f);
  if (finish.frequency != 100) scaleExposedPattern(doc, finish.frequency, false);
}

inline void applyCatalogVariation(Document& doc, size_t category, size_t index, size_t variation) {
  require(variation > 0 && variation <= catalogVariationCount(category, index),
    "variation", "This catalog preset does not offer that variation.");
  if (category == 9 && (index == 0 || index == 4)) {
    const auto& wave = index == 0 ? waterVariations[variation - 1] : oilVariations[variation - 1];
    auto& pattern = findNode(doc, "pattern")["parameters"];
    pattern["period"] = wave.period;
    pattern["amplitude"] = wave.amplitude;
    if (index == 0) pattern["crossChop"] = wave.crossChop;
    findNode(doc, "normal")["parameters"]["strength"] = wave.normalStrength;
    findNode(doc, "roughness")["parameters"]["value"] = wave.roughness;
  } else {
    const auto stains = stainVariationCount(category, index);
    if (variation <= stains) setCatalogStain(doc, category, index, catalogStains::selection[category][index][variation]);
    else applyCatalogFinish(doc, catalogFinishes[category][variation - stains - 1]);
  }
}

inline void applyCuratedVariation(Document& doc, const RecipeVariation& settings) {
  auto& surface = findNode(doc, "surface")["parameters"];
  auto& normal = findNode(doc, "normal")["parameters"];
  auto& roughness = findNode(doc, "roughness")["parameters"];
  surface["weight"] = settings.detail;
  normal["strength"] = settings.bump;
  roughness["value"] = settings.roughness;
  auto& damage = findNode(doc, "damage")["parameters"]["density"];
  damage = std::clamp(damage.get<float>() + .36f * (settings.detail - .65f), 0.f, 1.f);
  scaleExposedPattern(doc, settings.scalePercent, true);
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

struct FamilyControls {
  uint32_t seed, shade, accent;
  unsigned scale;
  float wear;
};

inline FamilyControls defaultFamilyControls(size_t index, uint32_t seed) {
  const auto& family = textureFamilies[index];
  return {seed, family.shade, family.accent, family.settings.scalePercent, 0.f};
}

inline Document familyMaterial(size_t index, size_t role, FamilyControls controls) {
  require(index < std::size(textureFamilies) && role < std::size(familyRoles), "family", "Unknown family member.");
  require(controls.shade <= 0xffffff && controls.accent <= 0xffffff &&
    controls.scale >= 25 && controls.scale <= 200 && controls.wear >= -.5f && controls.wear <= .5f,
    "family", "Family colors, scale or wear are out of range.");
  const auto& family = textureFamilies[index];
  const auto& [category, recipe] = family.recipes[role];
  const auto name = std::string(family.id) + "-" + familyRoles[role];
  auto doc = catalogPreset(category, recipe, name);
  auto settings = family.settings;
  settings.scalePercent = controls.scale;
  applyCuratedVariation(doc, settings);
  auto& density = findNode(doc, "damage")["parameters"]["density"];
  density = unit(density.get<float>() + controls.wear);
  doc.source["name"] = std::string(family.name) + " / " + familyRoles[role];
  doc.source["rootSeed"] = uint32_t(stableSeed(controls.seed, name));
  doc.source["provenance"]["family"] = {{"id", family.id}, {"role", familyRoles[role]}, {"seed", controls.seed},
    {"version", 3}, {"shade", controls.shade}, {"accent", controls.accent}, {"scalePercent", controls.scale},
    {"wear", controls.wear}};
  auto& pigment = findNode(doc, "pigment")["parameters"];
  const auto tint = [&](const char* key, uint32_t hex, float weight) {
    auto rgba = pigment[key].get<Pixel>();
    for (unsigned channel = 0; channel < 3; ++channel) {
      const float target = fromSrgb(float((hex >> (16 - channel * 8)) & 255) / 255);
      rgba[channel] += weight * (target - rgba[channel]);
    }
    pigment[key] = rgba;
  };
  tint("low", controls.shade, .72f);
  tint("high", controls.accent, role == 7 ? .4f : .72f);
  return doc;
}

inline Document familyMaterial(size_t index, size_t role, uint32_t seed) {
  require(index < std::size(textureFamilies), "family", "Unknown texture family.");
  return familyMaterial(index, role, defaultFamilyControls(index, seed));
}

inline std::optional<std::pair<size_t, size_t>> familyIdentity(const Document& doc) {
  const auto& provenance = doc.source.at("provenance");
  if (!provenance.contains("family")) return std::nullopt;
  const auto& saved = provenance.at("family");
  if (!saved.is_object() || !saved.contains("id") || !saved.contains("role")) return std::nullopt;
  for (size_t index = 0; index < std::size(textureFamilies); ++index)
    if (saved["id"] == textureFamilies[index].id)
      for (size_t role = 0; role < std::size(familyRoles); ++role)
        if (saved["role"] == familyRoles[role]) return {{index, role}};
  return std::nullopt;
}

inline FamilyControls readFamilyControls(const Document& doc, size_t index) {
  const auto& saved = doc.source.at("provenance").at("family");
  const auto seed = uint32_t(integer(member(saved, "seed", "family"), 0, UINT32_MAX, "family.seed"));
  if (!saved.contains("scalePercent")) return defaultFamilyControls(index, seed);
  return {seed,
    uint32_t(integer(member(saved, "shade", "family"), 0, 0xffffff, "family.shade")),
    uint32_t(integer(member(saved, "accent", "family"), 0, 0xffffff, "family.accent")),
    unsigned(integer(member(saved, "scalePercent", "family"), 25, 200, "family.scalePercent")),
    number(member(saved, "wear", "family"), -.5f, .5f, "family.wear")};
}

inline void refreshFamilyMember(Document& doc, FamilyControls controls, bool resetOverrides = false) {
  const auto identity = familyIdentity(doc);
  require(bool(identity), "family", "This source does not belong to a built-in family.");
  const auto [index, role] = *identity;
  auto previous = familyMaterial(index, role, readFamilyControls(doc, index));
  auto next = familyMaterial(index, role, controls);
  auto& nextInfo = next.source["provenance"]["family"];
  auto& overrides = nextInfo["overrides"] = Json::array();
  if (doc.seed() == previous.seed() || resetOverrides) doc.source["rootSeed"] = next.seed();
  else overrides.push_back("rootSeed");
  auto& nodes = doc.source["graph"]["nodes"];
  const auto inherit = [&](std::string_view id, const std::string& key) {
    const auto& previousNode = findNode(previous, id);
    const auto& nextParams = findNode(next, id)["parameters"];
    auto current = std::find_if(nodes.begin(), nodes.end(), [&](const Json& entry) {
      return entry["id"] == id && entry["type"] == previousNode["type"];
    });
    if (current == nodes.end() || !(*current)["parameters"].contains(key)) return;
    auto& value = (*current)["parameters"][key];
    if (value == previousNode["parameters"][key] || resetOverrides) value = nextParams[key];
    else overrides.push_back(std::string(id) + "/" + key);
  };
  inherit("pigment", "low");
  inherit("pigment", "high");
  inherit("damage", "density");
  for (const auto& binding : next.source["exposedParameters"]) {
    const auto id = binding["node"].get<std::string>();
    const auto key = binding["key"].get<std::string>();
    if ((id == "pattern" || id == "detail") && key != "digit") inherit(id, key);
  }
  doc.source["provenance"]["family"] = std::move(nextInfo);
}

} // namespace ftebroom::textureStudio
