#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "board_ah_ray_dual_s3.h"
#include "packets.h"

static constexpr uint16_t MAX_LED_COUNT = AH_RAY_MAX_LED_COUNT;
static constexpr uint16_t DEFAULT_NUM_LEDS = AH_RAY_DEFAULT_NUM_LEDS_A;

enum StripOutput : uint8_t {
  STRIP_A = 0,
  STRIP_B = 1,
  STRIP_ALL = 0xFF
};

struct NodeStripConfig {
  uint8_t x = 1;
  uint8_t y = 1;
  uint8_t span = 1;
  uint8_t axis = AXIS_X;
  uint8_t orientation = ORIENT_RIGHT;
  uint8_t z = 0;
  uint8_t membership = 0;

  uint16_t numLeds = DEFAULT_NUM_LEDS;
  uint8_t icType = 2;      // 2 = WS2812B
  uint8_t rgbOrder = 0;    // 0 = GRB
};

struct NodeRuntimeConfig {
  uint8_t uniqueNodeId = 0xFF;
  uint16_t masterId = 0;

  // Strip A is kept as the protocol-visible legacy map.
  uint8_t x = 1;
  uint8_t y = 1;
  uint8_t span = 1;
  uint8_t axis = AXIS_X;
  uint8_t orientation = ORIENT_RIGHT;
  uint8_t z = 0;
  uint8_t membership = 0;

  uint16_t numLeds = AH_RAY_DEFAULT_NUM_LEDS_A;
  uint8_t icType = 2;      // 2 = WS2812B
  uint8_t rgbOrder = 0;    // 0 = GRB

  uint8_t stageX = 5;
  uint8_t stageY = 1;
  uint8_t stageZ = STAGE_LEVEL_GROUND;

  NodeStripConfig stripB = {
    1, 2, 1, AXIS_X, ORIENT_RIGHT, 0, 1,
    AH_RAY_DEFAULT_NUM_LEDS_B, 2, 0
  };

  bool overrideActive = false;
};

struct SavedNodeConfig {
  uint32_t magic;
  uint8_t version;
  NodeRuntimeConfig cfg;
  uint8_t checksum;
};

struct SavedRunState {
  uint32_t magic;
  uint8_t version;
  RunPacket run;
  uint8_t checksum;
};

struct VirtualNodeList {
  uint8_t count = 0;
  VirtualNodeDefinition nodes[AH_MAX_VIRTUAL_NODES_PER_OUTPUT] = {};
};

extern NodeRuntimeConfig nodeConfig;
extern NodeState nodeState;
extern uint8_t myMac[6];
extern uint8_t masterMac[6];
extern VirtualNodeList virtualNodeLists[2];

void nodeConfigBegin();
bool nodeConfigLoad();
bool nodeConfigSave();

bool nodeRunLoad(RunPacket& out);
bool nodeRunSave(const RunPacket& run);

NodeStripConfig nodeConfigGetStrip(StripOutput strip);
void nodeConfigSetStrip(StripOutput strip, const NodeStripConfig& cfg);
void nodeConfigSanitize();

const VirtualNodeList& nodeVirtualNodesGet(StripOutput strip);
bool nodeVirtualNodesSet(StripOutput strip, const VirtualNodeDefinition* nodes, uint8_t count);
bool nodeVirtualNodesSave();
void nodeVirtualNodesReset(StripOutput strip);
bool nodeVirtualNodeForPixel(
  StripOutput strip,
  uint16_t pixel,
  VirtualNodeDefinition& out,
  uint16_t& localPixel
);

uint8_t simpleChecksum(const uint8_t* data, size_t len);
bool macEquals(const uint8_t a[6], const uint8_t b[6]);
bool macIsZero(const uint8_t mac[6]);
void clearMac(uint8_t mac[6]);


