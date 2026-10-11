#pragma once
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif
#ifndef FASTLED_INTERRUPT_RETRY_COUNT
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#endif
#include <FastLED.h>
#include "effect_types.h"

void renderEffect(CRGB* leds, uint16_t count, const EffectContext& ctx);
CRGB presetToRgb(uint8_t v);
