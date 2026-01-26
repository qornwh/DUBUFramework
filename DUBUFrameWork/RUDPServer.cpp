#include "RUDPServer.h"
#include "SocketConfig.h"
#include "BufferManager.h"
#include "Pool.h"
#include <iostream>

DUBU::RUDPServer::RUDPServer()
{
}

DUBU::RUDPServer::~RUDPServer()
{
}

void DUBU::RUDPServer::Start()
{
	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    assert(ret == 0);

    // io컴플리션 포트, 소켓 생성및 연결
    iocpHd_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    serverSocket_ = SocketConfig::CreateUDPSocket();
    SocketConfig::SetIoCompletionPort(serverSocket_, iocpHd_);

    // 소켓 설정
    SocketConfig::SetReuseAddress(serverSocket_, 1);

    // 바인딩
    SocketConfig::SocketBind(serverSocket_, port_);

    // 50개 미리 등록
    for (int i = 0; i < firstClientCount_; ++i)
    {
		RecvFrom();
    }
}

void DUBU::RUDPServer::Update()
{
}

void DUBU::RUDPServer::End()
{
	// 모든 연결된 iocp큐 제거
	const Set<DUBU::OverlappedPacketBuffer*>& useList = Singleton<PacketManager>::GetInstance().GetUseList();
	Uint64 EXIT_SIGNAL = 0xFFFFFFFFFFFFF;
	for (auto use : useList)
	{
		//CancelIoEx로 하면 GQCS가 false로 나오고 overlapped포인터는 잘나와서 post로 교체
		//Bool ret = CancelIoEx((HANDLE)serverSocket_, &use->overlapped_);
		Bool ret = PostQueuedCompletionStatus(iocpHd_, 0, EXIT_SIGNAL, &use->overlapped_);
		std::cout << ret << '\n';
	}

	DWORD dwTransferred = 0;
	ULONG_PTR completionKey = 0;
	LPOVERLAPPED lpOverlapped = nullptr;
	while (GetQueuedCompletionStatus(iocpHd_, &dwTransferred, &completionKey, &lpOverlapped, 10))
	{
		if (lpOverlapped) 
		{
			OverlappedPacketBuffer* ptr = reinterpret_cast<OverlappedPacketBuffer*>(lpOverlapped);

			Singleton<PacketManager>::GetInstance().PushPacketBuffer(ptr);
		}
	}

	int lastErr = GetLastError();
	if (lpOverlapped != nullptr) 
	{
		assert(-1);
	}

	closesocket(serverSocket_);
	CloseHandle(iocpHd_);
}

Bool DUBU::RUDPServer::RecvFrom()
{
	WSABUF wsabuf;
	DWORD bytesRecv = 0;
	DWORD flags = 0;

	OverlappedPacketBuffer* ptr = Singleton<PacketManager>::GetInstance().PopPacketBuffer();
	wsabuf.buf = static_cast<char*>(ptr->pos_);
	wsabuf.len = ptr->size_;
	OverlappedObj* ptr2 = static_cast<OverlappedObj*>(ptr);

	Int32 ret = WSARecvFrom(serverSocket_, &wsabuf, 1, &bytesRecv, &flags, (SOCKADDR*)&ptr2->remoteAddr_, &ptr2->addrSize_, &ptr2->overlapped_, nullptr);
	if (ret == SOCKET_ERROR)
	{
		int errCode = WSAGetLastError();

		// WSA_IO_PENDING : [정상] 작업이 성공적으로 완료되었고, 나중에 완료가 표시된다.
		if (errCode != WSA_IO_PENDING)
		{
			assert(false); // 일단 크래시냄
		}
	}

	return false;
}

Bool DUBU::RUDPServer::SendTo()
{
	//WSASendTo()
	return false;
}

Uint32 DUBU::RUDPServer::CreateNumber()
{
    return Uint32();
}
