#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace DUBU
{
    extern Uint32 g_maxclientcount;
	extern Uint32 g_firstClientCount;
	extern Uint16 g_servicePort;
	extern Uint32 g_defaultRttMs;
	extern Uint32 g_defaultRttMsDelay;
	extern Uint32 g_defaultDisconnectTimeoutMs;
	extern Uint32 g_defaultPingTimeoutMs;
    extern Uint32 g_channelMask;

	Bool LoadConfig(const String& path);
}

