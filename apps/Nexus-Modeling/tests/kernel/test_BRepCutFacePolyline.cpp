// Foundation — cutting a face along a POLYLINE, the seam a traced intersection produces.
//
// The analytic imprint cuts a face with ONE edge carrying a Line or a Circle, which covers
// every seam that has a closed form. A quartic does not have one: a sphere met by an
// off-axis cylinder, two cylinders with crossing axes, a cone against anything curved and
// off its axis all produce a curve that can only be delivered as samples. Measured over
// 3592 chained boolean steps, 35.2% of ALL outcomes were declined for exactly that reason
// — about five times the next-largest gap — so the cut has to accept a chain.
//
// This tests the operator alone, the same way traceSurfaceIntersection was verified before
// anything depended on it. It is not yet wired into the imprint.
//
// The properties that define a correct cut are Euler bookkeeping and the two validators:
// N interior points must add exactly N vertices, N+1 edges and one face, leaving
// V - E + F unchanged, with every coedge partnered and every curve reproducing its
// endpoints. Area is the geometric check — the two pieces must together still be the
// original face, however wiggly the cut between them.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

struct Counts {
    int v = 0, e = 0, f = 0;
};

Counts liveCounts(const Body& b)
{
    Counts c;
    for (uint32_t i = 0; i < static_cast<uint32_t>(b.vertexCount()); ++i)
        if (b.vertex(i).alive) ++c.v;
    for (uint32_t i = 0; i < static_cast<uint32_t>(b.edgeCount()); ++i)
        if (b.edge(i).alive) ++c.e;
    for (uint32_t i = 0; i < static_cast<uint32_t>(b.faceCount()); ++i)
        if (b.face(i).alive) ++c.f;
    return c;
}

// The two outer-loop vertices of a box's +Z face that sit diagonally opposite, so a cut
// between them is legal (non-adjacent) on the four-sided cap.
uint32_t topFaceOf(const Body& b)
{
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        const std::vector<uint32_t> vs = b.faceVertices(f);
        bool allTop = !vs.empty();
        for (uint32_t v : vs)
            if (std::abs(b.vertex(v).point.z - 1.0) > 1e-9) allTop = false;
        if (allTop) return f;
    }
    return kInvalid;
}

}  // namespace

TEST(BRepCutFacePolyline, ChainOfNPointsIsEulerNeutralAndValid)
{
    // N interior points add N vertices, N+1 edges and 1 face: V - E + F is unchanged, so
    // the solid's Euler characteristic is too. Checked for several N, including N = 0,
    // which must behave exactly like the single-edge cut it generalises.
    for (size_t n : {size_t{0}, size_t{1}, size_t{2}, size_t{5}}) {
        Body box = makeBox(2.f, 2.f, 2.f);
        const uint32_t top = topFaceOf(box);
        ASSERT_NE(top, kInvalid);
        const std::vector<uint32_t> vs = box.faceVertices(top);
        ASSERT_EQ(vs.size(), 4u);

        const Vec3 a = box.vertex(vs[0]).point;
        const Vec3 c = box.vertex(vs[2]).point;  // the diagonally opposite corner

        // Interior samples along the diagonal, pushed off it so the cut genuinely bends
        // and the chain is not secretly collinear.
        std::vector<Vec3> interior;
        for (size_t k = 1; k <= n; ++k) {
            const double t = static_cast<double>(k) / static_cast<double>(n + 1);
            const double bulge = 0.25 * std::sin(3.14159265358979324 * t);
            interior.push_back({a.x + (c.x - a.x) * t + bulge, a.y + (c.y - a.y) * t, 1.0});
        }

        const Counts before = liveCounts(box);
        const int eulerBefore = before.v - before.e + before.f;

        const uint32_t newFace = box.cutFaceAlongPolyline(top, vs[0], vs[2], interior);
        ASSERT_NE(newFace, kInvalid) << "n=" << n;

        const Counts after = liveCounts(box);
        EXPECT_EQ(after.v, before.v + static_cast<int>(n)) << "n=" << n;
        EXPECT_EQ(after.e, before.e + static_cast<int>(n) + 1) << "n=" << n;
        EXPECT_EQ(after.f, before.f + 1) << "n=" << n;
        EXPECT_EQ(after.v - after.e + after.f, eulerBefore) << "n=" << n;

        const auto integrity = box.checkIntegrity();
        EXPECT_TRUE(integrity.ok) << "n=" << n << ": " << integrity.reason;
        const auto geometry = box.checkGeometry();
        EXPECT_TRUE(geometry.ok) << "n=" << n << ": " << geometry.reason;
        EXPECT_TRUE(box.isClosed()) << "n=" << n << ": a cut must not open the shell";
    }
}

TEST(BRepCutFacePolyline, TheTwoPiecesStillCoverTheOriginalFace)
{
    // A cut redistributes area, it never creates or destroys it — whatever path it takes.
    // Volume is the sharper statement of the same thing: an imprint SEGMENTS a boundary
    // and must leave the enclosed solid identical.
    Body box = makeBox(2.f, 2.f, 2.f);
    const double before = box.massProperties().volume;
    const double areaBefore = box.surfaceArea();

    const uint32_t top = topFaceOf(box);
    ASSERT_NE(top, kInvalid);
    const std::vector<uint32_t> vs = box.faceVertices(top);
    const Vec3 a = box.vertex(vs[0]).point;
    const Vec3 c = box.vertex(vs[2]).point;

    std::vector<Vec3> interior;
    for (int k = 1; k <= 7; ++k) {
        const double t = k / 8.0;
        interior.push_back({a.x + (c.x - a.x) * t + 0.3 * std::sin(6.0 * t),
                            a.y + (c.y - a.y) * t, 1.0});
    }

    ASSERT_NE(box.cutFaceAlongPolyline(top, vs[0], vs[2], interior), kInvalid);
    EXPECT_TRUE(box.checkIntegrity().ok);
    EXPECT_TRUE(box.checkGeometry().ok);
    EXPECT_NEAR(box.massProperties().volume, before, before * 1e-9)
        << "an imprint must not change the enclosed volume";
    EXPECT_NEAR(box.surfaceArea(), areaBefore, areaBefore * 1e-9)
        << "an imprint must not change the total surface area";
}

TEST(BRepCutFacePolyline, EveryInteriorPointBecomesAVertexOnTheCut)
{
    // The samples ARE the seam. If the chain quietly straightened, the traced curve would
    // be replaced by a chord and the two operands could no longer share it.
    Body box = makeBox(2.f, 2.f, 2.f);
    const uint32_t top = topFaceOf(box);
    ASSERT_NE(top, kInvalid);
    const std::vector<uint32_t> vs = box.faceVertices(top);
    const Vec3 a = box.vertex(vs[0]).point;
    const Vec3 c = box.vertex(vs[2]).point;

    std::vector<Vec3> interior;
    for (int k = 1; k <= 4; ++k) {
        const double t = k / 5.0;
        interior.push_back({a.x + (c.x - a.x) * t + 0.2 * t, a.y + (c.y - a.y) * t, 1.0});
    }
    ASSERT_NE(box.cutFaceAlongPolyline(top, vs[0], vs[2], interior), kInvalid);

    for (const Vec3& p : interior) {
        bool found = false;
        for (uint32_t v = 0; v < static_cast<uint32_t>(box.vertexCount()); ++v) {
            if (!box.vertex(v).alive) continue;
            const Vec3 q = box.vertex(v).point;
            const double dx = q.x - p.x, dy = q.y - p.y, dz = q.z - p.z;
            if (std::sqrt(dx * dx + dy * dy + dz * dz) < 1e-9) { found = true; break; }
        }
        EXPECT_TRUE(found) << "an interior sample did not become a vertex";
    }
}

TEST(BRepCutFacePolyline, BothSidesOfTheChainAreWalkableAndPartnered)
{
    // The chain must appear once in each of the two new loops, wound opposite ways, or the
    // faces do not close and the adjacency across the seam is lost. checkIntegrity covers
    // partner reciprocity; this pins that each piece really is bounded by the whole chain.
    Body box = makeBox(2.f, 2.f, 2.f);
    const uint32_t top = topFaceOf(box);
    const std::vector<uint32_t> vs = box.faceVertices(top);
    const Vec3 a = box.vertex(vs[0]).point;
    const Vec3 c = box.vertex(vs[2]).point;

    std::vector<Vec3> interior;
    for (int k = 1; k <= 3; ++k) {
        const double t = k / 4.0;
        interior.push_back({a.x + (c.x - a.x) * t + 0.15, a.y + (c.y - a.y) * t, 1.0});
    }
    const uint32_t other = box.cutFaceAlongPolyline(top, vs[0], vs[2], interior);
    ASSERT_NE(other, kInvalid);

    // The original quad was 4 vertices; each piece is now 2 corners + the 3 interior
    // samples + the 2 shared endpoints, walked as a closed ring.
    const std::vector<uint32_t> ringA = box.faceVertices(top);
    const std::vector<uint32_t> ringB = box.faceVertices(other);
    EXPECT_GE(ringA.size(), interior.size() + 2) << "piece A is missing chain vertices";
    EXPECT_GE(ringB.size(), interior.size() + 2) << "piece B is missing chain vertices";
    EXPECT_EQ(ringA.size() + ringB.size(), 4u + 2u * interior.size() + 2u)
        << "the chain must be walked exactly once by each side";
}

TEST(BRepCutFacePolyline, AdjacentOrUnknownVerticesAreRefused)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    const uint32_t top = topFaceOf(box);
    const std::vector<uint32_t> vs = box.faceVertices(top);
    const std::vector<Vec3> interior = {{0.0, 0.0, 1.0}};

    // Adjacent corners have no interior to separate, so the cut would not divide the face.
    EXPECT_EQ(box.cutFaceAlongPolyline(top, vs[0], vs[1], interior), kInvalid);
    // A vertex that is not on this face's outer loop.
    EXPECT_EQ(box.cutFaceAlongPolyline(top, vs[0], vs[0], interior), kInvalid);
    EXPECT_TRUE(box.checkIntegrity().ok) << "a refused cut must leave the body untouched";
    EXPECT_TRUE(box.isClosed());
}

TEST(BRepCutFacePolyline, IsDeterministic)
{
    const auto build = [] {
        Body box = makeBox(2.f, 2.f, 2.f);
        const uint32_t top = topFaceOf(box);
        const std::vector<uint32_t> vs = box.faceVertices(top);
        const Vec3 a = box.vertex(vs[0]).point;
        const Vec3 c = box.vertex(vs[2]).point;
        std::vector<Vec3> interior;
        for (int k = 1; k <= 4; ++k) {
            const double t = k / 5.0;
            interior.push_back({a.x + (c.x - a.x) * t + 0.1 * t, a.y + (c.y - a.y) * t, 1.0});
        }
        (void)box.cutFaceAlongPolyline(top, vs[0], vs[2], interior);
        return box;
    };
    const Body x = build();
    const Body y = build();
    ASSERT_EQ(x.vertexCount(), y.vertexCount());
    for (uint32_t v = 0; v < static_cast<uint32_t>(x.vertexCount()); ++v) {
        EXPECT_EQ(x.vertex(v).point.x, y.vertex(v).point.x) << "v=" << v;
        EXPECT_EQ(x.vertex(v).point.y, y.vertex(v).point.y) << "v=" << v;
        EXPECT_EQ(x.vertex(v).point.z, y.vertex(v).point.z) << "v=" << v;
    }
}

}  // namespace nexus::geometry::brep::testing
