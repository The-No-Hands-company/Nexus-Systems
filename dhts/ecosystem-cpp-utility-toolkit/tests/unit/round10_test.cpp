#include <gtest/gtest.h>
#include <string>
#include <cstring>

#include "nexus/utility/allocator/arena_allocator.h"
#include "nexus/utility/datastructures/hash_collision_detector.h"
#include "nexus/utility/string/utf8_validator.h"

using namespace nexus::utility::allocator;
using namespace nexus::utility::datastructures;
using namespace nexus::utility::string;

// ── ArenaAllocator Tests ────────────────────────────────────────────────────

TEST(ArenaAllocatorTest, BasicAllocation) {
    ArenaAllocator arena(1024);
    void* p1 = arena.allocate(100);
    void* p2 = arena.allocate(200);

    EXPECT_NE(p1, nullptr);
    EXPECT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);
    EXPECT_GT(arena.used(), 0);
    EXPECT_LE(arena.used(), 1024);
}

TEST(ArenaAllocatorTest, Alignment) {
    ArenaAllocator arena(1024);
    void* p = arena.allocate(8, 64);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 64, 0);
}

TEST(ArenaAllocatorTest, ResetReclaimsAll) {
    ArenaAllocator arena(1024);
    arena.allocate(500);
    EXPECT_GT(arena.used(), 0);

    arena.reset();
    EXPECT_EQ(arena.used(), 0);
    EXPECT_EQ(arena.allocationCount(), 0);

    // Should be able to allocate again after reset
    void* p = arena.allocate(500);
    EXPECT_NE(p, nullptr);
}

TEST(ArenaAllocatorTest, TypedAllocation) {
    ArenaAllocator arena(1024);
    int* pi = arena.create<int>(42);
    double* pd = arena.create<double>(3.14);

    EXPECT_EQ(*pi, 42);
    EXPECT_DOUBLE_EQ(*pd, 3.14);
}

TEST(ArenaAllocatorTest, Utilization) {
    ArenaAllocator arena(1024);
    arena.allocate(512);
    EXPECT_NEAR(arena.utilization(), 0.5, 0.01);

    arena.allocate(256);
    EXPECT_NEAR(arena.utilization(), 0.75, 0.01);
}

TEST(ArenaAllocatorTest, CapacityTracking) {
    ArenaAllocator arena(1024);
    EXPECT_EQ(arena.capacity(), 1024);
    EXPECT_EQ(arena.remaining(), 1024);

    arena.allocate(100);
    EXPECT_EQ(arena.remaining(), 924);
}

// ── HashCollisionDetector Tests ────────────────────────────────────────────

TEST(HashCollisionDetectorTest, NoCollisionsEmpty) {
    HashCollisionDetector hcd(100);
    auto stats = hcd.getStats();
    EXPECT_EQ(stats.total_insertions, 0);
    EXPECT_EQ(stats.total_collisions, 0);
    EXPECT_EQ(stats.collision_rate, 0.0);
}

TEST(HashCollisionDetectorTest, DetectsCollisions) {
    HashCollisionDetector hcd(10);

    // Insert 10 items with hashes that map to different buckets (0-9)
    for (uint64_t i = 0; i < 10; ++i) {
        hcd.recordInsertion(i);  // hash i → bucket i % 10 = i
    }
    auto stats = hcd.getStats();
    EXPECT_EQ(stats.total_insertions, 10);
    EXPECT_EQ(stats.total_collisions, 0);

    // Insert a collision in bucket 0
    hcd.recordInsertion(10);  // 10 % 10 = 0 → collision with first
    stats = hcd.getStats();
    EXPECT_EQ(stats.total_collisions, 1);
    EXPECT_GT(stats.collision_rate, 0.0);
    EXPECT_EQ(stats.peak_bucket_size, 2);
}

TEST(HashCollisionDetectorTest, PeakTracking) {
    HashCollisionDetector hcd(5);
    for (int i = 0; i < 5; ++i) {
        hcd.recordInsertion(0);  // All go to bucket 0
    }

    auto stats = hcd.getStats();
    EXPECT_EQ(stats.peak_bucket_size, 5);
    EXPECT_EQ(stats.collision_rate, 0.8);  // 4 collisions / 5 insertions
}

TEST(HashCollisionDetectorTest, Removal) {
    HashCollisionDetector hcd(10);
    hcd.recordInsertion(5);
    hcd.recordInsertion(5);  // Collision in bucket 5
    EXPECT_EQ(hcd.getStats().total_collisions, 1);

    hcd.recordRemoval(5);
    auto stats = hcd.getStats();
    EXPECT_LT(stats.peak_bucket_size, 3);
}

TEST(HashCollisionDetectorTest, Reset) {
    HashCollisionDetector hcd(10);
    hcd.recordInsertion(1);
    hcd.recordInsertion(1);
    hcd.reset();

    auto stats = hcd.getStats();
    EXPECT_EQ(stats.total_insertions, 0);
    EXPECT_EQ(stats.peak_bucket_size, 0);
}

// ── Utf8Validator Tests ────────────────────────────────────────────────────

TEST(Utf8ValidatorTest, ValidAscii) {
    EXPECT_TRUE(Utf8Validator::isValid("Hello, World!"));
    auto result = Utf8Validator::validate("Hello");
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.code_points, 5);
    EXPECT_TRUE(result.invalid_offsets.empty());
}

TEST(Utf8ValidatorTest, ValidMultiByte) {
    // "café" - é is U+00E9 (2 bytes in UTF-8: 0xC3 0xA9)
    std::string cafe = "caf\xC3\xA9";
    EXPECT_TRUE(Utf8Validator::isValid(cafe));
    EXPECT_EQ(Utf8Validator::codePointCount(cafe), 4);
}

TEST(Utf8ValidatorTest, ValidEmoji) {
    // "😀" U+1F600 (4 bytes in UTF-8: 0xF0 0x9F 0x98 0x80)
    std::string emoji = "\xF0\x9F\x98\x80";
    EXPECT_TRUE(Utf8Validator::isValid(emoji));
    EXPECT_EQ(Utf8Validator::codePointCount(emoji), 1);
}

TEST(Utf8ValidatorTest, InvalidLeadingByte) {
    // 0xFF is never a valid UTF-8 leading byte
    std::string invalid = "test\xFF";
    EXPECT_FALSE(Utf8Validator::isValid(invalid));
    auto result = Utf8Validator::validate(invalid);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.invalid_offsets.size(), 1);
    EXPECT_EQ(result.invalid_offsets[0], 4);
}

TEST(Utf8ValidatorTest, IncompleteSequence) {
    // 0xC3 expects a continuation byte
    std::string incomplete = "ab\xC3";
    EXPECT_FALSE(Utf8Validator::isValid(incomplete));
    auto result = Utf8Validator::validate(incomplete);
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.invalid_offsets.empty());
}

TEST(Utf8ValidatorTest, OverlongEncoding) {
    // '/' (0x2F) encoded as 2 bytes (0xC0 0xAF) - overlong
    std::string overlong = "\xC0\xAF";
    EXPECT_FALSE(Utf8Validator::isValid(overlong));
}

TEST(Utf8ValidatorTest, SurrogateHalf) {
    // U+D800 encoded in UTF-8 (invalid per Unicode)
    std::string surrogate = "\xED\xA0\x80";
    EXPECT_FALSE(Utf8Validator::isValid(surrogate));
}

TEST(Utf8ValidatorTest, EmptyString) {
    EXPECT_TRUE(Utf8Validator::isValid(""));
    EXPECT_EQ(Utf8Validator::codePointCount(""), 0);
}
