#pragma once
#include "pch.h"

#include <vector>
#include <memory>
#include <map>
#include <bit>
#include <atomic>

namespace DUBU
{
	template<typename T, typename K>
	using Map = std::map<T, K>;
	template<typename T>
	using Vector = std::vector<T>;

	// 1바이트 DubuByte 생성
	using DubuByte = unsigned char;
	using DubuBytePtr = DubuByte*;
	using DubuByteDataSPtr = std::shared_ptr<struct DubuByteData>;

	/* 
	전역 변수 선언
	*/
	// pool_list : 1개로 큰 데이터로 풀링 할 예정이었으나 n개로 나누어 둔것
	extern Vector<DubuByteDataSPtr> pool_list;
	// pool_chunk_list : 실제 크기별로 할당할 데이터가 있음
	extern Map<int, Vector<DubuBytePtr>> pool_chunk_list;
	// pool_list : 기본 크기 지정
	extern const int POOLSIZE;

	struct DubuByteData : public std::enable_shared_from_this<DubuByteData>
	{
		DubuByteData();
		~DubuByteData();

		// size = (4 + 8 + 16 + 32) * 10'000 + (64 + 128) * 2'500 + (256 + 512) * 100
		const size_t size = 1'157'000 + 1;
		// 메모리 시작 주소 <= 이거를 키 및 탐색용으로 찾음
		DubuBytePtr ptr;
		size_t idx = 0;
		std::atomic<int> use_cnt = 0;
	};

	void Init();
	DubuByteDataSPtr FindBlock(DubuBytePtr ptr);

	template<typename T>
	void Push(T object)
	{
		// 템플릿 이므로 constexpr를 쓰면 컴파일 타임때 T의 타입을 추론 가능하므로 T가 해당 타입이면, 거짓인 블록은 컴파일 될때 사라진다
		if constexpr (std::is_pointer_v<T>)
		{
			DubuBytePtr ptr = reinterpret_cast<DubuBytePtr>(object);
			constexpr size_t len = std::bit_ceil(sizeof(T));
			pool_chunk_list[len].push_back(ptr);

			DubuByteDataSPtr pool_ptr = FindBlock(ptr);
			pool_ptr->use_cnt.fetch_sub(1);

			if (pool_ptr->use_cnt.load() == 0 && pool_list.size() > POOLSIZE)
			{
				auto it = pool_list.begin() + POOLSIZE;
				for (; it != pool_list.end(); ++it)
				{
					if (it->get()->ptr == pool_ptr->ptr)
					{
						pool_list.erase(it);
						pool_ptr.reset();
						break;
					}
				}
			}
		}
		else
		{
			// 프로그램 종료
			fprintf(stderr, "해당 타입은 포인터가 아님 !!!\n");
			exit(EXIT_FAILURE);
		}
	}

	template<typename T>
	T Pop()
	{
		using ElementType = std::remove_pointer_t<T>;
		DubuBytePtr ptr = nullptr;

		if constexpr (std::is_pointer_v<T>)
		{
			constexpr size_t len = std::bit_ceil(sizeof(ElementType));
			if (pool_chunk_list[len].empty())
			{
				// 만약 모든 풀링에 있는 데이터를 사용할 경우 다시 생성
				pool_list.push_back(std::make_shared<DubuByteData>());
			}

			ptr = pool_chunk_list[len].back();
			pool_chunk_list[len].pop_back();

			DubuByteDataSPtr pool_ptr = FindBlock(ptr);
			pool_ptr->use_cnt.fetch_add(1);
		}
		else
		{
			// 프로그램 종료
			fprintf(stderr, "해당 타입은 포인터가 아님 !!!\n");
			exit(EXIT_FAILURE);
		}

		return reinterpret_cast<T>(ptr);
	}
}
