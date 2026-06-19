#include "Config.h"

#include <fstream>
#include <limits>

namespace DUBU
{
	Int32 g_firstClientCount = FIRST_CLIENT_COUNT;
    Uint16 g_servicePort = SERVICE_PORT;
	Int32 g_defaultRttMs = DEFAULT_RTT_MS;
	Int32 g_defaultRttMsDelay = DEFAULT_RTT_MS_DELAY;
	Int32 g_defaultDisconnectTimeoutMs = DEFAULT_DISCONNECT_TIMEOUT_MS;
	Int32 g_defaultPingTimeoutMs = DEFAULT_PING_TIMEOUT_MS;

	template <typename T>
	static void SetNumberConfig(const json& config, const char* key, T& value)
	{
		if (config.contains(key) && !config[key].is_null())
		{
			if (config[key].is_number_integer())
			{
                // int64를 적용한 이유는 최대값 체크를 위한 1회 검증임.. (물론 unsinged는 고려안함 singed만.)
				Int64 configValue = config[key].get<Int64>();
				if (configValue >= static_cast<Int64>(std::numeric_limits<T>::min()) && configValue <= static_cast<Int64>(std::numeric_limits<T>::max()))
				{
					value = static_cast<T>(configValue);
				}
			}
		}
	}

	Bool LoadConfig(const String& path)
	{
		g_firstClientCount = FIRST_CLIENT_COUNT;
		g_servicePort = SERVICE_PORT;
		g_defaultRttMs = DEFAULT_RTT_MS;
		g_defaultRttMsDelay = DEFAULT_RTT_MS_DELAY;
		g_defaultDisconnectTimeoutMs = DEFAULT_DISCONNECT_TIMEOUT_MS;
		g_defaultPingTimeoutMs = DEFAULT_PING_TIMEOUT_MS;

		std::ifstream file(path);
		if (!file.is_open())
		{
			return false;
		}

		json config;
		file >> config;

		SetNumberConfig(config, "FIRST_CLIENT_COUNT", g_firstClientCount);
		SetNumberConfig(config, "SERVICE_PORT", g_servicePort);
		SetNumberConfig(config, "DEFAULT_RTT_MS", g_defaultRttMs);
		SetNumberConfig(config, "DEFAULT_RTT_MS_DELAY", g_defaultRttMsDelay);
		SetNumberConfig(config, "DEFAULT_DISCONNECT_TIMEOUT_MS", g_defaultDisconnectTimeoutMs);
		SetNumberConfig(config, "DEFAULT_PING_TIMEOUT_MS", g_defaultPingTimeoutMs);

		return true;
	}
}
