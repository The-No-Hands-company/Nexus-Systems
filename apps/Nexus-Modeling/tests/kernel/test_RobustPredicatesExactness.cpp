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

// Exact 4x4 lift determinant in int64. For coords in [-64, 64] the differences are <= 2^7
// and the determinant magnitude stays well under 2^63, so int64 is exact.
std::int64_t exactInSphere(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
                           const Vec3& e)
{
    const std::int64_t aex = iv(a.x) - iv(e.x), aey = iv(a.y) - iv(e.y), aez = iv(a.z) - iv(e.z);
    const std::int64_t bex = iv(b.x) - iv(e.x), bey = iv(b.y) - iv(e.y), bez = iv(b.z) - iv(e.z);
    const std::int64_t cex = iv(c.x) - iv(e.x), cey = iv(c.y) - iv(e.y), cez = iv(c.z) - iv(e.z);
    const std::int64_t dex = iv(d.x) - iv(e.x), dey = iv(d.y) - iv(e.y), dez = iv(d.z) - iv(e.z);

    const std::int64_t ab = aex * bey - bex * aey;
    const std::int64_t bc = bex * cey - cex * bey;
    const std::int64_t cd = cex * dey - dex * cey;
    const std::int64_t da = dex * aey - aex * dey;
    const std::int64_t ac = aex * cey - cex * aey;
    const std::int64_t bd = bex * dey - dex * bey;

    const std::int64_t abc = aez * bc - bez * ac + cez * ab;
    const std::int64_t bcd = bez * cd - cez * bd + dez * bc;
    const std::int64_t cda = cez * da + dez * ac + aez * cd;
    const std::int64_t dab = dez * ab + aez * bd + bez * da;

    const std::int64_t alift = aex * aex + aey * aey + aez * aez;
    const std::int64_t blift = bex * bex + bey * bey + bez * bez;
    const std::int64_t clift = cex * cex + cey * cey + cez * cez;
    const std::int64_t dlift = dex * dex + dey * dey + dez * dez;

    return (dlift * abc - clift * dab) + (blift * cda - alift * bcd);
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

TEST(RobustPredicatesExactness, InSphereSignMatchesExactIntegerDeterminant)
{
    std::mt19937 rng(24680);
    std::uniform_int_distribution<int> coord(-64, 64);
    auto pt = [&] { return Vec3{float(coord(rng)), float(coord(rng)), float(coord(rng))}; };

    int checked = 0;
    for (int i = 0; i < 200000; ++i) {
        Vec3 a = pt(), b = pt(), c = pt(), d = pt(), e = pt();
        // Skip flat tetrahedra: the in-sphere sign is only meaningful for a real sphere.
        if (exactOrient3D(a, b, c, d) == 0) continue;
        ++checked;
        const std::int64_t want = exactInSphere(a, b, c, d, e);
        ASSERT_EQ(sign(RobustPredicates::inSphere(a, b, c, d, e)), sign(want))
            << "mismatch at iteration " << i;
    }
    EXPECT_GT(checked, 1000);
    // The exactly-cospherical (== 0) case is exercised separately by the known-geometry and
    // cube-corner tests below; random coordinates are effectively never cospherical.
}

// The circumsphere of the unit corner tetrahedron is centred at (0.5,0.5,0.5), radius^2 3/4.
// Combined with orientation, e is inside iff inSphere(a,b,c,d,e) and orient3D(a,b,c,d) share
// a sign; exactly on the sphere gives zero.
TEST(RobustPredicatesExactness, InSphereConventionMatchesKnownGeometry)
{
    const Vec3 a{0, 0, 0}, b{1, 0, 0}, c{0, 1, 0}, d{0, 0, 1};
    const double o = RobustPredicates::orient3D(a, b, c, d);
    ASSERT_NE(o, 0.0);

    // (0.4,0.4,0.4): distance^2 to centre = 0.03 < 0.75 -> strictly inside.
    EXPECT_GT(RobustPredicates::inSphere(a, b, c, d, {0.4f, 0.4f, 0.4f}) * o, 0.0);
    // (2,2,2): far outside.
    EXPECT_LT(RobustPredicates::inSphere(a, b, c, d, {2.f, 2.f, 2.f}) * o, 0.0);
    // (1,1,1): distance^2 = 0.75 -> exactly on the sphere.
    EXPECT_EQ(RobustPredicates::inSphere(a, b, c, d, {1.f, 1.f, 1.f}), 0.0);
    // The four defining points are on the sphere by construction.
    EXPECT_EQ(RobustPredicates::inSphere(a, b, c, d, a), 0.0);
}

// The eight unit-cube corners are cospherical, so any four spanning a real tet report the
// other corners as exactly on the sphere.
TEST(RobustPredicatesExactness, CubeCornersAreExactlyCospherical)
{
    const Vec3 a{0, 0, 0}, b{1, 0, 0}, c{0, 1, 0}, d{0, 0, 1};
    for (const Vec3& e : {Vec3{1, 1, 0}, Vec3{1, 0, 1}, Vec3{0, 1, 1}, Vec3{1, 1, 1}})
        EXPECT_EQ(RobustPredicates::inSphere(a, b, c, d, e), 0.0)
            << "corner (" << e.x << "," << e.y << "," << e.z << ") should be cospherical";
}

// ─────────────────────────────────────────────────────────────────────────────
//  EXACTNESS BEYOND WHAT A DOUBLE CAN HOLD
//
//  Everything above bounds coordinates so the int64 reference stays exact, and those
//  bounds are tight: orient3D coords <= 2^15 give a determinant <= 2^51. That ceiling is
//  BELOW a double's 53-bit mantissa — so in that range plain double arithmetic is also
//  exact, and a predicate whose expansion arithmetic had been silently disabled would
//  pass every one of those batteries. It did. The build enabled -ffast-math, which folds
//  each error-free transformation's error term (an algebraically-zero expression) to a
//  literal zero, and the predicates degraded to double without a single test noticing:
//
//      -ffast-math      6 WRONG SIGNS in 5,675 cases, all on exactly-coplanar input,
//                       where orient3D confidently returned +/-512, +/-1024, ... not 0,
//                       so Simulation-of-Simplicity never got a tie to break
//      IEEE semantics   0 wrong signs in 400,000 cases
//
//  The bound chosen to keep the REFERENCE exact was the same bound that made the
//  SUBJECT's inexactness invisible. These batteries fix that: coordinates are raised
//  until the intermediates exceed 2^53 by a wide margin — while staying integer-valued,
//  hence exact in float — and the reference moves to __int128. A double cannot get these
//  right, so only genuine expansion arithmetic passes. This is the canary for the
//  floating-point policy in src/kernel/CMakeLists.txt: re-add -ffast-math and these fail.
//
//  __int128 is a GCC/Clang extension, so these are compiled only where it exists. The
//  int64 batteries above remain the portable floor.
// ─────────────────────────────────────────────────────────────────────────────
#if defined(__SIZEOF_INT128__)

namespace {

using i128 = __int128;

int sign128(i128 d) noexcept { return d > 0 ? 1 : (d < 0 ? -1 : 0); }
i128 wv(float f) { return static_cast<i128>(static_cast<std::int64_t>(f)); }

// Coordinates <= 2^21 -> differences <= 2^22, triple products <= 2^66: 8,192x past the
// 2^53 a double holds exactly, and comfortably inside __int128's 2^127.
i128 wideOrient3D(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d)
{
    const i128 adx = wv(a.x) - wv(d.x), ady = wv(a.y) - wv(d.y), adz = wv(a.z) - wv(d.z);
    const i128 bdx = wv(b.x) - wv(d.x), bdy = wv(b.y) - wv(d.y), bdz = wv(b.z) - wv(d.z);
    const i128 cdx = wv(c.x) - wv(d.x), cdy = wv(c.y) - wv(d.y), cdz = wv(c.z) - wv(d.z);
    return adx * (bdy * cdz - bdz * cdy) + ady * (bdz * cdx - bdx * cdz) +
           adz * (bdx * cdy - bdy * cdx);
}

// Coordinates <= 2^26 -> differences <= 2^27, products <= 2^54: past 2^53, so the
// cancellation in the 2x2 determinant is not something double can be trusted with.
i128 wideOrient2D(const Vec2& a, const Vec2& b, const Vec2& c)
{
    return (wv(a.u) - wv(c.u)) * (wv(b.v) - wv(c.v)) -
           (wv(a.v) - wv(c.v)) * (wv(b.u) - wv(c.u));
}

// Coordinates <= 2^20 -> lifted terms ~2^42, det ~2^105. Well past double, inside 2^127.
i128 wideInSphere(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& e)
{
    const i128 aex = wv(a.x) - wv(e.x), aey = wv(a.y) - wv(e.y), aez = wv(a.z) - wv(e.z);
    const i128 bex = wv(b.x) - wv(e.x), bey = wv(b.y) - wv(e.y), bez = wv(b.z) - wv(e.z);
    const i128 cex = wv(c.x) - wv(e.x), cey = wv(c.y) - wv(e.y), cez = wv(c.z) - wv(e.z);
    const i128 dex = wv(d.x) - wv(e.x), dey = wv(d.y) - wv(e.y), dez = wv(d.z) - wv(e.z);

    const i128 ab = aex * bey - bex * aey, bc = bex * cey - cex * bey;
    const i128 cd = cex * dey - dex * cey, da = dex * aey - aex * dey;
    const i128 ac = aex * cey - cex * aey, bd = bex * dey - dex * bey;

    const i128 abc = aez * bc - bez * ac + cez * ab;
    const i128 bcd = bez * cd - cez * bd + dez * bc;
    const i128 cda = cez * da + dez * ac + aez * cd;
    const i128 dab = dez * ab + aez * bd + bez * da;

    const i128 alift = aex * aex + aey * aey + aez * aez;
    const i128 blift = bex * bex + bey * bey + bez * bez;
    const i128 clift = cex * cex + cey * cey + cez * cez;
    const i128 dlift = dex * dex + dey * dey + dez * dez;

    return (dlift * abc - clift * dab) + (blift * cda - alift * bcd);
}

}  // namespace

// The battery that would have caught the folded error terms. Every fourth point is placed
// EXACTLY on the plane of the other three (the parallelogram corner b + c - a) and then
// nudged by a few units, so the run is dense in both exact degeneracies — which must
// report exactly zero — and configurations one hair off them.
TEST(RobustPredicatesExactness, Orient3DIsExactWhereADoubleCannotBe)
{
    std::mt19937_64 rng(20260730u);
    std::uniform_int_distribution<std::int64_t> coord(-(1L << 21), 1L << 21);
    std::uniform_int_distribution<std::int64_t> nudge(-4, 4);

    auto f = [](std::int64_t v) { return static_cast<float>(v); };
    int exactlyCoplanar = 0, offPlane = 0;
    for (int i = 0; i < 200000; ++i) {
        const std::int64_t ax = coord(rng), ay = coord(rng), az = coord(rng);
        const std::int64_t bx = coord(rng), by = coord(rng), bz = coord(rng);
        const std::int64_t cx = coord(rng), cy = coord(rng), cz = coord(rng);
        const std::int64_t dx = bx + cx - ax + nudge(rng);
        const std::int64_t dy = by + cy - ay + nudge(rng);
        const std::int64_t dz = bz + cz - az + nudge(rng);

        const Vec3 a{f(ax), f(ay), f(az)}, b{f(bx), f(by), f(bz)};
        const Vec3 c{f(cx), f(cy), f(cz)}, d{f(dx), f(dy), f(dz)};
        const i128 want = wideOrient3D(a, b, c, d);
        ASSERT_EQ(sign(RobustPredicates::orient3D(a, b, c, d)), sign128(want))
            << "wrong orient3D sign at i=" << i << " — coordinates are integer-valued and "
               "exact in float, so this is the predicate, not the input";
        if (want == 0) ++exactlyCoplanar; else ++offPlane;
    }
    // Both regimes must actually have been exercised, or the battery proves nothing.
    EXPECT_GT(exactlyCoplanar, 0) << "no exactly-coplanar quadruple was generated";
    EXPECT_GT(offPlane, 0) << "no off-plane quadruple was generated";
}

TEST(RobustPredicatesExactness, Orient2DIsExactWhereADoubleCannotBe)
{
    std::mt19937_64 rng(20260731u);
    std::uniform_int_distribution<std::int64_t> coord(-(1L << 26), 1L << 26);
    std::uniform_int_distribution<std::int64_t> nudge(-2, 2);

    auto f = [](std::int64_t v) { return static_cast<float>(v); };
    int exactlyCollinear = 0;
    for (int i = 0; i < 200000; ++i) {
        // c placed on the line through a and b (the far end of the doubled step), then
        // nudged — collinearity is the degeneracy orient2D exists to resolve.
        const std::int64_t ax = coord(rng), ay = coord(rng);
        const std::int64_t bx = coord(rng), by = coord(rng);
        const std::int64_t cx = 2 * bx - ax + nudge(rng);
        const std::int64_t cy = 2 * by - ay + nudge(rng);

        const Vec2 a{f(ax), f(ay)}, b{f(bx), f(by)}, c{f(cx), f(cy)};
        const i128 want = wideOrient2D(a, b, c);
        ASSERT_EQ(sign(RobustPredicates::orient2D(a, b, c)), sign128(want))
            << "wrong orient2D sign at i=" << i;
        if (want == 0) ++exactlyCollinear;
    }
    EXPECT_GT(exactlyCollinear, 0) << "no exactly-collinear triple was generated";
}

TEST(RobustPredicatesExactness, InSphereIsExactWhereADoubleCannotBe)
{
    std::mt19937_64 rng(20260732u);
    std::uniform_int_distribution<std::int64_t> coord(-(1L << 20), 1L << 20);

    auto f = [](std::int64_t v) { return static_cast<float>(v); };
    for (int i = 0; i < 20000; ++i) {
        Vec3 p[5];
        for (Vec3& q : p) q = {f(coord(rng)), f(coord(rng)), f(coord(rng))};
        const i128 want = wideInSphere(p[0], p[1], p[2], p[3], p[4]);
        ASSERT_EQ(sign(RobustPredicates::inSphere(p[0], p[1], p[2], p[3], p[4])), sign128(want))
            << "wrong inSphere sign at i=" << i;
    }
}

// THE BUILD-POLICY CANARY. The batteries above depend on a seeded draw finding a hard
// configuration; this one IS the configuration, recorded from the run that exposed the
// folded error terms, so the guard does not rest on a random number generator agreeing to
// look in the right place. Under -ffast-math this quadruple — exactly coplanar, verified
// against __int128 below — makes orient3D return -512.
//
// Note what a non-zero answer here costs. It is not a small numeric error: it is a
// confident side for a point that is ON the plane, so Simulation-of-Simplicity is never
// consulted and every downstream degeneracy strategy silently loses its tie-break.
//
// If this test fails, do not adjust the fixture. Check the floating-point flags in
// src/kernel/CMakeLists.txt — something re-granted the compiler permission to reassociate.
TEST(RobustPredicatesExactness, MeasuredCoplanarFixtureThatFastMathGetsWrong)
{
    const Vec3 a{1689837.f, -1587469.f, 2052225.f};
    const Vec3 b{1276634.f, -882946.f, 684609.f};
    const Vec3 c{-1353130.f, 1956769.f, 606251.f};
    const Vec3 d{-1766333.f, 2661292.f, -761365.f};  // == b + c - a, hence exactly coplanar

    ASSERT_EQ(wideOrient3D(a, b, c, d), 0) << "fixture is no longer coplanar";
    EXPECT_EQ(RobustPredicates::orient3D(a, b, c, d), 0.0)
        << "an exactly coplanar quadruple reported a side. The expansion arithmetic is not "
           "running — check the floating-point flags (-ffast-math folds the error terms of "
           "twoSum/twoProduct to zero, which degrades the predicate to plain double)";

    // And the resolution around it still works: one unit off the plane either way must give
    // opposite, non-zero signs agreeing with the exact reference.
    const Vec3 above{d.x, d.y, d.z + 1.f};
    const Vec3 below{d.x, d.y, d.z - 1.f};
    const int sAbove = sign(RobustPredicates::orient3D(a, b, c, above));
    const int sBelow = sign(RobustPredicates::orient3D(a, b, c, below));
    EXPECT_EQ(sAbove, sign128(wideOrient3D(a, b, c, above)));
    EXPECT_EQ(sBelow, sign128(wideOrient3D(a, b, c, below)));
    EXPECT_NE(sAbove, 0);
    EXPECT_EQ(sAbove, -sBelow) << "one unit either side of the plane did not resolve oppositely";
}

#endif  // __SIZEOF_INT128__
