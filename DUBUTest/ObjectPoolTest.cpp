#include "pch.h"
#include <thread>
#include <atomic>
#include <cstring>
#include <Pool.h>
#include <ObjectPool.h>

// ObjectPool 테스트 (1024 초과 타입 전용)
// - freeList 규약 : 소멸 완료된 빈 메모리
// - Pop = placement new(생성자), Push = ~T()(소멸자), delete 없음

namespace
{
    // sizeof 2048 > 1024 -> DUBU::Pop/Push가 ObjectPool로 라우팅
    struct BigObj
    {
        inline static std::atomic<int> ctorCount = 0;
        inline static std::atomic<int> dtorCount = 0;

        BigObj() { ctorCount.fetch_add(1); }
        ~BigObj() { dtorCount.fetch_add(1); }

        char payload[2048]{};
    };

    // Initialize 검증 전용 (BigObj와 카운터 분리)
    struct InitObj
    {
        inline static std::atomic<int> ctorCount = 0;

        InitObj() { ctorCount.fetch_add(1); }

        char payload[2048]{};
    };
}

// 1024 초과 타입은 ObjectPool로 라우팅되고, Pop=생성자 / Push=소멸자
TEST(ObjectPoolTest, PopCallsCtorPushCallsDtor)
{
    const int ctorBefore = BigObj::ctorCount.load();
    const int dtorBefore = BigObj::dtorCount.load();

    BigObj* obj = DUBU::Pop<BigObj>();
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(BigObj::ctorCount.load(), ctorBefore + 1);
    EXPECT_EQ(BigObj::dtorCount.load(), dtorBefore);

    DUBU::Push<BigObj>(obj);
    EXPECT_EQ(BigObj::dtorCount.load(), dtorBefore + 1);
}

// Push한 메모리가 다음 Pop에서 그대로 재사용된다 (freeList LIFO)
TEST(ObjectPoolTest, ReusesSamePointer)
{
    BigObj* first = DUBU::Pop<BigObj>();
    DUBU::Push<BigObj>(first);

    BigObj* second = DUBU::Pop<BigObj>();
    EXPECT_EQ(first, second);
    DUBU::Push<BigObj>(second);
}

// Initialize는 메모리만 확보하고 생성자를 호출하지 않는다
TEST(ObjectPoolTest, InitializeDoesNotCallCtor)
{
    const int ctorBefore = InitObj::ctorCount.load();

    DUBU::ObjectPool<InitObj>::GetInstance().Initialize(3);
    EXPECT_EQ(InitObj::ctorCount.load(), ctorBefore);

    // Pop 시점에 비로소 생성자 호출 (예약된 메모리 사용)
    InitObj* obj = DUBU::Pop<InitObj>();
    EXPECT_EQ(InitObj::ctorCount.load(), ctorBefore + 1);
    DUBU::Push<InitObj>(obj);
}

// 멀티스레드 Pop/Push - 생성/소멸 횟수가 정확히 맞아야 한다
TEST(ObjectPoolTest, MultiThreadPopPush)
{
    constexpr int ThreadCount = 4;
    constexpr int Iteration = 2000;

    const int ctorBefore = BigObj::ctorCount.load();
    const int dtorBefore = BigObj::dtorCount.load();
    std::atomic<int> corrupted = 0;

    auto task = [&corrupted](int tid) {
        const char mark = static_cast<char>(tid + 1);
        for (int i = 0; i < Iteration; ++i)
        {
            BigObj* obj = DUBU::Pop<BigObj>();
            std::memset(obj->payload, mark, sizeof(obj->payload));

            // 소유권이 겹치면 다른 스레드의 memset이 침범한다
            for (int k = 0; k < 256; k += 64)
            {
                if (obj->payload[k] != mark)
                {
                    corrupted.fetch_add(1);
                }
            }
            DUBU::Push<BigObj>(obj);
        }
    };

    Vector<std::thread> threads;
    for (int t = 0; t < ThreadCount; ++t)
    {
        threads.emplace_back(task, t);
    }
    for (auto& th : threads)
    {
        th.join();
    }

    EXPECT_EQ(corrupted.load(), 0);
    const int totalCalls = ThreadCount * Iteration;
    EXPECT_EQ(BigObj::ctorCount.load() - ctorBefore, totalCalls);
    EXPECT_EQ(BigObj::dtorCount.load() - dtorBefore, totalCalls);
}
