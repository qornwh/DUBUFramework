#pragma once

#ifndef FIRST
#define FIRST
#include <assert.h>
#include "Types.h"
#include "spdlog/spdlog.h"
// 순서 지켜야됨 max함수 flatbuffer랑 겹치는 현상 발생
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
#include <coroutine>
#pragma comment(lib, "ws2_32")

// 상수 모아둔다.
#define MAX_CLIENT_COUNT 500
#define FIRST_CLIENT_COUNT 50
#define SERVICE_PORT 12345
#define PACKET_MAX_SIZE 500 
#define DEFAULT_RTT_MS 100
#endif

namespace DUBU
{
	static void LogAsset(const char* str, bool ret)
	{
		if (!ret)
		{
			printf("%s", str);
			assert(ret);
		}
	}

	static long long GetCurrentTimeMs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}
