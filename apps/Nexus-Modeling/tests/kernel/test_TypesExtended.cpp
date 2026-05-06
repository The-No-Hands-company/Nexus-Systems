#include <nexus/gfx/Types.h>
#include <gtest/gtest.h>

#include <unordered_map>

using namespace nexus::gfx;

TEST(TypesExtended, HandleHashWorksInUnorderedMap)
{
    std::unordered_map<BufferHandle, int> m;
    BufferHandle a{}; a.id = 7;
    BufferHandle b{}; b.id = 11;
    m[a] = 10;
    m[b] = 20;

    EXPECT_EQ(m[a], 10);
    EXPECT_EQ(m[b], 20);
}

TEST(TypesExtended, BufferUsageAndNotOperators)
{
    auto flags = BufferUsage::VertexBuffer | BufferUsage::TransferDst;
    auto onlyV = flags & BufferUsage::VertexBuffer;
    EXPECT_EQ(static_cast<uint32_t>(onlyV), static_cast<uint32_t>(BufferUsage::VertexBuffer));

    auto cleared = flags & ~BufferUsage::TransferDst;
    EXPECT_TRUE(hasFlag(cleared, BufferUsage::VertexBuffer));
    EXPECT_FALSE(hasFlag(cleared, BufferUsage::TransferDst));
}

TEST(TypesExtended, TextureUsageAndNotOperators)
{
    auto flags = TextureUsage::Sampled | TextureUsage::ColorAttachment;
    auto onlySampled = flags & TextureUsage::Sampled;
    EXPECT_EQ(static_cast<uint32_t>(onlySampled), static_cast<uint32_t>(TextureUsage::Sampled));

    auto cleared = flags & ~TextureUsage::ColorAttachment;
    EXPECT_EQ(static_cast<uint32_t>(cleared), static_cast<uint32_t>(TextureUsage::Sampled));
}

TEST(TypesExtended, ShaderStageAndNotOperators)
{
    auto flags = ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Compute;
    auto graphics = flags & ~ShaderStage::Compute;

    EXPECT_NE(static_cast<uint32_t>(graphics & ShaderStage::Vertex), 0u);
    EXPECT_NE(static_cast<uint32_t>(graphics & ShaderStage::Fragment), 0u);
    EXPECT_EQ(static_cast<uint32_t>(graphics & ShaderStage::Compute), 0u);
}

TEST(TypesExtended, BufferAccessAndOperator)
{
    auto rw = BufferAccess::ShaderRead | BufferAccess::ShaderWrite;
    EXPECT_NE(static_cast<uint32_t>(rw & BufferAccess::ShaderRead), 0u);
    EXPECT_NE(static_cast<uint32_t>(rw & BufferAccess::ShaderWrite), 0u);
    EXPECT_EQ(static_cast<uint32_t>(rw & BufferAccess::IndexRead), 0u);
}
