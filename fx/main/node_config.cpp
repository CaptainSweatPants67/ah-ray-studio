#include "node_config.h"
#include <WiFi.h>
#include <string.h>

static constexpr uint32_t CONFIG_MAGIC = 0xA871C001;
static constexpr uint32_t RUN_MAGIC    = 0xA871C002;
static constexpr uint32_t VIRTUAL_NODE_MAGIC = 0xA871C003;
static constexpr uint8_t SAVE_VERSION  = 5;
static constexpr uint8_t VIRTUAL_NODE_SAVE_VERSION = 1;

Preferences prefs;

NodeRuntimeConfig nodeConfig;
NodeState nodeState = NODE_BOOTING;
uint8_t myMac[6] = {0};
uint8_t masterMac[6] = {0};
VirtualNodeList virtualNodeLists[2];

struct SavedVirtualNodes {
  uint32_t magic;
  uint8_t version;
  VirtualNodeList outputs[2];
  uint8_t checksum;
};

static uint8_t clampU8(uint8_t v, uint8_t lo, uint8_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static uint16_t clampU16(uint16_t v, uint16_t lo, uint16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static uint8_t sanitizeLedProtocol(uint8_t v) {
  switch (v) {
    case LED_PROTOCOL_WS2812B:
    case LED_PROTOCOL_WS2811:
    case LED_PROTOCOL_SK6812:
    case LED_PROTOCOL_TM1812:
    case LED_PROTOCOL_APA102:
      return v;
    default:
      return LED_PROTOCOL_WS2812B;
  }
}

static void sanitizeStrip(NodeStripConfig& s) {
  s.x = clampU8(s.x, 1, 9);
  s.y = clampU8(s.y, 1, 9);
  s.span = clampU8(s.span, 1, 9);
  if (s.axis > AXIS_Y) s.axis = AXIS_X;
  if (s.orientation > ORIENT_CEILING) s.orientation = ORIENT_RIGHT;
  s.z = clampU8(s.z, 0, 8);
  s.numLeds = clampU16(s.numLeds, 1, MAX_LED_COUNT);
  s.icType = sanitizeLedProtocol(s.icType);
  s.rgbOrder = 0;
}

static uint8_t virtualListIndex(StripOutput strip) {
  return strip == STRIP_B ? 1 : 0;
}

static VirtualNodeDefinition defaultVirtualNode(StripOutput strip) {
  const NodeStripConfig cfg = nodeConfigGetStrip(strip);
  VirtualNodeDefinition out = {};
  out.startLed = 0;
  out.ledCount = cfg.numLeds;
  out.x = cfg.x;
  out.y = cfg.y;
  out.span = cfg.span;
  out.axis = cfg.axis;
  out.orientation = cfg.orientation;
  out.z = cfg.z;
  out.membership = cfg.membership;
  return out;
}

static bool virtualNodeDefinitionValid(const VirtualNodeDefinition& node) {
  if (node.ledCount == 0) return false;
  if (node.x < 1 || node.x > 9 || node.y < 1 || node.y > 9) return false;
  if (node.span < 1 || node.span > 9 || node.z > 8) return false;
  if (node.axis > AXIS_Y || node.orientation > ORIENT_CEILING) return false;
  return true;
}

static bool virtualNodeListValid(StripOutput strip, const VirtualNodeList& list) {
  if (list.count < 1 || list.count > AH_MAX_VIRTUAL_NODES_PER_OUTPUT) return false;
  const uint16_t totalLeds = nodeConfigGetStrip(strip).numLeds;
  uint32_t expectedStart = 0;
  for (uint8_t i = 0; i < list.count; ++i) {
    const VirtualNodeDefinition& node = list.nodes[i];
    if (!virtualNodeDefinitionValid(node) || node.startLed != expectedStart) return false;
    expectedStart += node.ledCount;
    if (expectedStart > totalLeds) return false;
  }
  return expectedStart == totalLeds;
}

void nodeVirtualNodesReset(StripOutput strip) {
  VirtualNodeList& list = virtualNodeLists[virtualListIndex(strip)];
  memset(&list, 0, sizeof(list));
  list.count = 1;
  list.nodes[0] = defaultVirtualNode(strip);
}

static void sanitizeVirtualNodes() {
  if (!virtualNodeListValid(STRIP_A, virtualNodeLists[0])) nodeVirtualNodesReset(STRIP_A);
  if (!virtualNodeListValid(STRIP_B, virtualNodeLists[1])) nodeVirtualNodesReset(STRIP_B);
}

void nodeConfigSanitize() {
  NodeStripConfig stripA = nodeConfigGetStrip(STRIP_A);
  sanitizeStrip(stripA);
  nodeConfigSetStrip(STRIP_A, stripA);

  sanitizeStrip(nodeConfig.stripB);

  nodeConfig.stageX = clampU8(nodeConfig.stageX, 1, 9);
  nodeConfig.stageY = clampU8(nodeConfig.stageY, 1, 9);
  nodeConfig.stageZ = clampU8(nodeConfig.stageZ, STAGE_LEVEL_GROUND, STAGE_LEVEL_CEILING);
}

const VirtualNodeList& nodeVirtualNodesGet(StripOutput strip) {
  return virtualNodeLists[virtualListIndex(strip)];
}

bool nodeVirtualNodesSet(StripOutput strip, const VirtualNodeDefinition* nodes, uint8_t count) {
  if (!nodes || count < 1 || count > AH_MAX_VIRTUAL_NODES_PER_OUTPUT) return false;
  VirtualNodeList candidate = {};
  candidate.count = count;
  memcpy(candidate.nodes, nodes, sizeof(VirtualNodeDefinition) * count);
  if (!virtualNodeListValid(strip, candidate)) return false;
  virtualNodeLists[virtualListIndex(strip)] = candidate;
  return true;
}

bool nodeVirtualNodeForPixel(
  StripOutput strip,
  uint16_t pixel,
  VirtualNodeDefinition& out,
  uint16_t& localPixel
) {
  const VirtualNodeList& list = nodeVirtualNodesGet(strip);
  for (uint8_t i = 0; i < list.count; ++i) {
    const VirtualNodeDefinition& node = list.nodes[i];
    const uint32_t end = (uint32_t)node.startLed + node.ledCount;
    if (pixel >= node.startLed && pixel < end) {
      out = node;
      localPixel = pixel - node.startLed;
      return true;
    }
  }
  return false;
}

NodeStripConfig nodeConfigGetStrip(StripOutput strip) {
  if (strip == STRIP_B) return nodeConfig.stripB;

  NodeStripConfig out;
  out.x = nodeConfig.x;
  out.y = nodeConfig.y;
  out.span = nodeConfig.span;
  out.axis = nodeConfig.axis;
  out.orientation = nodeConfig.orientation;
  out.z = nodeConfig.z;
  out.membership = nodeConfig.membership;
  out.numLeds = nodeConfig.numLeds;
  out.icType = nodeConfig.icType;
  out.rgbOrder = nodeConfig.rgbOrder;
  return out;
}

void nodeConfigSetStrip(StripOutput strip, const NodeStripConfig& cfg) {
  if (strip == STRIP_B) {
    nodeConfig.stripB = cfg;
    return;
  }

  nodeConfig.x = cfg.x;
  nodeConfig.y = cfg.y;
  nodeConfig.span = cfg.span;
  nodeConfig.axis = cfg.axis;
  nodeConfig.orientation = cfg.orientation;
  nodeConfig.z = cfg.z;
  nodeConfig.membership = cfg.membership;
  nodeConfig.numLeds = cfg.numLeds;
  nodeConfig.icType = cfg.icType;
  nodeConfig.rgbOrder = cfg.rgbOrder;
}

uint8_t simpleChecksum(const uint8_t* data, size_t len) {
  uint8_t c = 0;
  for (size_t i = 0; i < len; i++) c ^= data[i];
  return c;
}

bool macEquals(const uint8_t a[6], const uint8_t b[6]) {
  return memcmp(a, b, 6) == 0;
}

bool macIsZero(const uint8_t mac[6]) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0) return false;
  }
  return true;
}

void clearMac(uint8_t mac[6]) {
  memset(mac, 0, 6);
}

void nodeConfigBegin() {
  prefs.begin("ahray-node", false);
  if (prefs.getBytes("master-mac", masterMac, sizeof(masterMac)) != sizeof(masterMac)) {
    clearMac(masterMac);
  }
}

bool nodeConfigLoad() {
  SavedNodeConfig saved = {};
  size_t got = prefs.getBytes("cfg", &saved, sizeof(saved));

  if (got == sizeof(saved) &&
      saved.magic == CONFIG_MAGIC &&
      saved.version == SAVE_VERSION) {

    uint8_t old = saved.checksum;
    saved.checksum = 0;

    if (simpleChecksum((uint8_t*)&saved, sizeof(saved)) == old) {
      nodeConfig = saved.cfg;
      nodeConfigSanitize();
      Serial.println("Loaded saved node config");
      SavedVirtualNodes savedVirtual = {};
      size_t virtualGot = prefs.getBytes("vnodes", &savedVirtual, sizeof(savedVirtual));
      bool virtualLoaded = false;
      if (virtualGot == sizeof(savedVirtual) &&
          savedVirtual.magic == VIRTUAL_NODE_MAGIC &&
          savedVirtual.version == VIRTUAL_NODE_SAVE_VERSION) {
        uint8_t virtualChecksum = savedVirtual.checksum;
        savedVirtual.checksum = 0;
        if (simpleChecksum((uint8_t*)&savedVirtual, sizeof(savedVirtual)) == virtualChecksum) {
          memcpy(virtualNodeLists, savedVirtual.outputs, sizeof(virtualNodeLists));
          virtualLoaded = true;
        }
      }
      if (!virtualLoaded) memset(virtualNodeLists, 0, sizeof(virtualNodeLists));
      sanitizeVirtualNodes();
      Serial.println(virtualLoaded ? "Loaded virtual nodes" : "Using default virtual nodes");
      return true;
    }
  }

  nodeConfig = NodeRuntimeConfig{};
  nodeConfigSanitize();
  memset(virtualNodeLists, 0, sizeof(virtualNodeLists));
  sanitizeVirtualNodes();
  Serial.println("Using default node config");
  return false;
}

bool nodeVirtualNodesSave() {
  sanitizeVirtualNodes();
  SavedVirtualNodes saved = {};
  saved.magic = VIRTUAL_NODE_MAGIC;
  saved.version = VIRTUAL_NODE_SAVE_VERSION;
  memcpy(saved.outputs, virtualNodeLists, sizeof(virtualNodeLists));
  saved.checksum = 0;
  saved.checksum = simpleChecksum((uint8_t*)&saved, sizeof(saved));
  size_t written = prefs.putBytes("vnodes", &saved, sizeof(saved));
  Serial.println(written == sizeof(saved) ? "Virtual nodes saved" : "Virtual nodes save failed");
  return written == sizeof(saved);
}

bool nodeConfigSave() {
  nodeConfigSanitize();

  SavedNodeConfig saved = {};
  saved.magic = CONFIG_MAGIC;
  saved.version = SAVE_VERSION;
  saved.cfg = nodeConfig;
  saved.checksum = 0;
  saved.checksum = simpleChecksum((uint8_t*)&saved, sizeof(saved));

  size_t written = prefs.putBytes("cfg", &saved, sizeof(saved));
  if (!macIsZero(masterMac)) {
    prefs.putBytes("master-mac", masterMac, sizeof(masterMac));
  }
  bool configOk = written == sizeof(saved);
  bool virtualOk = nodeVirtualNodesSave();
  Serial.println(configOk ? "Config saved" : "Config save failed");
  return configOk && virtualOk;
}

bool nodeRunLoad(RunPacket& out) {
  SavedRunState saved = {};
  size_t got = prefs.getBytes("run", &saved, sizeof(saved));

  if (got == sizeof(saved) &&
      saved.magic == RUN_MAGIC &&
      saved.version == SAVE_VERSION) {

    uint8_t old = saved.checksum;
    saved.checksum = 0;

    if (simpleChecksum((uint8_t*)&saved, sizeof(saved)) == old) {
      out = saved.run;
      Serial.println("Loaded saved run state");
      return true;
    }
  }

  return false;
}

bool nodeRunSave(const RunPacket& run) {
  SavedRunState saved = {};
  saved.magic = RUN_MAGIC;
  saved.version = SAVE_VERSION;
  saved.run = run;
  saved.checksum = 0;
  saved.checksum = simpleChecksum((uint8_t*)&saved, sizeof(saved));

  size_t written = prefs.putBytes("run", &saved, sizeof(saved));
  Serial.println(written == sizeof(saved) ? "Run state saved" : "Run state save failed");
  return written == sizeof(saved);
}
