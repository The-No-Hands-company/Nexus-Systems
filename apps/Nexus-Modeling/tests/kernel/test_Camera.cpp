// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Camera
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Camera.h>
#include <gtest/gtest.h>
#include <cmath>
#include <limits>

using namespace nexus::render;

TEST(Camera, PerspectiveFieldsSet)
{
    Camera cam;
    cam.setPerspective(60.f, 16.f/9.f, 0.1f, 1000.f);
    const auto& ubo = cam.ubo();
    EXPECT_FLOAT_EQ(ubo.fovY,        60.f);
    EXPECT_FLOAT_EQ(ubo.nearPlane,   0.1f);
    EXPECT_FLOAT_EQ(ubo.farPlane,    1000.f);
    EXPECT_NEAR(ubo.aspectRatio, 16.f/9.f, 1e-5f);
}

TEST(Camera, ProjectionNonZero)
{
    Camera cam;
    cam.setPerspective(60.f, 1.f, 0.1f, 100.f);
    const auto& P = cam.ubo().projection;
    // Diagonal elements must be non-zero
    EXPECT_NE(P.m[0][0], 0.f);
    EXPECT_NE(P.m[1][1], 0.f);
}

TEST(Camera, LookAtSetsPosition)
{
    Camera cam;
    cam.setPerspective(60.f, 1.f, 0.1f, 1000.f);
    cam.lookAt({0,0,5}, {0,0,0});
    auto pos = cam.position();
    EXPECT_NEAR(pos.z, 5.f, 1e-5f);
}

TEST(Camera, JitterRoundtrip)
{
    Camera cam;
    cam.setJitter(0.25f, -0.1f);
    EXPECT_FLOAT_EQ(cam.ubo().jitter.x, 0.25f);
    EXPECT_FLOAT_EQ(cam.ubo().jitter.y, -0.1f);
    cam.clearJitter();
    EXPECT_FLOAT_EQ(cam.ubo().jitter.x, 0.f);
}

TEST(Camera, TickAdvancesPrevViewProj)
{
    Camera cam;
    cam.setPerspective(60.f, 1.f, 0.1f, 100.f);
    cam.lookAt({1,2,3}, {0,0,0});
    auto vp1 = cam.ubo().viewProj;
    cam.tick();
    auto prev = cam.ubo().prevViewProj;
    // prevViewProj should match the VP from before tick
    EXPECT_EQ(prev.m[0][0], vp1.m[0][0]);
}

TEST(Camera, SetPerspectiveRejectsNaNFOV)
{
    Camera cam;
    cam.setPerspective(60.f, 1.f, 0.1f, 100.f);
    float fovBefore = cam.ubo().fovY;
    // Attempt to set NaN FOV; camera should reject and preserve prior value
    cam.setPerspective(std::numeric_limits<float>::quiet_NaN(), 1.f, 0.1f, 100.f);
    EXPECT_FLOAT_EQ(cam.ubo().fovY, fovBefore);
}

TEST(Camera, SetPerspectiveRejectsInfAspect)
{
    Camera cam;
    cam.setPerspective(60.f, 1.5f, 0.1f, 100.f);
    float aspectBefore = cam.ubo().aspectRatio;
    // Attempt to set Inf aspect; camera should reject and preserve prior value
    cam.setPerspective(60.f, std::numeric_limits<float>::infinity(), 0.1f, 100.f);
    EXPECT_FLOAT_EQ(cam.ubo().aspectRatio, aspectBefore);
}

TEST(Camera, SetOrthographicRejectsNaNWidth)
{
    Camera cam;
    cam.setOrthographic(10.f, 10.f, 0.1f, 100.f);
    auto prevMode = cam.ubo().projection.m[0][0];  // P[0][0] = 2/width
    // Attempt to set NaN width; camera should reject and preserve prior state
    cam.setOrthographic(std::numeric_limits<float>::quiet_NaN(), 10.f, 0.1f, 100.f);
    EXPECT_FLOAT_EQ(cam.ubo().projection.m[0][0], prevMode);
}

TEST(Camera, SetJitterRejectsNaNX)
{
    Camera cam;
    cam.setJitter(0.25f, -0.1f);
    float prevX = cam.ubo().jitter.x;
    // Attempt to set NaN jitter X; camera should reject and preserve prior value
    cam.setJitter(std::numeric_limits<float>::quiet_NaN(), -0.1f);
    EXPECT_FLOAT_EQ(cam.ubo().jitter.x, prevX);
}

TEST(Camera, SetJitterRejectsInfY)
{
    Camera cam;
    cam.setJitter(0.25f, -0.1f);
    float prevY = cam.ubo().jitter.y;
    // Attempt to set Inf jitter Y; camera should reject and preserve prior value
    cam.setJitter(0.25f, std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(cam.ubo().jitter.y, prevY);
}
