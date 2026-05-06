#include <nexus/render/Camera.h>
#include <gtest/gtest.h>

#include <cmath>

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
