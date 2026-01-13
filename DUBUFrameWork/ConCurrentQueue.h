#pragma once
#include "pch.h"

#include <queue>
#include "RWLock.h"

namespace DUBU 
{
	class ConcurrentQueue
	{
	public:
		ConcurrentQueue();
		ConcurrentQueue(const ConcurrentQueue& other) = delete;
		ConcurrentQueue(ConcurrentQueue&& other) noexcept;

		void Push(FbbPtr fbbPtr);
		FbbPtr Pop();

	private:
		std::queue<FbbPtr> q;
		Lock lk;
	};
}
