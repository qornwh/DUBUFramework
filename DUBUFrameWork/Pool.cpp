#include "Pool.h"

namespace DUBU
{
	Vector<DubuByteDataSPtr> pool_list;
	Map<int, Vector<DubuBytePtr>> pool_chunk_list;
	const int POOLSIZE = 3;
}

DUBU::DubuByteData::DubuByteData()
{
	ptr = static_cast<DubuBytePtr>(malloc(sizeof(DubuByte) * size));
	if (ptr == nullptr)
	{
		// 메모리 할당 실패 처리 및 종료
		char str[256];
		if (strerror_s(str, sizeof(str), errno) != 0) 
			snprintf(str, sizeof(str), "알 수 없는 오류(%d)", errno);
		// 운영체제 에러 메시지 출력
		fprintf(stderr, "메모리 할당 실패: %s\n", str); 
		exit(EXIT_FAILURE);
	}

	// 메모리 쪼개둔다.
	int pos = 0;
	int len = 1;

	// 4 8 16 32 => 10000 
	// 64 128    => 2500
	// 258 512   => 100
	for (size_t i = 1; i <= 10; ++i)
	{
		for (size_t j = 1; j <= 10; ++j)
		{
			pool_chunk_list[len].push_back(&ptr[pos]);
			pos += len;
		}
		len <<= 1;
	}
}

DUBU::DubuByteData::~DubuByteData()
{
	if (ptr != nullptr)
	{
		if (use_cnt > 0)
		{
			// 프로그램 종료
			fprintf(stderr, "메모리 이미 사용중 - %d !!!\n", use_cnt.load());
			exit(EXIT_FAILURE); 
		}

		free(ptr);
		ptr = nullptr;
	}

	if (ptr != nullptr)
	{
		// 프로그램 종료 => 로그로 남겨두는것도
		fprintf(stderr, "메모리 해제 실패 !!!\n");
		exit(EXIT_FAILURE); 
	}

	printf("메모리 해제 성공 !!!\n");
}

void DUBU::Init()
{
	for (int i = 0; i < POOLSIZE; ++i)
	{
		pool_list.push_back(std::make_shared<DubuByteData>());
	}
}

DUBU::DubuByteDataSPtr DUBU::FindBlock(DubuBytePtr ptr)
{
	for (auto& block : pool_list)
	{
		// ptr 해당주소의 값의 pool의 주소에 포함되면 리턴
		if (ptr >= block->ptr && ptr < block->ptr + block->size)
			return block;
	}

	// pool주소가 안맞다는 뜻임, 프로그램 종료
	fprintf(stderr, "해당하는 공간이 없음 !!!\n");
	exit(EXIT_FAILURE);
	return nullptr;
}
