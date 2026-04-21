#include "Server.h"
#include "RUDPSocket.h"
#include "Packet.h"
#include "BufferManager.h"

DUBU::Server::Server() : isRunning_(false), sessionManager_(SessionManager{}), sessionCAS_(false)
{
	rudpSocket_ = std::make_shared<RUDPSocket>();
	rudpSocket_->SetHandler(this);
}

DUBU::Server::~Server()
{
	rudpSocket_->EndServer();
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

        if (ptr == nullptr)
        {
            // 마지막 세션 체크
            CheckSession();
			continue;
        }

		OverlappedObj* ptr2 = reinterpret_cast<OverlappedObj*>(ptr);
		if ((ptr2->type_ & OverlappedObjType::RECVEFROM) == OverlappedObjType::RECVEFROM)
			rudpSocket_->RecvFromComplete(ptr, size);
		else if ((ptr2->type_ & OverlappedObjType::SENDTO) == OverlappedObjType::SENDTO)
			rudpSocket_->SendToComplete(ptr, size);
	}
}

void DUBU::Server::Stop()
{
	isRunning_ = false;
}

void DUBU::Server::OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opb = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	auto buffer = opb->buffer_;
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	auto sessionId = header->sessionId_;
	auto flag = header->flags_;
    auto seqNo = header->sequenceNo_;
	Uint64 key = PeerKey(addr);
	auto it = peerMap_.find(key);
	Bool result = false;

	if (flag == Packet::PacketHeaderFlag::SESSION)
	{
		if (it == peerMap_.end())
		{
			// 세션 추가
			WriteLockGuard wl(sessionLock_);
			Session* session = CreateSession(addr);
			header->sessionId_ = session->GetSessionId();
			// 세션 추가 완료 응답
			ConnectMessage(session);
			// Peer 생성
			peerMap_.emplace(key, Peer{key, addr, session});
		}
		else
		{
			// 중복으로 오는경우 재전송
			ReadLockGuard rl(sessionLock_);
			Session* session = it->second.session_;
			ConnectMessage(session);
		}
	}
	else
	{
		/*
		* 세션이 있을때
		* - discconect ACK는 연결 해제를 확인.
		*/ 
		if (sessionId > 0)
		{
			ReadLockGuard rl(sessionLock_);
			Session* session = sessionManager_.GetSession(sessionId);
			if (session == nullptr)
			{
                if (flag == Packet::PacketHeaderFlag::DISCONNECT)
                {
                    // 이미 서버에서 연결 끊음
                    spdlog::info("Already Disconnect Session");
                    return;
                }
				spdlog::error("SessionId{} not found", sessionId);
				return;
			}

			// PING/PONG 비트는 ACK 비트를 포함하므로 equality로 먼저 분기해야 함
			if (flag == Packet::PacketHeaderFlag::PONG)
			{
				result = session->RecvDispatchPong(buffer, size);
			}
			else if (flag == Packet::PacketHeaderFlag::PING)
			{
				// 서버는 PING을 받지 않는 설계 — 무시
				spdlog::debug("Server received PING (ignored) from session {}", sessionId);
			}
            else if (flag == Packet::PacketHeaderFlag::NONE)
            {
                // NONE 일때는 패킷을 정상 수신하여 처리 ACK 안보냄
                result = session->RecvDispatch(buffer, size);
            }
            else if ((flag & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT)
            {
                // ACK 일때는 클라쪽에서 결과를 받고 다시 보내왔다는 뜻이다.
                result = session->RecvDispatch(buffer, size);
                if (result)
                {
                    // ACK 전달
                    SendAck(seqNo, session);
                }
            }
            else if ((flag & Packet::PacketHeaderFlag::ACK) == Packet::PacketHeaderFlag::ACK)
            {
                // ACK 수신 pendingpacket 지움
                session->RecvDispatchACK(buffer, size);
            }
		}
	}

	// 일단 실패할때 코드 및 대시보드에 띄울 데이터 큐에 넣기?
}

void DUBU::Server::OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opb = reinterpret_cast<OverlappedPacketBuffer*>(ptr);
}

DUBU::Session* DUBU::Server::CreateSession(const SOCKADDR_IN& addr)
{
	Session* session = sessionManager_.AddSession();
	session->SetSockAddr(addr);
    session->SetSocket(rudpSocket_.get());
	session->SetTimestamp(DUBU::GetCurrentTimeMs());
	return session;
}

void DUBU::Server::ConnectMessage(Session* session)
{
	OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

	// 헤더 작성
	header->checksum_ = 0;
	header->flags_ = Packet::PacketHeaderFlag::SESSION;
	header->totalSize_ = sizeof(Packet::PacketHeader);
	header->sessionId_ = session->GetSessionId();
	header->sequenceNo_ = 0;
	header->timestamp_ = 0;

	// 전체 패킷 사이즈 설정
	opb->size_ = header->totalSize_;

	// crc32 암호화
	Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
	header->checksum_ = checksum;
	rudpSocket_->SendTo(session->GetSockAddr(), opb);
}

void DUBU::Server::DisconnectMessage(Session* session)
{
    // 그래도 클라한테 한번 넘겨줌
	OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

	// 헤더 작성
	header->checksum_ = 0;
	header->flags_ = Packet::PacketHeaderFlag::DISCONNECT;
	header->totalSize_ = sizeof(Packet::PacketHeader);
	header->sessionId_ = session->GetSessionId();
	header->sequenceNo_ = session->UpdateSendSequenceNo();
	header->timestamp_ = GetCurrentTimeMs();

	// 전체 패킷 사이즈 설정
	opb->size_ = header->totalSize_;

	// crc32 암호화
	Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
	header->checksum_ = checksum;
	rudpSocket_->SendTo(session->GetSockAddr(), opb);
}

void DUBU::Server::SendPing(Session* session)
{
	OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

	// 헤더 작성 (header-only, 비신뢰)
	header->checksum_ = 0;
	header->flags_ = Packet::PacketHeaderFlag::PING;
	header->totalSize_ = sizeof(Packet::PacketHeader);
	header->sessionId_ = session->GetSessionId();
	// 비신뢰 — 핑퐁 전용 시퀀스No사용
	header->sequenceNo_ = session->AccSequnceNo();
	header->timestamp_ = GetCurrentTimeMs();
	header->packetCode_ = 0;

	opb->size_ = header->totalSize_;

	Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
	header->checksum_ = checksum;
	rudpSocket_->SendTo(session->GetSockAddr(), opb);

	// AddPendingPacket 호출하지 않음 — 재전송/ACK 추적 없음
    session->AddPingCount();
	spdlog::info("PING session {} : {}", session->GetSessionId(), header->sequenceNo_);
}

void DUBU::Server::CheckSession()
{
	// 시간체크
	Uint64 now = GetCurrentTimeMs();
	// 끊을 세션
	Uint8 idx = 0;

	// 1스레드만 통과
	Bool expected = sessionCAS_.load();
	// false, 연산 실패시 통과
	if (expected || !sessionCAS_.compare_exchange_weak(expected, true, std::memory_order_acquire, std::memory_order_relaxed))
	{
		return;
	}

	{
		// 재전송로직 실행
		ReadLockGuard rw(sessionLock_);
		for (auto& [_, session] : sessionManager_.GetSessions())
		{
            if (!session->IsConnection())
            {
                continue;
            }

			Uint64 delayTime = now - session->GetTimestamp();
			if (delayTime > SessionTimeout)
			{
				// 끊김 감지
				removeListCache_[idx++] = session->GetSessionId();
			}
			else if (delayTime > PingTimeout)
			{
				// PingTimeout 간격으로 throttle — 3s 시점부터 3s마다 최대 ~3회
				if (now - session->GetLastPingSentTime() >= (Uint64)PingTimeout)
				{
					SendPing(session);
					session->SetLastPingSentTime(now);
				}
			}

			// 재전송, 왕복시간은 * 2 + DEFAULT_RTT_MS_DELAY
			session->RepeatMessage(rudpSocket_.get(), session->GetRttMillisec() * 2 + DEFAULT_RTT_MS_DELAY);
		}
	}

	{
		// 연결 해제 구간
		WriteLockGuard rw(sessionLock_);
		for (Int32 i = 0; i < idx; ++i)
		{
			Int32 sessionId = removeListCache_[i];
			Session* session = sessionManager_.GetSession(sessionId);
			if (session != nullptr)
			{
				// 세션 연결 해제
                DisconnectMessage(session);
				session->Disconnect();
			}
            sessionManager_.RemoveSession(sessionId);
		}
	}

	sessionCAS_.store(false);
}

void DUBU::Server::SendAck(Uint32 seqNo, Session* session)
{
    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

    // 헤더 작성 (header-only, 비신뢰)
    header->checksum_ = 0;
    header->flags_ = Packet::PacketHeaderFlag::ACK;
    header->totalSize_ = sizeof(Packet::PacketHeader);
    header->sessionId_ = session->GetSessionId();
    header->sequenceNo_ = seqNo;
    header->timestamp_ = GetCurrentTimeMs();
    header->packetCode_ = 0;

    opb->size_ = header->totalSize_;

    Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
    header->checksum_ = checksum;
    rudpSocket_->SendTo(session->GetSockAddr(), opb);
}

Uint64 DUBU::Server::PeerKey(const SOCKADDR_IN& addr)
{
	// ip를 16비트 시프트후 port와 or연산으로 key관리
	return ((Uint64)addr.sin_addr.s_addr << 16) | (Uint64)ntohs(addr.sin_port);
}
