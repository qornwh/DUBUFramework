#include "SocketConfig.h"

SOCKET DUBU::SocketConfig::CreateUDPSocket()
{
    // SIO_UDP_CONNRESET 옵션 추가, 클라이언트가 바인딩 안된 상태에서 iocp recv 등록시 문제가 있엇음
    SOCKET socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
    BOOL bNewBehavior = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
    return socket;
}

void DUBU::SocketConfig::SetIoCompletionPort(SOCKET socket, HANDLE iocpHd)
{
    HANDLE socketIocp = CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), iocpHd, (u_long)0, 0);
    if (socketIocp == nullptr)
    {
        closesocket(socket);
        WSACleanup();
        assert(-1);
    }
}

void DUBU::SocketConfig::SocketBind(SOCKET ServerSocket, Int32 port)
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

void DUBU::SocketConfig::SetReuseAddress(SOCKET socket, Int32 opt)
{
    // 서버소켓에서 서버 시작하고 종료할때, 포트 대기중인 경우 있음 이때, 바로 연결되도록 옵션 설정
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));
}
