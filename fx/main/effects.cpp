#include "effects.h"
#include "fx_package.h"
#include "new_effects.h"
#include "node_config.h"
#include "spatial.h"

static uint16_t safeCount(uint16_t count) {
  return count == 0 ? 1 : count;
}

CRGB presetToRgb(uint8_t v) {
  if (v == 0) return CRGB::Black;

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  if (ah_fx_color_preset_rgb(v, &r, &g, &b)) {
    return CRGB(r, g, b);
  }

  // Rainbow/auto color zone.
  return CHSV(millis() / 12, 255, 255);
}

static uint16_t pixelIndex(uint16_t i, uint16_t count, const EffectContext& ctx) {
  if (!ctx.spatial.reversePixels) return i;
  return (count - 1) - i;
}

static uint8_t fadeAmount(const EffectContext& ctx) {
  return map(ctx.params.fxFade, 0, 255, 255, 20);
}

static bool beatMode(const EffectContext& ctx) {
  return ctx.params.fxSpeed == 0;
}

static bool effectBeatPhase(const EffectContext& ctx, uint16_t beatMs, uint16_t *phaseOut) {
  if (beatMs == 0) beatMs = 1000;
  uint32_t age = beatMode(ctx) ? ctx.beatAgeMs : ctx.phaseMs;
  if (beatMode(ctx) && age >= beatMs) {
    if (phaseOut) *phaseOut = beatMs;
    return false;
  }
  if (phaseOut) {
    *phaseOut = beatMode(ctx) ? (uint16_t)age : (uint16_t)(age % beatMs);
  }
  return true;
}

static void effectSolid(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  fill_solid(leds, count, ctx.params.mainColor);
}

static void effectPulse(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  uint16_t phase = 0;
  if (!effectBeatPhase(ctx, beatMs, &phase)) {
    fill_solid(leds, count, CRGB::Black);
    return;
  }
  uint8_t wave = sin8(((uint32_t)phase * 255UL) / beatMs);

  CRGB c = ctx.params.mainColor;
  c.nscale8_video(wave);
  fill_solid(leds, count, c);
}

static void effectWipe(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  fadeToBlackBy(leds, count, fadeAmount(ctx));

  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  uint16_t phase = 0;
  if (!effectBeatPhase(ctx, beatMs, &phase)) return;
  uint16_t lit = ((uint32_t)phase * (count + 1UL)) / beatMs;
  if (lit > count) lit = count;

  for (uint16_t i = 0; i < lit; i++) {
    leds[pixelIndex(i, count, ctx)] = ctx.params.mainColor;
  }
}

static void effectGradient(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);

  if (count <= 1) {
    leds[0] = ctx.params.mainColor;
    return;
  }

  for (uint16_t i = 0; i < count; i++) {
    uint8_t amount = ((uint32_t)i * 255UL) / (count - 1);
    leds[pixelIndex(i, count, ctx)] = blend(ctx.params.mainColor, ctx.params.accentColor, amount);
  }
}

static void effectChase(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  fadeToBlackBy(leds, count, fadeAmount(ctx));

  uint16_t pos = 0;
  if (beatMode(ctx)) {
    uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
    uint16_t phase = 0;
    if (!effectBeatPhase(ctx, beatMs, &phase)) return;
    pos = ((uint32_t)phase * count) / beatMs;
    if (pos >= count) pos = count - 1;
  } else {
    uint8_t speed = map(ctx.params.fxSpeed, 0, 255, 30, 1);
    pos = (ctx.phaseMs / speed) % count;
  }

  leds[pixelIndex(pos, count, ctx)] = ctx.params.mainColor;

  if (count > 2) {
    uint16_t pos2 = (pos + count / 2) % count;
    leds[pixelIndex(pos2, count, ctx)] = ctx.params.accentColor;
  }
}

static uint16_t sparkleNodeSeed(const EffectContext& ctx) {
  return (uint16_t)(ctx.node.nodeId * 197U ^
                    ctx.node.x * 251U ^
                    ctx.node.y * 397U ^
                    ctx.node.z * 607U ^
                    ctx.node.span * 769U ^
                    ctx.node.axis * 911U ^
                    ctx.node.orientation * 1237U ^
                    ctx.node.membership * 1543U ^
                    ctx.node.stageX * 1871U ^
                    ctx.node.stageY * 2027U ^
                    ctx.node.stageZ * 2203U ^
                    ctx.spatial.offsetNorm ^
                    (uint16_t)ctx.spatial.phaseOffset);
}

static uint16_t sparkleHash16(uint32_t v) {
  v ^= v >> 16;
  v *= 0x7feb352dUL;
  v ^= v >> 15;
  v *= 0x846ca68bUL;
  v ^= v >> 16;
  return (uint16_t)v;
}

static void effectSparkle(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  if (count == 0) return;
  fadeToBlackBy(leds, count, fadeAmount(ctx));

  if (beatMode(ctx)) {
    uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
    uint16_t phase = 0;
    if (!effectBeatPhase(ctx, beatMs, &phase)) return;
  }

  uint8_t chance = map(ctx.params.fxExpand, 0, 255, 5, 80);
  uint16_t seed = sparkleNodeSeed(ctx);
  uint32_t timeBucket = (ctx.nowMs / 23U) + (beatMode(ctx) ? (ctx.beatAgeMs / 13U) : (ctx.phaseMs / 31U));
  uint16_t h = sparkleHash16((timeBucket << 16) ^ seed ^ (uint16_t)ctx.spatial.phaseOffset);
  if ((uint8_t)h < chance) {
    uint16_t pos = (uint16_t)((h >> 8) % count);
    uint16_t colorHash = sparkleHash16((uint32_t)h ^ seed ^ 0xA5A5U);
    leds[pixelIndex(pos, count, ctx)] = (colorHash & 1U) ? ctx.params.mainColor : ctx.params.accentColor;
  }
}

static uint8_t heatWave(uint32_t t, uint16_t i, const EffectContext& ctx) {
  uint8_t speed = map(ctx.params.fxSpeed, 0, 255, 18, 4);
  uint16_t seed = (uint16_t)(ctx.node.x * 29U + ctx.node.y * 43U + ctx.node.axis * 61U +
                             ctx.node.orientation * 71U + ctx.node.stageX * 11U +
                             ctx.node.stageY * 17U + ctx.node.stageZ * 19U);
  uint8_t waveA = sin8((t / speed) + (i * 23) + ctx.spatial.phaseOffset + seed);
  uint8_t waveB = inoise8(i * 35 + seed, (t / 9) + (seed >> 1));
  return qadd8(scale8(waveA, 130), scale8(waveB, 90));
}

static uint16_t nodeFireSeed(const EffectContext& ctx) {
  return (uint16_t)(ctx.node.x * 37U + ctx.node.y * 53U + ctx.node.z * 67U +
                    ctx.node.axis * 79U + ctx.node.orientation * 97U +
                    ctx.node.stageX * 13U + ctx.node.stageY * 17U +
                    ctx.node.stageZ * 23U + ctx.node.membership * 31U);
}

static void effectFireGround(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint8_t base = map(ctx.params.fxExpand, 0, 255, 70, 190);

  for (uint16_t i = 0; i < count; ++i) {
    uint8_t heat = qadd8(base, scale8(heatWave(ctx.phaseMs, i, ctx), 70));
    CRGB c = blend(ctx.params.mainColor, ctx.params.accentColor, scale8(heat, 96));
    c.nscale8_video(heat);
    leds[pixelIndex(i, count, ctx)] = c;
  }
}

static void effectFireVertical(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);
  uint16_t seed = nodeFireSeed(ctx);
  uint8_t speedDiv = map(ctx.params.fxSpeed, 0, 255, 8, 2);
  uint16_t t = (uint16_t)(ctx.phaseMs / speedDiv) + seed * 3U;
  uint8_t heightTaper = map(ctx.params.fxExpand, 0, 255, 215, 135);

  bool hanging = ctx.node.orientation == ORIENT_CEILING;
  for (uint16_t logical = 0; logical < count; ++logical) {
    uint16_t pixel = hanging ? ((count - 1) - logical) : logical;
    uint8_t height = count <= 1 ? 255 : ((uint32_t)logical * 255UL) / (count - 1);

    uint8_t riseA = inoise8(seed + logical * 29U, t * 3U - height * 9U);
    uint8_t riseB = inoise8(seed * 3U + logical * 67U, t * 5U - height * 17U);
    uint8_t lick = qadd8(scale8(riseA, 170), scale8(riseB, 110));
    uint8_t flicker = sin8((uint8_t)(t * 7U + logical * 31U + seed));
    uint8_t cutout = inoise8(seed * 5U + logical * 83U, t * 4U + height * 6U);
    uint8_t taper = qsub8(255, scale8(height, heightTaper));

    uint8_t heat = scale8(qadd8(lick, scale8(flicker, 75)), taper);
    heat = qsub8(heat, scale8(qsub8(cutout, 132), 150));
    if (height < 52) heat = qadd8(heat, qsub8(96, height));
    if (height > 190) heat = qsub8(heat, scale8(height - 190, 120));

    uint8_t colorMix = qadd8(scale8(height, 210), scale8(qsub8(210, heat), 70));
    CRGB c = blend(ctx.params.mainColor, ctx.params.accentColor, colorMix);
    c.nscale8_video(qadd8(heat, scale8(qsub8(70, height / 2), 120)));

    leds[pixel] = c;
  }
}

static void effectFireCeiling(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  uint16_t seed = nodeFireSeed(ctx);
  const uint32_t bucket = ctx.phaseMs / 17U;
  const uint32_t source = seed ^ ((uint32_t)ctx.node.nodeId << 16) ^
                          ((uint32_t)ctx.node.output << 24);
  fadeToBlackBy(leds, count, map(ctx.params.fxFade, 0, 255, 28, 8));

  uint8_t chance = map(ctx.params.fxExpand, 0, 255, 20, 110);
  uint8_t bursts = map(ctx.params.fxExpand, 0, 255, 2, 6);
  for (uint8_t b = 0; b < bursts; ++b) {
    const uint16_t h = sparkleHash16(source ^ bucket * 4051U ^ b * 977U);
    if ((uint8_t)h < chance) {
      const uint16_t detail = sparkleHash16(source ^ h ^ 0xA5A5U);
      uint16_t p = detail % count;
      CRGB ember = h & 0x100U ? ctx.params.accentColor : blend(ctx.params.mainColor, ctx.params.accentColor, 180);
      ember.nscale8_video(36U + ((detail >> 8) * 114U) / 256U);
      leds[pixelIndex(p, count, ctx)] += ember;
    }
  }

  if (count > 2 && (uint8_t)sparkleHash16(source ^ bucket ^ 0xD21FU) < 64) {
    uint16_t drift = (ctx.phaseMs / 43U + seed) % count;
    CRGB ember = ctx.params.accentColor;
    ember.nscale8_video(48);
    leds[pixelIndex(drift, count, ctx)] += ember;
  }
}

static void effectFire(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  switch (ctx.spatial.nodeRole) {
    case NODE_FIELD_VERTICAL:
      effectFireVertical(leds, count, ctx);
      break;
    case NODE_FIELD_CEILING:
      effectFireCeiling(leds, count, ctx);
      break;
    case NODE_FIELD_GROUND:
    default:
      effectFireGround(leds, count, ctx);
      break;
  }
}

static void fillSparkBackground(CRGB* leds, uint16_t count, const EffectContext& ctx, const CRGB& color, uint8_t variation) {
  for (uint16_t i = 0; i < count; i++) {
    CRGB c = color;
    uint8_t shimmer = inoise8((i * 29) + ctx.spatial.phaseOffset, ctx.phaseMs / 18);
    c.nscale8_video(qadd8(170, scale8(shimmer, variation)));
    leds[pixelIndex(i, count, ctx)] = c;
  }
}

static void effectSparkSurface(CRGB* leds, uint16_t count, const EffectContext& ctx, const CRGB& color) {
  uint8_t variation = map(ctx.params.fxWarp, 0, 255, 18, 76);
  uint8_t pulse = sin8(ctx.phaseMs / map(ctx.params.fxFade, 0, 255, 18, 5));

  for (uint16_t i = 0; i < count; i++) {
    CRGB c = color;
    uint8_t shimmer = inoise8((i * 37) + ctx.spatial.phaseOffset, ctx.phaseMs / 14);
    c.nscale8_video(qadd8(178, scale8(qadd8(shimmer, pulse / 4), variation)));
    leds[pixelIndex(i, count, ctx)] = c;
  }
}

static CRGB sparkBackgroundColor(const EffectContext& ctx) {
  CRGB bg = ctx.params.accentColor;
  bg.nscale8_video(170);
  return bg;
}

static void preserveSparkBackground(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  CRGB bg = sparkBackgroundColor(ctx);
  uint8_t settle = map(ctx.params.fxFade, 0, 255, 4, 96);
  for (uint16_t i = 0; i < count; i++) {
    // Cleared outputs start at the same background without shared static state.
    leds[i].r = max<uint8_t>(leds[i].r, bg.r);
    leds[i].g = max<uint8_t>(leds[i].g, bg.g);
    leds[i].b = max<uint8_t>(leds[i].b, bg.b);
    leds[i] = blend(leds[i], bg, settle);
  }
}

static void effectVerticalSparks(CRGB* leds, uint16_t count, const EffectContext& ctx, bool rising) {
  count = safeCount(count);

  preserveSparkBackground(leds, count, ctx);

  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  int32_t sparkPhaseMs = beatMode(ctx) ? (int32_t)ctx.beatAgeMs : (int32_t)ctx.phaseMs;
  if (ctx.spatial.enabled) {
    sparkPhaseMs -= ((uint32_t)beatMs * ctx.spatial.offsetNorm) / 65535UL;
  }

  if (sparkPhaseMs < 0) return;
  if (beatMode(ctx) && (uint32_t)sparkPhaseMs >= beatMs) return;

  uint16_t trailLen = map(ctx.params.fxExpand, 0, 255, min<uint16_t>(2, count), count);
  uint8_t softness = map(ctx.params.fxWarp, 0, 255, 18, 72);
  uint16_t t = beatMode(ctx) ? (uint16_t)sparkPhaseMs : (uint16_t)((uint32_t)sparkPhaseMs % beatMs);
  uint16_t travelPx = count + trailLen;
  int16_t travel = ((uint32_t)t * travelPx) / beatMs;
  bool hanging = ctx.node.orientation == ORIENT_CEILING;
  bool movePositive = rising ? !hanging : hanging;
  int16_t head = movePositive ? travel : ((int16_t)count - 1 - travel);

  for (uint16_t i = 0; i < count; i++) {
    int16_t dist = movePositive ? ((int16_t)head - (int16_t)i) : ((int16_t)i - (int16_t)head);
    if (dist < 0 || dist > trailLen) continue;

    uint8_t trail = 255 - (((uint32_t)dist * 255U) / max<uint16_t>(1, trailLen));
    trail = ease8InOutApprox(trail);
    uint8_t amount = qadd8(18, trail);
    amount = qadd8(amount, scale8(amount, softness / 5));

    CRGB spark = ctx.params.mainColor;
    spark.nscale8_video(amount);
    leds[i] += spark;
  }
}

static void effectRoleSparks(CRGB* leds, uint16_t count, const EffectContext& ctx, bool rising) {
  switch (ctx.spatial.nodeRole) {
    case NODE_FIELD_CEILING:
      effectSparkSurface(leds, count, ctx, rising ? ctx.params.accentColor : ctx.params.mainColor);
      break;
    case NODE_FIELD_VERTICAL:
      effectVerticalSparks(leds, count, ctx, rising);
      break;
    case NODE_FIELD_GROUND:
    default:
      if (rising) {
        effectSparkSurface(leds, count, ctx, ctx.params.mainColor);
      } else {
        fillSparkBackground(leds, count, ctx, ctx.params.accentColor, map(ctx.params.fxWarp, 0, 255, 8, 34));
      }
      break;
  }
}

static uint8_t angleToByte(int16_t a, int16_t b) {
  if (a == 0 && b == 0) return 0;

  uint16_t ax = a < 0 ? (uint16_t)-a : (uint16_t)a;
  uint16_t ay = b < 0 ? (uint16_t)-b : (uint16_t)b;
  uint8_t q = ax >= ay
    ? (uint8_t)((ay * 32U) / max<uint16_t>(1, ax))
    : (uint8_t)(64U - ((ax * 32U) / max<uint16_t>(1, ay)));

  if (a >= 0 && b >= 0) return q;
  if (a < 0 && b >= 0) return 128 - q;
  if (a < 0 && b < 0) return 128 + q;
  return 255 - q;
}

static CRGB splitWheelColor(uint8_t angle, const EffectContext& ctx, bool clockwise) {
  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  uint16_t beatPhase = 0;
  bool active = effectBeatPhase(ctx, beatMs, &beatPhase);
  uint8_t spin = active ? (((uint32_t)beatPhase * 255UL) / beatMs) : 255;
  uint8_t phase = clockwise ? angle + spin : angle - spin;
  uint8_t mainWidth = ctx.params.fxExpand;
  uint8_t softness = ctx.params.fxFade / 2;

  if (mainWidth == 0) return ctx.params.accentColor;
  if (mainWidth == 255) return ctx.params.mainColor;

  if (softness == 0) {
    return phase < mainWidth ? ctx.params.mainColor : ctx.params.accentColor;
  }

  uint8_t blendAmount = 0;
  if (phase < mainWidth) {
    uint8_t distToEdge = min<uint8_t>(phase, mainWidth - phase);
    blendAmount = distToEdge >= softness ? 0 : 255 - ((distToEdge * 255U) / softness);
  } else {
    uint8_t distToEdge = min<uint8_t>(phase - mainWidth, 255 - phase);
    blendAmount = distToEdge >= softness ? 255 : ((distToEdge * 255U) / softness);
  }

  return blend(ctx.params.mainColor, ctx.params.accentColor, blendAmount);
}

static void effectSplitWheel(CRGB* leds, uint16_t count, const EffectContext& ctx, uint8_t plane, bool clockwise) {
  count = safeCount(count);

  for (uint16_t i = 0; i < count; i++) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t angle = 0;
    switch (plane) {
      case 0: angle = angleToByte(sample.dx2, sample.dy2); break; // Yaw: X/Y around stage.
      case 1: angle = angleToByte(sample.dy2, sample.dz2); break; // Roll: Y/Z.
      case 2: angle = angleToByte(sample.dx2, sample.dz2); break; // Pitch: X/Z.
      default: break;
    }
    leds[i] = splitWheelColor(angle, ctx, clockwise);
  }
}

static uint16_t strobeDutyMs(uint16_t beatMs, uint8_t fade) {
  uint16_t minDuty = max<uint16_t>(8, beatMs / 64);
  uint16_t maxDuty = max<uint16_t>((uint16_t)(minDuty + 1), (uint16_t)(((uint32_t)beatMs * 240UL) / 255UL));
  return (uint16_t)map(fade, 0, 255, minDuty, maxDuty);
}

static uint16_t strobeAccentOffsetMs(uint16_t beatMs, uint8_t warp) {
  uint8_t folded = warp <= 127 ? warp : (uint8_t)(255 - warp);
  return (uint16_t)(((uint32_t)folded * beatMs) / 255UL);
}

static bool strobeOn(uint32_t ageMs, uint16_t beatMs, uint16_t dutyMs, bool beatMode, uint16_t offsetMs) {
  if (beatMode) {
    return ageMs >= offsetMs && ageMs < ((uint32_t)offsetMs + dutyMs);
  }

  uint16_t phase = (uint16_t)((ageMs + beatMs - offsetMs) % beatMs);
  return phase < dutyMs;
}

static void effectStrobe(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);

  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  bool beatMode = ctx.params.fxSpeed == 0;
  uint32_t ageMs = beatMode ? ctx.beatAgeMs : ctx.phaseMs;
  uint16_t dutyMs = strobeDutyMs(beatMs, ctx.params.fxFade);
  uint16_t accentOffsetMs = strobeAccentOffsetMs(beatMs, ctx.params.fxWarp);
  bool mainOn = strobeOn(ageMs, beatMs, dutyMs, beatMode, 0);
  bool accentOn = strobeOn(ageMs, beatMs, dutyMs, beatMode, accentOffsetMs);
  uint16_t reachNorm = (uint16_t)map(ctx.params.fxExpand, 0, 255, 8192, 65535);

  for (uint16_t i = 0; i < count; i++) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    bool inMainArea = sample.distanceNorm <= reachNorm;
    leds[i] = inMainArea
      ? (mainOn ? ctx.params.mainColor : CRGB::Black)
      : (accentOn ? ctx.params.accentColor : CRGB::Black);
  }
}

static void effectMeteorSweep(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);
  fadeToBlackBy(leds, count, map(ctx.params.fxFade, 0, 255, 210, 24));

  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  uint16_t phase = 0;
  if (!effectBeatPhase(ctx, beatMs, &phase)) return;

  uint16_t trailLen = map(ctx.params.fxExpand, 0, 255, 3, max<uint16_t>(4, count / 2));
  uint16_t travel = count + trailLen + 1U;
  int16_t head = ((uint32_t)phase * travel) / beatMs;
  int8_t wobble = (int8_t)map(ctx.params.fxWarp, 0, 255, -8, 8);
  head += (int16_t)((sin8((phase * 255UL) / beatMs) - 128) * wobble) / 32;

  for (uint16_t i = 0; i < count; i++) {
    int16_t dist = head - (int16_t)i;
    if (dist < 0 || dist > (int16_t)trailLen) continue;
    uint8_t amount = 255 - (((uint16_t)dist * 255U) / max<uint16_t>(1, trailLen));
    amount = ease8InOutApprox(amount);
    CRGB c = blend(ctx.params.accentColor, ctx.params.mainColor, amount);
    c.nscale8_video(qadd8(40, amount));
    leds[pixelIndex(i, count, ctx)] += c;
  }
}

static void effectBassBloom(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);
  fill_solid(leds, count, CRGB::Black);

  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  uint16_t phase = 0;
  if (!effectBeatPhase(ctx, beatMs, &phase)) return;

  uint8_t bloom = 255 - (((uint32_t)phase * 255UL) / beatMs);
  bloom = ease8InOutApprox(bloom);
  uint16_t radius = map(ctx.params.fxExpand, 0, 255, max<uint16_t>(2, count / 10), count);
  uint16_t center = count <= 1 ? 0 : (((uint32_t)ctx.params.fxWarp * (count - 1U)) / 255U);

  for (uint16_t i = 0; i < count; i++) {
    uint16_t dist = i > center ? i - center : center - i;
    uint8_t falloff = dist >= radius ? 0 : 255 - (((uint32_t)dist * 255UL) / max<uint16_t>(1, radius));
    uint8_t amount = scale8_video(bloom, ease8InOutApprox(falloff));
    CRGB c = blend(ctx.params.accentColor, ctx.params.mainColor, amount);
    c.nscale8_video(amount);
    leds[pixelIndex(i, count, ctx)] = c;
  }
}

static void effectLightningCrack(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);
  fadeToBlackBy(leds, count, map(ctx.params.fxFade, 0, 255, 255, 48));

  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  uint16_t phase = 0;
  if (!effectBeatPhase(ctx, beatMs, &phase)) return;
  uint16_t strikeMs = map(ctx.params.fxFade, 0, 255, max<uint16_t>(16, beatMs / 64), max<uint16_t>(32, beatMs / 5));
  if (phase > strikeMs) return;

  uint16_t seed = sparkleNodeSeed(ctx) ^ (uint16_t)(ctx.phaseMs / beatMs) ^ ((uint16_t)ctx.params.fxWarp << 3);
  uint8_t forks = map(ctx.params.fxExpand, 0, 255, 2, 9);
  uint8_t life = 255 - (((uint32_t)phase * 255UL) / max<uint16_t>(1, strikeMs));
  for (uint8_t f = 0; f < forks; f++) {
    uint16_t h = sparkleHash16(seed + f * 0x389U);
    uint16_t start = h % count;
    uint16_t width = 1 + ((h >> 8) % max<uint16_t>(1, count / 5));
    for (uint16_t w = 0; w <= width && w < count; w++) {
      uint16_t p = (start + w) % count;
      uint8_t jag = inoise8(seed + f * 31U, p * map(ctx.params.fxWarp, 0, 255, 18, 62));
      if (jag < map(ctx.params.fxWarp, 0, 255, 60, 145)) continue;
      CRGB c = (f & 1U) ? ctx.params.accentColor : ctx.params.mainColor;
      c.nscale8_video(scale8_video(life, qadd8(96, jag / 2)));
      leds[pixelIndex(p, count, ctx)] += c;
    }
  }
}

static void effectPrismSplit(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);
  uint8_t bandScale = map(ctx.params.fxWarp, 0, 255, 10, 48);
  uint8_t phase = (uint8_t)(ctx.phaseMs / map(ctx.params.fxSpeed, 0, 255, 18, 3)) + ctx.spatial.phaseOffset;
  uint8_t softness = map(ctx.params.fxFade, 0, 255, 8, 120);
  uint8_t mainWidth = ctx.params.fxExpand;

  for (uint16_t i = 0; i < count; i++) {
    uint8_t wave = sin8(phase + i * bandScale);
    uint8_t edge = wave > mainWidth ? wave - mainWidth : mainWidth - wave;
    uint8_t mix = wave < mainWidth ? 0 : 255;
    if (edge < softness) {
      uint8_t edgeMix = ((uint16_t)edge * 255U) / max<uint8_t>(1, softness);
      mix = wave < mainWidth ? edgeMix : 255 - edgeMix;
    }
    leds[pixelIndex(i, count, ctx)] = blend(ctx.params.mainColor, ctx.params.accentColor, mix);
  }
}

static void effectEmberDrift(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);
  fadeToBlackBy(leds, count, map(ctx.params.fxFade, 0, 255, 28, 5));

  uint8_t embers = map(ctx.params.fxExpand, 0, 255, 1, 7);
  uint16_t seed = sparkleNodeSeed(ctx);
  uint16_t drift = ctx.phaseMs / map(ctx.params.fxWarp, 0, 255, 60, 12);
  for (uint8_t e = 0; e < embers; e++) {
    uint16_t h = sparkleHash16(seed + e * 0x537U + drift);
    uint16_t pos = (h + drift * (e + 1U)) % count;
    uint8_t heat = qadd8(72, h >> 8);
    CRGB c = blend(ctx.params.accentColor, ctx.params.mainColor, scale8(heat, 120));
    c.nscale8_video(heat);
    leds[pixelIndex(pos, count, ctx)] += c;
  }
}

static void effectImpactFlash(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);
  fadeToBlackBy(leds, count, map(ctx.params.fxFade, 0, 255, 180, 18));

  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  uint16_t phase = 0;
  if (!effectBeatPhase(ctx, beatMs, &phase)) return;

  uint8_t level = 255 - (((uint32_t)phase * 255UL) / beatMs);
  level = ease8InOutApprox(level);
  uint16_t reach = map(ctx.params.fxExpand, 0, 255, max<uint16_t>(1, count / 6), count);
  uint16_t center = count / 2;

  for (uint16_t i = 0; i < count; i++) {
    uint16_t dist = i > center ? i - center : center - i;
    if (dist > reach) continue;
    uint8_t amount = scale8_video(level, 255 - (((uint32_t)dist * 255UL) / max<uint16_t>(1, reach)));
    uint8_t shimmer = inoise8(i * map(ctx.params.fxWarp, 0, 255, 18, 66), ctx.phaseMs / 8);
    CRGB c = blend(ctx.params.accentColor, ctx.params.mainColor, qadd8(amount, shimmer / 5));
    c.nscale8_video(amount);
    leds[pixelIndex(i, count, ctx)] += c;
  }
}

static void effectScannerBeam(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);
  fadeToBlackBy(leds, count, map(ctx.params.fxFade, 0, 255, 240, 34));

  uint16_t beatMs = computeBeatMs(ctx.params.bpm, ctx.params.fxSpeed);
  uint16_t phase = 0;
  if (!effectBeatPhase(ctx, beatMs, &phase)) return;

  uint8_t phase8 = ((uint32_t)phase * 255UL) / beatMs;
  uint16_t pos = ctx.params.fxWarp > 127
    ? (((uint32_t)triwave8(phase8) * (count - 1U)) / 255U)
    : (((uint32_t)phase8 * (count - 1U)) / 255U);
  uint16_t width = map(ctx.params.fxExpand, 0, 255, 1, max<uint16_t>(2, count / 3));

  for (uint16_t i = 0; i < count; i++) {
    uint16_t dist = i > pos ? i - pos : pos - i;
    if (dist > width) continue;
    uint8_t amount = 255 - (((uint32_t)dist * 255UL) / max<uint16_t>(1, width));
    CRGB c = blend(ctx.params.accentColor, ctx.params.mainColor, amount);
    c.nscale8_video(ease8InOutApprox(amount));
    leds[pixelIndex(i, count, ctx)] += c;
  }
}

static void effectAuroraWave(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  count = safeCount(count);
  uint8_t bandCount = map(ctx.params.fxWarp, 0, 255, 1, 7);
  uint8_t bandSize = map(ctx.params.fxExpand, 0, 255, 24, 128);
  uint8_t persistence = map(ctx.params.fxFade, 0, 255, 52, 230);
  uint8_t drift = (uint8_t)(ctx.nowMs / 96U);

  for (uint16_t i = 0; i < count; i++) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(ctx.node, i, count);
    uint8_t field = (uint8_t)(sample.nodeX2 * 11U + sample.nodeY2 * 17U +
                              sample.nodeZ2 * 23U);
    uint8_t a = sin8((uint8_t)(field * bandCount + drift));
    uint8_t b = inoise8(field * 7U, drift / 2U + sample.nodeZ2 * 19U);
    uint8_t mix = qadd8(scale8(a, 176), scale8(b, 79));
    uint8_t distance = mix > 128 ? mix - 128U : 128U - mix;
    uint8_t bandFalloff = max<uint8_t>(1, 128U - bandSize);
    uint8_t band = distance <= bandSize
      ? 255
      : qsub8(255, (uint8_t)(((uint16_t)(distance - bandSize) * 255U) /
                             bandFalloff));
    CRGB c = blend(ctx.params.mainColor, ctx.params.accentColor, mix);
    c.nscale8_video(qadd8(persistence, scale8(band, 255U - persistence)));
    leds[i] = c;
  }
}

static void renderStripEffect(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  if (count == 0) return;

  switch (ctx.params.lightEffect) {
    case 0: effectSolid(leds, count, ctx); break;
    case 1: effectPulse(leds, count, ctx); break;
    case 2: effectWipe(leds, count, ctx); break;
    case 3: effectGradient(leds, count, ctx); break;
    case 4: effectChase(leds, count, ctx); break;
    case 5: effectSparkle(leds, count, ctx); break;
    case 6: effectFire(leds, count, ctx); break;
    case 9: effectRoleSparks(leds, count, ctx, false); break;
    case 10: effectRoleSparks(leds, count, ctx, true); break;
    case 11: effectSplitWheel(leds, count, ctx, 0, true); break;
    case 12: effectSplitWheel(leds, count, ctx, 0, false); break;
    case 13: effectSplitWheel(leds, count, ctx, 1, true); break;
    case 14: effectSplitWheel(leds, count, ctx, 1, false); break;
    case 15: effectSplitWheel(leds, count, ctx, 2, true); break;
    case 16: effectSplitWheel(leds, count, ctx, 2, false); break;
    case 17: effectStrobe(leds, count, ctx); break;
    case 18: effectMeteorSweep(leds, count, ctx); break;
    case 19: effectBassBloom(leds, count, ctx); break;
    case 20: effectLightningCrack(leds, count, ctx); break;
    case 21: effectPrismSplit(leds, count, ctx); break;
    case 22: effectEmberDrift(leds, count, ctx); break;
    case 23: effectImpactFlash(leds, count, ctx); break;
    case 24: effectScannerBeam(leds, count, ctx); break;
    case 25: effectAuroraWave(leds, count, ctx); break;
    default:
      if (!renderNewEffect(leds, count, ctx)) {
        fill_solid(leds, count, CRGB::Black);
      }
      break;
  }
}

void renderEffect(CRGB* leds, uint16_t count, const EffectContext& ctx) {
  const uint8_t effect = ctx.params.lightEffect;
  const VirtualNodeList& segments = nodeVirtualNodesGet(ctx.node.output == 1 ? STRIP_B : STRIP_A);
  if ((effect != 6 && effect != 9 && effect != 10) || segments.count == 0) {
    renderStripEffect(leds, count, ctx);
    return;
  }

  // Fire and Sparks choose a floor, upright or ceiling animation per segment.
  uint16_t start = 0;
  for (uint8_t index = 0; index < segments.count; ++index) {
    const VirtualNodeDefinition& segment = segments.nodes[index];
    if (segment.startLed >= count) break;
    if (segment.startLed > start) {
      renderStripEffect(leds + start, segment.startLed - start, ctx);
    }
    const uint16_t length = min<uint16_t>(segment.ledCount, count - segment.startLed);
    EffectContext local = ctx;
    local.node = resolveNodePlacementForPixel(ctx.node, segment.startLed, count);
    local.spatial = computeSpatialResult(ctx.params, local.node);
    renderStripEffect(leds + segment.startLed, length, local);
    start = segment.startLed + length;
  }
  if (start < count) renderStripEffect(leds + start, count - start, ctx);
}
