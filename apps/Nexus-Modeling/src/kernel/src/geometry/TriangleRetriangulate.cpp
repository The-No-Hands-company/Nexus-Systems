#include <nexus/geometry/TriangleRetriangulate.h>

#include <nexus/geometry/ConstrainedDelaunay2D.h>
#include <nexus/geometry/RobustPredicates.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace nexus::geometry {

namespace {

using V = nexus::render::Vec3;

bool isFiniteF(float x) noexcept
{
    constexpr uint32_t kExpMask = 0x7F800000u;
    return (std::bit_cast<uint32_t>(x) & kExpMask) != kExpMask;
}
bool finite3(const V& p) noexcept { return isFiniteF(p.x) && isFiniteF(p.y) && isFiniteF(p.z); }

V    sub(const V& a, const V& b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V    addv(const V& a, const V& b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V    mul(const V& a, float s) noexcept { return {a.x * s, a.y * s, a.z * s}; }
float dot(const V& a, const V& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
V    cross(const V& a, const V& b) noexcept
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

}  // namespace

RetriangulationResult TriangleRetriangulate::apply(const V& a, const V& b, const V& c,
                                                   const std::vector<IntersectionSegment>& segments) noexcept
{
    RetriangulationResult out;
    auto single = [&]() -> RetriangulationResult {
        out.points    = {a, b, c};
        out.triangles = {{0u, 1u, 2u}};
        return out;
    };

    if (!finite3(a) || !finite3(b) || !finite3(c)) {
        return single();
    }
    const V     e1    = sub(b, a);
    const V     e2    = sub(c, a);
    const V     N     = cross(e1, e2);
    const float area2 = dot(N, N);
    const float uLen  = std::sqrt(dot(e1, e1));
    if (area2 < 1e-24f || uLen < 1e-20f || segments.empty()) {
        return single();  // degenerate triangle or nothing to cut
    }

    // In-plane orthonormal basis (origin a).
    const V U  = mul(e1, 1.f / uLen);
    const V Nn = mul(N, 1.f / std::sqrt(area2));
    const V Vv = cross(Nn, U);  // unit, in-plane, perpendicular to U

    auto to2d = [&](const V& p) -> Vec2 {
        const V d = sub(p, a);
        return Vec2{dot(d, U), dot(d, Vv)};
    };

    // Scale-aware dedup tolerance from the triangle's characteristic length.
    const float charLen = std::max({uLen, std::sqrt(dot(e2, e2)), std::sqrt(dot(sub(c, b), sub(c, b)))});
    const float eps2    = (1e-6f * charLen) * (1e-6f * charLen);

    std::vector<Vec2> pts;
    auto addPt = [&](const V& p) -> uint32_t {
        const Vec2 q = to2d(p);
        for (size_t i = 0; i < pts.size(); ++i) {
            const float du = pts[i].u - q.u;
            const float dv = pts[i].v - q.v;
            if (du * du + dv * dv <= eps2) {
                return static_cast<uint32_t>(i);
            }
        }
        pts.push_back(q);
        return static_cast<uint32_t>(pts.size() - 1);
    };

    addPt(a);
    addPt(b);
    addPt(c);  // indices 0,1,2

    std::vector<ConstraintEdge> constraints;
    for (const IntersectionSegment& s : segments) {
        if (!finite3(s.p0) || !finite3(s.p1)) {
            continue;
        }
        const uint32_t i0 = addPt(s.p0);
        const uint32_t i1 = addPt(s.p1);
        if (i0 != i1) {
            constraints.push_back(ConstraintEdge{i0, i1});
        }
    }

    if (constraints.empty()) {
        return single();  // segments collapsed to existing verts — nothing to add
    }

    const CDTResult cdt = ConstrainedDelaunay2D::triangulate(pts, constraints);
    if (cdt.triangles.empty()) {
        return single();  // CDT failed → safe fallback
    }

    // Lift 2D vertices back to 3D on the triangle's plane (affine, exact for
    // planar points).
    out.points.reserve(cdt.vertices.size());
    for (const Vec2& v : cdt.vertices) {
        out.points.push_back(addv(a, addv(mul(U, v.u), mul(Vv, v.v))));
    }

    // Restrict the result to the triangle's DOMAIN. The general CDT triangulates the
    // convex hull of all input points, and a seam endpoint that projects a hair
    // OUTSIDE this triangle (float noise on the plane projection) makes the CDT emit
    // sub-triangles that spill past the triangle's edges — extra area that overlaps the
    // neighbouring triangle's retriangulation and opens a boolean seam. A retriangulated
    // triangle must tile EXACTLY itself, so keep only sub-triangles whose centroid lies
    // inside triangle abc. Vertices 0,1,2 are a,b,c (added first); sidedness is EXACT
    // orient2D on the 2D projection.
    const Vec2& A2 = cdt.vertices[0];
    const Vec2& B2 = cdt.vertices[1];
    const Vec2& C2 = cdt.vertices[2];
    const double triOrient = RobustPredicates::orient2D(A2, B2, C2);
    out.triangles.reserve(cdt.triangles.size());
    for (const auto& t : cdt.triangles) {
        const Vec2& q0 = cdt.vertices[t[0]];
        const Vec2& q1 = cdt.vertices[t[1]];
        const Vec2& q2 = cdt.vertices[t[2]];
        const Vec2  ctr{(q0.u + q1.u + q2.u) / 3.f, (q0.v + q1.v + q2.v) / 3.f};
        const double d0 = RobustPredicates::orient2D(A2, B2, ctr);
        const double d1 = RobustPredicates::orient2D(B2, C2, ctr);
        const double d2 = RobustPredicates::orient2D(C2, A2, ctr);
        const bool inside = (triOrient >= 0.0) ? (d0 >= 0.0 && d1 >= 0.0 && d2 >= 0.0)
                                               : (d0 <= 0.0 && d1 <= 0.0 && d2 <= 0.0);
        if (!inside) continue;

        // Drop CAP sub-triangles: one corner sitting on the opposite edge, so the triangle
        // encloses no area. These are spurious. Where a seam point lands a hair off an
        // edge of this triangle, the CDT emits both the correct sub-triangles that use the
        // split path through that point AND a cap spanning the whole unsplit edge. The cap
        // duplicates area already covered: measured on a cut sphere, a cap's two shared
        // edges were each used THREE times (its own use plus the two real triangles that
        // split there) while its long edge was used once and had no partner — which is the
        // leak. Removing it takes those edges to two and the long edge out of existence.
        //
        // Losing area is not a risk: a cap encloses none. The test is SCALE-INVARIANT —
        // area against the triangle's own edge lengths, so it means the same for a 0.5mm
        // part and a 5km terrain — and it is deliberately strict, catching only triangles
        // that are degenerate to within float resolution rather than merely thin.
        const double e0u = q1.u - q0.u, e0v = q1.v - q0.v;
        const double e1u = q2.u - q1.u, e1v = q2.v - q1.v;
        const double e2u = q0.u - q2.u, e2v = q0.v - q2.v;
        const double sumSq = e0u * e0u + e0v * e0v + e1u * e1u + e1v * e1v
                           + e2u * e2u + e2v * e2v;
        if (sumSq > 0.0) {
            const double twiceArea = std::abs(RobustPredicates::orient2D(q0, q1, q2));
            // 2*area / (sum of squared edges) is dimensionless: ~0.29 for an equilateral
            // triangle, →0 as it degenerates. The threshold is measured, not guessed: over
            // 54,643 sub-triangles of a cut sphere the caps occupy 1.0e-7..4.5e-7 and the
            // next non-cap triangle is at 3e-6, with the 0.1th percentile of ordinary
            // geometry at 2.6e-4. 1e-6 sits in that gap with room on both sides.
            constexpr double kCapThinness = 1e-6;
            if (twiceArea / sumSq < kCapThinness) continue;
        }

        out.triangles.push_back(t);
    }
    if (out.triangles.empty()) {
        return single();  // domain filter removed everything (degenerate) → safe fallback
    }
    return out;
}

}  // namespace nexus::geometry
