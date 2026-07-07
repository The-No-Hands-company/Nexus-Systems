#include <gtest/gtest.h>
#include <string>
#include <chrono>
#include <thread>

#include "nexus/utility/scheduling/task_scheduler.h"

using namespace nexus::utility::scheduling;

TEST(TaskSchedulerTest, StartStop) {
    TaskScheduler sched(2);
    EXPECT_FALSE(sched.isRunning());

    sched.start();
    EXPECT_TRUE(sched.isRunning());

    sched.stop();
    EXPECT_FALSE(sched.isRunning());
}

TEST(TaskSchedulerTest, ScheduleAndExecute) {
    TaskScheduler sched(2);
    sched.start();

    std::atomic<int> counter{0};
    sched.schedule([&counter]() { counter.fetch_add(1); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sched.stop();

    EXPECT_EQ(counter.load(), 1);
}

TEST(TaskSchedulerTest, ScheduleDelayed) {
    TaskScheduler sched(2);
    sched.start();

    std::atomic<int> counter{0};
    sched.scheduleDelayed([&counter]() { counter.fetch_add(1); },
                           std::chrono::milliseconds(10));

    EXPECT_EQ(counter.load(), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sched.stop();

    EXPECT_EQ(counter.load(), 1);
}

TEST(TaskSchedulerTest, SchedulePeriodic) {
    TaskScheduler sched(2);
    sched.start();

    std::atomic<int> counter{0};
    sched.schedulePeriodic([&counter]() { counter.fetch_add(1); },
                            std::chrono::milliseconds(15));

    std::this_thread::sleep_for(std::chrono::milliseconds(45));
    sched.stop();

    EXPECT_GE(counter.load(), 2);
}

TEST(TaskSchedulerTest, PendingCount) {
    TaskScheduler sched(1);
    sched.start();

    EXPECT_EQ(sched.pendingCount(), 0);

    sched.schedule([]() { std::this_thread::sleep_for(std::chrono::milliseconds(20)); });
    sched.schedule([]() { std::this_thread::sleep_for(std::chrono::milliseconds(20)); });

    // One should be picked up by the worker thread
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    sched.stop();
}
