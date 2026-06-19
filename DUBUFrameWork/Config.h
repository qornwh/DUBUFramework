#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace DUBU
{
	extern Int32 g_firstClientCount;
	extern Uint16 g_servicePort;
	extern Int32 g_defaultRttMs;
	extern Int32 g_defaultRttMsDelay;
	extern Int32 g_defaultDisconnectTimeoutMs;
	extern Int32 g_defaultPingTimeoutMs;

	Bool LoadConfig(const String& path);
}

