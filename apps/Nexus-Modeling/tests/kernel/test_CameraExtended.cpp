#define private public
#include <nexus/render/Camera.h>
#undef private
#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace nexus::render;

namespace {

bool nearlyEqual(float a, float b, float eps = 1e-4f)
{
    return std::abs(a - b) <= eps;
}

} // namespace

TEST(CameraExtended, Mat4InverseIdentityRoundTrip)
{
    Mat4 m = Mat4::identity();
    Mat4 inv = m.inverse();
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            EXPECT_TRUE(nearlyEqual(inv.m[r][c], m.m[r][c]));
}

TEST(CameraExtended, Mat4InverseTranslationRoundTrip)
{
    Mat4 t = Mat4::identity();
    t.m[0][3] = 3.0f;
    t.m[1][3] = -2.0f;
    t.m[2][3] = 5.5f;

    Mat4 inv = t.inverse();
    Mat4 id = t * inv;

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            const float expected = (r == c) ? 1.0f : 0.0f;
            EXPECT_TRUE(nearlyEqual(id.m[r][c], expected, 1e-3f));
        }
    }
}

TEST(CameraExtended, InvViewProjUpdatesAfterProjectionAndLookAt)
{
    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 5000.f);
    cam.lookAt({4.f, 3.f, 10.f}, {0.f, 0.f, 0.f});

    const auto& inv = cam.ubo().invViewProj;
    // Identity matrix has zeros off-diagonal in these spots.
    EXPECT_NE(inv.m[0][3], 0.f);
    EXPECT_NE(inv.m[1][3], 0.f);
}

TEST(CameraExtended, PerspectiveRejectsDegenerateFiniteParameters)
{
    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 5000.f);

    const auto before = cam.ubo();

    cam.setPerspective(0.f, 16.f / 9.f, 0.1f, 5000.f);
    EXPECT_FLOAT_EQ(cam.ubo().fovY, before.fovY);

    cam.setPerspective(70.f, 0.f, 0.1f, 5000.f);
    EXPECT_FLOAT_EQ(cam.ubo().aspectRatio, before.aspectRatio);

    cam.setPerspective(70.f, 16.f / 9.f, 0.f, 5000.f);
    EXPECT_FLOAT_EQ(cam.ubo().nearPlane, before.nearPlane);

    cam.setPerspective(70.f, 16.f / 9.f, 10.f, 10.f);
    EXPECT_FLOAT_EQ(cam.ubo().farPlane, before.farPlane);
    EXPECT_TRUE(nearlyEqual(cam.ubo().projection.m[0][0], before.projection.m[0][0]));
    EXPECT_TRUE(nearlyEqual(cam.ubo().projection.m[1][1], before.projection.m[1][1]));
}

TEST(CameraExtended, OrthographicRejectsDegenerateFiniteParameters)
{
    Camera cam;
    cam.setOrthographic(10.f, 8.f, 0.1f, 100.f);

    const auto before = cam.ubo().projection;

    cam.setOrthographic(0.f, 8.f, 0.1f, 100.f);
    EXPECT_TRUE(nearlyEqual(cam.ubo().projection.m[0][0], before.m[0][0]));

    cam.setOrthographic(10.f, -2.f, 0.1f, 100.f);
    EXPECT_TRUE(nearlyEqual(cam.ubo().projection.m[1][1], before.m[1][1]));

    cam.setOrthographic(10.f, 8.f, 5.f, 5.f);
    EXPECT_TRUE(nearlyEqual(cam.ubo().projection.m[2][2], before.m[2][2]));
    EXPECT_TRUE(nearlyEqual(cam.ubo().projection.m[3][3], before.m[3][3]));
}

TEST(CameraExtended, LookAtRejectsNonFiniteVectors)
{
    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 5000.f);
    cam.lookAt({4.f, 3.f, 10.f}, {0.f, 0.f, 0.f});

    const auto before = cam.ubo();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    cam.lookAt({nan, 3.f, 10.f}, {0.f, 0.f, 0.f});
    EXPECT_TRUE(nearlyEqual(cam.ubo().position.x, before.position.x));
    EXPECT_TRUE(nearlyEqual(cam.ubo().view.m[0][3], before.view.m[0][3]));

    cam.lookAt({4.f, 3.f, 10.f}, {0.f, inf, 0.f});
    EXPECT_TRUE(nearlyEqual(cam.ubo().direction.y, before.direction.y));
    EXPECT_TRUE(nearlyEqual(cam.ubo().view.m[1][3], before.view.m[1][3]));

    cam.lookAt({4.f, 3.f, 10.f}, {0.f, 0.f, 0.f}, {0.f, nan, 0.f});
    EXPECT_TRUE(nearlyEqual(cam.ubo().viewProj.m[0][0], before.viewProj.m[0][0]));
    EXPECT_TRUE(nearlyEqual(cam.ubo().invViewProj.m[2][3], before.invViewProj.m[2][3]));
}

TEST(CameraExtended, FrustumExtractionZeroesDegeneratePlanesForSingularFiniteViewProj)
{
    Camera cam;
    cam.m_ubo.viewProj = {};
    cam.m_ubo.viewProj.m[0][3] = 1.f;
    cam.m_ubo.viewProj.m[1][3] = -1.f;
    cam.m_ubo.viewProj.m[2][0] = 1.f;
    cam.m_ubo.viewProj.m[3][3] = 2.f;

    cam.extractFrustum();

    const Frustum& frustum = cam.frustum();
    int zeroPlaneCount = 0;
    for (const Vec4& plane : frustum.planes) {
        EXPECT_TRUE(std::isfinite(plane.x));
        EXPECT_TRUE(std::isfinite(plane.y));
        EXPECT_TRUE(std::isfinite(plane.z));
        EXPECT_TRUE(std::isfinite(plane.w));

        if (plane.x == 0.f && plane.y == 0.f && plane.z == 0.f && plane.w == 0.f) {
            ++zeroPlaneCount;
        }
    }

    EXPECT_GE(zeroPlaneCount, 1);
    EXPECT_FLOAT_EQ(frustum.planes[0].x, 0.f);
    EXPECT_FLOAT_EQ(frustum.planes[0].y, 0.f);
    EXPECT_FLOAT_EQ(frustum.planes[0].z, 0.f);
    EXPECT_FLOAT_EQ(frustum.planes[0].w, 0.f);
}

TEST(CameraExtended, Vec3DotCrossNormalizeBehave)
{
    Vec3 x{1.f, 0.f, 0.f};
    Vec3 y{0.f, 1.f, 0.f};
    Vec3 z = x.cross(y);

    EXPECT_FLOAT_EQ(x.dot(y), 0.f);
    EXPECT_FLOAT_EQ(z.x, 0.f);
    EXPECT_FLOAT_EQ(z.y, 0.f);
    EXPECT_FLOAT_EQ(z.z, 1.f);

    Vec3 n = Vec3{2.f, 0.f, 0.f}.normalize();
    EXPECT_NEAR(n.x, 1.f, 1e-6f);
    EXPECT_NEAR(n.y, 0.f, 1e-6f);
    EXPECT_NEAR(n.z, 0.f, 1e-6f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  The distance overload of lookAt rejects non-finite input.
//
//  Its sibling lookAt(target, eye, up) has been covered above for a long time; this
//  overload was not, and its guard was written with std::isfinite while every other guard
//  in Camera.cpp uses the file's own bit-inspecting isFiniteFloat. That difference was not
//  cosmetic: the kernel was built with -ffast-math, under which std::isfinite reports NaN
//  and Inf as FINITE, so the guard passed them straight through and a NaN target reached
//  rebuildMatrices — from where it contaminates the view matrix, every world transform
//  derived from it, and any culling decision downstream. The flag is gone and the guard now
//  uses isFiniteFloat, which is correct either way. This pins the behaviour so neither can
//  regress silently again.
// ─────────────────────────────────────────────────────────────────────────────
TEST(CameraExtended, LookAtWithDistanceRejectsNonFiniteInput)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    Camera cam;
    cam.setPerspective(60.f, 16.f / 9.f, 0.1f, 100.f);
    cam.lookAt({4.f, 3.f, 10.f}, {0.f, 0.f, 0.f});
    const CameraUBO good = cam.ubo();

    auto unchanged = [&](const char* what) {
        const CameraUBO now = cam.ubo();
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                EXPECT_FLOAT_EQ(now.view.m[r][c], good.view.m[r][c])
                    << what << " modified the view matrix at [" << r << "][" << c << "]";
        EXPECT_FLOAT_EQ(now.position.x, good.position.x) << what;
        EXPECT_FLOAT_EQ(now.position.y, good.position.y) << what;
        EXPECT_FLOAT_EQ(now.position.z, good.position.z) << what;
    };

    cam.lookAt({nan, 0.f, 0.f}, 5.f);   unchanged("NaN target x");
    cam.lookAt({0.f, nan, 0.f}, 5.f);   unchanged("NaN target y");
    cam.lookAt({0.f, 0.f, nan}, 5.f);   unchanged("NaN target z");
    cam.lookAt({0.f, 0.f, 0.f}, nan);   unchanged("NaN distance");
    cam.lookAt({inf, 0.f, 0.f}, 5.f);   unchanged("Inf target x");
    cam.lookAt({0.f, 0.f, 0.f}, inf);   unchanged("Inf distance");
    cam.lookAt({0.f, 0.f, 0.f}, -inf);  unchanged("-Inf distance");
    cam.lookAt({0.f, 0.f, 0.f}, 0.f);   unchanged("zero distance");
    cam.lookAt({0.f, 0.f, 0.f}, -1.f);  unchanged("negative distance");

    // And a finite call still works, so the guard rejects rather than disables.
    cam.lookAt({1.f, 2.f, 3.f}, 7.f);
    const CameraUBO after = cam.ubo();
    bool anyFinite = true;
    for (int r = 0; r < 4 && anyFinite; ++r)
        for (int c = 0; c < 4; ++c)
            if (!std::isfinite(after.view.m[r][c])) { anyFinite = false; break; }
    EXPECT_TRUE(anyFinite) << "a valid lookAt produced a non-finite view matrix";
}
