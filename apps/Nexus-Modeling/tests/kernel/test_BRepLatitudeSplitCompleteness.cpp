// Phase 4d of the true-analytic-curved-boolean arc — SPLIT COMPLETENESS.
//
// A cylinder driven through a box is crossed by TWO planes, so each of its side faces
// must be cut into three pieces. Each was being cut exactly once. Measured on
// box(2,2,2) against cylinder(r=0.5,h=4,16 segments): the 16 side faces became 32 in
// four groups of eight — z∈[-2,-1], z∈[-2,+1], z∈[-1,+2], z∈[+1,+2] — so sixteen of
// them still straddled a plane, and a face that straddles is kept or dropped whole.
//
// The cause is that the second cut lands on vertices that already exist. Once one face
// of the cylinder has been cut at a latitude, its neighbour's upright edges are already
// split there, so for every remaining face the level meets each upright edge exactly AT
// an endpoint rather than inside it. The cylinder's crossing solver required a fraction
// strictly inside the edge, reported no crossings at all, and the face fell through to
// the interior-hole case, which refused it. The planar path had always accepted an
// endpoint fraction and snapped it onto the existing vertex; the cylinder path now does
// the same, so a crossing through an existing vertex reuses that vertex instead of
// being invisible.

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <cmath>
#include <map>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {

constexpr float kR = 0.5f;
constexpr uint32_t kSeg = 16;

struct Band { float lo, hi; };

// z-extent of every cylindrical face, bucketed to a hundredth.
std::map<std::pair<int, int>, int> cylindricalBands(const Body& b)
{
    std::map<std::pair<int, int>, int> bands;
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        const uint32_t s = b.face(f).surface;
        if (s >= b.surfaceCount() || b.surface(s).kind != SurfaceKind::Cylinder) continue;
        float lo = 1e30f, hi = -1e30f;
        for (uint32_t v : b.faceVertices(f)) {
            lo = std::min(lo, b.vertex(v).point.z);
            hi = std::max(hi, b.vertex(v).point.z);
        }
        bands[{static_cast<int>(std::lround(lo * 100)), static_cast<int>(std::lround(hi * 100))}]++;
    }
    return bands;
}

int ringSize(const Body& b, float z)
{
    int n = 0;
    for (uint32_t v = 0; v < static_cast<uint32_t>(b.vertexCount()); ++v) {
        if (!b.vertex(v).alive) continue;
        const Vec3 p = b.vertex(v).point;
        if (std::abs(p.z - z) > 1e-4f) continue;
        if (std::abs(std::sqrt(p.x * p.x + p.y * p.y) - kR) > 1e-4f) continue;
        ++n;
    }
    return n;
}

}  // namespace

// THE Phase 4d assertion: no face survives straddling a plane that crosses it.
TEST(BRepLatitudeSplitCompleteness, NoCylinderFaceStraddlesACuttingPlane)
{
    Body box = makeBox(2.f, 2.f, 2.f);          // z ∈ [-1, 1]
    Body cyl = makeCylinder(kR, 4.f, kSeg);     // z ∈ [-2, 2]
    ASSERT_TRUE(imprintMutually(box, cyl));

    for (const auto& [extent, count] : cylindricalBands(cyl)) {
        const float lo = extent.first / 100.f, hi = extent.second / 100.f;
        for (float plane : {-1.f, 1.f})
            EXPECT_FALSE(lo < plane - 1e-3f && hi > plane + 1e-3f)
                << count << " cylindrical face(s) span z ∈ [" << lo << ", " << hi
                << "] and straddle the plane z = " << plane
                << " — a straddling face is kept or dropped whole";
    }
}

// The split is not merely non-straddling but exactly the expected partition: three
// bands of sixteen, giving 16*3 + 2 caps = 50 faces.
TEST(BRepLatitudeSplitCompleteness, EverySideFaceIsCutIntoThreeBands)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(kR, 4.f, kSeg);
    ASSERT_TRUE(imprintMutually(box, cyl));

    const std::map<std::pair<int, int>, int> bands = cylindricalBands(cyl);
    ASSERT_EQ(bands.size(), 3u) << "expected exactly three z-bands of cylindrical faces";
    for (const auto& [extent, count] : bands)
        EXPECT_EQ(count, static_cast<int>(kSeg))
            << "band z ∈ [" << extent.first / 100.f << ", " << extent.second / 100.f << "]";

    EXPECT_EQ(cyl.faceCount(), kSeg * 3u + 2u) << "16 side bands x 3 plus two caps";
}

// The second cut REUSES the vertices the first cut created rather than manufacturing
// near-duplicates beside them — which is what accepting an endpoint fraction buys.
TEST(BRepLatitudeSplitCompleteness, SecondCutReusesTheExistingRingVertices)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(kR, 4.f, kSeg);
    const size_t before = cyl.vertexCount();
    ASSERT_TRUE(imprintMutually(box, cyl));

    // Two latitude rings of kSeg vertices each, and not one vertex more: a duplicated
    // ring would show up as 32 at a level, or as a total above before + 2*kSeg.
    EXPECT_EQ(ringSize(cyl, -1.f), static_cast<int>(kSeg));
    EXPECT_EQ(ringSize(cyl, 1.f), static_cast<int>(kSeg));
    EXPECT_EQ(cyl.vertexCount(), before + 2u * kSeg)
        << "the second cut manufactured vertices instead of reusing the ring";
}

// Splitting completely must not cost validity, and the cylinder stays closed.
TEST(BRepLatitudeSplitCompleteness, FullySplitOperandsRemainValid)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(kR, 4.f, kSeg);
    ASSERT_TRUE(imprintMutually(box, cyl));

    EXPECT_TRUE(cyl.checkIntegrity().ok) << cyl.checkIntegrity().reason;
    EXPECT_TRUE(cyl.checkGeometry().ok) << cyl.checkGeometry().reason;
    EXPECT_TRUE(cyl.isClosed()) << "cutting a closed cylinder's faces must leave it closed";
    EXPECT_TRUE(box.checkIntegrity().ok) << box.checkIntegrity().reason;
    EXPECT_TRUE(box.checkGeometry().ok) << box.checkGeometry().reason;
}

// A deeper stack of cutting planes still splits completely — three boxes give three
// distinct latitudes, so the reuse path is exercised repeatedly rather than once.
TEST(BRepLatitudeSplitCompleteness, RepeatedLatitudesAllSplit)
{
    Body cyl = makeCylinder(kR, 6.f, kSeg);  // z ∈ [-3, 3]
    for (float zc : {-1.5f, 0.f, 1.5f}) {
        Body slab = makeBox(2.f, 2.f, 1.f);  // half-height 0.5
        slab.translate({0.f, 0.f, zc});
        ASSERT_TRUE(imprintMutually(slab, cyl));
    }
    // Six cutting planes at z = -2,-1,-0.5,0.5,1,2 → seven bands.
    const std::map<std::pair<int, int>, int> bands = cylindricalBands(cyl);
    EXPECT_GE(bands.size(), 6u) << "repeated latitudes did not all cut";
    for (const auto& [extent, count] : bands) {
        const float lo = extent.first / 100.f, hi = extent.second / 100.f;
        for (float plane : {-2.f, -1.f, -0.5f, 0.5f, 1.f, 2.f})
            EXPECT_FALSE(lo < plane - 1e-3f && hi > plane + 1e-3f)
                << count << " face(s) span [" << lo << ", " << hi << "] across z = " << plane;
    }
    EXPECT_TRUE(cyl.checkIntegrity().ok) << cyl.checkIntegrity().reason;
    EXPECT_TRUE(cyl.checkGeometry().ok) << cyl.checkGeometry().reason;
    EXPECT_TRUE(cyl.isClosed());
}

}  // namespace nexus::geometry::brep::testing
