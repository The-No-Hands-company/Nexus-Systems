#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>

#include "nexus/utility/concurrency/thread_pool.h"

using namespace nexus::utility::concurrency;

// ── ThreadPool Tests ───────────────────────────────────────────────────────

TEST(ThreadPoolTest, CreateAndShutdown) {
    ThreadPool pool(2);
    EXPECT_TRUE(pool.isRunning());
    EXPECT_EQ(pool.workerCount(), 2);

    pool.shutdown();
    EXPECT_FALSE(pool.isRunning());
}

TEST(ThreadPoolTest, SubmitAndGetResult) {
    ThreadPool pool(2);
    auto future = pool.submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
    pool.shutdown();
}

TEST(ThreadPoolTest, SubmitWithArgs) {
    ThreadPool pool(2);
    auto future = pool.submit([](int a, int b) { return a + b; }, 10, 20);
    EXPECT_EQ(future.get(), 30);
    pool.shutdown();
}

TEST(ThreadPoolTest, MultipleTasks) {
    ThreadPool pool(4);
    constexpr int kTasks = 100;
    std::vector<std::future<int>> futures;

    for (int i = 0; i < kTasks; ++i) {
        futures.push_back(pool.submit([i]() { return i * i; }));
    }

    for (int i = 0; i < kTasks; ++i) {
        EXPECT_EQ(futures[i].get(), i * i);
    }

    pool.shutdown();
}

TEST(ThreadPoolTest, WaitAll) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    for (int i = 0; i < 50; ++i) {
        pool.submit([&counter]() {
            counter.fetch_add(1);
            return 0;
        });
    }

    pool.waitAll();
    EXPECT_EQ(counter.load(), 50);
    pool.shutdown();
}

TEST(ThreadPoolTest, PriorityExecution) {
    ThreadPool pool(2);
    std::atomic<int> high_count{0};
    std::atomic<int> low_count{0};

    // Submit low-priority tasks first
    for (int i = 0; i < 20; ++i) {
        pool.submit(ThreadPool::Priority::Low, [&low_count]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            low_count.fetch_add(1);
            return 0;
        });
    }

    // Submit high-priority tasks that should execute sooner
    auto f1 = pool.submit(ThreadPool::Priority::High, [&high_count]() {
        high_count.fetch_add(1);
        return 0;
    });
    auto f2 = pool.submit(ThreadPool::Priority::High, [&high_count]() {
        high_count.fetch_add(1);
        return 0;
    });

    f1.get();
    f2.get();
    pool.waitAll();

    EXPECT_GE(high_count.load(), 2);
    EXPECT_EQ(low_count.load(), 20);

    pool.shutdown();
}

TEST(ThreadPoolTest, FireAndForget) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 50; ++i) {
        pool.execute(ThreadPool::Priority::Normal, [&counter]() {
            counter.fetch_add(1);
        });
    }

    pool.waitAll();
    EXPECT_EQ(counter.load(), 50);
    pool.shutdown();
}

TEST(ThreadPoolTest, ExceptionPropagation) {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("test error");
        return 0;
    });

    EXPECT_THROW(future.get(), std::runtime_error);
    pool.shutdown();
}

TEST(ThreadPoolTest, AutoDetectCores) {
    ThreadPool pool(0);  // 0 = hardware concurrency
    EXPECT_GE(pool.workerCount(), 1);
    pool.shutdown();
}

TEST(ThreadPoolTest, ParallelReduce) {
    ThreadPool pool(4);
    constexpr int kSize = 10000;
    std::vector<int> data(kSize);
    std::iota(data.begin(), data.end(), 0);

    // Partition the data and sum each chunk in parallel
    constexpr int kChunks = 4;
    std::vector<std::future<int64_t>> futures;

    for (int c = 0; c < kChunks; ++c) {
        size_t start = c * kSize / kChunks;
        size_t end = (c + 1) * kSize / kChunks;
        futures.push_back(pool.submit([&data, start, end]() -> int64_t {
            return std::accumulate(data.begin() + static_cast<ptrdiff_t>(start),
                                    data.begin() + static_cast<ptrdiff_t>(end), 0LL);
        }));
    }

    int64_t total = 0;
    for (auto& f : futures) total += f.get();

    int64_t expected = static_cast<int64_t>(kSize) * (kSize - 1) / 2;
    EXPECT_EQ(total, expected);
    pool.shutdown();
}

TEST(ThreadPoolTest, DestructorWaits) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 100; ++i) {
            pool.submit([&counter]() {
                counter.fetch_add(1);
                return 0;
            });
        }
        // Destructor calls shutdown which calls waitAll
    }
    EXPECT_EQ(counter.load(), 100);
}

TEST(ThreadPoolTest, QueuedTaskCount) {
    ThreadPool pool(2);
    for (int i = 0; i < 10; ++i) {
        pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return 0;
        });
    }
    // Some tasks should still be queued
    EXPECT_GT(pool.queuedTasks() + pool.activeTasks(), 0);
    pool.waitAll();
    pool.shutdown();
}
