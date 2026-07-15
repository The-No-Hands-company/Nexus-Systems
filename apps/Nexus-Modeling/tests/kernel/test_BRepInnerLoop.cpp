// Foundation — analytic B-rep inner loops (holes). A circle lying fully inside a
// planar face imprints as an INNER LOOP (a cylinder-through-face hole): the
// validators accept the holed face, its edges are boundary edges (open shell in
// isolation), and toMesh tessellates the face-with-hole watertight (area = outer
// − hole). Bodies without holes are unaffected.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
double signedVolume(const Mesh& m)
{
    const auto& p = m.attributes().positions();
    const auto& t = m.topology();
    double v = 0.0;
    for (size_t i = 0; i < t.faceCount(); ++i) {
        const auto& id = t.face(i).indices;
        if (id.size() != 3) continue;
        v += static_cast<double>(p[id[0]].dot(p[id[1]].cross(p[id[2]]))) / 6.0;
    }
    return v;
}

// Summed area of mesh triangles whose vertices all lie on the plane z = zLevel.
double areaAtZ(const Mesh& m, float zLevel)
{
    const auto& p = m.attributes().positions();
    const auto& t = m.topology();
    double A = 0.0;
    for (size_t i = 0; i < t.faceCount(); ++i) {
        const auto& id = t.face(i).indices;
        if (id.size() != 3) continue;
        const Vec3 a = p[id[0]], b = p[id[1]], c = p[id[2]];
        if (std::abs(a.z - zLevel) > 1e-3f || std::abs(b.z - zLevel) > 1e-3f ||
            std::abs(c.z - zLevel) > 1e-3f)
            continue;
        A += 0.5 * static_cast<double>((b - a).cross(c - a).length());
    }
    return A;
}

uint32_t topFace(const Body& b)
{
    for (uint32_t f = 0; f < b.faceCount(); ++f) {
        const auto vs = b.faceVertices(f);
        if (vs.empty()) continue;
        bool all = true;
        for (uint32_t v : vs)
            if (std::abs(b.vertex(v).point.z - 1.f) > 1e-4f) { all = false; break; }
        if (all) return f;
    }
    return kInvalid;
}

Curve circleZ1(Vec3 c, float r)
{
    Curve cu;
    cu.kind = CurveKind::Circle;
    cu.origin = c;
    cu.dir = {0, 0, 1};
    cu.ref = {1, 0, 0};
    cu.radius = r;
    return cu;
}
}  // namespace

TEST(BRepInnerLoop, InteriorCircleAddsHole)
{
    Body b = makeBox(2.f, 2.f, 2.f);  // top face z=1, area 4
    const uint32_t tf = topFace(b);
    ASSERT_NE(tf, kInvalid);

    ASSERT_EQ(b.imprintCurve(tf, circleZ1({0, 0, 1}, 0.4f)), tf);  // returns the face
    EXPECT_EQ(b.face(tf).innerLoops.size(), 1u);

    // Topology + geometry are valid; the hole's edges are boundary edges, so in
    // isolation the shell is OPEN (a following boolean caps it with the tool wall).
    const auto ig = b.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
    EXPECT_EQ(ig.boundaryEdges, 8u);  // K=8 arc edges around the hole
    EXPECT_FALSE(b.isClosed());
}

TEST(BRepInnerLoop, HoleTessellationHasCorrectArea)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    const uint32_t tf = topFace(b);
    const float r = 0.4f;
    ASSERT_EQ(b.imprintCurve(tf, circleZ1({0, 0, 1}, r)), tf);

    // The top face tessellates to area 4 − π r² (within the arc-polygon tolerance).
    const double expected = 4.0 - M_PI * static_cast<double>(r) * static_cast<double>(r);
    EXPECT_NEAR(areaAtZ(b.toMesh(8), 1.f), expected, 0.02);
    EXPECT_LT(areaAtZ(b.toMesh(8), 1.f), 4.0);  // strictly less than the full face
}

TEST(BRepInnerLoop, CircleTooLargeRejected)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    const uint32_t tf = topFace(b);
    // Radius 5 circle spills outside the face → not a clean hole → kInvalid.
    EXPECT_EQ(b.imprintCurve(tf, circleZ1({0, 0, 1}, 5.f)), kInvalid);
    EXPECT_EQ(b.checkIntegrity().faces, 6u);  // unchanged
}

TEST(BRepInnerLoop, HolelessBodiesUnaffected)
{
    // isClosed() + the toMesh changes must not regress solids without holes.
    for (const Body& b : {makeBox(2.f, 2.f, 2.f), makeCylinder(1.f, 2.f, 24),
                          makeSphere(1.f, 8, 12)}) {
        EXPECT_TRUE(b.checkIntegrity().ok);
        EXPECT_EQ(b.checkIntegrity().euler, 2);
        EXPECT_EQ(b.checkIntegrity().boundaryEdges, 0u);
        EXPECT_TRUE(b.isClosed());
        EXPECT_TRUE(b.checkGeometry().ok);
    }
    EXPECT_NEAR(signedVolume(makeBox(2.f, 2.f, 2.f).toMesh(4)), 8.0, 1e-4);
}

}  // namespace nexus::geometry::brep::testing
