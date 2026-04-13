#include "RWLock.h"
#include <thread>

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
			uint32_t expected = count.load(std::memory_order_relaxed);

			// WRITE 비트 켜져있으면 스킵 (버그 수정 핵심!)
			if (expected & WRITE)
			{
				_mm_pause();
				continue;
			}

			uint32_t desired = expected + 1;
			if (count.compare_exchange_weak(expected, desired, std::memory_order_acquire,std::memory_order_relaxed))
			{
				return;
			}
			_mm_pause();
		}
		std::this_thread::yield();
	}
}

void DUBU::Lock::ReadUnLock()
{
	uint32_t prev = count.fetch_sub(1, std::memory_order_release);
	// 검증
	assert((prev & READ) > 0);
}

void DUBU::Lock::WriteLock()
{
	while (true)
	{
		for (uint32_t i = 0; i < MAX_SPIN_COUNT; i++)
		{
			uint32_t expected = EMPTY;
			if (count.compare_exchange_weak(expected, WRITE, std::memory_order_acquire, std::memory_order_relaxed))
			{
				return;
			}
			_mm_pause();  // 추가
		}
		std::this_thread::yield();
	}
}

void DUBU::Lock::WriteUnLock()
{
	count.store(EMPTY, std::memory_order_release);
}