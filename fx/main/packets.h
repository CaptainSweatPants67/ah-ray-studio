#pragma once
#include <Arduino.h>
#include "ah_protocol.h"

static constexpr uint8_t RF_BLACKOUT = RUNF_BLACKOUT;
static constexpr uint8_t RF_RESET_PHASE = RUNF_RESET_PHASE;
static constexpr uint8_t RF_SAVE_DEFAULT = RUNF_SAVE_DEFAULT;

static_assert(sizeof(TimePacket) <= 250, "TimePacket too large");
static_assert(sizeof(RunPacket) <= 250, "RunPacket too large");
static_assert(sizeof(DiscoveryStartPacket) <= 250, "DiscoveryStartPacket too large");
static_assert(sizeof(DiscoveryReplyPacket) <= 250, "DiscoveryReplyPacket too large");
static_assert(sizeof(DiscoveryAckPacket) <= 250, "DiscoveryAckPacket too large");
static_assert(sizeof(HelloNodePacket) <= 250, "HelloNodePacket too large");
static_assert(sizeof(ConfigRequestPacket) <= 250, "ConfigRequestPacket too large");
static_assert(sizeof(NodeStatusPacket) <= 250, "NodeStatusPacket too large");
static_assert(sizeof(NodeOverridePacket) <= 250, "NodeOverridePacket too large");
static_assert(sizeof(OutputOverridePacket) <= 250, "OutputOverridePacket too large");
static_assert(sizeof(VirtualNodeConfigPacket) <= 250, "VirtualNodeConfigPacket too large");
static_assert(sizeof(VirtualNodeStatusPacket) <= 250, "VirtualNodeStatusPacket too large");
static_assert(sizeof(NodeAckPacket) <= 250, "NodeAckPacket too large");
static_assert(sizeof(IdentifyStripPacket) <= 250, "IdentifyStripPacket too large");
static_assert(sizeof(OtaBeginPacket) <= 250, "OtaBeginPacket too large");
static_assert(sizeof(OtaDataPacket) <= 250, "OtaDataPacket too large");
static_assert(sizeof(OtaEndPacket) <= 250, "OtaEndPacket too large");
static_assert(sizeof(OtaAbortPacket) <= 250, "OtaAbortPacket too large");
static_assert(sizeof(OtaStatusPacket) <= 250, "OtaStatusPacket too large");
