#pragma once

#include <Arduino.h>
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif
#ifndef FASTLED_INTERRUPT_RETRY_COUNT
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#endif
#include <FastLED.h>

static constexpr uint8_t LED_B_CLOCK = 2;  // XIAO D1
static constexpr uint8_t LED_B_DATA  = 3;  // XIAO D2
static constexpr uint8_t LED_A_CLOCK = 6;  // XIAO D5
static constexpr uint8_t LED_A_DATA  = 43; // XIAO D6
static constexpr uint8_t ONBOARD_LED_PIN = 21; // XIAO ESP32S3 user LED.
static constexpr uint8_t ONBOARD_LED_ON = LOW;
static constexpr uint8_t ONBOARD_LED_OFF = HIGH;

static constexpr uint8_t A_CUR_SNS = 4;  // XIAO D3, ADC; jumpered from D7/A current sense
static constexpr uint8_t DC_IN_MON = 8;  // XIAO D9, ADC
static constexpr uint8_t B_CUR_SNS = 9;  // XIAO D10, ADC
static constexpr uint8_t A_CUR_SNS_TIED_UNUSED = 44; // XIAO D7, tied to A_CUR_SNS; leave disabled

static constexpr bool A_CUR_SNS_ADC_CAPABLE = true;
static constexpr bool B_CUR_SNS_ADC_CAPABLE = true;
static constexpr bool DC_IN_MON_ADC_CAPABLE = true;

static constexpr uint8_t GPIO7_D8_DO_NOT_USE = 7;

static_assert(LED_A_DATA != GPIO7_D8_DO_NOT_USE, "GPIO7/D8 is tied to GND; do not use it.");
static_assert(LED_A_CLOCK != GPIO7_D8_DO_NOT_USE, "GPIO7/D8 is tied to GND; do not use it.");
static_assert(LED_B_DATA != GPIO7_D8_DO_NOT_USE, "GPIO7/D8 is tied to GND; do not use it.");
static_assert(LED_B_CLOCK != GPIO7_D8_DO_NOT_USE, "GPIO7/D8 is tied to GND; do not use it.");
static_assert(ONBOARD_LED_PIN != GPIO7_D8_DO_NOT_USE, "GPIO7/D8 is tied to GND; do not use it.");
static_assert(A_CUR_SNS != GPIO7_D8_DO_NOT_USE, "GPIO7/D8 is tied to GND; do not use it.");
static_assert(A_CUR_SNS_TIED_UNUSED != GPIO7_D8_DO_NOT_USE, "GPIO7/D8 is tied to GND; do not use it.");
static_assert(B_CUR_SNS != GPIO7_D8_DO_NOT_USE, "GPIO7/D8 is tied to GND; do not use it.");
static_assert(DC_IN_MON != GPIO7_D8_DO_NOT_USE, "GPIO7/D8 is tied to GND; do not use it.");

static constexpr uint16_t AH_RAY_MAX_LED_COUNT = 1000;
static constexpr uint16_t AH_RAY_DEFAULT_NUM_LEDS_A = 5;
static constexpr uint16_t AH_RAY_DEFAULT_NUM_LEDS_B = 5;

#define AH_RAY_LED_CHIPSET WS2812B
#define AH_RAY_LED_COLOR_ORDER GRB

static constexpr uint16_t AH_RAY_ADC_MAX_COUNTS = 4095;
static constexpr float AH_RAY_ADC_FULL_SCALE_VOLTS = 3.3f;
static constexpr float AH_RAY_INPUT_DIVIDER_RATIO = 11.0f;
static constexpr float AH_RAY_INPUT_VOLTS_PER_COUNT =
  (AH_RAY_ADC_FULL_SCALE_VOLTS * AH_RAY_INPUT_DIVIDER_RATIO) / AH_RAY_ADC_MAX_COUNTS;

static constexpr float AH_RAY_CURRENT_SHUNT_OHMS = 0.010f;
static constexpr float AH_RAY_INA180_GAIN = 50.0f;
static constexpr float AH_RAY_CURRENT_VOLTS_PER_AMP =
  AH_RAY_CURRENT_SHUNT_OHMS * AH_RAY_INA180_GAIN;
static constexpr float AH_RAY_CURRENT_AMPS_PER_COUNT =
  AH_RAY_ADC_FULL_SCALE_VOLTS / AH_RAY_ADC_MAX_COUNTS / AH_RAY_CURRENT_VOLTS_PER_AMP;

static constexpr float AH_RAY_OVERCURRENT_WARN_AMPS = 5.5f;
static constexpr float AH_RAY_OVERCURRENT_CRITICAL_AMPS = 6.0f;
static constexpr uint16_t AH_RAY_ADC_CLIPPED_COUNTS = 4000;
