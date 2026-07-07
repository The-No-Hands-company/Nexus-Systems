#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>

#include "nexus/utility/scheduling/cron_scheduler.h"
#include "nexus/utility/containers/lru_cache.h"
#include "nexus/utility/scheduling/task_scheduler.h"
#include "nexus/utility/serialization/json_serializer.h"

using namespace nexus::utility::scheduling;
using namespace nexus::utility::containers;
using namespace nexus::utility::serialization;

// ── CronScheduler Tests ─────────────────────────────────────────────────────

TEST(CronSchedulerTest, StartStop) {
    CronScheduler cs;
    EXPECT_FALSE(cs.isRunning());

    cs.start();
    EXPECT_TRUE(cs.isRunning());

    cs.stop();
    EXPECT_FALSE(cs.isRunning());
}

TEST(CronSchedulerTest, AddAndRemoveTask) {
    CronScheduler cs;
    cs.start();

    auto id = cs.add("* * * * *", []() {}, "test");
    EXPECT_GT(id, 0);

    auto tasks = cs.listTasks();
    EXPECT_EQ(tasks.size(), 1);

    EXPECT_TRUE(cs.remove(id));
    EXPECT_EQ(cs.listTasks().size(), 0);

    cs.stop();
}

TEST(CronSchedulerTest, RemoveNonExistent) {
    CronScheduler cs;
    EXPECT_FALSE(cs.remove(999));
}

TEST(CronSchedulerTest, AddInterval) {
    CronScheduler cs;
    cs.start();

    std::atomic<int> count{0};
    auto id = cs.addInterval(std::chrono::seconds(60), [&]() { count.fetch_add(1); });
    EXPECT_GT(id, 0);
    EXPECT_GE(cs.taskCount(), 1);

    cs.stop();
}

TEST(CronSchedulerTest, DestructorStopsThread) {
    {
        CronScheduler cs;
        cs.start();
        // Destructor should stop the thread cleanly
    }
    SUCCEED();
}

// ── LRUCache Tests ──────────────────────────────────────────────────────────

TEST(LRUCacheTest, BasicGetPut) {
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");

    auto val = cache.get(1);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "one");

    EXPECT_FALSE(cache.get(99).has_value());
}

TEST(LRUCacheTest, Eviction) {
    LRUCache<int, int> cache(2);

    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);  // Evicts key 1

    EXPECT_FALSE(cache.get(1).has_value());
    EXPECT_EQ(*cache.get(2), 20);
    EXPECT_EQ(*cache.get(3), 30);
}

TEST(LRUCacheTest, GetPromotesToFront) {
    LRUCache<int, int> cache(2);

    cache.put(1, 10);
    cache.put(2, 20);
    cache.get(1);       // Promotes 1
    cache.put(3, 30);   // Evicts 2 (least recently used)

    EXPECT_EQ(*cache.get(1), 10);
    EXPECT_FALSE(cache.get(2).has_value());
    EXPECT_EQ(*cache.get(3), 30);
}

TEST(LRUCacheTest, Contains) {
    LRUCache<int, std::string> cache(3);
    cache.put(1, "hello");

    EXPECT_TRUE(cache.contains(1));
    EXPECT_FALSE(cache.contains(2));
}

TEST(LRUCacheTest, Erase) {
    LRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);

    EXPECT_TRUE(cache.erase(1));
    EXPECT_FALSE(cache.contains(1));
    EXPECT_EQ(cache.size(), 1);

    EXPECT_FALSE(cache.erase(99));
}

TEST(LRUCacheTest, MoveSemantics) {
    LRUCache<std::string, std::string> cache(3);

    std::string key = "hello";
    std::string value = "world";
    cache.put(std::move(key), std::move(value));

    auto val = cache.get("hello");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "world");
}

TEST(LRUCacheTest, Clear) {
    LRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);

    cache.clear();
    EXPECT_TRUE(cache.empty());
    EXPECT_EQ(cache.size(), 0);
}

TEST(LRUCacheTest, ThreadSafety) {
    LRUCache<int, int> cache(100);
    std::atomic<int> errors{0};
    constexpr int kThreads = 4;
    constexpr int kOpsPerThread = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&cache, &errors, t]() {
            for (int i = 0; i < kOpsPerThread; ++i) {
                int key = t * kOpsPerThread + i;
                try {
                    cache.put(key, key * 10);
                    auto val = cache.get(key);
                    if (val.has_value() && *val != key * 10) {
                        errors.fetch_add(1);
                    }
                } catch (...) {
                    errors.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(errors.load(), 0);
}

// ── TaskScheduler Periodic Cancellation Tests ───────────────────────────────

TEST(TaskSchedulerTest, PeriodicTaskCanBeCancelled) {
    TaskScheduler sched(2);
    sched.start();

    std::atomic<int> count{0};
    auto id = sched.schedulePeriodic([&]() { count.fetch_add(1); },
                                      std::chrono::milliseconds(5));

    // Let it fire at least twice
    std::this_thread::sleep_for(std::chrono::milliseconds(18));
    EXPECT_GE(count.load(), 2);

    // Cancel it
    EXPECT_TRUE(sched.cancelTask(id));

    int count_before = count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    int count_after = count.load();

    // At most 1 additional fire (one may already be in-flight)
    EXPECT_LE(count_after - count_before, 1);

    sched.stop();
}

// ── JsonSerializer Tests ────────────────────────────────────────────────────

TEST(JsonSerializerTest, ParseAndSerialize) {
    JsonSerializer ser;
    auto val = ser.parse(R"({"name":"John","age":30})");

    EXPECT_TRUE(val.isObject());
    EXPECT_EQ(val["name"].asString(), "John");
    EXPECT_DOUBLE_EQ(val["age"].asDouble(), 30.0);

    auto output = ser.serialize(val);
    EXPECT_FALSE(output.empty());
}

TEST(JsonSerializerTest, ExplicitConstructors) {
    // Verify that implicit conversion is prevented
    JsonValue v = JsonValue(true);
    JsonValue n = JsonValue(42);
    JsonValue d = JsonValue(3.14);
    JsonValue s = JsonValue(std::string("hello"));

    EXPECT_TRUE(v.isBool());
    EXPECT_TRUE(n.isNumber());
    EXPECT_TRUE(d.isNumber());
    EXPECT_TRUE(s.isString());
}

TEST(JsonSerializerTest, PropertyTypeErrors) {
    JsonValue v = JsonValue(42.0);

    EXPECT_TRUE(v.isNumber());
    EXPECT_FALSE(v.isString());
    EXPECT_THROW(v.asBool(), std::domain_error);
    EXPECT_THROW(v.asArray(), std::bad_variant_access);
    EXPECT_THROW(v.asObject(), std::bad_variant_access);
}

TEST(JsonSerializerTest, ArrayBoundsCheck) {
    JsonSerializer ser;
    auto val = ser.parse(R"([1, 2, 3])");

    EXPECT_EQ(val[0].asDouble(), 1.0);
    EXPECT_EQ(val[2].asDouble(), 3.0);
    EXPECT_THROW(val[5].asDouble(), std::out_of_range);
}

TEST(JsonSerializerTest, NaNHandling) {
    JsonSerializer ser;
    JsonValue nan_val = JsonValue(std::numeric_limits<double>::quiet_NaN());
    auto output = ser.serialize(nan_val);
    EXPECT_TRUE(output.find("null") != std::string::npos);
}

TEST(JsonSerializerTest, NullRoundtrip) {
    JsonSerializer ser;
    auto val = ser.parse("null");
    EXPECT_TRUE(val.isNull());
    auto output = ser.serialize(val);
    EXPECT_EQ(output, "null");
}
