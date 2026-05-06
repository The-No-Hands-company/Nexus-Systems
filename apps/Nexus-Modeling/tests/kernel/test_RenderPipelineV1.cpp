// Month 3 — Render Pipeline v1 tests
// Covers R3.2 (RenderGraphValidator) and R3.3 (FrameCaptureExporter).

#include <nexus/render/RenderGraphValidator.h>
#include <nexus/render/FrameCaptureExporter.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/RenderContext.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;
using namespace nexus::render;

// ─────────────────────────────────────────────────────────────────────────────
//  R3.2 RenderGraphValidator — canonical pass ordering
// ─────────────────────────────────────────────────────────────────────────────

TEST(RenderGraphValidator, ValidFullFramePassSequenceIsClean)
{
    // Canonical: shadow → geometry → composite, correct layouts everywhere.
    RenderGraphDesc desc;
    desc.record(RenderPassType::Shadow,
                TextureLayout::Undefined,           // gbuffer not relevant
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Geometry,
                TextureLayout::ColorAttachment,
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Composite,
                TextureLayout::ShaderRead,
                TextureLayout::DepthRead);

    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(report.issues.empty());
}

TEST(RenderGraphValidator, GeometryBeforeShadowIsRejected)
{
    RenderGraphDesc desc;
    desc.record(RenderPassType::Geometry,
                TextureLayout::ColorAttachment,
                TextureLayout::Undefined);
    desc.record(RenderPassType::Shadow,
                TextureLayout::Undefined,
                TextureLayout::DepthWrite);

    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_FALSE(report.valid);
    ASSERT_FALSE(report.issues.empty());
    EXPECT_EQ(report.issues[0].code, RenderGraphIssueCode::GeometryBeforeShadow);
    EXPECT_EQ(report.issues[0].passIndex, 0u);
}

TEST(RenderGraphValidator, CompositeBeforeGeometryIsRejected)
{
    RenderGraphDesc desc;
    desc.record(RenderPassType::Shadow,
                TextureLayout::Undefined,
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Composite,
                TextureLayout::ShaderRead,
                TextureLayout::DepthRead);

    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_FALSE(report.valid);
    const bool hasCode = std::any_of(report.issues.begin(), report.issues.end(),
        [](const RenderGraphIssue& i) {
            return i.code == RenderGraphIssueCode::CompositeBeforeGeometry;
        });
    EXPECT_TRUE(hasCode);
}

TEST(RenderGraphValidator, CompositeBeforeShadowIsRejected)
{
    // Geometry → composite (no shadow at all).
    RenderGraphDesc desc;
    desc.record(RenderPassType::Geometry,
                TextureLayout::ColorAttachment,
                TextureLayout::Undefined);
    desc.record(RenderPassType::Composite,
                TextureLayout::ShaderRead,
                TextureLayout::DepthRead);

    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_FALSE(report.valid);
    const bool hasGeomBefore = std::any_of(report.issues.begin(), report.issues.end(),
        [](const RenderGraphIssue& i) {
            return i.code == RenderGraphIssueCode::GeometryBeforeShadow;
        });
    const bool hasCompBefore = std::any_of(report.issues.begin(), report.issues.end(),
        [](const RenderGraphIssue& i) {
            return i.code == RenderGraphIssueCode::CompositeBeforeShadow;
        });
    EXPECT_TRUE(hasGeomBefore);
    EXPECT_TRUE(hasCompBefore);
}

// ─────────────────────────────────────────────────────────────────────────────
//  R3.2 RenderGraphValidator — resource state rules
// ─────────────────────────────────────────────────────────────────────────────

TEST(RenderGraphValidator, GBufferNotColorAttachmentAtGeometryIsRejected)
{
    RenderGraphDesc desc;
    desc.record(RenderPassType::Shadow,
                TextureLayout::Undefined,
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Geometry,
                TextureLayout::ShaderRead,   // wrong — must be ColorAttachment
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Composite,
                TextureLayout::ShaderRead,
                TextureLayout::DepthRead);

    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_FALSE(report.valid);
    const bool hasCode = std::any_of(report.issues.begin(), report.issues.end(),
        [](const RenderGraphIssue& i) {
            return i.code == RenderGraphIssueCode::GBufferNotColorAttachmentAtGeometry;
        });
    EXPECT_TRUE(hasCode);
}

TEST(RenderGraphValidator, GBufferNotShaderReadAtCompositeIsRejected)
{
    RenderGraphDesc desc;
    desc.record(RenderPassType::Shadow,
                TextureLayout::Undefined,
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Geometry,
                TextureLayout::ColorAttachment,
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Composite,
                TextureLayout::ColorAttachment,  // wrong — must be ShaderRead
                TextureLayout::DepthRead);

    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_FALSE(report.valid);
    const bool hasCode = std::any_of(report.issues.begin(), report.issues.end(),
        [](const RenderGraphIssue& i) {
            return i.code == RenderGraphIssueCode::GBufferNotShaderReadAtComposite;
        });
    EXPECT_TRUE(hasCode);
}

TEST(RenderGraphValidator, ShadowAtlasNotDepthWriteAtShadowIsRejected)
{
    RenderGraphDesc desc;
    desc.record(RenderPassType::Shadow,
                TextureLayout::Undefined,
                TextureLayout::ShaderRead);  // wrong — must be DepthWrite
    desc.record(RenderPassType::Geometry,
                TextureLayout::ColorAttachment,
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Composite,
                TextureLayout::ShaderRead,
                TextureLayout::DepthRead);

    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_FALSE(report.valid);
    const bool hasCode = std::any_of(report.issues.begin(), report.issues.end(),
        [](const RenderGraphIssue& i) {
            return i.code == RenderGraphIssueCode::ShadowAtlasNotDepthWriteAtShadow;
        });
    EXPECT_TRUE(hasCode);
}

TEST(RenderGraphValidator, ShadowAtlasNotDepthReadAtCompositeIsRejected)
{
    RenderGraphDesc desc;
    desc.record(RenderPassType::Shadow,
                TextureLayout::Undefined,
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Geometry,
                TextureLayout::ColorAttachment,
                TextureLayout::DepthWrite);
    desc.record(RenderPassType::Composite,
                TextureLayout::ShaderRead,
                TextureLayout::DepthWrite);  // wrong — must be DepthRead

    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_FALSE(report.valid);
    const bool hasCode = std::any_of(report.issues.begin(), report.issues.end(),
        [](const RenderGraphIssue& i) {
            return i.code == RenderGraphIssueCode::ShadowAtlasNotDepthReadAtComposite;
        });
    EXPECT_TRUE(hasCode);
}

TEST(RenderGraphValidator, EmptyDescIsValid)
{
    RenderGraphDesc desc;
    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(report.issues.empty());
}

TEST(RenderGraphValidator, MultipleViolationsAreAllReported)
{
    // Geometry before shadow AND gbuffer layout wrong.
    RenderGraphDesc desc;
    desc.record(RenderPassType::Geometry,
                TextureLayout::ShaderRead,   // wrong layout
                TextureLayout::Undefined);

    const auto report = RenderGraphValidator::validate(desc);
    EXPECT_FALSE(report.valid);
    EXPECT_GE(report.issues.size(), 2u); // at least ordering + state issue
}

// ─────────────────────────────────────────────────────────────────────────────
//  R3.3 NullFrameCaptureExporter — basic capture lifecycle
// ─────────────────────────────────────────────────────────────────────────────

TEST(FrameCaptureExporter, NoSnapshotBeforeFirstEndCapture)
{
    NullFrameCaptureExporter exporter;
    EXPECT_FALSE(exporter.hasSnapshot());
    EXPECT_EQ(exporter.totalFramesCaptured(), 0u);
}

TEST(FrameCaptureExporter, SingleFrameCaptureProducesCorrectSnapshot)
{
    NullFrameCaptureExporter exporter;
    exporter.beginCapture(0);

    FramePassEntry shadowEntry{};
    shadowEntry.passType       = RenderPassType::Shadow;
    shadowEntry.drawCalls      = 12;
    shadowEntry.triangles      = 360;
    shadowEntry.shadowAtlasLayout = TextureLayout::DepthWrite;
    exporter.recordPass(shadowEntry);

    FramePassEntry geoEntry{};
    geoEntry.passType           = RenderPassType::Geometry;
    geoEntry.drawCalls          = 42;
    geoEntry.triangles          = 1260;
    geoEntry.gbufferColorLayout = TextureLayout::ColorAttachment;
    exporter.recordPass(geoEntry);

    FramePassEntry compEntry{};
    compEntry.passType           = RenderPassType::Composite;
    compEntry.drawCalls          = 1;
    compEntry.gbufferColorLayout = TextureLayout::ShaderRead;
    compEntry.shadowAtlasLayout  = TextureLayout::DepthRead;
    exporter.recordPass(compEntry);

    exporter.endCapture();

    ASSERT_TRUE(exporter.hasSnapshot());
    EXPECT_EQ(exporter.totalFramesCaptured(), 1u);

    const auto& snap = exporter.lastSnapshot();
    EXPECT_EQ(snap.frameIndex, 0u);
    ASSERT_EQ(snap.passes.size(), 3u);
    EXPECT_EQ(snap.passes[0].passType, RenderPassType::Shadow);
    EXPECT_EQ(snap.passes[1].passType, RenderPassType::Geometry);
    EXPECT_EQ(snap.passes[2].passType, RenderPassType::Composite);
    EXPECT_EQ(snap.totalDrawCalls, 55u);    // 12+42+1
    EXPECT_EQ(snap.totalTriangles, 1620u);  // 360+1260+0
}

TEST(FrameCaptureExporter, FrameIndexMonotonicallyIncreasesAcrossFrames)
{
    NullFrameCaptureExporter exporter;

    for (uint64_t f = 0; f < 5; ++f) {
        exporter.beginCapture(f);
        exporter.endCapture();
        EXPECT_EQ(exporter.lastSnapshot().frameIndex, f);
    }
    EXPECT_EQ(exporter.totalFramesCaptured(), 5u);
}

TEST(FrameCaptureExporter, SnapshotIsStableAcrossIdenticalFrames)
{
    NullFrameCaptureExporter exporter;

    auto recordStandardFrame = [&](uint64_t idx) {
        exporter.beginCapture(idx);
        FramePassEntry e{};
        e.passType  = RenderPassType::Geometry;
        e.drawCalls = 10;
        e.triangles = 300;
        exporter.recordPass(e);
        exporter.endCapture();
    };

    recordStandardFrame(0);
    recordStandardFrame(1);

    const auto& snap0 = exporter.lastSnapshot();
    EXPECT_EQ(snap0.passes.size(), 1u);
    EXPECT_EQ(snap0.totalDrawCalls, 10u);
    EXPECT_EQ(snap0.totalTriangles, 300u);
}

TEST(FrameCaptureExporter, RecordWithoutBeginCaptureIsIgnored)
{
    NullFrameCaptureExporter exporter;
    FramePassEntry entry{};
    entry.passType = RenderPassType::Geometry;
    exporter.recordPass(entry);  // should be silently ignored
    exporter.endCapture();       // no active capture — noop

    EXPECT_FALSE(exporter.hasSnapshot());
    EXPECT_EQ(exporter.totalFramesCaptured(), 0u);
}

TEST(FrameCaptureExporter, EmptyFrameProducesValidZeroCountSnapshot)
{
    NullFrameCaptureExporter exporter;
    exporter.beginCapture(7);
    exporter.endCapture();

    ASSERT_TRUE(exporter.hasSnapshot());
    const auto& snap = exporter.lastSnapshot();
    EXPECT_EQ(snap.frameIndex, 7u);
    EXPECT_TRUE(snap.passes.empty());
    EXPECT_EQ(snap.totalDrawCalls, 0u);
    EXPECT_EQ(snap.totalTriangles, 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  R3.3 Renderer integration — capture and validation hooks
// ─────────────────────────────────────────────────────────────────────────────

namespace {

class RendererCaptureFixture : public ::testing::Test {
protected:
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain>    swapchain;
    std::unique_ptr<Renderer>      renderer;

    void SetUp() override
    {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);

        SwapchainDesc sd{};
        sd.extent = {1280, 720};
        swapchain = ctx->createSwapchain(sd);
        ASSERT_NE(swapchain, nullptr);

        renderer = std::make_unique<Renderer>(*ctx, *swapchain);
        ASSERT_NE(renderer, nullptr);
    }
};

} // anonymous namespace

TEST_F(RendererCaptureFixture, CaptureExporterAttachDetachRoundTrip)
{
    NullFrameCaptureExporter exporter;
    EXPECT_EQ(renderer->frameCaptureExporter(), nullptr);

    renderer->setFrameCaptureExporter(&exporter);
    EXPECT_EQ(renderer->frameCaptureExporter(), &exporter);

    renderer->setFrameCaptureExporter(nullptr);
    EXPECT_EQ(renderer->frameCaptureExporter(), nullptr);
}

TEST_F(RendererCaptureFixture, RenderGraphValidationDefaultsToDisabled)
{
    EXPECT_FALSE(renderer->isRenderGraphValidationEnabled());
    EXPECT_TRUE(renderer->lastRenderGraphReport().valid);
    EXPECT_TRUE(renderer->lastRenderGraphReport().issues.empty());
}

TEST_F(RendererCaptureFixture, RenderGraphValidationCanBeToggled)
{
    renderer->setRenderGraphValidationEnabled(true);
    EXPECT_TRUE(renderer->isRenderGraphValidationEnabled());

    renderer->setRenderGraphValidationEnabled(false);
    EXPECT_FALSE(renderer->isRenderGraphValidationEnabled());
}

TEST_F(RendererCaptureFixture, FullFrameWithExporterDoesNotCrash)
{
    NullFrameCaptureExporter exporter;
    renderer->setFrameCaptureExporter(&exporter);
    renderer->setRenderGraphValidationEnabled(true);

    Camera cam;
    cam.setPerspective(60.f, 16.f / 9.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 5.f, 10.f}, {0.f, 0.f, 0.f});
    SceneGraph sg;

    ASSERT_NO_THROW(renderer->beginFrame());
    ASSERT_NO_THROW(renderer->render(cam, sg));
    ASSERT_NO_THROW(renderer->endFrame());

    // Null backend uses the non-scheduler path; capture still fires through
    // beginFrame/endFrame hooks.
    EXPECT_TRUE(exporter.hasSnapshot());
    EXPECT_EQ(exporter.totalFramesCaptured(), 1u);
}
