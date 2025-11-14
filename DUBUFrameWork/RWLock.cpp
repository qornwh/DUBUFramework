#include "RWLock.h"
#include <thread>
#include <cassert>

DUBU::Lock::Lock()
{
	count.store(0);
}

void DUBU::Lock::ReadLock()
{
	while (true)
	{
        for (uint32_t i = 0; i < MAX_SPIN_COUNT; i++)
        {
            uint32_t expected = count.load() & READ; // 기대값
            uint32_t desired = expected + 1;  // 변경될값
            if (count.compare_exchange_strong(expected, desired))
            {
                return;
            }
        }
        std::this_thread::yield();
	}
}

void DUBU::Lock::ReadUnLock()
{
    auto cur = count.load();
    assert(cur > 0);
    count.fetch_sub(1);
}

void DUBU::Lock::WriteLock()
{
    while (true)
    {
        for (uint32_t i = 0; i < MAX_SPIN_COUNT; i++)
        {
            uint32_t expected = EMPTY; // 기대값
            uint32_t desired = WRITE;  // 변경될값
            if (count.compare_exchange_strong(expected, desired))
            {
                return;
            }
        }
        std::this_thread::yield();
    }
}

void DUBU::Lock::WriteUnLock()
{
    auto cur = count.load();
    assert(cur == WRITE);
    count.store(EMPTY);
}
