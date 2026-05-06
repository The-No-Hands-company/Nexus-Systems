// ─────────────────────────────────────────────────────────────────────────────
//  Tests: nexus::gfx::Types
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <gtest/gtest.h>

using namespace nexus::gfx;

TEST(Types, HandleDefaultsToInvalid)
{
    BufferHandle h;
    EXPECT_FALSE(h.valid());
    EXPECT_EQ(h.id, kNullHandle);
}

TEST(Types, HandleEqualityOnId)
{
    BufferHandle a, b;
    a.id = 42; b.id = 42;
    EXPECT_EQ(a, b);
    b.id = 99;
    EXPECT_NE(a, b);
}

TEST(Types, BufferUsageBitwiseOr)
{
    auto combined = BufferUsage::VertexBuffer | BufferUsage::TransferDst;
    EXPECT_TRUE(hasFlag(combined, BufferUsage::VertexBuffer));
    EXPECT_TRUE(hasFlag(combined, BufferUsage::TransferDst));
    EXPECT_FALSE(hasFlag(combined, BufferUsage::IndexBuffer));
}

TEST(Types, Extent2DDefaultsToZero)
{
    Extent2D e;
    EXPECT_EQ(e.width, 0u);
    EXPECT_EQ(e.height, 0u);
}
