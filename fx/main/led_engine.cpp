#include "led_engine.h"
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif
#ifndef FASTLED_INTERRUPT_RETRY_COUNT
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#endif
#include <FastLED.h>
#include "node_config.h"
#include "effects.h"
#include "new_effects.h"
#include "spatial.h"
#include "fx_package_runtime.h"
#include "node_debug.h"
#include "board_ah_ray_dual_s3.h"

static CRGB ledsA[MAX_LED_COUNT];
static CRGB ledsB[MAX_LED_COUNT];

struct ActiveRunState {
  bool valid = false;
  RunPacket pkt = {};
  uint32_t appliedAtMs = 0;
  uint32_t beatAtMs = 0;
};

static ActiveRunState activeRun;
static ActiveRunState pendingRun;
static bool havePending = false;
static bool pendingRunClearBuffers = false;

struct IdentifyStripState {
  bool active = false;
  bool selected = false;
  uint8_t output = 0;
  uint8_t virtualNode = 0xFF;
  CRGB color = CRGB::Black;
};

static IdentifyStripState identifyStrip;

static uint32_t lastMasterTimeMs = 0;
static uint32_t localTimeBaseMs = 0;
static bool haveTimeSync = false;

static bool saveRunPending = false;
static uint32_t nextRunSaveMs = 0;
static uint32_t lastRunSaveMs = 0;
static uint16_t lastRenderedCountA = 0;
static uint16_t lastRenderedCountB = 0;

uint32_t ledEngineGetSyncedTimeMs() {
  if (!haveTimeSync) return millis();
  return lastMasterTimeMs + (millis() - localTimeBaseMs);
}

void ledEngineUpdateTimeSync(uint32_t masterTimeMs) {
  lastMasterTimeMs = masterTimeMs;
  localTimeBaseMs = millis();
  haveTimeSync = true;
}

static void scheduleRunSave() {
  saveRunPending = true;
  nextRunSaveMs = millis() + 10000;
}

static void serviceRunSave() {
  if (!saveRunPending) return;
  if (millis() < nextRunSaveMs) return;
  if ((millis() - lastRunSaveMs) < 30000) return;

  if (activeRun.valid) {
    RunPacket stored = activeRun.pkt;
    stored.fireAtMs = 0;
    nodeRunSave(stored);
    lastRunSaveMs = millis();
  }

  saveRunPending = false;
}

void ledEngineClearStrip() {
  fill_solid(ledsA, MAX_LED_COUNT, CRGB::Black);
  fill_solid(ledsB, MAX_LED_COUNT, CRGB::Black);
  FastLED.show();
  lastRenderedCountA = 0;
  lastRenderedCountB = 0;
}

static uint16_t activeLedCount(StripOutput strip) {
  uint16_t count = nodeConfigGetStrip(strip).numLeds;
  if (count > MAX_LED_COUNT) count = MAX_LED_COUNT;
  return count;
}

static uint16_t outputFrameLedCount(StripOutput strip) {
  const uint16_t active = activeLedCount(strip);
  constexpr uint16_t tailBlackoutLeds = 96;
  if (active >= MAX_LED_COUNT - tailBlackoutLeds) return MAX_LED_COUNT;
  return active + tailBlackoutLeds;
}

static void clearUnusedTail(CRGB* leds, uint16_t count) {
  if (count >= MAX_LED_COUNT) return;
  fill_solid(leds + count, MAX_LED_COUNT - count, CRGB::Black);
}

static RunPacket normalizeRunPacket(const RunPacket& pkt) {
  RunPacket out = pkt;

  if (out.flags & RUNF_DMX_OVERRIDE) {
    out.brightness = out.dmx[AH_FX_DMX_MAIN_DIMMER];
    out.mainColor = out.dmx[AH_FX_DMX_MAIN_COLOR];
    out.accentDimmer = out.dmx[AH_FX_DMX_ACCENT_DIMMER];
    out.accentColor = out.dmx[AH_FX_DMX_ACCENT_COLOR];
    out.bpm = out.dmx[AH_FX_DMX_BPM];
    out.fxSpeed = out.dmx[AH_FX_DMX_BEAT];
    out.spatialEffect = out.dmx[AH_FX_DMX_SPATIAL_EFFECT];
    out.lightEffect = out.dmx[AH_FX_DMX_LIGHT_EFFECT];
    out.fxFade = out.dmx[AH_FX_DMX_FX_FADE];
    out.fxExpand = out.dmx[AH_FX_DMX_FX_EXPAND];
    out.fxWarp = out.dmx[AH_FX_DMX_FX_WARP];
    out.zone = out.dmx[AH_FX_DMX_ZONE];
    out.colorR = 255;
    out.colorG = 96;
    out.colorB = 0;
    out.settingCommand = 0;
  }

  return out;
}

static bool shouldStartNewPhase(const RunPacket& pkt) {
  if (!activeRun.valid && !havePending) return true;
  if (pkt.flags & RUNF_RESET_PHASE) return true;

  const RunPacket& current = havePending ? pendingRun.pkt : activeRun.pkt;
  if (pkt.lightEffect != current.lightEffect) return true;
  if (pkt.spatialEffect != current.spatialEffect) return true;

  return false;
}

static bool shouldTriggerBeatPulse(const RunPacket& pkt) {
  return pkt.fxSpeed == 0 && (pkt.flags & RUNF_RESET_PHASE);
}

static bool runNeedsBufferClear(const RunPacket& next) {
  if (!activeRun.valid && !havePending) return true;

  const RunPacket& current = havePending ? pendingRun.pkt : activeRun.pkt;
  if (next.flags & RUNF_RESET_PHASE) return true;
  if (next.lightEffect != current.lightEffect) return true;
  if (next.spatialEffect != current.spatialEffect) return true;
  if (next.brightness != current.brightness) return true;
  if (next.mainColor != current.mainColor) return true;
  if (next.accentDimmer != current.accentDimmer) return true;
  if (next.accentColor != current.accentColor) return true;
  if (next.colorR != current.colorR) return true;
  if (next.colorG != current.colorG) return true;
  if (next.colorB != current.colorB) return true;
  // Fade, Expand, and Warp are live effect parameters. Updating them must not
  // erase trails or make the active effect look as though it restarted.
  if (next.zone != current.zone) return true;
  return false;
}

static void clearRenderBuffers() {
  fill_solid(ledsA, MAX_LED_COUNT, CRGB::Black);
  fill_solid(ledsB, MAX_LED_COUNT, CRGB::Black);
  lastRenderedCountA = 0;
  lastRenderedCountB = 0;
}

static uint32_t preservedRunStartMs() {
  if (havePending) return pendingRun.appliedAtMs;
  if (activeRun.valid) return activeRun.appliedAtMs;
  return ledEngineGetSyncedTimeMs();
}

static uint32_t preservedBeatTriggerMs() {
  if (havePending) return pendingRun.beatAtMs;
  if (activeRun.valid) return activeRun.beatAtMs;
  return ledEngineGetSyncedTimeMs();
}

bool ledEngineBegin() {
  NodeStripConfig stripA = nodeConfigGetStrip(STRIP_A);
  NodeStripConfig stripB = nodeConfigGetStrip(STRIP_B);
  const uint16_t ledCountA = outputFrameLedCount(STRIP_A);
  const uint16_t ledCountB = outputFrameLedCount(STRIP_B);
  switch (stripA.icType) {
    case LED_PROTOCOL_WS2811:
      FastLED.addLeds<WS2811, LED_A_DATA, AH_RAY_LED_COLOR_ORDER>(ledsA, ledCountA);
      break;
    case LED_PROTOCOL_SK6812:
      FastLED.addLeds<SK6812, LED_A_DATA, AH_RAY_LED_COLOR_ORDER>(ledsA, ledCountA);
      break;
    case LED_PROTOCOL_TM1812:
      FastLED.addLeds<TM1812, LED_A_DATA, AH_RAY_LED_COLOR_ORDER>(ledsA, ledCountA);
      break;
    case LED_PROTOCOL_APA102:
      FastLED.addLeds<APA102, LED_A_DATA, LED_A_CLOCK, AH_RAY_LED_COLOR_ORDER>(ledsA, ledCountA);
      break;
    case LED_PROTOCOL_WS2812B:
    default:
      FastLED.addLeds<WS2812B, LED_A_DATA, AH_RAY_LED_COLOR_ORDER>(ledsA, ledCountA);
      break;
  }
  switch (stripB.icType) {
    case LED_PROTOCOL_WS2811:
      FastLED.addLeds<WS2811, LED_B_DATA, AH_RAY_LED_COLOR_ORDER>(ledsB, ledCountB);
      break;
    case LED_PROTOCOL_SK6812:
      FastLED.addLeds<SK6812, LED_B_DATA, AH_RAY_LED_COLOR_ORDER>(ledsB, ledCountB);
      break;
    case LED_PROTOCOL_TM1812:
      FastLED.addLeds<TM1812, LED_B_DATA, AH_RAY_LED_COLOR_ORDER>(ledsB, ledCountB);
      break;
    case LED_PROTOCOL_APA102:
      FastLED.addLeds<APA102, LED_B_DATA, LED_B_CLOCK, AH_RAY_LED_COLOR_ORDER>(ledsB, ledCountB);
      break;
    case LED_PROTOCOL_WS2812B:
    default:
      FastLED.addLeds<WS2812B, LED_B_DATA, AH_RAY_LED_COLOR_ORDER>(ledsB, ledCountB);
      break;
  }
  FastLED.setBrightness(64);
  ledEngineClearStrip();

  // Boot firmware marker for first 5 LEDs.
  uint16_t testCountA = activeLedCount(STRIP_A);
  uint16_t testCountB = activeLedCount(STRIP_B);
  if (testCountA > 5) testCountA = 5;
  if (testCountB > 5) testCountB = 5;
  if (testCountA < 1) testCountA = 1;
  if (testCountB < 1) testCountB = 1;

  fill_solid(ledsA, testCountA, CRGB::Magenta);
  fill_solid(ledsB, testCountB, CRGB::Orange);
  FastLED.show();
  delay(250);
  fill_solid(ledsA, testCountA, CRGB::Cyan);
  fill_solid(ledsB, testCountB, CRGB::Blue);
  FastLED.show();
  delay(250);
  fill_solid(ledsA, testCountA, CRGB::Black);
  fill_solid(ledsB, testCountB, CRGB::Black);
  FastLED.show();

  return true;
}

void ledEngineApplyConfigChange(const NodeRuntimeConfig& previousConfig, uint16_t overrideMask) {
  NodeStripConfig previousA = nodeConfigGetStrip(STRIP_A);
  NodeStripConfig previousB = previousConfig.stripB;

  previousA.x = previousConfig.x;
  previousA.y = previousConfig.y;
  previousA.span = previousConfig.span;
  previousA.axis = previousConfig.axis;
  previousA.orientation = previousConfig.orientation;
  previousA.z = previousConfig.z;
  previousA.membership = previousConfig.membership;
  previousA.numLeds = previousConfig.numLeds;
  previousA.icType = previousConfig.icType;
  previousA.rgbOrder = previousConfig.rgbOrder;

  NodeStripConfig currentA = nodeConfigGetStrip(STRIP_A);
  NodeStripConfig currentB = nodeConfigGetStrip(STRIP_B);

  bool ledOutputChanged =
    previousA.numLeds != currentA.numLeds ||
    previousA.icType != currentA.icType ||
    previousA.rgbOrder != currentA.rgbOrder ||
    previousB.numLeds != currentB.numLeds ||
    previousB.icType != currentB.icType ||
    previousB.rgbOrder != currentB.rgbOrder;

  bool placementChanged =
    previousA.x != currentA.x ||
    previousA.y != currentA.y ||
    previousA.span != currentA.span ||
    previousA.axis != currentA.axis ||
    previousA.orientation != currentA.orientation ||
    previousA.z != currentA.z ||
    previousA.membership != currentA.membership ||
    previousB.x != currentB.x ||
    previousB.y != currentB.y ||
    previousB.span != currentB.span ||
    previousB.axis != currentB.axis ||
    previousB.orientation != currentB.orientation ||
    previousB.z != currentB.z ||
    previousB.membership != currentB.membership ||
    previousConfig.stageX != nodeConfig.stageX ||
    previousConfig.stageY != nodeConfig.stageY ||
    previousConfig.stageZ != nodeConfig.stageZ;

  if (ledOutputChanged) {
    ledEngineClearStrip();
  } else if (overrideMask & OVR_NUM_LEDS) {
    clearRenderBuffers();
    FastLED.show();
  }

  if (ledOutputChanged || placementChanged) {
    if (activeRun.valid) {
      activeRun.appliedAtMs = ledEngineGetSyncedTimeMs();
    }
    if (havePending) {
      pendingRun.appliedAtMs = 0;
    }
  }

  if (ledOutputChanged) {
    nodeDebugLog("LED cfg A=%u B=%u ic=%u order=%u",
                 (unsigned)currentA.numLeds,
                 (unsigned)currentB.numLeds,
                 (unsigned)currentA.icType,
                 (unsigned)currentA.rgbOrder);
  } else if (placementChanged) {
    nodeDebugLog("Map cfg A=%u,%u,%u B=%u,%u,%u stage=%u,%u,%u",
                 (unsigned)currentA.x,
                 (unsigned)currentA.y,
                 (unsigned)currentA.z,
                 (unsigned)currentB.x,
                 (unsigned)currentB.y,
                 (unsigned)currentB.z,
                 (unsigned)nodeConfig.stageX,
                 (unsigned)nodeConfig.stageY,
                 (unsigned)nodeConfig.stageZ);
  }
}

void ledEngineApplyVirtualNodeChange() {
  clearRenderBuffers();
  if (activeRun.valid) activeRun.appliedAtMs = ledEngineGetSyncedTimeMs();
  if (havePending) pendingRun.appliedAtMs = 0;
}

void ledEngineLoadSavedRun() {
  RunPacket saved = {};
  bool loaded = nodeRunLoad(saved) && ledEngineApplyRunPacket(saved);
  if (!loaded) {
    RunPacket def = fxPackageDefaultRunPacket();
    ledEngineApplyRunPacket(def);
  }

  activeRun = pendingRun;
  activeRun.appliedAtMs = ledEngineGetSyncedTimeMs();
  activeRun.beatAtMs = activeRun.appliedAtMs;
  havePending = false;
  nodeState = NODE_STANDALONE_RUN;
}

bool ledEngineApplyRunPacket(const RunPacket& pkt) {
  if (pkt.magic != PACKET_MAGIC || pkt.type != PST_RUN) return false;
  if (!fxPackageRuntimeCompatible()) {
    nodeDebugLog("Run rejected: FX package mismatch");
    return false;
  }
  if (pkt.fxPackageVersion != AH_FX_PACKAGE_VERSION) {
    nodeDebugLog("Run rejected: fx v%u", (unsigned)pkt.fxPackageVersion);
    return false;
  }
  pendingRun.valid = true;
  pendingRun.pkt = normalizeRunPacket(pkt);
  pendingRunClearBuffers = runNeedsBufferClear(pendingRun.pkt);

  uint32_t now = ledEngineGetSyncedTimeMs();
  if (shouldStartNewPhase(pendingRun.pkt)) {
    pendingRun.appliedAtMs = pendingRun.pkt.fireAtMs == 0 ? now : pendingRun.pkt.fireAtMs;
    pendingRun.beatAtMs = pendingRun.appliedAtMs;
  } else {
    pendingRun.appliedAtMs = preservedRunStartMs();
    pendingRun.beatAtMs = shouldTriggerBeatPulse(pendingRun.pkt)
      ? (pendingRun.pkt.fireAtMs == 0 ? now : pendingRun.pkt.fireAtMs)
      : preservedBeatTriggerMs();
    pendingRun.pkt.fireAtMs = 0;
  }

  havePending = true;
  identifyStrip.active = false;
  return true;
}

void ledEngineApplyIdentifyStrip(const IdentifyStripPacket& pkt) {
  identifyStrip.active = (pkt.flags & IDF_ACTIVE) != 0;
  identifyStrip.selected = (pkt.flags & IDF_SELECTED) != 0;
  identifyStrip.output = pkt.output;
  identifyStrip.virtualNode = pkt.virtualNode;
  identifyStrip.color = CRGB(pkt.colorR, pkt.colorG, pkt.colorB);
}

static void applyPendingIfDue() {
  if (!havePending) return;

  uint32_t now = ledEngineGetSyncedTimeMs();
  if ((int32_t)(now - pendingRun.appliedAtMs) < 0) return;

  activeRun = pendingRun;
  havePending = false;
  if (pendingRunClearBuffers) {
    clearRenderBuffers();
    pendingRunClearBuffers = false;
  }

  if ((activeRun.pkt.flags & RUNF_LIVE_MODULATION) == 0) {
    nodeDebugLog("Run seq=%u fire=%lu now=%lu dim=%u fx=%u spatial=%u",
                 (unsigned)activeRun.pkt.sequence,
                 (unsigned long)activeRun.appliedAtMs,
                 (unsigned long)now,
                 (unsigned)activeRun.pkt.brightness,
                 (unsigned)activeRun.pkt.lightEffect,
                 (unsigned)activeRun.pkt.spatialEffect);
  }

  if (activeRun.pkt.flags & RF_SAVE_DEFAULT) {
    scheduleRunSave();
  }
}

static NodePlacement buildNodePlacement(StripOutput strip) {
  NodeStripConfig cfg = nodeConfigGetStrip(strip);
  NodePlacement n;
  n.nodeId = nodeConfig.uniqueNodeId;
  n.output = strip == STRIP_B ? 1 : 0;
  n.x = cfg.x;
  n.y = cfg.y;
  n.z = cfg.z;
  n.span = cfg.span;
  n.axis = cfg.axis;
  n.orientation = cfg.orientation;
  n.membership = cfg.membership;
  n.stageX = nodeConfig.stageX;
  n.stageY = nodeConfig.stageY;
  n.stageZ = nodeConfig.stageZ;
  return n;
}

static RenderParams buildRenderParams(const RunPacket& pkt) {
  RenderParams p;
  p.brightness = pkt.brightness;
  p.lightEffect = pkt.lightEffect;
  p.spatialEffect = pkt.spatialEffect;
  p.fxSpeed = pkt.fxSpeed;
  p.fxWarp = pkt.fxWarp;
  p.fxExpand = pkt.fxExpand;
  p.fxFade = pkt.fxFade;
  p.bpm = pkt.bpm;
  p.mainColorPreset = pkt.mainColor;
  p.accentColorPreset = pkt.accentColor;
  p.accentDimmer = pkt.accentDimmer;
  p.zone = pkt.zone;
  p.settingCommand = pkt.settingCommand;
  p.flags = pkt.flags;

  CRGB rawMain = pkt.mainColor > 0 ? presetToRgb(pkt.mainColor) : CRGB(pkt.colorR, pkt.colorG, pkt.colorB);
  p.mainColor = rawMain;
  p.mainColor.nscale8_video(pkt.brightness);

  if (pkt.accentColor > 0) {
    p.accentColor = presetToRgb(pkt.accentColor);
  } else {
    p.accentColor = rawMain;
  }
  p.accentColor.nscale8_video(pkt.accentDimmer);

  return p;
}

static bool pixelMatchesZone(uint8_t zoneValue, const NodePlacement& node, const SpatialFieldSample& sample) {
  uint8_t zone = ah_fx_zone_index(zoneValue);
  switch (zone) {
    case 0: return true;                                           // All zones.
    case 1: return node.axis == AXIS_Z;                            // Z axis only.
    case 2: return sample.nodeZ2 >= 18;                            // Ceiling level.
    case 3: return sample.nodeZ2 <= 2;                             // Ground level.
    case 4: return sample.nodeY2 <= 2;                             // Y1 backwall plane.
    case 5: return sample.nodeY2 >= 18;                            // Y9 front plane.
    case 6: return sample.nodeX2 <= 2;                             // X1 left plane.
    case 7: return sample.nodeX2 >= 18;                            // X9 right plane.
    case 8: return sample.nodeX2 <= 2 || sample.nodeX2 >= 18;      // Side planes.
    case 9: return sample.nodeY2 == sample.stageY2;                // Stage row plane.
    case 10: return sample.nodeX2 == sample.stageX2;               // Stage column plane.
    case 11: return sample.nodeX2 == sample.stageX2 &&
                    sample.nodeY2 == sample.stageY2;               // Stage position.
    default: return true;
  }
}

static uint8_t spatialPulseRingAmount(const RenderParams& params, const NodePlacement& node, const SpatialResult& spatial, uint16_t pixel, uint16_t count, int32_t runAgeMs) {
  if (runAgeMs < 0 || spatial.cycleMs == 0) return 0;

  if (spatial.beatMode && (uint32_t)runAgeMs >= spatial.cycleMs) return 0;
  uint16_t t = spatial.beatMode
    ? (uint16_t)runAgeMs
    : (uint16_t)((uint32_t)runAgeMs % spatial.cycleMs);
  uint16_t delay = (uint16_t)computeSpatialPixelDelayMs(params, node, pixel, count);
  if (delay >= spatial.cycleMs) delay = spatial.cycleMs - 1;
  uint16_t coreMs = map(params.fxWarp, 0, 255, max<uint16_t>(8, spatial.cycleMs / 96), max<uint16_t>(16, spatial.cycleMs / 10));
  uint16_t trailMs = map(params.fxFade, 0, 255, max<uint16_t>(16, spatial.cycleMs / 48), max<uint16_t>(32, spatial.cycleMs / 3));

  if (t < delay) return 0;

  uint16_t age = t - delay;
  if (age < coreMs) return 255;
  if (age < (coreMs + trailMs)) {
    return 255 - (((uint32_t)(age - coreMs) * 255UL) / trailMs);
  }

  return 0;
}

static void renderSpatialPulseRing(CRGB* leds, uint16_t count, const RenderParams& params, const NodePlacement& node, const SpatialResult& spatial, int32_t runAgeMs) {
  for (uint16_t i = 0; i < count; i++) {
    uint8_t shimmer = inoise8((i * 31) + node.membership * 17, runAgeMs / 11);
    uint8_t baseLevel = qadd8(18, scale8(shimmer, 42));
    CRGB c = params.accentColor;
    c.nscale8_video(baseLevel);

    uint8_t fallbackSpeed = (uint8_t)map(params.fxSpeed, 0, 255, 18, 3);
    if (fallbackSpeed < 1) fallbackSpeed = 1;
    uint8_t ring = spatial.enabled
      ? spatialPulseRingAmount(params, node, spatial, i, count, runAgeMs)
      : sin8((runAgeMs / fallbackSpeed) + i * 18);

    if (ring > 0) {
      c = blend(c, params.mainColor, ring);
    }

    leds[i] = c;
  }
}

static uint8_t fieldAngleToByte(int16_t a, int16_t b) {
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

static uint8_t standingWaveHorizontalDistance2(const SpatialFieldSample& sample) {
  uint16_t dx = sample.dx2 < 0 ? (uint16_t)-sample.dx2 : (uint16_t)sample.dx2;
  uint16_t dy = sample.dy2 < 0 ? (uint16_t)-sample.dy2 : (uint16_t)sample.dy2;
  uint16_t hi = dx > dy ? dx : dy;
  uint16_t lo = dx > dy ? dy : dx;
  uint16_t approx = hi + (lo / 2);
  return approx > 255 ? 255 : (uint8_t)approx;
}

static int16_t standingWaveCoord2(const SpatialFieldSample& sample, uint8_t spatialEffect) {
  if (sample.dx2 == 0 && sample.dy2 == 0) return 0;

  switch (spatialEffect) {
    case 3: return sample.dx2;
    case 17: return sample.dx2;
    case 4: return sample.dy2;
    case 18: return sample.dy2;
    case 19:
    case 20:
      return standingWaveHorizontalDistance2(sample);
    case 7: return (sample.dx2 + sample.dy2) / 2;
    case 8: return (sample.dx2 - sample.dy2) / 2;
    case 1:
    case 2:
    case 5:
    case 6:
    case 13:
    case 14:
    case 15:
    case 16:
    case 23:
      return standingWaveHorizontalDistance2(sample);
    case 22:
      return (int16_t)fieldAngleToByte(sample.dx2, sample.dy2) - 128;
    case 12:
    case 24:
      return sample.dx2 + sample.dy2 * 3;
    default:
      return standingWaveHorizontalDistance2(sample);
  }
}

static void renderStandingWave(CRGB* leds, uint16_t count, const RenderParams& params, const NodePlacement& node) {
  uint8_t waveScale = map(params.fxWarp, 0, 255, 14, 58);
  uint8_t softness = map(params.fxFade, 0, 255, 8, 96);
  int16_t amplitude2 = map(params.fxExpand, 0, 255, 0, 8);

  for (uint16_t i = 0; i < count; i++) {
    SpatialFieldSample sample = computeSpatialFieldSampleForPixel(node, i, count);
    int16_t coord2 = standingWaveCoord2(sample, params.spatialEffect);
    uint8_t wave = sin8((uint8_t)(128 + coord2 * waveScale));
    int16_t signedWave = (int16_t)wave - 128;
    int16_t surfaceZ2 = 10 + ((signedWave * amplitude2) / 128);
    int16_t delta = (sample.nodeZ2 - surfaceZ2) * 32;

    uint8_t blendAmount;
    if (delta <= -(int16_t)softness) {
      blendAmount = 0;
    } else if (delta >= (int16_t)softness) {
      blendAmount = 255;
    } else {
      blendAmount = ((int32_t)(delta + softness) * 255L) / ((int16_t)softness * 2);
    }

    leds[i] = blend(params.mainColor, params.accentColor, blendAmount);
  }
}

static void renderMapPreview(CRGB* leds, uint16_t count, const RenderParams& params) {
  bool selected = params.fxExpand >= 128;
  if (selected) {
    CRGB c = params.mainColor;
    uint8_t pulse = beatsin8(18, 96, 255);
    c.nscale8_video(pulse);
    fill_solid(leds, count, c);
    return;
  }

  fill_solid(leds, count, CRGB::Black);
  CRGB dim = params.mainColor;
  dim.nscale8_video(1);
  for (uint16_t i = 0; i < count; i += 2) {
    leds[i] = dim;
  }
}

static void renderIdentifyStrip(StripOutput strip, CRGB* leds, uint16_t count) {
  if (!identifyStrip.active) return;
  if (identifyStrip.selected) {
    CRGB c = identifyStrip.color;
    uint8_t pulse = beatsin8(22, 96, 255);
    c.nscale8_video(pulse);
    fill_solid(leds, count, CRGB::Black);
    const VirtualNodeList& list = nodeVirtualNodesGet(strip);
    if (identifyStrip.virtualNode < list.count) {
      const VirtualNodeDefinition& node = list.nodes[identifyStrip.virtualNode];
      uint16_t start = min<uint16_t>(node.startLed, count);
      uint16_t length = min<uint16_t>(node.ledCount, count - start);
      fill_solid(leds + start, length, c);
    } else {
      fill_solid(leds, count, c);
    }
    return;
  }

  fill_solid(leds, count, CRGB::Black);
  CRGB dim = identifyStrip.color;
  dim.nscale8_video(5);
  for (uint16_t i = 0; i < count; i += 2) {
    leds[i] = dim;
  }
}

static bool identifyMatchesStrip(StripOutput strip) {
  return identifyStrip.output == STRIP_ALL || identifyStrip.output == strip;
}

static void renderStrip(StripOutput strip, CRGB* leds, uint16_t& lastRenderedCount) {
  uint16_t count = activeLedCount(strip);
  if (count == 0) {
    fill_solid(leds, MAX_LED_COUNT, CRGB::Black);
    lastRenderedCount = 0;
    return;
  }

  if (identifyStrip.active && identifyMatchesStrip(strip)) {
    renderIdentifyStrip(strip, leds, count);
    clearUnusedTail(leds, count);
    lastRenderedCount = count;
    return;
  }

  if (!activeRun.valid) {
    fill_solid(leds, count, CRGB::Black);
    clearUnusedTail(leds, count);
    lastRenderedCount = count;
    return;
  }

  const RunPacket& pkt = activeRun.pkt;

  if (pkt.flags & RF_BLACKOUT) {
    fill_solid(leds, count, CRGB::Black);
    clearUnusedTail(leds, count);
    lastRenderedCount = count;
    return;
  }

  if (count < lastRenderedCount) {
    clearUnusedTail(leds, count);
  }

  RenderParams params = buildRenderParams(pkt);
  if (pkt.flags & RUNF_MAP_PREVIEW) {
    renderMapPreview(leds, count, params);
    lastRenderedCount = count;
    return;
  }

  NodePlacement node = buildNodePlacement(strip);
  SpatialResult spatial = computeSpatialResult(params, node);

  uint32_t now = ledEngineGetSyncedTimeMs();

  int32_t runAge = (int32_t)(now - activeRun.appliedAtMs);
  int32_t beatAge = (int32_t)(now - activeRun.beatAtMs);
  int32_t localAge = runAge - spatial.nodeDelayMs;
  bool beatMode = params.fxSpeed == 0;
  int32_t beatLocalAge = beatAge - spatial.nodeDelayMs;
  spatial.envelope = computeSpatialEnvelope(spatial, beatMode ? beatLocalAge : localAge);

  int32_t phase = beatMode ? beatLocalAge : (spatial.enabled ? runAge : localAge);
  if (phase < 0) phase = 0;

  EffectContext ctx;
  ctx.nowMs = now;
  ctx.phaseMs = phase;
  ctx.beatAgeMs = beatLocalAge < 0 ? 0 : (uint32_t)beatLocalAge;
  if (params.lightEffect == 45) {
    // Parallel room particles share the run/beat clock across all nodes.
    const int32_t roomAge = beatMode ? beatAge : runAge;
    ctx.phaseMs = roomAge < 0 ? 0 : (uint32_t)roomAge;
    ctx.beatAgeMs = beatAge < 0 ? 0 : (uint32_t)beatAge;
  }
  ctx.params = params;
  ctx.spatial = spatial;
  ctx.node = node;

  bool spatialPulseRing = params.lightEffect == 7;
  bool standingWave = params.lightEffect == 8;
  if (spatialPulseRing) {
    renderSpatialPulseRing(leds, count, params, node, spatial, beatMode ? beatAge : runAge);
  } else if (standingWave) {
    renderStandingWave(leds, count, params, node);
  } else {
    renderEffect(leds, count, ctx);
  }

  if (spatial.enabled && !spatialPulseRing && !standingWave &&
      !effectUsesIntrinsicSpatialGeometry(params.lightEffect)) {
    for (uint16_t i = 0; i < count; i++) {
      uint8_t envelope = computeSpatialPixelEnvelope(params, node, spatial, i, count, beatMode ? beatAge : runAge);
      uint8_t spatialScale = scale8_video(spatial.intensity, envelope);
      if (spatialScale < 255) {
        leds[i].nscale8_video(spatialScale);
      }
    }
  }

  if (ah_fx_zone_index(params.zone) != 0) {
    for (uint16_t i = 0; i < count; i++) {
      SpatialFieldSample sample = computeSpatialFieldSampleForPixel(node, i, count);
      NodePlacement pixelNode = resolveNodePlacementForPixel(node, i, count);
      if (!pixelMatchesZone(params.zone, pixelNode, sample)) leds[i] = CRGB::Black;
    }
  }

  lastRenderedCount = count;
}

static void renderActive() {
  renderStrip(STRIP_A, ledsA, lastRenderedCountA);
  renderStrip(STRIP_B, ledsB, lastRenderedCountB);
  clearUnusedTail(ledsA, activeLedCount(STRIP_A));
  clearUnusedTail(ledsB, activeLedCount(STRIP_B));
  FastLED.setBrightness(255);
  FastLED.show();
}

void ledEngineLoop() {
  applyPendingIfDue();
  renderActive();
  serviceRunSave();
}
