#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

#include "nexus/utility/containers/bloom_filter.h"
#include "nexus/utility/containers/ring_buffer.h"
#include "nexus/utility/containers/thread_safe_queue.h"

using namespace nexus::utility::containers;

TEST(BloomFilterTest, InsertAndContains) {
    BloomFilter<std::string> filter(1000, 0.01);

    filter.insert("hello");
    filter.insert("world");

    EXPECT_TRUE(filter.contains("hello"));
    EXPECT_TRUE(filter.contains("world"));
    EXPECT_FALSE(filter.contains("nonexistent"));
}

TEST(BloomFilterTest, ClearResets) {
    BloomFilter<std::string> filter(1000, 0.01);
    filter.insert("test");
    EXPECT_TRUE(filter.contains("test"));
    filter.clear();
    EXPECT_FALSE(filter.contains("test"));
}

TEST(BloomFilterTest, SizeAndHashCount) {
    BloomFilter<std::string> filter(1000, 0.01);
    EXPECT_GT(filter.size(), 0);
    EXPECT_GT(filter.hashCount(), 0);
}

TEST(RingBufferTest, PushPop) {
    RingBuffer<int> rb(4);

    EXPECT_TRUE(rb.empty());
    EXPECT_FALSE(rb.isFull());

    rb.push(1);
    rb.push(2);
    rb.push(3);

    EXPECT_EQ(rb.size(), 3);
    EXPECT_EQ(rb.capacity(), 4);

    EXPECT_EQ(rb.pop(), 1);
    EXPECT_EQ(rb.pop(), 2);
    EXPECT_EQ(rb.pop(), 3);
    EXPECT_TRUE(rb.empty());
}

TEST(RingBufferTest, FullOverwrite) {
    RingBuffer<int> rb(3);
    rb.push(1);
    rb.push(2);
    rb.push(3);
    EXPECT_TRUE(rb.isFull());

    rb.push(4);  // Overwrites oldest

    EXPECT_EQ(rb.size(), 3);
    EXPECT_EQ(rb.pop(), 2);
    EXPECT_EQ(rb.pop(), 3);
    EXPECT_EQ(rb.pop(), 4);
}

TEST(RingBufferTest, IndexOperator) {
    RingBuffer<int> rb(3);
    rb.push(10);
    rb.push(20);
    rb.push(30);

    EXPECT_EQ(rb[0], 10);
    EXPECT_EQ(rb[1], 20);
    EXPECT_EQ(rb[2], 30);
}

TEST(ThreadSafeQueueTest, PushPop) {
    ThreadSafeQueue<int> q;

    q.push(42);
    q.push(100);

    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 2);

    auto val1 = q.tryPop();
    ASSERT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, 42);

    auto val2 = q.tryPop();
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, 100);

    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.tryPop().has_value());
}

TEST(ThreadSafeQueueTest, ConcurrentPush) {
    ThreadSafeQueue<int> q;
    constexpr int kItemsPerThread = 500;
    constexpr int kThreads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&q, t]() {
            for (int i = 0; i < kItemsPerThread; ++i) {
                q.push(t * kItemsPerThread + i);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(q.size(), kItemsPerThread * kThreads);
}
