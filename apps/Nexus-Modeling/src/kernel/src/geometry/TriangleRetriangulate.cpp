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
        if (inside) out.triangles.push_back(t);
    }
    if (out.triangles.empty()) {
        return single();  // domain filter removed everything (degenerate) → safe fallback
    }
    return out;
}

}  // namespace nexus::geometry
