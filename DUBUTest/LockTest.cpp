#include "gtest/gtest.h"
#include "pch.h"
#include <iostream>
#include <thread>
#include <RWLock.h>
#include <functional>

#define LOCK_TEST(test_case_name, test_name) \
    TEST(test_case_name, test_name)

LOCK_TEST(DUBUTest, LockTest) {
    using namespace DUBU;
    Lock lock;
    int common_value = 0;
    std::function<void(int&)> task = [&lock](int& common_value) {
        for (int i = 0; i < 1000; ++i)
        {
            {
                WriteLockGuard wl(lock);
                ++common_value;
            }
            if (i % 100 == 0)
            {
                ReadLockGuard rl(lock);
                std::cerr << common_value << '\n';
            }
        }
    };
    std::thread t1(task, std::ref(common_value)), t2(task, std::ref(common_value));

    t1.join();
    t2.join();

    EXPECT_EQ(common_value, 2000);
}