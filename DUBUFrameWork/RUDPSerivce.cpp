#include "RUDPSerivce.h"
#include "SocketConfig.h"

RUDPSerivce::RUDPSerivce()
{
}

RUDPSerivce::~RUDPSerivce()
{
}

void RUDPSerivce::Start()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

    // io컴플리션 포트, 소켓 생성및 연결
    iocpHd_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    serverSocket_ = SocketConfig::CreateUDPSocket();
    SocketConfig::SetIoCompletionPort(serverSocket_, iocpHd_);

    // 소켓 설정
    SocketConfig::SetReuseAddress(serverSocket_, 1);

    // 바인딩
    SocketConfig::SocketBind(serverSocket_, port_);

    // 미리 연결해 두기 WSARecvFrom
}

void RUDPSerivce::Update()
{
}
