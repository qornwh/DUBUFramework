#include "gtest/gtest.h"
#include "pch.h"
#include <iostream>
#include <thread>
#include <RWLock.h>
#include <functional>
#include <chrono>
#include <shared_mutex>

#define LOCK_TEST(test_case_name, test_name) \
    TEST(test_case_name, test_name)

LOCK_TEST(DUBUTest, LockTest) 
{
	using namespace DUBU;
	Lock lock;
	Int32 common_value = 0;
	std::function<void(int&)> task = [&lock](int& common_value) {
		for (int i = 0; i < 10'000'000; ++i)
		{
			{
				WriteLockGuard wl(lock);
				++common_value;
			}
			if (i % 100 == 0)
			{
				ReadLockGuard rl(lock);
				volatile int read = common_value;  // 최적화 방지
			}
		}
	};

	auto start = std::chrono::steady_clock::now();
	Vector<std::thread> list;
	for (Int32 i = 0; i < 10; ++i)
	{
		list.emplace_back(task, std::ref(common_value));
	}

	for (Int32 i = 0; i < 10; ++i)
	{
		list[i].join();
	}
	auto end = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	spdlog::info("Lock 측정 시간 : {}ms", duration.count());

	EXPECT_EQ(common_value, 100'000'000);
}

LOCK_TEST(DUBUTest, LockTest2)
{
	using namespace DUBU;
	std::shared_mutex rw_mutex;
	Int32 common_value = 0;
	std::function<void(int&)> task = [&rw_mutex](int& common_value) {
		for (int i = 0; i < 10'000'000; ++i)
		{
			{
				std::unique_lock<std::shared_mutex> w_mutex(rw_mutex);
				++common_value;
			}
			if (i % 100 == 0)
			{
				std::shared_lock<std::shared_mutex> r_mutex(rw_mutex);
				volatile int read = common_value;  // 최적화 방지
			}
		}
	};

	auto start = std::chrono::steady_clock::now();
	Vector<std::thread> list;
	for (Int32 i = 0; i < 10; ++i)
	{
		list.emplace_back(task, std::ref(common_value));
	}

	for (Int32 i = 0; i < 10; ++i)
	{
		list[i].join();
	}
	auto end = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	spdlog::info("mutex 측정 시간 : {}ms", duration.count());

	EXPECT_EQ(common_value, 100'000'000);
}

// 

LOCK_TEST(DUBUTest, LockTest3)
{
	using namespace DUBU;
	Lock lock;
	Int32 common_value = 0;
	std::function<void(int&)> task = [&lock](int& common_value) {
		for (int i = 0; i < 10'000'000; ++i)
		{
			if (i % 100 == 0)
			{
				WriteLockGuard wl(lock);
				++common_value;
			}

			{
				ReadLockGuard rl(lock);
				volatile int read = common_value;  // 최적화 방지
			}
		}
	};

	auto start = std::chrono::steady_clock::now();
	Vector<std::thread> list;
	for (Int32 i = 0; i < 10; ++i)
	{
		list.emplace_back(task, std::ref(common_value));
	}

	for (Int32 i = 0; i < 10; ++i)
	{
		list[i].join();
	}
	auto end = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	spdlog::info("Lock 측정 시간 : {}ms", duration.count());

	EXPECT_EQ(common_value, 100'000'000 / 100);
}

LOCK_TEST(DUBUTest, LockTest4)
{
	using namespace DUBU;
	std::shared_mutex rw_mutex;
	Int32 common_value = 0;
	std::function<void(int&)> task = [&rw_mutex](int& common_value) {
		for (int i = 0; i < 10'000'000; ++i)
		{
			if (i % 100 == 0)
			{
				std::unique_lock<std::shared_mutex> w_mutex(rw_mutex);
				++common_value;
			}

			{
				std::shared_lock<std::shared_mutex> r_mutex(rw_mutex);
				volatile int read = common_value;  // 최적화 방지
			}
		}
	};

	auto start = std::chrono::steady_clock::now();
	Vector<std::thread> list;
	for (Int32 i = 0; i < 10; ++i)
	{
		list.emplace_back(task, std::ref(common_value));
	}

	for (Int32 i = 0; i < 10; ++i)
	{
		list[i].join();
	}
	auto end = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	spdlog::info("mutex 측정 시간 : {}ms", duration.count());

	EXPECT_EQ(common_value, 100'000'000 / 100);
}