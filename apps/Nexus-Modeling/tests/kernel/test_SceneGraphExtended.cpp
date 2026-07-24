#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/Device.h>
#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace nexus::render;
using namespace nexus::gfx;

namespace {

class CountingDestroyDevice final : public IDevice {
public:
    CountingDestroyDevice()
    {
        m_inner = createDevice(Backend::Null);
    }

    [[nodiscard]] Backend backend() const noexcept override { return m_inner->backend(); }
    [[nodiscard]] const DeviceCapabilities& caps() const noexcept override { return m_inner->caps(); }
    [[nodiscard]] HardwareTier tier() const noexcept override { return m_inner->tier(); }
    [[nodiscard]] std::string_view deviceName() const noexcept override { return m_inner->deviceName(); }

    [[nodiscard]] BufferHandle createBuffer(const BufferDesc& desc) override { return m_inner->createBuffer(desc); }
    [[nodiscard]] TextureHandle createTexture(const TextureDesc& desc) override { return m_inner->createTexture(desc); }
    [[nodiscard]] ShaderHandle createShader(const ShaderDesc& desc) override { return m_inner->createShader(desc); }
    [[nodiscard]] RenderPassHandle createRenderPass(const RenderPassDesc& desc) override { return m_inner->createRenderPass(desc); }
    [[nodiscard]] PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) override { return m_inner->createGraphicsPipeline(desc); }
    [[nodiscard]] PipelineHandle createMeshShaderPipeline(const MeshShaderPipelineDesc& desc) override { return m_inner->createMeshShaderPipeline(desc); }
    [[nodiscard]] PipelineHandle createComputePipeline(const ComputePipelineDesc& desc) override { return m_inner->createComputePipeline(desc); }
    [[nodiscard]] PipelineHandle createRayTracingPipeline(const RayTracingPipelineDesc& desc) override { return m_inner->createRayTracingPipeline(desc); }
    [[nodiscard]] SamplerHandle createSampler(const SamplerDesc& desc) override { return m_inner->createSampler(desc); }
    [[nodiscard]] QueryPoolHandle createQueryPool(const QueryPoolDesc& desc) override { return m_inner->createQueryPool(desc); }
    void readbackTimestamps(QueryPoolHandle pool, uint32_t first, uint32_t count, uint64_t* outNanos) override {
        m_inner->readbackTimestamps(pool, first, count, outNanos);
    }

    void destroyBuffer(BufferHandle h) override { ++destroyBufferCalls; m_inner->destroyBuffer(h); }
    void destroyTexture(TextureHandle h) override { m_inner->destroyTexture(h); }
    void destroyShader(ShaderHandle h) override { m_inner->destroyShader(h); }
    void destroyRenderPass(RenderPassHandle h) override { m_inner->destroyRenderPass(h); }
    void destroyPipeline(PipelineHandle h) override { m_inner->destroyPipeline(h); }
    void destroySampler(SamplerHandle h) override { m_inner->destroySampler(h); }
    void destroyQueryPool(QueryPoolHandle h) override { m_inner->destroyQueryPool(h); }

    [[nodiscard]] CmdBufHandle allocateCommandBuffer(QueueType queue = QueueType::Graphics) override {
        return m_inner->allocateCommandBuffer(queue);
    }
    void freeCommandBuffer(CmdBufHandle h) override { m_inner->freeCommandBuffer(h); }
    [[nodiscard]] ICommandBuffer* getCommandBuffer(CmdBufHandle h) override { return m_inner->getCommandBuffer(h); }

    [[nodiscard]] FenceHandle createFence(bool signaled = false) override { return m_inner->createFence(signaled); }
    [[nodiscard]] SemaphoreHandle createSemaphore() override { return m_inner->createSemaphore(); }
    void destroyFence(FenceHandle h) override { m_inner->destroyFence(h); }
    void destroySemaphore(SemaphoreHandle h) override { m_inner->destroySemaphore(h); }
    void waitForFence(FenceHandle h, uint64_t timeoutNs = UINT64_MAX) override { m_inner->waitForFence(h, timeoutNs); }
    void resetFence(FenceHandle h) override { m_inner->resetFence(h); }

    void uploadBuffer(BufferHandle dst, const void* data, uint64_t sizeBytes, uint64_t dstOffset = 0) override {
        m_inner->uploadBuffer(dst, data, sizeBytes, dstOffset);
    }
    void uploadTexture(TextureHandle dst, const void* data, uint64_t sizeBytes) override {
        m_inner->uploadTexture(dst, data, sizeBytes);
    }

    void submit(QueueType queue,
                std::span<const CmdBufHandle> cmds,
                std::span<const SemaphoreHandle> waitSemaphores = {},
                std::span<const SemaphoreHandle> signalSemaphores = {},
                FenceHandle signalFence = {}) override {
        m_inner->submit(queue, cmds, waitSemaphores, signalSemaphores, signalFence);
    }

    void waitIdle() override { m_inner->waitIdle(); }

    [[nodiscard]] AccelStructHandle buildBLAS(BufferHandle vertexBuffer,
                                              BufferHandle indexBuffer,
                                              uint32_t vertexCount,
                                              uint32_t indexCount) override {
        return m_inner->buildBLAS(vertexBuffer, indexBuffer, vertexCount, indexCount);
    }
    [[nodiscard]] AccelStructHandle buildTLAS(std::span<const AccelStructHandle> blases) override {
        (void)blases;
        AccelStructHandle h{};
        h.id = nextAccelStructId++;
        return h;
    }
    void destroyAccelStruct(AccelStructHandle h) override {
        (void)h;
        ++destroyAccelStructCalls;
    }

    uint32_t destroyBufferCalls = 0;
    uint32_t destroyAccelStructCalls = 0;

private:
    std::unique_ptr<IDevice> m_inner;
    uint64_t nextAccelStructId = 1;
};

} // namespace

TEST(SceneGraphExtended, RemoveNodeErasesFromIdIndex)
{
    SceneGraph sg;
    auto* n = sg.createNode("to_remove");
    ASSERT_NE(n, nullptr);
    const auto id = n->id();
    ASSERT_NE(sg.findNode(id), nullptr);

    sg.removeNode(id);
    EXPECT_EQ(sg.findNode(id), nullptr);
}

TEST(SceneGraphExtended, RemoveNodeAlsoErasesDescendants)
{
    SceneGraph sg;
    auto* p = sg.createNode("parent");
    auto* c = sg.createNode("child", p);
    auto* g = sg.createNode("grandchild", c);

    const auto pid = p->id();
    const auto cid = c->id();
    const auto gid = g->id();

    sg.removeNode(pid);

    EXPECT_EQ(sg.findNode(pid), nullptr);
    EXPECT_EQ(sg.findNode(cid), nullptr);
    EXPECT_EQ(sg.findNode(gid), nullptr);
}

TEST(SceneGraphExtended, RemoveRootIsNoOp)
{
    SceneGraph sg;
    auto* a = sg.createNode("a");
    ASSERT_NE(a, nullptr);

    // Root id is 0 by construction.
    sg.removeNode(0);

    EXPECT_NE(sg.findNode(a->id()), nullptr);
    EXPECT_EQ(sg.findNode("__root__"), &sg.root());
}

TEST(SceneGraphExtended, RemoveNodeDetachesFromParentChildren)
{
    SceneGraph sg;
    auto* p = sg.createNode("parent");
    auto* c = sg.createNode("child", p);
    ASSERT_NE(c, nullptr);

    const auto before = p->children().size();
    sg.removeNode(c->id());
    const auto after = p->children().size();

    EXPECT_EQ(before, 1u);
    EXPECT_EQ(after, 0u);
}

TEST(SceneGraphExtended, RemoveNodeAutoDestroysGpuPayloadWhenEnabled)
{
    CountingDestroyDevice dev;
    SceneGraph sg;
    sg.enableAutoDestroyNodeGpuPayloadOnRemove(dev);

    auto* p = sg.createNode("parent");
    auto* c = sg.createNode("child", p);
    ASSERT_NE(p, nullptr);
    ASSERT_NE(c, nullptr);

    p->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    p->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    p->mesh.blas.id = 111;  // simulate valid RT handle

    c->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    c->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    c->mesh.blas.id = 222;  // simulate valid RT handle

    sg.removeNode(p->id());

    EXPECT_EQ(dev.destroyBufferCalls, 4u);
    EXPECT_EQ(dev.destroyAccelStructCalls, 2u);
}

TEST(SceneGraphExtended, RemoveNodeDoesNotDestroyGpuPayloadWhenDisabled)
{
    CountingDestroyDevice dev;
    SceneGraph sg;
    sg.enableAutoDestroyNodeGpuPayloadOnRemove(dev);
    sg.disableAutoDestroyNodeGpuPayloadOnRemove();

    auto* n = sg.createNode("node");
    ASSERT_NE(n, nullptr);

    n->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    n->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    n->mesh.blas.id = 333;

    sg.removeNode(n->id());

    EXPECT_EQ(dev.destroyBufferCalls, 0u);
    EXPECT_EQ(dev.destroyAccelStructCalls, 0u);
}

TEST(SceneGraphExtended, ClearRemovesAllNonRootNodes)
{
    SceneGraph sg;
    auto* a = sg.createNode("a");
    auto* b = sg.createNode("b");
    auto* c = sg.createNode("c", a);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    const auto aId = a->id();
    const auto bId = b->id();
    const auto cId = c->id();

    sg.clear();

    EXPECT_EQ(sg.findNode(aId), nullptr);
    EXPECT_EQ(sg.findNode(bId), nullptr);
    EXPECT_EQ(sg.findNode(cId), nullptr);
    EXPECT_EQ(sg.root().children().size(), 0u);
    EXPECT_EQ(sg.findNode("__root__"), &sg.root());
}

TEST(SceneGraphExtended, ClearAutoDestroysGpuPayloadWhenEnabled)
{
    CountingDestroyDevice dev;
    SceneGraph sg;
    sg.enableAutoDestroyNodeGpuPayloadOnRemove(dev);

    auto* p = sg.createNode("parent");
    auto* c = sg.createNode("child", p);
    ASSERT_NE(p, nullptr);
    ASSERT_NE(c, nullptr);

    p->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    p->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    p->mesh.blas.id = 444;

    c->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    c->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    c->mesh.blas.id = 555;

    sg.clear();

    EXPECT_EQ(dev.destroyBufferCalls, 4u);
    EXPECT_EQ(dev.destroyAccelStructCalls, 2u);
    EXPECT_EQ(sg.root().children().size(), 0u);
}

TEST(SceneGraphExtended, ClearAndDestroyTLASClearsNodesAndReleasesLiveTLAS)
{
    CountingDestroyDevice dev;
    SceneGraph sg;

    auto* n = sg.createNode("tlas-node");
    ASSERT_NE(n, nullptr);
    const auto nid = n->id();
    n->mesh.blas.id = 900;

    ASSERT_TRUE(sg.rebuildTLAS(dev));
    ASSERT_TRUE(sg.tlas().valid());

    sg.clearAndDestroyTLAS(dev);

    EXPECT_EQ(sg.findNode(nid), nullptr);
    EXPECT_EQ(sg.root().children().size(), 0u);
    EXPECT_FALSE(sg.tlas().valid());
    EXPECT_EQ(dev.destroyAccelStructCalls, 1u);
}

TEST(SceneGraphExtended, ClearAndDestroyTLASWithoutLiveTLASStillClearsNodes)
{
    CountingDestroyDevice dev;
    SceneGraph sg;
    auto* n = sg.createNode("node");
    ASSERT_NE(n, nullptr);
    const auto nid = n->id();

    sg.clearAndDestroyTLAS(dev);

    EXPECT_EQ(sg.findNode(nid), nullptr);
    EXPECT_EQ(sg.root().children().size(), 0u);
    EXPECT_EQ(dev.destroyAccelStructCalls, 0u);
}

TEST(SceneGraphExtended, CollectVisibleUsesScaleAwareRadius)
{
    SceneGraph sg;
    auto* n = sg.createNode("scaled");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 1;
    n->localTransform().translation = {-2.0f, 0.0f, 0.0f};
    n->localTransform().scale = {3.0f, 3.0f, 3.0f};

    Frustum f{};
    // Plane x >= 0 (inside toward +X); with radius=3 and center x=-2, node intersects.
    f.planes[0] = {1.0f, 0.0f, 0.0f, 0.0f};

    std::vector<Node*> out;
    sg.collectVisible(f, out);

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], n);
}

namespace {
// Frustum bounding x∈[-1,1], y∈[-1,1], z∈[0,10] with inward unit normals.
Frustum boxFrustum()
{
    Frustum f{};
    f.planes[0] = { 1.f,  0.f,  0.f,  1.f};
    f.planes[1] = {-1.f,  0.f,  0.f,  1.f};
    f.planes[2] = { 0.f,  1.f,  0.f,  1.f};
    f.planes[3] = { 0.f, -1.f,  0.f,  1.f};
    f.planes[4] = { 0.f,  0.f,  1.f,  0.f};
    f.planes[5] = { 0.f,  0.f, -1.f, 10.f};
    return f;
}
} // namespace

TEST(SceneGraphExtended, CollectVisibleUsesLocalBoundsWhenPresent)
{
    SceneGraph sg;
    auto* inNode = sg.createNode("inside");
    ASSERT_NE(inNode, nullptr);
    inNode->mesh.vertexBuffer.id = 11;
    inNode->localTransform().translation = {0.f, 0.f, 5.f};
    inNode->setLocalBounds(Aabb{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}});

    auto* outNode = sg.createNode("outside");
    ASSERT_NE(outNode, nullptr);
    outNode->mesh.vertexBuffer.id = 12;
    outNode->localTransform().translation = {5.f, 0.f, 5.f}; // world AABB x∈[4.5,5.5]
    outNode->setLocalBounds(Aabb{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}});

    std::vector<Node*> out;
    sg.collectVisible(boxFrustum(), out);

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], inNode);
}

TEST(SceneGraphExtended, CollectVisibleLocalBoundsExtentDrivesDecision)
{
    // Two nodes whose centers both sit just outside the right plane (x=1) but
    // whose bounds differ: the wide one straddles back into the frustum and is
    // kept; the tight one stays fully outside and is culled. This is the win the
    // sphere-from-translation fallback cannot make on bounds alone.
    SceneGraph sg;
    auto* wide = sg.createNode("wide");
    ASSERT_NE(wide, nullptr);
    wide->mesh.vertexBuffer.id = 21;
    wide->localTransform().translation = {1.2f, 0.f, 5.f};
    wide->setLocalBounds(Aabb{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}}); // world x∈[0.7,1.7]

    auto* tight = sg.createNode("tight");
    ASSERT_NE(tight, nullptr);
    tight->mesh.vertexBuffer.id = 22;
    tight->localTransform().translation = {1.2f, 0.f, 5.f};
    tight->setLocalBounds(Aabb{{-0.1f, -0.1f, -0.1f}, {0.1f, 0.1f, 0.1f}}); // world x∈[1.1,1.3]

    std::vector<Node*> out;
    sg.collectVisible(boxFrustum(), out);

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], wide);
}

TEST(SceneGraphExtended, CollectVisibleClearLocalBoundsRestoresSphereFallback)
{
    SceneGraph sg;
    auto* n = sg.createNode("toggle");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 31;
    n->localTransform().translation = {1.2f, 0.f, 5.f};
    n->setLocalBounds(Aabb{{-0.1f, -0.1f, -0.1f}, {0.1f, 0.1f, 0.1f}}); // tight: culled

    std::vector<Node*> culled;
    sg.collectVisible(boxFrustum(), culled);
    EXPECT_TRUE(culled.empty());

    // Dropping bounds reverts to the conservative sphere (scale 1 -> unit radius),
    // whose reach from x=1.2 crosses x=1, so the node is kept.
    n->clearLocalBounds();
    std::vector<Node*> kept;
    sg.collectVisible(boxFrustum(), kept);
    ASSERT_EQ(kept.size(), 1u);
    EXPECT_EQ(kept[0], n);
}

TEST(SceneGraphExtended, CollectVisibleHandlesMirroredScaleConservatively)
{
    SceneGraph sg;
    auto* n = sg.createNode("mirrored");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 2;
    n->localTransform().translation = {-2.0f, 0.0f, 0.0f};
    n->localTransform().scale = {-3.0f, 2.0f, 1.0f};

    Frustum f{};
    f.planes[0] = {1.0f, 0.0f, 0.0f, 0.0f};

    std::vector<Node*> out;
    sg.collectVisible(f, out);

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], n);
}

TEST(SceneGraphExtended, WorldMatrixSanitizesNonFiniteLocalTransform)
{
    SceneGraph sg;
    auto* n = sg.createNode("sanitized");
    ASSERT_NE(n, nullptr);

    n->localTransform().translation = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};
    n->localTransform().rotation = {
        std::numeric_limits<float>::quiet_NaN(),
        0.f,
        0.f,
        std::numeric_limits<float>::infinity()};
    n->localTransform().scale = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};

    const Mat4 world = n->worldMatrix();
    EXPECT_TRUE(std::isfinite(world.m[0][3]));
    EXPECT_TRUE(std::isfinite(world.m[1][3]));
    EXPECT_TRUE(std::isfinite(world.m[2][3]));
    EXPECT_FLOAT_EQ(world.m[0][3], 0.f);
    EXPECT_FLOAT_EQ(world.m[1][3], 0.f);
    EXPECT_FLOAT_EQ(world.m[2][3], 0.f);
}

TEST(SceneGraphExtended, CollectVisibleUsesSanitizedCenterForNonFiniteTranslation)
{
    SceneGraph sg;
    auto* n = sg.createNode("nan_center");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 7;
    n->localTransform().translation = {
        std::numeric_limits<float>::quiet_NaN(),
        0.f,
        0.f};

    Frustum f{};
    // Plane x >= -0.5 ; sanitized center at x=0 remains visible.
    f.planes[0] = {1.0f, 0.0f, 0.0f, 0.5f};

    std::vector<Node*> out;
    sg.collectVisible(f, out);

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], n);
}

// ── Transform::toMatrix — TRS composition order ──────────────────────────────
//
// A decomposed local transform must compose as M = T * R * S (scale applied first,
// in the object's local space) — the glTF / Unity / Unreal convention, and the one
// the engine's own scale-recovery assumes: conservativeWorldRadius reads each scale
// factor back as the length of a world-matrix COLUMN, which only holds when scale
// multiplies column j by s_j (i.e. R * S). The matrix must NOT build S * R (scaling
// world axes after rotation), which differs whenever scale is non-uniform AND there
// is a rotation.
namespace {
Vec4 axisAngleQuat(Vec3 axis, float degrees) {
    const float r = degrees * 3.14159265358979323846f / 180.f;
    const float s = std::sin(r * 0.5f);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(r * 0.5f)};
}
} // namespace

TEST(TransformToMatrix, ScaleIsAppliedInLocalSpaceBeforeRotation) {
    Transform t;
    t.rotation = axisAngleQuat({0.f, 0.f, 1.f}, 90.f);  // +90 deg about Z
    t.scale    = {2.f, 1.f, 1.f};                        // stretch local X by 2

    const Mat4 m = t.toMatrix();

    // Local +X is scaled to length 2, then rotated 90 deg about Z onto world +Y.
    // T*R*S gives (0,2,0); the wrong S*R order gives (0,1,0) (the x-scale is lost
    // because the vector points along Y after the rotation).
    const Vec4 x = m * Vec4{1.f, 0.f, 0.f, 1.f};
    EXPECT_NEAR(x.x, 0.f, 1e-5f);
    EXPECT_NEAR(x.y, 2.f, 1e-5f);
    EXPECT_NEAR(x.z, 0.f, 1e-5f);

    // Local +Y (scale 1) rotates 90 deg about Z onto world -X, length 1.
    const Vec4 y = m * Vec4{0.f, 1.f, 0.f, 1.f};
    EXPECT_NEAR(y.x, -1.f, 1e-5f);
    EXPECT_NEAR(y.y, 0.f, 1e-5f);
    EXPECT_NEAR(y.z, 0.f, 1e-5f);
}

TEST(TransformToMatrix, ColumnLengthsRecoverPerAxisScale) {
    // The engine recovers each axis scale from the length of the corresponding
    // world-matrix column (see conservativeWorldRadius / axisScaleLength). With a
    // rotation and non-uniform scale, that identity only holds for R * S.
    Transform t;
    t.rotation = axisAngleQuat({0.3f, 0.7f, 0.64807f}, 52.f);  // arbitrary tilt
    t.scale    = {2.f, 3.f, 4.f};

    const Mat4 m = t.toMatrix();
    const float sx = std::sqrt(m.m[0][0]*m.m[0][0] + m.m[1][0]*m.m[1][0] + m.m[2][0]*m.m[2][0]);
    const float sy = std::sqrt(m.m[0][1]*m.m[0][1] + m.m[1][1]*m.m[1][1] + m.m[2][1]*m.m[2][1]);
    const float sz = std::sqrt(m.m[0][2]*m.m[0][2] + m.m[1][2]*m.m[1][2] + m.m[2][2]*m.m[2][2]);
    EXPECT_NEAR(sx, 2.f, 1e-4f);
    EXPECT_NEAR(sy, 3.f, 1e-4f);
    EXPECT_NEAR(sz, 4.f, 1e-4f);
}

TEST(TransformToMatrix, TranslationIsAppliedAfterRotationAndScale) {
    Transform t;
    t.translation = {5.f, -3.f, 2.f};
    t.rotation    = axisAngleQuat({0.f, 1.f, 0.f}, 90.f);
    t.scale       = {2.f, 2.f, 2.f};

    const Mat4 m = t.toMatrix();
    // Origin maps to the translation exactly.
    const Vec4 o = m * Vec4{0.f, 0.f, 0.f, 1.f};
    EXPECT_NEAR(o.x, 5.f, 1e-5f);
    EXPECT_NEAR(o.y, -3.f, 1e-5f);
    EXPECT_NEAR(o.z, 2.f, 1e-5f);
}
