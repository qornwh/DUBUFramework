#pragma once

#ifndef FIRST
#define FIRST
#include <assert.h>
#include "Types.h"
// 순서 지켜야됨 max함수 flatbuffer랑 겹치는 현상 발생
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
#pragma comment(lib, "ws2_32")

#define MAX_CLIENT_COUNT 500
#define FIRST_CLIENT_COUNT 50
#define SERVICE_PORT 12345
#define PACKET_MAX_SIZE 500 
#endif