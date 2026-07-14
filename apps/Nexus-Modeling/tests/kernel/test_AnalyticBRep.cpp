// Foundation — analytic B-rep (brep::Body). The data model binds typed topology
// (vertex/edge/coedge/loop/face/shell/solid) to analytic geometry (Line curves,
// Plane surfaces). checkIntegrity() is the authoritative validator; these tests
// prove the canonical analytic box is a watertight, coedge-partnered solid and
// that the builder rejects malformed input.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/MeshTopologyValidation.h>
#include <nexus/geometry/NurbsSurface.h>

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>

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

// The analytic UV sphere is a watertight solid for any (lat,lon): pole triangle
// fans + latitude quad bands on a Sphere surface. V = 2 + (lat-1)*lon,
// F = lat*lon, E = lon*(2*lat-1) ⇒ euler 2. Winding proven by checkIntegrity.
TEST(AnalyticBRep, SphereIsWatertightForAnyResolution)
{
    struct Res { uint32_t lat, lon; };
    for (Res res : {Res{2, 3}, Res{2, 8}, Res{3, 4}, Res{4, 8}, Res{8, 16}, Res{16, 24}}) {
        const uint32_t lat = res.lat, lon = res.lon;
        const Body sph = makeSphere(1.f, lat, lon);
        const auto r = sph.checkIntegrity();
        ASSERT_TRUE(r.ok) << "lat=" << lat << " lon=" << lon << ": " << r.reason;
        EXPECT_EQ(r.vertices, 2u + (lat - 1u) * lon) << "lat=" << lat << " lon=" << lon;
        EXPECT_EQ(r.faces, lat * lon) << "lat=" << lat << " lon=" << lon;
        EXPECT_EQ(r.edges, lon * (2u * lat - 1u)) << "lat=" << lat << " lon=" << lon;
        EXPECT_EQ(r.boundaryEdges, 0u) << "lat=" << lat << " lon=" << lon;
        EXPECT_EQ(r.euler, 2) << "lat=" << lat << " lon=" << lon;
        EXPECT_TRUE(sph.isClosed());
        EXPECT_EQ(MeshTopologyValidation::validate(sph.toMesh()).euler, 2)
            << "lat=" << lat << " lon=" << lon;
    }
}

TEST(AnalyticBRep, SphereVerticesLieOnRadius)
{
    const float radius = 3.25f;
    const Body sph = makeSphere(radius, 6, 10);
    for (uint32_t v = 0; v < sph.vertexCount(); ++v) {
        const Vec3& p = sph.vertex(v).point;
        EXPECT_NEAR(std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z), radius, 1e-4f);
    }
}

// Geometric consistency: for every primitive, the analytic geometry agrees with
// the topology (edge curves reproduce their vertices, geometry finite, normals
// unit). Topological validity (checkIntegrity) is necessary but NOT sufficient.
TEST(AnalyticBRep, CheckGeometryPassesForAllPrimitives)
{
    EXPECT_TRUE(makeBox(2.f, 3.f, 4.f).checkGeometry().ok);
    EXPECT_TRUE(makeCylinder(1.5f, 2.f, 12).checkGeometry().ok);
    EXPECT_TRUE(makeCone(1.f, 2.f, 10).checkGeometry().ok);
    const auto g = makeSphere(2.f, 8, 12).checkGeometry();
    EXPECT_TRUE(g.ok) << g.reason;
    EXPECT_GT(g.checkedEdges, 0u);
}

// Moving a vertex without updating its incident edge curves leaves the curve
// endpoint stale — checkGeometry must catch it (checkIntegrity, being purely
// topological, would still pass).
TEST(AnalyticBRep, CheckGeometryCatchesStaleCurveAfterVertexMove)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.checkGeometry().ok);
    box.vertexMut(0).point = {99.f, 99.f, 99.f};  // curves not updated
    EXPECT_TRUE(box.checkIntegrity().ok);          // topology still fine
    const auto g = box.checkGeometry();
    EXPECT_FALSE(g.ok);
    EXPECT_NE(g.reason.find("curve"), std::string::npos) << g.reason;
}

// A non-finite vertex point is rejected (bit-pattern NaN defeats -ffast-math
// folding, exercising the IEEE-754 bit-inspection path).
TEST(AnalyticBRep, CheckGeometryCatchesNonFinitePoint)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    box.vertexMut(1).point.x = std::bit_cast<float>(0x7FC00000u);  // quiet NaN
    const auto g = box.checkGeometry();
    EXPECT_FALSE(g.ok);
    EXPECT_NE(g.reason.find("non-finite"), std::string::npos) << g.reason;
}

// A B-rep face can lie on an exact NURBS surface: the surface is stored on the
// Body, the face is tagged Nurbs with a handle, and Body::surfacePoint delegates
// eval to the NURBS toolkit. Both validators still pass.
TEST(AnalyticBRep, NurbsSurfaceFaceValidatesAndEvaluates)
{
    // Bilinear patch (degree 1×1, 2×2 control points) = a flat quad in z=0.
    const std::vector<Vec3> pts = {{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}};
    const std::vector<Vec3> ctl = {{0, 0, 0}, {0, 2, 0}, {2, 0, 0}, {2, 2, 0}};  // i*nV+j
    NurbsSurface patch(1, 1, {0, 0, 1, 1}, {0, 0, 1, 1}, ctl, 2, 2);
    ASSERT_TRUE(patch.isValid());

    Body::FaceDef fd;
    fd.loop = {0u, 1u, 2u, 3u};
    fd.nurbsSurface = patch;
    const auto body = Body::fromFaces(pts, {fd});
    ASSERT_TRUE(body.has_value());

    EXPECT_TRUE(body->checkIntegrity().ok);
    EXPECT_TRUE(body->checkGeometry().ok);
    EXPECT_EQ(body->nurbsSurfaceCount(), 1u);

    // surfacePoint dispatches to the NURBS surface: bilinear centre = corner avg.
    const Vec3 c = body->surfacePoint(body->face(0).surface, 0.5f, 0.5f);
    EXPECT_NEAR(c.x, 1.f, 1e-4f);
    EXPECT_NEAR(c.y, 1.f, 1e-4f);
    EXPECT_NEAR(c.z, 0.f, 1e-4f);
}

// surfacePoint dispatches to the analytic Surface::eval for non-NURBS faces.
TEST(AnalyticBRep, SurfacePointUsesAnalyticEvalForPlaneFaces)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    EXPECT_EQ(box.nurbsSurfaceCount(), 0u);
    for (uint32_t f = 0; f < box.faceCount(); ++f) {
        const uint32_t sid = box.face(f).surface;
        const Vec3 viaBody = box.surfacePoint(sid, 0.3f, 0.7f);
        const Vec3 viaSurface = box.surface(sid).eval(0.3f, 0.7f);
        EXPECT_FLOAT_EQ(viaBody.x, viaSurface.x);
        EXPECT_FLOAT_EQ(viaBody.y, viaSurface.y);
        EXPECT_FLOAT_EQ(viaBody.z, viaSurface.z);
    }
}

// Euler operator splitEdge: inserting a vertex on any edge is χ-neutral
// (ΔV=+1, ΔE=+1, ΔF=0 ⇒ euler unchanged) and preserves BOTH validators — the
// topology stays a watertight coedge-partnered solid and the geometry stays
// consistent (new edges share the curve; new vertex lies on it).
TEST(AnalyticBRep, SplitEdgePreservesBothValidatorsAndEuler)
{
    auto exercise = [](const Body& base) {
        for (uint32_t e = 0; e < base.edgeCount(); ++e) {
            Body b = base;
            const auto before = b.checkIntegrity();
            const uint32_t nv = b.splitEdge(e, 0.5f);
            ASSERT_NE(nv, kInvalid) << "edge " << e;
            const auto ti = b.checkIntegrity();
            ASSERT_TRUE(ti.ok) << "edge " << e << ": " << ti.reason;
            const auto tg = b.checkGeometry();
            ASSERT_TRUE(tg.ok) << "edge " << e << ": " << tg.reason;
            EXPECT_EQ(ti.euler, before.euler) << "edge " << e;        // χ-neutral
            EXPECT_EQ(ti.vertices, before.vertices + 1u) << "edge " << e;
            EXPECT_EQ(ti.edges, before.edges + 1u) << "edge " << e;
            EXPECT_EQ(ti.faces, before.faces) << "edge " << e;
            EXPECT_EQ(ti.boundaryEdges, before.boundaryEdges) << "edge " << e;
        }
    };
    exercise(makeBox(2.f, 2.f, 2.f));
    exercise(makeCylinder(1.f, 2.f, 8));
    exercise(makeSphere(1.f, 4, 6));
}

TEST(AnalyticBRep, SplitEdgeRejectsInvalidEdge)
{
    Body box = makeBox(1.f, 1.f, 1.f);
    EXPECT_EQ(box.splitEdge(9999u, 0.5f), kInvalid);
}

// Euler operator splitFace: adding a diagonal edge across a face is χ-neutral
// (ΔV=0, ΔE=+1, ΔF=+1 ⇒ euler unchanged) and preserves both validators — the
// two sub-faces inherit the surface and the diagonal coedges are partners.
TEST(AnalyticBRep, SplitFacePreservesBothValidatorsAndEuler)
{
    for (uint32_t f = 0; f < 6u; ++f) {
        Body b = makeBox(2.f, 2.f, 2.f);
        const auto verts = b.faceVertices(f);
        ASSERT_GE(verts.size(), 4u);
        const auto before = b.checkIntegrity();
        const uint32_t nf = b.splitFace(f, verts[0], verts[2]);  // diagonal
        ASSERT_NE(nf, kInvalid) << "face " << f;
        const auto ti = b.checkIntegrity();
        ASSERT_TRUE(ti.ok) << "face " << f << ": " << ti.reason;
        const auto tg = b.checkGeometry();
        ASSERT_TRUE(tg.ok) << "face " << f << ": " << tg.reason;
        EXPECT_EQ(ti.euler, before.euler) << "face " << f;         // χ-neutral
        EXPECT_EQ(ti.vertices, before.vertices) << "face " << f;
        EXPECT_EQ(ti.edges, before.edges + 1u) << "face " << f;
        EXPECT_EQ(ti.faces, before.faces + 1u) << "face " << f;
        EXPECT_EQ(ti.boundaryEdges, before.boundaryEdges) << "face " << f;
    }
}

TEST(AnalyticBRep, SplitFaceRejectsAdjacentOrInvalid)
{
    Body box = makeBox(1.f, 1.f, 1.f);
    const auto v = box.faceVertices(0);
    ASSERT_GE(v.size(), 4u);
    EXPECT_EQ(box.splitFace(0, v[0], v[1]), kInvalid);  // adjacent → no diagonal
    EXPECT_EQ(box.splitFace(0, v[0], v[0]), kInvalid);  // same vertex
    EXPECT_EQ(box.splitFace(9999u, v[0], v[2]), kInvalid);  // invalid face
}

// splitEdge then splitFace compose: an Euler-op sequence keeps the solid valid.
TEST(AnalyticBRep, EulerOpsComposeAndStayValid)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    ASSERT_NE(b.splitEdge(0, 0.5f), kInvalid);
    const auto v = b.faceVertices(1);
    ASSERT_GE(v.size(), 4u);
    ASSERT_NE(b.splitFace(1, v[0], v[2]), kInvalid);
    EXPECT_TRUE(b.checkIntegrity().ok);
    EXPECT_TRUE(b.checkGeometry().ok);
    EXPECT_EQ(b.checkIntegrity().euler, 2);
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
