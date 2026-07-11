#include <nexus/geometry/TriTriIntersect.h>

#include <gtest/gtest.h>

#include <limits>

namespace nexus::geometry::testing {

using V = nexus::render::Vec3;

TEST(TriTriIntersect, CrossingTrianglesReturnSegmentOnBothPlanes)
{
    // T1 in the z=0 plane; T2 in the x=1 plane, passing through T1.
    const V a0{0, 0, 0}, a1{2, 0, 0}, a2{0, 2, 0};
    const V b0{1, -1, -1}, b1{1, 3, -1}, b2{1, -1, 3};

    const TriTriResult r = TriTriIntersect::intersect(a0, a1, a2, b0, b1, b2);
    ASSERT_EQ(r.kind, TriTriResult::Kind::Segment);
    // Endpoints must lie on both planes: z==0 (T1) and x==1 (T2).
    EXPECT_NEAR(r.p0.z, 0.f, 1e-4f);
    EXPECT_NEAR(r.p1.z, 0.f, 1e-4f);
    EXPECT_NEAR(r.p0.x, 1.f, 1e-4f);
    EXPECT_NEAR(r.p1.x, 1.f, 1e-4f);
    // The overlap is T1's x==1 chord: y in [0,1].
    const V d = r.p1 - r.p0;
    EXPECT_GT(d.lengthSq(), 1e-6f);
}

TEST(TriTriIntersect, ParallelSeparatedReturnsNone)
{
    const V a0{0, 0, 0}, a1{1, 0, 0}, a2{0, 1, 0};
    const V b0{0, 0, 5}, b1{1, 0, 5}, b2{0, 1, 5};  // parallel, far in z
    EXPECT_EQ(TriTriIntersect::intersect(a0, a1, a2, b0, b1, b2).kind, TriTriResult::Kind::None);
}

TEST(TriTriIntersect, CoplanarReturnsCoplanar)
{
    const V a0{0, 0, 0}, a1{2, 0, 0}, a2{0, 2, 0};
    const V b0{0.5f, 0.5f, 0}, b1{2, 0.5f, 0}, b2{0.5f, 2, 0};  // same z=0 plane
    EXPECT_EQ(TriTriIntersect::intersect(a0, a1, a2, b0, b1, b2).kind, TriTriResult::Kind::Coplanar);
}

TEST(TriTriIntersect, PlanesCrossButTrianglesDisjointReturnsNone)
{
    // T1 near the origin in z=0; T2 in the x=10 plane — the planes intersect
    // (line z=0, x=10) but the triangles are nowhere near each other.
    const V a0{0, 0, 0}, a1{2, 0, 0}, a2{0, 2, 0};
    const V b0{10, -1, -1}, b1{10, 3, -1}, b2{10, -1, 3};
    EXPECT_EQ(TriTriIntersect::intersect(a0, a1, a2, b0, b1, b2).kind, TriTriResult::Kind::None);
}

TEST(TriTriIntersect, VertexTouchIsNotASegment)
{
    // T2 touches T1's plane at a single vertex only.
    const V a0{0, 0, 0}, a1{2, 0, 0}, a2{0, 2, 0};
    const V b0{0.5f, 0.5f, 0}, b1{0.5f, 0.5f, 2}, b2{1.5f, 0.5f, 2};
    EXPECT_EQ(TriTriIntersect::intersect(a0, a1, a2, b0, b1, b2).kind, TriTriResult::Kind::None);
}

TEST(TriTriIntersect, RejectsNonFinite)
{
    const V a0{0, 0, 0}, a1{1, 0, 0}, a2{0, 1, 0};
    const V bad{std::numeric_limits<float>::quiet_NaN(), 0, 0};
    EXPECT_EQ(TriTriIntersect::intersect(a0, a1, a2, bad, a1, a2).kind, TriTriResult::Kind::None);
}

TEST(TriTriIntersect, IsSymmetricInArguments)
{
    const V a0{0, 0, 0}, a1{2, 0, 0}, a2{0, 2, 0};
    const V b0{1, -1, -1}, b1{1, 3, -1}, b2{1, -1, 3};
    const TriTriResult ab = TriTriIntersect::intersect(a0, a1, a2, b0, b1, b2);
    const TriTriResult ba = TriTriIntersect::intersect(b0, b1, b2, a0, a1, a2);
    EXPECT_EQ(ab.kind, ba.kind);  // same classification regardless of order
}

}  // namespace nexus::geometry::testing
