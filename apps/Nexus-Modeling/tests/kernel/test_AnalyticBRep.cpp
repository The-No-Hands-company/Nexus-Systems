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

// ──────────── Trimmed NURBS faces: parameter-space trim curves (pcurves) ──────
//
// A trimmed surface's boundary is defined ON the surface's (u,v) parameter
// domain (a pcurve per coedge), not just in 3D. These build the same bilinear
// NURBS patch face and attach a pcurve to every outer-loop coedge, mapping the
// coedge's directed 3D endpoints back to their (u,v) — the hallmark of a real
// trimmed B-rep face.
namespace {
// Build the flat bilinear NURBS-patch face (z=0 unit-ish quad) as a Body.
Body makeNurbsQuadFace()
{
    const std::vector<Vec3> pts = {{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}};
    const std::vector<Vec3> ctl = {{0, 0, 0}, {0, 2, 0}, {2, 0, 0}, {2, 2, 0}};  // i*nV+j
    NurbsSurface patch(1, 1, {0, 0, 1, 1}, {0, 0, 1, 1}, ctl, 2, 2);
    Body::FaceDef fd;
    fd.loop = {0u, 1u, 2u, 3u};
    fd.nurbsSurface = patch;
    return *Body::fromFaces(pts, {fd});
}
// For this patch, evaluate(u,v) = (2u, 2v, 0), so a 3D point maps back to
// (u,v) = (x/2, y/2).
std::pair<float, float> uvOfQuad(const Vec3& p) { return {p.x * 0.5f, p.y * 0.5f}; }

// Attach a correct pcurve to every outer-loop coedge of face 0; returns the
// number attached.
uint32_t attachOuterLoopPcurves(Body& b)
{
    const uint32_t first = b.loop(b.face(0).outerLoop).first;
    uint32_t c = first, added = 0;
    do {
        const Coedge& ce = b.coedge(c);
        const Edge& ed = b.edge(ce.edge);
        const uint32_t sV = ce.reversed ? ed.v1 : ed.v0;
        const uint32_t eV = ce.reversed ? ed.v0 : ed.v1;
        const auto [su, sv] = uvOfQuad(b.vertex(sV).point);
        const auto [eu, ev] = uvOfQuad(b.vertex(eV).point);
        if (b.setCoedgePcurve(c, su, sv, eu, ev)) ++added;
        c = ce.next;
    } while (c != first);
    return added;
}
}  // namespace

TEST(AnalyticBRepPcurve, AttachAndValidateOnNurbsFace)
{
    Body b = makeNurbsQuadFace();
    ASSERT_TRUE(b.checkGeometry().ok);

    EXPECT_EQ(attachOuterLoopPcurves(b), 4u);  // all four outer coedges trimmed

    // Every outer coedge now carries a present pcurve, and checkGeometry proves
    // the parameter-space endpoints map back to the 3D vertices.
    const auto g = b.checkGeometry();
    EXPECT_TRUE(g.ok) << g.reason;

    uint32_t present = 0;
    for (uint32_t c = 0; c < b.coedgeCount(); ++c)
        if (b.coedge(c).pcurve.present) ++present;
    EXPECT_EQ(present, 4u);
}

TEST(AnalyticBRepPcurve, RejectsEndpointsThatDoNotMapBack)
{
    Body b = makeNurbsQuadFace();
    const uint32_t first = b.loop(b.face(0).outerLoop).first;

    // (0.9,0.9) maps to (1.8,1.8,0) — nowhere near the coedge's start vertex.
    EXPECT_FALSE(b.setCoedgePcurve(first, 0.9f, 0.9f, 0.1f, 0.1f));
    EXPECT_FALSE(b.coedge(first).pcurve.present);  // unchanged

    // Non-finite parameters are rejected.
    const float inf = std::bit_cast<float>(0x7F800000u);
    EXPECT_FALSE(b.setCoedgePcurve(first, inf, 0.f, 1.f, 0.f));
    EXPECT_FALSE(b.coedge(first).pcurve.present);

    // Out-of-range coedge id is rejected.
    EXPECT_FALSE(b.setCoedgePcurve(9999u, 0.f, 0.f, 1.f, 0.f));
}

TEST(AnalyticBRepPcurve, SwappedEndpointsRejected)
{
    Body b = makeNurbsQuadFace();
    const uint32_t first = b.loop(b.face(0).outerLoop).first;
    const Coedge& ce = b.coedge(first);
    const Edge& ed = b.edge(ce.edge);
    const uint32_t sV = ce.reversed ? ed.v1 : ed.v0;
    const uint32_t eV = ce.reversed ? ed.v0 : ed.v1;
    const auto [su, sv] = uvOfQuad(b.vertex(sV).point);
    const auto [eu, ev] = uvOfQuad(b.vertex(eV).point);
    // Correct order attaches; reversed order (end params in start slot) must not.
    EXPECT_FALSE(b.setCoedgePcurve(first, eu, ev, su, sv));
    EXPECT_TRUE(b.setCoedgePcurve(first, su, sv, eu, ev));
}

TEST(AnalyticBRepPcurve, RoundTripsThroughSerialization)
{
    Body b = makeNurbsQuadFace();
    ASSERT_EQ(attachOuterLoopPcurves(b), 4u);

    const std::vector<std::uint8_t> bytes = b.serialize();
    const auto rt = Body::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());
    EXPECT_TRUE(rt->checkGeometry().ok);
    EXPECT_EQ(bytes, rt->serialize());  // byte-identical re-serialization

    // The pcurve payload survives exactly.
    uint32_t present = 0;
    for (uint32_t c = 0; c < rt->coedgeCount(); ++c) {
        const Pcurve& a = b.coedge(c).pcurve;
        const Pcurve& d = rt->coedge(c).pcurve;
        EXPECT_EQ(a.present, d.present);
        if (d.present) {
            ++present;
            EXPECT_FLOAT_EQ(a.u0, d.u0);
            EXPECT_FLOAT_EQ(a.v0, d.v0);
            EXPECT_FLOAT_EQ(a.u1, d.u1);
            EXPECT_FLOAT_EQ(a.v1, d.v1);
        }
    }
    EXPECT_EQ(present, 4u);
}

TEST(AnalyticBRepPcurve, LegacyV1BlobDecodes)
{
    // A pcurve-FREE body serializes to v2 with only an empty trailing pcurve
    // section; patch the version byte back to 1 and it must still decode
    // identically (the v1 reader stops before the trailing section).
    const Body box = makeBox(2.f, 2.f, 2.f);
    std::vector<std::uint8_t> bytes = box.serialize();
    ASSERT_GT(bytes.size(), 8u);
    EXPECT_EQ(bytes[4], 2u);  // magic(4) then version at byte 4
    bytes[4] = 1u;            // masquerade as a legacy v1 blob

    const auto rt = Body::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());
    EXPECT_TRUE(rt->checkIntegrity().ok);
    EXPECT_TRUE(rt->checkGeometry().ok);
    EXPECT_NEAR(rt->massProperties().volume, 8.0f, 1e-4f);
}

TEST(AnalyticBRepPcurve, WrongPcurveFailsCheckGeometry)
{
    // Inject an inconsistent pcurve past the setCoedgePcurve front door by
    // corrupting the serialized parameter (deserialize loads geometry without
    // re-validating it), so checkGeometry's pcurve clause is what catches it.
    Body b = makeNurbsQuadFace();
    ASSERT_EQ(attachOuterLoopPcurves(b), 4u);

    std::vector<std::uint8_t> bytes = b.serialize();
    ASSERT_GT(bytes.size(), 4u);
    // The trailing pcurve section ends the blob; its last float is the last
    // present coedge's v1. Overwrite it with a finite-but-wrong value.
    const std::uint32_t bad = std::bit_cast<std::uint32_t>(9.0f);
    bytes[bytes.size() - 4] = static_cast<std::uint8_t>(bad & 0xFFu);
    bytes[bytes.size() - 3] = static_cast<std::uint8_t>((bad >> 8) & 0xFFu);
    bytes[bytes.size() - 2] = static_cast<std::uint8_t>((bad >> 16) & 0xFFu);
    bytes[bytes.size() - 1] = static_cast<std::uint8_t>((bad >> 24) & 0xFFu);

    const auto rt = Body::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());               // loads fine (value is finite)
    EXPECT_TRUE(rt->checkIntegrity().ok);       // topology untouched
    const auto g = rt->checkGeometry();
    EXPECT_FALSE(g.ok);                          // the stale pcurve is caught
    EXPECT_NE(g.reason.find("pcurve"), std::string::npos);
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

// Liveness: the validators count only live entities and — the crucial guard,
// the same class that caught the HalfEdgeMesh bugs — a LIVE entity must never
// reference a DEAD one. Marking a face dead while a loop still references it is
// caught by checkIntegrity.
TEST(AnalyticBRep, CheckIntegrityCatchesLiveReferenceToDeadEntity)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.checkIntegrity().ok);
    box.faceMut(0).alive = false;  // tombstoned without unlinking its loop
    const auto r = box.checkIntegrity();
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.reason.find("dead"), std::string::npos) << r.reason;
}

// The liveness-aware validators are a no-op regression-wise: with nothing dead,
// every primitive still validates and the live counts equal the totals.
TEST(AnalyticBRep, LivenessAwareValidatorsNoRegression)
{
    for (const Body& b : {makeBox(1.f, 2.f, 3.f), makeCylinder(1.f, 2.f, 8),
                          makeCone(1.f, 2.f, 6), makeSphere(1.f, 5, 8)}) {
        const auto r = b.checkIntegrity();
        EXPECT_TRUE(r.ok) << r.reason;
        EXPECT_TRUE(b.checkGeometry().ok);
        EXPECT_EQ(r.euler, 2);
        EXPECT_EQ(r.vertices, b.vertexCount());  // nothing dead ⇒ live == total
        EXPECT_EQ(r.edges, b.edgeCount());
        EXPECT_EQ(r.faces, b.faceCount());
    }
}

// joinEdges is the inverse of splitEdge (kill-edge-vertex): splitting an edge
// then joining at the new vertex restores the exact LIVE counts and euler, with
// both validators clean — the tombstoned entities leave no dangling references
// (the liveness guard would fire otherwise).
TEST(AnalyticBRep, JoinEdgesRoundTripsSplitEdge)
{
    auto exercise = [](const Body& base) {
        const auto orig = base.checkIntegrity();
        for (uint32_t e = 0; e < base.edgeCount(); ++e) {
            Body b = base;
            const uint32_t nv = b.splitEdge(e, 0.4f);
            ASSERT_NE(nv, kInvalid) << "edge " << e;
            ASSERT_TRUE(b.joinEdges(nv)) << "edge " << e;
            const auto ti = b.checkIntegrity();
            ASSERT_TRUE(ti.ok) << "edge " << e << ": " << ti.reason;
            const auto tg = b.checkGeometry();
            ASSERT_TRUE(tg.ok) << "edge " << e << ": " << tg.reason;
            EXPECT_EQ(ti.vertices, orig.vertices) << "edge " << e;  // live counts restored
            EXPECT_EQ(ti.edges, orig.edges) << "edge " << e;
            EXPECT_EQ(ti.faces, orig.faces) << "edge " << e;
            EXPECT_EQ(ti.euler, 2) << "edge " << e;
        }
    };
    exercise(makeBox(2.f, 2.f, 2.f));
    exercise(makeCylinder(1.f, 2.f, 8));
    exercise(makeSphere(1.f, 4, 6));
}

// A corner of a box has degree 3, not 2 — joinEdges must refuse it.
TEST(AnalyticBRep, JoinEdgesRejectsNonDegree2Vertex)
{
    Body box = makeBox(1.f, 1.f, 1.f);
    EXPECT_FALSE(box.joinEdges(0));       // a cube corner (degree 3)
    EXPECT_FALSE(box.joinEdges(9999u));   // invalid vertex
    EXPECT_TRUE(box.checkIntegrity().ok); // unchanged
}

// mergeFaces is the inverse of splitFace (kill-edge-face): splitting a face then
// merging along the new diagonal edge restores the exact LIVE counts and euler,
// both validators clean — the loop-splice + tombstoning leave no dangling refs.
TEST(AnalyticBRep, MergeFacesRoundTripsSplitFace)
{
    for (uint32_t f = 0; f < 6u; ++f) {
        Body b = makeBox(2.f, 2.f, 2.f);
        const auto orig = b.checkIntegrity();
        const auto v = b.faceVertices(f);
        ASSERT_GE(v.size(), 4u);
        ASSERT_NE(b.splitFace(f, v[0], v[2]), kInvalid) << "face " << f;
        const uint32_t diagonal = static_cast<uint32_t>(b.edgeCount()) - 1u;  // splitFace's new edge
        ASSERT_TRUE(b.mergeFaces(diagonal)) << "face " << f;
        const auto ti = b.checkIntegrity();
        ASSERT_TRUE(ti.ok) << "face " << f << ": " << ti.reason;
        const auto tg = b.checkGeometry();
        ASSERT_TRUE(tg.ok) << "face " << f << ": " << tg.reason;
        EXPECT_EQ(ti.vertices, orig.vertices) << "face " << f;  // live counts restored
        EXPECT_EQ(ti.edges, orig.edges) << "face " << f;
        EXPECT_EQ(ti.faces, orig.faces) << "face " << f;
        EXPECT_EQ(ti.euler, 2) << "face " << f;
    }
}

TEST(AnalyticBRep, MergeFacesRejectsBoundaryOrInvalid)
{
    Body box = makeBox(1.f, 1.f, 1.f);
    EXPECT_FALSE(box.mergeFaces(9999u));  // invalid edge
    // Every box edge is a manifold shared edge between two faces, so mergeFaces
    // succeeds on it, but re-merging or invalid ids must fail.
    Body b2 = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(b2.mergeFaces(0));            // merge two adjacent box faces
    EXPECT_TRUE(b2.checkIntegrity().ok);      // still a valid (open-ish) shell
    EXPECT_FALSE(b2.mergeFaces(0));           // edge 0 is now dead → refused
}

// toMesh(subdivisions) places intermediate points per EDGE (shared by both
// incident faces), so the tessellation is watertight (crack-free) at any level.
TEST(AnalyticBRep, ToMeshSubdivisionsStayWatertight)
{
    for (const Body& b : {makeBox(2.f, 2.f, 2.f), makeCylinder(1.f, 2.f, 8),
                          makeCone(1.f, 2.f, 6), makeSphere(1.f, 5, 8)}) {
        size_t prevVerts = 0;
        for (uint32_t s : {0u, 1u, 2u, 4u}) {
            const Mesh m = b.toMesh(s);
            EXPECT_TRUE(m.isValid());
            EXPECT_EQ(MeshTopologyValidation::validate(m).euler, 2) << "subdiv " << s;
            if (s > 0) EXPECT_GT(m.attributes().vertexCount(), prevVerts) << "subdiv " << s;
            prevVerts = m.attributes().vertexCount();
        }
    }
}

// The cylinder's ring edges are true Circle arcs, so its subdivided tessellation
// lies exactly on the cylinder (not on chords) — proven by every tessellated
// vertex sitting on the given radius, while both validators still pass.
TEST(AnalyticBRep, CylinderHasArcEdgesAndTessellatesOnSurface)
{
    const float radius = 2.f;
    const Body cyl = makeCylinder(radius, 3.f, 8);
    EXPECT_TRUE(cyl.checkIntegrity().ok);
    EXPECT_TRUE(cyl.checkGeometry().ok) << "arc-edge curves must still meet their vertices";

    // At least one edge is now a Circle arc.
    bool anyArc = false;
    for (uint32_t e = 0; e < cyl.edgeCount(); ++e)
        if (cyl.curve(cyl.edge(e).curve).kind == CurveKind::Circle) anyArc = true;
    EXPECT_TRUE(anyArc);

    // Every tessellated vertex lies on the cylinder of the given radius.
    const Mesh m = cyl.toMesh(4);
    EXPECT_EQ(MeshTopologyValidation::validate(m).euler, 2);
    for (const Vec3& p : m.attributes().positions())
        EXPECT_NEAR(std::sqrt(p.x * p.x + p.y * p.y), radius, 1e-3f);
}

// Every sphere edge is a Circle arc (latitude rings + meridian great circles),
// so the analytic sphere is exact and toMesh subdivides directly onto it: every
// tessellated vertex sits on the sphere radius, at any resolution.
TEST(AnalyticBRep, SphereHasAllArcEdgesAndTessellatesOnSurface)
{
    const float R = 2.f;
    const Body sph = makeSphere(R, 6, 10);
    EXPECT_TRUE(sph.checkIntegrity().ok);
    EXPECT_TRUE(sph.checkGeometry().ok) << "arc-edge curves must still meet their vertices";

    for (uint32_t e = 0; e < sph.edgeCount(); ++e)
        EXPECT_EQ(sph.curve(sph.edge(e).curve).kind, CurveKind::Circle) << "edge " << e;

    for (uint32_t s : {1u, 4u}) {
        const Mesh m = sph.toMesh(s);
        EXPECT_EQ(MeshTopologyValidation::validate(m).euler, 2) << "subdiv " << s;
        for (const Vec3& p : m.attributes().positions())
            EXPECT_NEAR(std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z), R, 1e-3f) << "subdiv " << s;
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
