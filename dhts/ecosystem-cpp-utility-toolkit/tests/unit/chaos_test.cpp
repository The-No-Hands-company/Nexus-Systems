#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <cmath>

#include "nexus/utility/chaos/cpu_throttler.h"
#include "nexus/utility/chaos/network_fault_injector.h"
#include "nexus/utility/chaos/latency_injector.h"
#include "nexus/utility/chaos/packet_loss_simulator.h"
#include "nexus/utility/chaos/random_crash_injector.h"
#include "nexus/utility/chaos/disk_fault_injector.h"
#include "nexus/utility/chaos/memory_pressure_simulator.h"
#include "nexus/utility/chaos/clock_skew_injector.h"

using namespace nexus::utility::chaos;

// ── CpuThrottler ────────────────────────────────────────────────────────────

TEST(CpuThrottlerTest, StartStop) {
    auto& t = CpuThrottler::instance();
    t.initialize();
    EXPECT_FALSE(t.getUtilization() > 0);
    t.startThrottling(10.0);
    EXPECT_TRUE(t.getUtilization() > 0);
    EXPECT_NEAR(t.getUtilization(), 10.0, 0.1);
    t.stopThrottling();
    EXPECT_FALSE(t.getUtilization() > 0);
    t.shutdown();
}

// ── NetworkFaultInjector ────────────────────────────────────────────────────

TEST(NetworkFaultInjectorTest, BlockRule) {
    auto& n = NetworkFaultInjector::instance();
    n.initialize();
    n.addBlockRule("evil.com", 443);
    EXPECT_TRUE(n.shouldBlock("evil.com", 443));
    EXPECT_FALSE(n.shouldBlock("good.com", 80));
    n.removeRule("evil.com", 443);
    EXPECT_FALSE(n.shouldBlock("evil.com", 443));
    n.shutdown();
}

TEST(NetworkFaultInjectorTest, DelayRule) {
    auto& n = NetworkFaultInjector::instance();
    n.initialize();
    n.addDelayRule("slow.com", 80, 500);
    EXPECT_EQ(n.getDelay("slow.com", 80), 500);
    EXPECT_EQ(n.getDelay("fast.com", 80), 0);
    n.removeRule("slow.com", 80);
    EXPECT_EQ(n.getDelay("slow.com", 80), 0);
    n.shutdown();
}

// ── LatencyInjector ─────────────────────────────────────────────────────────

TEST(LatencyInjectorTest, InjectRemove) {
    auto& l = LatencyInjector::instance();
    l.initialize();
    l.injectLatency("db_query", 100);
    EXPECT_EQ(l.shouldDelay("db_query"), 100);
    EXPECT_EQ(l.shouldDelay("cache_get"), 0);
    l.removeLatency("db_query");
    EXPECT_EQ(l.shouldDelay("db_query"), 0);
    l.shutdown();
}

// ── PacketLossSimulator ─────────────────────────────────────────────────────

TEST(PacketLossSimulatorTest, FullLoss) {
    auto& p = PacketLossSimulator::instance();
    p.initialize();
    p.setLossRate(100.0);
    EXPECT_DOUBLE_EQ(p.getLossRate(), 100.0);
    // With 100% loss, every call should drop
    int drops = 0;
    for (int i = 0; i < 100; ++i) {
        if (p.shouldDrop()) drops++;
    }
    EXPECT_EQ(drops, 100);
    p.shutdown();
}

TEST(PacketLossSimulatorTest, NoLoss) {
    auto& p = PacketLossSimulator::instance();
    p.initialize();
    p.setLossRate(0.0);
    for (int i = 0; i < 100; ++i) {
        EXPECT_FALSE(p.shouldDrop());
    }
    p.shutdown();
}

// ── DiskFaultInjector ───────────────────────────────────────────────────────

TEST(DiskFaultInjectorTest, FaultPath) {
    auto& d = DiskFaultInjector::instance();
    d.initialize();
    d.addFaultPath("/mnt/failing", 100.0);
    EXPECT_TRUE(d.shouldFail("/mnt/failing/data.bin"));
    EXPECT_FALSE(d.shouldFail("/mnt/healthy/data.bin"));
    d.removeFaultPath("/mnt/failing");
    EXPECT_FALSE(d.shouldFail("/mnt/failing/data.bin"));
    d.shutdown();
}

// ── MemoryPressureSimulator ─────────────────────────────────────────────────

TEST(MemoryPressureSimulatorTest, ApplyRelease) {
    auto& m = MemoryPressureSimulator::instance();
    m.initialize();
    EXPECT_EQ(m.getPressureMB(), 0);
    m.applyPressure(10);
    EXPECT_EQ(m.getPressureMB(), 10);
    m.releasePressure();
    EXPECT_EQ(m.getPressureMB(), 0);
    m.shutdown();
}

// ── ClockSkewInjector ───────────────────────────────────────────────────────

TEST(ClockSkewInjectorTest, SkewOffset) {
    auto& c = ClockSkewInjector::instance();
    c.initialize();
    c.setSkew(5000); // 5 seconds
    EXPECT_EQ(c.getSkew(), 5000);
    auto real = std::chrono::system_clock::now();
    auto skewed = c.getSkewedNow();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(skewed - real).count();
    EXPECT_NEAR(diff, 5000, 100); // Within 100ms
    c.setSkew(0);
    EXPECT_EQ(c.getSkew(), 0);
    c.shutdown();
}
