#pragma once
#include "pch.h"
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
				q_ = std::move(other.q_);
			}

			void Push(T t)
			{
				WriteLockGuard wl(lk_);
				q_.push(t);
			}

			T Pop()
			{
				WriteLockGuard wl(lk_);
				if (q_.empty())
					return nullptr;
				T t = q_.front();
				q_.pop();
				return t;
			}

		private:
			Queue<T> q_;
			Lock lk_;
		};
	}
}
