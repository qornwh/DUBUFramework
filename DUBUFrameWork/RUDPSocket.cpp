#include "RUDPSocket.h"
#include "SocketConfig.h"
#include "BufferManager.h"
#include "Pool.h"
#include "Packet.h"
#include <iostream>
#include "spdlog/spdlog.h"

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
	assert(handler_ != nullptr);

	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	assert(ret == 0);

	// io컴플리션 포트, 소켓 생성및 연결
	iocpHd_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	socket_ = SocketConfig::CreateUDPSocket();
	SocketConfig::SetIoCompletionPort(socket_, iocpHd_);

	// 소켓 설정
	SocketConfig::SetReuseAddress(socket_, 1);

	// 바인딩
	SocketConfig::SocketBind(socket_, port_);

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

	// 패킷매니저의 모든 패킷 버퍼에 연결된 iocp큐 제거
	const Set<DUBU::OverlappedPacketBuffer*>& useList = Singleton<PacketManager>::GetInstance().GetUseList();
	Uint64 EXIT_SIGNAL = 0xFFFFFFFFFFFFF;
	for (auto use : useList)
	{
		// 할당만 되고 현재 사용중이것들 PostQueuedCompletionStatus로 처리
		Bool ret = PostQueuedCompletionStatus(iocpHd_, 0, EXIT_SIGNAL, &use->overlapped_);
		assert(ret);
	}

	// 소켓에 걸린 모든 비동기 I/O 취소
	CancelIoEx((HANDLE)socket_, NULL);

	// 모든 비동기 I/O 취소 
	while (true)
	{
		DWORD dwTransferred = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED lpOverlapped = nullptr;
		BOOL ret = GetQueuedCompletionStatus(iocpHd_, &dwTransferred, &completionKey, &lpOverlapped, 10);

		if (ret == FALSE && lpOverlapped == nullptr)
		{
			// 더 이상 없음
			break;
		}

		// ret가 false로 나와도 허용해야 된다(취소, 소켓 닫힘 등) 
		// 즉 현재 사용되고 있었는데, 소켓이 닫힘으로써 false가 나올수 있다.
		// 정상동작 완료
		if (lpOverlapped != nullptr)
		{
			OverlappedPacketBuffer* ptr = reinterpret_cast<OverlappedPacketBuffer*>(lpOverlapped);
			PacketManager::GetInstance().PushPacketBuffer(ptr);
		}
	}

	// 소켓 반환
	Closesocket();
	CloseHandle(iocpHd_);
}

void DUBU::RUDPSocket::StartClient(const String& serverIP, Uint16 serverPort)
{
	// 핸들러 등록 확인
	assert(handler_ != nullptr);

	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	assert(ret == 0);

	// 핸들설정
	iocpHd_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	socket_ = SocketConfig::CreateUDPSocket();
	SocketConfig::SetIoCompletionPort(socket_, iocpHd_);

	// 클라이언트 바인드
	SocketConfig::SocketBind(socket_, 0);
	
	// 서버 ip, port 설정
	serverIP_ = serverIP;
	serverPort_ = serverPort;
 
	// 서버 주소설정
	memset(&serverAddr_, 0, sizeof(serverAddr_));
	serverAddr_.sin_family = AF_INET;
	serverAddr_.sin_port = htons(serverPort_);
	if (inet_pton(AF_INET, serverIP_.c_str(), &serverAddr_.sin_addr) <= 0) 
	{
		assert(false);
	}
}

void DUBU::RUDPSocket::EndClient()
{
	WriteLockGuard wl(lk_);

	// 소켓에 걸린 모든 비동기 I/O 취소
	CancelIoEx((HANDLE)socket_, NULL);

	// 모든 비동기 I/O 취소 
	while (true)
	{
		DWORD dwTransferred = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED lpOverlapped = nullptr;
		BOOL ret = GetQueuedCompletionStatus(iocpHd_, &dwTransferred, &completionKey, &lpOverlapped, 10);

		if (ret == FALSE && lpOverlapped == nullptr)
		{
			// 더 이상 없음
			break;
		}

		// ret가 false로 나와도 허용해야 된다(취소, 소켓 닫힘 등) 
		// 즉 현재 사용되고 있었는데, 소켓이 닫힘으로써 false가 나올수 있다.
		// 정상동작 완료
		if (lpOverlapped != nullptr)
		{
			OverlappedPacketBuffer* ptr = reinterpret_cast<OverlappedPacketBuffer*>(lpOverlapped);
			PacketManager::GetInstance().PushPacketBuffer(ptr);
		}
	}

	// 소켓 반환
	Closesocket();
	CloseHandle(iocpHd_);
}

void DUBU::RUDPSocket::SetSocketAddr(SOCKADDR_IN& serverAddr)
{
	serverAddr_ = serverAddr;
}

SOCKADDR_IN& DUBU::RUDPSocket::GetSockAddr()
{
	return serverAddr_;
}

void DUBU::RUDPSocket::Closesocket()
{
	if (socket_ != INVALID_SOCKET)
	{
		closesocket(socket_);
		socket_ = INVALID_SOCKET;
	}
}

Int32 DUBU::RUDPSocket::Dispatch(LPOVERLAPPED* ptr, DWORD timeout)
{
	DWORD bytesTransferred = 0;
	ULONG_PTR completionKey = 0;
	bool ret = GetQueuedCompletionStatus(iocpHd_, &bytesTransferred, &completionKey, ptr, timeout);
	if (ret == false || ptr == nullptr)
		return -1;
	return (Int32)bytesTransferred;
}

void DUBU::RUDPSocket::SetHandler(ISocketHandler* handler)
{
	handler_ = handler;
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

	Int32 ret = WSARecvFrom(socket_, &wsabuf, 1, &bytesRecv, &flags, (SOCKADDR*)&opbPtr->remoteAddr_, & opbPtr->addrSize_, &opbPtr->overlapped_, nullptr);
	if (ret == SOCKET_ERROR)
	{
		int errCode = WSAGetLastError();
		if (errCode != WSA_IO_PENDING)
		{
			spdlog::error("WSARecvFrom Register Error : {}", errCode);
			assert(false);
		}
	}
}

void DUBU::RUDPSocket::SendTo(const SOCKADDR_IN& targetAddr, Uint8* buffer, Uint16 size)
{
	// 일반 패킷
	WSABUF wsabuf;
	DWORD flags = 0;
	DWORD bytesSent = size;

	OverlappedPacketBuffer* opbPtr = PacketManager::GetInstance().PopPacketBuffer();
	opbPtr->SetType(OverlappedObjType::SENDTO);

	// 패킷 복사
	Packet::Packet::PacketHeaderCopy(buffer, opbPtr->buffer_);
	Packet::Packet::PacketCopy(buffer, sizeof(Packet::PacketHeader), opbPtr->buffer_ + sizeof(Packet::PacketHeader));
	opbPtr->size_ = size;
	opbPtr->pos_ = opbPtr->buffer_;

	wsabuf.buf = static_cast<char*>(opbPtr->pos_);
	wsabuf.len = opbPtr->size_;
	Int32 ret = WSASendTo(socket_, &wsabuf, 1, &bytesSent, flags, (SOCKADDR*)&targetAddr, sizeof(SOCKADDR_IN), &opbPtr->overlapped_, NULL);
	if (ret == SOCKET_ERROR)
	{
		int errCode = WSAGetLastError();
		if (errCode != WSA_IO_PENDING)
		{
			spdlog::error("WSASendTo SendTo Error : {}", errCode);
			assert(false); // 일단 크래시냄
		}
	}
}

void DUBU::RUDPSocket::SendToReliable(const SOCKADDR_IN& targetAddr, Uint8* buffer, Uint16 size)
{
	// 신뢰성 패킷 전송
	WSABUF wsabuf;
	DWORD flags = 0;
	DWORD bytesSent = size;

	OverlappedPacketBuffer* opbPtr = PacketManager::GetInstance().PopPacketBuffer();
	opbPtr->SetType(OverlappedObjType::SENDTO | OverlappedObjType::RELIABLE);

	// 패킷 복사
	Packet::Packet::PacketHeaderCopy(buffer, opbPtr->buffer_);
	Packet::Packet::PacketCopy(buffer, sizeof(Packet::PacketHeader), opbPtr->buffer_ + sizeof(Packet::PacketHeader));
	opbPtr->size_ = size;
	opbPtr->pos_ = opbPtr->buffer_;

	wsabuf.buf = static_cast<char*>(opbPtr->pos_);
	wsabuf.len = opbPtr->size_;
	Int32 ret = WSASendTo(socket_, &wsabuf, 1, &bytesSent, flags, (SOCKADDR*)&targetAddr, sizeof(SOCKADDR_IN), &opbPtr->overlapped_, NULL);
	if (ret == SOCKET_ERROR)
	{
		int errCode = WSAGetLastError();
		if (errCode != WSA_IO_PENDING)
		{
			spdlog::error("WSASendTo SendToReliable Error : {}", errCode);
			assert(false); // 일단 크래시냄
		}
	}
}

void DUBU::RUDPSocket::SendToRepeat(const SOCKADDR_IN& targetAddr, Uint8* buffer, Uint16 size)
{
	// 재전송 패킷
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(buffer);

	// overlapped 재사용 전 초기화 (ZeroMemory)
	opbPtr->Initialize();

	// 패킷 복사는 없다 기존꺼 다시 보내는 함수
	WSABUF wsabuf;
	wsabuf.len = size;
	wsabuf.buf = static_cast<char*>(opbPtr->pos_);
	DWORD flags = 0;
	DWORD bytesSent = size;

	Int32 ret = WSASendTo(socket_, &wsabuf, 1, &bytesSent, flags, (SOCKADDR*)&targetAddr, sizeof(SOCKADDR_IN), &opbPtr->overlapped_, NULL);
	if (ret == SOCKET_ERROR)
	{
		int errCode = WSAGetLastError();
		if (errCode != WSA_IO_PENDING)
		{
			spdlog::error("WSASendTo SendToRepeat Error : {}", errCode);
			assert(false); // 일단 크래시냄
		}
	}
}

void DUBU::RUDPSocket::RecvFromComplete(OVERLAPPED* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	if (Packet::Packet::PacketHeaderCheck(static_cast<Uint8*>(opbPtr->buffer_), size))
	{
		Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opbPtr->buffer_);
		Uint32 checksum = header->checksum_;

		// 보낼때 체크썸은 0으로 초기화 된 상태라 롤백
		header->checksum_ = 0;
		if (checksum == Packet::Packet::CRC32(opbPtr->buffer_, size))
		{
			// 핸들러 통해서 처리 넘겨버린다
			if (handler_)
			{
				handler_->OnRecvFrom(opbPtr->remoteAddr_, reinterpret_cast<Uint8*>(opbPtr), size);
			}
		}
		else
		{
			// 체크썸 깨짐
			spdlog::warn("RecvFromComplete: checksum failed");
		}
	}
	else
	{
		// 헤더 깨짐
		spdlog::warn("RecvFromComplete: header failed");
	}

	// 반환, 다시 재등록
	PacketManager::GetInstance().PushPacketBuffer(opbPtr);
	RecvFrom();
}

void DUBU::RUDPSocket::SendToComplete(OVERLAPPED* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	// 핸들러 통해서 처리 넘겨버린다
	if (handler_)
	{
		handler_->OnSendTo(opbPtr->remoteAddr_, reinterpret_cast<Uint8*>(opbPtr), size);
	}

	// 재전송 패킷이 아니면 할당 해제 
	if ((opbPtr->type_ & OverlappedObjType::RELIABLE) != OverlappedObjType::RELIABLE)
	{
		PacketManager::GetInstance().PushPacketBuffer(opbPtr);
	}
}
