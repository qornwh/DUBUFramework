#pragma once
#include "pch.h"
#include "Types.h"
#include "BufferManager.h"

namespace DUBU
{
    class Server;

    struct SessionJob
    {
        Uint32 key = 0;
        OverlappedPacketBuffer* opb = nullptr;
    };

    class SessionWorker
    {
    public:
        SessionWorker();

        void SetServer(Server* server) { server_ = server; }
        void Stop() { isStart_.store(false); }
        Bool IsStart() { return isStart_.load(); }

        void Push(const SessionJob& job);
        void Process(Uint32 num);
        // 패킷 분기 처리
        void ProcessPacket(Uint8* buffer, Uint16 size, const SOCKADDR_IN& addr);
        // 종료시 큐에 남은 버퍼 전부 풀로 반환
        void ReturnBuffers();

    private:
        void CheckSessions(Uint32 num);

    private:
        Server* server_ = nullptr;
        ConcurrentQueue<SessionJob> queue_;
        Atomic<Bool> isStart_{ true };
        Uint32 lastCheckTime_ = 0;
    };
}

extern Deque<DUBU::SessionWorker> g_sessionWorkers;
