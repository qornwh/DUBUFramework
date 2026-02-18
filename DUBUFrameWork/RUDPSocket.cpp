#include "RUDPSocket.h"
#include "SocketConfig.h"
#include "BufferManager.h"
#include "Pool.h"
#include "Packet.h"
#include <iostream>

DUBU::RUDPSocket::RUDPSocket()
{
}

DUBU::RUDPSocket::~RUDPSocket()
{
	EndServer();
}

void DUBU::RUDPSocket::StartServer()
{
	// 핸들러 등록 확인
	assert(handler_ == nullptr);

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

	// 서버 소켓 체크
	isServer_ = true;
}

void DUBU::RUDPSocket::EndServer()
{
	WriteLockGuard wl(lk_);
	if (!isServer_)
		return;

	isServer_ = false;
	// 모든 연결된 iocp큐 제거
	const Set<DUBU::OverlappedPacketBuffer*>& useList = Singleton<PacketManager>::GetInstance().GetUseList();
	Uint64 EXIT_SIGNAL = 0xFFFFFFFFFFFFF;
	for (auto use : useList)
	{
		//CancelIoEx로 하면 GQCS가 false로 나오고 overlapped포인터는 잘나와서 post로 교체
		//Bool ret = CancelIoEx((HANDLE)serverSocket_, &use->overlapped_);
		Bool ret = PostQueuedCompletionStatus(iocpHd_, 0, EXIT_SIGNAL, &use->overlapped_);
		assert(ret);
	}

	DWORD dwTransferred = 0;
	ULONG_PTR completionKey = 0;
	LPOVERLAPPED lpOverlapped = nullptr;
	while (GetQueuedCompletionStatus(iocpHd_, &dwTransferred, &completionKey, &lpOverlapped, 10))
	{
		if (lpOverlapped && completionKey == EXIT_SIGNAL)
		{
			OverlappedPacketBuffer* ptr = reinterpret_cast<OverlappedPacketBuffer*>(lpOverlapped);
			Singleton<PacketManager>::GetInstance().PushPacketBuffer(ptr);
		}
	}

	int lastErr = GetLastError();
	if (lpOverlapped != nullptr)
	{
		// 일단 크래시냄
		assert(-1);
	}

	closesocket(serverSocket_);
	CloseHandle(iocpHd_);
}

Int32 DUBU::RUDPSocket::Dispatch(LPOVERLAPPED* ptr, DWORD timeout)
{
	DWORD ipNumberOfBytesTransferred = 0;
	// 키등록 안함
	ULONG_PTR completionKey = 0;
	// 포인터의 포인터 => 포인터 변수도 복사다. 결국 위치 변경됨
	bool ret = GetQueuedCompletionStatus(iocpHd_, &ipNumberOfBytesTransferred, &completionKey, ptr, timeout);
	if (ret == false || ptr == nullptr)
		return -1;
	return (Int32)ipNumberOfBytesTransferred;
}

void DUBU::RUDPSocket::RecvFrom()
{
	WSABUF wsabuf;
	DWORD bytesRecv = 0;
	DWORD flags = 0;

	OverlappedPacketBuffer* opbPtr = Singleton<PacketManager>::GetInstance().PopPacketBuffer();
	wsabuf.buf = static_cast<char*>(opbPtr->pos_);
	wsabuf.len = opbPtr->size_;
	opbPtr->SetType(OverlappedObjType::RECVEFROM);

	Int32 ret = WSARecvFrom(serverSocket_, &wsabuf, 1, &bytesRecv, &flags, (SOCKADDR*)&opbPtr->remoteAddr_, &opbPtr->addrSize_, &opbPtr->overlapped_, nullptr);
	if (ret == SOCKET_ERROR)
	{
		int errCode = WSAGetLastError();
		if (errCode != WSA_IO_PENDING)
		{
			assert(false); // 일단 크래시냄
		}
	}
}

void DUBU::RUDPSocket::SendTo(const SOCKADDR_IN& targetAddr, Byte* buffer, Int32 size)
{
	WSABUF wsabuf;
	DWORD flags = 0;
	DWORD bytesSent = size;

	OverlappedPacketBuffer* opbPtr = PacketManager::GetInstance().PopPacketBuffer();
	opbPtr->size_ = size;
	opbPtr->pos_ = buffer;
	opbPtr->SetType(OverlappedObjType::SENDTO);
	wsabuf.buf = static_cast<char*>(opbPtr->pos_);
	wsabuf.len = opbPtr->size_;
	Int32 ret = WSASendTo(serverSocket_, &wsabuf, 1, &bytesSent, flags, (SOCKADDR*)&targetAddr, sizeof(SOCKADDR_IN), &opbPtr->overlapped_, NULL);
	if (ret == SOCKET_ERROR)
	{
		int errCode = WSAGetLastError();
		if (errCode != WSA_IO_PENDING)
		{
			assert(false); // 일단 크래시냄
		}
	}
}

void DUBU::RUDPSocket::RecvFromComplete(OVERLAPPED* ptr, Int32 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	if (Packet::Packet::PacketHeaderCheck(static_cast<Uint8*>(opbPtr->buffer_), size))
	{
		Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opbPtr->buffer_);
		if (Packet::Packet::CRC32(opbPtr->buffer_, size) == header->checksum_)
		{
			// 핸들러 통해서 처리 넘겨버린다
			if (handler_)
			{
				handler_->OnRecvFrom(opbPtr->remoteAddr_, reinterpret_cast<Byte*>(opbPtr), size);
			}
		}
		else
		{
			// 체크썸 깨짐
		}
	}
	else
	{
		// 헤더 깨짐
	}

	// 반환, 다시 재등록
	PacketManager::GetInstance().PushPacketBuffer(opbPtr);
	RecvFrom();
}

void DUBU::RUDPSocket::SendToComplete(OVERLAPPED* ptr, Int32 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	// 핸들러 통해서 처리 넘겨버린다
	if (handler_)
	{
		handler_->OnSendTo(opbPtr->remoteAddr_, reinterpret_cast<Byte*>(opbPtr), size);
	}

	// 재전송 패킷이 아니면 할당 해제 
	if ((opbPtr->type_ & OverlappedObjType::RELIABLE) != OverlappedObjType::RELIABLE)
	{
		PacketManager::GetInstance().PushPacketBuffer(opbPtr);
	}
}
