#pragma once
#include "pch.h"

namespace DUBU 
{
    class Lock
    {
        enum : uint32_t
        {
            EMPTY = 0x0000000,
            READ = 0x000FFFF,
            WRITE = 0xFFF0000,
        };
    public:
        Lock();
        Lock(const Lock& lock) = delete;
        Lock(Lock&& lock) = delete;
        void ReadLock();
        void ReadUnLock();
        void WriteLock();
        void WriteUnLock();
    private:
        const Uint32 MAX_SPIN_COUNT = 500;
        Atomic<Uint32> count_;
    };

    class ReadLockGuard
    {
    public:
        ReadLockGuard(Lock& lock) : lock_(lock)
        {
            lock_.ReadLock();
        }

        ~ReadLockGuard()
        {
            lock_.ReadUnLock();
        }

    private:
        Lock& lock_;
    };

    class WriteLockGuard
    {
    public:
        WriteLockGuard(Lock& lock) : lock_(lock)
        {
            lock_.WriteLock();
        }

        ~WriteLockGuard()
        {
            lock_.WriteUnLock();
        }

    private:
        Lock& lock_;
    };
}