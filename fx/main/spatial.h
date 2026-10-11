#pragma once
#include "effect_types.h"

struct SpatialFieldSample {
  int16_t nodeX2 = 0;
  int16_t nodeY2 = 0;
  int16_t nodeZ2 = 0;
  int16_t stageX2 = 0;
  int16_t stageY2 = 0;
  int16_t stageZ2 = 0;
  int16_t dx2 = 0;
  int16_t dy2 = 0;
  int16_t dz2 = 0;
  uint16_t distance2 = 0;
  uint16_t distanceNorm = 0;
  uint16_t horizontalNorm = 0;
  uint16_t verticalNorm = 0;
};

SpatialResult computeSpatialResult(const RenderParams& params, const NodePlacement& node);
SpatialFieldSample computeSpatialFieldSample(const NodePlacement& node);
NodePlacement resolveNodePlacementForPixel(
  const NodePlacement& node,
  uint16_t pixel,
  uint16_t count,
  uint16_t* localPixel = nullptr,
  uint16_t* localCount = nullptr
);
SpatialFieldSample computeSpatialFieldSampleForPixel(const NodePlacement& node, uint16_t pixel, uint16_t count);
int32_t computeSpatialPixelDelayMs(const RenderParams& params, const NodePlacement& node, uint16_t pixel, uint16_t count);
uint8_t computeSpatialPixelEnvelope(const RenderParams& params, const NodePlacement& node, const SpatialResult& spatial, uint16_t pixel, uint16_t count, int32_t runAgeMs);
uint8_t computeSpatialEnvelope(const SpatialResult& spatial, int32_t localAgeMs);
uint16_t computeBeatMs(uint8_t bpm, uint8_t fxSpeed);
