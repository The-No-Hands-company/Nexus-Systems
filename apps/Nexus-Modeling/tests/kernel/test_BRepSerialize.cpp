// Foundation — analytic B-rep serialization. Body::serialize / deserialize
// round-trips the full analytic B-rep (topology + geometry + NURBS store) through
// a versioned binary format: the decoded body is validator-clean, count/euler/
// volume-identical, and re-serializes byte-for-byte. Bad/truncated/non-finite
// buffers are rejected (nullopt), never crash.

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>
#include <string>
#include <iterator>
#include <fstream>

#include <cstdint>
#include <vector>

namespace nexus::geometry::brep::testing {


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
    // Corrupt the first serialized vertex's x-coordinate to +Inf. Layout: 4-byte magic +
    // 4-byte version + 4-byte vertex count, then the first position — which is a DOUBLE
    // from v4 on, so the pattern is eight bytes, not four.
    std::vector<std::uint8_t> bytes = makeBox(2.f, 2.f, 2.f).serialize();
    ASSERT_GT(bytes.size(), 20u);
    for (int i = 0; i < 6; ++i) bytes[12 + i] = 0x00;
    bytes[18] = 0xF0;
    bytes[19] = 0x7F;  // 0x7FF0000000000000 = +Inf (little-endian)
    EXPECT_FALSE(Body::deserialize(bytes).has_value());
}


// ── BACKWARD COMPATIBILITY, against a REAL old blob ─────────────────────────────
//
// v4 widened stored positions from float to double. The kernel's rule is that a version
// bump must not break existing files, and until now that was checked by relabelling a
// CURRENT blob with an older version byte — which only ever worked because every version
// happened to share one payload layout. Across a width change that trick tests nothing: it
// would be a v4 payload with a v1 header, and decoding it as v1 means reading doubles as
// floats.
//
// So the fixture is a genuine v3 blob, produced by the v3 writer before the migration and
// committed as bytes. It cannot drift with the code, which is the entire point of it.
TEST(BRepSerialize, LegacyV3FixtureStillDecodes)
{
    const std::string path = std::string(NEXUS_TESTS_ROOT) + "/kernel/fixtures/brep_v3_box.nxb";
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.good()) << "missing fixture " << path;
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    ASSERT_GT(bytes.size(), 8u);
    EXPECT_EQ(bytes[4], 3u) << "the fixture is supposed to be a v3 blob";

    const auto rt = Body::deserialize(bytes);
    ASSERT_TRUE(rt.has_value()) << "a v3 blob no longer decodes — backward compatibility broke";
    EXPECT_TRUE(rt->checkIntegrity().ok) << rt->checkIntegrity().reason;
    EXPECT_TRUE(rt->checkGeometry().ok) << rt->checkGeometry().reason;
    EXPECT_TRUE(rt->isClosed());
    EXPECT_EQ(rt->faceCount(), 6u);
    EXPECT_NEAR(rt->massProperties().volume, 8.0, 1e-4);

    // It decodes to the same solid the current writer produces for the same box, so the
    // widening did not move any geometry — it only stopped rounding it.
    const Body now = makeBox(2.f, 2.f, 2.f);
    EXPECT_EQ(rt->vertexCount(), now.vertexCount());
    for (uint32_t v = 0; v < static_cast<uint32_t>(now.vertexCount()); ++v) {
        EXPECT_NEAR(rt->vertex(v).point.x, now.vertex(v).point.x, 1e-6);
        EXPECT_NEAR(rt->vertex(v).point.y, now.vertex(v).point.y, 1e-6);
        EXPECT_NEAR(rt->vertex(v).point.z, now.vertex(v).point.z, 1e-6);
    }
}

// And the widened format keeps precision a float blob could not: a coordinate that needs
// more than 24 bits of mantissa survives a round trip exactly.
TEST(BRepSerialize, DoublePositionsSurviveTheRoundTripExactly)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    // A value with significance well past float's 24-bit mantissa.
    const double precise = 1.2345678901234567;
    b.vertexMut(0).point = Vec3{precise, -precise, precise * 0.5};

    const auto rt = Body::deserialize(b.serialize());
    ASSERT_TRUE(rt.has_value());
    EXPECT_EQ(rt->vertex(0).point.x, precise) << "a double coordinate was rounded by the format";
    EXPECT_EQ(rt->vertex(0).point.y, -precise);
    EXPECT_EQ(rt->vertex(0).point.z, precise * 0.5);
    // The same value through float would have lost it — this is what v4 buys.
    EXPECT_NE(static_cast<double>(static_cast<float>(precise)), precise);
}

}  // namespace nexus::geometry::brep::testing
