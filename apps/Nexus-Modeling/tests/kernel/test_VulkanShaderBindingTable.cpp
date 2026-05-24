// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Vulkan shader binding table layout math (no GPU required)
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanShaderBindingTable.h"

#include <gtest/gtest.h>

using namespace nexus::gfx;

TEST(VulkanShaderBindingTable, AlignUpRoundsToMultiple) {
    EXPECT_EQ(alignUp(0, 64), 0u);
    EXPECT_EQ(alignUp(1, 64), 64u);
    EXPECT_EQ(alignUp(64, 64), 64u);
    EXPECT_EQ(alignUp(65, 64), 128u);
    EXPECT_EQ(alignUp(32, 32), 32u);
    EXPECT_EQ(alignUp(33, 1), 33u);  // alignment 1 = no-op
    EXPECT_EQ(alignUp(33, 0), 33u);  // alignment 0 treated as 1
}

TEST(VulkanShaderBindingTable, TypicalNvidiaLayout) {
    // Common desktop values: 32-byte handles, 32-byte handle alignment,
    // 64-byte base alignment; 2 miss groups, 1 hit group.
    const SbtLayout l = computeShaderBindingTableLayout(/*handleSize=*/32,
                                                        /*handleAlignment=*/32,
                                                        /*baseAlignment=*/64,
                                                        /*missCount=*/2,
                                                        /*hitCount=*/1);

    // Raygen: one record, size == stride, base-aligned to 64.
    EXPECT_EQ(l.raygen.offset, 0u);
    EXPECT_EQ(l.raygen.stride, 64u);
    EXPECT_EQ(l.raygen.size,   64u);
    EXPECT_EQ(l.raygen.size,   l.raygen.stride); // Vulkan requirement

    // Miss: 2 records of stride 32, region starts base-aligned.
    EXPECT_EQ(l.miss.offset, 64u);
    EXPECT_EQ(l.miss.stride, 32u);
    EXPECT_EQ(l.miss.size,   64u);

    // Hit: 1 record, region starts base-aligned after miss.
    EXPECT_EQ(l.hit.offset, 128u);
    EXPECT_EQ(l.hit.stride, 32u);
    EXPECT_EQ(l.hit.size,   32u);

    EXPECT_EQ(l.totalSize, 160u);
}

TEST(VulkanShaderBindingTable, HandleAlignmentPadsStride) {
    // handleSize 32 but handle alignment 64 -> stride padded to 64.
    const SbtLayout l = computeShaderBindingTableLayout(32, 64, 64, 1, 1);
    EXPECT_EQ(l.miss.stride, 64u);
    EXPECT_EQ(l.hit.stride,  64u);
    EXPECT_EQ(l.raygen.stride, 64u);
}

TEST(VulkanShaderBindingTable, EveryRegionStartsBaseAligned) {
    // Odd-ish but legal values; assert each region offset honors base alignment.
    const uint32_t base = 64;
    const SbtLayout l = computeShaderBindingTableLayout(32, 32, base, 3, 5);
    EXPECT_EQ(l.raygen.offset % base, 0u);
    EXPECT_EQ(l.miss.offset   % base, 0u);
    EXPECT_EQ(l.hit.offset    % base, 0u);
    EXPECT_GE(l.miss.offset, l.raygen.offset + l.raygen.size);
    EXPECT_GE(l.hit.offset,  l.miss.offset   + l.miss.size);
    EXPECT_EQ(l.totalSize,   l.hit.offset    + l.hit.size);
}
