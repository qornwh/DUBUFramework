#pragma once
#include "pch.h"

#include <vector>
#include <memory>
#include <map>
#include <bit>
#include <atomic>
#include "RWLock.h"
#include "ObjectPool.h"

namespace DUBU
{
	// 1바이트 DubuByte 생성
	using DubuByte = unsigned char;
	using DubuBytePtr = DubuByte*;
	using DubuByteDataSPtr = std::shared_ptr<struct DubuByteData>;

	/* 
	전역 변수 선언
	*/
	// pool_list : 1개로 큰 데이터로 풀링 할 예정이었으나 n개로 나누어 둔것
	extern Vector<DubuByteDataSPtr> PoolList;
	// pool_chunk_list : 실제 크기별로 할당할 데이터가 있음
	extern Map<int, Vector<DubuBytePtr>> PoolChunkList;
	// pool_list : 기본 크기 지정
	extern const int PoolSize;
	// Lock 
	extern Lock PoolLock;
	// 풀에 담긴 메모리 최대 크기
	constexpr int MaxChunkSize = 1 << 10;

	struct DubuByteData : public std::enable_shared_from_this<DubuByteData>
	{
		DubuByteData();
		~DubuByteData();

		const size_t size_ = 3'582'410;
		// 메모리 시작 주소 <= 이거를 키 및 탐색용으로 찾음
		DubuBytePtr ptr_;
		size_t idx_ = 0;
		std::atomic<int> useCnt_ = 0;
	};

	void Initialize();
	DubuByteDataSPtr FindBlock(DubuBytePtr ptr);

	// 구조체/클래스 단위
	template<typename T>
	void Push(T* object)
	{
		// bit_ceil : 2 > 4 > 8 > 16 > 32 2의 배수중 큰값중 최소로 잡는다 
		constexpr size_t len = std::bit_ceil(sizeof(T));

		if (MaxChunkSize < len)
		{
			// 오브젝트 풀링으로 대체한다
			return ObjectPool<T>::GetInstance().Push(object);
		}

		DubuBytePtr ptr = reinterpret_cast<DubuBytePtr>(object);

		WriteLockGuard wl(PoolLock);
		PoolChunkList[len].push_back(ptr);

		DubuByteDataSPtr pool_ptr = FindBlock(ptr);
		pool_ptr->useCnt_.fetch_sub(1);

		if (pool_ptr->useCnt_.load() == 0 && PoolList.size() > PoolSize)
		{
			auto it = PoolList.begin() + PoolSize;
			for (; it != PoolList.end(); ++it)
			{
				if (it->get()->ptr_ == pool_ptr->ptr_)
				{
					PoolList.erase(it);
					pool_ptr.reset();
					break;
				}
			}
		}
	}

	// 구조체/클래스 단위
	template<typename T, typename... Args>
	T* Pop(Args&& ...args)
	{
		using ElementType = std::remove_pointer_t<T>;
		DubuBytePtr ptr = nullptr;

		constexpr size_t len = std::bit_ceil(sizeof(ElementType));

		if (MaxChunkSize < len)
		{
			// 오브젝트 풀링으로 대체한다
			return ObjectPool<T>::GetInstance().Pop(std::forward<Args>(args)...);
		}

		WriteLockGuard wl(PoolLock);

		if (PoolChunkList[len].empty())
		{
			// 만약 모든 풀링에 있는 데이터를 사용할 경우 다시 생성
			PoolList.push_back(std::make_shared<DubuByteData>());
		}

		ptr = PoolChunkList[len].back();
		PoolChunkList[len].pop_back();

		DubuByteDataSPtr pool_ptr = FindBlock(ptr);
		pool_ptr->useCnt_.fetch_add(1);

		return reinterpret_cast<T*>(ptr);
	}
}
