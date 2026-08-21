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
#define MAX_CLIENT_COUNT 1000
#define FIRST_CLIENT_COUNT 50
#define SERVICE_PORT 12345
#define PACKET_MAX_SIZE 1024
#define DEFAULT_RTT_MS 100 
#define DEFAULT_WINDOW_COUNT 64
#define NO_REPEAT_ACCEPT_RANGE 0x1fff
#define DEFAULT_RTT_MS_DELAY 20
#define DEFAULT_DISCONNECT_TIMEOUT_MS 30000
#define DEFAULT_PING_TIMEOUT_MS 3000
#define DEFAULT_CHANNEL_MASK 0b0011 // 0 ~ 3까지만 따둔다.
#define DEFAULT_CHUNCK_MAX_SIZE 16 // 최대 16개 패킷까지
#define DEFAULT_CHUNK_END_MASK 0b1000'0000'0000'0000 // 청크 마지막 마스킹
#endif

namespace DUBU
{
    // 공통 전역 함수
    void LogAsset(const char* str, bool ret);
    Uint64 GetCurrentTimeMs();
    Uint32 GetRelativeTimeMs();
}
