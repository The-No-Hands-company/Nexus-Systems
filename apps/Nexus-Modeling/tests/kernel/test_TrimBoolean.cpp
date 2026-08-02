// Foundation — boolean operations on trimmed 2D regions.
//
// These tests used to assert only that the call RAN: `EXPECT_FALSE(result.empty())`,
// `EXPECT_GE(outerLoops.size(), 1u)`, and in one case `EXPECT_GE(size(), 0u)`, which is
// unsigned and therefore always true. One test finished with a comment and no assertion
// at all.
//
// None of that can distinguish a region from a bag of points, and it did not. Measured:
// EVERY non-empty result this function returned had an enclosed area of exactly ZERO. The
// old extractBoundary() scanned the mask row by row and pushed the first and last cell of
// each horizontal RUN into a "loop"; on the union of two 2x2 squares that gave the four
// correct corners in the order bottom-left, bottom-right, top-LEFT, top-right — a
// self-intersecting bowtie. It also returned a SINGLE loop, so a union of two disjoint
// squares could not be expressed at all.
//
// So the assertions here are areas, loop counts and windings: the properties that decide
// whether a region IS the region. Results are rasterised at gridRes, so an exact area is
// approached rather than met and the tolerance below is proportional — but the previous
// behaviour was wrong by 100%, not by a percent.

#include <gtest/gtest.h>

#include <nexus/geometry/TrimBoolean.h>

#include <cmath>
#include <cstddef>

namespace {

using namespace nexus::geometry;

TrimBooleanOptions highResOpts()
{
    TrimBooleanOptions opts;
    opts.gridRes = 512;
    return opts;
}

BooleanLoop makeSquareLoop(float x0, float y0, float size)
{
    BooleanLoop loop;
    loop.closed = true;
    loop.points = {
        Vec3{x0, y0, 0.f},
        Vec3{x0 + size, y0, 0.f},
        Vec3{x0 + size, y0 + size, 0.f},
        Vec3{x0, y0 + size, 0.f},
    };
    return loop;
}

BooleanRegion makeSquareRegion(float x, float y, float size)
{
    BooleanRegion region;
    region.outerLoops.push_back(makeSquareLoop(x, y, size));
    return region;
}

BooleanRegion makeSquareWithHole(float outerX, float outerY, float outerSize,
                                 float holeX, float holeY, float holeSize)
{
    BooleanRegion region;
    region.outerLoops.push_back(makeSquareLoop(outerX, outerY, outerSize));
    region.innerLoops.push_back(makeSquareLoop(holeX, holeY, holeSize));
    return region;
}

// Signed area by the shoelace formula: positive counter-clockwise. A self-intersecting
// "loop" of the kind the old extractor produced integrates to ~0, which is exactly what
// these tests exist to catch.
double signedArea(const BooleanLoop& l)
{
    double a = 0.0;
    const std::size_t n = l.points.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Vec3& p = l.points[i];
        const Vec3& q = l.points[(i + 1) % n];
        a += static_cast<double>(p.x) * q.y - static_cast<double>(q.x) * p.y;
    }
    return 0.5 * a;
}

// Material area: outer loops minus the holes.
double netArea(const BooleanRegion& r)
{
    double a = 0.0;
    for (const auto& l : r.outerLoops) a += std::abs(signedArea(l));
    for (const auto& l : r.innerLoops) a -= std::abs(signedArea(l));
    return a;
}

// Rasterisation places each boundary within half a cell of the true edge, so the area
// error scales with perimeter over grid resolution. 2% sits comfortably inside that.
constexpr double kAreaTol = 0.02;

void expectArea(const BooleanRegion& r, double expected, const char* what)
{
    EXPECT_NEAR(netArea(r), expected, expected * kAreaTol) << what;
}

}  // namespace

TEST(TrimBoolean, UnionOfIdenticalSquaresIsThatSquare)
{
    const auto r = TrimBoolean::compute(makeSquareRegion(0.f, 0.f, 1.f),
                                        makeSquareRegion(0.f, 0.f, 1.f),
                                        BooleanOp::Union, highResOpts());
    ASSERT_EQ(r.outerLoops.size(), 1u);
    EXPECT_EQ(r.innerLoops.size(), 0u);
    expectArea(r, 1.0, "A union A must be A");
}

TEST(TrimBoolean, IntersectionOfIdenticalSquaresIsThatSquare)
{
    const auto r = TrimBoolean::compute(makeSquareRegion(0.f, 0.f, 1.f),
                                        makeSquareRegion(0.f, 0.f, 1.f),
                                        BooleanOp::Intersection, highResOpts());
    ASSERT_EQ(r.outerLoops.size(), 1u);
    expectArea(r, 1.0, "A intersect A must be A");
}

TEST(TrimBoolean, DifferenceOfIdenticalSquaresIsEmpty)
{
    // The previous test hedged — "may be empty; that is a valid result" — and then
    // asserted a tautology. A regularised difference of a region with itself is empty.
    const auto r = TrimBoolean::compute(makeSquareRegion(0.f, 0.f, 1.f),
                                        makeSquareRegion(0.f, 0.f, 1.f),
                                        BooleanOp::Difference, highResOpts());
    EXPECT_TRUE(r.empty()) << "A minus A must be empty";
}

TEST(TrimBoolean, OverlappingSquaresHaveTheExactAreasOfTheirOps)
{
    // Two 2x2 squares offset by 1 in x: union 6, intersection 2, difference 2.
    const auto A = makeSquareRegion(0.f, 0.f, 2.f);
    const auto B = makeSquareRegion(1.f, 0.f, 2.f);
    expectArea(TrimBoolean::compute(A, B, BooleanOp::Union, highResOpts()), 6.0, "union");
    expectArea(TrimBoolean::compute(A, B, BooleanOp::Intersection, highResOpts()), 2.0,
               "intersection");
    expectArea(TrimBoolean::compute(A, B, BooleanOp::Difference, highResOpts()), 2.0,
               "difference");
}

TEST(TrimBoolean, DisjointUnionKeepsBothComponents)
{
    // THE structural regression guard. The old extractor returned exactly one loop, so a
    // result with two separate pieces had no way to be represented at all.
    const auto r = TrimBoolean::compute(makeSquareRegion(0.f, 0.f, 2.f),
                                        makeSquareRegion(5.f, 0.f, 2.f),
                                        BooleanOp::Union, highResOpts());
    EXPECT_EQ(r.outerLoops.size(), 2u) << "two disjoint squares are two loops";
    expectArea(r, 8.0, "disjoint union area");
}

TEST(TrimBoolean, DisjointIntersectionIsEmpty)
{
    EXPECT_TRUE(TrimBoolean::compute(makeSquareRegion(0.f, 0.f, 2.f),
                                     makeSquareRegion(5.f, 0.f, 2.f),
                                     BooleanOp::Intersection, highResOpts())
                    .empty());
}

TEST(TrimBoolean, SubtractingAnInteriorSquareLeavesARingWoundBothWays)
{
    // 4x4 minus a centred 2x2 is a ring of area 12, and the hole must wind AGAINST the
    // outer boundary — otherwise it bounds material instead of removing it.
    const auto r = TrimBoolean::compute(makeSquareRegion(0.f, 0.f, 4.f),
                                        makeSquareRegion(1.f, 1.f, 2.f),
                                        BooleanOp::Difference, highResOpts());
    ASSERT_EQ(r.outerLoops.size(), 1u);
    ASSERT_EQ(r.innerLoops.size(), 1u);
    EXPECT_GT(signedArea(r.outerLoops[0]), 0.0) << "outer boundary must be counter-clockwise";
    EXPECT_LT(signedArea(r.innerLoops[0]), 0.0) << "a hole must wind the other way";
    expectArea(r, 12.0, "4x4 minus a centred 2x2");
}

TEST(TrimBoolean, AHoleSurvivesUnionAndIntersectionWithItself)
{
    const auto ring = makeSquareWithHole(0.f, 0.f, 4.f, 1.f, 1.f, 2.f);
    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection}) {
        const auto r = TrimBoolean::compute(ring, ring, op, highResOpts());
        ASSERT_EQ(r.outerLoops.size(), 1u) << "op " << static_cast<int>(op);
        ASSERT_EQ(r.innerLoops.size(), 1u) << "the hole was lost, op " << static_cast<int>(op);
        expectArea(r, 12.0, "ring combined with itself is the ring");
    }
}

TEST(TrimBoolean, SubtractingASquareThatSwallowsTheHoleGivesTheRightRemainder)
{
    // This case previously ended with a trailing comment and no assertion.
    // A is [0,4]^2 minus [1,3]^2; B is [0.5,3.5]^2, which contains the hole entirely, so
    // A - B = [0,4]^2 \ [0.5,3.5]^2 = 16 - 9 = 7, and the result is again a ring.
    const auto r = TrimBoolean::compute(makeSquareWithHole(0.f, 0.f, 4.f, 1.f, 1.f, 2.f),
                                        makeSquareRegion(0.5f, 0.5f, 3.f),
                                        BooleanOp::Difference, highResOpts());
    ASSERT_EQ(r.outerLoops.size(), 1u);
    ASSERT_EQ(r.innerLoops.size(), 1u);
    expectArea(r, 7.0, "ring minus a square that covers its hole");
}

TEST(TrimBoolean, NoLoopEnclosesZeroArea)
{
    // The direct guard: a self-intersecting ring integrates to ~0, which is what every
    // non-empty result used to do while all the old assertions passed.
    const BooleanRegion cases[2] = {makeSquareRegion(0.f, 0.f, 2.f),
                                    makeSquareWithHole(0.f, 0.f, 4.f, 1.f, 1.f, 2.f)};
    for (const auto& A : cases) {
        for (BooleanOp op :
             {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            const auto r =
                TrimBoolean::compute(A, makeSquareRegion(1.f, 1.f, 2.f), op, highResOpts());
            for (const auto& l : r.outerLoops) {
                EXPECT_TRUE(l.closed);
                EXPECT_GE(l.points.size(), 3u);
                EXPECT_GT(std::abs(signedArea(l)), 1e-3) << "an outer loop encloses nothing";
            }
            for (const auto& l : r.innerLoops) {
                EXPECT_GT(std::abs(signedArea(l)), 1e-3) << "an inner loop encloses nothing";
            }
        }
    }
}

TEST(TrimBoolean, IsDeterministic)
{
    const auto run = [] {
        return TrimBoolean::compute(makeSquareWithHole(0.f, 0.f, 4.f, 1.f, 1.f, 2.f),
                                    makeSquareRegion(0.5f, 0.5f, 3.f), BooleanOp::Difference,
                                    highResOpts());
    };
    const auto a = run();
    const auto b = run();
    ASSERT_EQ(a.outerLoops.size(), b.outerLoops.size());
    ASSERT_EQ(a.innerLoops.size(), b.innerLoops.size());
    for (std::size_t i = 0; i < a.outerLoops.size(); ++i) {
        ASSERT_EQ(a.outerLoops[i].points.size(), b.outerLoops[i].points.size());
        for (std::size_t k = 0; k < a.outerLoops[i].points.size(); ++k) {
            EXPECT_EQ(a.outerLoops[i].points[k].x, b.outerLoops[i].points[k].x);
            EXPECT_EQ(a.outerLoops[i].points[k].y, b.outerLoops[i].points[k].y);
        }
    }
}
