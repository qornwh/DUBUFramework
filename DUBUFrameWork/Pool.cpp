#include "Pool.h"

namespace DUBU
{
	Vector<DubuByteDataSPtr> PoolList;
	Map<int, Vector<DubuBytePtr>> PoolChunkList;
	const int PoolSize = 10;
	Lock PoolLock;
}

DUBU::DubuByteData::DubuByteData()
{
	ptr_ = static_cast<DubuBytePtr>(malloc(sizeof(DubuByte) * size_));
	if (ptr_ == nullptr)
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
	int len = 4;

	// 4 8 16 32 64 => 10000 
	// 128 258 512  => 2500
	// 1024         => 100
	for (size_t i = 1; i < 10; ++i)
	{
		Int32 cnt = 10000;
		if (len > 64) cnt = 2500;
		if (len > 512) cnt = 100;

		for (size_t j = 1; j <= cnt; ++j)
		{
			PoolChunkList[len].push_back(&ptr_[pos]);
			pos += len;
		}
		len <<= 1;
	}
}

DUBU::DubuByteData::~DubuByteData()
{
	if (ptr_ != nullptr)
	{
		if (useCnt_ > 0)
		{
			// 프로그램 종료
			fprintf(stderr, "메모리 이미 사용중 - %d !!!\n", useCnt_.load());
			//exit(EXIT_FAILURE); 
		}

		free(ptr_);
		ptr_ = nullptr;
	}

	if (ptr_ != nullptr)
	{
		// 프로그램 종료 => 로그로 남겨두는것도
		fprintf(stderr, "메모리 해제 실패 !!!\n");
		exit(EXIT_FAILURE); 
	}

	printf("메모리 해제 성공 !!!\n");
}

void DUBU::Initialize()
{
	for (int i = 0; i < PoolSize; ++i)
	{
		PoolList.push_back(std::make_shared<DubuByteData>());
	}
}

DUBU::DubuByteDataSPtr DUBU::FindBlock(DubuBytePtr ptr)
{
	for (auto& block : PoolList)
	{
		// ptr 해당주소의 값의 pool의 주소에 포함되면 리턴
		if (ptr >= block->ptr_ && ptr < block->ptr_ + block->size_)
			return block;
	}

	// pool주소가 안맞다는 뜻임, 프로그램 종료
	fprintf(stderr, "해당하는 공간이 없음 !!!\n");
	exit(EXIT_FAILURE);
	return nullptr;
}
