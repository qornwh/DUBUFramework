#include "RWLock.h"
#include <thread>

DUBU::Lock::Lock()
{
	count_.store(0);
}

void DUBU::Lock::ReadLock()
{
	while (true)
	{
		for (uint32_t i = 0; i < MAX_SPIN_COUNT; i++)
		{
			uint32_t expected = count_.load(std::memory_order_relaxed);

			if (expected & WRITE)
			{
				_mm_pause();
				continue;
			}

			uint32_t desired = expected + 1;
			if (count_.compare_exchange_weak(expected, desired, std::memory_order_acquire,std::memory_order_relaxed))
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
	uint32_t prev = count_.fetch_sub(1, std::memory_order_release);
	assert((prev & READ) > 0);
}

void DUBU::Lock::WriteLock()
{
	while (true)
	{
		for (uint32_t i = 0; i < MAX_SPIN_COUNT; i++)
		{
			uint32_t expected = EMPTY;
			if (count_.compare_exchange_weak(expected, WRITE, std::memory_order_acquire, std::memory_order_relaxed))
			{
				return;
			}
			_mm_pause();
		}
		std::this_thread::yield();
	}
}

void DUBU::Lock::WriteUnLock()
{
	count_.store(EMPTY, std::memory_order_release);
}