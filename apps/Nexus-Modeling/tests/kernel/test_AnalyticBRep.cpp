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
#include <set>

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
    // A pcurve-FREE body serializes with only an empty trailing pcurve section;
    // patch the version byte back to 1 and it must still decode identically (the
    // v1 reader stops before the trailing section).
    const Body box = makeBox(2.f, 2.f, 2.f);
    std::vector<std::uint8_t> bytes = box.serialize();
    ASSERT_GT(bytes.size(), 8u);
    EXPECT_EQ(bytes[4], 3u);  // magic(4) then version at byte 4 (current: v3)
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
    ASSERT_GT(bytes.size(), 8u);
    // The last pcurve entry ends with [u0,v0,u1,v1, interiorCount(=0)] (v3), so
    // the last present coedge's v1 float sits 8 bytes from the end (the 4-byte
    // interior count trails it). Overwrite it with a finite-but-wrong value.
    const std::uint32_t bad = std::bit_cast<std::uint32_t>(9.0f);
    bytes[bytes.size() - 8] = static_cast<std::uint8_t>(bad & 0xFFu);
    bytes[bytes.size() - 7] = static_cast<std::uint8_t>((bad >> 8) & 0xFFu);
    bytes[bytes.size() - 6] = static_cast<std::uint8_t>((bad >> 16) & 0xFFu);
    bytes[bytes.size() - 5] = static_cast<std::uint8_t>((bad >> 24) & 0xFFu);

    const auto rt = Body::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());               // loads fine (value is finite)
    EXPECT_TRUE(rt->checkIntegrity().ok);       // topology untouched
    const auto g = rt->checkGeometry();
    EXPECT_FALSE(g.ok);                          // the stale pcurve is caught
    EXPECT_NE(g.reason.find("pcurve"), std::string::npos);
}

// ──────────── Trimmed-NURBS tessellation: toMesh walks the trim region ────────
//
// tessellateTrimmedFace samples the NURBS surface across its (u,v) domain and
// keeps only the region inside the coedge pcurves — so the tessellation lies on
// the surface AND honors the parameter-space trim, not the 3D loop.
namespace {
// A patch spanning (u,v)∈[0,1]² → (4u,4v,0), whose FACE is the inner 2×2 quad
// [1,3]² — so the pcurve trim loop is the sub-square (u,v)∈[0.25,0.75]², a
// proper trim (the face is smaller than the full surface domain).
Body makeSubTrimmedNurbsFace()
{
    const std::vector<Vec3> ctl = {{0, 0, 0}, {0, 4, 0}, {4, 0, 0}, {4, 4, 0}};  // i*nV+j
    NurbsSurface patch(1, 1, {0, 0, 1, 1}, {0, 0, 1, 1}, ctl, 2, 2);
    const std::vector<Vec3> pts = {{1, 1, 0}, {3, 1, 0}, {3, 3, 0}, {1, 3, 0}};
    Body::FaceDef fd;
    fd.loop = {0u, 1u, 2u, 3u};
    fd.nurbsSurface = patch;
    Body b = *Body::fromFaces(pts, {fd});
    const uint32_t first = b.loop(b.face(0).outerLoop).first;
    uint32_t c = first;
    do {  // pcurve param = (x/4, y/4)
        const Coedge& ce = b.coedge(c);
        const Edge& ed = b.edge(ce.edge);
        const uint32_t sV = ce.reversed ? ed.v1 : ed.v0;
        const uint32_t eV = ce.reversed ? ed.v0 : ed.v1;
        const Vec3 sp = b.vertex(sV).point, ep = b.vertex(eV).point;
        b.setCoedgePcurve(c, sp.x * 0.25f, sp.y * 0.25f, ep.x * 0.25f, ep.y * 0.25f);
        c = ce.next;
    } while (c != first);
    return b;
}
double meshArea(const Mesh& m)
{
    double a = 0.0;
    const auto& p = m.attributes().positions();
    for (size_t f = 0; f < m.topology().faceCount(); ++f) {
        const auto& id = m.topology().face(f).indices;
        if (id.size() != 3) continue;
        const Vec3 A = p[id[0]], B = p[id[1]], C = p[id[2]];
        const float x1 = B.x - A.x, y1 = B.y - A.y, z1 = B.z - A.z;
        const float x2 = C.x - A.x, y2 = C.y - A.y, z2 = C.z - A.z;
        const float cx = y1 * z2 - z1 * y2, cy = z1 * x2 - x1 * z2, cz = x1 * y2 - y1 * x2;
        a += 0.5 * std::sqrt(static_cast<double>(cx * cx + cy * cy + cz * cz));
    }
    return a;
}
}  // namespace

TEST(AnalyticBRepTrimTessellate, HonorsPcurveTrimRegion)
{
    Body b = makeSubTrimmedNurbsFace();
    ASSERT_TRUE(b.checkGeometry().ok);

    // res=40: the trim boundary 0.25/0.75 lands exactly on grid lines, so the
    // conservative interior sampling reproduces the trim area EXACTLY.
    const Mesh m = b.tessellateTrimmedFace(0, 40);
    ASSERT_GT(m.topology().faceCount(), 0u);
    EXPECT_NEAR(meshArea(m), 4.0, 1e-3);  // inner 2×2 quad, not the 4×4 full patch (16)

    // Every referenced vertex lies on the surface (z=0) and inside the trim
    // ([1,3]²) — nothing from the untrimmed [0,4] domain leaks through.
    std::set<uint32_t> ref;
    for (size_t f = 0; f < m.topology().faceCount(); ++f)
        for (uint32_t i : m.topology().face(f).indices) ref.insert(i);
    const auto& p = m.attributes().positions();
    for (uint32_t i : ref) {
        EXPECT_NEAR(p[i].z, 0.f, 1e-4f);
        EXPECT_GE(p[i].x, 1.f - 1e-4f);
        EXPECT_LE(p[i].x, 3.f + 1e-4f);
        EXPECT_GE(p[i].y, 1.f - 1e-4f);
        EXPECT_LE(p[i].y, 3.f + 1e-4f);
    }
}

TEST(AnalyticBRepTrimTessellate, CoarseAlignedExactAndConservativeOtherwise)
{
    Body b = makeSubTrimmedNurbsFace();
    // res=4 also aligns (0.25·4=1, 0.75·4=3) → exact trimmed area.
    EXPECT_NEAR(meshArea(b.tessellateTrimmedFace(0, 4)), 4.0, 1e-3);
    // A misaligned grid (res=7: no cell centre lands on the 0.25/0.75 boundary)
    // undertessellates — strictly less than the trimmed area, but a fair fraction.
    const double a7 = meshArea(b.tessellateTrimmedFace(0, 7));
    EXPECT_LT(a7, 4.0);
    EXPECT_GT(a7, 2.0);
}

TEST(AnalyticBRepTrimTessellate, DeterministicAndRejectsNonTrimmedFaces)
{
    Body b = makeSubTrimmedNurbsFace();
    const Mesh m1 = b.tessellateTrimmedFace(0, 16);
    const Mesh m2 = b.tessellateTrimmedFace(0, 16);
    ASSERT_EQ(m1.topology().faceCount(), m2.topology().faceCount());
    ASSERT_EQ(m1.attributes().positions().size(), m2.attributes().positions().size());
    EXPECT_EQ(m1.topology().face(0).indices, m2.topology().face(0).indices);

    // A plane (non-NURBS) face → empty; a NURBS face WITHOUT pcurves → empty.
    const Body box = makeBox(2.f, 2.f, 2.f);
    EXPECT_EQ(box.tessellateTrimmedFace(0, 8).topology().faceCount(), 0u);
    const Body untrimmed = makeNurbsQuadFace();
    EXPECT_EQ(untrimmed.tessellateTrimmedFace(0, 8).topology().faceCount(), 0u);
    EXPECT_EQ(b.tessellateTrimmedFace(999u, 8).topology().faceCount(), 0u);  // bad face id
}

// ──────────── Curved (polyline) pcurves: curved trim boundaries ───────────────
namespace {
// Find the outer-loop coedge of face 0 whose directed 3D endpoints are s → e.
uint32_t findCoedge(const Body& b, const Vec3& s, const Vec3& e)
{
    auto near = [](const Vec3& a, const Vec3& q) {
        return std::abs(a.x - q.x) < 1e-4f && std::abs(a.y - q.y) < 1e-4f &&
               std::abs(a.z - q.z) < 1e-4f;
    };
    const uint32_t first = b.loop(b.face(0).outerLoop).first;
    uint32_t c = first;
    do {
        const Coedge& ce = b.coedge(c);
        const Edge& ed = b.edge(ce.edge);
        const Vec3 sv = b.vertex(ce.reversed ? ed.v1 : ed.v0).point;
        const Vec3 ev = b.vertex(ce.reversed ? ed.v0 : ed.v1).point;
        if (near(sv, s) && near(ev, e)) return c;
        c = ce.next;
    } while (c != first);
    return kInvalid;
}
// Shoelace area of a (u,v) polygon, scaled by the (4u,4v) patch jacobian (16).
double shoelace16(const std::vector<std::pair<float, float>>& poly)
{
    double a = 0.0;
    for (size_t i = 0, n = poly.size(); i < n; ++i) {
        const auto& p = poly[i];
        const auto& q = poly[(i + 1) % n];
        a += static_cast<double>(p.first) * q.second - static_cast<double>(q.first) * p.second;
    }
    return std::abs(a) * 0.5 * 16.0;
}
}  // namespace

TEST(AnalyticBRepTrimTessellate, PolylineRectangularNotchExactArea)
{
    Body b = makeSubTrimmedNurbsFace();  // outer trim = [0.25,0.75]² (straight pcurves)
    const uint32_t bottom = findCoedge(b, Vec3{1, 1, 0}, Vec3{3, 1, 0});
    ASSERT_NE(bottom, kInvalid);
    // Replace the bottom straight pcurve with a rectangular notch bulging up into
    // the square: removes [0.4,0.6]×[0.25,0.35] (uv area 0.02).
    const std::vector<std::pair<float, float>> notch = {
        {0.25f, 0.25f}, {0.4f, 0.25f}, {0.4f, 0.35f},
        {0.6f, 0.35f},  {0.6f, 0.25f}, {0.75f, 0.25f}};
    ASSERT_TRUE(b.setCoedgePcurvePolyline(bottom, notch));
    ASSERT_TRUE(b.checkGeometry().ok);

    // The full trim polygon (bottom notched + the three straight sides).
    const std::vector<std::pair<float, float>> poly = {
        {0.25f, 0.25f}, {0.4f, 0.25f}, {0.4f, 0.35f}, {0.6f, 0.35f}, {0.6f, 0.25f},
        {0.75f, 0.25f}, {0.75f, 0.75f}, {0.25f, 0.75f}};
    const double expected = shoelace16(poly);  // (0.25 − 0.02)·16 = 3.68
    EXPECT_NEAR(expected, 3.68, 1e-4);

    // res=40 puts every notch boundary (0.25/0.4/0.6/0.75/0.35) on a grid line →
    // centre classification is exact.
    EXPECT_NEAR(meshArea(b.tessellateTrimmedFace(0, 40)), expected, 1e-3);
}

TEST(AnalyticBRepTrimTessellate, ArcTrimConvergesAndOnSurface)
{
    Body b = makeSubTrimmedNurbsFace();
    const uint32_t bottom = findCoedge(b, Vec3{1, 1, 0}, Vec3{3, 1, 0});
    ASSERT_NE(bottom, kInvalid);
    // A semicircular notch (centre (0.5,0.25), r=0.15) bulging up into the square,
    // sampled into a polyline arc between (0.35,0.25) and (0.65,0.25).
    std::vector<std::pair<float, float>> arc;
    arc.emplace_back(0.25f, 0.25f);
    const int N = 32;
    for (int k = 0; k <= N; ++k) {  // angle π → 0 (upper half)
        const float t = 3.14159265f * (1.f - static_cast<float>(k) / static_cast<float>(N));
        arc.emplace_back(0.5f + 0.15f * std::cos(t), 0.25f + 0.15f * std::sin(t));
    }
    arc.emplace_back(0.75f, 0.25f);
    ASSERT_TRUE(b.setCoedgePcurvePolyline(bottom, arc));
    ASSERT_TRUE(b.checkGeometry().ok);

    // Expected = the sampled-polyline trim area (exact for that polyline).
    std::vector<std::pair<float, float>> poly = arc;
    poly.emplace_back(0.75f, 0.75f);
    poly.emplace_back(0.25f, 0.75f);
    const double expected = shoelace16(poly);
    EXPECT_LT(expected, 4.0);   // notch removed material
    EXPECT_GT(expected, 3.2);

    // Finer grids converge toward the trimmed-polyline area.
    const double coarse = meshArea(b.tessellateTrimmedFace(0, 40));
    const double fine = meshArea(b.tessellateTrimmedFace(0, 240));
    EXPECT_LT(std::abs(fine - expected), 0.12);
    EXPECT_LE(std::abs(fine - expected), std::abs(coarse - expected) + 1e-9);

    // Every emitted vertex lies on the surface (z=0) and inside the trim ([1,3]²).
    const Mesh m = b.tessellateTrimmedFace(0, 240);
    const auto& p = m.attributes().positions();
    std::set<uint32_t> ref;
    for (size_t f = 0; f < m.topology().faceCount(); ++f)
        for (uint32_t i : m.topology().face(f).indices) ref.insert(i);
    for (uint32_t i : ref) {
        EXPECT_NEAR(p[i].z, 0.f, 1e-4f);
        EXPECT_GE(p[i].x, 1.f - 1e-4f);
        EXPECT_LE(p[i].x, 3.f + 1e-4f);
        EXPECT_GE(p[i].y, 1.f - 1e-4f);
        EXPECT_LE(p[i].y, 3.f + 1e-4f);
    }
}

TEST(AnalyticBRepTrimTessellate, PolylineRejectsBadInput)
{
    // Un-trimmed patch (evaluate = (2u,2v,0), domain [0,1]); bottom coedge
    // (0,0,0)→(2,0,0) maps to (u,v) (0,0)→(1,0).
    Body b = makeNurbsQuadFace();
    const uint32_t bottom = findCoedge(b, Vec3{0, 0, 0}, Vec3{2, 0, 0});
    ASSERT_NE(bottom, kInvalid);
    const float inf = std::bit_cast<float>(0x7F800000u);

    EXPECT_FALSE(b.setCoedgePcurvePolyline(bottom, {{0.0f, 0.0f}}));  // <2 points
    // Interior point outside the [0,1] parameter domain.
    EXPECT_FALSE(b.setCoedgePcurvePolyline(
        bottom, {{0.0f, 0.0f}, {5.0f, 5.0f}, {1.0f, 0.0f}}));
    // Non-finite interior point.
    EXPECT_FALSE(b.setCoedgePcurvePolyline(
        bottom, {{0.0f, 0.0f}, {inf, 0.3f}, {1.0f, 0.0f}}));
    // Endpoint that does not map to the coedge's start vertex.
    EXPECT_FALSE(b.setCoedgePcurvePolyline(
        bottom, {{0.6f, 0.6f}, {0.5f, 0.3f}, {1.0f, 0.0f}}));
    EXPECT_FALSE(b.coedge(bottom).pcurve.present);  // all rejects left it unattached

    // A valid curved polyline attaches and validates.
    EXPECT_TRUE(b.setCoedgePcurvePolyline(bottom, {{0.0f, 0.0f}, {0.5f, 0.2f}, {1.0f, 0.0f}}));
    EXPECT_TRUE(b.checkGeometry().ok);
}

TEST(AnalyticBRepTrimTessellate, PolylineSerializationV3RoundTrips)
{
    Body b = makeSubTrimmedNurbsFace();
    const uint32_t bottom = findCoedge(b, Vec3{1, 1, 0}, Vec3{3, 1, 0});
    ASSERT_NE(bottom, kInvalid);
    ASSERT_TRUE(b.setCoedgePcurvePolyline(
        bottom, {{0.25f, 0.25f}, {0.4f, 0.35f}, {0.6f, 0.35f}, {0.75f, 0.25f}}));

    const std::vector<std::uint8_t> bytes = b.serialize();
    EXPECT_EQ(bytes[4], 3u);  // writer stamps v3
    const auto rt = Body::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());
    EXPECT_TRUE(rt->checkGeometry().ok);
    EXPECT_EQ(bytes, rt->serialize());  // byte-identical
    EXPECT_EQ(rt->coedge(bottom).pcurve.interior.size(), 2u);  // curved trim survives
}

TEST(AnalyticBRepTrimTessellate, LegacyV1AndV2BlobsDecode)
{
    // v1: a body with no pcurves decodes when the version byte is patched to 1.
    const Body box = makeBox(2.f, 2.f, 2.f);
    std::vector<std::uint8_t> v1 = box.serialize();
    v1[4] = 1u;
    ASSERT_TRUE(Body::deserialize(v1).has_value());

    // v2: a body with a single STRAIGHT pcurve — its v3 interior-count (0) is the
    // trailing bytes, so patching the version to 2 still decodes (the v2 reader
    // stops before the interior count, leaving it as ignored trailing bytes).
    Body b = makeNurbsQuadFace();
    const uint32_t first = b.loop(b.face(0).outerLoop).first;
    const Coedge& ce = b.coedge(first);
    const Edge& ed = b.edge(ce.edge);
    const uint32_t sV = ce.reversed ? ed.v1 : ed.v0;
    const uint32_t eV = ce.reversed ? ed.v0 : ed.v1;
    const Vec3 sp = b.vertex(sV).point, ep = b.vertex(eV).point;
    ASSERT_TRUE(b.setCoedgePcurve(first, sp.x * 0.5f, sp.y * 0.5f, ep.x * 0.5f, ep.y * 0.5f));

    std::vector<std::uint8_t> v2 = b.serialize();
    ASSERT_EQ(v2[4], 3u);
    v2[4] = 2u;
    const auto rt = Body::deserialize(v2);
    ASSERT_TRUE(rt.has_value());
    EXPECT_TRUE(rt->checkGeometry().ok);
    EXPECT_TRUE(rt->coedge(first).pcurve.present);
    EXPECT_TRUE(rt->coedge(first).pcurve.interior.empty());
}

// ──────────── Interior trim-loop holes (parameter-space holes) ────────────────
namespace {
// Punch a square hole [lo,hi]² (in u,v) into face 0 of a (4u,4v)-patch body and
// attach its inner-loop pcurves. Returns true on success.
bool addSquareHole(Body& b, float lo, float hi)
{
    const std::vector<Vec3> ring = {
        {4 * lo, 4 * lo, 0}, {4 * hi, 4 * lo, 0}, {4 * hi, 4 * hi, 0}, {4 * lo, 4 * hi, 0}};
    const uint32_t first = b.addTrimHole(0, ring);
    if (first == kInvalid) return false;
    uint32_t c = first;
    do {  // inner-loop pcurve param = (x/4, y/4)
        const Coedge& ce = b.coedge(c);
        const Edge& ed = b.edge(ce.edge);
        const uint32_t sV = ce.reversed ? ed.v1 : ed.v0;
        const uint32_t eV = ce.reversed ? ed.v0 : ed.v1;
        const Vec3 sp = b.vertex(sV).point, ep = b.vertex(eV).point;
        if (!b.setCoedgePcurve(c, sp.x * 0.25f, sp.y * 0.25f, ep.x * 0.25f, ep.y * 0.25f))
            return false;
        c = ce.next;
    } while (c != first);
    return true;
}
}  // namespace

TEST(AnalyticBRepTrimHole, ExcludesInteriorHole)
{
    Body b = makeSubTrimmedNurbsFace();       // outer trim [0.25,0.75]² → area 4.0
    ASSERT_TRUE(addSquareHole(b, 0.4f, 0.6f));  // hole [0.4,0.6]² → removes 0.04·16
    EXPECT_TRUE(b.checkIntegrity().ok);
    const auto g = b.checkGeometry();
    EXPECT_TRUE(g.ok) << g.reason;

    // res=40 lands every boundary (0.25/0.4/0.6/0.75) on a grid line → exact.
    const Mesh m = b.tessellateTrimmedFace(0, 40);
    EXPECT_NEAR(meshArea(m), (0.25 - 0.04) * 16.0, 1e-3);  // 3.36 (< the hole-less 4.0)

    // No referenced vertex is strictly inside the hole ([1.6,2.4]²), and all lie
    // on the surface — the hole is genuinely excluded.
    const auto& p = m.attributes().positions();
    std::set<uint32_t> ref;
    for (size_t f = 0; f < m.topology().faceCount(); ++f)
        for (uint32_t i : m.topology().face(f).indices) ref.insert(i);
    for (uint32_t i : ref) {
        const bool inHole = p[i].x > 1.6f + 1e-4f && p[i].x < 2.4f - 1e-4f &&
                            p[i].y > 1.6f + 1e-4f && p[i].y < 2.4f - 1e-4f;
        EXPECT_FALSE(inHole);
        EXPECT_NEAR(p[i].z, 0.f, 1e-4f);
    }
}

TEST(AnalyticBRepTrimHole, SerializesAndDeterministic)
{
    Body b = makeSubTrimmedNurbsFace();
    ASSERT_TRUE(addSquareHole(b, 0.4f, 0.6f));

    const std::vector<std::uint8_t> bytes = b.serialize();
    const auto rt = Body::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());
    EXPECT_TRUE(rt->checkIntegrity().ok);
    EXPECT_TRUE(rt->checkGeometry().ok);
    EXPECT_EQ(bytes, rt->serialize());  // byte-identical (inner-loop pcurves survive)
    EXPECT_NEAR(meshArea(rt->tessellateTrimmedFace(0, 40)), 3.36, 1e-3);

    EXPECT_EQ(b.tessellateTrimmedFace(0, 24).topology().faceCount(),
              b.tessellateTrimmedFace(0, 24).topology().faceCount());
}

TEST(AnalyticBRepTrimHole, AddTrimHoleRejectsBadInput)
{
    Body b = makeSubTrimmedNurbsFace();
    const float inf = std::bit_cast<float>(0x7F800000u);
    EXPECT_EQ(b.addTrimHole(0, {{1.6f, 1.6f, 0}, {2.4f, 1.6f, 0}}), kInvalid);  // <3 points
    EXPECT_EQ(b.addTrimHole(99u, {{1.6f, 1.6f, 0}, {2.4f, 1.6f, 0}, {2.4f, 2.4f, 0}}),
              kInvalid);  // bad face id
    EXPECT_EQ(b.addTrimHole(0, {{inf, 1.6f, 0}, {2.4f, 1.6f, 0}, {2.4f, 2.4f, 0}}),
              kInvalid);  // non-finite point
    EXPECT_TRUE(b.checkIntegrity().ok);  // rejects left the body intact
}

// ──────────── Exact point-vs-plane predicate (facePlaneSide via orient3D) ─────

// On a box, the exact predicate maps to the outward normal with an on-plane band.
TEST(AnalyticBRepExactPredicate, FacePlaneSideOnBox)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    for (uint32_t f = 0; f < box.faceCount(); ++f) {
        const Surface& s = box.surface(box.face(f).surface);
        Vec3 n = s.normal;
        if (box.face(f).reversed) n = Vec3{-n.x, -n.y, -n.z};
        const Vec3 c = box.faceCentroid(f);
        const Vec3 outP{c.x + n.x * 0.5f, c.y + n.y * 0.5f, c.z + n.z * 0.5f};
        const Vec3 inP{c.x - n.x * 0.5f, c.y - n.y * 0.5f, c.z - n.z * 0.5f};
        EXPECT_EQ(box.facePlaneSide(f, outP), 1) << "face " << f;   // outward side
        EXPECT_EQ(box.facePlaneSide(f, inP), -1) << "face " << f;   // inward side
        EXPECT_EQ(box.facePlaneSide(f, c), 0) << "face " << f;      // on the plane (band)
    }
    EXPECT_EQ(box.facePlaneSide(999u, Vec3{0, 0, 0}), 0);  // bad face id
}

// A near-degenerate configuration: a planar face on 3x+5y+7z=0 with large integer
// coordinates. The exact orient3D-based side always matches the double-precision
// ground truth, while a naive float32 plane-distance flips sign on some points.
TEST(AnalyticBRepExactPredicate, FacePlaneSideBeatsNaiveFloat)
{
    // Large float32-exact integer coordinates on the plane 3x+5y+7z=0. The ground
    // truth is the LINEAR plane value 3x+5y+7z (exact in int64 at any magnitude);
    // orient3D (exact) matches it, while the old float32 signed-distance method
    // (dot(P, normalize((3,5,7)))) flips sign near the plane at this scale.
    const float K = 1000000.f;  // corners exactly on the plane, integer-representable
    const std::vector<Vec3> pts = {
        {7 * K, 0, -3 * K}, {0, 7 * K, -5 * K}, {-7 * K, 0, 3 * K}, {0, -7 * K, 5 * K}};
    Body::FaceDef fd;
    fd.loop = {0u, 1u, 2u, 3u};
    fd.surface.kind = SurfaceKind::Plane;
    fd.surface.origin = Vec3{0, 0, 0};
    const float inv83 = 1.f / std::sqrt(83.f);
    const Vec3 nHat{3 * inv83, 5 * inv83, 7 * inv83};  // float32 unit normal (irrational)
    fd.surface.normal = nHat;
    const float inv58 = 1.f / std::sqrt(58.f);
    fd.surface.uAxis = Vec3{7 * inv58, 0, -3 * inv58};
    const auto body = Body::fromFaces(pts, {fd});
    ASSERT_TRUE(body.has_value());

    int definite = 0, exactWrong = 0, naiveWrong = 0;
    for (int i = 0; i < 80; ++i) {
        for (int j = 0; j < 80; ++j) {
            const long long Px = 5000000LL + static_cast<long long>(i) * 131;
            const long long Py = -3000000LL - static_cast<long long>(j) * 91;
            const long long Pz = std::llround(-(3.0 * static_cast<double>(Px) +
                                                5.0 * static_cast<double>(Py)) / 7.0);
            // Ground truth: the EXACT plane value in int64 (plane is 3x+5y+7z=0).
            const long long gt = 3 * Px + 5 * Py + 7 * Pz;
            const int gtSign = gt > 0 ? 1 : (gt < 0 ? -1 : 0);
            const Vec3 P{static_cast<float>(Px), static_cast<float>(Py), static_cast<float>(Pz)};
            const int exact = body->facePlaneSide(0, P);
            // The old non-robust method: float32 signed distance to the plane.
            const float naive = P.x * nHat.x + P.y * nHat.y + P.z * nHat.z;
            const int naiveSign = naive > 0.f ? 1 : (naive < 0.f ? -1 : 0);

            if (gtSign == 0) continue;                 // exactly on the plane — skip
            if (exact == 0) continue;                  // within the on-plane band — skip
            ++definite;
            if (exact != gtSign) ++exactWrong;         // the exact predicate must never err
            if (naiveSign != gtSign) ++naiveWrong;     // the naive float path does
        }
    }
    EXPECT_GT(definite, 100);
    EXPECT_EQ(exactWrong, 0);   // orient3D is exact on every definite point
    EXPECT_GT(naiveWrong, 0);   // naive float32 mis-signs at least one — the point of exactness

    // Deterministic.
    const Vec3 probe{5000131.f, -3000091.f, -21428.f};
    EXPECT_EQ(body->facePlaneSide(0, probe), body->facePlaneSide(0, probe));
}

// ──────────── Exact segment-vs-triangle predicate ────────────────────────────

// segmentCrossesTriangleExact agrees with an exact int64 point-in-triangle ground
// truth on a near-degenerate sweep, while a naive float32 Möller–Trumbore
// barycentric test mis-classifies near the hypotenuse at large coordinates.
TEST(AnalyticBRepExactPredicate, SegmentTriangleBeatsNaiveFloat)
{
    const float K = 8000000.f;  // float32-exact (< 2^24); products break float32 M-T
    const Vec3 v0{0, 0, 0}, v1{K, 0, 0}, v2{0, K, 0};  // right triangle in z=0

    // Naive float32 Möller–Trumbore crossing test for a vertical downward segment.
    auto naiveCross = [&](const Vec3& A) -> bool {
        const Vec3 d{0.f, 0.f, -2.f};
        const Vec3 e1{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
        const Vec3 e2{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
        const Vec3 h{d.y * e2.z - d.z * e2.y, d.z * e2.x - d.x * e2.z, d.x * e2.y - d.y * e2.x};
        const float a = e1.x * h.x + e1.y * h.y + e1.z * h.z;
        if (a == 0.f) return false;
        const float f = 1.f / a;
        const Vec3 s{A.x - v0.x, A.y - v0.y, A.z - v0.z};
        const float u = f * (s.x * h.x + s.y * h.y + s.z * h.z);
        const Vec3 q{s.y * e1.z - s.z * e1.y, s.z * e1.x - s.x * e1.z, s.x * e1.y - s.y * e1.x};
        const float vv = f * (d.x * q.x + d.y * q.y + d.z * q.z);
        return u >= 0.f && u <= 1.f && vv >= 0.f && u + vv <= 1.f;
    };

    int definite = 0, exactWrong = 0, naiveWrong = 0;
    for (int i = 0; i < 40; ++i) {
        for (int m = -6; m <= 6; ++m) {
            const long long Qx = 4000000LL + static_cast<long long>(i) * 17;
            const long long Qy = 4000000LL - static_cast<long long>(i) * 17 + m;
            // Ground truth (exact int64): strictly inside the right triangle.
            const bool gtInside = Qx > 0 && Qy > 0 && (Qx + Qy) < static_cast<long long>(K);
            if (Qx + Qy == static_cast<long long>(K)) continue;  // on the hypotenuse — skip
            const Vec3 A{static_cast<float>(Qx), static_cast<float>(Qy), 1.f};
            const Vec3 B{static_cast<float>(Qx), static_cast<float>(Qy), -1.f};
            bool degenerate = false;
            const bool exact = segmentCrossesTriangleExact(A, B, v0, v1, v2, degenerate);
            if (degenerate) continue;  // exact edge/vertex hit
            ++definite;
            if (exact != gtInside) ++exactWrong;
            if (naiveCross(A) != gtInside) ++naiveWrong;
        }
    }
    EXPECT_GT(definite, 100);
    EXPECT_EQ(exactWrong, 0);   // orient3D-based test is exact on every definite point
    EXPECT_GT(naiveWrong, 0);   // naive float32 barycentric mis-classifies near the edge

    // A clean interior crossing and a clean miss, and determinism.
    bool dg = false;
    EXPECT_TRUE(segmentCrossesTriangleExact({1000000.f, 1000000.f, 1.f},
                                            {1000000.f, 1000000.f, -1.f}, v0, v1, v2, dg));
    EXPECT_FALSE(dg);
    EXPECT_FALSE(segmentCrossesTriangleExact({7000000.f, 7000000.f, 1.f},
                                             {7000000.f, 7000000.f, -1.f}, v0, v1, v2, dg));
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
