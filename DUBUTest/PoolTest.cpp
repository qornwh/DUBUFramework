#include "pch.h"
#include <thread>
#include <atomic>
#include <cstring>
#include <Pool.h>

// 바이트 풀(Pop/Push) 테스트
// - 청크는 생성자 호출 없는 raw 메모리 (1024 이하 크기 전용)

namespace
{
    // bit_ceil(9) = 16
    struct Chunk16A { char b[9]; };
    // bit_ceil(13) = 16 (Chunk16A와 같은 계급)
    struct Chunk16B { char b[13]; };
    // bit_ceil(33) = 64
    struct Chunk64 { char b[33]; };
    // bit_ceil(600) = 1024
    struct Chunk1024 { char b[600]; };
    // 멀티스레드 검증용 (정확히 16바이트)
    struct Chunk16Full { char b[16]; };
}

// Pop -> Push -> Pop 하면 같은 청크가 재사용된다 (LIFO)
TEST(PoolTest, PopPushReusesSameChunk)
{
    DUBU::Initialize();

    Chunk16A* first = DUBU::Pop<Chunk16A>();
    ASSERT_NE(first, nullptr);
    DUBU::Push<Chunk16A>(first);

    Chunk16A* second = DUBU::Pop<Chunk16A>();
    EXPECT_EQ(first, second);
    DUBU::Push<Chunk16A>(second);
}

// 같은 bit_ceil 계급이면 타입이 달라도 같은 청크를 공유한다
TEST(PoolTest, SameSizeClassSharesChunk)
{
    DUBU::Initialize();

    Chunk16A* a = DUBU::Pop<Chunk16A>();
    DUBU::Push<Chunk16A>(a);

    // 9바이트, 13바이트 모두 16 계급 -> 방금 반환한 청크가 나온다
    Chunk16B* b = DUBU::Pop<Chunk16B>();
    EXPECT_EQ(reinterpret_cast<void*>(a), reinterpret_cast<void*>(b));
    DUBU::Push<Chunk16B>(b);

    // 33바이트는 64 계급 -> 다른 청크
    Chunk64* c = DUBU::Pop<Chunk64>();
    EXPECT_NE(reinterpret_cast<void*>(a), reinterpret_cast<void*>(c));
    DUBU::Push<Chunk64>(c);
}

// 청크 소진시 새 블록이 증설된다 (1024 계급은 블록당 100개)
TEST(PoolTest, GrowsNewBlockWhenExhausted)
{
    DUBU::Initialize();

    const size_t blockCountBefore = DUBU::PoolList.size();

    Vector<Chunk1024*> popped;
    popped.reserve(300);
    while (DUBU::PoolList.size() == blockCountBefore)
    {
        ASSERT_LT(popped.size(), 300u) << "300개를 뽑아도 블록이 안 늘어남";
        popped.push_back(DUBU::Pop<Chunk1024>());
    }

    EXPECT_GT(DUBU::PoolList.size(), blockCountBefore);

    // 전부 반환
    for (Chunk1024* ptr : popped)
    {
        DUBU::Push<Chunk1024>(ptr);
    }
}

// 반환 후 useCnt가 원상 복구된다
TEST(PoolTest, UseCountBalanced)
{
    DUBU::Initialize();

    auto sumUseCount = []() {
        int sum = 0;
        for (auto& block : DUBU::PoolList)
        {
            sum += block->useCnt_.load();
        }
        return sum;
    };

    const int before = sumUseCount();

    Vector<Chunk64*> popped;
    for (int i = 0; i < 50; ++i)
    {
        popped.push_back(DUBU::Pop<Chunk64>());
    }
    EXPECT_EQ(sumUseCount(), before + 50);

    for (Chunk64* ptr : popped)
    {
        DUBU::Push<Chunk64>(ptr);
    }
    EXPECT_EQ(sumUseCount(), before);
}

// 멀티스레드 Pop/Push - 같은 청크가 두 스레드에 동시에 나가면 패턴이 깨진다
TEST(PoolTest, MultiThreadPopPush)
{
    DUBU::Initialize();

    constexpr int ThreadCount = 4;
    constexpr int Iteration = 5000;
    std::atomic<int> corrupted = 0;

    auto task = [&corrupted](int tid) {
        const char mark = static_cast<char>(tid + 1);
        for (int i = 0; i < Iteration; ++i)
        {
            Chunk16Full* ptr = DUBU::Pop<Chunk16Full>();
            std::memset(ptr->b, mark, sizeof(ptr->b));

            // 소유권이 겹치면 다른 스레드의 memset이 침범한다
            for (int k = 0; k < 4; ++k)
            {
                for (char byte : ptr->b)
                {
                    if (byte != mark)
                    {
                        corrupted.fetch_add(1);
                    }
                }
            }
            DUBU::Push<Chunk16Full>(ptr);
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
}
