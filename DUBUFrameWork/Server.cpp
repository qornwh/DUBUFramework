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
			ReadLockGuard rl(lk_);
			Session* session = sessionManager_.GetSession(sessionId);
			if (session == nullptr)
			{
				spdlog::error("sessionId{} not found", sessionId);
				return;
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

	// repeat ACK전송
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
	WriteLockGuard wl(lk_);
	Session* session = sessionManager_.AddSession();
	session->SetSockAddr(addr);
}

void DUBU::Server::CheckSession()
{
	// 재전송로직 실행
	Uint64 now = GetCurrentTimeMs();

	{
		// 끊을 세션
		Uint8 idx = 0;
		// 읽기만 묶는다.
		ReadLockGuard rl(lk_);
		for (auto& [_, session] : sessionManager_.GetSessions())
		{
			// 끊김 감지
			if (now - session->GetTimestamp() > SessionTimeout) 
			{
				removeListCache_[idx++] = session->GetSessionId();
			}

			// 재전송
			for (auto& [time, seqNo, opb] : session->GetPendingQueue())
			{
				if (now - opb->sentAt_ > session->GetRttMillisec() * 2)
				{
					rudpSocket_->SendToRepeat(session->GetSockAddr(), opb->buffer_, opb->size_);
				}
				else
				{
					break;
				}
			}
		}
	}
	

}
