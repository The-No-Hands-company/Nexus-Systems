#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

#include "nexus/utility/allocator/arena_allocator.h"
#include "nexus/utility/allocator/pool_allocator.h"
#include "nexus/utility/financial/decimal_precision_validator.h"
#include "nexus/utility/financial/rounding_error_detector.h"
#include "nexus/utility/financial/audit_trail_generator.h"

using namespace nexus::utility::allocator;
using namespace nexus::utility::financial;

// ── ArenaAllocator ──────────────────────────────────────────────────────────

TEST(ArenaAllocatorExtendedTest, AllocatorAlignment) {
    ArenaAllocator arena(4096);
    void* p = arena.allocate(8, 64);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 64, 0);
    arena.reset();
}

TEST(ArenaAllocatorExtendedTest, ResetAndReuse) {
    ArenaAllocator arena(1024);
    void* p1 = arena.allocate(500);
    EXPECT_GT(arena.used(), 0);
    arena.reset();
    EXPECT_EQ(arena.used(), 0);
    void* p2 = arena.allocate(500);
    EXPECT_EQ(p1, p2); // Same address after reset
}

TEST(ArenaAllocatorExtendedTest, ScopedArena) {
    void* before = nullptr;
    {
        ScopedArena arena(1024);
        before = arena.allocate(100);
        EXPECT_NE(before, nullptr);
    }
    // ScopedArena destructor calls reset
}

// ── DecimalPrecisionValidator ───────────────────────────────────────────────

TEST(DecimalPrecisionValidatorTest, ValidatePrecision) {
    auto& v = DecimalPrecisionValidator::instance(); v.initialize();
    v.setPrecision(2);
    EXPECT_TRUE(v.validate(10.50));
    EXPECT_TRUE(v.validate(10.00));
    EXPECT_FALSE(v.validate(10.505));
}

TEST(DecimalPrecisionValidatorTest, RoundToPrecision) {
    auto& v = DecimalPrecisionValidator::instance(); v.initialize();
    v.setPrecision(2);
    EXPECT_DOUBLE_EQ(v.round(10.505), 10.51);
    EXPECT_DOUBLE_EQ(v.round(10.501), 10.50);
}

// ── RoundingErrorDetector ───────────────────────────────────────────────────

TEST(RoundingErrorDetectorTest, AccumulatesError) {
    auto& d = RoundingErrorDetector::instance(); d.initialize();
    d.recordOperation(100.0, 100.05);
    d.recordOperation(200.0, 199.95);
    EXPECT_NEAR(d.getAccumulatedError(), 0.10, 0.01);
    EXPECT_EQ(d.getOperationCount(), 2);
    EXPECT_GT(d.getMaxError(), 0.0);
}

// ── AuditTrailGenerator ─────────────────────────────────────────────────────

TEST(AuditTrailGeneratorTest, LogAndQuery) {
    auto& a = AuditTrailGenerator::instance(); a.initialize();
    a.logEvent("DEPOSIT", "Cash deposit", 1000.00);
    a.logEvent("WITHDRAWAL", "ATM withdrawal", 200.00);
    a.logEvent("DEPOSIT", "Check deposit", 500.00);

    auto entries = a.getEntries();
    EXPECT_EQ(entries.size(), 3);

    auto deposits = a.getEntriesByType("DEPOSIT");
    EXPECT_EQ(deposits.size(), 2);

    EXPECT_NEAR(a.getTotalByType("DEPOSIT"), 1500.00, 0.01);
}
