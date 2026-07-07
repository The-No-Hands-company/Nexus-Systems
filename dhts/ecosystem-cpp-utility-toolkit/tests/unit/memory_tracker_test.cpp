#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "nexus/utility/memory/memory_tracker.h"

using namespace nexus::utility::memory;

class MemoryTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        MemoryTracker::instance().reset();
    }

    void TearDown() override {
        MemoryTracker::instance().reset();
    }
};

TEST_F(MemoryTrackerTest, SingletonReturnsSameInstance) {
    auto& a = MemoryTracker::instance();
    auto& b = MemoryTracker::instance();
    EXPECT_EQ(&a, &b);
}

TEST_F(MemoryTrackerTest, TracksAllocationAndDeallocation) {
    auto& tracker = MemoryTracker::instance();
    tracker.setEnabled(true);

    int dummy;
    tracker.recordAllocation(&dummy, 1024, "int");
    auto stats = tracker.getStats();
    EXPECT_EQ(stats.total_allocations, 1);
    EXPECT_EQ(stats.current_allocations, 1);
    EXPECT_EQ(stats.current_bytes, 1024);

    tracker.recordDeallocation(&dummy);
    stats = tracker.getStats();
    EXPECT_EQ(stats.total_deallocations, 1);
    EXPECT_EQ(stats.current_allocations, 0);
    EXPECT_EQ(stats.current_bytes, 0);
}

TEST_F(MemoryTrackerTest, DetectsLeaks) {
    auto& tracker = MemoryTracker::instance();
    tracker.setEnabled(true);

    int leaked;
    int freed;

    tracker.recordAllocation(&leaked, 512, "int");
    tracker.recordAllocation(&freed, 256, "int");
    tracker.recordDeallocation(&freed);

    auto leaks = tracker.detectLeaks();
    EXPECT_EQ(leaks.size(), 1);
    EXPECT_EQ(leaks[0].address, &leaked);
    EXPECT_EQ(leaks[0].size, 512);
}

TEST_F(MemoryTrackerTest, DisabledTrackerIgnoresOperations) {
    auto& tracker = MemoryTracker::instance();
    tracker.setEnabled(false);

    int dummy;
    tracker.recordAllocation(&dummy, 1024, "int");
    auto stats = tracker.getStats();
    EXPECT_EQ(stats.total_allocations, 0);

    tracker.setEnabled(true);
    tracker.recordAllocation(&dummy, 1024, "int");
    stats = tracker.getStats();
    EXPECT_EQ(stats.total_allocations, 1);
}

TEST_F(MemoryTrackerTest, ResetClearsAllState) {
    auto& tracker = MemoryTracker::instance();
    tracker.setEnabled(true);

    int dummy;
    tracker.recordAllocation(&dummy, 1024, "int");
    tracker.reset();

    auto stats = tracker.getStats();
    EXPECT_EQ(stats.total_allocations, 0);
    EXPECT_EQ(stats.current_allocations, 0);
    auto leaks = tracker.detectLeaks();
    EXPECT_TRUE(leaks.empty());
}

TEST_F(MemoryTrackerTest, PeakTracking) {
    auto& tracker = MemoryTracker::instance();
    tracker.setEnabled(true);

    int a, b, c;
    tracker.recordAllocation(&a, 100, "int");  // 100 bytes
    tracker.recordAllocation(&b, 200, "int");  // 300 bytes
    tracker.recordAllocation(&c, 300, "int");  // 600 bytes

    auto stats = tracker.getStats();
    EXPECT_EQ(stats.peak_bytes, 600);
    EXPECT_EQ(stats.peak_allocations, 3);

    tracker.recordDeallocation(&b);
    stats = tracker.getStats();
    EXPECT_EQ(stats.peak_bytes, 600);   // Peak should remain
    EXPECT_EQ(stats.peak_allocations, 3);  // Peak should remain
    EXPECT_EQ(stats.current_bytes, 400);
}

TEST_F(MemoryTrackerTest, MemoryTrackingScope) {
    auto& tracker = MemoryTracker::instance();
    tracker.setEnabled(false);

    {
        MemoryTrackingScope scope(true);
        EXPECT_TRUE(tracker.isEnabled());
    }
    EXPECT_FALSE(tracker.isEnabled());
}

TEST_F(MemoryTrackerTest, NullPointerSkipped) {
    auto& tracker = MemoryTracker::instance();
    tracker.setEnabled(true);

    tracker.recordAllocation(nullptr, 1024, "null");
    auto stats = tracker.getStats();
    EXPECT_EQ(stats.total_allocations, 0);

    tracker.recordDeallocation(nullptr);
    stats = tracker.getStats();
    EXPECT_EQ(stats.total_deallocations, 0);
}

TEST_F(MemoryTrackerTest, PrintReportDoesNotThrow) {
    auto& tracker = MemoryTracker::instance();
    tracker.setEnabled(true);

    int dummy;
    tracker.recordAllocation(&dummy, 1024, "test");
    EXPECT_NO_THROW(tracker.printReport(std::cout));
}
