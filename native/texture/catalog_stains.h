// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace ftebroom::textureStudio {

struct StainPalette {
  std::array<uint32_t, 4> pigments;
  unsigned period;
  float coverage, strength, relief;
  bool horizontal = false;
};

namespace catalogStains {
using S = StainShape;
inline constexpr StainPalette cement{{0x645e50, 0xc0b495, 0x62594d, 0x797b68},
  4, .54f, .64f, .12f};
inline constexpr StainPalette damp{{0x282b23, 0x9b9f80, 0x51432f, 0x60664d},
  4, .60f, .69f, .06f};
inline constexpr StainPalette clay{{0x493027, 0xbfae8d, 0xa35e3d, 0x83745a},
  4, .49f, .54f, .08f};
inline constexpr StainPalette oldClay{{0x594336, 0xc9bba5, 0x946047, 0x918674},
  5, .46f, .53f, .09f};
inline constexpr StainPalette rock{{0x44463a, 0xb4a98b, 0x836752, 0x727b5a},
  5, .53f, .66f, .12f};
inline constexpr StainPalette darkRock{{0x272a29, 0x93948a, 0x62564a, 0x555f52},
  5, .51f, .63f, .08f};
inline constexpr StainPalette plaster{{0x73675a, 0xdbd2bf, 0xa69478, 0x777a62},
  4, .58f, .61f, .12f};
inline constexpr StainPalette ceramic{{0x544e40, 0xd5ceb5, 0x928570, 0x6d785b},
  5, .45f, .45f, .05f};
inline constexpr StainPalette marble{{0x5c6067, 0xebebe5, 0x918c80, 0x777d84},
  4, .45f, .39f, .02f, true};
inline constexpr StainPalette steel{{0x3f4542, 0xabafa6, 0x70665b, 0x6e7771},
  6, .46f, .57f, .03f, true};
inline constexpr StainPalette rust{{0x3c2721, 0xb17543, 0x824425, 0x735840},
  5, .55f, .71f, .12f};
inline constexpr StainPalette paint{{0x30332e, 0x9b8e73, 0x794e38, 0x59625a},
  5, .47f, .63f, .09f};
inline constexpr StainPalette wood{{0x493224, 0xb9a382, 0x97633e, 0x817963},
  5, .54f, .67f, .06f};
inline constexpr StainPalette weatheredWood{{0x3c3933, 0xa49783, 0x696359, 0x8c8779},
  5, .49f, .57f, .04f};
inline constexpr StainPalette charred{{0x211a16, 0x938577, 0x684532, 0x4b463d},
  5, .62f, .70f, .09f};
inline constexpr StainPalette earth{{0x372b22, 0xb09d7b, 0x8d6543, 0x615a3b},
  4, .59f, .70f, .07f};
inline constexpr StainPalette sand{{0x79604b, 0xcdbd9c, 0xb79a75, 0x8f8370},
  5, .55f, .52f, .06f, true};
inline constexpr StainPalette asphalt{{0x252724, 0x96948a, 0x645a4e, 0x50564f},
  4, .55f, .69f, .09f};
inline constexpr StainPalette frost{{0x68818a, 0xf2f1ea, 0xb7c5c6, 0x84958e},
  4, .57f, .58f, .07f};
inline constexpr StainPalette moss{{0x343822, 0xa0a37c, 0x766b3c, 0x535e32},
  4, .61f, .73f, .11f};
inline constexpr StainPalette skin{{0x623232, 0xcba084, 0xa46558, 0x85686a},
  5, .48f, .48f, .04f};
inline constexpr StainPalette tissue{{0x4d2530, 0xc78d74, 0x8e493f, 0x795b65},
  4, .61f, .60f, .06f};
inline constexpr StainPalette slime{{0x253529, 0xa0ac65, 0x6c7445, 0x486252},
  4, .62f, .68f, .07f};
inline constexpr StainPalette molten{{0x332420, 0xd68b45, 0x994f2b, 0x694238},
  4, .61f, .66f, .10f};
inline constexpr StainPalette energy{{0x32314f, 0x9aa9c8, 0x627494, 0x745f8e},
  4, .47f, .48f, .04f};
inline constexpr StainPalette enamel{{0x55534b, 0xb1ab94, 0x8e7957, 0x696b5f},
  6, .42f, .41f, .04f};

inline constexpr const StainPalette* palettes[][10]{
  {&cement,&cement,&cement,&cement,&damp,&cement,&plaster,&damp,&cement,&rust},
  {&clay,&clay,&oldClay,&rock,&rock,&rock,&rock,&darkRock,&darkRock,&rock},
  {&plaster,&plaster,&ceramic,&ceramic,&ceramic,&damp,&paint,&cement,&plaster,&sand},
  {&steel,&rust,&paint,&steel,&steel,&steel,&rust,&steel,&steel,&rust},
  {&paint,&paint,&steel,&steel,&rust,&paint,&paint,&steel,&steel,&enamel},
  {&wood,&wood,&wood,&wood,&weatheredWood,&paint,&charred,&charred,&wood,&wood},
  {&earth,&rock,&sand,&earth,&earth,&asphalt,&asphalt,&frost,&earth,&darkRock},
  {&rock,&rock,&sand,&marble,&frost,&moss,&moss,&earth,&darkRock,&frost},
  {&skin,&tissue,&tissue,&plaster,&charred,&tissue,&slime,&tissue,&energy,&tissue},
  {nullptr,&slime,nullptr,&slime,nullptr,&slime,nullptr,nullptr,&molten,&slime},
  {&enamel,&enamel,&enamel,&enamel,&rust,&damp,&asphalt,&asphalt,&earth,&moss},
  {&steel,&rock,&steel,&paint,&darkRock,&paint,&energy,&enamel,&paint,&steel}
};

// The default and alternate effects are chosen per recipe. An empty entry
// keeps the authored surface; it does not substitute another kind of stain.
constexpr S clean = S::count;
using Choices = std::array<S, 5>;
constexpr Choices finishes(S standard = clean, S a = clean, S b = clean, S c = clean, S d = clean) {
  return {standard, a, b, c, d};
}

inline constexpr Choices selection[][10]{
  { // Concrete
    finishes(clean, S::deposits, S::specks, S::runoff),    // Poured
    finishes(clean, S::runoff, S::bands),                 // Board-formed
    finishes(S::deposits, S::film, S::specks),             // Cracked slab
    finishes(clean, S::deposits, S::runoff),              // Chipped precast
    finishes(S::runoff, S::film, S::blooms),              // Wet bunker
    finishes(clean, S::film, S::deposits),                // Aggregate
    finishes(clean, S::flakes, S::deposits),              // Skim coat and patches
    finishes(S::film, S::deposits),                        // Ceiling leak
    finishes(clean, S::scuffs, S::film),                  // Expansion floor
    finishes(S::runoff, S::deposits, S::specks),           // Exposed rebar
  },
  { // Masonry
    finishes(clean, S::deposits, S::runoff, S::blooms),    // Warm running brick
    finishes(clean, S::specks, S::deposits),              // Flemish bond
    finishes(S::flakes, S::deposits, S::blooms),          // Broken brick and lime
    finishes(S::blooms, S::deposits, S::runoff),           // Rough stone
    finishes(clean, S::runoff, S::deposits),              // Ashlar
    finishes(S::film, S::specks, S::blooms),               // Wet cobblestone
    finishes(S::blooms, S::specks, S::deposits),           // Rubble masonry
    finishes(S::runoff, S::deposits),                     // Gothic block
    finishes(clean, S::specks, S::scuffs),                // Basalt setts
    finishes(clean, S::runoff, S::blooms),                // Arch trim
  },
  { // Plaster and tiles
    finishes(clean, S::flakes, S::runoff),                // Peeling plaster
    finishes(clean, S::flakes, S::scuffs),                // Cracked paint
    finishes(clean, S::film),                             // Glazed porcelain
    finishes(clean, S::film, S::runoff),                  // Subway tile
    finishes(clean, S::film),                             // Mosaic
    finishes(S::runoff, S::blooms, S::deposits),           // Stained grout
    finishes(clean, S::flakes, S::runoff),                // Painted brick
    finishes(clean, S::scuffs, S::film),                  // Polished terrazzo
    finishes(clean, S::film, S::specks),                  // Acoustic ceiling
    finishes(clean, S::deposits),                         // Insulation
  },
  { // Metal
    finishes(clean, S::scuffs, S::film),                  // Brushed steel
    finishes(S::deposits, S::flakes, S::runoff, S::specks), // Oxidized iron
    finishes(clean, S::flakes, S::runoff, S::scuffs),      // Painted steel
    finishes(clean, S::scuffs, S::film, S::deposits, S::specks), // Diamond plate
    finishes(clean, S::specks, S::deposits),               // Perforated sheet
    finishes(clean, S::runoff, S::deposits),               // Corrugated sheet
    finishes(S::runoff, S::specks, S::deposits),           // Riveted plate
    finishes(clean, S::bands, S::scuffs),                 // Welded seam
    finishes(clean, S::specks, S::film),                  // Cast metal
    finishes(S::flakes, S::specks, S::deposits, S::runoff), // Rust flakes
  },
  { // Tech panels
    finishes(clean, S::flakes, S::scuffs),                // Modular wall
    finishes(clean, S::scuffs, S::runoff),                // Service hatch
    finishes(clean, S::deposits, S::scuffs),              // Recessed panel
    finishes(clean, S::deposits, S::specks),              // Vent grille
    finishes(clean, S::runoff, S::specks),                // Conduit plate
    finishes(clean, S::specks, S::scuffs),                // Cable tray
    finishes(clean, S::scuffs, S::flakes),                // Access door
    finishes(clean, S::scuffs, S::film),                  // Armored floor
    finishes(clean, S::deposits),                         // Ceiling module
    finishes(),                                          // Laboratory panel
  },
  { // Wood
    finishes(clean, S::bands, S::runoff, S::specks),       // Sawn planks
    finishes(clean, S::scuffs, S::film),                  // Parquet
    finishes(clean, S::bands),                            // Plywood
    finishes(clean, S::specks),                           // End grain
    finishes(S::bands, S::runoff, S::specks),              // Weathered boards
    finishes(clean, S::flakes, S::runoff),                // Painted boards
    finishes(clean, S::flakes, S::deposits),              // Charred wood
    finishes(clean, S::bands, S::blooms),                 // Bark
    finishes(clean, S::scuffs, S::bands),                 // Crate
    finishes(clean, S::bands, S::deposits),               // Split timber
  },
  { // Ground
    finishes(clean, S::specks, S::deposits),              // Loose soil
    finishes(clean, S::specks, S::deposits),              // Mixed gravel
    finishes(clean, S::bands),                            // Sand ripples
    finishes(S::film, S::deposits, S::specks),             // Mud and puddles
    finishes(clean, S::deposits, S::specks),              // Cracked earth
    finishes(clean, S::scuffs, S::film, S::deposits),      // Dense asphalt
    finishes(clean, S::deposits, S::film),                // Damaged asphalt
    finishes(clean, S::blooms),                           // Snow crust
    finishes(),                                          // Leaf litter
    finishes(clean, S::specks, S::deposits),              // Rock scree
  },
  { // Natural surfaces
    finishes(clean, S::specks),                           // Granite
    finishes(clean, S::bands, S::runoff),                 // Stratified rock
    finishes(clean, S::bands, S::deposits),               // Sandstone
    finishes(clean, S::bands),                            // Marble inclusions
    finishes(clean, S::blooms, S::film),                  // Frost and thaw
    finishes(S::blooms, S::deposits),                     // Cushion moss
    finishes(S::blooms, S::specks, S::deposits),           // Crustose lichen
    finishes(clean, S::film, S::deposits),                // Exposed roots
    finishes(clean, S::runoff, S::blooms),                // Cliff face
    finishes(clean, S::blooms, S::flakes),                // Salt crust
  },
  { // Organic horror
    finishes(clean, S::specks, S::deposits),              // Freckles and pigment
    finishes(clean, S::bands, S::blooms),                 // Veins and mottling
    finishes(clean, S::deposits),                         // Scar tissue
    finishes(clean, S::deposits, S::specks),              // Bone plates
    finishes(clean, S::specks, S::bands),                 // Chitin pigmentation
    finishes(clean, S::blooms, S::deposits),              // Root infestation
    finishes(clean, S::film, S::blooms),                  // Slime membrane
    finishes(clean, S::bands),                            // Tendon fibres
    finishes(clean, S::blooms, S::specks),                // Cell pigment
    finishes(clean, S::film, S::bands),                  // Biomechanical wall
  },
  { // Liquids and energy
    finishes(),                                          // Clear water has only waves
    finishes(clean, S::film, S::blooms),                  // Toxic fluid surface
    finishes(),                                          // Cooling lava crust
    finishes(S::film, S::blooms, S::deposits),             // Sludge
    finishes(),                                          // Oil sheen comes from film thickness
    finishes(clean, S::blooms),                          // Acid foam
    finishes(),                                          // Energy field
    finishes(),                                          // Plasma grid
    finishes(clean, S::film),                             // Oxide skin on molten metal
    finishes(clean, S::film, S::bands),                   // Flowing slime
  },
  { // Decals and markings
    finishes(clean, S::scuffs, S::flakes),                // Hazard stripes
    finishes(clean, S::flakes),                          // Room number
    finishes(clean, S::scuffs, S::flakes),                // Arrow
    finishes(clean, S::flakes, S::scuffs),                // Warning panel
    finishes(S::runoff, S::bands),                         // Rust drip
    finishes(S::runoff, S::bands),                         // Water streak on a wall
    finishes(clean, S::specks),                           // Impact dust
    finishes(),                                          // Crack silhouette
    finishes(S::film, S::deposits, S::runoff, S::blooms),  // Leak stain
    finishes(clean, S::deposits, S::blooms),               // Grime edge
  },
  { // Trims and displays
    finishes(clean, S::scuffs, S::deposits),              // Industrial trim
    finishes(clean, S::runoff, S::blooms),                // Gothic trim
    finishes(clean, S::runoff, S::specks),                // Pipe border
    finishes(clean, S::scuffs),                           // Cable border
    finishes(clean, S::scuffs, S::film),                  // Floor border
    finishes(clean, S::scuffs, S::flakes),                // Door frame
    finishes(),                                          // Terminal glass
    finishes(),                                          // Status lights
    finishes(clean, S::deposits),                         // Grille strip
    finishes(clean, S::runoff, S::scuffs),                // Structural beam
  },
};

static_assert(std::size(selection) == std::size(palettes));

struct Treatment {
  const char* name;
  unsigned pigment, edge;
  float coverage, strength, reliefSign, roughnessOffset;
  int periodOffset;
};

inline constexpr Treatment treatments[]{
  {"Speckled",       2, 1, .59f, .86f, -1.f,  .11f,  1},
  {"Runoff",         0, 3, .52f, .94f, -.2f, -.22f,  0},
  {"Flaking",        1, 0, .48f, 1.0f, -1.f,  .17f,  1},
  {"Deposits",       2, 0, .56f, 1.0f,  .6f,  .12f, -1},
  {"Mottled",        3, 1, .59f, 1.0f,  .8f,  .16f,  0},
  {"Scuffed",        1, 2, .43f, .86f, -.6f, -.15f,  2},
  {"Film and rims",  0, 1, .60f, 1.0f, -.2f, -.29f, -1},
  {"Directional",    2, 3, .55f, .94f,  .3f,  .04f,  1},
};
static_assert(std::size(treatments) == unsigned(S::count));
} // namespace catalogStains

} // namespace ftebroom::textureStudio
