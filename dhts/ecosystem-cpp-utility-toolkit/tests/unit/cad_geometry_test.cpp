#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "nexus/utility/scientific/mesh_quality_validator.h"
#include "nexus/utility/graphics/geometry_validator.h"
#include "nexus/utility/graphics/selection_utils.h"
#include "nexus/utility/graphics/shader_compiler_error_parser.h"
#include "nexus/utility/graphics/pipeline_state_dumper.h"
#include "nexus/utility/graphics/framebuffer_validator.h"
#include "nexus/utility/graphics/texture_leak_detector.h"

// ── MeshQualityValidator ────────────────────────────────────────────────────

TEST(MeshQualityValidatorTest, BasicTriangleQuality) {
    nexus::utility::scientific::MeshQualityValidator v;
    float verts[] = {0,0,0, 1,0,0, 0.5f,0.866f,0};
    unsigned idx[] = {0,1,2};
    v.setMesh(3, verts, 1, idx);
    EXPECT_NEAR(v.computeMinAngle(0), 60.0, 0.1);
    EXPECT_NEAR(v.computeAspectRatio(0), 1.0, 0.01);
    EXPECT_EQ(v.getDegenerateCount(), 0);
}

TEST(MeshQualityValidatorTest, DegenerateDetection) {
    nexus::utility::scientific::MeshQualityValidator v;
    float verts[] = {0,0,0, 1,0,0, 0.5f,0,0};
    unsigned idx[] = {0,1,2};
    v.setMesh(3, verts, 1, idx);
    EXPECT_GT(v.getDegenerateCount(), 0);
}

TEST(MeshQualityValidatorTest, WatertightCheck) {
    nexus::utility::scientific::MeshQualityValidator v;
    float verts[] = {0,0,0, 1,0,0, 0.5f,0.866f,0, 0.5f,0.289f,0.816f};
    unsigned idx[] = {0,1,2, 0,2,3, 0,3,1, 1,3,2};
    v.setMesh(4, verts, 4, idx);
    EXPECT_TRUE(v.isWatertight());
}

// ── GeometryValidator ───────────────────────────────────────────────────────

TEST(GeometryValidatorTest, FaceArea) {
    using V3 = nexus::utility::graphics::Vector3;
    V3 v0{0,0,0}, v1{3,0,0}, v2{0,4,0};
    float area = nexus::utility::graphics::GeometryValidator::computeFaceArea(v0, v1, v2);
    EXPECT_FLOAT_EQ(area, 6.0f);
}

TEST(GeometryValidatorTest, DegenerateFace) {
    using V3 = nexus::utility::graphics::Vector3;
    V3 v0{0,0,0}, v1{0,0,0}, v2{1,1,1};
    EXPECT_TRUE(nexus::utility::graphics::GeometryValidator::isDegenerate(v0, v1, v2, 0.001f));
}

// ── SelectionUtils ──────────────────────────────────────────────────────────

TEST(SelectionUtilsTest, RayTriangleHit) {
    using namespace nexus::utility::graphics;
    SelectionUtils::Ray ray{{0,0,-5}, {0,0,1}};
    Vector3 v0{-1,-1,0}, v1{1,-1,0}, v2{0,1,0};
    auto d = SelectionUtils::rayTriangleIntersect(ray, v0, v1, v2);
    ASSERT_TRUE(d.has_value());
    EXPECT_NEAR(*d, 5.0f, 0.01f);
}

TEST(SelectionUtilsTest, RayBoxIntersect) {
    using namespace nexus::utility::graphics;
    SelectionUtils::Ray ray{{0.5f,0.5f,-5}, {0,0,1}};
    auto d = SelectionUtils::rayBoxIntersect(ray, Vector3{0,0,0}, Vector3{1,1,1});
    ASSERT_TRUE(d.has_value());
    EXPECT_GT(*d, 0);
}

// ── ShaderCompilerErrorParser ───────────────────────────────────────────────

TEST(ShaderCompilerErrorParserTest, ParseGLSLError) {
    nexus::utility::graphics::ShaderCompilerErrorParser p;
    auto errors = p.parseErrors("ERROR: 0:42: 'foo' : undeclared identifier");
    EXPECT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].line, 42);
    EXPECT_TRUE(p.hasErrors());
}

TEST(ShaderCompilerErrorParserTest, ParseHLSLError) {
    nexus::utility::graphics::ShaderCompilerErrorParser p;
    auto errors = p.parseErrors("shader.hlsl(10,5): error X3000: syntax error");
    EXPECT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].line, 10);
    EXPECT_EQ(p.getErrorCount(), 1);
}

// ── PipelineStateDumper ─────────────────────────────────────────────────────

TEST(PipelineStateDumperTest, DumpToString) {
    nexus::utility::graphics::PipelineStateDumper psd;
    psd.setViewport(0, 0, 1920, 1080, 0.0f, 1.0f);
    psd.setDepthState(true, true, nexus::utility::graphics::PipelineStateDumper::CompareFunc::Less);
    auto s = psd.dumpToString();
    EXPECT_FALSE(s.empty());
    EXPECT_TRUE(s.find("1920") != std::string::npos);
}

// ── FramebufferValidator ────────────────────────────────────────────────────

TEST(FramebufferValidatorTest, MatchingDimensions) {
    nexus::utility::graphics::FramebufferValidator fv;
    fv.addColorAttachment(0, 1920, 1080, "RGBA8");
    fv.setDepthAttachment(1920, 1080, "D32");
    EXPECT_TRUE(fv.validate());
    EXPECT_EQ(fv.getAttachmentCount(), 2);
}

TEST(FramebufferValidatorTest, MismatchedDimensions) {
    nexus::utility::graphics::FramebufferValidator fv;
    fv.addColorAttachment(0, 1920, 1080, "RGBA8");
    fv.setDepthAttachment(1024, 768, "D32");
    EXPECT_FALSE(fv.validate());
}

// ── TextureLeakDetector ────────────────────────────────────────────────────

TEST(TextureLeakDetectorTest, RegisterAndLeak) {
    nexus::utility::graphics::TextureLeakDetector tld;
    tld.registerTexture(1, 1024, 1024, 1, "RGBA8", "albedo");
    tld.registerTexture(2, 512, 512, 1, "RGBA8", "normal");
    EXPECT_EQ(tld.getActiveTextureCount(), 2);
    tld.unregisterTexture(1);
    EXPECT_EQ(tld.getActiveTextureCount(), 1);
}
