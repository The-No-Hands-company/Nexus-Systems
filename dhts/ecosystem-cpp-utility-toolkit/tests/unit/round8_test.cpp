#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

#include "nexus/utility/concurrency/concurrency_primitives.h"
#include "nexus/utility/memory/memory_tracker.h"
#include "nexus/utility/logging/logger.h"
#include "nexus/utility/error/error_reporter.h"
#include "nexus/utility/concurrency/thread_tracker.h"

using namespace nexus::utility::concurrency;
using namespace nexus::utility::memory;

// ── Semaphore Tests ─────────────────────────────────────────────────────────

TEST(SemaphoreTest, AcquireRelease) {
    Semaphore sem(1);
    EXPECT_EQ(sem.available(), 1);
    sem.acquire();
    EXPECT_EQ(sem.available(), 0);
    sem.release();
    EXPECT_EQ(sem.available(), 1);
}

TEST(SemaphoreTest, BlockingAcquire) {
    Semaphore sem(0);
    std::atomic<bool> acquired{false};

    std::thread t([&]() {
        sem.acquire();
        acquired.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(acquired.load());

    sem.release();
    t.join();
    EXPECT_TRUE(acquired.load());
}

TEST(SemaphoreTest, TryAcquire) {
    Semaphore sem(0);
    EXPECT_FALSE(sem.tryAcquire());

    sem.release(2);
    EXPECT_TRUE(sem.tryAcquire());
    EXPECT_TRUE(sem.tryAcquire());
    EXPECT_FALSE(sem.tryAcquire());
}

TEST(SemaphoreTest, ReleaseMultiple) {
    Semaphore sem(0);
    sem.release(5);
    EXPECT_EQ(sem.available(), 5);
    EXPECT_TRUE(sem.tryAcquire());
    EXPECT_EQ(sem.available(), 4);
}

// ── Barrier Tests ───────────────────────────────────────────────────────────

TEST(BarrierTest, BasicSync) {
    constexpr size_t kThreads = 4;
    Barrier barrier(kThreads);
    std::atomic<int> phase{0};
    std::atomic<int> count_at_barrier{0};

    std::vector<std::thread> threads;
    for (size_t i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            phase.fetch_add(1);
            count_at_barrier.fetch_add(1);
            bool leader = barrier.wait();
            if (leader) {
                EXPECT_EQ(count_at_barrier.load(), kThreads);
            }
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(phase.load(), kThreads);
}

TEST(BarrierTest, LeaderElection) {
    Barrier barrier(3);
    std::atomic<int> leader_count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&]() {
            if (barrier.wait()) {
                leader_count.fetch_add(1);
            }
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(leader_count.load(), 1);  // Exactly one leader
}

TEST(BarrierTest, TwoPhaseSync) {
    Barrier barrier(2);
    std::atomic<int> phase{0};
    std::atomic<bool> main_passed_phase1{false};
    std::atomic<bool> main_passed_phase2{false};

    std::thread t([&]() {
        phase.store(1);
        barrier.wait();
        // Wait for main to check phase
        while (!main_passed_phase1.load()) { std::this_thread::yield(); }
        phase.store(2);
        barrier.wait();
        // Wait for main to check phase
        while (!main_passed_phase2.load()) { std::this_thread::yield(); }
        phase.store(3);
    });

    barrier.wait();
    EXPECT_EQ(phase.load(), 1);
    main_passed_phase1.store(true);

    barrier.wait();
    EXPECT_EQ(phase.load(), 2);
    main_passed_phase2.store(true);

    t.join();
    EXPECT_EQ(phase.load(), 3);
}

// ── Latch Tests ─────────────────────────────────────────────────────────────

TEST(LatchTest, CountDownToZero) {
    Latch latch(3);
    EXPECT_FALSE(latch.tryWait());
    EXPECT_EQ(latch.remaining(), 3);

    latch.countDown(2);
    EXPECT_EQ(latch.remaining(), 1);
    EXPECT_FALSE(latch.tryWait());

    latch.countDown();
    EXPECT_TRUE(latch.tryWait());
    EXPECT_EQ(latch.remaining(), 0);
}

TEST(LatchTest, WaitForZero) {
    Latch latch(1);
    std::atomic<bool> done{false};

    std::thread t([&]() {
        latch.wait();
        done.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_FALSE(done.load());

    latch.countDown();
    t.join();
    EXPECT_TRUE(done.load());
}

// ── ReadWriteLock Tests ─────────────────────────────────────────────────────

TEST(ReadWriteLockTest, MultipleReaders) {
    ReadWriteLock rwlock;
    std::atomic<int> readers{0};
    std::atomic<int> max_readers{0};

    auto reader = [&]() {
        ReadWriteLock::ReadGuard guard(rwlock);
        int r = readers.fetch_add(1) + 1;
        int prev = max_readers.load();
        while (r > prev && !max_readers.compare_exchange_weak(prev, r)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        readers.fetch_sub(1);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(reader);
    }
    for (auto& t : threads) t.join();

    EXPECT_GT(max_readers.load(), 1);  // Multiple readers allowed
}

TEST(ReadWriteLockTest, WriteExclusive) {
    ReadWriteLock rwlock;
    std::atomic<bool> write_active{false};
    std::atomic<bool> violation{false};

    auto writer = [&]() {
        ReadWriteLock::WriteGuard guard(rwlock);
        if (write_active.exchange(true)) {
            violation.store(true);  // Two writers shouldn't be active
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        write_active.store(false);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(writer);
    }
    for (auto& t : threads) t.join();

    EXPECT_FALSE(violation.load());
}

// ── Integration Tests ───────────────────────────────────────────────────────

TEST(IntegrationTest, AllocationTrackingAndReporting) {
    auto& tracker = MemoryTracker::instance();
    tracker.reset();
    tracker.setEnabled(true);

    int a, b, c;
    tracker.recordAllocation(&a, 100, "int");
    tracker.recordAllocation(&b, 200, "int");
    tracker.recordAllocation(&c, 300, "int");
    tracker.recordDeallocation(&b);

    auto stats = tracker.getStats();
    EXPECT_EQ(stats.total_allocations, 3);
    EXPECT_EQ(stats.total_deallocations, 1);
    EXPECT_EQ(stats.current_allocations, 2);

    auto leaks = tracker.detectLeaks();
    EXPECT_EQ(leaks.size(), 2);

    tracker.reset();
}

TEST(IntegrationTest, ErrorReporterLoggingIntegration) {
    auto& reporter = nexus::utility::error::ErrorReporter::instance();

    bool callback_called = false;
    reporter.setErrorCallback([&](const std::string& msg, const std::source_location& loc) {
        callback_called = true;
        EXPECT_FALSE(msg.empty());
    });

    reporter.reportError("test integration error");

    EXPECT_TRUE(callback_called);
    reporter.setErrorCallback(nullptr);  // Reset
}

TEST(IntegrationTest, ThreadTrackerIntegration) {
    auto& tracker = nexus::utility::concurrency::ThreadTracker::instance();

    tracker.registerThread("IntegrationTest");
    EXPECT_EQ(tracker.getActiveThreadCount(), 1);
    EXPECT_EQ(tracker.getCurrentThreadName(), "IntegrationTest");
    tracker.unregisterThread();
    EXPECT_EQ(tracker.getActiveThreadCount(), 0);
}

TEST(IntegrationTest, LoggerAndMetricsIntegration) {
    auto& logger = nexus::utility::logging::Logger::instance();
    logger.setLogLevel(nexus::utility::logging::LogLevel::Debug);
    logger.setTimestamps(false);

    // Verify no-throw on logging
    EXPECT_NO_THROW(NEXUS_LOG_DEBUG("integration test debug message"));
    EXPECT_NO_THROW(NEXUS_LOG_INFO("integration test info message"));
    EXPECT_NO_THROW(NEXUS_LOG_WARN("integration test warning"));
    EXPECT_NO_THROW(NEXUS_LOG_ERROR("integration test error"));

    logger.flush();
}
