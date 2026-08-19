#include "SessionWorker.h"
#include "Server.h"
#include "Packet.h"

Deque<DUBU::SessionWorker> g_sessionWorkers;

DUBU::SessionWorker::SessionWorker()
{
    queue_.set_capacity(8192);
}

void DUBU::SessionWorker::Push(const SessionJob& job)
{
    if (!queue_.try_push(job))
    {
        // 큐 가득참 : 드랍하고 버퍼는 풀로 반환
        if (job.opb != nullptr)
        {
            PacketManager::GetInstance().PushPacketBuffer(job.opb);
        }
    }
}

void DUBU::SessionWorker::Process(Uint32 num)
{
    assert(server_ != nullptr);

    SessionJob job;
    // 한번에 최대 64개까지만 소비 (주기작업 굶김 방지)
    for (Int32 i = 0; i < 64 && queue_.try_pop(job); ++i)
    {
        if (job.opb != nullptr)
        {
            Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(job.opb->buffer_);
            ProcessPacket(job.opb->buffer_, header->totalSize_, job.opb->remoteAddr_);
            // RUDPSocket에서 직접 반환 안함.
            PacketManager::GetInstance().PushPacketBuffer(job.opb);
        }
    }

    // 내 소유 세션들 재전송/핑/타임아웃 체크
    Uint32 now = GetRelativeTimeMs();
    if (now - lastCheckTime_ >= 50)
    {
        lastCheckTime_ = now;
        CheckSessions(num);
    }

    std::this_thread::yield();
}

void DUBU::SessionWorker::ProcessPacket(Uint8* buffer, Uint16 size, const SOCKADDR_IN& addr)
{
    // TODO : 이후 server의 세션 여기도 가지고 있기.
    assert(server_ != nullptr);

    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

    Uint32 sessionId = header->sessionId_;
    Uint8 flag = header->flags_;
    Uint32 seqNo = header->sequenceNo_;
    Uint64 key = server_->PeerKey(addr);
    Bool result = false;

    if (flag == Packet::PacketHeaderFlag::SESSION)
    {
        WriteLockGuard wl(server_->sessionLock_);
        auto it = server_->peerMap_.find(key);
        if (it == server_->peerMap_.end())
        {
            // 새 세션 생성. 라우팅이 주소 기준이라 이 주소의 모든 패킷은 계속 이 워커로 온다.
            Session* session = server_->CreateSession(addr);
            header->sessionId_ = session->GetSessionId();
            // 세션 추가 완료 응답
            server_->ConnectMessage(session);
            // Peer 생성
            server_->peerMap_.emplace(key, Peer{ key, addr, session });
        }
        else
        {
            // 이미 연결된 상태에서 SESSION이 또 옴 (접속 재시도 or 재접속) -> 연결 응답만 다시 보낸다
            // 라우팅이 주소 기준이라 여기도 이 세션의 담당 스레드다
            Session* session = it->second.session_;
            server_->ConnectMessage(session);
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
            ReadLockGuard rl(server_->sessionLock_);
            Session* session = server_->sessionManager_.GetSession(sessionId);
            if (session == nullptr)
            {
                if (flag == Packet::PacketHeaderFlag::DISCONNECT)
                {
                    // 이미 서버에서 연결 끊음
                    spdlog::info("Already Disconnect Session");
                }
                else
                {
                    spdlog::error("SessionId {} not found", sessionId);
                }
                return;
            }

            // 수신시 시간 갱신
            session->SetTimestamp(DUBU::GetRelativeTimeMs());

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
                    Packet::PacketOpctions opt{ true, false, 0 };
                    if ((flag & Packet::PacketHeaderFlag::CHANNEL) == Packet::PacketHeaderFlag::CHANNEL)
                    {
                        opt.order_ = true;
                        opt.channelID_ = (flag & Packet::PacketHeaderFlag::CHANNELMASK) >> 3;
                    }
                    server_->SendAck(seqNo, session, opt);
#ifdef _DEBUG
                    server_->sendAckCount_.fetch_add(1);
#endif
                }
            }
            else if ((flag & Packet::PacketHeaderFlag::ACK) == Packet::PacketHeaderFlag::ACK)
            {
                // ACK 수신 pendingpacket 지움
                result = session->RecvDispatchACK(buffer, size);
#ifdef _DEBUG
                server_->recvAckCount_.fetch_add(1);
#endif
            }

#ifdef _DEBUG
            server_->recvByteCount_.fetch_add(header->totalSize_);
#endif
        }
    }
}

void DUBU::SessionWorker::CheckSessions(Uint32 num)
{
    // TODO : 이후 server의 세션 여기도 가지고 있기.
    assert(server_ != nullptr);

    const Uint32 workerCount = static_cast<Uint32>(g_sessionWorkers.size());
    if (workerCount == 0)
    {
        return;
    }

    Uint32 now = GetRelativeTimeMs();

    // 끊을 세션 (이번 주기에 못 담은 것은 다음 주기에 처리)
    Uint32 removeList[64];
    Uint32 removeCount = 0;

    {
        // 내 소유 세션만 재전송/핑/타임아웃 체크 (세션 내부는 소유 워커만 접근하므로 락 불필요, 맵 순회만 ReadLock)
        ReadLockGuard rl(server_->sessionLock_);
        for (auto& [id, session] : server_->sessionManager_.GetSessions())
        {
            // 담당 판별도 라우팅과 같은 기준 : 주소 해시 % 워커수
            if (session == nullptr || server_->PeerKey(session->GetSockAddr()) % workerCount != num)
            {
                continue;
            }

            if (session == nullptr || !session->IsConnection())
            {
                continue;
            }

            if (session->GetAwaysConnect())
            {
                continue;
            }

            Uint32 delayTime = now - session->GetTimestamp();
            if (delayTime > server_->SessionTimeout)
            {
                if (removeCount < 64)
                {
                    removeList[removeCount++] = id;
                }
#ifdef _DEBUG
                server_->timeoutSessionCount_.fetch_add(1);
#endif
            }
            else if (delayTime > server_->PingTimeout)
            {
                if (now - session->GetLastPingSentTime() >= (Uint32)server_->PingTimeout)
                {
                    server_->SendPing(session);
                    session->SetLastPingSentTime(now);
                }
            }

            // 재전송, 왕복시간은 * 2 + g_defaultRttMsDelay
            session->RepeatMessageAll(server_->rudpSocket_.get(), session->GetRttMillisec() * 2 + g_defaultRttMsDelay);
        }
    }

    if (removeCount > 0)
    {
        // 연결 해제 : 세션과 peer를 같이 지운다
        WriteLockGuard wl(server_->sessionLock_);
        for (Uint32 i = 0; i < removeCount; ++i)
        {
            Uint32 sessionId = removeList[i];
            Session* session = server_->sessionManager_.GetSession(sessionId);
            if (session != nullptr)
            {
                server_->DisconnectMessage(session);
                session->Disconnect();
            }

            server_->RemoveSession(sessionId);
        }
    }
}

void DUBU::SessionWorker::ReturnBuffers()
{
    SessionJob job;
    while (queue_.try_pop(job))
    {
        if (job.opb != nullptr)
        {
            PacketManager::GetInstance().PushPacketBuffer(job.opb);
        }
    }
}
