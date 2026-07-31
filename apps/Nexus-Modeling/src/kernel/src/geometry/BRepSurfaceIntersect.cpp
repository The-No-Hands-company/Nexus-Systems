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
Vec3 scale(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
// Returns DOUBLE. Returning float here quietly re-rounded every length, every angle and
// every projection in the B-rep back to single precision, however wide the points were:
// atan2(dot(...), dot(...)) cannot give a double angle from float arguments. This was the
// last float in the construction chain, and it was in the three-line helpers.
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 normalize(const Vec3& a)
{
    const double l = length(a);
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
// DOUBLE radius, for the same reason `dot` above returns double. Every caller computes
// the radius as a double — sqrt(R² − d²) for a plane section, the chord formula for
// sphere∩sphere — and `Curve::radius` stores a double, so a float parameter here rounded
// the value on its way between two double quantities and nothing downstream could see it.
//
// Measured on box(2³) against sphere(r=1.2): the seam radius came out
// 0.66332507133483887 where the exact value is 0.66332504433416373 — wrong by 2.7e-08,
// which is float resolution at this magnitude and nothing to do with the geometry. The
// box's ring was cut at the rounded radius while the sphere's ring, whose vertices are
// pinned to both the sphere and the cutting plane, landed at the exact one, leaving two
// concentric rings 2.7e-08 apart where there is only one circle. With the parameter
// widened they agree to 5.6e-17.
//
// Scope, stated honestly: this is a real narrowing and it is fixed here, but it is NOT
// why box/sphere sews to empty. That failure survives the fix unchanged, because the two
// rings are duplicated as SEPARATE VERTICES regardless of how exactly they agree.
Curve circleCurve(const Vec3& center, const Vec3& axis, double radius)
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
    const double L2 = dot(dir, dir);
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
    const double dA = dot(nA, a.origin), dB = dot(nB, b.origin);  // plane: dot(n,x)=d
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
    const double d = dot(sub(sphere.origin, plane.origin), n);  // signed dist centre→plane
    const Vec3 foot = sub(sphere.origin, scale(n, d));         // centre projected to plane
    SurfaceIntersection r;
    if (std::abs(d) > sphere.radius + tol.absolute) { r.kind = SurfaceIntersectionKind::None; return r; }
    if (tol.nearlyEqual(std::abs(d), sphere.radius)) {
        r.kind = SurfaceIntersectionKind::Point;
        r.point = foot;
        return r;
    }
    const double cr = std::sqrt(std::max(0.0, sphere.radius * sphere.radius - d * d));
    r.kind = SurfaceIntersectionKind::Circle;
    r.curve = circleCurve(foot, n, cr);
    return r;
}

SurfaceIntersection planeCylinder(const Surface& plane, const Surface& cyl, Tolerance tol)
{
    // The perpendicularity decision is exact (no tolerance band); the PARALLEL branch below
    // does need tol, since "the axis lies in the plane" is a metric test.

    const Vec3 n = normalize(plane.normal);
    const Vec3 ax = normalize(cyl.normal);  // cylinder axis
    SurfaceIntersection r;
    // EXACT: the plane is perpendicular to the axis — so the section is a true
    // CIRCLE — iff the plane normal is collinear with the axis (nA × ax == 0).
    // Robust where the old float nearlyEqual(|n·ax|, 1) band mis-classified a
    // near-perpendicular plane, whose real section is an ellipse, as a circle.
    if (exactlyCollinear(plane.normal, cyl.normal)) {
        // Axis line: cyl.origin + t*ax; find t where it meets the plane.
        const double denom = dot(ax, n);  // ±1 for collinear unit vectors → nonzero
        const double t = dot(sub(plane.origin, cyl.origin), n) / denom;
        const Vec3 center = add(cyl.origin, scale(ax, t));
        r.kind = SurfaceIntersectionKind::Circle;
        r.curve = circleCurve(center, ax, cyl.radius);
        return r;
    }
    // The plane CONTAINS the axis direction — it is parallel to the axis — iff the axis is
    // perpendicular to the plane normal. Then the section is not a curve of second degree at
    // all: the plane slices the cylinder along two straight generatrices (one, where it is
    // tangent; none, where it misses). This is the other half of the cylinder-through-box
    // case: the ±Z planes cut latitude circles, and the side walls cut exactly this pair of
    // uprights. Leaving it Unsupported meant an OFFSET cylinder never got its side wall
    // imprinted, so nothing could sew, and the box's own side face was never cut either.
    //
    // The decision is a dot product against zero, which is what a tolerance is for — but the
    // BRANCH is exact-adjacent: the perpendicular case above already claimed every plane
    // collinear with the axis, so what remains is genuinely either parallel or skew, and
    // only the parallel one is analytic here. A skew plane cuts an ELLIPSE and stays
    // Unsupported.
    const double axisDotNormal = dot(ax, n);
    if (std::abs(axisDotNormal) <= tol.at(1.f)) {
        // Distance from the axis to the plane, measured along the plane normal.
        const double d = dot(sub(cyl.origin, plane.origin), n);
        const double rad = cyl.radius;
        const double half2 = rad * rad - d * d;
        if (half2 < -tol.at(rad) * rad) {  // plane misses the cylinder entirely
            r.kind = SurfaceIntersectionKind::None;
            return r;
        }
        // Foot of the axis on the plane, and the in-plane direction perpendicular to the
        // axis — the direction the two generatrices are offset along.
        const Vec3 foot = sub(cyl.origin, scale(n, d));
        const Vec3 across = normalize(cross(ax, n));
        if (half2 <= tol.at(rad) * rad) {  // tangent: the two lines coincide
            r.kind = SurfaceIntersectionKind::Line;
            r.curve = lineCurve(foot, ax);
            return r;
        }
        const double half = std::sqrt(half2);
        r.kind = SurfaceIntersectionKind::TwoLines;
        r.curve = lineCurve(add(foot, scale(across, half)), ax);
        r.curve2 = lineCurve(sub(foot, scale(across, half)), ax);
        return r;
    }

    r.kind = SurfaceIntersectionKind::Unsupported;  // skew → ellipse (follow-up)
    return r;
}

// PLANE against CONE.
//
// A cone is stored as apex + axis + SLOPE (base radius over height), and the ring radius
// at axial distance v from the apex is slope*v. Two families of section are curves this
// kernel can represent, and they are the two that matter for cutting a cone with a box:
//
//   * a plane PERPENDICULAR to the axis cuts a CIRCLE, centred on the axis at the plane's
//     own axial distance and of radius slope*v — the same shape a plane cuts across a
//     cylinder, with the radius depending on where along the axis it falls;
//   * a plane CONTAINING both the apex and the axis cuts the two generatrices that lie in
//     it — TwoLines, exactly as a plane parallel to a cylinder's axis does.
//
// Every other plane cuts an ellipse, a parabola or a hyperbola, none of which is a Line or
// a Circle, so those stay Unsupported rather than being approximated. That is a real
// boundary and not an oversight: a box face parallel to the axis but missing the apex cuts
// a HYPERBOLA, so a cone wider than the box it sits in is still out of scope.
//
// The cone is single-napped — v is a distance from the apex along the axis — so a plane on
// the far side of the apex misses it entirely, and one through the apex meets it at exactly
// that point.
// CYLINDER against CYLINDER, for the case whose answer is straight: PARALLEL AXES.
//
// Two cylinders that share an axis direction meet along RULINGS — lines parallel to that
// direction — because the whole problem collapses to two circles in the plane
// perpendicular to it. Solve the circle pair there and extrude each solution point along
// the axis. Everything is decided by the perpendicular distance d between the axes:
//
//   d > rA + rB          apart, nothing
//   d = rA + rB          externally tangent, ONE ruling
//   |rA-rB| < d < rA+rB  two rulings
//   d = |rA - rB|        internally tangent, ONE ruling
//   d < |rA - rB|        one bore inside the other, nothing
//   d = 0, rA = rB       the same surface twice — Unsupported, as coincident planes are
//
// Axes that are skew or crossing are a different problem entirely: the section is a
// quartic space curve (the bicylinder / Steinmetz curve), which is neither a Line nor a
// Circle and is not approximated here.
SurfaceIntersection cylinderCylinder(const Surface& a, const Surface& b, Tolerance tol)
{
    SurfaceIntersection r;
    if (!exactlyCollinear(a.normal, b.normal)) {
        r.kind = SurfaceIntersectionKind::Unsupported;  // quartic; not a Line or a Circle
        return r;
    }
    const Vec3 ax = normalize(a.normal);
    // Offset between the axes, measured perpendicular to them — the axial part is
    // irrelevant, which is exactly why parallel axes are tractable at all.
    const Vec3 delta = sub(b.origin, a.origin);
    const Vec3 perp = sub(delta, scale(ax, dot(delta, ax)));
    const double d = length(perp);
    const double rA = a.radius, rB = b.radius;

    if (d <= tol.absolute) {
        // Concentric: either the same surface or two that never meet.
        r.kind = tol.nearlyEqual(rA, rB) ? SurfaceIntersectionKind::Unsupported
                                         : SurfaceIntersectionKind::None;
        return r;
    }
    if (d > rA + rB + tol.absolute || d < std::abs(rA - rB) - tol.absolute) {
        r.kind = SurfaceIntersectionKind::None;
        return r;
    }

    // Circle-pair solution in the perpendicular plane: the radical line sits at axial
    // offset `t` from A's centre, and the two solutions are ±h off it.
    const Vec3 u = scale(perp, 1.0 / d);
    const Vec3 w = cross(ax, u);  // unit: ax and u are perpendicular unit vectors
    const double t = (d * d + rA * rA - rB * rB) / (2.0 * d);
    const double h2 = rA * rA - t * t;
    const Vec3 base = add(a.origin, scale(u, t));

    if (h2 <= tol.absolute * tol.absolute) {
        // Tangent — the two rulings have merged into one. Reported as a single Line rather
        // than as two coincident ones, so a caller cannot imprint the same seam twice.
        r.kind = SurfaceIntersectionKind::Line;
        r.curve = lineCurve(base, ax);
        return r;
    }
    const double h = std::sqrt(h2);
    r.kind = SurfaceIntersectionKind::TwoLines;
    r.curve = lineCurve(add(base, scale(w, h)), ax);
    r.curve2 = lineCurve(sub(base, scale(w, h)), ax);
    return r;
}

SurfaceIntersection planeCone(const Surface& plane, const Surface& cone, Tolerance tol)
{
    const Vec3 n = normalize(plane.normal);
    const Vec3 ax = normalize(cone.normal);  // apex -> base
    SurfaceIntersection r;

    // EXACT perpendicularity, like the cylinder path: the section is a true circle iff the
    // plane normal is collinear with the axis. A float angle band would call a
    // near-perpendicular plane — whose section is an ellipse — a circle.
    if (exactlyCollinear(plane.normal, cone.normal)) {
        const double v = dot(sub(plane.origin, cone.origin), ax);
        if (v < -tol.absolute) { r.kind = SurfaceIntersectionKind::None; return r; }
        if (tol.isZero(v) || !(cone.radius > 0.0)) {
            r.kind = SurfaceIntersectionKind::Point;
            r.point = cone.origin;  // the apex itself
            return r;
        }
        r.kind = SurfaceIntersectionKind::Circle;
        r.curve = circleCurve(add(cone.origin, scale(ax, v)), ax, cone.radius * v);
        return r;
    }

    // The axis lies IN the plane and the apex is on it — the plane slices the cone through
    // its point, along the two rulings it contains.
    if (tol.isZero(dot(ax, n)) && tol.isZero(dot(sub(cone.origin, plane.origin), n))) {
        // A ruling has direction ax + slope*w for a unit radial w. It lies in the plane
        // when w is perpendicular to n as well as to the axis, which leaves exactly two.
        const Vec3 w = normalize(cross(ax, n));
        if (dot(w, w) < 0.5) { r.kind = SurfaceIntersectionKind::Unsupported; return r; }
        r.kind = SurfaceIntersectionKind::TwoLines;
        r.curve = lineCurve(cone.origin, add(ax, scale(w, cone.radius)));
        r.curve2 = lineCurve(cone.origin, sub(ax, scale(w, cone.radius)));
        return r;
    }

    r.kind = SurfaceIntersectionKind::Unsupported;  // ellipse / parabola / hyperbola
    return r;
}

SurfaceIntersection sphereSphere(const Surface& a, const Surface& b, Tolerance tol)
{
    const Vec3 delta = sub(b.origin, a.origin);
    const double dist = length(delta);
    SurfaceIntersection r;
    if (dist < 1e-9f) { r.kind = SurfaceIntersectionKind::None; return r; }  // concentric
    if (dist > a.radius + b.radius + tol.absolute ||
        dist < std::abs(a.radius - b.radius) - tol.absolute) {
        r.kind = SurfaceIntersectionKind::None;
        return r;
    }
    const Vec3 u = scale(delta, 1.f / dist);
    const double s = (dist * dist + a.radius * a.radius - b.radius * b.radius) / (2.f * dist);
    const Vec3 center = add(a.origin, scale(u, s));
    const double cr2 = a.radius * a.radius - s * s;
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
    if (a.kind == K::Cylinder && b.kind == K::Cylinder) return cylinderCylinder(a, b, tol);
    if (a.kind == K::Plane && b.kind == K::Cone) return planeCone(a, b, tol);
    if (a.kind == K::Cone && b.kind == K::Plane) return planeCone(b, a, tol);
    SurfaceIntersection r;
    r.kind = SurfaceIntersectionKind::Unsupported;
    return r;
}

}  // namespace nexus::geometry::brep
