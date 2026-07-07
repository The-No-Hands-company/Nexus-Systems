#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <sstream>

#include "nexus/utility/cli/ini_reader.h"
#include "nexus/utility/allocator/pool_allocator.h"
#include "nexus/utility/containers/lru_cache.h"
#include "nexus/utility/concurrency/concurrency_primitives.h"

using namespace nexus::utility::cli;
using namespace nexus::utility::allocator;
using namespace nexus::utility::containers;
using namespace nexus::utility::concurrency;

// ── IniReader Tests ────────────────────────────────────────────────────────

TEST(IniReaderTest, ParseSimpleIni) {
    IniReader ini;
    ASSERT_TRUE(ini.parse(R"(
[server]
host = localhost
port = 8080
debug = true

[database]
url = postgres://localhost:5432/db
max_connections = 20
)"));
    EXPECT_EQ(ini.get<std::string>("server", "host"), "localhost");
    EXPECT_EQ(ini.get<int>("server", "port"), 8080);
    EXPECT_EQ(ini.get<bool>("server", "debug"), true);
    EXPECT_EQ(ini.get<std::string>("database", "url"), "postgres://localhost:5432/db");
    EXPECT_EQ(ini.get<int>("database", "max_connections"), 20);
}

TEST(IniReaderTest, DefaultValues) {
    IniReader ini;
    ini.parse("[app]\nname = test\n");
    EXPECT_EQ(ini.get<std::string>("app", "name"), "test");
    EXPECT_EQ(ini.get<int>("app", "missing"), 0);
    EXPECT_EQ(ini.get<std::string>("app", "missing", "default"), "default");
    EXPECT_EQ(ini.get<int>("missing_section", "key", 42), 42);
}

TEST(IniReaderTest, BoolVariants) {
    IniReader ini;
    ini.parse("[flags]\na=true\nb=yes\nc=1\nd=on\ne=false\nf=no\ng=0\nh=off\n");
    EXPECT_TRUE(ini.get<bool>("flags", "a"));
    EXPECT_TRUE(ini.get<bool>("flags", "b"));
    EXPECT_TRUE(ini.get<bool>("flags", "c"));
    EXPECT_TRUE(ini.get<bool>("flags", "d"));
    EXPECT_FALSE(ini.get<bool>("flags", "e"));
    EXPECT_FALSE(ini.get<bool>("flags", "f"));
    EXPECT_FALSE(ini.get<bool>("flags", "g"));
    EXPECT_FALSE(ini.get<bool>("flags", "h"));
}

TEST(IniReaderTest, GlobalSection) {
    IniReader ini;
    ini.parse("key1 = value1\nkey2 = 42\n[section]\nskey = sval\n");
    EXPECT_EQ(ini.get<std::string>("", "key1"), "value1");
    EXPECT_EQ(ini.get<int>("", "key2"), 42);
    EXPECT_EQ(ini.get<std::string>("section", "skey"), "sval");
}

TEST(IniReaderTest, Comments) {
    IniReader ini;
    ini.parse(R"(
# This is a comment
[config]
# Another comment
key = value ; inline comment
; Semicolon comment
port = 3000
)");
    EXPECT_EQ(ini.get<std::string>("config", "key"), "value ; inline comment");
    EXPECT_EQ(ini.get<int>("config", "port"), 3000);
}

TEST(IniReaderTest, SetAndGet) {
    IniReader ini;
    ini.set("server", "host", std::string("example.com"));
    ini.set("server", "port", 443);
    ini.set("server", "ssl", true);

    EXPECT_EQ(ini.get<std::string>("server", "host"), "example.com");
    EXPECT_EQ(ini.get<int>("server", "port"), 443);
    EXPECT_EQ(ini.get<bool>("server", "ssl"), true);
    EXPECT_TRUE(ini.hasKey("server", "host"));
    EXPECT_FALSE(ini.hasKey("server", "missing"));
    EXPECT_TRUE(ini.hasSection("server"));
    EXPECT_FALSE(ini.hasSection("nonexistent"));
}

TEST(IniReaderTest, SectionsAndKeys) {
    IniReader ini;
    ini.parse("[a]\nk1=v\nk2=v\n[b]\nk3=v\n");
    auto sections = ini.getSections();
    EXPECT_EQ(sections.size(), 2);
    auto keys = ini.getKeys("a");
    EXPECT_EQ(keys.size(), 2);
    EXPECT_EQ(ini.sectionCount(), 2);
}

TEST(IniReaderTest, Clear) {
    IniReader ini;
    ini.parse("[app]\nname=test\n");
    EXPECT_TRUE(ini.hasSection("app"));
    ini.clear();
    EXPECT_FALSE(ini.hasSection("app"));
    EXPECT_EQ(ini.sectionCount(), 0);
}

// ── Stress Tests ────────────────────────────────────────────────────────────

TEST(StressTest, ConcurrentPoolAllocator) {
    PoolAllocator<long> pool(5000);
    std::atomic<int> errors{0};
    constexpr int kThreads = 8;
    constexpr int kIters = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < kIters; ++i) {
                auto* p = pool.allocate();
                if (p) {
                    *p = i;
                    std::this_thread::yield();
                    pool.deallocate(p);
                }
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(pool.allocated(), 0);
}

TEST(StressTest, LRUCacheConcurrent) {
    LRUCache<int, int> cache(100);
    std::atomic<int> errors{0};
    constexpr int kThreads = 8;
    constexpr int kOps = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&cache, &errors, t]() {
            for (int i = 0; i < kOps; ++i) {
                int key = (t * kOps + i) % 200;
                try {
                    cache.put(key, key * 10);
                    auto val = cache.get(key);
                    // Value may have been evicted by concurrent threads, accept nullopt
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

TEST(StressTest, BarrierMultipleCycles) {
    constexpr size_t kThreads = 4;
    constexpr int kCycles = 100;
    Barrier barrier(kThreads);
    std::atomic<int> errors{0};
    std::atomic<int> cycle_count{0};

    std::vector<std::thread> threads;
    for (size_t t = 0; t < kThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < kCycles; ++i) {
                bool leader = barrier.wait();
                if (leader) {
                    cycle_count.fetch_add(1);
                }
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(cycle_count.load(), kCycles);
    EXPECT_EQ(errors.load(), 0);
}

TEST(StressTest, SemaphoreProducerConsumer) {
    Semaphore items(0);
    Semaphore spaces(10);
    std::mutex cout_mutex;
    std::vector<int> produced;
    std::vector<int> consumed;
    std::atomic<bool> done{false};
    std::atomic<int> errors{0};

    std::thread producer([&]() {
        for (int i = 0; i < 1000; ++i) {
            spaces.acquire();
            {
                std::lock_guard lock(cout_mutex);
                produced.push_back(i);
            }
            items.release();
        }
        done.store(true);
        items.release();  // Wake consumer for final check
    });

    std::thread consumer([&]() {
        while (true) {
            items.acquire();
            if (done.load() && produced.size() == consumed.size()) break;
            {
                std::lock_guard lock(cout_mutex);
                if (!produced.empty() && consumed.size() < produced.size()) {
                    consumed.push_back(produced[consumed.size()]);
                }
            }
            spaces.release();
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(produced.size(), 1000);
    EXPECT_EQ(consumed.size(), 1000);
    EXPECT_EQ(errors.load(), 0);
}
