#pragma once

#include "effect_types.h"

bool renderNewEffect(CRGB* leds, uint16_t count, const EffectContext& ctx);
bool effectUsesIntrinsicSpatialGeometry(uint8_t lightEffect);
