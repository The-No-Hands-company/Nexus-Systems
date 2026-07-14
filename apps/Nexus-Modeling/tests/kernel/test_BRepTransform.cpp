// Foundation — analytic B-rep affine transform. Body::transform moves the whole
// body consistently (vertices AND every Curve/Surface frame) so both validators
// stay clean; radii and Line params scale with a uniform scale, Circle angles
// are preserved. Unsupported linear parts (shear, non-uniform, reflection) and
// non-finite matrices are rejected, leaving the body unchanged.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <bit>
#include <cmath>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;
using nexus::render::Vec4;
using nexus::render::Mat4;
using PC = Body::PointContainment;

namespace {
Mat4 trans(Vec3 t)
{
    Mat4 m = Mat4::identity();
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
    return m;
}
Mat4 rotZ(float a)
{
    Mat4 m = Mat4::identity();
    const float c = std::cos(a), s = std::sin(a);
    m.m[0][0] = c;
    m.m[0][1] = -s;
    m.m[1][0] = s;
    m.m[1][1] = c;
    return m;
}
Mat4 uscale(float k)
{
    Mat4 m = Mat4::identity();
    m.m[0][0] = m.m[1][1] = m.m[2][2] = k;
    return m;
}
Vec3 apply(const Mat4& m, Vec3 p)
{
    const Vec4 r = m * Vec4{p.x, p.y, p.z, 1.f};
    return {r.x, r.y, r.z};
}
float dist(Vec3 a, Vec3 b)
{
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
}  // namespace

TEST(BRepTransform, TranslatedBoxStaysValidAndMovesMesh)
{
    Body a = makeBox(2.f, 2.f, 2.f);
    const Mesh before = a.toMesh(2);

    const Mat4 m = trans({5.f, 3.f, -2.f});
    ASSERT_TRUE(a.translate({5.f, 3.f, -2.f}));

    const auto ig = a.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);
    EXPECT_TRUE(a.checkGeometry().ok) << a.checkGeometry().reason;

    // Tessellation is the original mesh with every vertex translated (same order).
    const Mesh after = a.toMesh(2);
    const auto& pb = before.attributes().positions();
    const auto& pa = after.attributes().positions();
    ASSERT_EQ(pb.size(), pa.size());
    for (size_t i = 0; i < pb.size(); ++i)
        EXPECT_LT(dist(pa[i], apply(m, pb[i])), 1e-4f) << "vertex " << i;
}

TEST(BRepTransform, ClassificationIsTransformInvariant)
{
    const Body base = makeBox(2.f, 2.f, 2.f);
    const Mat4 m = rotZ(0.6f) * trans({2.f, -1.f, 0.5f});
    Body moved = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(moved.transform(m));

    // classifyPoint(T(p)) on T(body) must equal classifyPoint(p) on body.
    const Vec3 pts[] = {{0, 0, 0},   {0.5f, 0.5f, 0.5f}, {-0.7f, 0.2f, 0.3f},
                        {5, 0, 0},   {0, 0, 3},          {2, 2, 2}};
    for (const Vec3& p : pts)
        EXPECT_EQ(moved.classifyPoint(apply(m, p)), base.classifyPoint(p))
            << "p (" << p.x << "," << p.y << "," << p.z << ")";
}

TEST(BRepTransform, RotatedCylinderStaysValid)
{
    Body c = makeCylinder(1.f, 2.f, 24);
    ASSERT_TRUE(c.transform(rotZ(0.9f) * trans({1.f, 2.f, 3.f})));
    EXPECT_TRUE(c.checkIntegrity().ok) << c.checkIntegrity().reason;
    EXPECT_TRUE(c.checkGeometry().ok) << c.checkGeometry().reason;
    EXPECT_EQ(c.checkIntegrity().euler, 2);
}

TEST(BRepTransform, UniformScaleScalesSphereRadius)
{
    Body s = makeSphere(1.f, 8, 12);
    const Vec3 centreBefore{0, 0, 0};  // unit sphere centred at origin
    ASSERT_TRUE(s.transform(uscale(3.f)));
    EXPECT_TRUE(s.checkGeometry().ok) << s.checkGeometry().reason;
    EXPECT_TRUE(s.checkIntegrity().ok);
    // Every control vertex now lies on radius 3.
    for (uint32_t v = 0; v < s.vertexCount(); ++v)
        EXPECT_NEAR(dist(s.vertex(v).point, centreBefore), 3.f, 1e-4f) << "vertex " << v;
    // And the internal Circle-edge / Sphere-surface radii scaled too (checkGeometry
    // above already reproduces endpoints from the scaled curves).
}

TEST(BRepTransform, RejectsUnsupportedAndNonFinite)
{
    // Shear.
    {
        Body b = makeBox(2.f, 2.f, 2.f);
        Mat4 sh = Mat4::identity();
        sh.m[0][1] = 0.5f;
        EXPECT_FALSE(b.transform(sh));
        EXPECT_TRUE(b.checkGeometry().ok);  // unchanged
    }
    // Non-uniform scale.
    {
        Body b = makeBox(2.f, 2.f, 2.f);
        Mat4 ns = Mat4::identity();
        ns.m[0][0] = 2.f;
        EXPECT_FALSE(b.transform(ns));
    }
    // Reflection (det < 0).
    {
        Body b = makeBox(2.f, 2.f, 2.f);
        Mat4 rf = Mat4::identity();
        rf.m[0][0] = -1.f;
        EXPECT_FALSE(b.transform(rf));
    }
    // Non-finite entry (bit-pattern Inf, reliable under -ffast-math).
    {
        Body b = makeBox(2.f, 2.f, 2.f);
        Mat4 nf = Mat4::identity();
        nf.m[0][3] = std::bit_cast<float>(0x7F800000u);  // +Inf
        EXPECT_FALSE(b.transform(nf));
        EXPECT_TRUE(b.checkIntegrity().ok);  // unchanged
    }
}

TEST(BRepTransform, Deterministic)
{
    const Mat4 m = rotZ(0.4f) * trans({1.f, 1.f, 1.f});
    Body a = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(a.transform(m));
    Body b = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(b.transform(m));
    ASSERT_EQ(a.vertexCount(), b.vertexCount());
    for (uint32_t v = 0; v < a.vertexCount(); ++v)
        EXPECT_EQ(dist(a.vertex(v).point, b.vertex(v).point), 0.f) << "vertex " << v;
}

}  // namespace nexus::geometry::brep::testing
