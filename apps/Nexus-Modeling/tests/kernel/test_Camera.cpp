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

// ── Frustum intersection queries ──────────────────────────────────────────────
//
// Hand-built frustum bounding the box x∈[-1,1], y∈[-1,1], z∈[0,10] with
// inward, unit-length normals (matching Camera::extractFrustum's convention).
namespace {
Frustum makeBoxFrustum()
{
    Frustum f{};
    f.planes[0] = { 1.f,  0.f,  0.f,  1.f}; // x >= -1
    f.planes[1] = {-1.f,  0.f,  0.f,  1.f}; // x <=  1
    f.planes[2] = { 0.f,  1.f,  0.f,  1.f}; // y >= -1
    f.planes[3] = { 0.f, -1.f,  0.f,  1.f}; // y <=  1
    f.planes[4] = { 0.f,  0.f,  1.f,  0.f}; // z >=  0
    f.planes[5] = { 0.f,  0.f, -1.f, 10.f}; // z <= 10
    return f;
}
} // namespace

TEST(Camera, FrustumContainsPoint)
{
    const Frustum f = makeBoxFrustum();
    EXPECT_TRUE(f.containsPoint({0.f, 0.f, 5.f}));   // center
    EXPECT_TRUE(f.containsPoint({1.f, 1.f, 10.f}));  // far corner (boundary is inside)
    EXPECT_FALSE(f.containsPoint({0.f, 0.f, -1.f})); // behind near plane
    EXPECT_FALSE(f.containsPoint({2.f, 0.f, 5.f}));  // right of right plane
    EXPECT_FALSE(f.containsPoint({0.f, 0.f, 11.f})); // beyond far plane
}

TEST(Camera, FrustumIntersectsSphere)
{
    const Frustum f = makeBoxFrustum();
    EXPECT_TRUE(f.intersectsSphere({0.f, 0.f, 5.f}, 0.5f));  // fully inside
    EXPECT_FALSE(f.intersectsSphere({2.f, 0.f, 5.f}, 0.5f)); // sits 1.0 right of x=1, radius too small
    EXPECT_TRUE(f.intersectsSphere({2.f, 0.f, 5.f}, 1.5f));  // radius reaches back across x=1
    EXPECT_FALSE(f.intersectsSphere({0.f, 0.f, -2.f}, 1.f)); // entirely behind near plane
    EXPECT_TRUE(f.intersectsSphere({0.f, 0.f, -0.5f}, 1.f)); // straddles near plane
}

TEST(Camera, FrustumIntersectsAabb)
{
    const Frustum f = makeBoxFrustum();
    EXPECT_TRUE(f.intersectsAabb({{-0.5f, -0.5f, 4.f}, {0.5f, 0.5f, 6.f}})); // inside
    EXPECT_FALSE(f.intersectsAabb({{2.f, 2.f, 11.f}, {3.f, 3.f, 12.f}}));    // fully outside
    EXPECT_TRUE(f.intersectsAabb({{0.5f, -0.5f, 4.f}, {3.f, 0.5f, 6.f}}));   // straddles right plane
    EXPECT_TRUE(f.intersectsAabb({{-5.f, -5.f, -5.f}, {5.f, 5.f, 15.f}}));   // encloses the frustum
}

TEST(Camera, FrustumDegeneratePlaneIsConservative)
{
    // An all-zero frustum (every plane degenerate) must never cull: dot+d == 0
    // satisfies the >= 0 inside test, so everything is reported visible.
    Frustum f{};
    EXPECT_TRUE(f.containsPoint({1000.f, -1000.f, 1000.f}));
    EXPECT_TRUE(f.intersectsSphere({0.f, 0.f, 0.f}, 0.f));
    EXPECT_TRUE(f.intersectsAabb({{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}}));
}

TEST(Camera, FrustumFromRealCameraCullsBehindAndSides)
{
    Camera cam;
    cam.setPerspective(60.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}); // looking down -Z at the origin
    const Frustum& f = cam.frustum();

    EXPECT_TRUE(f.containsPoint({0.f, 0.f, 0.f}));      // target, in front
    EXPECT_FALSE(f.containsPoint({0.f, 0.f, 10.f}));    // behind the camera
    EXPECT_FALSE(f.containsPoint({1000.f, 0.f, 0.f}));  // far off to the side
    EXPECT_TRUE(f.intersectsSphere({0.f, 0.f, 0.f}, 1.f));
    EXPECT_TRUE(f.intersectsAabb({{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}}));
}

TEST(Camera, AabbCenterAndExtents)
{
    const Aabb box{{-2.f, 0.f, 1.f}, {4.f, 6.f, 5.f}};
    const Vec3 c = box.center();
    const Vec3 e = box.extents();
    EXPECT_FLOAT_EQ(c.x, 1.f);  EXPECT_FLOAT_EQ(c.y, 3.f);  EXPECT_FLOAT_EQ(c.z, 3.f);
    EXPECT_FLOAT_EQ(e.x, 3.f);  EXPECT_FLOAT_EQ(e.y, 3.f);  EXPECT_FLOAT_EQ(e.z, 2.f);
}

// Pins the matrix convention: column-vector (clip = viewProj * world) with
// reversed-Z Vulkan NDC. This is the ground truth the frustum extraction relies
// on; if the projection layout or P*V order regresses, this fails first.
TEST(Camera, PerspectiveProjectsToReversedZNdc)
{
    Camera cam;
    cam.setPerspective(60.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}); // look down -Z
    const Mat4& vp = cam.ubo().viewProj;

    auto ndc = [&](Vec3 world) {
        const Vec4 clip = vp * Vec4{world.x, world.y, world.z, 1.f};
        return Vec3{clip.x / clip.w, clip.y / clip.w, clip.z / clip.w};
    };

    // Near plane (world z = 5 - 0.1 = 4.9) -> NDC z = 1; far plane (z = -95) -> 0.
    EXPECT_NEAR(ndc({0.f, 0.f, 4.9f}).z, 1.f, 1e-3f);
    EXPECT_NEAR(ndc({0.f, 0.f, -95.f}).z, 0.f, 1e-3f);

    // The look-at target projects to screen center, within the depth range.
    const Vec3 c = ndc({0.f, 0.f, 0.f});
    EXPECT_NEAR(c.x, 0.f, 1e-4f);
    EXPECT_NEAR(c.y, 0.f, 1e-4f);
    EXPECT_GT(c.z, 0.f);
    EXPECT_LT(c.z, 1.f);
}
