// Foundation — analytic B-rep CIRCLE imprint on a coplanar planar face (the first
// curved-boolean building block). imprintCurve, generalised from Line-only, cuts
// a planar face along a Circle arc when the circle lies in the face's plane and
// crosses the boundary at two points on distinct edges. The cut edge is a true
// arc on the circle and on the face.

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
    double vol = 0.0;
    for (size_t i = 0; i < t.faceCount(); ++i) {
        const auto& id = t.face(i).indices;
        if (id.size() != 3) continue;
        vol += static_cast<double>(p[id[0]].dot(p[id[1]].cross(p[id[2]]))) / 6.0;
    }
    return vol;
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

Curve circleZ1(Vec3 centre, float r)
{
    Curve c;
    c.kind = CurveKind::Circle;
    c.origin = centre;
    c.dir = {0, 0, 1};  // axis ⟂ the z=1 plane
    c.ref = {1, 0, 0};
    c.radius = r;
    return c;
}
}  // namespace

TEST(BRepImprintCircle, ArcSplitsCoplanarFace)
{
    Body b = makeBox(2.f, 2.f, 2.f);  // top face z=1, x,y ∈ [-1,1]
    const uint32_t tf = topFace(b);
    ASSERT_NE(tf, kInvalid);

    // Circle centred on the +X/+Y corner, radius 1.5 → crosses the +X and +Y edges.
    const Curve circ = circleZ1({1, 1, 1}, 1.5f);
    const auto i0 = b.checkIntegrity();
    const uint32_t nf = b.imprintCurve(tf, circ);
    ASSERT_NE(nf, kInvalid);

    const auto i1 = b.checkIntegrity();
    EXPECT_TRUE(i1.ok) << i1.reason;
    EXPECT_EQ(i1.euler, 2);                        // χ-neutral
    EXPECT_EQ(i1.faces, i0.faces + 1);             // one new face
    EXPECT_EQ(i1.boundaryEdges, 0u);               // still watertight
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
    EXPECT_TRUE(b.isClosed());
    EXPECT_NEAR(signedVolume(b.toMesh(4)), 8.0, 1e-4);  // planar cut, solid unchanged

    // The shared cut edge is a Circle arc; its samples lie on the circle (radius
    // 1.5 from the centre) and on the face plane (z=1).
    int arc = -1;
    float maxRErr = 0.f, maxZErr = 0.f;
    for (uint32_t e = 0; e < b.edgeCount(); ++e) {
        const auto& ed = b.edge(e);
        if (!ed.alive) continue;
        const auto& cu = b.curve(ed.curve);
        if (cu.kind != CurveKind::Circle) continue;
        const Vec3 p0 = b.vertex(ed.v0).point, p1 = b.vertex(ed.v1).point;
        if (std::abs(p0.z - 1.f) > 1e-3f || std::abs(p1.z - 1.f) > 1e-3f) continue;
        arc = static_cast<int>(e);
        for (int k = 0; k <= 12; ++k) {
            const float t = ed.t0 + (ed.t1 - ed.t0) * static_cast<float>(k) / 12.f;
            const Vec3 p = cu.eval(t);
            const float rr = std::sqrt((p.x - 1.f) * (p.x - 1.f) + (p.y - 1.f) * (p.y - 1.f));
            maxRErr = std::max(maxRErr, std::abs(rr - 1.5f));
            maxZErr = std::max(maxZErr, std::abs(p.z - 1.f));
        }
    }
    ASSERT_GE(arc, 0);
    EXPECT_LT(maxRErr, 1e-4f);
    EXPECT_LT(maxZErr, 1e-4f);
}

TEST(BRepImprintCircle, NonCoplanarCircleRejected)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    const uint32_t tf = topFace(b);
    // Circle whose axis lies IN the face plane → the arc would leave the face.
    Curve c;
    c.kind = CurveKind::Circle;
    c.origin = {0, 0, 1};
    c.dir = {1, 0, 0};  // axis in-plane, not ⟂
    c.ref = {0, 1, 0};
    c.radius = 0.5f;
    EXPECT_EQ(b.imprintCurve(tf, c), kInvalid);
    EXPECT_EQ(b.checkIntegrity().faces, 6u);  // unchanged
}

TEST(BRepImprintCircle, FullyInteriorCircleAddsHole)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    const uint32_t tf = topFace(b);
    // Circle entirely inside the face → an inner loop (hole), not a split. It
    // returns the same face and adds one inner loop. (Full hole behaviour is
    // covered by test_BRepInnerLoop.)
    EXPECT_EQ(b.imprintCurve(tf, circleZ1({0, 0, 1}, 0.3f)), tf);
    EXPECT_EQ(b.face(tf).innerLoops.size(), 1u);
    EXPECT_TRUE(b.checkIntegrity().ok);
    EXPECT_TRUE(b.checkGeometry().ok);
}

TEST(BRepImprintCircle, LineImprintStillWorks)
{
    // The Line path is unchanged: a Line diagonal still splits the face.
    Body b = makeBox(2.f, 2.f, 2.f);
    const uint32_t tf = topFace(b);
    Curve line;
    line.kind = CurveKind::Line;
    line.origin = {0, 0, 1};
    line.dir = {0, 1, 0};  // x=0 line in the z=1 plane
    const uint32_t nf = b.imprintCurve(tf, line);
    ASSERT_NE(nf, kInvalid);
    EXPECT_TRUE(b.checkIntegrity().ok);
    EXPECT_EQ(b.checkIntegrity().euler, 2);
    EXPECT_TRUE(b.checkGeometry().ok);
}

}  // namespace nexus::geometry::brep::testing
