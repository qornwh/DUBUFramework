#pragma once
#include "pch.h"

#include <queue>
#include "RWLock.h"

namespace DUBU
{
	namespace DS
	{
		template<typename T>
		class ConcurrentQueue
		{
		public:
			ConcurrentQueue()
			{

			}
			ConcurrentQueue(const ConcurrentQueue& other) = delete;
			ConcurrentQueue(ConcurrentQueue&& other) noexcept
			{
				q = std::move(other.q);
			}

			void Push(T t)
			{
				WriteLockGuard wl(lk);
				q.push(t);
			}

			T Pop()
			{
				WriteLockGuard wl(lk);
				if (q.empty())
					return nullptr;
				T t = q.front();
				q.pop();
				return t;
			}

		private:
			std::queue<T> q;
			Lock lk;
		};
	}
}
