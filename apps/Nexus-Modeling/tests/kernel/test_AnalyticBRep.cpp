// Foundation — analytic B-rep (brep::Body). The data model binds typed topology
// (vertex/edge/coedge/loop/face/shell/solid) to analytic geometry (Line curves,
// Plane surfaces). checkIntegrity() is the authoritative validator; these tests
// prove the canonical analytic box is a watertight, coedge-partnered solid and
// that the builder rejects malformed input.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/MeshTopologyValidation.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

TEST(AnalyticBRep, BoxIsWatertightCoedgePartneredSolid)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    const auto r = box.checkIntegrity();
    ASSERT_TRUE(r.ok) << r.reason;
    EXPECT_EQ(r.vertices, 8u);
    EXPECT_EQ(r.edges, 12u);
    EXPECT_EQ(r.faces, 6u);
    EXPECT_EQ(r.coedges, 24u);   // each of 12 edges used by 2 faces
    EXPECT_EQ(r.loops, 6u);
    EXPECT_EQ(r.shells, 1u);
    EXPECT_EQ(r.boundaryEdges, 0u);  // closed
    EXPECT_EQ(r.euler, 2);           // V - E + F = 8 - 12 + 6
    EXPECT_TRUE(box.isClosed());
}

// The tessellation cross-checks against the mesh validator: a closed genus-0
// polygon shell (euler 2).
TEST(AnalyticBRep, BoxTessellatesToValidGenus0Mesh)
{
    const Body box = makeBox(1.f, 2.f, 3.f);
    const Mesh m = box.toMesh();
    EXPECT_TRUE(m.isValid());
    EXPECT_EQ(m.attributes().vertexCount(), 8u);
    EXPECT_EQ(MeshTopologyValidation::validate(m).euler, 2);
}

// Every coedge on a closed solid has a reciprocal, opposite-oriented partner on
// the adjacent face (validated inside checkIntegrity; asserted here explicitly).
TEST(AnalyticBRep, EveryCoedgeHasReciprocalPartner)
{
    const Body box = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(box.checkIntegrity().ok);
    for (uint32_t c = 0; c < box.coedgeCount(); ++c) {
        const Coedge& ce = box.coedge(c);
        ASSERT_NE(ce.partner, kInvalid) << "coedge " << c << " unpaired";
        const Coedge& pt = box.coedge(ce.partner);
        EXPECT_EQ(pt.partner, c);
        EXPECT_EQ(pt.edge, ce.edge);
        EXPECT_NE(pt.reversed, ce.reversed);  // opposite orientation
    }
}

// Line-curve edges evaluate to their endpoint vertices at the stored param
// range, and plane-face surfaces report a unit outward normal.
TEST(AnalyticBRep, AnalyticGeometryEvaluatesConsistently)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    for (uint32_t e = 0; e < box.edgeCount(); ++e) {
        const Edge& ed = box.edge(e);
        const Curve& cv = box.curve(ed.curve);
        const Vec3 p0 = cv.eval(ed.t0);
        const Vec3 p1 = cv.eval(ed.t1);
        const Vec3& a = box.vertex(ed.v0).point;
        const Vec3& b = box.vertex(ed.v1).point;
        EXPECT_NEAR(p0.x, a.x, 1e-4f); EXPECT_NEAR(p0.y, a.y, 1e-4f); EXPECT_NEAR(p0.z, a.z, 1e-4f);
        EXPECT_NEAR(p1.x, b.x, 1e-4f); EXPECT_NEAR(p1.y, b.y, 1e-4f); EXPECT_NEAR(p1.z, b.z, 1e-4f);
    }
    for (uint32_t f = 0; f < box.faceCount(); ++f) {
        const Surface& s = box.surface(box.face(f).surface);
        const Vec3 n = s.normalAt(0.f, 0.f);
        EXPECT_NEAR(std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z), 1.f, 1e-4f);
    }
}

// The analytic cylinder is a watertight solid for any segment count: V=2n,
// E=3n, F=n+2 ⇒ euler 2, with coedges partnered (side quads on a Cylinder
// surface + two planar caps). Winding is proven by checkIntegrity, not eyeball.
TEST(AnalyticBRep, CylinderIsWatertightForAnySegmentCount)
{
    for (uint32_t n : {3u, 4u, 8u, 16u, 32u}) {
        const Body cyl = makeCylinder(1.f, 2.f, n);
        const auto r = cyl.checkIntegrity();
        ASSERT_TRUE(r.ok) << "n=" << n << ": " << r.reason;
        EXPECT_EQ(r.vertices, 2u * n) << "n=" << n;
        EXPECT_EQ(r.edges, 3u * n) << "n=" << n;
        EXPECT_EQ(r.faces, n + 2u) << "n=" << n;
        EXPECT_EQ(r.boundaryEdges, 0u) << "n=" << n;
        EXPECT_EQ(r.euler, 2) << "n=" << n;
        EXPECT_TRUE(cyl.isClosed()) << "n=" << n;
        EXPECT_EQ(MeshTopologyValidation::validate(cyl.toMesh()).euler, 2) << "n=" << n;
    }
}

// The analytic cone: apex + bottom ring, V=n+1, E=2n, F=n+1 ⇒ euler 2.
TEST(AnalyticBRep, ConeIsWatertightForAnySegmentCount)
{
    for (uint32_t n : {3u, 5u, 8u, 24u}) {
        const Body cone = makeCone(1.f, 2.f, n);
        const auto r = cone.checkIntegrity();
        ASSERT_TRUE(r.ok) << "n=" << n << ": " << r.reason;
        EXPECT_EQ(r.vertices, n + 1u) << "n=" << n;
        EXPECT_EQ(r.edges, 2u * n) << "n=" << n;
        EXPECT_EQ(r.faces, n + 1u) << "n=" << n;
        EXPECT_EQ(r.boundaryEdges, 0u) << "n=" << n;
        EXPECT_EQ(r.euler, 2) << "n=" << n;
        EXPECT_TRUE(cone.isClosed()) << "n=" << n;
        EXPECT_EQ(MeshTopologyValidation::validate(cone.toMesh()).euler, 2) << "n=" << n;
    }
}

// Cylinder side vertices lie exactly on the cylinder of the given radius.
TEST(AnalyticBRep, CylinderVerticesLieOnRadius)
{
    const float radius = 2.5f;
    const Body cyl = makeCylinder(radius, 3.f, 12);
    for (uint32_t v = 0; v < cyl.vertexCount(); ++v) {
        const Vec3& p = cyl.vertex(v).point;
        EXPECT_NEAR(std::sqrt(p.x * p.x + p.y * p.y), radius, 1e-4f);
    }
}

TEST(AnalyticBRep, FromFacesRejectsMalformedInput)
{
    // No points / no faces.
    EXPECT_FALSE(Body::fromFaces({}, {}).has_value());
    // A loop with fewer than 3 vertices.
    std::vector<Vec3> pts = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
    Body::FaceDef sub;
    sub.loop = {0u, 1u};
    EXPECT_FALSE(Body::fromFaces(pts, {sub}).has_value());
    // An out-of-range vertex index.
    Body::FaceDef oob;
    oob.loop = {0u, 1u, 9u};
    EXPECT_FALSE(Body::fromFaces(pts, {oob}).has_value());
}

}  // namespace nexus::geometry::brep::testing
