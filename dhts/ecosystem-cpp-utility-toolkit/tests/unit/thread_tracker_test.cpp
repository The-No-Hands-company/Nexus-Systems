#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "nexus/utility/concurrency/thread_tracker.h"

using namespace nexus::utility::concurrency;

class ThreadTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ThreadTrackerTest, SingletonReturnsSameInstance) {
    auto& a = ThreadTracker::instance();
    auto& b = ThreadTracker::instance();
    EXPECT_EQ(&a, &b);
}

TEST_F(ThreadTrackerTest, RegisterAndGetThreadName) {
    auto& tracker = ThreadTracker::instance();
    auto id = std::this_thread::get_id();

    tracker.registerThread("TestThread");
    auto name = tracker.getThreadName(id);
    EXPECT_TRUE(name.has_value());
    EXPECT_EQ(*name, "TestThread");

    tracker.unregisterThread();
    auto name2 = tracker.getThreadName(id);
    EXPECT_FALSE(name2.has_value());
}

TEST_F(ThreadTrackerTest, GetCurrentThreadName) {
    auto& tracker = ThreadTracker::instance();

    tracker.registerThread("CurrentTest");
    auto name = tracker.getCurrentThreadName();
    EXPECT_EQ(name, "CurrentTest");

    tracker.unregisterThread();
}

TEST_F(ThreadTrackerTest, ActiveThreadCount) {
    auto& tracker = ThreadTracker::instance();

    EXPECT_EQ(tracker.getActiveThreadCount(), 0);

    tracker.registerThread("ThreadA");
    EXPECT_EQ(tracker.getActiveThreadCount(), 1);

    tracker.registerThread("ThreadB");  // Same thread, overrides name
    EXPECT_EQ(tracker.getActiveThreadCount(), 1);

    tracker.unregisterThread();
    EXPECT_EQ(tracker.getActiveThreadCount(), 0);
}

TEST_F(ThreadTrackerTest, PrintReportDoesNotThrow) {
    auto& tracker = ThreadTracker::instance();
    tracker.registerThread("ReporterThread");
    EXPECT_NO_THROW(tracker.printReport(std::cout));
    tracker.unregisterThread();
}

TEST_F(ThreadTrackerTest, ScopedThreadRegistration) {
    auto& tracker = ThreadTracker::instance();

    {
        ScopedThreadRegistration reg("ScopedTest");
        EXPECT_EQ(tracker.getActiveThreadCount(), 1);
        auto name = tracker.getCurrentThreadName();
        EXPECT_EQ(name, "ScopedTest");
    }

    EXPECT_EQ(tracker.getActiveThreadCount(), 0);
}

TEST_F(ThreadTrackerTest, NonRegisteredThreadReturnsNoName) {
    auto& tracker = ThreadTracker::instance();
    auto id = std::this_thread::get_id();
    auto name = tracker.getThreadName(id);
    EXPECT_FALSE(name.has_value());
}

TEST_F(ThreadTrackerTest, TrackedThreadAutoRegisters) {
    auto& tracker = ThreadTracker::instance();

    std::atomic<bool> started{false};
    std::atomic<bool> registered{false};

    TrackedThread t("TrackedWorker", [&]() {
        started.store(true);
        auto name = tracker.getCurrentThreadName();
        if (name == "TrackedWorker") {
            registered.store(true);
        }
    });

    t.join();

    EXPECT_TRUE(started.load());
    EXPECT_TRUE(registered.load());
    EXPECT_EQ(tracker.getActiveThreadCount(), 0);
}
