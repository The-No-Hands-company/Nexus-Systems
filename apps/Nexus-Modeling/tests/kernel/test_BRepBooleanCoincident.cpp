// Foundation — analytic B-rep boolean with COINCIDENT faces (coplanar overlap).
// selectFace resolves faces that lie on the other solid's boundary via the
// relative outward orientation (probe just inside the face): SAME-side interiors
// keep one copy for Union/Intersection; OPPOSITE (touching) drop it there but
// keep it for Difference. Proven on aligned boxes that share a face plane.

#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshTopologyAnalysis.h>

#include <gtest/gtest.h>

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

// Axis-aligned box spanning [lo, hi], built with valid Curve/Surface frames.
Body boxMinMax(Vec3 lo, Vec3 hi)
{
    Body b = makeBox(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
    b.translate({(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f});
    return b;
}

void expectWatertight(const Mesh& m)
{
    const TopologyInfo info = MeshTopologyAnalyser::analyse(m);
    EXPECT_TRUE(info.isManifold);
    EXPECT_TRUE(info.isClosed);
    EXPECT_EQ(info.euler, 2);
}
}  // namespace

TEST(BRepBooleanCoincident, StackedCubesOppositeNormal)
{
    // A = [0,1]^3, B = [0,1]x[0,1]x[1,2] — share the z=1 face (opposite outward
    // normals: A's top +z, B's bottom -z; the solids merely touch).
    const Body a = boxMinMax({0, 0, 0}, {1, 1, 1});
    const Body b = boxMinMax({0, 0, 1}, {1, 1, 2});

    const Mesh uni = booleanToMesh(a, b, BooleanOp::Union);
    const Mesh diff = booleanToMesh(a, b, BooleanOp::Difference);
    const Mesh inter = booleanToMesh(a, b, BooleanOp::Intersection);

    expectWatertight(uni);
    EXPECT_NEAR(signedVolume(uni), 2.0, 1e-4);  // the 1x1x2 box; shared face gone

    expectWatertight(diff);
    EXPECT_NEAR(signedVolume(diff), 1.0, 1e-4);  // A - B = A (B only touches)

    // Intersection is a zero-volume contact — a clean empty result, not a crash.
    EXPECT_EQ(inter.topology().faceCount(), 0u);
    EXPECT_NEAR(signedVolume(inter), 0.0, 1e-4);
}

TEST(BRepBooleanCoincident, NestedShareFaceSameNormal)
{
    // A = [0,2]x[0,2]x[0,1] (vol 4), B = [0,1]^3 (vol 1) nested in a corner of A,
    // sharing the z=0 bottom (and x=0, y=0) planes with the SAME outward normal.
    const Body a = boxMinMax({0, 0, 0}, {2, 2, 1});
    const Body b = boxMinMax({0, 0, 0}, {1, 1, 1});

    const Mesh uni = booleanToMesh(a, b, BooleanOp::Union);
    const Mesh inter = booleanToMesh(a, b, BooleanOp::Intersection);
    const Mesh diff = booleanToMesh(a, b, BooleanOp::Difference);

    expectWatertight(uni);
    expectWatertight(inter);
    expectWatertight(diff);

    EXPECT_NEAR(signedVolume(uni), 4.0, 1e-4);    // B ⊂ A → A
    EXPECT_NEAR(signedVolume(inter), 1.0, 1e-4);  // B ⊂ A → B
    EXPECT_NEAR(signedVolume(diff), 3.0, 1e-4);   // A minus the corner cube
}

TEST(BRepBooleanCoincident, SewnBodyWithCoincidentFaces)
{
    const Body a = boxMinMax({0, 0, 0}, {1, 1, 1});
    const Body b = boxMinMax({0, 0, 1}, {1, 1, 2});

    const Body uni = booleanToBody(a, b, BooleanOp::Union);
    ASSERT_TRUE(uni.checkIntegrity().ok) << uni.checkIntegrity().reason;
    EXPECT_EQ(uni.checkIntegrity().euler, 2);
    EXPECT_TRUE(uni.isClosed());
    EXPECT_TRUE(uni.checkGeometry().ok);
    EXPECT_NEAR(signedVolume(uni.toMesh()), 2.0, 1e-4);

    const Body diff = booleanToBody(a, b, BooleanOp::Difference);
    ASSERT_TRUE(diff.checkIntegrity().ok) << diff.checkIntegrity().reason;
    EXPECT_TRUE(diff.isClosed());
    EXPECT_NEAR(signedVolume(diff.toMesh()), 1.0, 1e-4);

    // Degenerate intersection → an empty Body (no faces), not a crash.
    const Body inter = booleanToBody(a, b, BooleanOp::Intersection);
    EXPECT_EQ(inter.faceCount(), 0u);
}

TEST(BRepBooleanCoincident, Deterministic)
{
    const Body a = boxMinMax({0, 0, 0}, {2, 2, 1});
    const Body b = boxMinMax({0, 0, 0}, {1, 1, 1});
    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
        const Mesh m1 = booleanToMesh(a, b, op);
        const Mesh m2 = booleanToMesh(a, b, op);
        EXPECT_EQ(m1.topology().faceCount(), m2.topology().faceCount());
        EXPECT_NEAR(signedVolume(m1), signedVolume(m2), 1e-6);
    }
}

}  // namespace nexus::geometry::brep::testing
