// Foundation — analytic B-rep serialization. Body::serialize / deserialize
// round-trips the full analytic B-rep (topology + geometry + NURBS store) through
// a versioned binary format: the decoded body is validator-clean, count/euler/
// volume-identical, and re-serializes byte-for-byte. Bad/truncated/non-finite
// buffers are rejected (nullopt), never crash.

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

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

// Round-trip `b` and assert the decoded body matches it exactly.
void expectRoundTrips(const Body& b)
{
    const std::vector<std::uint8_t> bytes = b.serialize();
    const auto rt = Body::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());

    EXPECT_TRUE(rt->checkIntegrity().ok) << rt->checkIntegrity().reason;
    EXPECT_TRUE(rt->checkGeometry().ok) << rt->checkGeometry().reason;

    const auto i0 = b.checkIntegrity();
    const auto i1 = rt->checkIntegrity();
    EXPECT_EQ(i0.vertices, i1.vertices);
    EXPECT_EQ(i0.edges, i1.edges);
    EXPECT_EQ(i0.faces, i1.faces);
    EXPECT_EQ(i0.euler, i1.euler);
    EXPECT_EQ(b.isClosed(), rt->isClosed());
    EXPECT_NEAR(signedVolume(b.toMesh()), signedVolume(rt->toMesh()), 1e-5);

    // Re-serialization is byte-identical (format is canonical / lossless).
    EXPECT_EQ(bytes, rt->serialize());
}
}  // namespace

TEST(BRepSerialize, RoundTripsPrimitives)
{
    expectRoundTrips(makeBox(2.f, 3.f, 4.f));
    expectRoundTrips(makeCylinder(1.f, 2.f, 24));  // has Circle-arc edges
    expectRoundTrips(makeSphere(1.f, 8, 12));       // Sphere surfaces + arc edges
}

TEST(BRepSerialize, RoundTripsBooleanAndSimplifiedBodies)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);

    const Body uni = booleanToBody(a, b, BooleanOp::Union);  // has tombstoned entities
    expectRoundTrips(uni);

    Body simplified = uni;
    simplified.simplify();
    expectRoundTrips(simplified);
}

TEST(BRepSerialize, DeterministicBytes)
{
    const Body b = booleanToBody(makeBox(2.f, 2.f, 2.f), boxAt({1.f, 1.f, 1.f}, 2, 2, 2),
                                 BooleanOp::Difference);
    EXPECT_EQ(b.serialize(), b.serialize());
}

TEST(BRepSerialize, RejectsBadBuffers)
{
    EXPECT_FALSE(Body::deserialize({}).has_value());                       // empty
    EXPECT_FALSE(Body::deserialize({1, 2, 3, 4, 5, 6, 7, 8}).has_value());  // bad magic

    // Truncated mid-stream.
    std::vector<std::uint8_t> good = makeBox(2.f, 2.f, 2.f).serialize();
    good.resize(good.size() / 2);
    EXPECT_FALSE(Body::deserialize(good).has_value());
}

TEST(BRepSerialize, RejectsNonFiniteFloats)
{
    // Corrupt the first serialized vertex's x-coordinate to +Inf. Layout: 4-byte
    // magic + 4-byte version + 4-byte vertex count, then the first Vec3.
    std::vector<std::uint8_t> bytes = makeBox(2.f, 2.f, 2.f).serialize();
    ASSERT_GT(bytes.size(), 16u);
    bytes[12] = 0x00;
    bytes[13] = 0x00;
    bytes[14] = 0x80;
    bytes[15] = 0x7F;  // 0x7F800000 = +Inf (little-endian)
    EXPECT_FALSE(Body::deserialize(bytes).has_value());
}

}  // namespace nexus::geometry::brep::testing
