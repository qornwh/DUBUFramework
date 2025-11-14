#pragma once
#include "pch.h"

#include <queue>
#include "RWLock.h"

namespace DUBU 
{
	class ConCurrentQueue
	{
	public:
		ConCurrentQueue();
		ConCurrentQueue(const ConCurrentQueue& other) = delete;
		ConCurrentQueue(ConCurrentQueue&& other) noexcept;

		void Push(FbbPtr fbbPtr);
		FbbPtr Pop();

	private:
		std::queue<FbbPtr> q;
		Lock lk;
	};
}
