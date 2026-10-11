#include "new_effects.h"

#include "spatial.h"

static uint8_t absDiff8(uint8_t a, uint8_t b) {
  return a > b ? a - b : b - a;
}

static uint8_t circularDistance8(uint8_t a, uint8_t b) {
  uint8_t distance = absDiff8(a, b);
  return distance > 128U ? (uint8_t)(256U - distance) : distance;
}

static uint8_t roomCoordToByte(int16_t value) {
  if (value < 2) value = 2;
  if (value > 18) value = 18;
  return (uint8_t)(((int32_t)(value - 2) * 255L) / 16L);
}

static uint16_t effectSeed(const EffectContext& ctx, uint16_t salt = 0) {
  uint32_t value = (uint32_t)ctx.node.nodeId * 197U ^
                   (uint32_t)ctx.node.x * 251U ^
                   (uint32_t)ctx.node.y * 397U ^
                   (uint32_t)ctx.node.z * 607U ^
                   (uint32_t)ctx.node.axis * 911U ^
                   (uint32_t)ctx.node.orientation * 1237U ^ salt;
  value ^= value >> 16;
  value *= 0x7feb352dUL;
  value ^= value >> 15;
  return (uint16_t)(value ^ (value >> 16));
}

static uint16_t hash16(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dUL;
  value ^= value >> 15;
  value *= 0x846ca68bUL;
  value ^= value >> 16;
  return (uint16_t)value;
}

static uint8_t angleToByte(int16_t a, int16_t b) {
  if (a == 0 && b == 0) return 0;
  uint16_t aa = a < 0 ? (uint16_t)-a : (uint16_t)a;
  uint16_t ab = b < 0 ? (uint16_t)-b : (uint16_t)b;
  uint8_t q = aa >= ab
    ? (uint8_t)((ab * 32U) / max<uint16_t>(1, aa))
    : (uint8_t)(64U - ((aa * 32U) / max<uint16_t>(1, ab)));
  if (a >= 0 && b >= 0) return q;
  if (a < 0 && b >= 0) return 128U - q;
  if (a < 0 && b < 0) return 128U + q;
  return 255U - q;
}

static bool cyclePhase(const EffectContext& ctx, uint8_t* phaseOut) {
  uint16_t cycleMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  uint32_t age = ctx.params.fxSpeed == 0 ? ctx.beatAgeMs : ctx.phaseMs;
  if (ctx.params.fxSpeed == 0 && age >= cycleMs) {
    if (phaseOut) *phaseOut = 255;
    return false;
  }
  uint16_t phaseMs = (uint16_t)(age % max<uint16_t>(1, cycleMs));
  if (phaseOut) {
    *phaseOut = (uint8_t)(((uint32_t)phaseMs * 255UL) / max<uint16_t>(1, cycleMs));
  }
  return true;
}

static uint8_t softBand(uint8_t distance, uint8_t width, uint8_t softness) {
  if (distance <= width) return 255;
  if (softness == 0 || distance >= qadd8(width, softness)) return 0;
  return 255U - (uint8_t)(((uint16_t)(distance - width) * 255U) / softness);
}

static CRGB scaled(CRGB color, uint8_t level) {
  color.nscale8_video(level);
  return color;
}

static uint16_t bpmBeatMs(const EffectContext& ctx) {
  uint32_t duration = ctx.params.bpm > 0 ? 60000UL / ctx.params.bpm : 1000UL;
  if (duration < 20UL) duration = 20UL;
  if (duration > 60000UL) duration = 60000UL;
  return (uint16_t)duration;
}

static uint16_t durationFromControl(const EffectContext& ctx, uint8_t control) {
  const uint32_t beatMs = bpmBeatMs(ctx);
  uint32_t duration = beatMs - ((beatMs * 15UL * control) / (16UL * 255UL));
  if (duration < 1UL) duration = 1UL;
  return (uint16_t)duration;
}

static bool phaseForDuration(const EffectContext& ctx, uint16_t durationMs, uint8_t* phaseOut) {
  const uint32_t age = ctx.params.fxSpeed == 0 ? ctx.beatAgeMs : ctx.phaseMs;
  if (ctx.params.fxSpeed == 0 && age >= durationMs) {
    if (phaseOut) *phaseOut = 255;
    return false;
  }
  const uint32_t phaseMs = age % max<uint16_t>(1, durationMs);
  if (phaseOut) {
    *phaseOut = (uint8_t)((phaseMs * 255UL) / max<uint16_t>(1, durationMs));
  }
  return true;
}

static uint8_t projectRoomAxis(int16_t horizontal2, int16_t vertical2, uint8_t angle) {
  const int16_t horizontal = (int16_t)roomCoordToByte(horizontal2) - 128;
  const int16_t vertical = (int16_t)roomCoordToByte(vertical2) - 128;
  const int16_t cosine = (int16_t)cos8(angle) - 128;
  // Positive quarter-turn means travel from ceiling toward the floor.
  const int16_t sine = 128 - (int16_t)sin8(angle);
  int32_t projected = 128L +
    (((int32_t)horizontal * cosine + (int32_t)vertical * sine) / 128L);
  if (projected < 0) projected = 0;
  if (projected > 255) projected = 255;
  return (uint8_t)projected;
}

static uint8_t curvedProgress(uint8_t phase, uint8_t curve) {
  const uint8_t easeIn = (uint8_t)(((uint16_t)phase * phase) / 255U);
  const uint8_t inverse = 255U - phase;
  const uint8_t easeOut = 255U - (uint8_t)(((uint16_t)inverse * inverse) / 255U);
  if (curve < 127U) {
    const uint8_t strength = (uint8_t)((127U - curve) * 2U);
    return blend8(phase, easeIn, strength);
  }
  const uint8_t strength = (uint8_t)min<uint16_t>(255U, (uint16_t)(curve - 127U) * 2U);
  return blend8(phase, easeOut, strength);
}

static void retainLight(CRGB& previous, const CRGB& next) {
  previous.r = max<uint8_t>(previous.r, next.r);
  previous.g = max<uint8_t>(previous.g, next.g);
  previous.b = max<uint8_t>(previous.b, next.b);
}

// Signed inclination about the horizontal plane, within the forward hemisphere.
static uint8_t laserElevation(uint8_t value) {
  return (uint8_t)((int16_t)((uint16_t)value * 80U / 255U) - 40);
}

static void effectNegativePulse(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t phase = 0;
  if (!cyclePhase(ctx, &phase)) {
    fill_solid(leds, count, ctx.params.mainColor);
    return;
  }
  uint8_t darkness = sin8(phase);
  CRGB color = ctx.params.mainColor;
  color.nscale8_video(255U - darkness);
  fill_solid(leds, count, color);
}

static void effectGravity(CRGB* leds, uint16_t count, const EffectContext& ctx, bool outward) {
  uint8_t phase = 0;
  cyclePhase(ctx, &phase);
  uint8_t density = map(ctx.params.fxExpand, 0, 255, 3, 18);
  uint8_t wellRadius = map(ctx.params.fxFade, 0, 255, 8, 72);
  uint8_t particleWidth = map(ctx.params.fxWarp, 0, 255, 2, 16);
  uint16_t seed = effectSeed(ctx, outward ? 0xA17U : 0x51BU);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t radial = (uint8_t)(sample.distanceNorm >> 8);
    uint8_t travel = outward ? (uint8_t)(radial - phase) : (uint8_t)(radial + phase);
    uint8_t cell = (uint8_t)(((uint16_t)travel * density) >> 8);
    uint16_t h = hash16(seed ^ ((uint32_t)cell << 16) ^ (i * 131U));
    uint8_t center = (uint8_t)h;
    uint8_t local = (uint8_t)(((uint16_t)travel * density) & 0xFFU);
    uint8_t particle = softBand(circularDistance8(local, center), particleWidth, particleWidth);
    uint8_t centerGlow = radial >= wellRadius
      ? 0
      : 255U - (uint8_t)(((uint16_t)radial * 255U) / max<uint8_t>(1, wellRadius));
    uint8_t densityBoost = qadd8(48, 255U - radial);
    uint8_t amount = qadd8(scale8(particle, densityBoost), scale8(centerGlow, 170));
    CRGB color = blend(ctx.params.accentColor, ctx.params.mainColor, amount);
    leds[i] = scaled(color, amount);
  }
}

static void effectFloorWave(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t phase = 0;
  bool active = cyclePhase(ctx, &phase);
  uint8_t duty = map(ctx.params.fxExpand, 0, 255, 8, 100);
  uint8_t decay = map(ctx.params.fxFade, 0, 255, 8, 96);
  int16_t amplitude2 = map(ctx.params.fxWarp, 0, 255, 1, 16);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t radial = (uint8_t)(sample.horizontalNorm >> 8);
    uint8_t distance = active ? circularDistance8(radial, phase) : 255;
    uint8_t crest = softBand(distance, duty / 2U, decay);
    int16_t surfaceZ2 = 2 + ((int32_t)crest * amplitude2) / 255L;
    int16_t verticalDelta = sample.nodeZ2 - surfaceZ2;
    uint8_t vertical = verticalDelta <= 0
      ? 255
      : (verticalDelta >= 4 ? 0 : (uint8_t)(255 - verticalDelta * 64));
    uint8_t amount = scale8_video(crest, vertical);
    leds[i] = scaled(ctx.params.mainColor, amount);
  }
}

static void effectStarfield(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t density = map(ctx.params.fxExpand, 0, 255, 3, 70);
  uint8_t starSize = map(ctx.params.fxFade, 0, 255, 1, 5);
  uint8_t twinkle = map(ctx.params.fxWarp, 0, 255, 8, 255);
  uint16_t seed = effectSeed(ctx, 0x57A2U);
  uint32_t slowTime = ctx.nowMs / 80U;

  for (uint16_t i = 0; i < count; ++i) {
    CRGB result = CRGB::Black;
    uint8_t brightest = 0;
    for (uint8_t offset = 0; offset < starSize; ++offset) {
      const int32_t center = (int32_t)i + offset - starSize / 2U;
      if (center < 0 || center >= count) continue;
      const uint16_t h = hash16(seed ^ (uint32_t)center * 4051U);
      if ((uint8_t)h >= density) continue;
      const uint8_t shimmer = sin8((uint8_t)(slowTime + (h >> 8)));
      const uint8_t amount = qadd8(48, scale8(shimmer, twinkle));
      if (amount > brightest) {
        brightest = amount;
        result = scaled((h & 0x100U) ? ctx.params.mainColor : ctx.params.accentColor, amount);
      }
    }
    leds[i] = result;
  }
}

static void effectBlackoutCollapse(CRGB* leds, uint16_t count, const EffectContext& ctx, bool expanding) {
  uint8_t phase = 0;
  const uint16_t durationMs = durationFromControl(ctx, ctx.params.fxExpand);
  bool active = phaseForDuration(ctx, durationMs, &phase);
  if (!active) {
    fill_solid(leds, count, expanding ? ctx.params.mainColor : ctx.params.accentColor);
    return;
  }
  uint8_t softness = map(ctx.params.fxFade, 0, 255, 2, 80);
  uint8_t boundary = expanding ? phase : (uint8_t)(255U - phase);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t radial = (uint8_t)(sample.distanceNorm >> 8);
    uint8_t amount = 0;
    if (radial <= boundary) {
      amount = 255;
    } else if (radial < qadd8(boundary, softness)) {
      amount = 255U - (uint8_t)(((uint16_t)(radial - boundary) * 255U) / max<uint8_t>(1, softness));
    }
    leds[i] = blend(ctx.params.accentColor, ctx.params.mainColor, amount);
  }
}

static void effectWaterSlosh(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t phase = 0;
  cyclePhase(ctx, &phase);
  int16_t tilt = (int16_t)map(ctx.params.fxWarp, 0, 255, 0, 10);
  int16_t rocking = ((int16_t)sin8(phase) - 128) * tilt;
  uint8_t thickness = map(ctx.params.fxExpand, 0, 255, 4, 72);
  uint8_t blendWidth = map(ctx.params.fxFade, 0, 255, 2, 96);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    int16_t slope = ((int32_t)sample.dx2 * rocking) / 128L;
    int16_t surface = sample.stageZ2 + slope;
    int16_t delta = (sample.nodeZ2 - surface) * 16;
    uint8_t mix;
    int16_t half = thickness / 2;
    if (delta <= -half) {
      mix = 0;
    } else if (delta >= half + blendWidth) {
      mix = 255;
    } else {
      int16_t span = max<int16_t>(1, thickness + blendWidth);
      mix = (uint8_t)(((int32_t)(delta + half) * 255L) / span);
    }
    leds[i] = blend(ctx.params.mainColor, ctx.params.accentColor, mix);
  }
}

static void effectRoomLightning(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t phase = 0;
  bool active = cyclePhase(ctx, &phase);
  uint8_t hold = map(ctx.params.fxFade, 0, 255, 8, 116);
  const uint8_t travel = map(ctx.params.fxWarp, 0, 255, 128, 16);
  if (!active || phase > (uint16_t)travel + hold) {
    fill_solid(leds, count, CRGB::Black);
    return;
  }
  uint8_t thickness = map(ctx.params.fxExpand, 0, 255, 1, 8);
  uint8_t reachedZ = phase >= travel ? 255 : ((uint16_t)phase * 255U) / travel;
  uint16_t strike = hash16((ctx.phaseMs / max<uint16_t>(1, computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed))) ^ 0xB017U);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t z = 255U - roomCoordToByte(sample.nodeZ2);
    if (z > reachedZ) {
      leds[i] = CRGB::Black;
      continue;
    }
    int8_t bendX = (int8_t)(inoise8(strike, sample.nodeZ2 * 31U) - 128) / 24;
    int8_t bendY = (int8_t)(inoise8(strike + 91U, sample.nodeZ2 * 37U) - 128) / 24;
    uint8_t distance = (uint8_t)(min<uint16_t>(255,
      (uint16_t)abs(sample.dx2 - bendX) * 12U + (uint16_t)abs(sample.dy2 - bendY) * 12U));
    uint8_t amount = softBand(distance, thickness * 4U, thickness * 7U);
    const uint8_t decay = phase > travel ? phase - travel : 0;
    amount = scale8_video(amount, 255U - (uint8_t)(((uint16_t)decay * 255U) / hold));
    CRGB color = blend(ctx.params.accentColor, ctx.params.mainColor, amount);
    leds[i] = scaled(color, amount);
  }
}

static void effectRain(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t density = map(ctx.params.fxExpand, 0, 255, 5, 96);
  uint8_t trail = map(ctx.params.fxFade, 0, 255, 6, 80);
  uint8_t speedDiv = map(ctx.params.fxWarp, 0, 255, 28, 3);
  uint16_t seed = effectSeed(ctx, 0x2A11U);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint16_t columnSeed = hash16(seed ^ ((uint32_t)sample.nodeX2 << 8) ^ sample.nodeY2);
    uint8_t columnChance = (uint8_t)columnSeed;
    uint8_t ceilingGlow = roomCoordToByte(sample.nodeZ2) > 232 ? 54 : 0;
    uint8_t amount = ceilingGlow;
    if (columnChance < density) {
      uint8_t head = (uint8_t)(255U - (ctx.phaseMs / speedDiv + (columnSeed >> 8)));
      uint8_t z = roomCoordToByte(sample.nodeZ2);
      uint8_t behind = (uint8_t)(z - head);
      if (behind < trail) {
        amount = max<uint8_t>(amount, 255U - (uint8_t)(((uint16_t)behind * 255U) / max<uint8_t>(1, trail)));
      }
    }
    CRGB color = blend(ctx.params.accentColor, ctx.params.mainColor, amount);
    leds[i] = scaled(color, amount);
  }
}

static void effectWarpRing(CRGB* leds, uint16_t count, const EffectContext& ctx, bool horizontal) {
  uint8_t phase = 0;
  cyclePhase(ctx, &phase);
  uint8_t width = map(ctx.params.fxExpand, 0, 255, 2, 40);
  uint8_t trail = map(ctx.params.fxFade, 0, 255, 2, 96);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    const int16_t travelAxis = horizontal ? sample.nodeX2 : 20 - sample.nodeY2;
    uint8_t position = projectRoomAxis(travelAxis, sample.nodeZ2, ctx.params.fxWarp);
    uint8_t behind = (uint8_t)(phase - position);
    uint8_t amount = behind <= width
      ? 255
      : (behind < qadd8(width, trail)
          ? 255U - (uint8_t)(((uint16_t)(behind - width) * 255U) / max<uint8_t>(1, trail))
          : 0);
    CRGB color = blend(ctx.params.accentColor, ctx.params.mainColor, amount);
    leds[i] = scaled(color, amount);
  }
}

static void effectExplosionPre(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t phase = 0;
  cyclePhase(ctx, &phase);
  uint8_t particleDensity = map(ctx.params.fxExpand, 0, 255, 3, 20);
  uint8_t orbRadius = map(ctx.params.fxFade, 0, 255, 8, 72);
  uint8_t speed = map(ctx.params.fxWarp, 0, 255, 1, 5);
  uint8_t pulse = sin8((uint8_t)(phase * 4U));
  uint16_t seed = effectSeed(ctx, 0xE12EU);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t radial = (uint8_t)(sample.distanceNorm >> 8);
    uint8_t orb = radial >= orbRadius
      ? 0
      : 255U - (uint8_t)(((uint16_t)radial * 255U) / max<uint8_t>(1, orbRadius));
    orb = scale8_video(orb, qadd8(96, pulse / 2U));
    uint8_t particlePhase = (uint8_t)(radial + phase * speed);
    uint8_t cell = (uint8_t)(((uint16_t)particlePhase * particleDensity) >> 8);
    uint16_t h = hash16(seed ^ ((uint32_t)cell << 12) ^ i * 313U);
    uint8_t particle = circularDistance8((uint8_t)(particlePhase * particleDensity), (uint8_t)h) < 8 ? (uint8_t)(h >> 8) : 0;
    uint8_t amount = qadd8(orb, particle);
    CRGB color = blend(ctx.params.accentColor, ctx.params.mainColor, orb);
    leds[i] = scaled(color, amount);
  }
}

static void effectExplosionShell(CRGB* leds, uint16_t count, const EffectContext& ctx, bool inward) {
  uint8_t phase = 0;
  bool active = cyclePhase(ctx, &phase);
  if (!active) {
    fill_solid(leds, count, inward ? ctx.params.mainColor : CRGB::Black);
    return;
  }
  uint8_t speed = map(ctx.params.fxWarp, 0, 255, 1, 4);
  uint8_t center = (uint8_t)(phase * speed);
  if (inward) center = 255U - center;
  uint8_t width = map(ctx.params.fxExpand, 0, 255, 2, 48);
  uint8_t softness = map(ctx.params.fxFade, 0, 255, 2, 90);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t radial = (uint8_t)(sample.distanceNorm >> 8);
    uint8_t distance = absDiff8(radial, center);
    uint8_t amount = softBand(distance, width / 2U, softness);
    uint8_t edge = distance <= width / 2U ? 255 : amount;
    CRGB color = blend(ctx.params.mainColor, ctx.params.accentColor, edge);
    leds[i] = scaled(color, amount);
  }
}

static void effectCorkscrew(CRGB* leds, uint16_t count, const EffectContext& ctx, bool downward) {
  uint8_t phase = 0;
  cyclePhase(ctx, &phase);
  uint8_t turns = map(ctx.params.fxExpand, 0, 255, 1, 8);
  uint8_t width = map(ctx.params.fxFade, 0, 255, 2, 48);
  uint8_t softness = max<uint8_t>(2, width / 2U);
  int16_t signedSpeed = (int16_t)ctx.params.fxWarp - 127;
  uint8_t speed = (uint8_t)min<uint16_t>(255U, (uint16_t)abs(signedSpeed) * 2U);
  int16_t spin = scale8(phase, speed);
  if (signedSpeed < 0) spin = -spin;
  if (downward) spin = -spin;

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t angle = angleToByte(sample.dx2, sample.dy2);
    uint8_t height = roomCoordToByte(sample.nodeZ2);
    uint8_t spiral = (uint8_t)(height * turns + spin);
    uint8_t amount = softBand(circularDistance8(angle, spiral), width / 2U, softness);
    CRGB color = blend(ctx.params.accentColor, ctx.params.mainColor, amount);
    leds[i] = scaled(color, amount);
  }
}

static uint8_t laserDistance(const SpatialFieldSample& sample, uint8_t azimuth, uint8_t elevation) {
  const uint8_t angle = angleToByte(sample.dx2, sample.dy2);
  const uint16_t x = abs(sample.dx2), y = abs(sample.dy2);
  const uint16_t horizontal = max<uint16_t>(x, y) + min<uint16_t>(x, y) / 2U;
  const uint8_t pixelElevation = angleToByte((int16_t)horizontal, sample.dz2);
  return max<uint8_t>(circularDistance8(angle, azimuth),
                      circularDistance8(pixelElevation, elevation));
}

static void effectLaserBeams(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t phase = 0;
  cyclePhase(ctx, &phase);
  const uint8_t lineCount = map(ctx.params.fxExpand, 0, 255, 2, 24);
  const uint8_t width = 6;
  const uint8_t spin = scale8(phase, ctx.params.fxWarp);
  const uint16_t fadeMs = durationFromControl(ctx, ctx.params.fxFade);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t amount = 0;
    uint8_t selectedLine = 0;
    for (uint8_t line = 0; line < lineCount; ++line) {
      const uint8_t target = (uint8_t)(((uint16_t)line * 256U) / lineCount + spin);
      const uint8_t targetElevation = laserElevation(((uint16_t)line * 255U) / max<uint8_t>(1, lineCount - 1U));
      uint8_t beam = softBand(laserDistance(sample, target, targetElevation), width, width);
      if (ctx.params.fxFade > 0) {
        const uint32_t offset = ((uint32_t)hash16(line * 977U + 0x1A53U) * fadeMs) / 65535UL;
        const uint32_t fadePhase = (ctx.phaseMs + offset) % max<uint32_t>(1, (uint32_t)fadeMs * 2UL);
        const uint8_t gate = fadePhase < fadeMs
          ? (uint8_t)(255UL - (fadePhase * 255UL) / fadeMs)
          : (uint8_t)(((fadePhase - fadeMs) * 255UL) / fadeMs);
        beam = scale8_video(beam, gate);
      }
      if (beam > amount) {
        amount = beam;
        selectedLine = line;
      }
    }
    CRGB color = (selectedLine & 1U) ? ctx.params.accentColor : ctx.params.mainColor;
    leds[i] = scaled(color, amount);
  }
}

static void effectLaserBeamsRandom(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  const uint16_t stepMs = durationFromControl(ctx, ctx.params.fxExpand);
  const uint32_t step = ctx.phaseMs / stepMs;
  const uint32_t age = ctx.phaseMs % stepMs;
  const uint16_t fadeMs = durationFromControl(ctx, ctx.params.fxFade);
  const uint8_t history = ctx.params.fxFade == 0 ? 0 : min<uint32_t>(step, fadeMs / stepMs);

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    CRGB result = CRGB::Black;
    for (uint8_t back = 0; back <= history; ++back) {
      const uint32_t beamAge = age + (uint32_t)back * stepMs;
      if (ctx.params.fxFade > 0 && beamAge >= fadeMs) continue;
      const uint32_t sequence = step - back + ctx.params.fxWarp;
      const uint16_t seed = hash16(sequence ^ 0x1A53U);
      uint8_t amount = softBand(laserDistance(sample, (uint8_t)seed, laserElevation(seed >> 8)), 6, 8);
      const uint8_t gate = ctx.params.fxFade == 0 ? 255 : 255UL - beamAge * 255UL / fadeMs;
      amount = scale8_video(amount, gate);
      retainLight(result, scaled((sequence & 1U) ? ctx.params.accentColor : ctx.params.mainColor, amount));
    }
    if (ctx.params.fxFade == 0) retainLight(leds[i], result);
    else leds[i] = result;
  }
}

static void effectParticleField(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t phase = 0;
  cyclePhase(ctx, &phase);
  const uint8_t bulletCount = map(ctx.params.fxExpand, 0, 255, 2, 28);
  const uint16_t cycleMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  const uint16_t fadeMs = durationFromControl(ctx, ctx.params.fxFade);
  const uint8_t tail = ctx.params.fxFade == 0 ? 8 :
    min<uint32_t>(255U, max<uint32_t>(8U, (uint32_t)fadeMs * 255U / cycleMs));
  // A single deterministic room field: device ID, wiring and strip placement
  // must not change a particle's trajectory or phase at an intersection.
  const uint8_t angle = (uint8_t)(((uint16_t)ctx.params.fxWarp * 256U) / 255U);
  const int16_t cosine = (int16_t)cos8(angle) - 128;
  const int16_t sine = (int16_t)sin8(angle) - 128;
  const int16_t extent = max<int16_t>(1, abs(cosine) + abs(sine));
  uint16_t bullets[28], lanes[28];
  for (uint8_t bullet = 0; bullet < bulletCount; ++bullet) {
    bullets[bullet] = hash16(0xB117U ^ ((uint32_t)bullet * 4051U));
    lanes[bullet] = hash16(0x71C3U ^ ((uint32_t)bullet * 977U));
  }

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    const int16_t y = (int16_t)roomCoordToByte(sample.nodeY2) - 128;
    const int16_t z = (int16_t)roomCoordToByte(sample.nodeZ2) - 128;
    // Rotate both travel and its perpendicular lane axis about room X.
    // 0: front to back; 90: ceiling to floor; 180/270 reverse these.
    const int16_t travel = (-(int32_t)y * cosine - (int32_t)z * sine) / 128L;
    const int16_t across = ((int32_t)z * cosine - (int32_t)y * sine) / 128L;
    const uint8_t travelAxis = ((int32_t)(travel + extent) * 255L) / (2L * extent);
    const uint8_t laneAxis = ((int32_t)(across + extent) * 255L) / (2L * extent);
    const uint8_t crossAxis = roomCoordToByte(sample.nodeX2);
    uint8_t amount = 0;
    uint8_t selectedBullet = 0;
    for (uint8_t bullet = 0; bullet < bulletCount; ++bullet) {
      const uint16_t h = bullets[bullet];
      const uint8_t head = (uint8_t)(phase + (uint8_t)h);
      const uint8_t behind = (uint8_t)(head - travelAxis);
      if (behind > tail) continue;
      const uint8_t lane = (uint8_t)(h >> 8);
      const uint8_t laneAmount = scale8_video(
        softBand(absDiff8(crossAxis, lane), 5, 8),
        softBand(absDiff8(laneAxis, (uint8_t)lanes[bullet]), 5, 8));
      const uint8_t trailAmount = ctx.params.fxFade == 0
        ? 255
        : (uint8_t)(255U - ((uint16_t)behind * 255U) / max<uint8_t>(1, tail));
      const uint8_t bulletAmount = scale8_video(laneAmount, trailAmount);
      if (bulletAmount > amount) {
        amount = bulletAmount;
        selectedBullet = bullet;
      }
    }
    const CRGB result = scaled((selectedBullet & 1U) ? ctx.params.accentColor : ctx.params.mainColor, amount);
    if (ctx.params.fxFade == 0) retainLight(leds[i], result);
    else leds[i] = result;
  }
}

static void effectRaiseUp(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  const uint16_t cycleMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  const uint16_t fadeMs = durationFromControl(ctx, ctx.params.fxFade);
  const uint32_t age = ctx.params.fxSpeed == 0 ? ctx.beatAgeMs : ctx.phaseMs;
  const uint32_t localAge = ctx.params.fxSpeed == 0 ? age : age % cycleMs;

  for (uint16_t i = 0; i < count; ++i) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    const uint8_t height = roomCoordToByte(sample.nodeZ2);
    uint8_t amount = 0;
    // Invert the growth curve to get the time this physical height was reached.
    uint16_t low = 0, high = 255;
    while (low < high) {
      const uint16_t mid = (low + high) / 2U;
      if (curvedProgress(mid, ctx.params.fxWarp) >= height) high = mid;
      else low = mid + 1U;
    }
    const uint32_t arrivalMs = ((uint32_t)low * cycleMs + 254U) / 255U;
    int32_t sinceArrival = (int32_t)localAge - arrivalMs;
    if (sinceArrival < 0 && ctx.params.fxSpeed != 0 && age >= cycleMs && ctx.params.fxFade > 0) {
      sinceArrival += cycleMs;
    }
    if (sinceArrival >= 0) {
      amount = ctx.params.fxFade == 0 ? 255 :
        (sinceArrival >= fadeMs ? 0 : 255UL - ((uint32_t)sinceArrival * 255UL) / fadeMs);
    }
    const CRGB color = blend(ctx.params.mainColor, ctx.params.accentColor, height);
    leds[i] = scaled(color, amount);
  }
}

bool renderNewEffect(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  switch (ctx.params.lightEffect) {
    case 26: effectNegativePulse(leds, count, ctx); return true;
    case 27: effectGravity(leds, count, ctx, false); return true;
    case 28: effectGravity(leds, count, ctx, true); return true;
    case 29: effectFloorWave(leds, count, ctx); return true;
    case 30: effectStarfield(leds, count, ctx); return true;
    case 31: effectBlackoutCollapse(leds, count, ctx, false); return true;
    case 32: effectBlackoutCollapse(leds, count, ctx, true); return true;
    case 33: effectWaterSlosh(leds, count, ctx); return true;
    case 34: effectRoomLightning(leds, count, ctx); return true;
    case 35: effectRain(leds, count, ctx); return true;
    case 36: effectWarpRing(leds, count, ctx, false); return true;
    case 37: effectWarpRing(leds, count, ctx, true); return true;
    case 38: effectExplosionPre(leds, count, ctx); return true;
    case 39: effectExplosionShell(leds, count, ctx, false); return true;
    case 40: effectExplosionShell(leds, count, ctx, true); return true;
    case 41: effectCorkscrew(leds, count, ctx, false); return true;
    case 42: effectCorkscrew(leds, count, ctx, true); return true;
    case 43: effectLaserBeams(leds, count, ctx); return true;
    case 44: effectLaserBeamsRandom(leds, count, ctx); return true;
    case 45: effectParticleField(leds, count, ctx); return true;
    case 46: effectRaiseUp(leds, count, ctx); return true;
    default: return false;
  }
}

bool effectUsesIntrinsicSpatialGeometry(uint8_t lightEffect) {
  return lightEffect == 25U || (lightEffect >= 27U && lightEffect <= 46U);
}
