#include "ConCurrentQueue.h"

DUBU::ConCurrentQueue::ConCurrentQueue()
{
}

DUBU::ConCurrentQueue::ConCurrentQueue(DUBU::ConCurrentQueue&& other) noexcept
{
	q = std::move(other.q);
}

void DUBU::ConCurrentQueue::Push(FbbPtr fbbPtr)
{
	WriteLockGuard wl(lk);
	q.push(fbbPtr);
}

FbbPtr DUBU::ConCurrentQueue::Pop()
{
	WriteLockGuard wl(lk);
	if (q.empty())
		return nullptr;
	FbbPtr fbbptr = q.front();
	q.pop();
	return fbbptr;
}
