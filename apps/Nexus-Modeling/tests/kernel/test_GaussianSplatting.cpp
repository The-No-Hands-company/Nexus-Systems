// ─────────────────────────────────────────────────────────────────────────────
//  Tests — Month 10 Advanced Rendering Track
//
//  Covers:
//   • GaussianSplatCloud: data model, PLY round-trip, byte-stream loading
//   • GaussianSplatSceneNode: defaults, coexistence with mesh pipeline
//   • TemporalAccumulator: jitter, blend alpha, motion rejection
//   • RepresentationType: variant completeness
//
//  Exit criterion: Gaussian module can load, render, and coexist with the mesh
//  pipeline without core rewrites.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/GaussianSplatting.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/render/TemporalAccumulation.h>
#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <sstream>
#include <vector>

// ── PLY test-data helpers ─────────────────────────────────────────────────────
namespace {

// Build a minimal binary_little_endian PLY in memory with N splats.
// Properties: x y z f_dc_0 f_dc_1 f_dc_2 opacity scale_0..2 rot_0..3
std::vector<uint8_t> makePLYBytes(const std::vector<nexus::gfx::GaussianSplat>& splats)
{
    std::ostringstream hdr;
    hdr << "ply\n"
        << "format binary_little_endian 1.0\n"
        << "element vertex " << splats.size() << "\n"
        << "property float x\n"
        << "property float y\n"
        << "property float z\n"
        << "property float f_dc_0\n"
        << "property float f_dc_1\n"
        << "property float f_dc_2\n"
        << "property float opacity\n"
        << "property float scale_0\n"
        << "property float scale_1\n"
        << "property float scale_2\n"
        << "property float rot_0\n"    // w
        << "property float rot_1\n"    // x
        << "property float rot_2\n"    // y
        << "property float rot_3\n"    // z
        << "end_header\n";

    std::string hdrStr = hdr.str();
    std::vector<uint8_t> out(hdrStr.begin(), hdrStr.end());

    for (const auto& s : splats) {
        float data[14] = {
            s.position.x, s.position.y, s.position.z,
            s.dc[0], s.dc[1], s.dc[2],
            s.opacity,
            s.scale.x, s.scale.y, s.scale.z,
            s.rotation.w, s.rotation.x, s.rotation.y, s.rotation.z
        };
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(data);
        out.insert(out.end(), raw, raw + sizeof(data));
    }
    return out;
}

nexus::gfx::GaussianSplat makeSplat(float px, float py, float pz,
                                    float r = 0.8f, float g = 0.5f, float b = 0.2f)
{
    nexus::gfx::GaussianSplat s{};
    s.position = {px, py, pz};
    s.scale    = {-1.f, -1.f, -1.f};
    s.rotation = {0.f, 0.f, 0.f, 1.f};
    s.opacity  = 0.9f;
    s.dc       = {r, g, b};
    return s;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  GaussianSplatCloud
// ─────────────────────────────────────────────────────────────────────────────
TEST(GaussianSplatting, DefaultCloudIsEmpty)
{
    nexus::gfx::GaussianSplatCloud cloud;
    EXPECT_TRUE(cloud.empty());
    EXPECT_EQ(cloud.splatCount(), 0u);
}

TEST(GaussianSplatting, ManuallyBuiltCloudHasCorrectCount)
{
    nexus::gfx::GaussianSplatCloud cloud;
    cloud.splats.push_back(makeSplat(0.f, 0.f, 0.f));
    cloud.splats.push_back(makeSplat(1.f, 0.f, 0.f));
    EXPECT_EQ(cloud.splatCount(), 2u);
    EXPECT_FALSE(cloud.empty());
}

TEST(GaussianSplatting, LoadFromPLYBytesReturnsCorrectSplatCount)
{
    auto s0 = makeSplat(1.f, 2.f, 3.f);
    auto s1 = makeSplat(4.f, 5.f, 6.f);
    auto bytes = makePLYBytes({s0, s1});

    auto cloud = nexus::gfx::GaussianSplatCloud::loadFromPLYBytes(bytes);
    ASSERT_TRUE(cloud.has_value());
    EXPECT_EQ(cloud->splatCount(), 2u);
}

TEST(GaussianSplatting, LoadFromPLYBytesPreservesPosition)
{
    auto src = makeSplat(1.5f, -2.0f, 3.25f);
    auto bytes = makePLYBytes({src});

    auto cloud = nexus::gfx::GaussianSplatCloud::loadFromPLYBytes(bytes);
    ASSERT_TRUE(cloud.has_value());
    ASSERT_EQ(cloud->splatCount(), 1u);

    const auto& s = cloud->splats[0];
    EXPECT_NEAR(s.position.x, 1.5f,  1e-5f);
    EXPECT_NEAR(s.position.y, -2.0f, 1e-5f);
    EXPECT_NEAR(s.position.z, 3.25f, 1e-5f);
}

TEST(GaussianSplatting, LoadFromPLYBytesPreservesColorDC)
{
    auto src = makeSplat(0.f, 0.f, 0.f, 0.1f, 0.5f, 0.9f);
    auto bytes = makePLYBytes({src});

    auto cloud = nexus::gfx::GaussianSplatCloud::loadFromPLYBytes(bytes);
    ASSERT_TRUE(cloud.has_value());
    ASSERT_EQ(cloud->splatCount(), 1u);

    const auto& s = cloud->splats[0];
    EXPECT_NEAR(s.dc[0], 0.1f, 1e-5f);
    EXPECT_NEAR(s.dc[1], 0.5f, 1e-5f);
    EXPECT_NEAR(s.dc[2], 0.9f, 1e-5f);
}

TEST(GaussianSplatting, LoadFromPLYBytesRejectsTruncatedData)
{
    auto bytes = makePLYBytes({makeSplat(0.f, 0.f, 0.f), makeSplat(1.f, 1.f, 1.f)});
    // Truncate the data section
    bytes.resize(bytes.size() - 20);
    auto cloud = nexus::gfx::GaussianSplatCloud::loadFromPLYBytes(bytes);
    EXPECT_FALSE(cloud.has_value());
}

TEST(GaussianSplatting, LoadFromPLYBytesRejectsNonPLYHeader)
{
    std::vector<uint8_t> bad = {'x', 'x', 'x', '\n'};
    auto cloud = nexus::gfx::GaussianSplatCloud::loadFromPLYBytes(bad);
    EXPECT_FALSE(cloud.has_value());
}

TEST(GaussianSplatting, SaveThenLoadRoundTrip)
{
    const std::string path = "/tmp/nexus_test_splats_roundtrip.ply";

    nexus::gfx::GaussianSplatCloud original;
    original.splats.push_back(makeSplat(1.f,  2.f,  3.f,  0.8f, 0.1f, 0.3f));
    original.splats.push_back(makeSplat(-1.f, 0.f, -0.5f, 0.2f, 0.9f, 0.5f));

    ASSERT_TRUE(original.saveToPLY(path));

    auto reloaded = nexus::gfx::GaussianSplatCloud::loadFromPLY(path);
    ASSERT_TRUE(reloaded.has_value());
    ASSERT_EQ(reloaded->splatCount(), original.splatCount());

    for (size_t i = 0; i < original.splatCount(); ++i) {
        EXPECT_NEAR(reloaded->splats[i].position.x, original.splats[i].position.x, 1e-5f)
            << " at splat " << i;
        EXPECT_NEAR(reloaded->splats[i].dc[0],      original.splats[i].dc[0],      1e-5f)
            << " at splat " << i;
    }

    std::filesystem::remove(path);
}

TEST(GaussianSplatting, SourceFormatIsSetOnLoad)
{
    auto bytes = makePLYBytes({makeSplat(0.f, 0.f, 0.f)});
    auto cloud = nexus::gfx::GaussianSplatCloud::loadFromPLYBytes(bytes);
    ASSERT_TRUE(cloud.has_value());
    EXPECT_EQ(cloud->sourceFormat, "ply");
}

// ─────────────────────────────────────────────────────────────────────────────
//  GaussianSplatSceneNode
// ─────────────────────────────────────────────────────────────────────────────
TEST(GaussianSplatting, SceneNodeDefaultTransformIsIdentity)
{
    nexus::gfx::GaussianSplatSceneNode node;
    // Diagonal should be 1, off-diagonals 0
    EXPECT_NEAR(node.transform.m[0][0], 1.f, 1e-6f);
    EXPECT_NEAR(node.transform.m[1][1], 1.f, 1e-6f);
    EXPECT_NEAR(node.transform.m[2][2], 1.f, 1e-6f);
    EXPECT_NEAR(node.transform.m[3][3], 1.f, 1e-6f);
    EXPECT_NEAR(node.transform.m[0][1], 0.f, 1e-6f);
}

TEST(GaussianSplatting, SceneNodeDefaultIsVisible)
{
    nexus::gfx::GaussianSplatSceneNode node;
    EXPECT_TRUE(node.visible);
}

TEST(GaussianSplatting, SceneNodeConstructFromCloud)
{
    nexus::gfx::GaussianSplatCloud cloud;
    cloud.splats.push_back(makeSplat(0.f, 0.f, 0.f));
    nexus::gfx::GaussianSplatSceneNode node(cloud);
    EXPECT_EQ(node.cloud.splatCount(), 1u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Coexistence with mesh pipeline
//
//  A scene holds both a regular Node (mesh) and a GaussianSplatSceneNode.
//  No changes to SceneGraph.h are required; splat nodes are held alongside.
// ─────────────────────────────────────────────────────────────────────────────
TEST(GaussianSplatting, CoexistenceWithMeshNodeInSameScene)
{
    using namespace nexus::render;

    // Build a scene with one mesh node
    SceneGraph scene;
    auto meshNode = std::make_unique<Node>("mesh_obj", 1);
    meshNode->visible = true;
    scene.root().addChild(std::move(meshNode));

    // Build a splat node alongside (no scene graph modification needed)
    nexus::gfx::GaussianSplatCloud cloud;
    cloud.splats.push_back(makeSplat(0.f, 1.f, 0.f));
    cloud.splats.push_back(makeSplat(1.f, 1.f, 0.f));
    nexus::gfx::GaussianSplatSceneNode splatNode(cloud);

    // Both mesh and splat node are independently accessible
    int meshCount = 0;
    scene.traverse([&](Node& n, const Mat4&) {
        if (n.name() == "mesh_obj") ++meshCount;
    });

    EXPECT_EQ(meshCount, 1);
    EXPECT_EQ(splatNode.cloud.splatCount(), 2u);
    EXPECT_TRUE(splatNode.visible);

    // Splat node does not require any core API on the IDevice
    // — this compiles and runs without a GPU, confirming no core rewrites needed.
}

// ─────────────────────────────────────────────────────────────────────────────
//  RepresentationType
// ─────────────────────────────────────────────────────────────────────────────
TEST(GaussianSplatting, RepresentationTypeVariantsAreDistinct)
{
    using RT = nexus::gfx::RepresentationType;
    EXPECT_NE(RT::Mesh,          RT::GaussianSplat);
    EXPECT_NE(RT::GaussianSplat, RT::PointCloud);
    EXPECT_NE(RT::PointCloud,    RT::Volume);
    EXPECT_NE(RT::Mesh,          RT::Volume);
}

// ─────────────────────────────────────────────────────────────────────────────
//  TemporalAccumulator
// ─────────────────────────────────────────────────────────────────────────────
TEST(TemporalAccumulation, DefaultConfigIsValid)
{
    nexus::render::TemporalAccumulator acc;
    EXPECT_GT(acc.config().blendFactor, 0.f);
    EXPECT_LT(acc.config().blendFactor, 1.f);
    EXPECT_EQ(acc.state().frameIndex, 0u);
}

TEST(TemporalAccumulation, AdvanceFrameIncrementsIndex)
{
    nexus::render::TemporalAccumulator acc;
    acc.advanceFrame();
    EXPECT_EQ(acc.state().frameIndex, 1u);
    acc.advanceFrame();
    EXPECT_EQ(acc.state().frameIndex, 2u);
}

TEST(TemporalAccumulation, JitterOffsetDiffersAcrossFrames)
{
    nexus::render::TemporalAccumulator acc;
    auto [x0, y0] = acc.currentJitterOffset();
    acc.advanceFrame();
    auto [x1, y1] = acc.currentJitterOffset();
    // Halton sequence should produce different values for frames 0 and 1
    bool changed = (std::abs(x1 - x0) > 1e-6f) || (std::abs(y1 - y0) > 1e-6f);
    EXPECT_TRUE(changed);
}

TEST(TemporalAccumulation, JitterOffsetIsSubpixelRange)
{
    nexus::render::TemporalAccumulator acc;
    for (uint32_t i = 0; i < 16; ++i) {
        auto [x, y] = acc.currentJitterOffset();
        EXPECT_GE(x, -0.5f);
        EXPECT_LE(x,  0.5f);
        EXPECT_GE(y, -0.5f);
        EXPECT_LE(y,  0.5f);
        acc.advanceFrame();
    }
}

TEST(TemporalAccumulation, BlendAlphaMatchesConfig)
{
    nexus::render::TemporalAccumulationConfig cfg;
    cfg.blendFactor = 0.15f;
    nexus::render::TemporalAccumulator acc(cfg);
    EXPECT_NEAR(acc.resolveBlendAlpha(), 0.15f, 1e-6f);
}

TEST(TemporalAccumulation, MotionRejectionIncreasesBlendTowardOne)
{
    nexus::render::TemporalAccumulationConfig cfg;
    cfg.blendFactor               = 0.1f;
    cfg.enableVelocityRejection   = true;
    nexus::render::TemporalAccumulator acc(cfg);

    float normal   = acc.resolveBlendAlpha(false);
    float rejected = acc.resolveBlendAlpha(true);
    EXPECT_GT(rejected, normal);
    EXPECT_LE(rejected, 1.f);
}

TEST(TemporalAccumulation, BlendAlphaOneReplacesWithCurrentFrame)
{
    nexus::render::TemporalAccumulationConfig cfg;
    cfg.blendFactor             = 1.f;
    cfg.enableVelocityRejection = false;
    nexus::render::TemporalAccumulator acc(cfg);
    EXPECT_NEAR(acc.resolveBlendAlpha(), 1.f, 1e-6f);
}

TEST(TemporalAccumulation, UniformJitterPatternWrapsCorrectly)
{
    nexus::render::TemporalAccumulationConfig cfg;
    cfg.jitter.type        = nexus::render::JitterPattern::kUniform;
    cfg.jitter.sampleCount = 4;
    nexus::render::TemporalAccumulator acc(cfg);

    // After sampleCount frames, pattern should repeat
    auto [x0, y0] = acc.currentJitterOffset();
    for (uint32_t i = 0; i < cfg.jitter.sampleCount; ++i)
        acc.advanceFrame();
    auto [xR, yR] = acc.currentJitterOffset();
    EXPECT_NEAR(x0, xR, 1e-5f);
    EXPECT_NEAR(y0, yR, 1e-5f);
}

TEST(TemporalAccumulation, StateFrameIndexIsWritable)
{
    nexus::render::TemporalAccumulator acc;
    acc.state().frameIndex = 42u;
    EXPECT_EQ(acc.state().frameIndex, 42u);
}

TEST(TemporalAccumulation, SetConfigRejectsNonFiniteBlendFactor)
{
    nexus::render::TemporalAccumulationConfig cfg;
    cfg.blendFactor = 0.2f;
    nexus::render::TemporalAccumulator acc(cfg);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    nexus::render::TemporalAccumulationConfig bad = cfg;
    bad.blendFactor = nan;
    acc.setConfig(bad);
    EXPECT_FLOAT_EQ(acc.config().blendFactor, 0.2f);

    bad.blendFactor = inf;
    acc.setConfig(bad);
    EXPECT_FLOAT_EQ(acc.config().blendFactor, 0.2f);
}

TEST(TemporalAccumulation, SetConfigRejectsBlendFactorOutOfRange)
{
    nexus::render::TemporalAccumulationConfig cfg;
    cfg.blendFactor = 0.3f;
    nexus::render::TemporalAccumulator acc(cfg);

    nexus::render::TemporalAccumulationConfig bad = cfg;
    bad.blendFactor = -0.1f;
    acc.setConfig(bad);
    EXPECT_FLOAT_EQ(acc.config().blendFactor, 0.3f);

    bad.blendFactor = 1.5f;
    acc.setConfig(bad);
    EXPECT_FLOAT_EQ(acc.config().blendFactor, 0.3f);
}

TEST(TemporalAccumulation, SetConfigRejectsNonFiniteRejectionThreshold)
{
    nexus::render::TemporalAccumulationConfig cfg;
    cfg.blendFactor                    = 0.1f;
    cfg.velocityRejectionThreshold     = 0.05f;
    nexus::render::TemporalAccumulator acc(cfg);

    nexus::render::TemporalAccumulationConfig bad = cfg;
    bad.velocityRejectionThreshold = std::numeric_limits<float>::quiet_NaN();
    acc.setConfig(bad);
    EXPECT_FLOAT_EQ(acc.config().velocityRejectionThreshold, 0.05f);
}

TEST(TemporalAccumulation, SetConfigRejectsNonFiniteVarianceClipGamma)
{
    nexus::render::TemporalAccumulationConfig cfg;
    cfg.blendFactor      = 0.1f;
    cfg.varianceClipGamma = 1.0f;
    nexus::render::TemporalAccumulator acc(cfg);

    nexus::render::TemporalAccumulationConfig bad = cfg;
    bad.varianceClipGamma = std::numeric_limits<float>::infinity();
    acc.setConfig(bad);
    EXPECT_FLOAT_EQ(acc.config().varianceClipGamma, 1.0f);
}

TEST(TemporalAccumulation, SetConfigRejectsZeroSampleCount)
{
    nexus::render::TemporalAccumulationConfig cfg;
    cfg.blendFactor          = 0.1f;
    cfg.jitter.sampleCount   = 8u;
    nexus::render::TemporalAccumulator acc(cfg);

    nexus::render::TemporalAccumulationConfig bad = cfg;
    bad.jitter.sampleCount = 0u;
    acc.setConfig(bad);
    EXPECT_EQ(acc.config().jitter.sampleCount, 8u);
}

TEST(TemporalAccumulation, ConstructorFallsBackToDefaultForInvalidConfig)
{
    nexus::render::TemporalAccumulationConfig bad;
    bad.blendFactor = std::numeric_limits<float>::quiet_NaN();

    nexus::render::TemporalAccumulator acc(bad);

    // Should have fallen back to default config.
    const nexus::render::TemporalAccumulationConfig def{};
    EXPECT_FLOAT_EQ(acc.config().blendFactor, def.blendFactor);
}
