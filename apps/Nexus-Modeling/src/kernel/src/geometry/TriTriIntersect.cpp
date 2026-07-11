#include <nexus/geometry/TriTriIntersect.h>

#include <nexus/geometry/RobustPredicates.h>

#include <algorithm>
#include <bit>
#include <cstdint>

namespace nexus::geometry {

namespace {

using V = nexus::render::Vec3;

// -ffast-math makes std::isfinite unreliable; detect via IEEE-754 bit inspection.
bool isFiniteF(float x) noexcept
{
    constexpr uint32_t kExpMask = 0x7F800000u;
    return (std::bit_cast<uint32_t>(x) & kExpMask) != kExpMask;
}
bool finite3(const V& p) noexcept { return isFiniteF(p.x) && isFiniteF(p.y) && isFiniteF(p.z); }

V sub(const V& a, const V& b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V addv(const V& a, const V& b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V mul(const V& a, float s) noexcept { return {a.x * s, a.y * s, a.z * s}; }
V cross(const V& a, const V& b) noexcept
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double dotd(const V& a, const V& b) noexcept
{
    return static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y
         + static_cast<double>(a.z) * b.z;
}

// Intersection point of edge (p,q) with a plane, given the orient3D signed
// values dp (for p) and dq (for q), which have opposite signs.
V edgePlanePoint(const V& p, const V& q, double dp, double dq) noexcept
{
    const double denom = dp - dq;
    const double t     = (denom != 0.0) ? dp / denom : 0.5;
    return addv(p, mul(sub(q, p), static_cast<float>(t)));
}

// Up to two points where triangle (v0,v1,v2) meets a plane, given per-vertex
// orient3D signs d0,d1,d2. Returns the count; fills pts[0..count-1].
int crossingPoints(const V& v0, const V& v1, const V& v2, double d0, double d1, double d2, V pts[2]) noexcept
{
    const V      v[3] = {v0, v1, v2};
    const double d[3] = {d0, d1, d2};
    int          n    = 0;
    for (int i = 0; i < 3 && n < 2; ++i) {
        const int j = (i + 1) % 3;
        if (d[i] == 0.0) {
            pts[n++] = v[i];  // vertex lies on the plane
        } else if (d[j] != 0.0 && ((d[i] > 0.0) != (d[j] > 0.0))) {
            pts[n++] = edgePlanePoint(v[i], v[j], d[i], d[j]);
        }
    }
    return n;
}

bool allSameNonzero(double x, double y, double z) noexcept
{
    return (x > 0 && y > 0 && z > 0) || (x < 0 && y < 0 && z < 0);
}
bool allZero(double x, double y, double z) noexcept { return x == 0.0 && y == 0.0 && z == 0.0; }

}  // namespace

TriTriResult TriTriIntersect::intersect(const V& a0, const V& a1, const V& a2,
                                        const V& b0, const V& b1, const V& b2) noexcept
{
    TriTriResult r{};
    if (!finite3(a0) || !finite3(a1) || !finite3(a2) || !finite3(b0) || !finite3(b1) || !finite3(b2)) {
        return r;  // None
    }

    // Exact signs: B's vertices vs A's plane, and A's vertices vs B's plane.
    const double db0 = RobustPredicates::orient3D(a0, a1, a2, b0);
    const double db1 = RobustPredicates::orient3D(a0, a1, a2, b1);
    const double db2 = RobustPredicates::orient3D(a0, a1, a2, b2);
    const double da0 = RobustPredicates::orient3D(b0, b1, b2, a0);
    const double da1 = RobustPredicates::orient3D(b0, b1, b2, a1);
    const double da2 = RobustPredicates::orient3D(b0, b1, b2, a2);

    // If either triangle lies wholly on one side of the other's plane, no cross.
    if (allSameNonzero(db0, db1, db2) || allSameNonzero(da0, da1, da2)) {
        return r;  // None
    }

    // Both triangles in the same plane → coplanar overlap (deferred to a later stage).
    if (allZero(db0, db1, db2) && allZero(da0, da1, da2)) {
        r.kind = TriTriResult::Kind::Coplanar;
        return r;
    }

    // Points where each triangle crosses the other's plane — all on the shared
    // plane/plane line L.
    V         aPts[2], bPts[2];
    const int na = crossingPoints(a0, a1, a2, da0, da1, da2, aPts);
    const int nb = crossingPoints(b0, b1, b2, db0, db1, db2, bPts);
    if (na < 2 || nb < 2) {
        return r;  // touching at a single vertex/edge only → no proper segment
    }

    // Order both crossing pairs along L, then overlap the intervals.
    const V      nA = cross(sub(a1, a0), sub(a2, a0));
    const V      nB = cross(sub(b1, b0), sub(b2, b0));
    const V      D  = cross(nA, nB);
    if (dotd(D, D) < 1e-30) {
        r.kind = TriTriResult::Kind::Coplanar;  // parallel planes that aren't disjoint
        return r;
    }

    auto proj = [&](const V& p) { return dotd(p, D); };
    double aLoP = proj(aPts[0]), aHiP = proj(aPts[1]);
    V      aLo = aPts[0], aHi = aPts[1];
    if (aLoP > aHiP) { std::swap(aLoP, aHiP); std::swap(aLo, aHi); }
    double bLoP = proj(bPts[0]), bHiP = proj(bPts[1]);
    V      bLo = bPts[0], bHi = bPts[1];
    if (bLoP > bHiP) { std::swap(bLoP, bHiP); std::swap(bLo, bHi); }

    const double loP = std::max(aLoP, bLoP);
    const double hiP = std::min(aHiP, bHiP);
    if (loP > hiP) {
        return r;  // planes cross, but the triangles don't overlap along L → None
    }

    r.kind = TriTriResult::Kind::Segment;
    r.p0   = (aLoP >= bLoP) ? aLo : bLo;
    r.p1   = (aHiP <= bHiP) ? aHi : bHi;
    return r;
}

}  // namespace nexus::geometry
