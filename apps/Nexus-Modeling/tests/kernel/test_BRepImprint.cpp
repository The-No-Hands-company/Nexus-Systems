// Foundation — analytic B-rep IMPRINT (the boolean's imprint step). Splitting a
// planar face along an analytic intersection curve, with the resulting shared
// edge lying exactly on that curve. Proven χ-neutral, integrity- and
// geometry-clean, on-both-surfaces, and composable with mergeFaces (its inverse
// direction). Line imprint (plane∩plane) is the case landed here.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
// The +Z face of a box centred at the origin (all four verts at z = +halfDepth).
uint32_t topFace(const Body& b, float z)
{
    for (uint32_t f = 0; f < b.faceCount(); ++f) {
        const auto vs = b.faceVertices(f);
        if (vs.empty()) continue;
        bool all = true;
        for (uint32_t v : vs)
            if (std::abs(b.vertex(v).point.z - z) > 1e-4f) { all = false; break; }
        if (all) return f;
    }
    return kInvalid;
}

Surface plane(Vec3 o, Vec3 n) { Surface s; s.kind = SurfaceKind::Plane; s.origin = o; s.normal = n; return s; }
}  // namespace

TEST(BRepImprint, LineSplitsPlanarFaceEdgeToEdge)
{
    Body box = makeBox(2.f, 2.f, 2.f);  // [-1,1]^3
    ASSERT_TRUE(box.checkIntegrity().ok);
    ASSERT_TRUE(box.checkGeometry().ok);

    const uint32_t f = topFace(box, 1.f);
    ASSERT_NE(f, kInvalid);

    // plane z=+1 (the top face) ∩ plane x=0 → the Line x=0 in the z=1 plane.
    const auto ssi = intersectSurfaces(plane({0, 0, 1}, {0, 0, 1}), plane({0, 0, 0}, {1, 0, 0}));
    ASSERT_EQ(ssi.kind, SurfaceIntersectionKind::Line);

    const size_t V0 = box.vertexCount(), E0 = box.edgeCount(), F0 = box.faceCount();
    const uint32_t nf = box.imprintCurve(f, ssi.curve);
    ASSERT_NE(nf, kInvalid);

    // χ-neutral: ΔV=+2 (pierce verts), ΔE=+3 (two splits + the cut), ΔF=+1.
    EXPECT_EQ(box.vertexCount(), V0 + 2);
    EXPECT_EQ(box.edgeCount(), E0 + 3);
    EXPECT_EQ(box.faceCount(), F0 + 1);

    const auto ig = box.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);                 // still a closed genus-0 solid
    EXPECT_EQ(ig.boundaryEdges, 0u);        // still watertight
    EXPECT_TRUE(box.checkGeometry().ok) << box.checkGeometry().reason;
    EXPECT_TRUE(box.isClosed());
}

TEST(BRepImprint, CutEdgeLiesOnBothSurfaces)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = topFace(box, 1.f);
    const Surface top = plane({0, 0, 1}, {0, 0, 1});
    const Surface cut = plane({0, 0, 0}, {1, 0, 0});
    const auto ssi = intersectSurfaces(top, cut);
    ASSERT_NE(box.imprintCurve(f, ssi.curve), kInvalid);

    // The new cut edge (a Line at x=0, z=1 spanning y) must lie on BOTH planes.
    int found = -1;
    float maxd = 0.f;
    for (uint32_t e = 0; e < box.edgeCount(); ++e) {
        const auto& ed = box.edge(e);
        if (!ed.alive) continue;
        const auto& cu = box.curve(ed.curve);
        if (cu.kind != CurveKind::Line) continue;
        const Vec3 p0 = box.vertex(ed.v0).point, p1 = box.vertex(ed.v1).point;
        if (std::abs(p0.x) < 1e-4f && std::abs(p1.x) < 1e-4f &&
            std::abs(p0.z - 1.f) < 1e-4f && std::abs(p1.z - 1.f) < 1e-4f &&
            std::abs(p0.y - p1.y) > 0.5f) {
            found = static_cast<int>(e);
            for (int k = 0; k <= 12; ++k) {
                const float t = ed.t0 + (ed.t1 - ed.t0) * static_cast<float>(k) / 12.f;
                const Vec3 p = cu.eval(t);
                maxd = std::max(maxd, std::max(std::abs(surfaceDistance(top, p)),
                                               std::abs(surfaceDistance(cut, p))));
            }
        }
    }
    ASSERT_GE(found, 0);
    EXPECT_LT(maxd, 1e-4f);
}

TEST(BRepImprint, CurveMissingFaceReturnsInvalid)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = topFace(box, 1.f);
    const size_t E0 = box.edgeCount(), F0 = box.faceCount();
    // plane x=5 does not cross the [-1,1] top face → its intersection Line misses.
    const auto ssi = intersectSurfaces(plane({0, 0, 1}, {0, 0, 1}), plane({5, 0, 0}, {1, 0, 0}));
    ASSERT_EQ(ssi.kind, SurfaceIntersectionKind::Line);
    EXPECT_EQ(box.imprintCurve(f, ssi.curve), kInvalid);
    // Body is left unchanged and still valid.
    EXPECT_EQ(box.edgeCount(), E0);
    EXPECT_EQ(box.faceCount(), F0);
    EXPECT_TRUE(box.checkIntegrity().ok);
}

TEST(BRepImprint, ComposesWithMergeFacesInverse)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    const uint32_t f = topFace(box, 1.f);
    const auto ssi = intersectSurfaces(plane({0, 0, 1}, {0, 0, 1}), plane({0, 0, 0}, {1, 0, 0}));
    const size_t F0 = box.faceCount();
    ASSERT_NE(box.imprintCurve(f, ssi.curve), kInvalid);
    ASSERT_EQ(box.faceCount(), F0 + 1);

    // Locate the cut edge and merge the two faces back (kill-edge-face).
    uint32_t cutEdge = kInvalid;
    for (uint32_t e = 0; e < box.edgeCount(); ++e) {
        const auto& ed = box.edge(e);
        if (!ed.alive) continue;
        const Vec3 p0 = box.vertex(ed.v0).point, p1 = box.vertex(ed.v1).point;
        if (std::abs(p0.x) < 1e-4f && std::abs(p1.x) < 1e-4f &&
            std::abs(p0.z - 1.f) < 1e-4f && std::abs(p1.z - 1.f) < 1e-4f &&
            std::abs(p0.y - p1.y) > 0.5f) { cutEdge = e; break; }
    }
    ASSERT_NE(cutEdge, kInvalid);
    ASSERT_TRUE(box.mergeFaces(cutEdge));
    const auto ig = box.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.faces, static_cast<uint32_t>(F0));  // face count restored
    EXPECT_EQ(ig.euler, 2);
    EXPECT_TRUE(box.checkGeometry().ok);
}

}  // namespace nexus::geometry::brep::testing
