// Foundation — analytic B-rep point classification (the boolean's CLASSIFY
// step). Body::classifyPoint ray-casts against the watertight triangulation of
// the shell: interior points classify Inside, exterior Outside, and points on a
// face/edge/vertex OnBoundary. Proven on box / cylinder / sphere, deterministic.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;
using PC = Body::PointContainment;

TEST(BRepClassifyPoint, BoxInteriorExteriorBoundary)
{
    const Body b = makeBox(2.f, 2.f, 2.f);  // [-1,1]^3
    // Interior.
    EXPECT_EQ(b.classifyPoint({0.f, 0.f, 0.f}), PC::Inside);
    EXPECT_EQ(b.classifyPoint({0.5f, 0.5f, 0.5f}), PC::Inside);
    EXPECT_EQ(b.classifyPoint({-0.9f, 0.2f, 0.7f}), PC::Inside);
    // Exterior.
    EXPECT_EQ(b.classifyPoint({5.f, 0.f, 0.f}), PC::Outside);
    EXPECT_EQ(b.classifyPoint({0.f, 0.f, 2.f}), PC::Outside);
    EXPECT_EQ(b.classifyPoint({1.5f, 1.5f, 1.5f}), PC::Outside);
    // On a face / edge / vertex.
    EXPECT_EQ(b.classifyPoint({1.f, 0.f, 0.f}), PC::OnBoundary);    // +X face centre
    EXPECT_EQ(b.classifyPoint({0.f, -1.f, 0.3f}), PC::OnBoundary);  // -Y face
    EXPECT_EQ(b.classifyPoint({1.f, 1.f, 0.f}), PC::OnBoundary);    // an edge
    EXPECT_EQ(b.classifyPoint({1.f, 1.f, 1.f}), PC::OnBoundary);    // a corner vertex
}

TEST(BRepClassifyPoint, CylinderInteriorExteriorBoundary)
{
    const Body b = makeCylinder(1.f, 2.f, 24);  // centred, z in [-1,1]
    EXPECT_EQ(b.classifyPoint({0.f, 0.f, 0.f}), PC::Inside);       // true centre
    EXPECT_EQ(b.classifyPoint({0.9f, 0.f, 0.f}), PC::Inside);      // just inside the wall
    EXPECT_EQ(b.classifyPoint({3.f, 0.f, 0.f}), PC::Outside);      // beyond the radius
    EXPECT_EQ(b.classifyPoint({0.f, 0.f, 2.f}), PC::Outside);      // above the top cap
    EXPECT_EQ(b.classifyPoint({0.f, 0.f, 1.f}), PC::OnBoundary);   // top-cap centre
    EXPECT_EQ(b.classifyPoint({0.f, 0.f, -1.f}), PC::OnBoundary);  // bottom-cap centre
    // Every control vertex lies exactly on the boundary.
    for (uint32_t v = 0; v < b.vertexCount(); ++v)
        EXPECT_EQ(b.classifyPoint(b.vertex(v).point), PC::OnBoundary) << "vertex " << v;
}

TEST(BRepClassifyPoint, SphereInteriorExteriorBoundary)
{
    const Body b = makeSphere(1.f, 8, 12);
    EXPECT_EQ(b.classifyPoint({0.f, 0.f, 0.f}), PC::Inside);   // centre
    EXPECT_EQ(b.classifyPoint({0.3f, -0.2f, 0.1f}), PC::Inside);
    EXPECT_EQ(b.classifyPoint({2.f, 0.f, 0.f}), PC::Outside);  // outside the radius
    EXPECT_EQ(b.classifyPoint({0.f, 0.f, 1.5f}), PC::Outside);
    // Control vertices sit on the sphere surface (and are tessellation vertices).
    for (uint32_t v = 0; v < b.vertexCount(); ++v)
        EXPECT_EQ(b.classifyPoint(b.vertex(v).point), PC::OnBoundary) << "vertex " << v;
}

TEST(BRepClassifyPoint, Deterministic)
{
    const Body b = makeSphere(1.f, 6, 10);
    const Vec3 pts[] = {{0, 0, 0}, {0.4f, 0.4f, 0.4f}, {3, 0, 0}, {0.7f, 0, 0}};
    for (const Vec3& p : pts) {
        const PC first = b.classifyPoint(p);
        for (int r = 0; r < 5; ++r) EXPECT_EQ(b.classifyPoint(p), first);
    }
}

TEST(BRepClassifyPoint, EmptyBodyIsOutside)
{
    Body b;  // no faces
    EXPECT_EQ(b.classifyPoint({0.f, 0.f, 0.f}), PC::Outside);
}

}  // namespace nexus::geometry::brep::testing
