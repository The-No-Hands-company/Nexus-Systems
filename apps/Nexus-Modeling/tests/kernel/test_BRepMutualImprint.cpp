// Foundation — analytic B-rep mutual imprint (the boolean's SEGMENTATION step).
// imprintMutually cuts two overlapping solids along their face-plane
// intersection lines until NO face of either straddles the other's boundary —
// the precondition for selecting and sewing sub-faces per boolean op. Proven on
// overlapping boxes: both bodies stay watertight/valid and every face becomes
// uniformly Inside or Outside the other solid.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;
using PC = Body::PointContainment;

namespace {
// A correctly-built axis-aligned box centred at `c` (valid Curve/Surface frames).
Body boxAt(Vec3 c, float w, float h, float d)
{
    Body b = makeBox(w, h, d);
    b.translate(c);
    return b;
}

// True if every strictly-interior sample of face f classifies the same way
// against `other` (i.e. the face does not straddle other's boundary). Samples
// the centroid plus interior points toward each vertex.
bool faceIsUniform(const Body& body, uint32_t f, const Body& other, PC& cls)
{
    const auto vs = body.faceVertices(f);
    if (vs.empty()) return true;
    const Vec3 c = body.faceCentroid(f);
    cls = other.classifyPoint(c);
    bool uniform = true;
    for (float t : {0.3f, 0.6f, 0.85f}) {
        for (uint32_t v : vs) {
            const Vec3 p = body.vertex(v).point;
            const Vec3 s{c.x + (p.x - c.x) * t, c.y + (p.y - c.y) * t, c.z + (p.z - c.z) * t};
            if (other.classifyPoint(s) != cls) uniform = false;
        }
    }
    return uniform;
}

// Count faces of `body` by their classification vs `other`; returns false if any
// face straddles.
bool allFacesSegmented(const Body& body, const Body& other, int& inside, int& outside)
{
    inside = outside = 0;
    bool ok = true;
    for (uint32_t f = 0; f < body.faceCount(); ++f) {
        if (!body.face(f).alive) continue;
        PC c;
        if (!faceIsUniform(body, f, other, c)) { ok = false; continue; }
        if (c == PC::Inside) ++inside;
        else if (c == PC::Outside) ++outside;
    }
    return ok;
}
}  // namespace

TEST(BRepMutualImprint, CornerOverlapSegmentsBothBodies)
{
    Body a = makeBox(2.f, 2.f, 2.f);        // [-1,1]^3
    Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);  // [0,2]^3, corner overlap [0,1]^3

    imprintMutually(a, b);

    // Both bodies remain watertight, valid solids.
    for (const Body* body : {&a, &b}) {
        const auto ig = body->checkIntegrity();
        EXPECT_TRUE(ig.ok) << ig.reason;
        EXPECT_EQ(ig.euler, 2);
        EXPECT_EQ(ig.boundaryEdges, 0u);
        EXPECT_TRUE(body->checkGeometry().ok) << body->checkGeometry().reason;
    }

    // No face straddles; the overlap is non-empty so each body has ≥1 inside face.
    int inA = 0, outA = 0, inB = 0, outB = 0;
    EXPECT_TRUE(allFacesSegmented(a, b, inA, outA));
    EXPECT_TRUE(allFacesSegmented(b, a, inB, outB));
    EXPECT_GT(inA, 0);
    EXPECT_GT(inB, 0);
    EXPECT_GT(outA, 0);
    EXPECT_GT(outB, 0);
}

TEST(BRepMutualImprint, OffsetPartialOverlapSegments)
{
    Body a = makeBox(2.f, 2.f, 2.f);
    Body b = boxAt({1.f, 0.5f, 0.5f}, 2, 2, 2);  // partial overlap, no coincident faces

    imprintMutually(a, b);

    EXPECT_TRUE(a.checkIntegrity().ok) << a.checkIntegrity().reason;
    EXPECT_TRUE(b.checkIntegrity().ok) << b.checkIntegrity().reason;
    EXPECT_EQ(a.checkIntegrity().euler, 2);
    EXPECT_EQ(b.checkIntegrity().euler, 2);
    EXPECT_TRUE(a.checkGeometry().ok);
    EXPECT_TRUE(b.checkGeometry().ok);

    int inA = 0, outA = 0, inB = 0, outB = 0;
    EXPECT_TRUE(allFacesSegmented(a, b, inA, outA));
    EXPECT_TRUE(allFacesSegmented(b, a, inB, outB));
    EXPECT_GT(inA, 0);
    EXPECT_GT(inB, 0);
}

TEST(BRepMutualImprint, DisjointBodiesUnchanged)
{
    Body a = makeBox(2.f, 2.f, 2.f);
    Body b = boxAt({10.f, 0.f, 0.f}, 2, 2, 2);  // no overlap
    const size_t fa = a.faceCount(), fb = b.faceCount();
    imprintMutually(a, b);
    // Nothing to cut: face counts unchanged, both still valid.
    EXPECT_EQ(a.faceCount(), fa);
    EXPECT_EQ(b.faceCount(), fb);
    EXPECT_TRUE(a.checkIntegrity().ok);
    EXPECT_TRUE(b.checkIntegrity().ok);
}

TEST(BRepMutualImprint, Deterministic)
{
    Body a1 = makeBox(2.f, 2.f, 2.f), b1 = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);
    Body a2 = makeBox(2.f, 2.f, 2.f), b2 = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);
    imprintMutually(a1, b1);
    imprintMutually(a2, b2);
    EXPECT_EQ(a1.faceCount(), a2.faceCount());
    EXPECT_EQ(a1.vertexCount(), a2.vertexCount());
    EXPECT_EQ(b1.faceCount(), b2.faceCount());
    ASSERT_EQ(a1.vertexCount(), a2.vertexCount());
    for (uint32_t v = 0; v < a1.vertexCount(); ++v) {
        EXPECT_FLOAT_EQ(a1.vertex(v).point.x, a2.vertex(v).point.x);
        EXPECT_FLOAT_EQ(a1.vertex(v).point.y, a2.vertex(v).point.y);
        EXPECT_FLOAT_EQ(a1.vertex(v).point.z, a2.vertex(v).point.z);
    }
}

}  // namespace nexus::geometry::brep::testing
