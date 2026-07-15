// Foundation — analytic B-rep boolean (select + emit), the boolean KEYSTONE.
// booleanToMesh mutually imprints two solids, selects the faces kept per op,
// and emits one welded triangle Mesh. Proven on overlapping boxes: the result
// is watertight (closed, 2-manifold, euler 2) with the exact regularised-boolean
// volume for Union / Intersection / Difference.

#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshTopologyAnalysis.h>

#include <gtest/gtest.h>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
// Signed volume enclosed by a closed triangle mesh (divergence theorem):
// Σ v0 · (v1 × v2) / 6. Positive when triangles wind outward.
double signedVolume(const Mesh& m)
{
    const auto& p = m.attributes().positions();
    const auto& t = m.topology();
    double vol = 0.0;
    for (size_t i = 0; i < t.faceCount(); ++i) {
        const auto& id = t.face(i).indices;
        if (id.size() != 3) continue;
        const Vec3 a = p[id[0]], b = p[id[1]], c = p[id[2]];
        vol += static_cast<double>(a.dot(b.cross(c))) / 6.0;
    }
    return vol;
}

Body boxAt(Vec3 c, float w, float h, float d)
{
    Body b = makeBox(w, h, d);
    b.translate(c);
    return b;
}

void expectWatertight(const Mesh& m)
{
    const TopologyInfo info = MeshTopologyAnalyser::analyse(m);
    EXPECT_TRUE(info.isManifold);
    EXPECT_TRUE(info.isClosed);
    EXPECT_EQ(info.euler, 2);  // closed genus-0 solid boundary
}
}  // namespace

TEST(BRepBoolean, CornerOverlapBoxesExactVolumes)
{
    // A = [-1,1]^3 (vol 8), B = [0,2]^3 (vol 8), overlap [0,1]^3 (vol 1).
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);

    const Mesh uni = booleanToMesh(a, b, BooleanOp::Union);
    const Mesh inter = booleanToMesh(a, b, BooleanOp::Intersection);
    const Mesh diff = booleanToMesh(a, b, BooleanOp::Difference);

    expectWatertight(uni);
    expectWatertight(inter);
    expectWatertight(diff);

    EXPECT_NEAR(signedVolume(uni), 15.0, 1e-4);    // 8 + 8 - 1
    EXPECT_NEAR(signedVolume(inter), 1.0, 1e-4);   // the overlap cube
    EXPECT_NEAR(signedVolume(diff), 7.0, 1e-4);    // 8 - 1
}

TEST(BRepBoolean, OffsetOverlapExactVolumes)
{
    // A = [-1,1]^3, B = [0,2] x [-0.5,1.5] x [-0.5,1.5]; overlap 1 x 1.5 x 1.5 = 2.25.
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 0.5f, 0.5f}, 2, 2, 2);

    const Mesh uni = booleanToMesh(a, b, BooleanOp::Union);
    const Mesh inter = booleanToMesh(a, b, BooleanOp::Intersection);
    const Mesh diff = booleanToMesh(a, b, BooleanOp::Difference);

    expectWatertight(uni);
    expectWatertight(inter);
    expectWatertight(diff);

    EXPECT_NEAR(signedVolume(uni), 13.75, 1e-4);   // 8 + 8 - 2.25
    EXPECT_NEAR(signedVolume(inter), 2.25, 1e-4);
    EXPECT_NEAR(signedVolume(diff), 5.75, 1e-4);   // 8 - 2.25
}

TEST(BRepBoolean, DifferenceIsOrientedOutward)
{
    // A - B must have positive enclosed volume (outward-consistent winding),
    // i.e. the reversed B cavity walls point outward for the A-B solid.
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);
    EXPECT_GT(signedVolume(booleanToMesh(a, b, BooleanOp::Difference)), 0.0);
}

TEST(BRepBoolean, Deterministic)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);
    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
        const Mesh m1 = booleanToMesh(a, b, op);
        const Mesh m2 = booleanToMesh(a, b, op);
        EXPECT_EQ(m1.topology().faceCount(), m2.topology().faceCount());
        EXPECT_EQ(m1.attributes().positions().size(), m2.attributes().positions().size());
        EXPECT_NEAR(signedVolume(m1), signedVolume(m2), 1e-6);
    }
}

}  // namespace nexus::geometry::brep::testing
