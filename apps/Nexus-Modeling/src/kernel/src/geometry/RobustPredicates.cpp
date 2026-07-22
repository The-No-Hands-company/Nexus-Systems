#include <nexus/geometry/RobustPredicates.h>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstddef>
#include <vector>

namespace nexus::geometry {

// --- Exact arithmetic -------------------------------------------------------------
//
// These three predicates are the kernel's bedrock: every Delaunay/CDT insertion, every
// boolean classification, every Simulation-of-Simplicity tie-break asks one of them which
// side of something a point lies on. They must return the CORRECT SIGN — not a good
// estimate — or the structures built on them are unsound in ways no downstream check can
// repair.
//
// Each is evaluated in two stages, following Shewchuk ("Adaptive Precision Floating-Point
// Arithmetic and Fast Robust Geometric Predicates", 1997):
//
//   1. A floating-point estimate with Shewchuk's published error bound. If the estimate
//      exceeds that bound in magnitude, its sign is provably correct and it is returned.
//   2. Otherwise the determinant is recomputed in EXACT expansion arithmetic, which
//      represents an exact value as a sum of nonoverlapping doubles of increasing
//      magnitude. The sign of such an expansion is the sign of its largest component.
//
// Shewchuk's intermediate adaptive stages (B/C/D) are deliberately omitted: they are pure
// optimizations that refine the estimate before falling through to exact, and each one is
// another chance to get a subtle error bound wrong. Two stages give the same guarantee
// with far less that can be quietly incorrect.
//
// The arithmetic below is exact for ALL double inputs, not merely for the float
// coordinates the kernel currently stores — twoSum/twoDiff/twoProduct each capture their
// own rounding error, so nothing is assumed about the operands' exponent range.

namespace {

// Shewchuk's epsilon is half a unit in the last place, 2^-53. C's DBL_EPSILON is 2^-52.
constexpr double kEpsilon = DBL_EPSILON * 0.5;

// Published error bounds (Shewchuk, exactinit). Each multiplies a "permanent" — a
// quantity bounding the determinant's own rounding error.
constexpr double kCcwErrBoundA = (3.0 + 16.0 * kEpsilon) * kEpsilon;
constexpr double kO3dErrBoundA = (7.0 + 56.0 * kEpsilon) * kEpsilon;
constexpr double kIccErrBoundA = (10.0 + 96.0 * kEpsilon) * kEpsilon;

constexpr double kSplitter = 134217729.0;  // 2^27 + 1

struct Pair { double hi, lo; };

// a + b exactly, as hi + lo. Valid for arbitrary a, b (Knuth).
inline Pair twoSum(double a, double b) noexcept {
    const double s = a + b;
    const double bv = s - a;
    return {s, (a - (s - bv)) + (b - bv)};
}

// a - b exactly, as hi + lo. Valid for arbitrary a, b.
inline Pair twoDiff(double a, double b) noexcept {
    const double s = a - b;
    const double bv = a - s;
    return {s, (a - (s + bv)) + (bv - b)};
}

inline void split(double a, double& hi, double& lo) noexcept {
    const double c = kSplitter * a;
    const double big = c - a;
    hi = c - big;
    lo = a - hi;
}

// a * b exactly, as hi + lo.
inline Pair twoProduct(double a, double b) noexcept {
    const double x = a * b;
    double ahi, alo, bhi, blo;
    split(a, ahi, alo);
    split(b, bhi, blo);
    return {x, ((ahi * bhi - x) + ahi * blo + alo * bhi) + alo * blo};
}

// An expansion: nonoverlapping components in INCREASING magnitude, zeros eliminated. Its
// exact value is the sum of its components, so the largest component decides the sign.
using Expansion = std::vector<double>;

inline Expansion fromPair(Pair p) {
    Expansion e;
    e.reserve(2);
    if (p.lo != 0.0) e.push_back(p.lo);
    if (p.hi != 0.0) e.push_back(p.hi);
    return e;
}

// Exact sum of two expansions (Shewchuk's fast_expansion_sum_zeroelim). Merging the inputs
// by magnitude and running a carry chain of exact two-sums yields an expansion whose
// components are again nonoverlapping and increasing.
Expansion expansionSum(const Expansion& e, const Expansion& f) {
    if (e.empty()) return f;
    if (f.empty()) return e;

    Expansion merged;
    merged.reserve(e.size() + f.size());
    std::size_t i = 0, j = 0;
    while (i < e.size() && j < f.size()) {
        merged.push_back(std::abs(e[i]) < std::abs(f[j]) ? e[i++] : f[j++]);
    }
    while (i < e.size()) merged.push_back(e[i++]);
    while (j < f.size()) merged.push_back(f[j++]);

    Expansion h;
    h.reserve(merged.size());
    double q = merged[0];
    for (std::size_t k = 1; k < merged.size(); ++k) {
        const Pair s = twoSum(q, merged[k]);
        q = s.hi;
        if (s.lo != 0.0) h.push_back(s.lo);
    }
    if (q != 0.0 || h.empty()) h.push_back(q);
    return h;
}

// Exact product of an expansion and a scalar (Shewchuk's scale_expansion_zeroelim).
Expansion scaleExpansion(const Expansion& e, double b) {
    Expansion h;
    if (e.empty() || b == 0.0) return h;
    h.reserve(e.size() * 2);

    const Pair p0 = twoProduct(e[0], b);
    double q = p0.hi;
    if (p0.lo != 0.0) h.push_back(p0.lo);

    for (std::size_t i = 1; i < e.size(); ++i) {
        const Pair pi = twoProduct(e[i], b);
        const Pair s1 = twoSum(q, pi.lo);
        if (s1.lo != 0.0) h.push_back(s1.lo);
        // twoSum rather than fastTwoSum, so no operand-ordering precondition exists to be
        // violated; the cost is one extra subtraction on a path that is already rare.
        const Pair s2 = twoSum(pi.hi, s1.hi);
        q = s2.hi;
        if (s2.lo != 0.0) h.push_back(s2.lo);
    }
    if (q != 0.0 || h.empty()) h.push_back(q);
    return h;
}

// Exact product of two expansions: distribute, summing exactly as we go.
Expansion expansionMul(const Expansion& e, const Expansion& f) {
    Expansion acc;
    for (const double fi : f) acc = expansionSum(acc, scaleExpansion(e, fi));
    return acc;
}

Expansion expansionNegate(const Expansion& e) {
    Expansion h;
    h.reserve(e.size());
    for (const double v : e) h.push_back(-v);  // negation is exact
    return h;
}

Expansion expansionSub(const Expansion& a, const Expansion& b) {
    return expansionSum(a, expansionNegate(b));
}

// A double carrying the expansion's exact SIGN: components are nonoverlapping and
// increasing, so the last one dominates the sum and decides it.
inline double expansionSign(const Expansion& e) noexcept {
    return e.empty() ? 0.0 : e.back();
}

// --- Exact determinants ------------------------------------------------------------

double orient2DExact(const Vec2& a, const Vec2& b, const Vec2& c) {
    const Expansion acx = fromPair(twoDiff(a.u, c.u));
    const Expansion acy = fromPair(twoDiff(a.v, c.v));
    const Expansion bcx = fromPair(twoDiff(b.u, c.u));
    const Expansion bcy = fromPair(twoDiff(b.v, c.v));

    return expansionSign(expansionSub(expansionMul(acx, bcy), expansionMul(acy, bcx)));
}

double orient3DExact(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                     const nexus::render::Vec3& c, const nexus::render::Vec3& d) {
    const Expansion adx = fromPair(twoDiff(a.x, d.x));
    const Expansion ady = fromPair(twoDiff(a.y, d.y));
    const Expansion adz = fromPair(twoDiff(a.z, d.z));
    const Expansion bdx = fromPair(twoDiff(b.x, d.x));
    const Expansion bdy = fromPair(twoDiff(b.y, d.y));
    const Expansion bdz = fromPair(twoDiff(b.z, d.z));
    const Expansion cdx = fromPair(twoDiff(c.x, d.x));
    const Expansion cdy = fromPair(twoDiff(c.y, d.y));
    const Expansion cdz = fromPair(twoDiff(c.z, d.z));

    const Expansion m0 = expansionSub(expansionMul(bdy, cdz), expansionMul(bdz, cdy));
    const Expansion m1 = expansionSub(expansionMul(bdz, cdx), expansionMul(bdx, cdz));
    const Expansion m2 = expansionSub(expansionMul(bdx, cdy), expansionMul(bdy, cdx));

    const Expansion det = expansionSum(expansionSum(expansionMul(adx, m0),
                                                    expansionMul(ady, m1)),
                                       expansionMul(adz, m2));
    return expansionSign(det);
}

double inCircleExact(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) {
    const Expansion adx = fromPair(twoDiff(a.u, d.u));
    const Expansion ady = fromPair(twoDiff(a.v, d.v));
    const Expansion bdx = fromPair(twoDiff(b.u, d.u));
    const Expansion bdy = fromPair(twoDiff(b.v, d.v));
    const Expansion cdx = fromPair(twoDiff(c.u, d.u));
    const Expansion cdy = fromPair(twoDiff(c.v, d.v));

    const Expansion ab = expansionSub(expansionMul(adx, bdy), expansionMul(ady, bdx));
    const Expansion bc = expansionSub(expansionMul(bdx, cdy), expansionMul(bdy, cdx));
    const Expansion ca = expansionSub(expansionMul(cdx, ady), expansionMul(cdy, adx));

    const Expansion alift = expansionSum(expansionMul(adx, adx), expansionMul(ady, ady));
    const Expansion blift = expansionSum(expansionMul(bdx, bdx), expansionMul(bdy, bdy));
    const Expansion clift = expansionSum(expansionMul(cdx, cdx), expansionMul(cdy, cdy));

    const Expansion det = expansionSum(expansionSum(expansionMul(alift, bc),
                                                    expansionMul(blift, ca)),
                                       expansionMul(clift, ab));
    return expansionSign(det);
}

}  // anonymous namespace

// --- orient2D ---------------------------------------------------------------

double RobustPredicates::orient2D(const Vec2& a, const Vec2& b, const Vec2& c) noexcept {
    const double acx = static_cast<double>(a.u) - c.u;
    const double bcx = static_cast<double>(b.u) - c.u;
    const double acy = static_cast<double>(a.v) - c.v;
    const double bcy = static_cast<double>(b.v) - c.v;

    const double left = acx * bcy;
    const double right = acy * bcx;
    const double det = left - right;

    // Only a same-sign pair can cancel; with opposite signs the difference cannot lose the
    // sign, so the estimate is already safe (Shewchuk's orient2dfast reasoning).
    double detsum;
    if (left > 0.0) {
        if (right <= 0.0) return det;
        detsum = left + right;
    } else if (left < 0.0) {
        if (right >= 0.0) return det;
        detsum = -left - right;
    } else {
        return det;
    }

    const double errBound = kCcwErrBoundA * detsum;
    if (det >= errBound || -det >= errBound) return det;

    return orient2DExact(a, b, c);
}

// --- orient3D ---------------------------------------------------------------

double RobustPredicates::orient3D(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                                  const nexus::render::Vec3& c, const nexus::render::Vec3& d) noexcept {
    const double adx = static_cast<double>(a.x) - d.x;
    const double ady = static_cast<double>(a.y) - d.y;
    const double adz = static_cast<double>(a.z) - d.z;
    const double bdx = static_cast<double>(b.x) - d.x;
    const double bdy = static_cast<double>(b.y) - d.y;
    const double bdz = static_cast<double>(b.z) - d.z;
    const double cdx = static_cast<double>(c.x) - d.x;
    const double cdy = static_cast<double>(c.y) - d.y;
    const double cdz = static_cast<double>(c.z) - d.z;

    const double bdxcdy = bdx * cdy, cdxbdy = cdx * bdy;
    const double cdxady = cdx * ady, adxcdy = adx * cdy;
    const double adxbdy = adx * bdy, bdxady = bdx * ady;

    const double det = adz * (bdxcdy - cdxbdy)
                     + bdz * (cdxady - adxcdy)
                     + cdz * (adxbdy - bdxady);

    const double permanent = (std::abs(bdxcdy) + std::abs(cdxbdy)) * std::abs(adz)
                           + (std::abs(cdxady) + std::abs(adxcdy)) * std::abs(bdz)
                           + (std::abs(adxbdy) + std::abs(bdxady)) * std::abs(cdz);
    const double errBound = kO3dErrBoundA * permanent;
    if (det > errBound || -det > errBound) return det;

    return orient3DExact(a, b, c, d);
}

// --- inCircle ---------------------------------------------------------------

double RobustPredicates::inCircle(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) noexcept {
    const double adx = static_cast<double>(a.u) - d.u, ady = static_cast<double>(a.v) - d.v;
    const double bdx = static_cast<double>(b.u) - d.u, bdy = static_cast<double>(b.v) - d.v;
    const double cdx = static_cast<double>(c.u) - d.u, cdy = static_cast<double>(c.v) - d.v;

    const double bdxcdy = bdx * cdy, cdxbdy = cdx * bdy;
    const double cdxady = cdx * ady, adxcdy = adx * cdy;
    const double adxbdy = adx * bdy, bdxady = bdx * ady;

    const double alift = adx * adx + ady * ady;
    const double blift = bdx * bdx + bdy * bdy;
    const double clift = cdx * cdx + cdy * cdy;

    const double det = alift * (bdxcdy - cdxbdy)
                     + blift * (cdxady - adxcdy)
                     + clift * (adxbdy - bdxady);

    const double permanent = (std::abs(bdxcdy) + std::abs(cdxbdy)) * alift
                           + (std::abs(cdxady) + std::abs(adxcdy)) * blift
                           + (std::abs(adxbdy) + std::abs(bdxady)) * clift;
    const double errBound = kIccErrBoundA * permanent;
    if (det > errBound || -det > errBound) return det;

    return inCircleExact(a, b, c, d);
}

} // namespace nexus::geometry
