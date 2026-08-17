#include "pch.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <RingBuffer.h>

namespace
{
	using RB = DUBU::DS::RingBuffer<Uint64>;
}

// 단일 스레드 : 빈 상태 Pop 실패 -> 64개 Push -> 가득 참 Push 실패 -> FIFO Pop -> 다시 빈 상태
TEST(RingBufferTest, SingleThreadBasic)
{
	RB rb;
	Uint64 v = 0;
	EXPECT_FALSE(rb.Pop(v));

	for (Uint64 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
	{
		ASSERT_TRUE(rb.Push(i * 10 + 7)) << "push 실패 i=" << i;
	}
	EXPECT_FALSE(rb.Push(9999)); // 가득 참

	for (Uint64 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
	{
		ASSERT_TRUE(rb.Pop(v)) << "pop 실패 i=" << i;
		EXPECT_EQ(v, i * 10 + 7);
	}
	EXPECT_FALSE(rb.Pop(v));
	EXPECT_EQ(rb.head_.load(), rb.tail_.load());
}

// 단일 스레드 : 64개 넘어서 랩어라운드 되는 경우 (seq 갱신 검증)
TEST(RingBufferTest, SingleThreadWrapAround)
{
	RB rb;
	Uint64 v = 0;
	for (Uint64 i = 0; i < DEFAULT_WINDOW_COUNT * 10; ++i)
	{
		ASSERT_TRUE(rb.Push(i)) << "push 실패 i=" << i;
		ASSERT_TRUE(rb.Pop(v)) << "pop 실패 i=" << i;
		EXPECT_EQ(v, i);
	}
}

// 멀티 스레드 : 8개 스레드 동시 Push -> 성공한 값 64개 = 드레인한 값 64개 (유실/중복 없음)
TEST(RingBufferTest, MultiPushThenDrain)
{
	RB rb;
	constexpr int THREADS = 8;
	std::vector<std::vector<Uint64>> pushed(THREADS);
	std::vector<std::thread> list;
	for (int t = 0; t < THREADS; ++t)
	{
		list.emplace_back([&rb, &pushed, t] {
			for (Uint64 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
			{
				Uint64 value = (Uint64)(t + 1) * 1000 + i;
				if (rb.Push(value))
				{
					pushed[t].push_back(value);
				}
			}
		});
	}
	for (auto& th : list)
	{
		th.join();
	}

	std::vector<Uint64> expected;
	for (auto& p : pushed)
	{
		expected.insert(expected.end(), p.begin(), p.end());
	}
	ASSERT_EQ(expected.size(), DEFAULT_WINDOW_COUNT); // 딱 버퍼 크기만큼만 성공

	std::vector<Uint64> popped;
	Uint64 v = 0;
	while (rb.Pop(v))
	{
		popped.push_back(v);
	}

	std::sort(expected.begin(), expected.end());
	std::sort(popped.begin(), popped.end());
	EXPECT_EQ(popped, expected);
}

// 멀티 스레드 : 가득 채운 뒤 8개 스레드 동시 Pop -> 전체 64개, 유실/중복 없음
TEST(RingBufferTest, MultiPopAfterFill)
{
	RB rb;
	for (Uint64 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
	{
		ASSERT_TRUE(rb.Push(i + 100));
	}

	constexpr int THREADS = 8;
	std::vector<std::vector<Uint64>> popped(THREADS);
	std::vector<std::thread> list;
	for (int t = 0; t < THREADS; ++t)
	{
		list.emplace_back([&rb, &popped, t] {
			Uint64 v = 0;
			while (rb.Pop(v))
			{
				popped[t].push_back(v);
			}
		});
	}
	for (auto& th : list)
	{
		th.join();
	}

	std::vector<Uint64> all;
	for (auto& p : popped)
	{
		all.insert(all.end(), p.begin(), p.end());
	}
	ASSERT_EQ(all.size(), DEFAULT_WINDOW_COUNT);
	std::sort(all.begin(), all.end());
	for (Uint64 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
	{
		EXPECT_EQ(all[i], i + 100);
	}
	Uint64 v = 0;
	EXPECT_FALSE(rb.Pop(v));
}

// 멀티 스레드 : 4 producer x 4 consumer 동시 스트리밍, 총 40만개 유실/중복 없이 전달
TEST(RingBufferTest, MultiPushPopConcurrent)
{
	RB rb;
	constexpr int PRODUCERS = 4;
	constexpr int CONSUMERS = 4;
	constexpr Uint64 ITEMS = 100'000; // producer 당 개수
	constexpr Uint64 TOTAL = PRODUCERS * ITEMS;

	std::atomic<Uint64> consumedCount{ 0 };
	std::atomic<bool> timedOut{ false };
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);

	std::vector<std::vector<Uint64>> consumed(CONSUMERS);
	std::vector<std::thread> list;

	for (int p = 0; p < PRODUCERS; ++p)
	{
		list.emplace_back([&rb, &timedOut, deadline, p] {
			for (Uint64 i = 0; i < ITEMS; ++i)
			{
				Uint64 value = (Uint64)(p + 1) * 10'000'000 + i;
				while (!rb.Push(value))
				{
					if (timedOut.load() || std::chrono::steady_clock::now() > deadline)
					{
						timedOut.store(true);
						return;
					}
					std::this_thread::yield();
				}
			}
		});
	}

	for (int c = 0; c < CONSUMERS; ++c)
	{
		list.emplace_back([&rb, &consumed, &consumedCount, &timedOut, deadline, c] {
			consumed[c].reserve(TOTAL / CONSUMERS + 1024);
			Uint64 v = 0;
			while (true)
			{
				if (rb.Pop(v))
				{
					consumed[c].push_back(v);
					consumedCount.fetch_add(1);
				}
				else
				{
					if (consumedCount.load() >= TOTAL || timedOut.load())
					{
						return;
					}
					if (std::chrono::steady_clock::now() > deadline)
					{
						timedOut.store(true);
						return;
					}
					std::this_thread::yield();
				}
			}
		});
	}

	for (auto& th : list)
	{
		th.join();
	}
	ASSERT_FALSE(timedOut.load()) << "타임아웃, consumed=" << consumedCount.load() << "/" << TOTAL;

	std::vector<Uint64> all;
	all.reserve(TOTAL);
	for (auto& c : consumed)
	{
		all.insert(all.end(), c.begin(), c.end());
	}
	ASSERT_EQ(all.size(), TOTAL);

	std::vector<Uint64> expected;
	expected.reserve(TOTAL);
	for (int p = 0; p < PRODUCERS; ++p)
	{
		for (Uint64 i = 0; i < ITEMS; ++i)
		{
			expected.push_back((Uint64)(p + 1) * 10'000'000 + i);
		}
	}
	std::sort(all.begin(), all.end());
	std::sort(expected.begin(), expected.end());
	EXPECT_EQ(all, expected);
	EXPECT_EQ(rb.head_.load(), rb.tail_.load());
}
