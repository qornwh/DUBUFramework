#pragma once
#include "pch.h"

class SocketConfig
{
public:
    static SOCKET CreateUDPSocket();

    static void SetIoCompletionPort(SOCKET socket, HANDLE iocpHd);
    static void SocketBind(SOCKET ServerSocket, Int32 port);
    static void SetReuseAddress(SOCKET socket, Int32 opt);
};

