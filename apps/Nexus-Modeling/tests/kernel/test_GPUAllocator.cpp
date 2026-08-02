#include <gtest/gtest.h>

#include <nexus/gfx/Allocator.h>
#include <nexus/gfx/RenderContext.h>

using namespace nexus::gfx;

class GPUAllocatorTest : public ::testing::Test {
protected:
    std::unique_ptr<RenderContext> ctx;

    void SetUp() override {
        RenderContextDesc desc;
        desc.preferredBackend = Backend::Null;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);
    }
};

// budgetBytes() is unsigned, so the old `EXPECT_GE(budgetBytes(), 0u)` — under the name
// BudgetIsNonNegative — was true of every possible value and could not fail. What is
// actually knowable here is the relationship between the two counters.
TEST_F(GPUAllocatorTest, BudgetAndAllocatedAreConsistentBeforeAnyAllocation)
{
    auto& alloc = ctx->allocator();
    EXPECT_EQ(alloc.allocatedBytes(), 0u) << "nothing has been allocated yet";
    if (alloc.budgetBytes() > 0u) {
        EXPECT_LE(alloc.allocatedBytes(), alloc.budgetBytes())
            << "allocated must never exceed the budget";
    }
}

TEST_F(GPUAllocatorTest, AllocatedZeroInitially)
{
    auto& alloc = ctx->allocator();
    EXPECT_EQ(alloc.allocatedBytes(), 0u);
}

TEST_F(GPUAllocatorTest, UsedBytesZeroInitially)
{
    auto& alloc = ctx->allocator();
    EXPECT_EQ(alloc.usedBytes(), 0u);
}

TEST_F(GPUAllocatorTest, BudgetMinusAllocatedEqualsUsedInitially)
{
    auto& alloc = ctx->allocator();
    EXPECT_EQ(alloc.usedBytes(), alloc.allocatedBytes());
    EXPECT_LE(alloc.allocatedBytes(), alloc.budgetBytes());
}
