#include "SocketConfig.h"

SOCKET SocketConfig::CreateUDPSocket()
{
    return WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
}

void SocketConfig::SetIoCompletionPort(SOCKET socket, HANDLE iocpHd)
{
    HANDLE socketIocp = CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), iocpHd, (u_long)0, 0);
    if (socketIocp == nullptr)
    {
        closesocket(socket);
        WSACleanup();
        assert(-1);
    }
}

void SocketConfig::SocketBind(SOCKET ServerSocket, Int32 port)
{
    SOCKADDR_IN socketAddr;
    socketAddr.sin_family = AF_INET;
    socketAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddr.sin_port = htons(static_cast<u_short>(port));

    if (bind(ServerSocket, reinterpret_cast<const SOCKADDR*>(&socketAddr), sizeof(socketAddr)) == SOCKET_ERROR)
    {
        closesocket(ServerSocket);
        WSACleanup();
        assert(-1);
    }
}

void SocketConfig::SetReuseAddress(SOCKET socket, Int32 opt)
{
    // 서버소켓에서 서버 시작하고 종료할때, 포트 대기중인 경우 있음 이때, 바로 연결되도록 옵션 설정
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));
}
