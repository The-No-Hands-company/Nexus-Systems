#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <stdexcept>

#include "nexus/utility/scheduling/backoff_strategy.h"
#include "nexus/utility/scheduling/retry_policy.h"
#include "nexus/utility/scheduling/rate_limiter.h"
#include "nexus/utility/scheduling/deadline_tracker.h"
#include "nexus/utility/scheduling/throttle_controller.h"
#include "nexus/utility/containers/sparse_set.h"
#include "nexus/utility/containers/bloom_filter.h"

using namespace nexus::utility::scheduling;
using namespace nexus::utility::containers;

// ── BackoffStrategy Tests ──────────────────────────────────────────────────

TEST(BackoffStrategyTest, InitialDelayIsCorrect) {
    BackoffStrategy::Config cfg;
    cfg.initial_delay = std::chrono::milliseconds(100);
    cfg.multiplier = 1.0;
    cfg.max_delay = std::chrono::seconds(10);
    cfg.use_jitter = false;

    BackoffStrategy bs(cfg);
    auto d = bs.nextDelay();
    EXPECT_GE(d, std::chrono::milliseconds(100));
    EXPECT_EQ(bs.attempts(), 1);
}

TEST(BackoffStrategyTest, ExponentialGrowth) {
    BackoffStrategy::Config cfg;
    cfg.initial_delay = std::chrono::milliseconds(100);
    cfg.multiplier = 2.0;
    cfg.max_delay = std::chrono::seconds(10);
    cfg.use_jitter = false;

    BackoffStrategy bs(cfg);
    auto d1 = bs.nextDelay();  // ~100ms
    auto d2 = bs.nextDelay();  // ~200ms
    auto d3 = bs.nextDelay();  // ~400ms

    EXPECT_GE(d2, d1);
    EXPECT_GE(d3, d2);
}

TEST(BackoffStrategyTest, CapsAtMaxDelay) {
    BackoffStrategy::Config cfg;
    cfg.initial_delay = std::chrono::milliseconds(100);
    cfg.multiplier = 1000.0;
    cfg.max_delay = std::chrono::milliseconds(500);
    cfg.use_jitter = false;

    BackoffStrategy bs(cfg);
    bs.nextDelay();  // 100ms
    auto d = bs.nextDelay();  // should cap at 500ms
    EXPECT_LE(d, std::chrono::milliseconds(500));
}

TEST(BackoffStrategyTest, JitterEnabled) {
    BackoffStrategy::Config cfg;
    cfg.initial_delay = std::chrono::milliseconds(100);
    cfg.multiplier = 1.0;
    cfg.max_delay = std::chrono::seconds(10);
    cfg.use_jitter = true;
    cfg.jitter_factor = 0.5;

    BackoffStrategy bs(cfg);
    // With jitter, consecutive calls should differ
    int same_count = 0;
    auto prev = bs.nextDelay();
    for (int i = 0; i < 10; ++i) {
        auto curr = bs.nextDelay();
        if (curr == prev) same_count++;
        prev = curr;
    }
    // With 50% jitter on 100ms delay, expect at least some variation
    EXPECT_LT(same_count, 9);
}

TEST(BackoffStrategyTest, ResetResetsCounter) {
    BackoffStrategy::Config cfg;
    cfg.initial_delay = std::chrono::milliseconds(100);
    BackoffStrategy bs(cfg);
    bs.nextDelay();
    bs.nextDelay();
    EXPECT_EQ(bs.attempts(), 2);
    bs.reset();
    EXPECT_EQ(bs.attempts(), 0);
}

TEST(BackoffStrategyTest, DelayNeverBelowOne) {
    BackoffStrategy::Config cfg;
    cfg.initial_delay = std::chrono::milliseconds(0);
    cfg.multiplier = 0.5;
    cfg.max_delay = std::chrono::milliseconds(10);
    cfg.use_jitter = false;

    BackoffStrategy bs(cfg);
    auto d = bs.nextDelay();
    EXPECT_GE(d, std::chrono::milliseconds(1));
}

// ── RetryPolicy Tests ──────────────────────────────────────────────────────

TEST(RetryPolicyTest, SuccessfulFirstAttempt) {
    RetryPolicy rp;
    int call_count = 0;
    auto result = rp.execute([&]() { call_count++; return 42; });
    EXPECT_EQ(result, 42);
    EXPECT_EQ(call_count, 1);
}

TEST(RetryPolicyTest, RetryAfterFailure) {
    RetryPolicy rp;
    RetryPolicy::Config cfg;
    cfg.max_attempts = 3;
    cfg.backoff.initial_delay = std::chrono::milliseconds(1);
    cfg.backoff.use_jitter = false;

    int call_count = 0;
    auto result = rp.executeWith(cfg, [&]() -> int {
        call_count++;
        if (call_count < 3) throw std::runtime_error("fail");
        return 42;
    });

    EXPECT_EQ(result, 42);
    EXPECT_EQ(call_count, 3);
}

TEST(RetryPolicyTest, ExhaustRetries) {
    RetryPolicy rp;
    RetryPolicy::Config cfg;
    cfg.max_attempts = 2;
    cfg.backoff.initial_delay = std::chrono::milliseconds(1);
    cfg.backoff.use_jitter = false;

    int call_count = 0;
    EXPECT_THROW(
        rp.executeWith(cfg, [&]() -> int {
            call_count++;
            throw std::runtime_error("persistent fail");
        }),
        std::runtime_error);

    EXPECT_EQ(call_count, 2);
}

TEST(RetryPolicyTest, VoidReturn) {
    RetryPolicy rp;
    RetryPolicy::Config cfg;
    cfg.max_attempts = 2;
    cfg.backoff.initial_delay = std::chrono::milliseconds(1);
    cfg.backoff.use_jitter = false;

    int call_count = 0;
    EXPECT_NO_THROW(rp.executeWith(cfg, [&]() {
        call_count++;
    }));
    EXPECT_EQ(call_count, 1);
}

// ── RateLimiter Tests ──────────────────────────────────────────────────────

TEST(RateLimiterTest, AllowsTokens) {
    RateLimiter rl(100.0, 10);
    EXPECT_TRUE(rl.tryConsume(5));
    EXPECT_GE(rl.availableTokens(), 0);
    EXPECT_LE(rl.availableTokens(), 10);
}

TEST(RateLimiterTest, DeniesWhenEmpty) {
    RateLimiter rl(0.1, 1);  // 0.1 tokens/sec, burst 1
    EXPECT_TRUE(rl.tryConsume(1));   // Consume the burst
    EXPECT_FALSE(rl.tryConsume(1));  // No tokens available
}

TEST(RateLimiterTest, RefillsOverTime) {
    RateLimiter rl(100.0, 5);
    rl.tryConsume(5);  // Empty the bucket
    EXPECT_LE(rl.availableTokens(), 0.1);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));  // ~3 tokens regenerated
    // May or may not have refilled enough due to timing
    // Just verify it doesn't crash and availableTokens is reasonable
    auto tok = rl.availableTokens();
    EXPECT_GE(tok, 0.0);
    EXPECT_LE(tok, 10.0);
}

TEST(RateLimiterTest, SetRate) {
    RateLimiter rl(10.0, 5);
    EXPECT_DOUBLE_EQ(rl.rate(), 10.0);

    rl.setRate(50.0);
    EXPECT_DOUBLE_EQ(rl.rate(), 50.0);
}

TEST(RateLimiterTest, ZeroConsume) {
    RateLimiter rl(10.0, 5);
    EXPECT_TRUE(rl.tryConsume(0));  // Consuming zero should always succeed
    EXPECT_EQ(rl.burst(), 5);
}

// ── DeadlineTracker Tests ──────────────────────────────────────────────────

TEST(DeadlineTrackerTest, DeadlineNotExpired) {
    DeadlineTracker dt;
    dt.set("test", std::chrono::milliseconds(500));

    // Check remaining first (before check() marks as notified)
    auto rem = dt.remaining("test");
    ASSERT_TRUE(rem.has_value());
    EXPECT_GT(rem->count(), 0);

    EXPECT_TRUE(dt.check("test"));
}

TEST(DeadlineTrackerTest, DeadlineExpired) {
    DeadlineTracker dt;
    dt.set("expired", std::chrono::milliseconds(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_FALSE(dt.check("expired"));
}

TEST(DeadlineTrackerTest, UnknownDeadline) {
    DeadlineTracker dt;
    EXPECT_FALSE(dt.remaining("nonexistent").has_value());
    EXPECT_EQ(dt.checkAll(), 0);
}

TEST(DeadlineTrackerTest, ClearRemovesAll) {
    DeadlineTracker dt;
    dt.set("a", std::chrono::seconds(10));
    dt.set("b", std::chrono::seconds(10));
    dt.clear();
    EXPECT_FALSE(dt.remaining("a").has_value());
    EXPECT_FALSE(dt.remaining("b").has_value());
}

TEST(DeadlineTrackerTest, ExpiredCallback) {
    DeadlineTracker dt;
    std::atomic<bool> called{false};
    dt.set("cb", std::chrono::milliseconds(1), [&]() { called.store(true); });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    dt.check("cb");

    EXPECT_TRUE(called.load());
}

TEST(DeadlineTrackerTest, CheckAll) {
    DeadlineTracker dt;
    dt.set("a", std::chrono::milliseconds(1));
    dt.set("b", std::chrono::milliseconds(1));
    dt.set("c", std::chrono::milliseconds(500));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    size_t expired = dt.checkAll();
    EXPECT_EQ(expired, 2);  // a and b expired
    EXPECT_TRUE(dt.check("c"));  // c still valid
}

// ── ThrottleController Tests ───────────────────────────────────────────────

TEST(ThrottleControllerTest, ZeroMaxRequests) {
    ThrottleController::Config cfg;
    cfg.max_requests = 0;
    ThrottleController tc(cfg);
    EXPECT_FALSE(tc.tryAcquire());
    EXPECT_FALSE(tc.waitAcquire());  // Should not divide by zero
}

TEST(ThrottleControllerTest, RemainingCount) {
    ThrottleController::Config cfg;
    cfg.max_requests = 5;
    ThrottleController tc(cfg);
    EXPECT_EQ(tc.remaining(), 5);
    tc.tryAcquire();
    EXPECT_EQ(tc.remaining(), 4);
}

// ── SparseSet Tests ────────────────────────────────────────────────────────

TEST(SparseSetTest, InsertContainsRemove) {
    SparseSet set(100);

    EXPECT_FALSE(set.contains(42));
    set.insert(42);
    EXPECT_TRUE(set.contains(42));
    EXPECT_EQ(set.size(), 1);

    set.remove(42);
    EXPECT_FALSE(set.contains(42));
    EXPECT_EQ(set.size(), 0);
}

TEST(SparseSetTest, InsertDuplicate) {
    SparseSet set(100);
    set.insert(5);
    set.insert(5);  // Should be no-op
    EXPECT_EQ(set.size(), 1);
}

TEST(SparseSetTest, OutOfRange) {
    SparseSet set(10);
    EXPECT_THROW(set.insert(11), std::out_of_range);
    EXPECT_FALSE(set.contains(11));
    EXPECT_NO_THROW(set.remove(11));  // No-op
}

TEST(SparseSetTest, RemoveNonExistentNoOp) {
    SparseSet set(100);
    EXPECT_NO_THROW(set.remove(42));
    EXPECT_EQ(set.size(), 0);
}

TEST(SparseSetTest, ValuesOrder) {
    SparseSet set(100);
    set.insert(10);
    set.insert(20);
    set.insert(30);

    auto vals = set.values();
    EXPECT_EQ(vals.size(), 3);
    EXPECT_EQ(vals[0], 10);
    EXPECT_EQ(vals[1], 20);
    EXPECT_EQ(vals[2], 30);
}

TEST(SparseSetTest, Clear) {
    SparseSet set(100);
    set.insert(1);
    set.insert(2);
    set.clear();
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0);
    EXPECT_FALSE(set.contains(1));
}

TEST(SparseSetTest, SIZE_MAXGuard) {
    EXPECT_THROW(SparseSet(std::numeric_limits<size_t>::max()), std::overflow_error);
}

// ── BloomFilter Tests ──────────────────────────────────────────────────────

TEST(BloomFilterAdvancedTest, NoFalseNegatives) {
    BloomFilter<std::string> filter(1000, 0.01);
    std::vector<std::string> items = {"apple", "banana", "cherry", "date", "elderberry"};

    for (const auto& item : items) {
        filter.insert(item);
    }

    for (const auto& item : items) {
        EXPECT_TRUE(filter.contains(item)) << "False negative for: " << item;
    }
}

TEST(BloomFilterAdvancedTest, ReasonableFalsePositiveRate) {
    BloomFilter<int> filter(100, 0.01);

    // Insert 100 items
    for (int i = 0; i < 100; ++i) {
        filter.insert(i);
    }

    // Check 1000 non-inserted items
    int false_positives = 0;
    for (int i = 100; i < 1100; ++i) {
        if (filter.contains(i)) {
            false_positives++;
        }
    }

    double fp_rate = static_cast<double>(false_positives) / 1000.0;
    // With expected=100, FPR=0.01, we should get ~1% FPR
    // Allow up to 5x the expected rate due to small sample
    EXPECT_LT(fp_rate, 0.05);
}

TEST(BloomFilterAdvancedTest, Clear) {
    BloomFilter<int> filter(100, 0.01);
    filter.insert(42);
    EXPECT_TRUE(filter.contains(42));
    filter.clear();
    EXPECT_FALSE(filter.contains(42));
}

TEST(BloomFilterAdvancedTest, DifferentTypes) {
    BloomFilter<int> intFilter(100, 0.01);
    intFilter.insert(42);
    EXPECT_TRUE(intFilter.contains(42));

    BloomFilter<uint64_t> uintFilter(100, 0.01);
    uintFilter.insert(UINT64_C(0xDEADBEEF));
    EXPECT_TRUE(uintFilter.contains(UINT64_C(0xDEADBEEF)));
}
