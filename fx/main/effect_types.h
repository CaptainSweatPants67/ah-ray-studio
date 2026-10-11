#pragma once
#include <Arduino.h>
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif
#ifndef FASTLED_INTERRUPT_RETRY_COUNT
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#endif
#include <FastLED.h>
#include "packets.h"

struct RenderParams {
  uint8_t brightness;
  uint8_t lightEffect;
  uint8_t spatialEffect;

  uint8_t fxSpeed;
  uint8_t fxWarp;
  uint8_t fxExpand;
  uint8_t fxFade;

  uint8_t bpm;

  uint8_t mainColorPreset;
  uint8_t accentColorPreset;
  uint8_t accentDimmer;
  uint8_t zone;

  CRGB mainColor;
  CRGB accentColor;

  uint8_t settingCommand;
  uint8_t flags;
};

struct NodePlacement {
  uint8_t nodeId;
  uint8_t output;
  uint8_t x;
  uint8_t y;
  uint8_t z;
  uint8_t span;
  uint8_t axis;
  uint8_t orientation;
  uint8_t membership;

  uint8_t stageX;
  uint8_t stageY;
  uint8_t stageZ;
};

enum NodeFieldRole : uint8_t {
  NODE_FIELD_GROUND = 0,
  NODE_FIELD_VERTICAL = 1,
  NODE_FIELD_CEILING = 2
};

struct SpatialResult {
  int32_t nodeDelayMs = 0;
  int32_t pixelDelayMs = 0;
  int16_t phaseOffset = 0;
  uint16_t offsetNorm = 0;
  uint16_t cycleMs = 0;
  uint16_t attackMs = 0;
  uint16_t holdMs = 0;
  uint16_t releaseMs = 0;
  uint8_t envelope = 255;
  uint8_t intensity = 255;
  NodeFieldRole nodeRole = NODE_FIELD_GROUND;
  bool reversePixels = false;
  bool enabled = true;
  bool beatMode = false;
};

struct EffectContext {
  uint32_t nowMs;
  uint32_t phaseMs;
  uint32_t beatAgeMs;

  RenderParams params;
  SpatialResult spatial;
  NodePlacement node;
};
