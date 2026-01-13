#include "ConcurrentQueue.h"

DUBU::ConcurrentQueue::ConcurrentQueue()
{
}

DUBU::ConcurrentQueue::ConcurrentQueue(DUBU::ConcurrentQueue&& other) noexcept
{
	q = std::move(other.q);
}

void DUBU::ConcurrentQueue::Push(FbbPtr fbbPtr)
{
	WriteLockGuard wl(lk);
	q.push(fbbPtr);
}

FbbPtr DUBU::ConcurrentQueue::Pop()
{
	WriteLockGuard wl(lk);
	if (q.empty())
		return nullptr;
	FbbPtr fbbptr = q.front();
	q.pop();
	return fbbptr;
}
