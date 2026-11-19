// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Included by evaluate.h inside the texture-studio namespace, after the
// periodic noise, cell distance, and wood grain primitives.

struct GranularFinish {
  float density, minRadius, maxRadius, angularity, exposure, background, contrast;
};

inline constexpr GranularFinish granularFinishes[]{
  {.93f, .21f, .57f, .32f, .28f, .43f, .75f}, // washed concrete
  {.77f, .15f, .64f, .86f, .025f, .73f, .93f}, // polished terrazzo
  {.98f, .26f, .68f, .52f, .68f, .24f, .63f}, // gravel over soil
  {.94f, .28f, .77f, .91f, .82f, .20f, .66f}, // fragmented scree
  {.89f, .17f, .47f, .70f, .17f, .23f, .42f}, // aggregate embedded in bitumen
};
static_assert(std::size(granularFinishes) == unsigned(AggregateKind::moss));

inline Pixel mossBed(int period, uint64_t seed, float u, float v) {
  const float bank = .60f * valueNoise(seed, 4, 5, u, v) +
    .26f * valueNoise(seed + 1, 11, 13, u, v) + .14f * valueNoise(seed + 2, 42, u, v);
  const float colony = softThreshold(.32f, .61f, bank);
  const float aspect = valueNoise(seed + 3, 17, 11, u, v);
  const float mat = valueNoise(seed + 4, period * 4, u, v);
  const float drift = .008f * (aspect - .5f);
  const float fronds = mineralMarks(seed + 5, period * 4, .41f + .43f * colony,
    u + drift, v - drift * .61f, .12f, .35f, 1.82f);
  const float tips = mineralMarks(seed + 6, period * 7, .62f, u - drift * .46f, v, .09f, .30f, 1.51f);
  const float crowns = unit(.59f * fronds + .41f * tips);
  const float soil = .12f + .15f * aspect + .06f * (mat - .5f);
  const float tone = mix(soil, .36f + .31f * aspect + .20f * crowns + .17f * (mat - .5f), colony);
  return {unit(.21f + .23f * colony + .20f * crowns * colony + .11f * (mat - .5f)), colony, unit(tone), mat};
}

inline Pixel granularSurface(AggregateKind kind, int period, uint64_t seed, float u, float v) {
  if (kind == AggregateKind::moss) return mossBed(period, seed, u, v);
  if (kind == AggregateKind::windSand) {
    const float fine = valueNoise(seed, 129, u, v);
    const float bank = valueNoise(seed + 1, 5, 7, u, v);
    const float crest = valueNoise(seed + 2, 5, 11, u, v);
    const float dune = u * 3.f + v * period + .87f * (bank - .5f) + .45f * (crest - .5f);
    const float ripple = .5f + .44f * cycleSine(dune) + .08f * cycleSine(dune * 2.f + .13f);
    const float exposure = .33f + .67f * softThreshold(.18f, .74f, bank);
    return {unit(.42f + exposure * .22f * ripple + .058f * (fine - .5f)),
      exposure, unit(.50f + .21f * (bank - .5f) + .12f * (fine - .5f) + .13f * (ripple - .5f)), fine};
  }
  const bool damaged = kind == AggregateKind::damagedAsphalt;
  const auto& finish = granularFinishes[unsigned(damaged ? AggregateKind::asphalt : kind)];
  const float x = frac(u) * period, y = frac(v) * period;
  const float ground = valueNoise(seed, 5, 7, u, v);
  const float grit = valueNoise(seed + 2, period * 5, u, v);
  const float fine = valueNoise(seed + 3, period * 11, u, v);
  const bool terrazzo = kind == AggregateKind::terrazzo;
  float height = .20f + .08f * grit + .033f * ground;
  float pigment = finish.background + .13f * (ground - .5f) + .16f * (grit - .5f);
  float face = 0.f;
  for (int j = int(y) - 1; j <= int(y) + 1; ++j) {
    for (int i = int(x) - 1; i <= int(x) + 1; ++i) {
      const auto a = wrap(i, period), b = wrap(j, period);
      const float size = lattice(seed + 4, a, b);
      if (size > finish.density) continue;
      const float dx = x - i - lattice(seed + 5, a, b);
      const float dy = y - j - lattice(seed + 6, a, b);
      const float square = dx * dx + dy * dy;
      if (square > .86f) continue;
      const float spin = lattice(seed + 7, a, b);
      const float c = cycleSine(spin + .25f), s = cycleSine(spin);
      const float ax = dx * c + dy * s, ay = dy * c - dx * s;
      const float aspect = mix(.55f, 1.03f, lattice(seed + 8, a, b));
      const float flat = std::max(std::abs(ax), .64f * std::abs(ax) + .70f * std::abs(ay) / aspect);
      const float round = std::sqrt(ax * ax + (ay * ay) / (aspect * aspect));
      const float radius = mix(finish.minRadius, finish.maxRadius, size * size);
      const float microEdge = .055f * (fine - .5f);
      const float distance = mix(round, flat, finish.angularity) +
        (.10f + .10f * finish.angularity) * (grit - .5f) + microEdge;
      if (distance >= radius) continue;
      const float coverage = 1.f - softThreshold(radius * .86f, radius, distance);
      const float crown = 1.f - softThreshold(.12f, 1.f, distance / radius);
      const float layer = lattice(seed + 9, a, b);
      const float relief = finish.exposure * (.28f + .44f * crown + .18f * layer);
      const float top = (.20f + .08f * grit + .033f * ground) + coverage * relief;
      if (top <= height) continue;
      height = top;
      const float clast = lattice(seed + 10, a, b);
      const float grain = .16f * (grit - .5f) + .12f * (fine - .5f);
      const float facet = .05f * crown;
      pigment = mix(finish.background + .13f * (ground - .5f),
        .5f + finish.contrast * (clast - .5f) + grain + facet, coverage);
      face = coverage;
    }
  }
  if (terrazzo) height = .51f + .08f * (fine - .5f) + .016f * face;
  if (damaged) {
    const float dx = u + .081f * (valueNoise(seed + 11, 6, 9, u, v) - .5f);
    const float dy = v + .085f * (valueNoise(seed + 12, 9, 6, u, v) - .5f);
    const auto fracture = cellularDistances(seed + 13, std::max(3, period / 7), .94f, dx, dy);
    const float edge = fracture[1] - fracture[0];
    const float opening = 1.f - softThreshold(.005f, .032f + .069f * ground, edge);
    const float fray = (1.f - softThreshold(.019f, .13f, edge)) * grit;
    height = unit(height - .30f * opening - .08f * fray);
    pigment = unit(pigment * (1.f - .67f * opening) + .15f * fray);
  }
  return {unit(height), face, unit(pigment), grit};
}

inline Pixel installedSurface(InstalledKind kind, int period, uint64_t seed, float u, float v) {
  if (kind == InstalledKind::crate) {
    const float grain = sawnGrain(40, seed + 1, u, v);
    const float railGrain = sawnGrain(43, seed + 2, v, u);
    const float lane = frac(u * period);
    const float seam = softThreshold(.006f, .024f, std::min(lane, 1.f - lane));
    const auto batten = [](float d, float radius) { return 1.f - softThreshold(radius - .017f, radius, std::abs(d)); };
    const float x = frac(u), y = frac(v);
    const float cross = batten(frac(x - y + .5f) - .5f, .073f);
    const float top = batten(std::abs(y - .5f) - .396f, .057f);
    const float rail = std::max(cross, top);
    const float hue = lattice(seed + 3, unsigned(x * period), 0);
    const float screw = std::max(.0f, 1.f - std::sqrt(
      (std::abs(y - .5f) - .396f) * (std::abs(y - .5f) - .396f) + (lane - .48f) * (lane - .48f) / (period * period)) * 121.f);
    const float face = std::max(rail, seam);
    return {unit(.27f + .21f * seam + .27f * rail + .042f * screw + .08f * (grain - .5f)),
      face, unit(mix(.17f + seam * (.18f + .16f * hue + .28f * (grain - .5f)),
        .53f + .30f * (railGrain - .5f), rail) - .38f * screw), grain};
  }
  if (kind == InstalledKind::voussoirs) {
    const float x = frac(u) - .5f, y = .76f - frac(v);
    const float radius = std::sqrt(x * x + y * y);
    const float detail = valueNoise(seed, 69, u, v);
    const float aged = valueNoise(seed + 1, 8, 12, u, v);
    const float angle = (std::atan2(y, x) + 3.14159265f) / 6.28318531f;
    const float stone = angle * period + .10f * (detail - .5f);
    const float joints = softThreshold(.025f, .075f, std::min(frac(stone), 1.f - frac(stone)));
    const float arch = softThreshold(-.016f, .011f, y) *
      softThreshold(.285f, .313f, radius + .023f * (aged - .5f)) *
      (1.f - softThreshold(.419f, .442f, radius + .016f * (detail - .5f)));
    const float face = arch * joints;
    const float tone = lattice(seed + 3, unsigned(stone + 1.f), 0);
    return {unit(.28f + .38f * face + .13f * (detail - .5f)), face,
      unit(.46f + .28f * tone + .11f * (detail - .5f)), detail};
  }
  if (kind == InstalledKind::spalledConcrete) {
    const float area = .74f * valueNoise(seed, 3, 3, u, v) +
      .18f * valueNoise(seed + 1, 13, 17, u, v) + .08f * valueNoise(seed + 2, 68, u, v);
    const float chipped = softThreshold(.45f, .62f, area);
    const float trap = softThreshold(.61f, .80f, area);
    const float material = valueNoise(seed + 3, 61, u, v);
    const float d = (frac(u * period + .25f) - .5f) * 2.f;
    const float rod = std::sqrt(unit(1.f - 109.f * d * d)) * softThreshold(.37f, .86f, chipped);
    const float notch = softThreshold(.14f, .97f, .5f + .5f * cycleSine(v * 43.f + d * 4.f));
    const float surface = std::max(.66f - .43f * chipped + .10f * (material - .5f),
      rod * (.51f + .10f * notch));
    return {unit(surface), rod, unit(.62f - .30f * chipped + .28f * (material - .5f) - .12f * trap), material};
  }
  const bool stone = kind == InstalledKind::limestone || kind == InstalledKind::basalt;
  const bool mosaic = kind == InstalledKind::mosaic;
  const bool parquet = kind == InstalledKind::parquet;
  const int columns = kind == InstalledKind::limestone ? std::max(2, period - 1) : period;
  const float row = frac(v) * period;
  const unsigned j = unsigned(row);
  const float rowShift = stone ? .50f * (j % 2) + .20f * lattice(seed, j, 0) : 0.f;
  const float column = frac(u + rowShift / columns) * columns;
  const unsigned i = unsigned(column);
  const float id = lattice(seed + 3, i, j);
  const float x = frac(column), y = frac(row);
  const float broad = valueNoise(seed + 4, 5, 4, u, v);
  const float grain = valueNoise(seed + 5, period * 10, u, v);
  const float chip = stone ? .105f * (valueNoise(seed + 6, period * 3, u, v) - .45f) + .041f * (grain - .5f) : 0.f;
  const float breadth = parquet ? .012f : mosaic ? .086f : kind == InstalledKind::limestone ? .046f : .078f;
  const float sx = std::min(x, 1.f - x);
  const float sy = std::min(y, 1.f - y);
  const float rawEdge = std::min(sx, sy);
  const float roundedCorner = std::min(rawEdge, .69f * (sx + sy) - breadth * .51f);
  const float edge = roundedCorner - breadth * .5f - std::max(chip, -.43f * breadth);
  const float face = softThreshold(0, parquet ? .012f : mosaic ? .057f : .07f, edge);
  if (parquet) {
    const bool turned = ((i + j) % 2) != 0;
    const float s = turned ? y : x, t = turned ? x : y;
    const float slat = s * 5.f;
    const float joint = softThreshold(.004f, .024f, std::min(frac(slat), 1.f - frac(slat)));
    const float hue = lattice(seed + 11 + unsigned(slat), i, j);
    const float wood = sawnGrain(36, seed + 12, frac(s * .72f + .25f * id), frac(t * .87f + hue));
    return {mix(.44f, .53f + .055f * (wood - .5f), face * joint), face * joint,
      unit(.42f + .23f * (hue - .5f) + .16f * (id - .5f) + .40f * (wood - .5f)), wood};
  }
  const float variation = mosaic ? .32f : .37f;
  const float hue = .47f + variation * (id - .5f) + .14f * (broad - .5f);
  const float tooling = mosaic ? .025f * (grain - .5f) : .20f * (grain - .5f) + .091f * (broad - .5f);
  return {mix(.18f + .071f * grain, .65f + .12f * (id - .5f) + tooling, face),
    face, unit(hue + tooling), grain};
}

inline Pixel formedMetal(MetalForm form, int period, uint64_t seed, float u, float v) {
  const float x = frac(u), y = frac(v);
  const float fine = .72f * valueNoise(seed, 109, 89, x, y) + .28f * valueNoise(seed + 2, 9, 134, x, y);
  const float oxidation = valueNoise(seed + 1, 12, 17, x, y);
  const auto bar = [](float d, float w, float bevel) { return 1.f - softThreshold(w - bevel, w, std::abs(d)); };
  const auto line = [](float coordinate, float spacing) { return frac(coordinate * spacing + .5f) - .5f; };
  const auto rounded = [&](float dx, float dy, float w, float h, float radius) {
    const float a = std::max(0.f, std::abs(dx) - w + radius);
    const float b = std::max(0.f, std::abs(dy) - h + radius);
    return 1.f - softThreshold(radius * .72f, radius, std::sqrt(a * a + b * b));
  };
  const float stock = .43f + .085f * (fine - .5f) + .12f * (oxidation - .5f);
  const float axis = x - .5f;
  switch (form) {
  case MetalForm::riveted: {
    const float seam = bar(line(y, 2), .13f, .023f);
    const float dy = (std::abs(line(y, 2)) - .082f) * .66f;
    const float dx = line(x, period) / period * 2.f;
    const float rad = std::sqrt(dx * dx + dy * dy);
    const float head = 1.f - softThreshold(.011f, .039f, rad);
    const float grime = (1.f - softThreshold(.023f, .067f, rad)) * (.38f + .62f * oxidation);
    return {unit(.34f + .22f * seam + .38f * head + .06f * fine), 1,
      unit(stock + .12f * seam + .19f * head - .32f * grime), fine};
  }
  case MetalForm::weld: {
    const float drift = .026f * (valueNoise(seed + 4, 3, 9, x, y) - .5f);
    const float bead = bar(axis + drift, .048f + .014f * oxidation, .031f);
    const float scale = period * 5.f;
    const float successive = softThreshold(.12f, .96f, cycleSine(y * scale + 7.0f * axis * axis + drift) * .5f + .5f);
    const float heat = bar(axis + drift, .145f, .11f) * (1.f - .72f * bead);
    return {unit(.42f + .39f * bead * (.74f + .26f * successive) + .03f * fine), 1,
      unit(stock + .25f * bead * (.79f + .21f * successive) - .23f * heat), fine};
  }
  case MetalForm::conduit:
  case MetalForm::pipes:
  case MetalForm::cables: {
    const bool conduit = form == MetalForm::conduit;
    const bool cable = form == MetalForm::cables;
    const float bracket = bar(line(y, conduit ? 2 : period), .105f, .039f);
    const float local = cable ? line(x + .018f * cycleSine(y + .3f), period) : axis;
    const float radius = cable ? .31f : conduit ? .133f : .215f;
    const float cylinder = std::sqrt(unit(1.f - (local * local) / (radius * radius)));
    const float adjoining = cable || conduit ? 0.f :
      std::sqrt(unit(1.f - (std::abs(local) - .373f) * (std::abs(local) - .373f) / .0045f));
    const float collar = bracket * std::max(cylinder, adjoining);
    const float plate = conduit ? rounded(axis, y - .5f, .26f, .35f, .04f) : .08f;
    const float head = conduit ? rounded(std::abs(axis) - .212f, std::abs(y - .5f) - .29f, .019f, .019f, .013f) : 0.f;
    return {unit(.16f + .13f * plate + .49f * cylinder + .30f * adjoining + .13f * collar + .14f * head), 1,
      unit(stock - .24f + .11f * plate + .35f * cylinder + .19f * adjoining + .23f * collar + .14f * head), fine};
  }
  case MetalForm::tray:
  case MetalForm::slotted: {
    const bool tray = form == MetalForm::tray;
    const float lip = bar(std::abs(axis) - .388f, .052f, .018f);
    const float rung = tray ? bar(line(y, period), .103f, .041f) * bar(axis, .399f, .044f) : 0.f;
    const float panel = tray ? 0.f : rounded(axis, y - .5f, .449f, .425f, .027f);
    const float slot = tray ? 0.f : rounded(line(x + 1.f / 6.f, 3), line(y, period), .31f, .21f, .13f) * panel;
    const float head = !tray ? rounded(std::abs(axis) - .408f, std::abs(y - .5f) - .376f, .018f, .019f, .013f) : 0.f;
    const float inside = tray ? std::max(lip, rung) : panel;
    return {unit(.24f + .28f * inside - .17f * slot + .15f * lip + .10f * head + .024f * fine), 1,
      unit((tray ? .19f : stock - .13f) + .26f * inside - .49f * slot + .19f * lip + .12f * head), fine};
  }
  case MetalForm::beam: {
    const float flange = bar(std::abs(axis) - .315f, .11f, .028f);
    const float web = bar(axis, .34f, .066f);
    const float seam = flange * bar(line(y, period), .042f, .025f);
    return {unit(.24f + .20f * web + .29f * flange - .12f * seam + .018f * fine), 1,
      unit(stock - .14f + .11f * web + .18f * flange - .13f * seam), fine};
  }
  case MetalForm::hatch: {
    const float surround = rounded(axis, y - .5f, .454f, .456f, .052f);
    const float door = rounded(axis, y - .5f, .405f, .407f, .047f);
    const float handle = rounded(x - .77f, y - .54f, .070f, .038f, .023f);
    const float lever = rounded(x - .731f, y - .54f, .069f, .013f, .010f);
    const float screw = rounded(std::abs(axis) - .434f, std::abs(y - .5f) - .388f, .014f, .015f, .010f);
    const float hinge = rounded(x - .094f, std::abs(y - .5f) - .244f, .025f, .072f, .017f);
    const float dents = .08f * (valueNoise(seed + 6, period, u, v) - .5f) * door;
    return {unit(.25f + .21f * surround + .12f * door - .09f * handle +
      .24f * lever + .14f * screw + .17f * hinge + dents), 1,
      unit(stock - .13f + .10f * surround + .1f * door - .23f * handle +
        .30f * lever + .16f * screw + .14f * hinge), fine};
  }
  case MetalForm::count: break;
  }
  return {stock, 1, stock, fine};
}

inline float filament(float distance, float radius) {
  return 1.f - softThreshold(radius * .19f, radius, std::abs(distance));
}

inline Pixel rootNetwork(int period, uint64_t seed, float u, float v, float soil, float grit) {
  const float x = frac(u) * period, y = frac(v) * period;
  float height = 0.f, bark = 0.f;
  const auto segment = [](float x, float y, float dx, float dy, float width) {
    const float along = unit((x * dx + y * dy) / (dx * dx + dy * dy));
    x -= dx * along;
    y -= dy * along;
    const float taper = width * mix(.49f, 1.12f, 1.f - along);
    return (1.f - softThreshold(.10f, 1.f, (x * x + y * y) / (taper * taper))) *
      (1.f - softThreshold(.86f, 1.f, along));
  };
  for (int j = int(y) - 2; j <= int(y) + 2; ++j) {
    for (int i = int(x) - 2; i <= int(x) + 2; ++i) {
      const unsigned a = wrap(i, period), b = wrap(j, period);
      if (x < i - 1.56f || x > i + 2.56f || y < j - 1.56f || y > j + 2.56f) continue;
      const float size = lattice(seed, a, b);
      if (size < .15f) continue;
      const float dx = x - i - lattice(seed + 1, a, b);
      const float dy = y - j - lattice(seed + 2, a, b);
      if (dx * dx + dy * dy > 2.37f) continue;
      const float bend = .29f * (soil - .5f) + .066f * (grit - .5f);
      const float turn = lattice(seed + 3, a, b);
      const float ax = dx * cycleSine(turn + .25f) + dy * cycleSine(turn);
      const float ay = dy * cycleSine(turn + .25f) - dx * cycleSine(turn);
      const float span = mix(.46f, .94f, size);
      const float sag = -.23f + .38f * soil;
      const float stem = segment(ax + bend, ay, span, sag, mix(.049f, .128f, size));
      const float runner = segment(ax - span * .78f, ay - sag * .78f, .48f * size + .10f,
        sag * .31f - bend, .051f + .024f * size);
      const float fork = segment(ax - .24f, ay - .24f * sag / span, .42f, mix(.36f, .68f, turn), .048f + .026f * size);
      const float twig = segment(ax - .10f, ay - .10f * sag / span, .27f, -.42f - .19f * size, .028f);
      const float face = std::max({stem, .84f * runner, .70f * fork, .51f * twig});
      if (face <= height) continue;
      height = face;
      bark = face * mix(.32f, .71f, lattice(seed + 4, a, b));
    }
  }
  const float channels = valueNoise(seed + 12, 5, 7, u, v);
  const float route = u * std::max(2, period / 2) + v + .75f * (channels - .5f) +
    .32f * (soil - .5f) + .036f * (grit - .5f);
  const float taper = softThreshold(.22f, .67f, channels) * (.50f + .50f * soil);
  const float root = filament(frac(route) - .5f, .045f + .086f * taper) * taper;
  const float spur = filament(frac(route + .41f * (soil - .25f)) - .5f, .031f) *
    softThreshold(.42f, .71f, soil) * taper;
  height = std::max({height, root, spur});
  bark = std::max({bark, .79f * root, .50f * spur});
  const float bed = .24f + .18f * (grit - .5f) + .20f * (soil - .5f);
  return {unit(bed + .53f * height), height, unit(.18f + .48f * bark + .22f * (soil - .5f) + .22f * (grit - .5f)), grit};
}

inline Pixel fibrousSurface(FibrousKind kind, int period, uint64_t seed, float u, float v) {
  u = frac(u);
  v = frac(v);
  const float bulk = valueNoise(seed, 4, 5, u, v);
  const float detail = valueNoise(seed + 1, period * 7, period * 11, u, v);
  const float fibers = valueNoise(seed + 2, period * 12, period, u + .025f * (bulk - .5f), v);
  if (kind == FibrousKind::roots) return rootNetwork(period, seed + 6, u, v, bulk, detail);
  if (kind == FibrousKind::grime) {
    const float gap = std::abs(v - .5f);
    const float moisture = .67f * bulk + .22f * valueNoise(seed + 3, period, u, v) + .11f * detail;
    const float face = softThreshold(.025f, .37f, (.38f + .51f * moisture) - gap * 1.72f) *
      softThreshold(.21f, .67f, moisture);
    return {.46f + .06f * face * detail, face, .25f + .52f * moisture, detail};
  }
  if (kind == FibrousKind::ribs) {
    const float curl = .17f * (bulk - .5f);
    const float seams = filament(frac((u + curl) * period) - .5f, .14f);
    const float pockets = filament(frac(v * 3.f + .16f * fibers) - .5f, .22f) *
      filament(frac((u + curl) * period + .5f) - .5f, .34f);
    const float rib = seams * (.65f + .35f * fibers);
    const float height = .42f + .31f * rib - .19f * pockets + .11f * (detail - .5f);
    const float tone = .42f + .25f * rib - .22f * pockets + .22f * (bulk - .5f) + .13f * (detail - .5f);
    return {unit(height), 1.f - pockets, unit(tone), detail};
  }
  if (kind == FibrousKind::scutes || kind == FibrousKind::cells) {
    const bool cells = kind == FibrousKind::cells;
    const float x = u + .19f * (bulk - .5f);
    const float y = v * (cells ? 1.f : 2.f) + .18f * (valueNoise(seed + 3, 5, 7, u, v) - .5f);
    const auto distances = cellularDistances(seed + 4, period, .81f, x, y);
    const float margin = distances[1] - distances[0];
    const float gap = softThreshold(.007f, cells ? .097f : .17f, margin + .028f * (detail - .5f));
    const float nucleus = cells ? .35f * filament(distances[0] - .04f, .19f) : 0.f;
    const float layers = .047f * cycleSine(margin * (cells ? 5.0f : 19.f) + detail * .22f);
    const float tone = .18f + .42f * gap + .24f * (bulk - .5f) + layers - nucleus;
    return {unit(.19f + .39f * gap + .078f * (detail - .5f) + layers - .24f * nucleus), gap, unit(tone), detail};
  }
  if (kind == FibrousKind::bark) {
    const float x = u + .084f * (bulk - .5f);
    const float y = v * .46f + .052f * (valueNoise(seed + 3, 8, 9, u, v) - .5f);
    // Use a second periodic axis for the long splits and finer flakes above them.
    const float fibersCrossing = valueNoise(seed + 4, period * 3, period / 2, x, v);
    const float layers = valueNoise(seed + 5, period, std::max(2, int(period * .46f)), x, frac(y + .54f * v));
    const float shifted = x * period + .86f * fibersCrossing + .29f * layers;
    const float split = filament(frac(shifted) - .5f, .15f + .12f * layers);
    const float brokenRidge = filament(frac(v * (period + 5) + .42f * fibersCrossing + .30f * detail) - .5f, .085f) *
      softThreshold(.31f, .80f, .63f * layers + .37f * fibersCrossing);
    const float broken = softThreshold(.38f, .70f, .61f * layers + .39f * detail);
    const float crevice = split * (.45f + .55f * broken);
    const float tone = .52f + .31f * (layers - .5f) + .26f * (detail - .5f) - .27f * crevice - .24f * brokenRidge;
    return {unit(.53f - .39f * crevice + .29f * (layers - .5f) + .16f * (detail - .5f) - .18f * brokenRidge),
      1.f - crevice, unit(tone), fibers};
  }
  if (kind == FibrousKind::tendon) {
    const float grain = valueNoise(seed + 3, period * 2, 3, u, v);
    const float bed = u * period + .38f * (bulk - .5f) + .34f * (grain - .5f);
    const float d = std::abs(frac(bed) - .5f);
    const float crevice = filament(d, .105f);
    const float broken = filament(frac(v * period * 2 + grain * .91f) - .5f, .105f) * (1.f - d);
    const float profile = .49f + .22f * (grain - .5f) + .11f * (fibers - .5f) -
      .17f * crevice - .015f * broken;
    const float tone = .52f + .30f * (grain - .5f) + .22f * (fibers - .5f) - .25f * crevice - .10f * broken;
    return {unit(profile), 1.f - crevice, unit(tone), fibers};
  }
  const bool marble = kind == FibrousKind::marble;
  const bool scar = kind == FibrousKind::scar;
  const bool fracture = kind == FibrousKind::fracture;
  const bool singular = scar || fracture;
  // Periodic displacement keeps a continuing branch on the same path across a tile edge.
  const float drift = valueNoise(seed + 3, 3, 7, u, v);
  const float along = frac(v);
  const float shear = marble ? 1.f : singular ? 0.f : 1.f;
  const float path = (u + shear * v) * period + .82f * (bulk - .5f) +
    .62f * (drift - .5f) + .10f * (detail - .5f);
  const float across = singular ? (u - .5f) * 3.2f + .22f * (drift - .5f) :
    frac(path) - .5f;
  const unsigned segment = unsigned(along * period);
  const float local = frac(along * period);
  const float split = lattice(seed + 4, segment, 0);
  const float taper = softThreshold(.035f, .22f, local) * (1.f - softThreshold(.56f, .96f, local));
  const float direction = split < .46f ? -1.f : 1.f;
  const float branchOffset = direction * local * mix(.24f, .74f, split);
  const float width = marble ? .034f : fracture ? .034f : scar ? .058f : .043f;
  const float main = filament(across, width * mix(.52f, 1.53f, drift));
  const float branch = taper * filament(across - branchOffset - .06f * (detail - .5f), width * .67f);
  const float twigs = taper * filament(across - 1.70f * branchOffset - .13f * (detail - .5f), width * .27f);
  const float mask = std::max({main, .72f * branch, .48f * twigs});
  if (marble) {
    const float cloud = filament(across + .12f * (bulk - .5f), .24f);
    return {.52f + .009f * (detail - .5f), mask,
      unit(.79f + .22f * (bulk - .5f) - .63f * mask - .22f * cloud), detail};
  }
  if (fracture) {
    const float fragments = taper * filament(across, .13f) * detail;
    return {unit(.52f - .39f * mask), unit(mask + .25f * fragments), .12f + .34f * detail, detail};
  }
  return {unit(.49f + .073f * mask + .079f * (detail - .5f)), mask,
    unit(.56f + .26f * (bulk - .5f) + .10f * (detail - .5f) + (scar ? .34f : -.21f) * mask), detail};
}

inline float liquidFoam(int period, uint64_t seed, float u, float v, float phase) {
  const float drift = valueNoise(seed, 3, 5, u + .08f * cycleSine(phase), v);
  const float x = frac(u + .16f * (drift - .5f) - phase);
  const float y = frac(v + .10f * (drift - .5f));
  const float pool = valueNoise(seed + 1, 3, 5, x, y);
  const float dist = cellularDistance(seed + 2, period, .91f, x, y);
  const float size = .13f + .27f * valueNoise(seed + 4, std::max(2, period / 2), x, y);
  const float rim = std::abs(dist - size);
  const float curvature = unit(1.f - dist / size);
  const float bubbles = (.70f * (1.f - softThreshold(.011f, .059f, rim)) + .30f * curvature) *
    softThreshold(.36f, .68f, pool);
  const float micro = mineralMarks(seed + 3, period * 4, .73f, x, y, .10f, .37f, 1.f);
  return unit(.28f + .24f * pool + .29f * bubbles + .21f * micro);
}

inline float coolingCrust(int period, uint64_t seed, float u, float v, float phase) {
  const float pulse = cycleSine(phase);
  const float warpX = valueNoise(seed, 4, 7, u, v + .031f * pulse);
  const float warpY = valueNoise(seed + 1, 7, 4, u, v - phase);
  const float fold = valueNoise(seed + 2, 12, 6, u, v);
  const float x = u + v + .37f * (warpX - .5f) + .022f * fold;
  const float y = v + .28f * (warpY - .5f);
  const auto cells = cellularDistances(seed + 3, period, .96f, x, y);
  const float gap = cells[1] - cells[0];
  const float margin = softThreshold(.012f, .21f, gap);
  const float plates = .42f + .36f * warpX + .13f * fold;
  return unit(gap * .91f + margin * plates);
}

inline float dischargeFilaments(int period, uint64_t seed, float u, float v, float phase, bool corona) {
  const float drift = valueNoise(seed, 5, 8, u, frac(v - phase));
  const float jitter = valueNoise(seed + 1, 18, 19, u, v);
  const float cloud = valueNoise(seed + 3, 3, 4, u + phase, v);
  const float path = u * period + v * (corona ? 2.f : -1.f) +
    (corona ? .97f : 1.70f) * (drift - .5f) + .40f * (jitter - .5f) + 1.21f * (cloud - .5f);
  const float width = corona ? .12f : .09f;
  const float line = filament(frac(path) - .5f, width);
  const float halo = filament(frac(path) - .5f, width * 3.10f);
  const float branch = frac(v * 4.f - phase + (drift - .5f) * .35f);
  const float taper = softThreshold(.04f, .23f, branch) * (1.f - softThreshold(.57f, .93f, branch));
  const float fork = (.54f + .24f * jitter) * taper * (2.7f * (drift - .5f));
  const float sideline = filament(frac(path + fork) - .5f, width * .45f) * taper;
  const float travel = valueNoise(seed + 2, 7, 6, u - phase, v);
  const float flicker = softThreshold(.22f, .78f, .58f * travel + .42f * cloud);
  return unit(.028f + .78f * line * (.13f + .87f * flicker) + (.059f + .12f * flicker) * halo +
    .32f * sideline * flicker);
}
