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
        const uint32_t MAX_SPIN_COUNT = 500;
        std::atomic<uint32_t> count;
    };

    class ReadLockGuard
    {
    public:
        ReadLockGuard(Lock& lock) : _lock(lock)
        {
            _lock.ReadLock();
        }

        ~ReadLockGuard()
        {
            _lock.ReadUnLock();
        }

    private:
        Lock& _lock;
    };

    class WriteLockGuard
    {
    public:
        WriteLockGuard(Lock& lock) : _lock(lock)
        {
            _lock.WriteLock();
        }

        ~WriteLockGuard()
        {
            _lock.WriteUnLock();
        }

    private:
        Lock& _lock;
    };
}