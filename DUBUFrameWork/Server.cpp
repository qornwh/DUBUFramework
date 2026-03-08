#include "Server.h"
#include "RUDPSocket.h"
#include "Packet.h"
#include "BufferManager.h"

DUBU::Server::Server() : isRunning_(false), sessionManager_(SessionManager{})
{
	rudpSocket_ = std::make_shared<RUDPSocket>();
}

DUBU::Server::~Server()
{
	rudpSocket_->EndServer();
	PacketManager::GetInstance().Release();
}

void DUBU::Server::Initialize()
{
	rudpSocket_->StartServer();
	isRunning_ = true;
}

void DUBU::Server::Run()
{
	while (isRunning_)
	{
		assert(rudpSocket_ != nullptr);
		LPOVERLAPPED ptr = nullptr;
		Int32 size = rudpSocket_->Dispatch(&ptr);

		// 이때 다른 작업도 괞찮아 보임
		if (ptr == nullptr)
			continue;

		OverlappedObj* ptr2 = reinterpret_cast<OverlappedObj*>(ptr);
		if ((ptr2->type_ & OverlappedObjType::RECVEFROM) == OverlappedObjType::RECVEFROM)
			rudpSocket_->RecvFromComplete(ptr, size);
		else if ((ptr2->type_ & OverlappedObjType::SENDTO) == OverlappedObjType::SENDTO)
			rudpSocket_->SendToComplete(ptr, size);
	}
}

void DUBU::Server::OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	auto buffer = opbPtr->buffer_;
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	auto sessionId = header->sessionId_;
	auto flag = header->flags_;

	bool result = false;
	if (flag == Packet::PacketHeaderFlag::SESSION)
	{
		// 세션 추가
		CreateSession(addr);
	}
	else
	{
		// 세션이 있을때
		if (sessionId > 0)
		{
			Session* session = sessionManager_.GetSession(sessionId);
			if (session == nullptr)
			{
				spdlog::warn("sessionId{} not found", sessionId);
			}

			// NONE or REPEAT 일때는 패킷에 대한 내용을 처리
			if ((flag & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT || flag == Packet::PacketHeaderFlag::NONE)
			{
				result = session->RecvDispatch(buffer, size);
			}

			// ACK 일때는 클라쪽에서 제대로 받고 다시 보내왔다는 것이다.
			if ((flag & Packet::PacketHeaderFlag::ACK) == Packet::PacketHeaderFlag::ACK)
			{
				result = session->RecvDispatchACK(buffer, size);
			}
		}
	}

	// repeat 재전송
	if (result && (flag & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT)
	{
		auto remote = opbPtr->remoteAddr_;
		auto addSize = opbPtr->size_;
		rudpSocket_->SendTo(remote, buffer, addSize);
	}

	// 일단 실패할때 코드 및 대시보드에 띄울 데이터 큐에 넣기?
}

void DUBU::Server::OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);
}

void DUBU::Server::CreateSession(const SOCKADDR_IN& addr)
{
	Session* session = sessionManager_.AddSession();
	session->SetSockAddr(addr);
}
