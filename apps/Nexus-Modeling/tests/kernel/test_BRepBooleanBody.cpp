// Foundation — analytic B-rep boolean sewn into a Body. booleanToBody assembles
// the kept faces (via Body::fromFaces) into a first-class analytic solid: both
// validators clean, closed, euler 2, correct volume — and it can feed another
// boolean (booleans compose).

#include <nexus/geometry/BRepBoolean.h>

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

Body boxAt(Vec3 c, float w, float h, float d)
{
    Body b = makeBox(w, h, d);
    b.translate(c);
    return b;
}

void expectValidClosedSolid(const Body& r)
{
    const auto ig = r.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);
    EXPECT_EQ(ig.boundaryEdges, 0u);
    EXPECT_TRUE(r.checkGeometry().ok) << r.checkGeometry().reason;
    EXPECT_TRUE(r.isClosed());
}
}  // namespace

TEST(BRepBooleanBody, CornerOverlapSewnSolids)
{
    const Body a = makeBox(2.f, 2.f, 2.f);        // [-1,1]^3
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);  // [0,2]^3

    const Body uni = booleanToBody(a, b, BooleanOp::Union);
    const Body inter = booleanToBody(a, b, BooleanOp::Intersection);
    const Body diff = booleanToBody(a, b, BooleanOp::Difference);

    expectValidClosedSolid(uni);
    expectValidClosedSolid(inter);
    expectValidClosedSolid(diff);

    EXPECT_NEAR(signedVolume(uni.toMesh()), 15.0, 1e-4);
    EXPECT_NEAR(signedVolume(inter.toMesh()), 1.0, 1e-4);
    EXPECT_NEAR(signedVolume(diff.toMesh()), 7.0, 1e-4);
}

TEST(BRepBooleanBody, OffsetOverlapSewnSolids)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 0.5f, 0.5f}, 2, 2, 2);  // overlap 1 x 1.5 x 1.5 = 2.25

    const Body uni = booleanToBody(a, b, BooleanOp::Union);
    const Body inter = booleanToBody(a, b, BooleanOp::Intersection);
    const Body diff = booleanToBody(a, b, BooleanOp::Difference);

    expectValidClosedSolid(uni);
    expectValidClosedSolid(inter);
    expectValidClosedSolid(diff);

    EXPECT_NEAR(signedVolume(uni.toMesh()), 13.75, 1e-4);
    EXPECT_NEAR(signedVolume(inter.toMesh()), 2.25, 1e-4);
    EXPECT_NEAR(signedVolume(diff.toMesh()), 5.75, 1e-4);
}

TEST(BRepBooleanBody, ResultFeedsAnotherBoolean)
{
    // (A ∪ B) is a first-class solid usable as an input to a further boolean.
    const Body a = makeBox(2.f, 2.f, 2.f);        // [-1,1]^3
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);  // [0,2]^3
    const Body ab = booleanToBody(a, b, BooleanOp::Union);  // vol 15
    expectValidClosedSolid(ab);

    const Body c = boxAt({-1.f, -1.f, -1.f}, 2, 2, 2);  // [-2,0]^3, overlaps A in [-1,0]^3
    const Body chained = booleanToBody(ab, c, BooleanOp::Union);
    expectValidClosedSolid(chained);
    // 15 + 8 - 1 (C∩(A∪B) is the [-1,0]^3 corner of A) = 22.
    EXPECT_NEAR(signedVolume(chained.toMesh()), 22.0, 1e-4);
}

TEST(BRepBooleanBody, Deterministic)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);
    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
        const Body r1 = booleanToBody(a, b, op);
        const Body r2 = booleanToBody(a, b, op);
        EXPECT_EQ(r1.faceCount(), r2.faceCount());
        EXPECT_EQ(r1.vertexCount(), r2.vertexCount());
        EXPECT_NEAR(signedVolume(r1.toMesh()), signedVolume(r2.toMesh()), 1e-6);
    }
}

}  // namespace nexus::geometry::brep::testing
