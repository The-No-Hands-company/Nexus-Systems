#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>

#include "nexus/utility/concurrency/lock_tracker.h"
#include "nexus/utility/concurrency/deadlock_detector.h"
#include "nexus/utility/scheduling/circuit_breaker.h"
#include "nexus/utility/scheduling/rate_limiter.h"
#include "nexus/utility/scheduling/throttle_controller.h"
#include "nexus/utility/event/event_bus.h"
#include "nexus/utility/network/socket_wrapper.h"
#include "nexus/utility/network/url_parser.h"

using namespace nexus::utility::concurrency;
using namespace nexus::utility::scheduling;
using namespace nexus::utility::event;
using namespace nexus::utility::network;

// ── LockTracker Tests ───────────────────────────────────────────────────────

TEST(LockTrackerTest, RecordsLockAcquisition) {
    auto& tracker = LockTracker::instance();

    void* fake_mutex = reinterpret_cast<void*>(0x1000);
    tracker.onBeforeLock(fake_mutex);
    tracker.onAfterLock(fake_mutex);

    auto held = tracker.getSnapshotHeldLocks();
    auto tid = std::this_thread::get_id();
    ASSERT_TRUE(held.count(tid));
    ASSERT_EQ(held[tid].size(), 1);
    EXPECT_EQ(held[tid][0], fake_mutex);

    tracker.onUnlock(fake_mutex);
    held = tracker.getSnapshotHeldLocks();
    EXPECT_FALSE(held.count(tid));
}

TEST(LockTrackerTest, TracksWaitingThreads) {
    auto& tracker = LockTracker::instance();
    void* fake_mutex = reinterpret_cast<void*>(0x2000);

    tracker.onBeforeLock(fake_mutex);
    auto waiting = tracker.getSnapshotWaiting();
    auto tid = std::this_thread::get_id();
    ASSERT_TRUE(waiting.count(tid));
    EXPECT_EQ(waiting[tid], fake_mutex);

    // Cleanup
    tracker.onAfterLock(fake_mutex);
    tracker.onUnlock(fake_mutex);
}

// ── DeadlockDetector Tests ──────────────────────────────────────────────────

TEST(DeadlockDetectorTest, NoDeadlockEmptyState) {
    std::unordered_map<std::thread::id, const void*> waiting;
    std::unordered_map<const void*, std::thread::id> owners;
    EXPECT_FALSE(DeadlockDetector::checkCycle(waiting, owners).has_value());
}

TEST(DeadlockDetectorTest, DetectsSimpleCycle) {
    std::thread::id tid1 = std::this_thread::get_id();
    std::thread::id tid2;
    std::thread t([&]() { tid2 = std::this_thread::get_id(); });
    t.join();

    void* m1 = reinterpret_cast<void*>(1);
    void* m2 = reinterpret_cast<void*>(2);

    // T1 holds m1, waits for m2
    // T2 holds m2, waits for m1
    std::unordered_map<std::thread::id, const void*> waiting;
    waiting[tid1] = m2;
    waiting[tid2] = m1;

    std::unordered_map<const void*, std::thread::id> owners;
    owners[m1] = tid1;
    owners[m2] = tid2;

    auto result = DeadlockDetector::checkCycle(waiting, owners);
    EXPECT_TRUE(result.has_value());
}

TEST(DeadlockDetectorTest, NoCycleWhenNoWait) {
    void* m1 = reinterpret_cast<void*>(1);
    void* m2 = reinterpret_cast<void*>(2);

    std::unordered_map<std::thread::id, const void*> waiting;
    waiting[std::this_thread::get_id()] = m1;

    std::unordered_map<const void*, std::thread::id> owners;
    owners[m2] = std::this_thread::get_id();

    auto result = DeadlockDetector::checkCycle(waiting, owners);
    EXPECT_FALSE(result.has_value());
}

// ── CircuitBreaker Tests ────────────────────────────────────────────────────

TEST(CircuitBreakerTest, StartsClosed) {
    CircuitBreaker cb;
    EXPECT_NO_THROW(cb.execute([]() { return 42; }));
}

TEST(CircuitBreakerTest, TripsAfterFailures) {
    CircuitBreaker::Config cfg;
    cfg.failure_threshold = 3;
    CircuitBreaker cb(cfg);

    for (int i = 0; i < 3; ++i) {
        try {
            cb.execute([]() { throw std::runtime_error("fail"); });
        } catch (const std::runtime_error&) {}
    }

    EXPECT_THROW(cb.execute([]() { return 42; }), CircuitBreaker::CircuitOpenException);
}

TEST(CircuitBreakerTest, ResetAfterTrip) {
    CircuitBreaker::Config cfg;
    cfg.failure_threshold = 1;
    cfg.recovery_timeout = std::chrono::milliseconds(1);
    CircuitBreaker cb(cfg);

    try { cb.execute([]() { throw std::runtime_error("fail"); }); }
    catch (const std::runtime_error&) {}

    // Wait for recovery
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Should be in half-open now
    EXPECT_NO_THROW(cb.execute([]() { return 42; }));
}

// ── RateLimiter Tests ────────────────────────────────────────────────────────

TEST(RateLimiterTest, AllowsWithinLimit) {
    RateLimiter rl(100.0, 10);  // 100 tokens/sec, burst 10
    EXPECT_TRUE(rl.tryConsume(5));
    EXPECT_GE(rl.availableTokens(), 0.0);
}

TEST(RateLimiterTest, DeniesOverLimit) {
    RateLimiter rl(1.0, 2);  // 1 token/sec, burst 2
    EXPECT_TRUE(rl.tryConsume(2));
    EXPECT_FALSE(rl.tryConsume(1));  // No tokens left
}

// ── ThrottleController Tests ─────────────────────────────────────────────────

TEST(ThrottleControllerTest, AllowsWithinWindow) {
    ThrottleController::Config cfg;
    cfg.max_requests = 3;
    ThrottleController tc(cfg);

    EXPECT_TRUE(tc.tryAcquire());
    EXPECT_TRUE(tc.tryAcquire());
    EXPECT_TRUE(tc.tryAcquire());
    EXPECT_FALSE(tc.tryAcquire());
    EXPECT_EQ(tc.currentCount(), 3);
    EXPECT_EQ(tc.remaining(), 0);
}

// ── EventBus Tests ──────────────────────────────────────────────────────────

TEST(EventBusTest, SubscribeAndPublish) {
    EventBus<std::string> bus;
    std::atomic<int> call_count{0};

    auto id = bus.subscribe([&](const std::string&) { call_count.fetch_add(1); });
    bus.publish("hello");
    bus.publish("world");

    EXPECT_EQ(call_count.load(), 2);
    EXPECT_EQ(bus.subscriberCount(), 1);
}

TEST(EventBusTest, Unsubscribe) {
    EventBus<int> bus;
    std::atomic<int> count{0};

    auto id = bus.subscribe([&](int) { count.fetch_add(1); });
    bus.publish(1);
    bus.unsubscribe(id);
    bus.publish(2);

    EXPECT_EQ(count.load(), 1);
    EXPECT_EQ(bus.subscriberCount(), 0);
}

// ── UrlParser Tests ─────────────────────────────────────────────────────────

TEST(UrlParserEncodeTest, ProperHexPadding) {
    // 0x0A should encode as %0A, not %A
    std::string input("\x0A\x0D\x00", 3);
    auto encoded = UrlParser::encode(input);
    // Check that each percent-encoded byte has exactly 2 hex digits
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%') {
            ASSERT_LT(i + 2, encoded.size());
            EXPECT_TRUE(std::isxdigit(static_cast<unsigned char>(encoded[i + 1])));
            EXPECT_TRUE(std::isxdigit(static_cast<unsigned char>(encoded[i + 2])));
            i += 2;
        }
    }
}

TEST(UrlParserEncodeTest, RoundTripPreservesData) {
    std::string original = "Hello World! @#$%^&*()+=[]{}|;:',.<>?/~`";
    auto encoded = UrlParser::encode(original);
    auto decoded = UrlParser::decode(encoded);
    EXPECT_EQ(decoded, original);
}

// ── SocketWrapper Tests ─────────────────────────────────────────────────────

TEST(SocketWrapperTest, ConstructAndMove) {
    SocketWrapper sock(SocketWrapper::Type::TCP);
    EXPECT_TRUE(sock.isValid());

    SocketWrapper moved(std::move(sock));
    EXPECT_TRUE(moved.isValid());
    EXPECT_FALSE(sock.isValid());  // Moved-from should be invalid
}
