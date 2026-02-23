#include "Server.h"
#include "RUDPSocket.h"
#include "Packet.h"
#include "BufferManager.h"
#include "spdlog/spdlog.h"

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

void DUBU::Server::OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	auto buffer = opbPtr->buffer_;
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	auto sessionId = header->sessionId_;
	auto flag = header->flags_;

	spdlog::logger("recvfrom !!!\n");

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
			if (session != nullptr)
			{
				result = session->RecvDispatch(buffer, size);
			}
			else
			{
				spdlog::warn("sessionId{} not found", sessionId);
			}
		}
	}

	// repeat 재전송
	if (result && (flag & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT)
	{
		auto remote = opbPtr->remoteAddr_;
		auto addSize = opbPtr->addrSize_;
		rudpSocket_->SendTo(remote, buffer, addSize);
	}
}

void DUBU::Server::OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Int32 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);
}

void DUBU::Server::CreateSession(const SOCKADDR_IN& addr)
{
	Session* session = sessionManager_.AddSession();
	session->SetSockAddr(addr);
}
