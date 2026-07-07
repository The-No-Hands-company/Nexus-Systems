#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

#include "nexus/utility/allocator/pool_allocator.h"

using namespace nexus::utility::allocator;

struct TestObject {
    int value;
    std::string name;
    bool active = false;

    TestObject() : value(0) {}
    TestObject(int v, std::string n) : value(v), name(std::move(n)), active(true) {}
    ~TestObject() { active = false; }
};

// ── PoolAllocator Tests ─────────────────────────────────────────────────────

TEST(PoolAllocatorTest, AllocateDeallocate) {
    PoolAllocator<TestObject> pool(10);

    auto* obj = pool.allocate();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.allocated(), 1);
    EXPECT_EQ(pool.available(), 9);

    pool.deallocate(obj);
    EXPECT_EQ(pool.allocated(), 0);
    EXPECT_EQ(pool.available(), 10);
}

TEST(PoolAllocatorTest, ConstructDestroy) {
    PoolAllocator<TestObject> pool(5);

    auto* obj = pool.construct(42, "test");
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 42);
    EXPECT_EQ(obj->name, "test");
    EXPECT_TRUE(obj->active);

    pool.destroy(obj);
    EXPECT_EQ(pool.allocated(), 0);
    EXPECT_FALSE(obj->active);  // Destructor ran
}

TEST(PoolAllocatorTest, Exhaustion) {
    PoolAllocator<TestObject> pool(3);

    auto* a = pool.allocate();
    auto* b = pool.allocate();
    auto* c = pool.allocate();
    auto* d = pool.allocate();  // Should be nullptr

    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_NE(c, nullptr);
    EXPECT_EQ(d, nullptr);
    EXPECT_TRUE(pool.full());

    pool.deallocate(b);
    auto* e = pool.allocate();  // Reuses b's slot
    EXPECT_EQ(e, b);
}

TEST(PoolAllocatorTest, ReuseAfterFree) {
    PoolAllocator<TestObject> pool(2);

    auto* a = pool.allocate();
    pool.deallocate(a);
    auto* b = pool.allocate();

    EXPECT_EQ(a, b);  // Same address reused
}

TEST(PoolAllocatorTest, ZeroCountThrows) {
    EXPECT_THROW(PoolAllocator<TestObject>(0), std::invalid_argument);
}

TEST(PoolAllocatorTest, CapacityQueries) {
    PoolAllocator<TestObject> pool(100);
    EXPECT_EQ(pool.capacity(), 100);
    EXPECT_EQ(pool.available(), 100);
    EXPECT_TRUE(pool.empty());
    EXPECT_FALSE(pool.full());

    pool.allocate();
    EXPECT_EQ(pool.allocated(), 1);
    EXPECT_NEAR(pool.utilization(), 0.01, 0.001);
}

TEST(PoolAllocatorTest, ThreadSafety) {
    PoolAllocator<TestObject> pool(1000);
    std::atomic<int> errors{0};
    constexpr int kThreads = 4;
    constexpr int kOpsPerThread = 200;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&pool, &errors]() {
            std::vector<TestObject*> allocated;
            for (int i = 0; i < kOpsPerThread; ++i) {
                auto* obj = pool.allocate();
                if (obj) {
                    allocated.push_back(obj);
                }
                if (allocated.size() > 50) {
                    pool.deallocate(allocated.back());
                    allocated.pop_back();
                }
            }
            for (auto* obj : allocated) {
                pool.deallocate(obj);
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(pool.allocated(), 0);  // All returned
}

TEST(PoolAllocatorTest, MoveSemantics) {
    PoolAllocator<TestObject> pool_a(10);
    auto* obj = pool_a.allocate();
    EXPECT_EQ(pool_a.allocated(), 1);

    PoolAllocator<TestObject> pool_b(std::move(pool_a));
    EXPECT_EQ(pool_b.allocated(), 1);
    EXPECT_EQ(pool_a.capacity(), 0);  // Moved-from

    pool_b.deallocate(obj);
    EXPECT_EQ(pool_b.allocated(), 0);
}

// ── UnsafePoolAllocator Tests ───────────────────────────────────────────────

TEST(UnsafePoolAllocatorTest, BasicOperations) {
    UnsafePoolAllocator<TestObject> pool(5);

    auto* a = pool.construct(1, "a");
    auto* b = pool.construct(2, "b");

    EXPECT_EQ(a->value, 1);
    EXPECT_EQ(b->value, 2);
    EXPECT_EQ(pool.allocated(), 2);

    pool.destroy(a);
    EXPECT_EQ(pool.allocated(), 1);

    auto* c = pool.construct(3, "c");
    EXPECT_EQ(c, a);  // Reuses a's slot
    EXPECT_EQ(c->value, 3);
}
