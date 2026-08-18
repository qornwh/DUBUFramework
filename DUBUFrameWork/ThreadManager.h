#pragma once
#include <thread>
#include "pch.h"
#include "Types.h"
#include "Singleton.h"

namespace DUBU
{
    class ThreadManager : public Singleton<ThreadManager>
    {
    public:
        void SetRecvLoop(Function<void()> loop);
        void SetSessionLoop(Function<void(Uint32)> loop);
        void Start(Function<void()> logicLoop);
        void Join();

        Uint32 GetTotal() const { return totalThread_; }
        Uint32 GetRThreadSize() const { return rthreadSize_; }
        Uint32 GetSThreadSize() const { return sthreadSize_; }
        Uint32 GetLThreadSize() const { return lthreadSize_; }

    private:
        Function<void()> recvLoop_;
        Function<void(Uint32)> sessionLoop_;

        // 수신 스레드
        Vector<std::thread> rThreads_;
        // 세션 스레드
        Vector<std::thread> sThreads_;
        // 로직 스레드
        Vector<std::thread> lThreads_;

        Uint32 totalThread_ = 0;
        Uint32 rthreadSize_ = 0;
        Uint32 sthreadSize_ = 0;
        Uint32 lthreadSize_ = 0;
    };
}
