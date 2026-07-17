#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <nexus/geometry/RobustPredicates.h>

#include <cmath>

namespace nexus::geometry::brep {

namespace {
// EXACT collinearity of two vectors: u × v == 0 iff all three 2×2 minors vanish,
// each an orient2D determinant about the origin (Shewchuk adaptive-exact). Robust
// where a float |cross|² < eps threshold mis-classifies a genuine shallow angle.
bool exactlyCollinear(const Vec3& u, const Vec3& v)
{
    const nexus::geometry::Vec2 O{0.f, 0.f};
    const double mx = RobustPredicates::orient2D(O, {u.y, u.z}, {v.y, v.z});  // u.y·v.z − u.z·v.y
    const double my = RobustPredicates::orient2D(O, {u.z, u.x}, {v.z, v.x});  // u.z·v.x − u.x·v.z
    const double mz = RobustPredicates::orient2D(O, {u.x, u.y}, {v.x, v.y});  // u.x·v.y − u.y·v.x
    return mx == 0.0 && my == 0.0 && mz == 0.0;
}
Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scale(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 normalize(const Vec3& a)
{
    const float l = length(a);
    return (l > 1e-20f) ? Vec3{a.x / l, a.y / l, a.z / l} : Vec3{0.f, 0.f, 0.f};
}
// A unit vector perpendicular to n.
Vec3 perp(const Vec3& n)
{
    const Vec3 t = (std::abs(n.x) <= std::abs(n.y) && std::abs(n.x) <= std::abs(n.z))
                       ? Vec3{1.f, 0.f, 0.f}
                       : (std::abs(n.y) <= std::abs(n.z) ? Vec3{0.f, 1.f, 0.f} : Vec3{0.f, 0.f, 1.f});
    return normalize(cross(n, t));
}
Curve lineCurve(const Vec3& origin, const Vec3& dir)
{
    Curve c;
    c.kind = CurveKind::Line;
    c.origin = origin;
    c.dir = normalize(dir);
    return c;
}
Curve circleCurve(const Vec3& center, const Vec3& axis, float radius)
{
    Curve c;
    c.kind = CurveKind::Circle;
    c.origin = center;
    c.dir = normalize(axis);
    c.ref = perp(c.dir);
    c.radius = radius;
    return c;
}

SurfaceIntersection planePlane(const Surface& a, const Surface& b, Tolerance tol)
{
    const Vec3 nA = normalize(a.normal), nB = normalize(b.normal);
    const Vec3 dir = cross(nA, nB);
    const float L2 = dot(dir, dir);
    // EXACT parallel decision on the stored normals — the planes are parallel iff
    // their normals are collinear. This is robust where the old `L2 < 1e-12`
    // float threshold mis-classified a genuine shallow-angle pair as parallel
    // (missing its real intersection Line) or vice-versa. The Line geometry below
    // is unchanged, so non-shallow cases are byte-identical.
    if (exactlyCollinear(a.normal, b.normal) || L2 < 1e-30f) {  // parallel (guard div-by-0)
        SurfaceIntersection r;
        r.kind = tol.isZero(dot(sub(b.origin, a.origin), nA)) ? SurfaceIntersectionKind::Unsupported
                                                              : SurfaceIntersectionKind::None;
        return r;  // coincident planes → whole-plane overlap (Unsupported here)
    }
    const float dA = dot(nA, a.origin), dB = dot(nB, b.origin);  // plane: dot(n,x)=d
    // Point on both planes nearest the origin: ((dA nB - dB nA) × dir) / |dir|².
    const Vec3 p0 = scale(cross(sub(scale(nB, dA), scale(nA, dB)), dir), 1.f / L2);
    SurfaceIntersection r;
    r.kind = SurfaceIntersectionKind::Line;
    r.curve = lineCurve(p0, dir);
    return r;
}

SurfaceIntersection planeSphere(const Surface& plane, const Surface& sphere, Tolerance tol)
{
    const Vec3 n = normalize(plane.normal);
    const float d = dot(sub(sphere.origin, plane.origin), n);  // signed dist centre→plane
    const Vec3 foot = sub(sphere.origin, scale(n, d));         // centre projected to plane
    SurfaceIntersection r;
    if (std::abs(d) > sphere.radius + tol.absolute) { r.kind = SurfaceIntersectionKind::None; return r; }
    if (tol.nearlyEqual(std::abs(d), sphere.radius)) {
        r.kind = SurfaceIntersectionKind::Point;
        r.point = foot;
        return r;
    }
    const float cr = std::sqrt(std::max(0.f, sphere.radius * sphere.radius - d * d));
    r.kind = SurfaceIntersectionKind::Circle;
    r.curve = circleCurve(foot, n, cr);
    return r;
}

SurfaceIntersection planeCylinder(const Surface& plane, const Surface& cyl, Tolerance tol)
{
    const Vec3 n = normalize(plane.normal);
    const Vec3 ax = normalize(cyl.normal);  // cylinder axis
    const float ndotax = dot(n, ax);
    SurfaceIntersection r;
    if (tol.nearlyEqual(std::abs(ndotax), 1.f)) {  // plane ⟂ axis → a circle
        // Axis line: cyl.origin + t*ax; find t where it meets the plane.
        const float denom = dot(ax, n);
        const float t = dot(sub(plane.origin, cyl.origin), n) / denom;
        const Vec3 center = add(cyl.origin, scale(ax, t));
        r.kind = SurfaceIntersectionKind::Circle;
        r.curve = circleCurve(center, ax, cyl.radius);
        return r;
    }
    r.kind = SurfaceIntersectionKind::Unsupported;  // parallel/skew → lines/ellipse (follow-up)
    return r;
}

SurfaceIntersection sphereSphere(const Surface& a, const Surface& b, Tolerance tol)
{
    const Vec3 delta = sub(b.origin, a.origin);
    const float dist = length(delta);
    SurfaceIntersection r;
    if (dist < 1e-9f) { r.kind = SurfaceIntersectionKind::None; return r; }  // concentric
    if (dist > a.radius + b.radius + tol.absolute ||
        dist < std::abs(a.radius - b.radius) - tol.absolute) {
        r.kind = SurfaceIntersectionKind::None;
        return r;
    }
    const Vec3 u = scale(delta, 1.f / dist);
    const float s = (dist * dist + a.radius * a.radius - b.radius * b.radius) / (2.f * dist);
    const Vec3 center = add(a.origin, scale(u, s));
    const float cr2 = a.radius * a.radius - s * s;
    if (cr2 <= tol.absolute * tol.absolute) {
        r.kind = SurfaceIntersectionKind::Point;
        r.point = center;
        return r;
    }
    r.kind = SurfaceIntersectionKind::Circle;
    r.curve = circleCurve(center, u, std::sqrt(cr2));
    return r;
}
}  // namespace

float surfaceDistance(const Surface& s, const Vec3& p)
{
    switch (s.kind) {
        case SurfaceKind::Plane:
            return dot(sub(p, s.origin), normalize(s.normal));
        case SurfaceKind::Sphere:
            return length(sub(p, s.origin)) - s.radius;
        case SurfaceKind::Cylinder: {
            const Vec3 ax = normalize(s.normal);
            const Vec3 w = sub(p, s.origin);
            const Vec3 radial = sub(w, scale(ax, dot(w, ax)));
            return length(radial) - s.radius;
        }
        default:
            return 1e30f;  // NURBS: not measured analytically here
    }
}

SurfaceIntersection intersectSurfaces(const Surface& a, const Surface& b, Tolerance tol)
{
    using K = SurfaceKind;
    if (a.kind == K::Plane && b.kind == K::Plane) return planePlane(a, b, tol);
    if (a.kind == K::Plane && b.kind == K::Sphere) return planeSphere(a, b, tol);
    if (a.kind == K::Sphere && b.kind == K::Plane) return planeSphere(b, a, tol);
    if (a.kind == K::Plane && b.kind == K::Cylinder) return planeCylinder(a, b, tol);
    if (a.kind == K::Cylinder && b.kind == K::Plane) return planeCylinder(b, a, tol);
    if (a.kind == K::Sphere && b.kind == K::Sphere) return sphereSphere(a, b, tol);
    SurfaceIntersection r;
    r.kind = SurfaceIntersectionKind::Unsupported;
    return r;
}

}  // namespace nexus::geometry::brep
