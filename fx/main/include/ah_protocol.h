#pragma once

#include <Arduino.h>
#include "fx_package.h"

static constexpr uint8_t PACKET_MAGIC = 0xAF;
static constexpr uint8_t PROTOCOL_VERSION = 4;
static constexpr uint8_t REGION_LIST_REV = 2;
static constexpr uint8_t AH_MAX_VIRTUAL_NODES_PER_OUTPUT = 8;

enum NodeType : uint8_t {
  NODE_TYPE_LEGACY = 1,
  NODE_TYPE_DUAL_S3 = 2
};

enum LedProtocolType : uint8_t {
  LED_PROTOCOL_WS2812B = 2,
  LED_PROTOCOL_WS2811 = 3,
  LED_PROTOCOL_SK6812 = 4,
  LED_PROTOCOL_TM1812 = 5,
  LED_PROTOCOL_APA102 = 6
};
static constexpr uint8_t FX_PACKAGE_VERSION = AH_FX_PACKAGE_VERSION;
static constexpr uint8_t NODE_SOFTWARE_VERSION_LEN = 12;
static constexpr uint8_t NODE_HARDWARE_VERSION_LEN = 12;

enum PacketStructType : uint8_t {
  PST_TIME              = 0x01,
  PST_RUN               = 0x02,
  PST_DISCOVERY_START   = 0x10,
  PST_DISCOVERY_REPLY   = 0x11,
  PST_DISCOVERY_ACK     = 0x12,
  PST_CONFIG_REQUEST    = 0x13,
  PST_NODE_STATUS       = 0x14,
  PST_NODE_OVERRIDE     = 0x15,
  PST_NODE_ACK          = 0x16,
  PST_HELLO_NODE        = 0x17,
  PST_IDENTIFY_STRIP    = 0x18,
  PST_OUTPUT_OVERRIDE   = 0x19,
  PST_NODE_CONTROL      = 0x1A,
  PST_VIRTUAL_NODE_CONFIG = 0x1B,
  PST_VIRTUAL_NODE_STATUS = 0x1C,
  PST_OTA_BEGIN         = 0x30,
  PST_OTA_DATA          = 0x31,
  PST_OTA_END           = 0x32,
  PST_OTA_ABORT         = 0x33,
  PST_OTA_STATUS        = 0x34
};

enum DiscoveryFlags : uint8_t {
  DISCOVERY_FORCE_ADOPT = 1 << 0
};

enum NodeStatusFlags : uint8_t {
  NSF_ONLINE           = 1 << 0,
  NSF_SEATED           = 1 << 1,
  NSF_ACTIVE           = 1 << 2,
  NSF_OVERRIDE_ACTIVE  = 1 << 3,
  NSF_VERSION_MISMATCH = 1 << 4,
  NSF_REGION_MISMATCH  = 1 << 5,
  NSF_ERROR            = 1 << 6
};

enum AxisType : uint8_t {
  AXIS_Z = 0,
  AXIS_X = 1,
  AXIS_Y = 2
};

enum OrientationType : uint8_t {
  ORIENT_RIGHT   = 0,
  ORIENT_LEFT    = 1,
  ORIENT_UP      = 2,
  ORIENT_DOWN    = 3,
  ORIENT_GROUND  = 4,
  ORIENT_CEILING = 5
};

enum StageLevel : uint8_t {
  STAGE_LEVEL_GROUND = 1,
  STAGE_LEVEL_RAISED = 5,
  STAGE_LEVEL_CEILING = 9
};

enum NodeState : uint8_t {
  NODE_BOOTING = 0,
  NODE_STANDALONE_RUN,
  NODE_STANDING_BY,
  NODE_RAISED_HAND,
  NODE_SEATED,
  NODE_ACTIVE,
  NODE_STALE_MASTER,
  NODE_IDLE,
  NODE_HEARD_DISCOVERY,
  NODE_WAITING_RANDOM_BACKOFF,
  NODE_SENT_DISCOVERY_REPLY,
  NODE_DISCOVERY_ACKED,
  NODE_CONFIG_REQUESTED,
  NODE_READY
};

enum OverrideMask : uint16_t {
  OVR_X             = 1 << 0,
  OVR_Y             = 1 << 1,
  OVR_SPAN          = 1 << 2,
  OVR_AXIS          = 1 << 3,
  OVR_ORIENTATION   = 1 << 4,
  OVR_Z             = 1 << 5,
  OVR_MEMBERSHIP    = 1 << 6,
  OVR_NUM_LEDS      = 1 << 7,
  OVR_IC_TYPE       = 1 << 8,
  OVR_RGB_ORDER     = 1 << 9,
  OVR_STAGE_X       = 1 << 10,
  OVR_STAGE_Y       = 1 << 11,
  OVR_STAGE_Z       = 1 << 12
};

enum OverrideFlags : uint8_t {
  OVF_APPLY_NOW      = 1 << 0,
  OVF_SAVE_TO_NVS    = 1 << 1,
  OVF_ACK_REQUIRED   = 1 << 2,
  OVF_CLEAR_OVERRIDE = 1 << 3
};

enum RunFlags : uint8_t {
  RUNF_DMX_OVERRIDE = 1 << 0,
  RUNF_BLACKOUT     = 1 << 1,
  RUNF_RESET_PHASE  = 1 << 2,
  RUNF_MAP_PREVIEW  = 1 << 3,
  RUNF_LIVE_MODULATION = 1 << 4,
  RUNF_SAVE_DEFAULT = 1 << 5
};

enum IdentifyStripFlags : uint8_t {
  IDF_ACTIVE   = 1 << 0,
  IDF_SELECTED = 1 << 1
};

enum AckCode : uint8_t {
  ACK_OK = 0,
  ACK_BAD_SESSION = 1,
  ACK_BAD_TARGET = 2,
  ACK_SAVE_OK = 3,
  ACK_SAVE_FAIL = 4,
  ACK_BAD_CONFIG = 5
};

enum NodeControlCommand : uint8_t {
  NODE_CONTROL_REBOOT = 1
};

enum OtaStatusCode : uint8_t {
  OTA_STATUS_IDLE = 0,
  OTA_STATUS_BEGIN_OK = 1,
  OTA_STATUS_CHUNK_OK = 2,
  OTA_STATUS_DONE = 3,
  OTA_STATUS_ABORTED = 4,
  OTA_STATUS_ERROR = 5
};

static constexpr uint8_t OTA_VERSION_TEXT_LEN = 12;
static constexpr uint16_t OTA_DATA_BYTES = 176;

#pragma pack(push, 1)

struct TimePacket {
  uint8_t magic;
  uint8_t type;
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint32_t masterTimeMs;
  uint8_t seq;
};

struct DiscoveryStartPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t version;
  uint8_t masterMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;  // Backward-compatible source alias during the MasterID transition.
  };
  uint8_t flags;
  uint16_t sequence;
  uint32_t masterTimeMs;
  uint16_t windowMs;
};

struct DiscoveryReplyPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t version;
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t savedNodeId;
  uint8_t nodeType;
  uint8_t firmwareMajor;
  uint8_t firmwareMinor;
  uint8_t fxPackageVersion;
  uint8_t nodeState;
};

struct DiscoveryAckPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t version;
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t targetNodeMac[6];
  uint8_t assignedNodeId;
};

struct HelloNodePacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  uint8_t masterMac[6];
  uint8_t uniqueNodeId;
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t version;
  uint8_t regionListRev;
};

struct ConfigRequestPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t requestId;
};

struct RunPacket {
  uint8_t magic;
  uint8_t type;
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t sequence;
  uint32_t fireAtMs;       // Master clock time when the base effect wave starts.
  uint8_t flags;
  uint8_t fxPackageVersion;
  uint8_t dmx[AH_FX_DMX_CHANNEL_COUNT];
  uint8_t brightness;
  uint8_t lightEffect;
  uint8_t spatialEffect;
  uint8_t fxSpeed;
  uint8_t fxWarp;
  uint8_t fxExpand;
  uint8_t fxFade;
  uint8_t bpm;
  uint8_t mainColor;
  uint8_t accentColor;
  uint8_t accentDimmer;
  uint8_t zone;
  uint8_t colorR;
  uint8_t colorG;
  uint8_t colorB;
  uint8_t settingCommand;
};

struct NodeStatusPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t uniqueNodeId;
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t protocolVersion;
  uint8_t regionListRev;
  uint8_t nodeMac[6];
  uint8_t x;
  uint8_t y;
  uint8_t span;
  uint8_t axis;
  uint8_t orientation;
  uint8_t z;            // Node height slice, 0 = ground, 8 = ceiling
  uint8_t membership;
  uint16_t numLeds;
  uint8_t icType;
  uint8_t rgbOrder;
  uint8_t stageX;
  uint8_t stageY;
  uint8_t stageZ;       // Stage height, 1 = ground, 9 = ceiling
  uint8_t nodeState;
  uint8_t lastRunSeq;
  uint8_t lastTimeSeq;
  int8_t lastMasterRssi;
  uint8_t flags;
  uint8_t errorFlags;
  uint8_t fxPackageVersion;
  char softwareVersion[NODE_SOFTWARE_VERSION_LEN];
  char hardwareVersion[NODE_HARDWARE_VERSION_LEN];
  uint16_t inputVoltageMv;
  uint16_t stripACurrentMa;
  uint16_t stripBCurrentMa;
  uint16_t inputVoltageRaw;
  uint16_t stripACurrentRaw;
  uint16_t stripBCurrentRaw;
  uint16_t stripBNumLeds;
  uint8_t stripBIcType;
  uint8_t stripBRgbOrder;
  uint8_t stripBX;
  uint8_t stripBY;
  uint8_t stripBSpan;
  uint8_t stripBAxis;
  uint8_t stripBOrientation;
  uint8_t stripBZ;
  uint32_t lastTimeSyncAgeMs;
};

struct NodeOverridePacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint16_t overrideMask;
  uint8_t x;
  uint8_t y;
  uint8_t span;
  uint8_t axis;
  uint8_t orientation;
  uint8_t z;            // Node height slice, 0 = ground, 8 = ceiling
  uint8_t membership;
  uint16_t numLeds;
  uint8_t icType;
  uint8_t rgbOrder;
  uint8_t stageX;
  uint8_t stageY;
  uint8_t stageZ;       // Stage height, 1 = ground, 9 = ceiling
  uint8_t flags;
};

struct OutputOverridePacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t output;
  uint16_t overrideMask;
  uint8_t x;
  uint8_t y;
  uint8_t span;
  uint8_t axis;
  uint8_t orientation;
  uint8_t z;
  uint8_t membership;
  uint16_t numLeds;
  uint8_t icType;
  uint8_t rgbOrder;
  uint8_t flags;
};

// A Virtual Node is a contiguous range on one physical LED output. The range
// keeps the physical LED timing continuous while giving the spatial renderer
// an independent placement at bends and corners.
struct VirtualNodeDefinition {
  uint16_t startLed;
  uint16_t ledCount;
  uint8_t x;
  uint8_t y;
  uint8_t span;
  uint8_t axis;
  uint8_t orientation;
  uint8_t z;
  uint8_t membership;
};

struct VirtualNodeConfigPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t output;
  uint8_t count;
  uint8_t flags;
  VirtualNodeDefinition nodes[AH_MAX_VIRTUAL_NODES_PER_OUTPUT];
};

struct VirtualNodeStatusPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t uniqueNodeId;
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t nodeMac[6];
  uint8_t regionListRev;
  uint8_t counts[2];
  VirtualNodeDefinition nodes[2][AH_MAX_VIRTUAL_NODES_PER_OUTPUT];
};

struct NodeAckPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t uniqueNodeId;
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t ackCode;
  uint8_t relatedType;
};

struct IdentifyStripPacket {
  uint8_t magic;
  uint8_t type;
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t sequence;
  uint8_t targetNodeId;
  uint8_t output;
  uint8_t virtualNode;
  uint8_t flags;
  uint8_t colorR;
  uint8_t colorG;
  uint8_t colorB;
};

struct NodeControlPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint8_t command;
  uint8_t requestId;
};

struct OtaBeginPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint16_t updateId;
  uint32_t firmwareSize;
  uint32_t firmwareCrc32;
  uint16_t chunkSize;
  char version[OTA_VERSION_TEXT_LEN];
};

struct OtaDataPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint16_t updateId;
  uint16_t chunkIndex;
  uint32_t offset;
  uint16_t dataLen;
  uint32_t chunkCrc32;
  uint8_t data[OTA_DATA_BYTES];
};

struct OtaEndPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint16_t updateId;
  uint32_t firmwareSize;
  uint32_t firmwareCrc32;
};

struct OtaAbortPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t targetNodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint16_t updateId;
  uint8_t reason;
};

struct OtaStatusPacket {
  uint8_t magic;
  uint8_t type;
  uint8_t nodeMac[6];
  union {
    uint16_t masterId;
    uint16_t sessionId;
  };
  uint16_t updateId;
  uint8_t status;
  uint16_t chunkIndex;
  uint32_t bytesReceived;
  uint8_t errorCode;
};

#pragma pack(pop)
