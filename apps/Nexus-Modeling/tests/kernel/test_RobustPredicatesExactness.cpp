// The exact-predicate bedrock, guarded.
//
// orient2D/orient3D/inCircle decide which side of something a point lies on. Every
// Delaunay and CDT insertion, every boolean classification and every Simulation-of-
// Simplicity tie-break rests on them returning the CORRECT SIGN — not a close estimate.
// A single wrong sign silently corrupts the structure built on top, in a way no
// downstream validator can repair: a Delaunay cavity is computed from the wrong triangle
// set, and the resulting triangulation under-fills its own convex hull.
//
// This is not hypothetical. Before these were rewritten on genuine expansion arithmetic,
// all three returned wrong signs on near-degenerate input — measured against an exact
// reference at roughly 0.4%, 0.5% and 0.4% of sliver configurations respectively — because
// the "exact" fallbacks naively summed six floating-point terms of wildly different
// magnitude and gave up under a hard-coded 1e-10 threshold.
//
// The reference here is exact and PORTABLE: integer-valued coordinates are represented
// exactly by float, and with the ranges bounded below, the whole determinant is computed
// exactly in int64_t. No wide float type is required.
//
//   orient2D  coords <= 2^20 : diffs <= 2^21, products <= 2^42, det <= 2^43
//   orient3D  coords <= 2^15 : diffs <= 2^16, minors <= 2^33, det <= 2^51
//   inCircle  coords <= 2^10 : diffs <= 2^11, lifts <= 2^23, det <= 2^48
//
// all far inside int64_t's 2^63. Generators deliberately produce NEAR-DEGENERATE input,
// so the floating-point fast path fails its error bound and the exact path is the thing
// actually under test.

#include <nexus/geometry/RobustPredicates.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <random>

using namespace nexus::geometry;
using nexus::render::Vec3;

namespace {

int sign(double d) noexcept { return d > 0.0 ? 1 : (d < 0.0 ? -1 : 0); }
int sign(std::int64_t d) noexcept { return d > 0 ? 1 : (d < 0 ? -1 : 0); }

std::int64_t iv(float f) { return static_cast<std::int64_t>(f); }

std::int64_t exactOrient2D(const Vec2& a, const Vec2& b, const Vec2& c)
{
    return (iv(a.u) - iv(c.u)) * (iv(b.v) - iv(c.v))
         - (iv(a.v) - iv(c.v)) * (iv(b.u) - iv(c.u));
}

std::int64_t exactOrient3D(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d)
{
    const std::int64_t adx = iv(a.x) - iv(d.x), ady = iv(a.y) - iv(d.y), adz = iv(a.z) - iv(d.z);
    const std::int64_t bdx = iv(b.x) - iv(d.x), bdy = iv(b.y) - iv(d.y), bdz = iv(b.z) - iv(d.z);
    const std::int64_t cdx = iv(c.x) - iv(d.x), cdy = iv(c.y) - iv(d.y), cdz = iv(c.z) - iv(d.z);
    return adx * (bdy * cdz - bdz * cdy)
         + ady * (bdz * cdx - bdx * cdz)
         + adz * (bdx * cdy - bdy * cdx);
}

std::int64_t exactInCircle(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d)
{
    const std::int64_t adx = iv(a.u) - iv(d.u), ady = iv(a.v) - iv(d.v);
    const std::int64_t bdx = iv(b.u) - iv(d.u), bdy = iv(b.v) - iv(d.v);
    const std::int64_t cdx = iv(c.u) - iv(d.u), cdy = iv(c.v) - iv(d.v);
    return (adx * adx + ady * ady) * (bdx * cdy - bdy * cdx)
         + (bdx * bdx + bdy * bdy) * (cdx * ady - cdy * adx)
         + (cdx * cdx + cdy * cdy) * (adx * bdy - ady * bdx);
}

// Integer lattice points hugging the line y = x/2 — the sliver regime, where the
// determinant is tiny relative to the coordinates and the fast path must give way.
Vec2 sliverPoint(std::mt19937& rng, std::int64_t span, std::int64_t wobble)
{
    std::uniform_int_distribution<std::int64_t> along(-span, span);
    std::uniform_int_distribution<std::int64_t> off(-wobble, wobble);
    const std::int64_t t = along(rng);
    return {static_cast<float>(t + off(rng)), static_cast<float>(t / 2 + off(rng))};
}

}  // namespace

TEST(RobustPredicatesExactness, Orient2DSignMatchesExactIntegerDeterminant)
{
    std::mt19937 rng(20260722u);
    int degenerate = 0;
    for (int i = 0; i < 200000; ++i) {
        const Vec2 a = sliverPoint(rng, 1 << 20, 2);
        const Vec2 b = sliverPoint(rng, 1 << 20, 2);
        const Vec2 c = sliverPoint(rng, 1 << 20, 2);
        const std::int64_t want = exactOrient2D(a, b, c);
        ASSERT_EQ(sign(RobustPredicates::orient2D(a, b, c)), sign(want))
            << "wrong orientation sign at i=" << i;
        if (want == 0) ++degenerate;
    }
    EXPECT_GT(degenerate, 0) << "the battery never produced an exactly-collinear triple";
}

TEST(RobustPredicatesExactness, Orient3DSignMatchesExactIntegerDeterminant)
{
    std::mt19937 rng(1234u);
    std::uniform_int_distribution<std::int64_t> along(-(1 << 15), 1 << 15);
    std::uniform_int_distribution<std::int64_t> off(-2, 2);
    int degenerate = 0;
    for (int i = 0; i < 200000; ++i) {
        Vec3 p[4];
        for (auto& q : p) {
            const std::int64_t t = along(rng);
            q = {static_cast<float>(t + off(rng)), static_cast<float>(t / 2 + off(rng)),
                 static_cast<float>(t / 4 + off(rng))};
        }
        const std::int64_t want = exactOrient3D(p[0], p[1], p[2], p[3]);
        ASSERT_EQ(sign(RobustPredicates::orient3D(p[0], p[1], p[2], p[3])), sign(want))
            << "wrong orientation sign at i=" << i;
        if (want == 0) ++degenerate;
    }
    EXPECT_GT(degenerate, 0) << "the battery never produced an exactly-coplanar quadruple";
}

TEST(RobustPredicatesExactness, InCircleSignMatchesExactIntegerDeterminant)
{
    std::mt19937 rng(999u);
    int degenerate = 0;
    for (int i = 0; i < 200000; ++i) {
        const Vec2 a = sliverPoint(rng, 1 << 10, 2);
        const Vec2 b = sliverPoint(rng, 1 << 10, 2);
        const Vec2 c = sliverPoint(rng, 1 << 10, 2);
        const Vec2 d = sliverPoint(rng, 1 << 10, 2);
        const std::int64_t want = exactInCircle(a, b, c, d);
        ASSERT_EQ(sign(RobustPredicates::inCircle(a, b, c, d)), sign(want))
            << "wrong in-circle sign at i=" << i;
        if (want == 0) ++degenerate;
    }
    EXPECT_GT(degenerate, 0) << "the battery never produced an exactly-cocircular quadruple";
}

// A predicate that merely gets close is not enough: an exactly degenerate configuration
// must report exactly zero, or Simulation-of-Simplicity has no tie to break.
TEST(RobustPredicatesExactness, ExactDegeneraciesReportExactlyZero)
{
    EXPECT_EQ(RobustPredicates::orient2D({0.f, 0.f}, {1.f, 0.5f}, {2.f, 1.f}), 0.0);
    EXPECT_EQ(RobustPredicates::orient2D({0.f, 0.f}, {1e-6f, 5e-7f}, {2e-6f, 1e-6f}), 0.0);

    EXPECT_EQ(RobustPredicates::orient3D({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f},
                                         {1.f, 1.f, 0.f}), 0.0);

    // The unit square's corners are exactly cocircular.
    EXPECT_EQ(RobustPredicates::inCircle({0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}), 0.0);
}

// One ULP off a degeneracy must be resolved in the right direction — the resolution that
// an estimate-with-a-threshold cannot deliver. Perturbations are taken off a coordinate of
// magnitude 1, never off 0: one ULP from zero is a DENORMAL, and expansion arithmetic
// (which splits operands by multiplying by 2^27+1) is only defined in the absence of
// underflow. That is a documented limit of the technique, not a defect to assert against.
TEST(RobustPredicatesExactness, OneUlpOffDegeneracyResolvesCorrectly)
{
    // a=(0,0), b=(1,0.5): the line y = x/2, on which c=(2,1) lies exactly.
    // orient2D > 0 means c is left of a->b, i.e. ABOVE this line.
    const Vec2 a{0.f, 0.f}, b{1.f, 0.5f};
    const float onLine = 1.f;
    EXPECT_EQ(RobustPredicates::orient2D(a, b, {2.f, onLine}), 0.0);
    EXPECT_GT(RobustPredicates::orient2D(a, b, {2.f, std::nextafter(onLine, 2.f)}), 0.0)
        << "a point one ULP above the line did not read as above it";
    EXPECT_LT(RobustPredicates::orient2D(a, b, {2.f, std::nextafter(onLine, 0.f)}), 0.0)
        << "a point one ULP below the line did not read as below it";

    // The unit square's corners are exactly cocircular; a,b,c are counter-clockwise, so
    // inCircle > 0 means d is INSIDE. Pulling d's y one ULP toward the centre puts it
    // inside; pushing it one ULP away puts it outside.
    const Vec2 p0{0.f, 0.f}, p1{1.f, 0.f}, p2{1.f, 1.f};
    ASSERT_GT(RobustPredicates::orient2D(p0, p1, p2), 0.0) << "fixture is not CCW";
    EXPECT_EQ(RobustPredicates::inCircle(p0, p1, p2, {0.f, 1.f}), 0.0);
    EXPECT_GT(RobustPredicates::inCircle(p0, p1, p2, {0.f, std::nextafter(1.f, 0.f)}), 0.0)
        << "a point one ULP inside the circle did not read as inside";
    EXPECT_LT(RobustPredicates::inCircle(p0, p1, p2, {0.f, std::nextafter(1.f, 2.f)}), 0.0)
        << "a point one ULP outside the circle did not read as outside";
}

TEST(RobustPredicatesExactness, PredicatesAreDeterministic)
{
    std::mt19937 rng(31337u);
    for (int i = 0; i < 20000; ++i) {
        const Vec2 a = sliverPoint(rng, 1 << 10, 2);
        const Vec2 b = sliverPoint(rng, 1 << 10, 2);
        const Vec2 c = sliverPoint(rng, 1 << 10, 2);
        const Vec2 d = sliverPoint(rng, 1 << 10, 2);
        ASSERT_EQ(RobustPredicates::orient2D(a, b, c), RobustPredicates::orient2D(a, b, c));
        ASSERT_EQ(RobustPredicates::inCircle(a, b, c, d), RobustPredicates::inCircle(a, b, c, d));
    }
}
