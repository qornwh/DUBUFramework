#include "Server.h"
#include "RUDPSocket.h"
#include "Packet.h"
#include "BufferManager.h"
#include "ConnectionType.h"
#include "ThreadManager.h"

DUBU::Server* g_server = nullptr;

DUBU::Server::Server() : isRunning_(false), sessionManager_(SessionManager{})
{
	rudpSocket_ = std::make_shared<RUDPSocket>();
	rudpSocket_->SetHandler(this);
    g_server = this;
}

DUBU::Server::~Server()
{
    Stop();

    // 서버 종료전, 남은 세션 전부 반환
    for (auto& [id, session] : sessionManager_.GetSessions())
    {
        if (session != nullptr)
        {
            DestroySession(session);
        }
    }

	rudpSocket_->EndServer();
}

void DUBU::Server::Initialize(const Map<Uint8, Packet::PacketHandler>* handlers)
{
    sessionManager_.SetHandlers(handlers);

    rudpSocket_->StartServer();
	isRunning_.store(true);

#ifdef TEST_MODE
    lastStatsTickMs_.store(GetRelativeTimeMs());
#endif
}

void DUBU::Server::Run()
{
	while (isRunning_.load())
	{
		assert(rudpSocket_ != nullptr);
        Dispatch();
	}
}

void DUBU::Server::Dispatch()
{
	LPOVERLAPPED ptr = nullptr;
	Int32 size = rudpSocket_->Dispatch(&ptr, 1);

	if (ptr != nullptr)
	{
		OverlappedObj* obj = reinterpret_cast<OverlappedObj*>(ptr);
		if ((obj->type_ & OverlappedObjType::RECVEFROM) == OverlappedObjType::RECVEFROM)
		{
			rudpSocket_->RecvFromComplete(ptr, size);
#ifdef TEST_MODE
			recvPacketCount_.fetch_add(1);
#endif
		}
		else if ((obj->type_ & OverlappedObjType::SENDTO) == OverlappedObjType::SENDTO)
		{
			rudpSocket_->SendToComplete(ptr, static_cast<Uint16>(size));
#ifdef TEST_MODE
			sendPacketCount_.fetch_add(1);
#endif
		}
	}

#ifdef TEST_MODE
    Uint32 now = GetRelativeTimeMs();
    Uint32 last = lastStatsTickMs_.load();
    // 수신 스레드 여러 개 중 CAS 성공한 한 스레드만 통계 출력
    if (now - last >= logTimeOut_ && lastStatsTickMs_.compare_exchange_strong(last, now))
    {
        Uint32 activeCount = 0;
        Uint32 rttSum = 0;
        Uint32 rttMax = 0;

        {
            ReadLockGuard rl(sessionLock_);
            for (auto& [sessionId, session] : sessionManager_.GetSessions())
            {
                if (session != nullptr)
                {
                    Uint32 rtt = session->GetRttMillisec();
                    if (rtt > rttMax)
                    {
                        rttMax = rtt;
                    }
                    rttSum += rtt;
                    activeCount++;
                }
            }
        }

        float rttAvg = 0.0f;
        if (activeCount > 0)
        {
            rttAvg = static_cast<float>(rttSum) / activeCount;
        }
        PrintStats(activeCount, rttAvg, rttMax);
    }
#endif
}

void DUBU::Server::Stop()
{
    if (!isRunning_.exchange(false))
        return;

    // 세션 워커 정지
    for (auto& worker : g_sessionWorkers)
    {
        worker.Stop();
    }
}

void DUBU::Server::OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
	// 현재 처리된곳에서 버퍼를 반환하는것으로 변경됨
	OverlappedPacketBuffer* opb = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	if (g_sessionWorkers.empty())
	{
		spdlog::warn("OnRecvFrom: no session workers, packet dropped");
		PacketManager::GetInstance().PushPacketBuffer(opb);
		return;
	}

	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);
	Uint32 sessionId = header->sessionId_;
	const Uint32 workerCount = static_cast<Uint32>(g_sessionWorkers.size());

	if (header->totalSize_ != size)
	{
		PacketManager::GetInstance().PushPacketBuffer(opb);
		return;
	}

	Uint32 target = static_cast<Uint32>(PeerKey(addr) % workerCount);
	g_sessionWorkers[target].Push(SessionJob{ sessionId, opb });
}

void DUBU::Server::OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opb = reinterpret_cast<OverlappedPacketBuffer*>(ptr);
#ifdef TEST_MODE
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);
    sendByteCount_.fetch_add(header->totalSize_);
#endif
}

DUBU::Session* DUBU::Server::CreateSession(const SOCKADDR_IN& addr)
{
	Session* session = sessionManager_.AddSession();
	session->SetSockAddr(addr);
    session->SetSocket(rudpSocket_.get());
	session->SetTimestamp(DUBU::GetRelativeTimeMs());
#ifdef TEST_MODE
    newSessionCount_.fetch_add(1);
#endif

	return session;
}

void DUBU::Server::DestroySession(Session* session)
{
	if (session->IsConnection())
	{
		session->Disconnect();
	}

	session->Reset();
	Push<Session>(session);
}

void DUBU::Server::RemoveSession(Uint32 sessionId)
{
	Session* session = sessionManager_.GetSession(sessionId);
	if (session == nullptr)
	{
		return;
	}

	peerMap_.erase(PeerKey(session->GetSockAddr()));
	sessionManager_.RemoveSession(sessionId);
	DestroySession(session);
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
    // disconnect는 0번 보낸다.
	header->sequenceNo_ = 0;
	header->timestamp_ = GetRelativeTimeMs();

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
	header->timestamp_ = GetRelativeTimeMs();
	header->packetCode_ = 0;

	opb->size_ = header->totalSize_;

	Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
	header->checksum_ = checksum;
	rudpSocket_->SendTo(session->GetSockAddr(), opb);

	// AddPendingPacket 호출하지 않음 — 재전송/ACK 추적 없음
    session->AddPingCount();

#ifdef TEST_MODE
	spdlog::debug("PING session {} : {}", session->GetSessionId(), header->sequenceNo_);
#endif
}

void DUBU::Server::SendAck(Uint32 seqNo, Session* session, const Packet::PacketOpctions& opt)
{
    OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

    // 헤더 작성 (header-only, 비신뢰)
    header->checksum_ = 0;
    header->flags_ = Packet::PacketHeaderFlag::ACK;
    if (opt.order_)
    {
        header->flags_ |= Packet::PacketHeaderFlag::CHANNEL;
        header->flags_ |= opt.channelID_ << 3;
    }
    header->totalSize_ = sizeof(Packet::PacketHeader);
    header->sessionId_ = session->GetSessionId();
    header->sequenceNo_ = seqNo;
    header->timestamp_ = GetRelativeTimeMs();
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

#ifdef TEST_MODE
void DUBU::Server::PrintStats(Uint32 activeCount, float rttAvgMillisec, Uint32 rttMaxMillisec)
{
    Uint64 recvPacket = recvPacketCount_.exchange(0);
    Uint64 sendPacket = sendPacketCount_.exchange(0);
    Uint64 recvByte = recvByteCount_.exchange(0);
    Uint64 sendByte = sendByteCount_.exchange(0);
    Uint64 recvAck = recvAckCount_.exchange(0);
    Uint64 sendAck = sendAckCount_.exchange(0);
    Uint64 timeout = timeoutSessionCount_.exchange(0);
    Uint32 newCount = newSessionCount_.exchange(0);

    spdlog::info(
        "[STATS] active={} new=+{} "
        "recv={}p/{:.2f}MB send={}p/{:.2f}MB "
        "recv_ack={} send_ack={} timeout={} "
        "rtt_avg={:.1f}ms rtt_max={}ms",
        activeCount, newCount,
        recvPacket, recvByte / 1048576.0,
        sendPacket, sendByte / 1048576.0,
        recvAck, sendAck, timeout,
        rttAvgMillisec, rttMaxMillisec);
}
#endif
