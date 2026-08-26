#include "ThreadManager.h"
#include "SessionWorker.h"
#include "Server.h"

void DUBU::ThreadManager::SetRecvLoop(Function<void()> loop)
{
    if (loop != nullptr)
    {
        recvLoop_ = std::move(loop);
    }
    else
    {
        recvLoop_ = []()
        {
            assert(g_server != nullptr);

            while (g_server->IsRunning())
            {
                g_server->Dispatch();
            }
        };
    }
}

void DUBU::ThreadManager::SetSessionLoop(Function<void(Uint32)> loop)
{
    if (loop != nullptr)
    {
        sessionLoop_ = std::move(loop);
    }
    else
    {
        sessionLoop_ = [](Uint32 idx) 
        {
            assert(g_server != nullptr);
            assert(idx < g_sessionWorkers.size());

            SessionWorker& worker = g_sessionWorkers[idx];
            worker.SetServer(g_server);
            while (worker.IsStart())
            {
                worker.Process(idx);
            }
            worker.ReturnBuffers();
        };
    }
}

void DUBU::ThreadManager::Start(Function<void()> logicLoop)
{
    // 수신/세션 루프 등록 확인
    assert(recvLoop_ != nullptr);
    assert(sessionLoop_ != nullptr);

    // hardware_concurrency 실패시 0 리턴이라 최소값 보장
    Uint32 total = std::thread::hardware_concurrency();
    if (total < 4) total = 4;
    totalThread_ = total;

    // 배분 : 세션 = 전체/2, 로직 = 전체/4, 수신 = 나머지
    Uint32 remain = total;
    Uint32 sessionCount = total / 2;
    remain -= sessionCount;
    Uint32 logicCount = remain / 2;
    remain -= logicCount;
    Uint32 recvCount = remain;

    for (Uint32 i = 0; i < sessionCount; ++i)
    {
        g_sessionWorkers.emplace_back();
    }

    sThreads_.reserve(sessionCount);
    for (Uint32 i = 0; i < sessionCount; ++i)
    {
        sThreads_.emplace_back(sessionLoop_, i);
    }

    lThreads_.reserve(logicCount);
    for (Uint32 i = 0; i < logicCount; ++i)
    {
        lThreads_.emplace_back(logicLoop);
    }

    rThreads_.reserve(recvCount);
    for (Uint32 i = 0; i < recvCount; ++i)
    {
        rThreads_.emplace_back(recvLoop_);
    }

    sthreadSize_ = static_cast<Uint32>(sThreads_.size());
    lthreadSize_ = static_cast<Uint32>(lThreads_.size());
    rthreadSize_ = static_cast<Uint32>(rThreads_.size());
}

void DUBU::ThreadManager::Join()
{
    for (auto& t : rThreads_)
    {
        if (t.joinable()) t.join();
    }
    for (auto& t : sThreads_)
    {
        if (t.joinable()) t.join();
    }
    for (auto& t : lThreads_)
    {
        if (t.joinable()) t.join();
    }

    rThreads_.clear();
    sThreads_.clear();
    lThreads_.clear();
}
