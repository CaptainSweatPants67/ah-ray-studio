#include "spatial.h"
#include "fx_package.h"
#include "node_config.h"

// Spatial field model
// -------------------
// X/Y are the 1..9 room grid. Internally they are doubled to 2..18 so a
// spanning node can resolve to a center between grid cells without floats.
//
// Node Z is stored as a 0..8 height slice and is projected onto the same
// doubled 1..9 field as X/Y:
//   2  = ground height 1
//   18 = ceiling height 9
//
// Stage orientation intentionally does not participate in timing. Effects use
// distance or axis offsets from the stage point, so moving the stage changes the
// field and rotating the stage does not.

static constexpr uint16_t MAX_XY_DISTANCE2 = 16;
static constexpr uint16_t MAX_AXIS_DISTANCE2 = 8;
static constexpr uint16_t MAX_Z_DISTANCE2 = 16;
static constexpr uint16_t MAX_DISTANCE2 = 28; // ceil(sqrt(16^2 + 16^2 + 16^2)).

static uint8_t clampGrid(uint8_t v) {
  if (v < 1) return 1;
  if (v > 9) return 9;
  return v;
}

static uint8_t clampSpan(uint8_t v) {
  if (v < 1) return 1;
  if (v > 9) return 9;
  return v;
}

static uint8_t clampHeightIndex(uint8_t v) {
  if (v > 8) return 8;
  return v;
}

static int16_t clampCoord2(int16_t v) {
  if (v < 2) return 2;
  if (v > 18) return 18;
  return v;
}

static uint16_t abs16(int16_t v) {
  return v < 0 ? (uint16_t)-v : (uint16_t)v;
}

static uint16_t isqrt32(uint32_t v) {
  uint32_t bit = 1UL << 30;
  while (bit > v) bit >>= 2;

  uint32_t out = 0;
  while (bit != 0) {
    if (v >= out + bit) {
      v -= out + bit;
      out = (out >> 1) + bit;
    } else {
      out >>= 1;
    }
    bit >>= 2;
  }

  return (uint16_t)out;
}

static uint16_t normalizedFromValue(uint16_t value, uint16_t maxValue) {
  if (maxValue == 0) return 0;
  if (value > maxValue) value = maxValue;
  return ((uint32_t)value * 65535UL) / maxValue;
}

static uint16_t normalizedFromSigned(int16_t value, int16_t minValue, int16_t maxValue) {
  if (maxValue <= minValue) return 0;
  if (value < minValue) value = minValue;
  if (value > maxValue) value = maxValue;
  return ((uint32_t)(value - minValue) * 65535UL) / (uint16_t)(maxValue - minValue);
}

static uint8_t snakeIndex(uint8_t x, uint8_t y) {
  x = clampGrid(x);
  y = clampGrid(y);

  uint8_t row = y - 1;
  uint8_t col = x - 1;

  if (row & 1) col = 8 - col;
  return row * 9 + col;
}

static uint16_t snakeIndex3dNorm(const SpatialFieldSample& s) {
  uint8_t x = clampGrid(s.nodeX2 / 2);
  uint8_t y = clampGrid(s.nodeY2 / 2);
  uint8_t z = clampGrid(s.nodeZ2 / 2);
  uint16_t layer = z - 1;
  uint16_t xy = snakeIndex(x, y);
  if (layer & 1U) xy = 80U - xy;
  return normalizedFromValue(layer * 81U + xy, 728);
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

static uint16_t normalizedFromByte(uint8_t value) {
  return ((uint16_t)value << 8) | value;
}

static uint8_t zoneChaseIndex(const SpatialFieldSample& s, uint8_t membership) {
  uint8_t x = clampGrid(s.nodeX2 / 2);
  uint8_t y = clampGrid(s.nodeY2 / 2);
  uint8_t z = clampGrid(s.nodeZ2 / 2);
  if (x == clampGrid(s.stageX2 / 2) && y == clampGrid(s.stageY2 / 2)) return 0;
  if (y == clampGrid(s.stageY2 / 2)) return 1;
  if (x == clampGrid(s.stageX2 / 2)) return 2;
  if (x <= 2) return 3;
  if (x >= 8) return 4;
  if (y <= 2) return 5;
  if (y >= 8) return 6;
  if (z <= 2) return 7;
  if (z >= 8) return 8;
  return (uint8_t)(9 + ((z - 3) / 2) + (membership & 1U));
}

static uint8_t stageHeightToZ2(uint8_t stageHeight) {
  if (stageHeight == 0) stageHeight = STAGE_LEVEL_GROUND;
  return clampGrid(stageHeight) * 2;
}

static uint8_t nodeZToZ2(uint8_t z) {
  return (uint8_t)((clampHeightIndex(z) + 1) * 2);
}

static uint8_t z2ToNodeZ(int16_t z2) {
  z2 = clampCoord2(z2);
  return (uint8_t)((z2 / 2) - 1);
}

static int16_t nodeCenterX2(const NodePlacement& n) {
  int16_t x2 = (int16_t)clampGrid(n.x) * 2;
  int16_t halfSpan2 = clampSpan(n.span) - 1;

  if (n.axis != AXIS_X) return x2;
  return clampCoord2(x2 + halfSpan2);
}

static int16_t nodeCenterY2(const NodePlacement& n) {
  int16_t y2 = (int16_t)clampGrid(n.y) * 2;
  int16_t halfSpan2 = clampSpan(n.span) - 1;

  if (n.axis != AXIS_Y) return y2;
  return clampCoord2(y2 + halfSpan2);
}

static int16_t nodeCenterZ2(const NodePlacement& n) {
  int16_t z2 = nodeZToZ2(n.z);
  int16_t halfSpan2 = clampSpan(n.span) - 1;

  if (n.axis != AXIS_Z) return z2;
  return clampCoord2(z2 + halfSpan2);
}

SpatialFieldSample computeSpatialFieldSample(const NodePlacement& node) {
  SpatialFieldSample s;
  s.nodeX2 = nodeCenterX2(node);
  s.nodeY2 = nodeCenterY2(node);
  s.nodeZ2 = nodeCenterZ2(node);

  s.stageX2 = (int16_t)clampGrid(node.stageX) * 2;
  s.stageY2 = (int16_t)clampGrid(node.stageY) * 2;
  s.stageZ2 = stageHeightToZ2(node.stageZ);

  s.dx2 = s.nodeX2 - s.stageX2;
  s.dy2 = s.nodeY2 - s.stageY2;
  s.dz2 = s.nodeZ2 - s.stageZ2;

  uint32_t distanceSquared =
    (uint32_t)s.dx2 * s.dx2 +
    (uint32_t)s.dy2 * s.dy2 +
    (uint32_t)s.dz2 * s.dz2;
  s.distance2 = isqrt32(distanceSquared);
  s.distanceNorm = normalizedFromValue(s.distance2, MAX_DISTANCE2);

  uint32_t horizontalSquared = (uint32_t)s.dx2 * s.dx2 + (uint32_t)s.dy2 * s.dy2;
  s.horizontalNorm = normalizedFromValue(isqrt32(horizontalSquared), MAX_XY_DISTANCE2);
  s.verticalNorm = normalizedFromValue(abs16(s.dz2), MAX_Z_DISTANCE2);

  return s;
}

static int16_t spanPixelOffset2(uint8_t span, uint16_t pixel, uint16_t count) {
  span = clampSpan(span);
  if (span <= 1 || count <= 1) return 0;
  if (pixel >= count) pixel = count - 1;

  return ((uint32_t)pixel * (uint16_t)(span - 1) * 2UL) / (count - 1);
}

static int16_t spanExtent2(uint8_t span) {
  return (int16_t)(clampSpan(span) - 1) * 2;
}

NodePlacement resolveNodePlacementForPixel(
  const NodePlacement& node,
  uint16_t pixel,
  uint16_t count,
  uint16_t* localPixel,
  uint16_t* localCount
) {
  NodePlacement out = node;
  uint16_t resolvedPixel = pixel;
  uint16_t resolvedCount = count;
  VirtualNodeDefinition virtualNode = {};
  if (nodeVirtualNodeForPixel(
        node.output == 1 ? STRIP_B : STRIP_A,
        pixel,
        virtualNode,
        resolvedPixel)) {
    out.x = virtualNode.x;
    out.y = virtualNode.y;
    out.z = virtualNode.z;
    out.span = virtualNode.span;
    out.axis = virtualNode.axis;
    out.orientation = virtualNode.orientation;
    out.membership = virtualNode.membership;
    resolvedCount = virtualNode.ledCount;
  }
  if (localPixel) *localPixel = resolvedPixel;
  if (localCount) *localCount = resolvedCount;
  return out;
}

SpatialFieldSample computeSpatialFieldSampleForPixel(const NodePlacement& node, uint16_t pixel, uint16_t count) {
  uint16_t localPixel = pixel;
  uint16_t localCount = count;
  NodePlacement n = resolveNodePlacementForPixel(node, pixel, count, &localPixel, &localCount);
  pixel = localPixel;
  count = localCount;
  const uint8_t axis = n.axis;
  const uint8_t orientation = n.orientation;
  uint8_t effectiveSpan = n.span;
  n.span = 1;
  int16_t offset2 = spanPixelOffset2(effectiveSpan, pixel, count);
  int16_t x2 = (int16_t)clampGrid(n.x) * 2;
  int16_t y2 = (int16_t)clampGrid(n.y) * 2;
  int16_t z2 = nodeZToZ2(n.z);

  int16_t extent2 = spanExtent2(effectiveSpan);
  if (axis == AXIS_X) {
    x2 += orientation == ORIENT_LEFT ? extent2 - offset2 : offset2;
    n.x = (uint8_t)((clampCoord2(x2) + 1) / 2);
  } else if (axis == AXIS_Y) {
    y2 += orientation == ORIENT_UP ? extent2 - offset2 : offset2;
    n.y = (uint8_t)((clampCoord2(y2) + 1) / 2);
  } else if (axis == AXIS_Z) {
    z2 += orientation == ORIENT_CEILING ? extent2 - offset2 : offset2;
    n.z = z2ToNodeZ(z2);
  }

  SpatialFieldSample s = computeSpatialFieldSample(n);
  if (axis == AXIS_X) s.nodeX2 = clampCoord2(x2);
  if (axis == AXIS_Y) s.nodeY2 = clampCoord2(y2);
  if (axis == AXIS_Z) {
    s.nodeZ2 = clampCoord2(z2);
  }

  s.dx2 = s.nodeX2 - s.stageX2;
  s.dy2 = s.nodeY2 - s.stageY2;
  s.dz2 = s.nodeZ2 - s.stageZ2;

  uint32_t distanceSquared =
    (uint32_t)s.dx2 * s.dx2 +
    (uint32_t)s.dy2 * s.dy2 +
    (uint32_t)s.dz2 * s.dz2;
  s.distance2 = isqrt32(distanceSquared);
  s.distanceNorm = normalizedFromValue(s.distance2, MAX_DISTANCE2);

  uint32_t horizontalSquared = (uint32_t)s.dx2 * s.dx2 + (uint32_t)s.dy2 * s.dy2;
  s.horizontalNorm = normalizedFromValue(isqrt32(horizontalSquared), MAX_XY_DISTANCE2);
  s.verticalNorm = normalizedFromValue(abs16(s.dz2), MAX_Z_DISTANCE2);

  return s;
}

uint16_t computeBeatMs(uint8_t bpm, uint8_t fxSpeed) {
  uint32_t baseMs = bpm > 0 ? (60000UL / bpm) : 1000UL;
  if (fxSpeed == 0) {
    if (baseMs < 20UL) baseMs = 20UL;
    if (baseMs > 60000UL) baseMs = 60000UL;
    return (uint16_t)baseMs;
  }
  switch (ah_fx_beat_division_index(fxSpeed)) {
    case 0: baseMs *= 8UL; break;
    case 1: baseMs *= 6UL; break;
    case 2: baseMs *= 4UL; break;
    case 3: baseMs *= 3UL; break;
    case 4: baseMs *= 2UL; break;
    case 5: break;
    case 6: baseMs /= 2UL; break;
    case 7: baseMs /= 3UL; break;
    case 8: baseMs /= 4UL; break;
    case 9: baseMs /= 6UL; break;
    case 10: baseMs /= 8UL; break;
    case 11: baseMs /= 16UL; break;
    case 12: baseMs /= 32UL; break;
    default: break;
  }
  if (baseMs < 20UL) baseMs = 20UL;
  if (baseMs > 60000UL) baseMs = 60000UL;
  return (uint16_t)baseMs;
}

static bool reversePixelsForNode(const NodePlacement& n) {
  if (n.axis == AXIS_X && n.orientation == ORIENT_LEFT) return true;
  if (n.axis == AXIS_Y && n.orientation == ORIENT_UP) return true;
  if (n.axis == AXIS_Z && n.orientation == ORIENT_CEILING) return true;
  return false;
}

static NodeFieldRole nodeFieldRole(const NodePlacement& n) {
  if (n.axis == AXIS_Z) return NODE_FIELD_VERTICAL;
  if (clampHeightIndex(n.z) >= 7) return NODE_FIELD_CEILING;
  return NODE_FIELD_GROUND;
}

uint8_t computeSpatialEnvelope(const SpatialResult& spatial, int32_t localAgeMs) {
  if (!spatial.enabled) return 255;
  if (localAgeMs < 0) return 0;

  uint16_t cycleMs = spatial.cycleMs;
  if (cycleMs == 0) return 255;

  if (spatial.beatMode && (uint32_t)localAgeMs >= cycleMs) return 0;
  uint16_t t = spatial.beatMode
    ? (uint16_t)localAgeMs
    : (uint16_t)((uint32_t)localAgeMs % cycleMs);

  if (spatial.attackMs > 0 && t < spatial.attackMs) {
    return ((uint32_t)t * 255UL) / spatial.attackMs;
  }
  t -= spatial.attackMs;

  if (t < spatial.holdMs) return 255;
  t -= spatial.holdMs;

  if (spatial.releaseMs > 0 && t < spatial.releaseMs) {
    return 255 - (((uint32_t)t * 255UL) / spatial.releaseMs);
  }

  return 0;
}

static uint16_t spatialEffectOffsetNorm(uint8_t effect, const SpatialFieldSample& s, uint8_t membership) {
  switch (effect) {
    case 0:  return 0;                                                  // none
    case 1:  return s.distanceNorm;                                     // stage outward
    case 2:  return 65535 - s.distanceNorm;                             // stage inward
    case 3:  return normalizedFromValue(abs16(s.dx2), MAX_AXIS_DISTANCE2); // X distance from stage
    case 4:  return normalizedFromValue(abs16(s.dy2), MAX_AXIS_DISTANCE2); // Y distance from stage
    case 5:  return s.horizontalNorm;                                   // horizontal rings
    case 6:  return 65535 - s.horizontalNorm;                           // inverse horizontal rings
    case 7:  return normalizedFromSigned(s.dx2 + s.dy2 + s.dz2, -48, 48); // 3D diagonal one way
    case 8:  return normalizedFromSigned(s.dx2 - s.dy2 + s.dz2, -48, 48); // 3D diagonal other way
    case 9:  return s.verticalNorm;                                     // ground/raised/ceiling up
    case 10: return 65535 - s.verticalNorm;                             // ceiling/raised/ground down
    case 11: {                                                          // stable random
      uint8_t h = ((s.nodeX2 / 2) * 31) ^ ((s.nodeY2 / 2) * 17) ^
                  (s.nodeZ2 * 53) ^ ((s.stageX2 / 2) * 7) ^
                  ((s.stageY2 / 2) * 13) ^ (membership * 11);
      return normalizedFromValue(h % 16, 15);
    }
    case 12: return snakeIndex3dNorm(s);                                // 3D room snake
    case 13: return s.distanceNorm;                                     // stage away
    case 14: return 65535 - s.distanceNorm;                             // stage toward
    case 15: return s.distanceNorm;                                     // stage ripple
    case 16: return 65535 - s.distanceNorm;                             // room collapse
    case 17: return normalizedFromSigned(s.nodeX2, 2, 18);              // left/right sweep
    case 18: return normalizedFromSigned(s.nodeY2, 2, 18);              // front/back sweep
    case 19: return 65535 - normalizedFromSigned(s.nodeZ2, 2, 18);      // ceiling drop
    case 20: return normalizedFromSigned(s.nodeZ2, 2, 18);              // floor rise
    case 21: {                                                          // random islands
      uint8_t h = ((s.nodeX2 / 2) * 43) ^ ((s.nodeY2 / 2) * 71) ^
                  (s.nodeZ2 * 29) ^ (membership * 97);
      return normalizedFromValue(h % 10, 9);
    }
    case 22: {                                                          // spiral room with height phase
      uint8_t angle = angleToByte(s.dx2, s.dy2);
      uint8_t heightPhase = (uint8_t)(normalizedFromSigned(s.nodeZ2, 2, 18) >> 10);
      return normalizedFromByte(angle + heightPhase);
    }
    case 23: return normalizedFromValue((s.distanceNorm >> 11) & 0x07, 7); // distance bands
    case 24: return normalizedFromValue(zoneChaseIndex(s, membership), 12); // zone chase
    default: return 0;
  }
}

SpatialResult computeSpatialResult(const RenderParams& params, const NodePlacement& node) {
  SpatialResult out;
  SpatialFieldSample field = computeSpatialFieldSample(node);

  uint16_t beatMs = computeBeatMs(params.bpm, params.fxSpeed);
  uint32_t spreadMs = ((uint32_t)beatMs * params.fxExpand) / 255UL;
  uint16_t norm = spatialEffectOffsetNorm(params.spatialEffect, field, node.membership);

  out.offsetNorm = norm;
  out.nodeDelayMs = ((uint32_t)spreadMs * norm) / 65535UL;

  out.cycleMs = beatMs;
  out.attackMs = map(params.fxFade, 0, 255, 15, beatMs / 4);
  out.holdMs = map(params.fxExpand, 0, 255, beatMs / 12, beatMs / 2);
  out.releaseMs = map(params.fxFade, 0, 255, beatMs / 2, beatMs / 8);

  uint32_t envelopeTotal = (uint32_t)out.attackMs + out.holdMs + out.releaseMs;
  if (envelopeTotal >= beatMs) {
    out.holdMs = beatMs > (out.attackMs + out.releaseMs) ? beatMs - out.attackMs - out.releaseMs : 0;
  }

  out.pixelDelayMs = ((uint32_t)map(params.fxWarp, 0, 255, 0, 25) * norm) / 65535UL;
  out.phaseOffset = map((uint8_t)(norm >> 8), 0, 255, -128, 127);
  out.envelope = 255;
  out.intensity = 255;
  out.nodeRole = nodeFieldRole(node);
  out.reversePixels = reversePixelsForNode(node);
  out.enabled = params.spatialEffect != 0;
  out.beatMode = params.fxSpeed == 0;

  return out;
}

int32_t computeSpatialPixelDelayMs(const RenderParams& params, const NodePlacement& node, uint16_t pixel, uint16_t count) {
  if (params.spatialEffect == 0) return 0;

  SpatialFieldSample field = computeSpatialFieldSampleForPixel(node, pixel, count);
  NodePlacement pixelNode = resolveNodePlacementForPixel(node, pixel, count);
  uint16_t beatMs = computeBeatMs(params.bpm, params.fxSpeed);
  uint32_t spreadMs = ((uint32_t)beatMs * params.fxExpand) / 255UL;
  uint16_t norm = spatialEffectOffsetNorm(params.spatialEffect, field, pixelNode.membership);

  return ((uint32_t)spreadMs * norm) / 65535UL;
}

uint8_t computeSpatialPixelEnvelope(const RenderParams& params, const NodePlacement& node, const SpatialResult& spatial, uint16_t pixel, uint16_t count, int32_t runAgeMs) {
  if (!spatial.enabled) return 255;
  if (runAgeMs < 0) return 0;
  if (spatial.cycleMs == 0) return 255;

  if (spatial.beatMode && (uint32_t)runAgeMs >= spatial.cycleMs) return 0;
  uint16_t t = spatial.beatMode
    ? (uint16_t)runAgeMs
    : (uint16_t)((uint32_t)runAgeMs % spatial.cycleMs);
  uint16_t requestedSpread = ((uint32_t)spatial.cycleMs * params.fxExpand) / 255UL;
  uint16_t availableSpread = 0;
  if (spatial.cycleMs > (spatial.attackMs + spatial.releaseMs)) {
    availableSpread = (spatial.cycleMs - spatial.attackMs - spatial.releaseMs) / 2;
  }

  uint16_t spreadMs = requestedSpread < availableSpread ? requestedSpread : availableSpread;
  uint16_t entryDelay = 0;
  if (requestedSpread > 0 && spreadMs > 0) {
    int32_t requestedDelay = computeSpatialPixelDelayMs(params, node, pixel, count);
    if (requestedDelay < 0) requestedDelay = 0;
    if (requestedDelay > requestedSpread) requestedDelay = requestedSpread;
    entryDelay = ((uint32_t)requestedDelay * spreadMs) / requestedSpread;
  }

  uint16_t exitDelay = spreadMs - entryDelay;
  uint16_t attackEnd = entryDelay + spatial.attackMs;
  uint16_t releaseStart = spatial.cycleMs - spatial.releaseMs - spreadMs + exitDelay;

  if (t < entryDelay) return 0;
  if (t < attackEnd) {
    if (spatial.attackMs == 0) return 255;
    return ((uint32_t)(t - entryDelay) * 255UL) / spatial.attackMs;
  }
  if (t < releaseStart) return 255;
  if (t < (releaseStart + spatial.releaseMs)) {
    if (spatial.releaseMs == 0) return 0;
    return 255 - (((uint32_t)(t - releaseStart) * 255UL) / spatial.releaseMs);
  }

  return 0;
}
