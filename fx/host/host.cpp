#include "Arduino.h"
#include "effects.h"
#include "new_effects.h"
#include "spatial.h"
#include "node_config.h"
#include "fx_package.h"
#include <array>
#include "fastled_rgb.inc"

static uint32_t clockMs = 0;
extern "C" uint32_t millis() { return clockMs; }
extern "C" uint32_t micros() { return clockMs * 1000U; }
extern "C" void pinMode(uint8_t, uint8_t) {}
extern "C" void delay(int) {}
extern "C" void yield() {}

NodeRuntimeConfig nodeConfig;
VirtualNodeList virtualNodeLists[2];
static NodeStripConfig strips[2];
NodeStripConfig nodeConfigGetStrip(StripOutput strip) { return strips[strip == STRIP_B]; }
const VirtualNodeList& nodeVirtualNodesGet(StripOutput strip) { return virtualNodeLists[strip == STRIP_B]; }
// Generated at build time from node_config.cpp, so segment lookup stays in sync.
#include "virtual_lookup.inc"

struct HostRunState {
  bool valid = true;
  RunPacket pkt = {};
  uint32_t appliedAtMs = 0;
  uint32_t beatAtMs = 0;
};
static HostRunState activeRun;
static struct { bool active = false; } identifyStrip;
uint32_t ledEngineGetSyncedTimeMs() { return clockMs; }
static uint16_t activeLedCount(StripOutput strip) { return strips[strip == STRIP_B].numLeds; }
static void clearUnusedTail(CRGB* leds, uint16_t count) {
  fill_solid(leds + count, MAX_LED_COUNT - count, CRGB::Black);
}
static bool identifyMatchesStrip(StripOutput) { return false; }
static void renderIdentifyStrip(StripOutput, CRGB*, uint16_t) {}
// Exact buildNodePlacement, color conversion, special effects and renderStrip
// functions are extracted from led_engine.cpp without editing the firmware.
#include "render_pipeline.inc"

// Desktop capacity only; the firmware and radio protocol remain unchanged.
static constexpr int EMULATED_NODE_CAPACITY = 128;
static std::array<std::array<CRGB, MAX_LED_COUNT>, EMULATED_NODE_CAPACITY * 2> buffers;
static std::array<uint16_t, EMULATED_NODE_CAPACITY * 2> lastCounts = {};
static int currentSlot = 0;

#define EXPORT extern "C"
EXPORT void lab_reset() {
  for (auto& strip : buffers) fill_solid(strip.data(), MAX_LED_COUNT, CRGB::Black);
  lastCounts.fill(0);
  clockMs = 0;
}
EXPORT int lab_version() { return AH_FX_PACKAGE_VERSION; }
EXPORT int lab_node_capacity() { return EMULATED_NODE_CAPACITY; }
EXPORT int lab_effect_count() { return AH_FX_LIGHT_EFFECT_COUNT; }
EXPORT int lab_spatial_count() { return AH_FX_SPATIAL_EFFECT_COUNT; }
EXPORT const char* lab_effect_name(int i) { return ah_fx_light_effect_name(i); }
EXPORT const char* lab_spatial_name(int i) { return ah_fx_spatial_effect_name(i); }
EXPORT const char* lab_value_name(int channel, int value) {
  static char name[128];
  ah_fx_format_dmx_value_name(channel, value, name, sizeof(name));
  return name;
}
EXPORT void lab_configure(int slot, const uint16_t* config, const uint16_t* segments, int segmentCount) {
  currentSlot = slot;
  nodeConfig.uniqueNodeId = config[0];
  int output = slot % 2;
  auto& strip = strips[output];
  strip.numLeds = config[1]; strip.x = config[2]; strip.y = config[3];
  strip.z = config[4]; strip.span = config[5]; strip.axis = config[6];
  strip.orientation = config[7]; strip.membership = config[8];
  nodeConfig.stageX = config[9]; nodeConfig.stageY = config[10]; nodeConfig.stageZ = config[11];
  auto& list = virtualNodeLists[output]; list = {};
  list.count = segmentCount;
  for (int i = 0; i < segmentCount; ++i) {
    auto& v = list.nodes[i]; const uint16_t* s = segments + i * 9;
    v.startLed = s[0]; v.ledCount = s[1]; v.x = s[2]; v.y = s[3];
    v.z = s[4]; v.span = s[5]; v.axis = s[6]; v.orientation = s[7]; v.membership = s[8];
  }
}
EXPORT void lab_render(const uint8_t* dmx, const uint8_t* rgb, uint32_t now,
                       uint32_t start, uint32_t beat, int blackout, uint8_t* out, int16_t* xyz) {
  clockMs = now;
  activeRun.appliedAtMs = start; activeRun.beatAtMs = beat;
  auto& p = activeRun.pkt;
  p.brightness = dmx[0]; p.mainColor = dmx[1]; p.accentDimmer = dmx[2];
  p.accentColor = dmx[3]; p.bpm = dmx[4]; p.fxSpeed = dmx[5];
  p.spatialEffect = dmx[6]; p.lightEffect = dmx[7]; p.fxFade = dmx[8];
  p.fxExpand = dmx[9]; p.fxWarp = dmx[10]; p.zone = dmx[11];
  p.colorR = rgb[0]; p.colorG = rgb[1]; p.colorB = rgb[2];
  p.flags = blackout ? RF_BLACKOUT : 0; p.settingCommand = 0;
  auto& leds = buffers[currentSlot];
  StripOutput output = currentSlot % 2 ? STRIP_B : STRIP_A;
  renderStrip(output, leds.data(), lastCounts[currentSlot]);
  NodePlacement node = buildNodePlacement(output);
  for (int i = 0; i < activeLedCount(output); ++i) {
    out[i*3] = leds[i].r; out[i*3+1] = leds[i].g; out[i*3+2] = leds[i].b;
    auto s = computeSpatialFieldSampleForPixel(node, i, activeLedCount(output));
    xyz[i*3] = s.nodeX2; xyz[i*3+1] = s.nodeY2; xyz[i*3+2] = s.nodeZ2;
  }
}
