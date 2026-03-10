#pragma once
#include "pch.h"
#include "Types.h"
#include "RWLock.h"

#define DEFAULT_RINGQUEUE_COUNT 200

namespace DUBU::DS
{
	/*
	* 일단 Lock기반 큐로 구현
	*/
	template<typename T>
	struct RingQueue
	{
		void Push(T t);
		void Pop(T& t);
		// 일단 매개변수 없는 타입으로만 적용
		bool PeekProcess(Function<bool()>&& function);

		T buffer[DEFAULT_RINGQUEUE_COUNT];
		Int32 startPos_ = 0;
		Int32 endPos_ = 0;

	private:
		bool Check();
		DUBU::Lock lk_;
	};

	template<typename T>
	inline void RingQueue<T>::Push(T t)
	{
		DUBU::WriteLockGuard wl(lk_);
		buffer[endPos_++] = t;
		if (endPos_ >= DEFAULT_RINGQUEUE_COUNT)
		{
			endPos_ = 0;
		}

		if (endPos_ == startPos_)
		{
			// 추가했는데 만약 넘처버렸다
			// 일단 프로그램 다운시킨다.
			assert(false);
		}
	}

	template<typename T>
	inline void RingQueue<T>::Pop(T& t)
	{
		DUBU::WriteLockGuard wl(lk_);
		if (!Check())
		{
			return;
		}

		t = buffer[startPos_];
		++startPos_;
		if (startPos_ >= DEFAULT_RINGQUEUE_COUNT)
		{
			startPos_ = 0;
		}
	}

	template<typename T>
	inline bool RingQueue<T>::PeekProcess(Function<bool()>&& function)
	{
		WriteLockGuard wl(lk_);
		if (!Check())
		{
			return false;
		}

		return function();
	}

	template<typename T>
	inline bool RingQueue<T>::Check()
	{
		if (startPos_ == endPos_)
		{
			return false;
		}
		return true;
	}
}

