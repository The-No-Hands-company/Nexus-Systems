#include <nexus/geometry/AnalyticBRep.h>

#include <nexus/geometry/MeshMassProperties.h>

#include <array>

#include <nexus/geometry/BRepSurfaceIntersect.h>
#include <nexus/geometry/RobustPredicates.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace nexus::geometry::brep {

namespace {
Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scale(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
// Returns DOUBLE. Returning float here quietly re-rounded every length, every angle and
// every projection in the B-rep back to single precision, however wide the points were:
// atan2(dot(...), dot(...)) cannot give a double angle from float arguments. This was the
// last float in the construction chain, and it was in the three-line helpers.
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 normalize(const Vec3& a)
{
    const double l = length(a);
    return (l > 1e-20f) ? Vec3{a.x / l, a.y / l, a.z / l} : Vec3{0.f, 0.f, 0.f};
}

// Parameter on an analytic curve that evaluates (closest) to point p. For a Line
// this is the projection distance along dir; for a Circle the sweep angle in the
// (ref, dir×ref) frame — matching Curve::eval so eval(paramOnCurve(c,p)) ≈ p when
// p lies on the curve.
// Returns DOUBLE. A float parameter was the last float in the construction chain: the ring
// vertex is placed in double, its angle is recovered here, and eval() puts it back — so a
// float angle re-rounded the point to float accuracy no matter how wide everything else was.
// Measured: the worst curve/vertex mismatch on an imprinted box sat at 1.24e-7, which is
// float's epsilon and not a coincidence.
double paramOnCurve(const Curve& c, const Vec3& p)
{
    if (c.kind == CurveKind::Circle) {
        const Vec3 bi = cross(c.dir, c.ref);
        const Vec3 w = sub(p, c.origin);
        return std::atan2(dot(w, bi), dot(w, c.ref));
    }
    return dot(sub(p, c.origin), c.dir);  // Line (Nurbs handled elsewhere)
}

// Fraction s in (0,1) at which the straight segment A→B crosses the Line imprint
// `curve` in a single interior point. The crossing DECISION is exact: the face is
// planar (normal `n`), and both the edge A→B and the imprint Line (origin O, dir
// D) lie in that plane, so A and B cross the line iff they lie on strictly
// OPPOSITE sides of it — decided by the exact sign of orient3D(O, O+D, ·, O+n)
// (the in-plane side determinant), immune to the catastrophic cancellation of a
// float closest-approach parameter. The split fraction `s` (a coordinate, not a
// combinatorial decision) is carried in double and snapped onto the curve by splitEdge.
// Only a Line imprint is handled (a Line in the planar face lies on the face).
bool segmentLineCrossing(const Vec3& A, const Vec3& B, const Curve& curve, const Vec3& n,
                         double eps, double& sOut)
{
    if (curve.kind != CurveKind::Line) return false;
    const Vec3 O = curve.origin, D = curve.dir;
    const Vec3 O1{O.x + D.x, O.y + D.y, O.z + D.z};
    const Vec3 On{O.x + n.x, O.y + n.y, O.z + n.z};
    // Exact in-plane side of each endpoint relative to the directed imprint line.
    const double sa = RobustPredicates::orient3D(O, O1, A, On);
    const double sb = RobustPredicates::orient3D(O, O1, B, On);
    if (sa == 0.0 || sb == 0.0) return false;    // endpoint on the line ⇒ the vertex case
    if ((sa > 0.0) == (sb > 0.0)) return false;  // same side ⇒ no crossing
    // Split location: closest-approach fraction of A→B to the imprint line.
    const Vec3 E = sub(B, A);
    const Vec3 ExD = cross(E, D);
    const double denom = dot(ExD, ExD);
    if (denom < 1e-20f) return false;  // parallel (defensive; a straddle implies not)
    const double s = dot(cross(sub(O, A), D), ExD) / denom;
    // Coplanarity sanity guard: the meeting point must lie on the line (a planar
    // face guarantees it; rejects a genuinely skew edge).
    const Vec3 P = add(A, scale(E, s));
    const Vec3 w = sub(P, O);
    if (length(sub(w, scale(D, dot(w, D)))) > eps) return false;
    // Only clamp away genuine roundoff outside [0,1] (the orient3D straddle test
    // already guarantees a crossing strictly between A and B). Do NOT push a
    // near-0/1 result away from the endpoint it belongs to — the caller decides,
    // from the *unperturbed* value, whether the crossing is close enough to an
    // existing boundary vertex to be resolved onto it rather than split (see
    // imprintCurve). Squashing the fraction here used to manufacture a spurious
    // vertex up to ~1% of the edge's length away from a true shared corner,
    // which is exactly the seam-vertex mismatch that opened the sewn shell.
    sOut = std::clamp(s, 0.0, 1.0);
    return true;
}
// Fractions s in (0, 1) at which the straight segment A→B crosses the circle of
// centre C radius r (both coplanar): roots of |A + sE − C|² = r². Returns 0/1/2
// interior crossings (a tangent counts once). The roots are returned at their
// true position — a crossing near an endpoint is NOT dropped or snapped here;
// the caller decides, from the unperturbed fraction, whether it is within a
// coincidence tolerance of an existing boundary vertex and resolves it onto that
// vertex, exactly as the line-imprint path does. (The former fixed (0.02, 0.98)
// band silently discarded legitimate near-corner arc bites — a crossing ~2% of
// an edge from a vertex is ~hundreds of times the coincidence tolerance, so it
// is a real crossing, not a degeneracy.)
// `eps` is a length tolerance on the segment's own ends. A crossing that lands exactly ON
// an endpoint has to COUNT, and demanding a strictly interior root is the same mistake
// this file has now made three times — once on a cylinder's uprights, once on a sphere's
// bounding arcs, and here on the planar path, which is the oldest of the three.
//
// It is not a rare tie. Two seam circles cut by two box faces that share an edge MEET on
// that edge, necessarily: both are sections of the same sphere, so their common points are
// exactly where the sphere crosses the shared edge. Whichever face is imprinted first
// splits that edge there, and the second circle then meets the edge precisely at a vertex.
//
// Which side of the boundary the root lands on is then pure rounding. MEASURED on
// box(2³) against sphere(r1.2): at offset 0.5 all eight crossings came back at
// s = 0.9999999999999999 and were accepted, and the face was cut correctly; at offset 0.7
// the same eight landed a few ulp the other side of 1, every edge reported zero crossings,
// and the +X face was left uncut and straddling — ten box faces where fourteen were owed,
// and all three operators empty. The two configurations differ by nothing structural.
int circleSegmentFracs(const Vec3& A, const Vec3& B, const Vec3& C, double r, double out[2],
                       double eps)
{
    const Vec3 E = sub(B, A), d = sub(A, C);
    const double a = dot(E, E);
    if (a < 1e-20f) return 0;
    const double b = 2.f * dot(d, E), c = dot(d, d) - r * r;
    const double disc = b * b - 4.f * a * c;
    if (disc < 0.f) return 0;
    const double sq = std::sqrt(disc);
    const double len = std::sqrt(a);
    const double slack = (len > 0.0) ? (eps / len) : 0.0;
    auto accept = [&](double s, double& dst) {
        if (!(s > -slack && s < 1.0 + slack)) return false;
        dst = std::min(std::max(s, 0.0), 1.0);  // caller snaps a clamped end to its vertex
        return true;
    };
    int cnt = 0;
    const double s0 = (-b - sq) / (2.f * a), s1 = (-b + sq) / (2.f * a);
    if (accept(s0, out[cnt])) ++cnt;
    if (sq > 1e-9f && accept(s1, out[cnt])) ++cnt;  // skip tangent duplicate
    // Both roots snapping to the SAME end is one crossing seen twice, not two.
    if (cnt == 2 && std::abs(out[0] - out[1]) * len <= eps) cnt = 1;
    return cnt;
}

// Nearest point of an analytic surface to `p`, where that has a closed form. Returns false
// for kinds without one — a plane needs none, and NURBS has no closed form here — so the
// caller can leave the point alone rather than invent one.
bool projectOntoSurface(const Surface& s, const Vec3& p, Vec3& out)
{
    switch (s.kind) {
        case SurfaceKind::Sphere: {
            const Vec3 d = sub(p, s.origin);
            const double l = length(d);
            if (!(l > 1e-12) || !(s.radius > 0.0)) return false;
            out = add(s.origin, scale(d, s.radius / l));
            return true;
        }
        case SurfaceKind::Cylinder: {
            const Vec3 ax = normalize(s.normal);
            const Vec3 d = sub(p, s.origin);
            const double axial = dot(d, ax);
            const Vec3 radial = sub(d, scale(ax, axial));
            const double rl = length(radial);
            if (!(rl > 1e-12) || !(s.radius > 0.0)) return false;
            out = add(add(s.origin, scale(ax, axial)), scale(radial, s.radius / rl));
            return true;
        }
        case SurfaceKind::Cone: {
            // origin = apex, normal = axis apex->base, radius = slope.
            const Vec3 ax = normalize(s.normal);
            const Vec3 d = sub(p, s.origin);
            const double v = dot(d, ax);
            if (!(v > 1e-12) || !(s.radius > 0.0)) return false;
            const Vec3 radial = sub(d, scale(ax, v));
            const double rl = length(radial);
            if (!(rl > 1e-12)) return false;
            out = add(add(s.origin, scale(ax, v)), scale(radial, (s.radius * v) / rl));
            return true;
        }
        default:
            return false;
    }
}

// Point-in-polygon for a planar polygon `poly` with normal `n`, projected to the
// plane's 2D frame (ray-crossing rule).
bool pointInPlanarPolygon(const Vec3& p, const std::vector<Vec3>& poly, const Vec3& n)
{
    if (poly.size() < 3) return false;
    const Vec3 u = normalize(sub(poly[1], poly[0]));
    const Vec3 v = cross(n, u);
    auto x2 = [&](const Vec3& q) { return dot(sub(q, poly[0]), u); };
    auto y2 = [&](const Vec3& q) { return dot(sub(q, poly[0]), v); };
    const double px = x2(p), py = y2(p);
    bool inside = false;
    const size_t m = poly.size();
    for (size_t i = 0, j = m - 1; i < m; j = i++) {
        const double xi = x2(poly[i]), yi = y2(poly[i]);
        const double xj = x2(poly[j]), yj = y2(poly[j]);
        if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
            inside = !inside;
    }
    return inside;
}
// Whether `circle` lies ON `s`, i.e. every point of the circle is a point of the
// surface — the precondition for imprinting it as a trim curve of a face on `s`.
//   Plane:    the circle's plane IS the surface plane (axis ∥ normal, centre on it).
//   Cylinder: a LATITUDE circle — axis ∥ the cylinder axis, centre on the axis, and
//             the same radius, which is the only circle a cylinder contains.
// `eps` is the face-proportioned coincidence tolerance the caller already derived.
bool circleLiesOnSurface(const Surface& s, const Curve& circle, float eps)
{
    if (circle.radius <= 0.f || !isFinite(circle.radius)) return false;
    const Vec3 cAxis = normalize(circle.dir);
    switch (s.kind) {
        case SurfaceKind::Plane: {
            const Vec3 n = s.normal;
            if (std::abs(dot(cAxis, n)) < 1.f - 1e-4f) return false;  // circle plane ∥ face plane
            return std::abs(dot(sub(circle.origin, s.origin), n)) <= eps;  // and coincident
        }
        case SurfaceKind::Cylinder: {
            const Vec3 ax = normalize(s.normal);
            if (std::abs(dot(cAxis, ax)) < 1.f - 1e-4f) return false;  // circle plane ⟂ axis
            if (std::abs(circle.radius - s.radius) > eps) return false;
            // Centre on the axis: the component of centre-origin across the axis is 0.
            const Vec3 d = sub(circle.origin, s.origin);
            return length(sub(d, scale(ax, dot(d, ax)))) <= eps;
        }
        case SurfaceKind::Sphere: {
            // A sphere carries a far richer circle family than a cylinder does. Every
            // plane section of a sphere is a circle, so rather than one radius at one
            // orientation there is a two-parameter family — which is exactly what a box
            // face cutting a sphere produces, and why this case is the one box/sphere
            // waits on. The condition: writing d for centre-to-centre, every point of
            // the circle is |d + r·w|² = |d|² + 2r·(d·w) + r² for a unit w in the
            // circle's plane, and that is constant in w only when d ⟂ that plane — i.e.
            // d lies along the circle's own axis. Then the constant is |d|² + r², which
            // must be the sphere's R².
            const Vec3 d = sub(circle.origin, s.origin);
            const double along = dot(d, cAxis);
            if (length(sub(d, scale(cAxis, along))) > eps) return false;  // centre off the axis
            return std::abs(std::sqrt(dot(d, d) + circle.radius * circle.radius) - s.radius)
                   <= eps;
        }
        case SurfaceKind::Cone: {
            // A cone's own circles are its rings: centred on the axis, perpendicular to it,
            // and — unlike a cylinder's, which all share one radius — of radius slope*v at
            // axial distance v from the apex. So the radius is not a constant to compare
            // against but a function of where along the axis the circle sits.
            const Vec3 ax = normalize(s.normal);
            if (std::abs(dot(cAxis, ax)) < 1.0 - 1e-6) return false;
            const Vec3 d = sub(circle.origin, s.origin);
            const double v = dot(d, ax);
            if (v < 0.0) return false;  // single-napped: nothing behind the apex
            if (length(sub(d, scale(ax, v))) > eps) return false;  // centre off the axis
            return std::abs(circle.radius - s.radius * v) <= eps;
        }
        default:
            return false;  // NURBS circle imprints are a later increment
    }
}

// Which parameter of `s` wraps every 2π, or -1 for none.
//
// NOT a formality: a cylinder sweeps its circumference in u, but a sphere's eval puts
// the poles on ±uAxis and sweeps LONGITUDE IN V — the opposite convention. Assuming
// "u wraps" would unwrap a sphere's latitude, which is bounded and must not be shifted,
// while leaving the genuinely periodic parameter to alias across the seam.
int periodicParam(const Surface& s)
{
    switch (s.kind) {
        case SurfaceKind::Cylinder: return 0;  // u sweeps the circumference
        case SurfaceKind::Sphere:   return 1;  // v is longitude; u is latitude
        case SurfaceKind::Cone:     return 0;  // u sweeps the ring; v is axial distance
        default:                    return -1;
    }
}

// Parameter-domain image (u,v) of a point lying on `s`, inverting Surface::eval for
// the kinds circleLiesOnSurface admits. Containment on a TRIMMED face is defined in
// the parameter domain (the same reason a Pcurve is stored there), which is what
// makes a curved face's boundary testable with the ordinary planar rule.
// DOUBLE out-params: `u` is an atan2 of two projections, and the whole point of the
// double migration was that such an angle cannot be recovered from float arguments.
bool surfaceUV(const Surface& s, const Vec3& p, double& u, double& v)
{
    const Vec3 d = sub(p, s.origin);
    switch (s.kind) {
        case SurfaceKind::Plane:
            u = dot(d, s.uAxis);
            v = dot(d, s.vAxis());
            return true;
        case SurfaceKind::Cylinder: {
            const Vec3 ax = normalize(s.normal);
            v = dot(d, ax);
            const Vec3 radial = sub(d, scale(ax, v));
            u = std::atan2(dot(radial, s.vAxis()), dot(radial, s.uAxis));
            return true;
        }
        case SurfaceKind::Sphere: {
            // Inverting eval: p = origin + R(sin u·uAxis + cos u cos v·vAxis + cos u sin v·normal).
            // So u (LATITUDE, poles on ±uAxis) comes from the uAxis component, and v
            // (longitude) from the remaining two. Guard the asin argument: a point on the
            // sphere gives |·| ≤ 1 exactly in exact arithmetic, and a hair over it in
            // floating point, where asin would return NaN.
            if (!(s.radius > 0.0)) return false;
            const double su = std::min(1.0, std::max(-1.0, dot(d, s.uAxis) / s.radius));
            u = std::asin(su);
            v = std::atan2(dot(d, s.normal), dot(d, s.vAxis()));
            return true;
        }
        case SurfaceKind::Cone: {
            // Inverting eval: p = apex + v*axis + (slope*v)(cos u * uAxis + sin u * vAxis).
            // v is the axial distance from the apex, and the angle comes from the radial
            // part exactly as on a cylinder — the radius varying with v does not move it.
            const Vec3 ax = normalize(s.normal);
            v = dot(d, ax);
            const Vec3 radial = sub(d, scale(ax, v));
            if (length(radial) < 1e-12) {
                u = 0.0;  // on the axis, i.e. the apex, where the angle has no value
                return true;
            }
            u = std::atan2(dot(radial, s.vAxis()), dot(radial, s.uAxis));
            return true;
        }
        default:
            return false;
    }
}

// Point-in-face test for a face on a CURVED surface: map the boundary ring and the
// query point into (u,v) and apply the planar rule there. The periodic u of a
// cylinder is UNWRAPPED along the ring — each successive u is shifted by whole turns
// so it stays within half a turn of its predecessor — which reconstructs a
// non-wrapping polygon even for a patch straddling the u = ±π seam. The query point
// is then placed on whichever branch of u falls inside the polygon's own u range, so
// a candidate arc's midpoint is compared against the patch on the same branch the
// patch occupies rather than against an aliased copy of it.
bool pointInSurfacePatchUV(const Surface& s, const Vec3& p, const std::vector<Vec3>& poly)
{
    if (poly.size() < 3) return false;
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    // WHICH parameter wraps is surface-dependent — u on a cylinder, v on a sphere — so the
    // unwrapping selects a component rather than assuming one. Getting this backwards would
    // shift a bounded latitude by whole turns and leave the periodic one aliasing.
    const int per = periodicParam(s);

    std::vector<Vec3> uv;
    uv.reserve(poly.size());
    double prev = 0.0;
    for (size_t i = 0; i < poly.size(); ++i) {
        double u = 0.0, v = 0.0;
        if (!surfaceUV(s, poly[i], u, v)) return false;
        double* cyc = (per == 0) ? &u : (per == 1) ? &v : nullptr;
        if (cyc != nullptr && i > 0) {
            while (*cyc - prev > kTwoPi * 0.5) *cyc -= kTwoPi;
            while (prev - *cyc > kTwoPi * 0.5) *cyc += kTwoPi;
        }
        if (cyc != nullptr) prev = *cyc;
        uv.push_back({u, v, 0.0});
    }

    double pu = 0.0, pv = 0.0;
    if (!surfaceUV(s, p, pu, pv)) return false;
    if (per >= 0) {
        double* q = (per == 0) ? &pu : &pv;
        double lo = (per == 0) ? uv[0].x : uv[0].y, hi = lo;
        for (const Vec3& e : uv) {
            const double c = (per == 0) ? e.x : e.y;
            lo = std::min(lo, c);
            hi = std::max(hi, c);
        }
        const double mid = (lo + hi) * 0.5;
        while (*q - mid > kTwoPi * 0.5) *q -= kTwoPi;
        while (mid - *q > kTwoPi * 0.5) *q += kTwoPi;
    }
    return pointInPlanarPolygon({pu, pv, 0.0}, uv, {0.0, 0.0, 1.0});
}

uint64_t edgeKey(uint32_t a, uint32_t b)
{
    const uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
    return (static_cast<uint64_t>(lo) << 32) | hi;
}
uint64_t dirKey(uint32_t a, uint32_t b)
{
    return (static_cast<uint64_t>(a) << 32) | b;
}

// Squared distance from point p to triangle (a,b,c). Standard region-based
// closest-point-on-triangle (Ericson, Real-Time Collision Detection).
float pointTriangleDist2(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c)
{
    const Vec3 ab = sub(b, a), ac = sub(c, a), ap = sub(p, a);
    const double d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0.f && d2 <= 0.f) return dot(ap, ap);
    const Vec3 bp = sub(p, b);
    const double d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0.f && d4 <= d3) return dot(bp, bp);
    const Vec3 cp = sub(p, c);
    const double d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0.f && d5 <= d6) return dot(cp, cp);
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
        const double w = d1 / (d1 - d3);
        const Vec3 q = add(a, scale(ab, w));
        return dot(sub(p, q), sub(p, q));
    }
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
        const double w = d2 / (d2 - d6);
        const Vec3 q = add(a, scale(ac, w));
        return dot(sub(p, q), sub(p, q));
    }
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
        const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        const Vec3 q = add(b, scale(sub(c, b), w));
        return dot(sub(p, q), sub(p, q));
    }
    const double denom = 1.f / (va + vb + vc);
    const double w2 = vb * denom, w3 = vc * denom;
    const Vec3 q = add(a, add(scale(ab, w2), scale(ac, w3)));
    return dot(sub(p, q), sub(p, q));
}

}  // namespace

// ──────────── Geometry evaluation ────────────────────────────────────────────

Vec3 Curve::eval(double t) const noexcept
{
    switch (kind) {
        case CurveKind::Line:
            return {origin.x + dir.x * t, origin.y + dir.y * t, origin.z + dir.z * t};
        case CurveKind::Circle: {
            const Vec3 bi = cross(dir, ref);  // dir = axis normal, ref = radius dir
            const double c = std::cos(t), s = std::sin(t);
            return {origin.x + radius * (c * ref.x + s * bi.x),
                    origin.y + radius * (c * ref.y + s * bi.y),
                    origin.z + radius * (c * ref.z + s * bi.z)};
        }
        case CurveKind::Nurbs:
        default:
            return origin;  // NURBS store wired in a later increment
    }
}

Vec3 Curve::normalAt(double t) const noexcept
{
    switch (kind) {
        case CurveKind::Line:
            return dir;  // tangent = direction
        case CurveKind::Circle: {
            const Vec3 bi = cross(dir, ref);
            const double c = std::cos(t), s = std::sin(t);
            return {c * ref.x + s * bi.x, c * ref.y + s * bi.y, c * ref.z + s * bi.z};
        }
        case CurveKind::Nurbs:
        default:
            return dir;
    }
}

Vec3 Surface::vAxis() const noexcept { return cross(normal, uAxis); }

Vec3 Surface::eval(double u, double v) const noexcept
{
    switch (kind) {
        case SurfaceKind::Plane: {
            const Vec3 va = vAxis();
            return {origin.x + u * uAxis.x + v * va.x,
                    origin.y + u * uAxis.y + v * va.y,
                    origin.z + u * uAxis.z + v * va.z};
        }
        case SurfaceKind::Cylinder: {
            const Vec3 va = vAxis();
            const double c = std::cos(u), s = std::sin(u);
            return {origin.x + radius * (c * uAxis.x + s * va.x) + v * normal.x,
                    origin.y + radius * (c * uAxis.y + s * va.y) + v * normal.y,
                    origin.z + radius * (c * uAxis.z + s * va.z) + v * normal.z};
        }
        case SurfaceKind::Sphere: {
            const Vec3 va = vAxis();
            const double cu = std::cos(u), su = std::sin(u);
            const double cv = std::cos(v), sv = std::sin(v);
            // u = longitude [-π,π], v = latitude [-π/2, π/2]
            return {origin.x + radius * (su * uAxis.x + cu * cv * va.x + cu * sv * normal.x),
                    origin.y + radius * (su * uAxis.y + cu * cv * va.y + cu * sv * normal.y),
                    origin.z + radius * (su * uAxis.z + cu * cv * va.z + cu * sv * normal.z)};
        }
        case SurfaceKind::Cone: {
            // origin = apex, normal = axis (apex -> base), radius = slope.
            // v is axial distance from the apex; the ring radius there is slope*v.
            const Vec3 va = vAxis();
            const double c = std::cos(u), sn = std::sin(u);
            const double rr = radius * v;
            return {origin.x + v * normal.x + rr * (c * uAxis.x + sn * va.x),
                    origin.y + v * normal.y + rr * (c * uAxis.y + sn * va.y),
                    origin.z + v * normal.z + rr * (c * uAxis.z + sn * va.z)};
        }
        case SurfaceKind::Nurbs:
        default:
            return origin;
    }
}

Vec3 Surface::normalAt(double u, double v) const noexcept
{
    switch (kind) {
        case SurfaceKind::Plane:
            return normal;
        case SurfaceKind::Cylinder: {
            const double c = std::cos(u), s = std::sin(u);
            return normalize({c * uAxis.x + s * vAxis().x,
                              c * uAxis.y + s * vAxis().y,
                              c * uAxis.z + s * vAxis().z});
        }
        case SurfaceKind::Sphere: {
            const Vec3 va = vAxis();
            const double cu = std::cos(u), su = std::sin(u);
            const double cv = std::cos(v), sv = std::sin(v);
            // outward normal = position - center = radius * (su*uAxis + cu*cv*va + cu*sv*normal)
            return normalize({su * uAxis.x + cu * cv * va.x + cu * sv * normal.x,
                              su * uAxis.y + cu * cv * va.y + cu * sv * normal.y,
                              su * uAxis.z + cu * cv * va.z + cu * sv * normal.z});
        }
        case SurfaceKind::Cone: {
            // On a cone |p_perp| = slope*(p . axis), so the gradient of that implicit
            // form gives the outward normal: radial direction minus slope along the axis,
            // normalised. It does not depend on v — every point up a ruling shares it.
            const Vec3 va = vAxis();
            const double c = std::cos(u), sn = std::sin(u);
            return normalize({c * uAxis.x + sn * va.x - radius * normal.x,
                              c * uAxis.y + sn * va.y - radius * normal.y,
                              c * uAxis.z + sn * va.z - radius * normal.z});
        }
        default:
            (void)v;
            return normal;
    }
}

// ──────────── Construction ───────────────────────────────────────────────────

std::optional<Body> Body::fromFaces(const std::vector<Vec3>& points,
                                     const std::vector<FaceDef>& faces)
{
    if (points.empty() || faces.empty()) return std::nullopt;

    Body b;
    b.m_verts.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) b.m_verts[i].point = points[i];

    std::unordered_map<uint64_t, uint32_t> edgeMap;    // undirected -> edge id
    std::unordered_map<uint64_t, uint32_t> dirCoedge;  // directed (a,b) -> coedge id

    // Build one coedge ring for `loopId` from an ordered vertex list, deduplicating
    // edges across every ring of every face and partnering opposite traversals. Outer
    // and inner (hole) rings are topologically the same construction — a hole differs
    // only in winding and in being listed as an inner loop — so both go through here
    // rather than the inner case getting a second, subtly different implementation.
    auto buildRing = [&](const std::vector<uint32_t>& ring, uint32_t loopId) -> bool {
        const size_t n = ring.size();
        if (n < 3) return false;
        for (uint32_t vi : ring)
            if (vi >= points.size()) return false;

        const uint32_t firstCoedge = static_cast<uint32_t>(b.m_coedges.size());
        for (size_t j = 0; j < n; ++j) {
            const uint32_t a = ring[j];
            const uint32_t c = ring[(j + 1) % n];
            if (a == c) return false;  // degenerate edge

            const uint64_t ek = edgeKey(a, c);
            uint32_t edgeId;
            auto it = edgeMap.find(ek);
            if (it != edgeMap.end()) {
                edgeId = it->second;
            } else {
                const uint32_t curveId = static_cast<uint32_t>(b.m_curves.size());
                Curve cur;
                cur.kind = CurveKind::Line;
                cur.origin = points[a];
                const Vec3 d = sub(points[c], points[a]);
                cur.dir = normalize(d);
                b.m_curves.push_back(cur);

                edgeId = static_cast<uint32_t>(b.m_edges.size());
                Edge ed;
                ed.curve = curveId;
                ed.v0 = a;
                ed.v1 = c;
                ed.t0 = 0.f;
                ed.t1 = length(d);
                b.m_edges.push_back(ed);
                edgeMap.emplace(ek, edgeId);
            }

            const uint32_t coedgeId = static_cast<uint32_t>(b.m_coedges.size());
            Coedge ce;
            ce.edge = edgeId;
            ce.reversed = (b.m_edges[edgeId].v0 != a);  // edge stored a->c or c->a
            ce.loop = loopId;
            b.m_coedges.push_back(ce);

            if (b.m_edges[edgeId].coedge == kInvalid) b.m_edges[edgeId].coedge = coedgeId;
            if (b.m_verts[a].coedge == kInvalid) b.m_verts[a].coedge = coedgeId;

            // Reject non-manifold input (rather than silently building a corrupt
            // Body): the same directed edge a→c used twice, or a shared edge that
            // already has both its coedges — either would over-write a partner and
            // leave a non-reciprocal link. Degenerate boolean sews (near-coincident
            // welded facets) surface here, so the caller gets a clean nullopt.
            if (dirCoedge.count(dirKey(a, c))) return false;  // directed edge reused
            auto pit = dirCoedge.find(dirKey(c, a));
            if (pit != dirCoedge.end()) {
                if (b.m_coedges[pit->second].partner != kInvalid) return false;  // 3rd coedge
                b.m_coedges[coedgeId].partner = pit->second;
                b.m_coedges[pit->second].partner = coedgeId;
            }
            dirCoedge[dirKey(a, c)] = coedgeId;
        }

        for (size_t j = 0; j < n; ++j) {
            const uint32_t cur = firstCoedge + static_cast<uint32_t>(j);
            b.m_coedges[cur].next = firstCoedge + static_cast<uint32_t>((j + 1) % n);
            b.m_coedges[cur].prev = firstCoedge + static_cast<uint32_t>((j + n - 1) % n);
        }
        b.m_loops[loopId].first = firstCoedge;
        return true;
    };

    for (const FaceDef& fd : faces) {
        if (fd.loop.size() < 3) return std::nullopt;

        const uint32_t surfaceId = static_cast<uint32_t>(b.m_surfaces.size());
        b.m_surfaces.push_back(fd.surface);
        if (fd.nurbsSurface.has_value()) {
            const uint32_t handle = static_cast<uint32_t>(b.m_nurbsSurfaces.size());
            b.m_nurbsSurfaces.push_back(*fd.nurbsSurface);
            b.m_surfaces[surfaceId].kind = SurfaceKind::Nurbs;
            b.m_surfaces[surfaceId].nurbs = handle;
        }

        const uint32_t faceId = static_cast<uint32_t>(b.m_faces.size());
        b.m_faces.push_back({});
        const uint32_t loopId = static_cast<uint32_t>(b.m_loops.size());
        b.m_loops.push_back({});
        b.m_faces[faceId].surface = surfaceId;
        b.m_faces[faceId].reversed = fd.reversed;
        b.m_faces[faceId].outerLoop = loopId;
        b.m_loops[loopId].face = faceId;
        b.m_loops[loopId].outer = true;
        if (!buildRing(fd.loop, loopId)) return std::nullopt;

        // Inner (hole) rings. A boolean seam through the middle of a face — a cylinder
        // piercing a box's face — leaves the face bounded outside and holed inside, and
        // until now that hole could not be expressed here at all, so it was dropped and
        // the sew failed. Each ring is wound opposite the outer loop by the caller.
        for (const std::vector<uint32_t>& hole : fd.innerLoops) {
            if (hole.size() < 3) return std::nullopt;
            const uint32_t innerId = static_cast<uint32_t>(b.m_loops.size());
            b.m_loops.push_back({});
            b.m_loops[innerId].face = faceId;
            b.m_loops[innerId].outer = false;
            if (!buildRing(hole, innerId)) return std::nullopt;
            b.m_faces[faceId].innerLoops.push_back(innerId);
        }
    }

    // One shell of all faces; closed iff every edge is used by two coedges.
    Shell sh;
    sh.faces.resize(b.m_faces.size());
    for (uint32_t f = 0; f < b.m_faces.size(); ++f) { sh.faces[f] = f; b.m_faces[f].shell = 0; }
    std::vector<uint32_t> coedgesPerEdge(b.m_edges.size(), 0u);
    for (const Coedge& ce : b.m_coedges)
        if (ce.edge < coedgesPerEdge.size()) ++coedgesPerEdge[ce.edge];
    sh.closed = true;
    for (uint32_t c : coedgesPerEdge)
        if (c != 2u) { sh.closed = false; break; }
    b.m_shells.push_back(std::move(sh));
    b.m_solids.push_back(Solid{{0u}});

    return b;
}

// ──────────── Integrity validation ───────────────────────────────────────────

Body::IntegrityReport Body::checkIntegrity() const
{
    IntegrityReport r;
    const uint32_t V = static_cast<uint32_t>(m_verts.size());
    const uint32_t E = static_cast<uint32_t>(m_edges.size());
    const uint32_t C = static_cast<uint32_t>(m_coedges.size());
    const uint32_t L = static_cast<uint32_t>(m_loops.size());
    const uint32_t F = static_cast<uint32_t>(m_faces.size());

    auto fail = [&](std::string why) -> IntegrityReport {
        IntegrityReport bad;
        bad.ok = false;
        bad.reason = std::move(why);
        return bad;
    };
    auto startV = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v1 : m_edges[e.edge].v0; };
    auto endV   = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v0 : m_edges[e.edge].v1; };
    // Liveness predicates: an entity is live iff in range AND its alive flag set.
    auto vLive = [&](uint32_t i) { return i < V && m_verts[i].alive; };
    auto eLive = [&](uint32_t i) { return i < E && m_edges[i].alive; };
    auto cLive = [&](uint32_t i) { return i < C && m_coedges[i].alive; };
    auto lLive = [&](uint32_t i) { return i < L && m_loops[i].alive; };
    auto fLive = [&](uint32_t i) { return i < F && m_faces[i].alive; };

    // Only LIVE edges are validated; a live edge must reference live vertices.
    for (uint32_t e = 0; e < E; ++e) {
        if (!m_edges[e].alive) continue;
        const Edge& ed = m_edges[e];
        if (ed.curve >= m_curves.size()) return fail("edge " + std::to_string(e) + " has invalid curve");
        if (!vLive(ed.v0) || !vLive(ed.v1)) return fail("live edge " + std::to_string(e) + " references a dead/invalid vertex");
        if (ed.v0 == ed.v1) return fail("edge " + std::to_string(e) + " is degenerate (v0==v1)");
    }

    std::vector<uint32_t> coedgesPerEdge(E, 0u);
    for (uint32_t c = 0; c < C; ++c) {
        if (!m_coedges[c].alive) continue;
        const Coedge& ce = m_coedges[c];
        if (!eLive(ce.edge)) return fail("live coedge " + std::to_string(c) + " references a dead/invalid edge");
        if (!lLive(ce.loop)) return fail("live coedge " + std::to_string(c) + " references a dead/invalid loop");
        if (!cLive(ce.next) || !cLive(ce.prev)) return fail("live coedge " + std::to_string(c) + " has dead/invalid next/prev");
        if (m_coedges[ce.next].prev != c) return fail("next.prev mismatch at coedge " + std::to_string(c));
        if (m_coedges[ce.prev].next != c) return fail("prev.next mismatch at coedge " + std::to_string(c));
        if (m_coedges[ce.next].loop != ce.loop) return fail("loop cycle spans multiple loops at coedge " + std::to_string(c));
        if (endV(c) != startV(ce.next)) return fail("vertex discontinuity at coedge " + std::to_string(c));
        if (ce.partner != kInvalid) {
            if (!cLive(ce.partner)) return fail("live coedge " + std::to_string(c) + " has a dead/invalid partner");
            const Coedge& pt = m_coedges[ce.partner];
            if (pt.partner != c) return fail("partner not reciprocal at coedge " + std::to_string(c));
            if (pt.edge != ce.edge) return fail("partner uses a different edge at coedge " + std::to_string(c));
            if (pt.reversed == ce.reversed) return fail("partner has same orientation at coedge " + std::to_string(c));
            if (pt.loop == ce.loop) return fail("partner on same loop at coedge " + std::to_string(c));
        }
        ++coedgesPerEdge[ce.edge];
        ++r.coedges;
    }

    for (uint32_t l = 0; l < L; ++l) {
        if (!m_loops[l].alive) continue;
        const Loop& lp = m_loops[l];
        if (!fLive(lp.face)) return fail("live loop " + std::to_string(l) + " references a dead/invalid face");
        if (!cLive(lp.first)) return fail("live loop " + std::to_string(l) + " references a dead/invalid first coedge");
        uint32_t walk = lp.first, steps = 0;
        do {
            if (m_coedges[walk].loop != l) return fail("loop " + std::to_string(l) + " ring leaves the loop");
            walk = m_coedges[walk].next;
            if (++steps > C) return fail("loop " + std::to_string(l) + " ring does not close");
        } while (walk != lp.first);
        if (steps < 3) return fail("loop " + std::to_string(l) + " has fewer than 3 coedges");
        ++r.loops;
    }

    for (uint32_t f = 0; f < F; ++f) {
        if (!m_faces[f].alive) continue;
        const Face& fc = m_faces[f];
        if (fc.surface >= m_surfaces.size()) return fail("face " + std::to_string(f) + " has invalid surface");
        if (!lLive(fc.outerLoop)) return fail("live face " + std::to_string(f) + " references a dead/invalid outer loop");
        if (!m_loops[fc.outerLoop].outer) return fail("face " + std::to_string(f) + " outer loop is not marked outer");
        if (m_loops[fc.outerLoop].face != f) return fail("face " + std::to_string(f) + " outer loop points elsewhere");
        for (uint32_t il : fc.innerLoops) {
            if (!lLive(il)) return fail("live face " + std::to_string(f) + " references a dead/invalid inner loop");
            if (m_loops[il].outer) return fail("face " + std::to_string(f) + " inner loop marked outer");
            if (m_loops[il].face != f) return fail("face " + std::to_string(f) + " inner loop points elsewhere");
        }
        ++r.faces;
    }

    for (uint32_t v = 0; v < V; ++v) {
        if (!m_verts[v].alive) continue;
        ++r.vertices;
        if (m_verts[v].coedge != kInvalid) {
            if (!cLive(m_verts[v].coedge)) return fail("live vertex " + std::to_string(v) + " references a dead/invalid coedge");
            if (startV(m_verts[v].coedge) != v) return fail("vertex " + std::to_string(v) + " coedge not rooted here");
        }
    }

    for (uint32_t e = 0; e < E; ++e) {
        if (!m_edges[e].alive) continue;
        ++r.edges;
        if (coedgesPerEdge[e] == 0u) return fail("edge " + std::to_string(e) + " is orphaned (no coedge)");
        if (coedgesPerEdge[e] > 2u) return fail("edge " + std::to_string(e) + " is non-manifold (>2 coedges)");
        if (coedgesPerEdge[e] == 1u) ++r.boundaryEdges;
    }

    r.euler = static_cast<int>(r.vertices) - static_cast<int>(r.edges) + static_cast<int>(r.faces);
    r.shells = static_cast<uint32_t>(m_shells.size());
    return r;
}

bool Body::isClosed() const noexcept
{
    // Topologically closed ⇔ every live edge is used by exactly two coedges (no
    // boundary edge). Reflects the actual complex, incl. holes/open faces, rather
    // than a possibly-stale shell flag.
    std::vector<uint32_t> cnt(m_edges.size(), 0u);
    for (const Coedge& c : m_coedges)
        if (c.alive && c.edge < cnt.size()) ++cnt[c.edge];
    bool anyLive = false;
    for (uint32_t e = 0; e < m_edges.size(); ++e) {
        if (!m_edges[e].alive) continue;
        anyLive = true;
        if (cnt[e] != 2u) return false;
    }
    return anyLive;
}

// ──────────── Geometric-consistency validation ───────────────────────────────

Body::GeometryReport Body::checkGeometry(Tolerance tol) const
{
    GeometryReport r;
    auto fail = [&](std::string why) -> GeometryReport {
        GeometryReport bad;
        bad.ok = false;
        bad.reason = std::move(why);
        return bad;
    };

    // All (live) vertex points are finite.
    for (uint32_t v = 0; v < m_verts.size(); ++v)
        if (m_verts[v].alive && !isFinite(m_verts[v].point))
            return fail("vertex " + std::to_string(v) + " has a non-finite point");

    // All curve geometry is finite.
    for (uint32_t c = 0; c < m_curves.size(); ++c) {
        const Curve& cu = m_curves[c];
        if (!isFinite(cu.origin) || !isFinite(cu.dir) || !isFinite(cu.ref) || !isFinite(cu.radius))
            return fail("curve " + std::to_string(c) + " has non-finite geometry");
    }

    // All surface geometry is finite and normals are unit length.
    for (uint32_t s = 0; s < m_surfaces.size(); ++s) {
        const Surface& su = m_surfaces[s];
        if (!isFinite(su.origin) || !isFinite(su.normal) || !isFinite(su.uAxis) || !isFinite(su.radius))
            return fail("surface " + std::to_string(s) + " has non-finite geometry");
        if (!tol.nearlyEqual(length(su.normal), 1.f))
            return fail("surface " + std::to_string(s) + " normal is not unit length");
    }

    // Each edge's curve reproduces its endpoint vertices over its param range.
    for (uint32_t e = 0; e < m_edges.size(); ++e) {
        if (!m_edges[e].alive) continue;
        const Edge& ed = m_edges[e];
        if (ed.curve >= m_curves.size() || ed.v0 >= m_verts.size() || ed.v1 >= m_verts.size())
            return fail("edge " + std::to_string(e) + " has invalid references");
        const Curve& cu = m_curves[ed.curve];
        if (!coincident(cu.eval(ed.t0), m_verts[ed.v0].point, tol))
            return fail("edge " + std::to_string(e) + " curve does not meet v0 at t0");
        if (!coincident(cu.eval(ed.t1), m_verts[ed.v1].point, tol))
            return fail("edge " + std::to_string(e) + " curve does not meet v1 at t1");
        ++r.checkedEdges;
    }

    // Partnered coedges traverse the shared edge in opposite directions, so the
    // start vertex of one is the end vertex of the other (and vice versa).
    auto sVert = [&](const Coedge& x) { return x.reversed ? m_edges[x.edge].v1 : m_edges[x.edge].v0; };
    auto eVert = [&](const Coedge& x) { return x.reversed ? m_edges[x.edge].v0 : m_edges[x.edge].v1; };
    for (uint32_t c = 0; c < m_coedges.size(); ++c) {
        if (!m_coedges[c].alive) continue;
        const Coedge& ce = m_coedges[c];
        if (ce.partner == kInvalid || ce.partner >= m_coedges.size()) continue;
        const Coedge& pt = m_coedges[ce.partner];
        if (sVert(ce) != eVert(pt) || eVert(ce) != sVert(pt))
            return fail("coedge " + std::to_string(c) + " and partner disagree on shared-edge endpoints");
    }

    // Pcurves (parameter-space trim curves) map back onto their coedge's 3D
    // endpoint vertices through the owning face's surface — the consistency that
    // makes a trimmed (NURBS) surface valid.
    for (uint32_t c = 0; c < m_coedges.size(); ++c) {
        const Coedge& ce = m_coedges[c];
        if (!ce.alive || !ce.pcurve.present) continue;
        const Pcurve& pc = ce.pcurve;
        if (!isFinite(pc.u0) || !isFinite(pc.v0) || !isFinite(pc.u1) || !isFinite(pc.v1))
            return fail("coedge " + std::to_string(c) + " has a non-finite pcurve");
        if (ce.edge >= m_edges.size() || ce.loop >= m_loops.size())
            return fail("coedge " + std::to_string(c) + " pcurve has invalid references");
        const Edge& ed = m_edges[ce.edge];
        if (ed.v0 >= m_verts.size() || ed.v1 >= m_verts.size())
            return fail("coedge " + std::to_string(c) + " pcurve edge has invalid vertices");
        const uint32_t faceId = m_loops[ce.loop].face;
        if (faceId >= m_faces.size() || m_faces[faceId].surface >= m_surfaces.size())
            return fail("coedge " + std::to_string(c) + " pcurve has invalid surface");
        const uint32_t surfId = m_faces[faceId].surface;
        const uint32_t sV = ce.reversed ? ed.v1 : ed.v0;
        const uint32_t eV = ce.reversed ? ed.v0 : ed.v1;
        if (!coincident(surfacePoint(surfId, pc.u0, pc.v0), m_verts[sV].point, tol))
            return fail("coedge " + std::to_string(c) + " pcurve start does not map to its start vertex");
        if (!coincident(surfacePoint(surfId, pc.u1, pc.v1), m_verts[eV].point, tol))
            return fail("coedge " + std::to_string(c) + " pcurve end does not map to its end vertex");
        // Curved (polyline) interior points must be finite and inside the
        // surface's parameter domain so tessellation samples the surface validly.
        const Surface& psurf = m_surfaces[surfId];
        bool haveDomain = false;
        float du0 = 0.f, du1 = 0.f, dv0 = 0.f, dv1 = 0.f;
        if (psurf.kind == SurfaceKind::Nurbs && psurf.nurbs < m_nurbsSurfaces.size()) {
            const auto [a, b] = m_nurbsSurfaces[psurf.nurbs].domainU();
            const auto [cc, d] = m_nurbsSurfaces[psurf.nurbs].domainV();
            du0 = a; du1 = b; dv0 = cc; dv1 = d;
            haveDomain = isFinite(a) && isFinite(b) && isFinite(cc) && isFinite(d);
        }
        const double su = haveDomain ? tol.at(du1 - du0) : 0.f;
        const double sv = haveDomain ? tol.at(dv1 - dv0) : 0.f;
        for (const auto& ip : pc.interior) {
            if (!isFinite(ip.first) || !isFinite(ip.second))
                return fail("coedge " + std::to_string(c) + " pcurve has a non-finite interior point");
            if (haveDomain && (ip.first < du0 - su || ip.first > du1 + su ||
                               ip.second < dv0 - sv || ip.second > dv1 + sv))
                return fail("coedge " + std::to_string(c) + " pcurve interior point is outside the surface domain");
        }
    }


    // Every face's own vertices must LIE ON that face's surface.
    //
    // Nothing checked this, and a body could therefore carry a surface that simply was not
    // the surface of its face. makeCone tagged its lateral faces as cylinders "as an
    // approximation"; the tagged cylinder does not contain the apex at all, so 16 of a
    // cone's 48 lateral vertices sat a full unit off the surface every query would consult
    // for them — and checkGeometry reported the body clean. Topological validity is not
    // geometric consistency, which is why there are two checkers; this is the check that
    // makes the second one mean what its name says.
    for (uint32_t fi = 0; fi < static_cast<uint32_t>(m_faces.size()); ++fi) {
        const Face& face = m_faces[fi];
        if (!face.alive || face.surface == kInvalid) continue;
        if (face.surface >= m_surfaces.size()) continue;
        const Surface& su = m_surfaces[face.surface];
        if (su.kind == SurfaceKind::Nurbs) continue;  // evaluated through the NURBS store

        const double scale = std::max(1.0, std::abs(su.radius));
        for (const uint32_t vid : faceVertices(fi)) {
            if (vid >= m_verts.size() || !m_verts[vid].alive) continue;
            const Vec3 p = m_verts[vid].point;
            const Vec3 d{p.x - su.origin.x, p.y - su.origin.y, p.z - su.origin.z};
            const Vec3 va = su.vAxis();
            const double axial = d.x * su.normal.x + d.y * su.normal.y + d.z * su.normal.z;
            const Vec3 perp{d.x - axial * su.normal.x, d.y - axial * su.normal.y,
                            d.z - axial * su.normal.z};
            const double radial = length(perp);

            float deviation = 0.f;
            switch (su.kind) {
                case SurfaceKind::Plane:    deviation = std::abs(axial); break;
                case SurfaceKind::Cylinder: deviation = std::abs(radial - su.radius); break;
                case SurfaceKind::Sphere:   deviation = std::abs(length(d) - su.radius); break;
                // Ring radius grows as slope * axial distance from the apex.
                case SurfaceKind::Cone:     deviation = std::abs(radial - su.radius * axial); break;
                default:                    deviation = 0.f; break;
            }
            (void)va;
            if (!(deviation <= 1e-3f * scale)) {
                return fail("face " + std::to_string(fi) + " vertex " + std::to_string(vid)
                            + " is not on its own surface " + std::to_string(face.surface)
                            + " (deviation " + std::to_string(deviation) + ")");
            }
        }
    }

    return r;
}

// ──────────── Euler operators ────────────────────────────────────────────────

uint32_t Body::splitEdge(uint32_t edgeId, double t, Tolerance tol)
{
    if (edgeId >= m_edges.size()) return kInvalid;
    const uint32_t curveId = m_edges[edgeId].curve;
    const uint32_t v0 = m_edges[edgeId].v0;
    const uint32_t v1 = m_edges[edgeId].v1;
    if (curveId >= m_curves.size() || v0 >= m_verts.size() || v1 >= m_verts.size()) return kInvalid;
    const double et0 = m_edges[edgeId].t0, et1 = m_edges[edgeId].t1;
    // Floor `t` away from 0/1 only enough to keep both sub-edges at/above the
    // Tolerance floor for THIS edge's own length — a degenerate-length safety
    // net, sized to the edge, not a fixed fraction of it. The previous fixed
    // [0.01, 0.99] clamp forced any near-endpoint split at least 1% of the
    // edge's length away from the true crossing; for a seam imprint that true
    // crossing legitimately sits within a coincidence tolerance of an existing
    // vertex on ONE operand but not (yet) recognised as such, that 1% forced
    // move manufactured a vertex measurably off the shared corner — the two
    // operands then disagreed on where the seam vertex sat and the sewn shell
    // was left open. A tolerance-scaled floor keeps the split at its true
    // computed position whenever that position is not already within a hair of
    // an endpoint, and callers that DO land within a coincidence tolerance of
    // an endpoint are expected to resolve directly onto that vertex instead of
    // calling this at all (see imprintCurve's crossing resolution).
    const double edgeLen = std::abs(et1 - et0);
    const double floorT = (edgeLen > 0.0)
                              ? std::min(0.5, static_cast<double>(tol.at(static_cast<float>(edgeLen))) / edgeLen)
                              : 1e-4;
    t = std::clamp(t, floorT, 1.0 - floorT);
    const double tm = et0 + (et1 - et0) * t;
    // Guard the far-from-origin case the tolerance-scaled floor alone does not:
    // for a huge |et0|/|et1| the sub-edge param delta can be smaller than the
    // endpoint's float ULP, so `tm` rounds back onto an endpoint and the split
    // would emit a zero-length edge. Reject cleanly (as splitEdge does for every
    // other failure) rather than manufacture a degenerate — the caller then
    // treats the imprint as a no-op, preserving watertight-or-empty. Handles
    // either param direction (Circle arc ranges may store t1 < t0).
    const double loT = std::min(et0, et1), hiT = std::max(et0, et1);
    if (!(tm > loT && tm < hiT)) return kInvalid;

    // New vertex on the shared curve.
    const uint32_t nv = static_cast<uint32_t>(m_verts.size());
    {
        Vertex vtx;
        vtx.point = m_curves[curveId].eval(tm);
        m_verts.push_back(vtx);
    }

    // Edge A reuses `edgeId` (v0 -> nv over [t0,tm]); edge B is new (nv -> v1).
    const uint32_t edgeA = edgeId;
    const uint32_t edgeB = static_cast<uint32_t>(m_edges.size());
    {
        Edge b;
        b.curve = curveId;
        b.v0 = nv;
        b.v1 = v1;
        b.t0 = tm;
        b.t1 = et1;
        m_edges.push_back(b);
    }
    m_edges[edgeA].v1 = nv;
    m_edges[edgeA].t1 = tm;

    // The (up to two) coedges over the original edge.
    const uint32_t c = m_edges[edgeA].coedge;
    if (c >= m_coedges.size()) return kInvalid;
    const uint32_t p = m_coedges[c].partner;

    // Split one coedge: reuse it as sub1, insert a sub2 after it in its loop.
    // A forward coedge (reversed=false) reads v0->v1, so sub1 is over edge A and
    // sub2 over edge B; a reversed coedge reads v1->v0, so sub1 is over edge B
    // and sub2 over edge A. Returns sub2's id.
    auto splitCoedge = [&](uint32_t ce) -> uint32_t {
        const bool rev = m_coedges[ce].reversed;
        const uint32_t oldNext = m_coedges[ce].next;
        const uint32_t sub2 = static_cast<uint32_t>(m_coedges.size());
        Coedge nc;
        nc.reversed = rev;
        nc.loop = m_coedges[ce].loop;
        nc.prev = ce;
        nc.next = oldNext;
        nc.edge = rev ? edgeA : edgeB;
        m_coedges.push_back(nc);
        m_coedges[ce].next = sub2;
        if (oldNext < m_coedges.size()) m_coedges[oldNext].prev = sub2;
        m_coedges[ce].edge = rev ? edgeB : edgeA;
        return sub2;
    };

    const uint32_t cNew = splitCoedge(c);
    uint32_t pNew = kInvalid;
    if (p != kInvalid && p < m_coedges.size()) pNew = splitCoedge(p);

    // Of a (coedge, its new sub2) pair, which sub sits over edge A / edge B.
    auto overA = [&](uint32_t ce, uint32_t ceNew) { return m_coedges[ce].reversed ? ceNew : ce; };
    auto overB = [&](uint32_t ce, uint32_t ceNew) { return m_coedges[ce].reversed ? ce : ceNew; };

    if (pNew != kInvalid) {
        const uint32_t cA = overA(c, cNew), cB = overB(c, cNew);
        const uint32_t pA = overA(p, pNew), pB = overB(p, pNew);
        m_coedges[cA].partner = pA; m_coedges[pA].partner = cA;
        m_coedges[cB].partner = pB; m_coedges[pB].partner = cB;
    } else {
        m_coedges[c].partner = kInvalid;
        m_coedges[cNew].partner = kInvalid;
    }

    // Edge back-references and the new vertex's outgoing coedge.
    m_edges[edgeA].coedge = overA(c, cNew);
    m_edges[edgeB].coedge = overB(c, cNew);
    // The coedge that starts at nv: over edge B if c is forward (nv->v1), over
    // edge A if c is reversed (nv->v0).
    m_verts[nv].coedge = m_coedges[c].reversed ? overA(c, cNew) : overB(c, cNew);

    return nv;
}

std::vector<uint32_t> Body::faceVertices(uint32_t faceId) const
{
    std::vector<uint32_t> out;
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return out;
    const uint32_t loopId = m_faces[faceId].outerLoop;
    if (loopId >= m_loops.size()) return out;
    uint32_t w = m_loops[loopId].first, guard = 0;
    if (w >= m_coedges.size()) return out;
    do {
        const Coedge& x = m_coedges[w];
        out.push_back(x.reversed ? m_edges[x.edge].v1 : m_edges[x.edge].v0);
        w = m_coedges[w].next;
        if (++guard > m_coedges.size() + 1) break;
    } while (w != m_loops[loopId].first);
    return out;
}

Vec3 Body::faceSamplePoint(uint32_t faceId) const
{
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return {};
    const Vec3 centroid = faceCentroid(faceId);

    const std::vector<std::vector<uint32_t>> holeRings = faceInnerLoopVertices(faceId);

    const uint32_t sid = m_faces[faceId].surface;
    if (sid >= m_surfaces.size()) return centroid;

    // A CURVED face is sampled at its outline's centroid PROJECTED ONTO ITS SURFACE.
    //
    // The material test below is a planar point-in-polygon one, so a curved face cannot use
    // the candidate search and falls back to the centroid. But the centroid of a ring of
    // points on a curved surface does not lie on that surface — it sags inside, by the
    // chord-versus-arc difference. The point that decides how a whole face is classified is
    // then a point the face does not contain, which is the same mistake the holed-planar
    // case was fixed for, wearing different clothes.
    //
    // It is not a small effect at the scale that matters. MEASURED on a sphere of radius
    // 1.10 meeting a cylinder of radius 1 along its axis: every one of the sphere's 120
    // face samples sat at |p| = 1.0735 rather than 1.10, and for the 24 narrow faces
    // between the seam and the neighbouring grid latitude that sag was enough to move the
    // sample from radius 1.008 — outside the cylinder, where the face's material is — to
    // 0.978, inside it. All 24 were classified Inside, dropped from the union, and left 72
    // boundary edges one-sided; the boolean returned empty. At radius 1.30 the same sag is
    // not enough to flip anything, which is why neighbouring configurations worked and made
    // the failure look like a property of the radius.
    //
    // Projecting restores the invariant the sample point exists to have: it lies on the
    // face. A plane needs no projection and gets none, so every planar caller is unchanged.
    if (m_surfaces[sid].kind != SurfaceKind::Plane) {
        const Surface& surf = m_surfaces[sid];
        Vec3 projected{};
        const bool haveProjection = projectOntoSurface(surf, centroid, projected);
        if (!haveProjection) return centroid;
        if (holeRings.empty()) return projected;

        // A curved face WITH a hole needs the same treatment the planar one gets, for the
        // same reason: the outline's average is drawn toward the middle, and on a face
        // whose middle is an opening that is exactly where it must not be. Projecting alone
        // does not help — it moves the point onto the surface while leaving it over the
        // hole.
        //
        // Not reachable through the imprint today: sweeping every configuration this kernel
        // can imprint — 254 bodies across box/sphere, box/cylinder, box/cone, cylinder
        // pairs, sphere pairs and chained plates — produces NO curved face carrying an
        // inner loop, because the interior-circle path is gated to planes and every curved
        // pair that could cut one is a quartic this kernel declines. It is reachable
        // through `fromFaces`, which is public: a cylindrical patch with a hole punched in
        // it was sampled at the exact centre of the hole.
        //
        // Containment is decided in the surface's (u,v) domain, where a trimmed face's
        // boundary is an ordinary polygon, and every candidate is projected onto the
        // surface before being judged — so the point that wins is on the face in both
        // senses. If the parametrisation cannot answer (it is singular at a sphere's
        // poles), the projected centroid stands, which is no worse than before.
        std::vector<Vec3> outerUV;
        for (const uint32_t v : faceVertices(faceId))
            if (v < m_verts.size()) outerUV.push_back(m_verts[v].point);
        if (outerUV.size() < 3) return projected;

        std::vector<std::vector<Vec3>> holePts;
        for (const std::vector<uint32_t>& ring : holeRings) {
            std::vector<Vec3> pts;
            for (const uint32_t v : ring)
                if (v < m_verts.size()) pts.push_back(m_verts[v].point);
            if (pts.size() >= 3) holePts.push_back(std::move(pts));
        }
        if (holePts.empty()) return projected;

        auto onCurvedMaterial = [&](const Vec3& p) {
            if (!pointInSurfacePatchUV(surf, p, outerUV)) return false;
            for (const std::vector<Vec3>& h : holePts)
                if (pointInSurfacePatchUV(surf, p, h)) return false;
            return true;
        };
        auto ringDistance = [](const Vec3& p, const std::vector<Vec3>& ring) {
            double best = std::numeric_limits<double>::max();
            for (size_t i = 0; i < ring.size(); ++i) {
                const Vec3& a = ring[i];
                const Vec3& b = ring[(i + 1) % ring.size()];
                const Vec3 ab = sub(b, a);
                const double den = dot(ab, ab);
                double t = (den > 0.0) ? (dot(sub(p, a), ab) / den) : 0.0;
                t = std::min(1.0, std::max(0.0, t));
                best = std::min(best, length(sub(p, add(a, scale(ab, t)))));
            }
            return best;
        };

        Vec3 best = projected;
        double bestClear = -1.0;
        auto offerCurved = [&](const Vec3& raw) {
            Vec3 p{};
            if (!projectOntoSurface(surf, raw, p)) return;
            if (!onCurvedMaterial(p)) return;
            double c = ringDistance(p, outerUV);
            for (const std::vector<Vec3>& h : holePts) c = std::min(c, ringDistance(p, h));
            if (c > bestClear) {
                bestClear = c;
                best = p;
            }
        };
        // Same candidate order as the planar ladder: the outer-to-hole midpoints first,
        // since those are what survive a hole concentric with its face.
        for (const Vec3& o : outerUV)
            for (const std::vector<Vec3>& h : holePts)
                for (const Vec3& hp : h)
                    offerCurved({(o.x + hp.x) * 0.5, (o.y + hp.y) * 0.5, (o.z + hp.z) * 0.5});
        for (const double t : {0.25, 0.5, 0.75})
            for (size_t i = 0; i < outerUV.size(); ++i) {
                const Vec3& a = outerUV[i];
                const Vec3& b = outerUV[(i + 1) % outerUV.size()];
                const Vec3 mid{(a.x + b.x) * 0.5, (a.y + b.y) * 0.5, (a.z + b.z) * 0.5};
                offerCurved({mid.x + (centroid.x - mid.x) * t, mid.y + (centroid.y - mid.y) * t,
                             mid.z + (centroid.z - mid.z) * t});
            }
        return bestClear >= 0.0 ? best : projected;
    }
    const Vec3 n = m_surfaces[sid].normal;

    // The boundary as a polygon — but built by REFINING each curved edge, not by taking its
    // endpoints. A ring of bare vertices replaces every arc with its chord, and for an arc
    // that bulges INTO the face that chord encloses material the face does not own. The arc
    // bite is exactly that shape: the remainder of a bitten square has a chord along the
    // original straight edge, so as a bare-vertex polygon it reads as the whole square and
    // the lens that was removed looks like part of it. Sampling the arc puts the boundary
    // back where it is. Straight edges are unaffected, so a face with no curved boundary
    // produces the identical ring it always did.
    std::vector<Vec3> outerPts;
    {
        const uint32_t loopId = m_faces[faceId].outerLoop;
        if (loopId >= m_loops.size() || m_loops[loopId].first >= m_coedges.size())
            return centroid;
        constexpr uint32_t kArcSamples = 8;  // intermediate points per arc edge
        const uint32_t first = m_loops[loopId].first;
        uint32_t w = first, guard = 0;
        do {
            const Coedge& ce = m_coedges[w];
            if (ce.edge >= m_edges.size()) return centroid;
            const Edge& ed = m_edges[ce.edge];
            const uint32_t vs = ce.reversed ? ed.v1 : ed.v0;
            if (vs >= m_verts.size()) return centroid;
            outerPts.push_back(m_verts[vs].point);
            // Interior samples of a curved edge, walked in the coedge's own direction so the
            // ring stays ordered.
            if (ed.curve < m_curves.size() && m_curves[ed.curve].kind == CurveKind::Circle) {
                const Curve& cu = m_curves[ed.curve];
                for (uint32_t k = 1; k < kArcSamples; ++k) {
                    const double f = static_cast<float>(k) / static_cast<float>(kArcSamples);
                    const double t = ce.reversed ? (ed.t1 + (ed.t0 - ed.t1) * f)
                                                : (ed.t0 + (ed.t1 - ed.t0) * f);
                    outerPts.push_back(cu.eval(t));
                }
            }
            w = ce.next;
            if (++guard > m_coedges.size() + 1u) return centroid;  // corrupt loop guard
        } while (w != first);
    }
    if (outerPts.size() < 3) return centroid;

    std::vector<std::vector<Vec3>> holes;
    holes.reserve(holeRings.size());
    for (const std::vector<uint32_t>& ring : holeRings) {
        std::vector<Vec3> pts;
        pts.reserve(ring.size());
        for (uint32_t v : ring)
            if (v < m_verts.size()) pts.push_back(m_verts[v].point);
        if (pts.size() >= 3) holes.push_back(std::move(pts));
    }

    // On the material = inside the outer boundary and inside none of the holes.
    auto onMaterial = [&](const Vec3& p) {
        if (!pointInPlanarPolygon(p, outerPts, n)) return false;
        for (const std::vector<Vec3>& h : holes)
            if (pointInPlanarPolygon(p, h, n)) return false;
        return true;
    };

    // A face with no holes whose centroid is genuinely on it needs nothing further, and
    // returning the centroid unchanged keeps every such caller bit-for-bit identical.
    //
    // But "no holes" is NOT the same as "the outline's average is on the face", which is
    // what this used to assume. That holds for a CONVEX face and fails for a concave one —
    // and an arc bite produces concave faces by construction. Take the square left when a
    // circle bites a lens out of one edge: its outline average sits inside the lens, i.e.
    // in the part that was removed. Measured on box(2,2,2) against a cylinder offset to
    // straddle the +X wall, the average of the remainder face's six boundary vertices is
    // (0.333, 0), which is 0.367 from the cylinder's axis and so INSIDE it — where the
    // face's own material is almost entirely outside. Five of the box's ten faces were
    // classified Inside, the ±Z remainders were dropped from the union, and the offered
    // face set had 38 one-sided edges. Same failure as the holed case, different geometry
    // producing it, so the candidate search below now serves both.
    if (holes.empty() && onMaterial(centroid)) return centroid;

    // Distance from a point to a ring, treated as a polyline. This is what turns "on the
    // material" into "SAFELY on the material", and the difference is not cosmetic.
    //
    // Every ring here is a CHORDAL POLYGON standing in for the face's real boundary, and
    // for a seam the real boundary is a circle. Between the polygon and the circle it
    // approximates lies a sliver — bounded by the polygon's inradius and its circumradius —
    // that is outside the polygon and inside the true curve. A point there passes
    // `onMaterial` and is nonetheless not on the face at all.
    //
    // MEASURED on box(2³) against sphere(r1.2) offset 0.1: the +X face's annulus got a
    // sample at radius 0.750609 from the seam centre, where the twelve-sided hole polygon
    // has inradius 0.743719 and the true seam circle has radius 0.793725. Outside the
    // polygon by seven thousandths, inside the circle by four hundredths. classifyPoint
    // then judged it against the SPHERE, not against the polygon, and reported the annulus
    // as touching the sphere — so selectFace treated it as a coincident-face pair and kept a
    // face whose material is entirely outside. Every seam edge gained a third user and the
    // sew refused; the intersection and difference of that pair returned empty.
    auto distToRing = [](const Vec3& p, const std::vector<Vec3>& ring) {
        double best = std::numeric_limits<double>::max();
        for (size_t i = 0; i < ring.size(); ++i) {
            const Vec3& a = ring[i];
            const Vec3& b = ring[(i + 1) % ring.size()];
            const Vec3 ab = sub(b, a);
            const double denom = dot(ab, ab);
            double t = (denom > 0.0) ? (dot(sub(p, a), ab) / denom) : 0.0;
            t = std::min(1.0, std::max(0.0, t));
            best = std::min(best, length(sub(p, add(a, scale(ab, t)))));
        }
        return best;
    };
    auto clearance = [&](const Vec3& p) {
        double c = distToRing(p, outerPts);
        for (const std::vector<Vec3>& h : holes) c = std::min(c, distToRing(p, h));
        return c;
    };

    // Candidates are generated in a FIXED order and the one with the greatest clearance
    // wins — ties keeping the earlier, so the answer stays reproducible and the
    // classification that consumes it stays deterministic. Taking the FIRST valid candidate
    // is what allowed a marginal one to be chosen while a comfortable one existed.
    //
    // A midpoint between an outer vertex and a hole vertex is generated first because it is
    // the candidate that survives the case which defeats every averaging scheme: a hole
    // concentric with its face, where the outline's average and the area-weighted
    // centroid are both the hole's centre.
    Vec3 best = centroid;
    double bestClear = -1.0;
    auto offer = [&](const Vec3& p) {
        if (!onMaterial(p)) return;
        const double c = clearance(p);
        if (c > bestClear) {
            bestClear = c;
            best = p;
        }
    };

    for (const Vec3& o : outerPts)
        for (const std::vector<Vec3>& h : holes)
            for (const Vec3& hp : h)
                offer({(o.x + hp.x) * 0.5f, (o.y + hp.y) * 0.5f, (o.z + hp.z) * 0.5f});

    // Then points drawn in from the outer boundary — an edge midpoint and a vertex,
    // each pulled part of the way toward the centroid.
    for (float t : {0.25f, 0.5f, 0.75f})
        for (size_t i = 0; i < outerPts.size(); ++i) {
            const Vec3& a = outerPts[i];
            const Vec3& b = outerPts[(i + 1) % outerPts.size()];
            const Vec3 edgeMid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f};
            offer({edgeMid.x + (centroid.x - edgeMid.x) * t,
                   edgeMid.y + (centroid.y - edgeMid.y) * t,
                   edgeMid.z + (centroid.z - edgeMid.z) * t});
            offer({a.x + (centroid.x - a.x) * t, a.y + (centroid.y - a.y) * t,
                   a.z + (centroid.z - a.z) * t});
        }

    return bestClear >= 0.0 ? best : centroid;  // nothing found → no worse than before
}

std::vector<std::vector<uint32_t>> Body::faceInnerLoopVertices(uint32_t faceId) const
{
    std::vector<std::vector<uint32_t>> out;
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return out;
    for (uint32_t loopId : m_faces[faceId].innerLoops) {
        if (loopId >= m_loops.size() || !m_loops[loopId].alive) continue;
        const uint32_t first = m_loops[loopId].first;
        if (first >= m_coedges.size()) continue;
        std::vector<uint32_t> ring;
        uint32_t w = first, guard = 0;
        do {
            const Coedge& x = m_coedges[w];
            if (x.edge >= m_edges.size()) break;
            ring.push_back(x.reversed ? m_edges[x.edge].v1 : m_edges[x.edge].v0);
            w = m_coedges[w].next;
            if (++guard > m_coedges.size() + 1) break;
        } while (w != first);
        if (ring.size() >= 3) out.push_back(std::move(ring));
    }
    return out;
}

uint32_t Body::splitFace(uint32_t faceId, uint32_t vA, uint32_t vB)
{
    return cutFaceBetween(faceId, vA, vB, nullptr);
}

uint32_t Body::cutFaceBetween(uint32_t faceId, uint32_t vA, uint32_t vB,
                              const Curve* explicitCurve)
{
    if (faceId >= m_faces.size() || vA == vB) return kInvalid;
    const uint32_t loopId = m_faces[faceId].outerLoop;
    if (loopId >= m_loops.size() || m_loops[loopId].first >= m_coedges.size()) return kInvalid;

    // Walk the outer loop: coedges in order + their start vertices.
    std::vector<uint32_t> ce, sv;
    {
        uint32_t w = m_loops[loopId].first, guard = 0;
        do {
            ce.push_back(w);
            const Coedge& x = m_coedges[w];
            sv.push_back(x.reversed ? m_edges[x.edge].v1 : m_edges[x.edge].v0);
            w = m_coedges[w].next;
            if (++guard > m_coedges.size() + 1) return kInvalid;
        } while (w != m_loops[loopId].first);
    }
    const int n = static_cast<int>(ce.size());
    if (n < 4) return kInvalid;  // need a non-adjacent diagonal

    int posA = -1, posB = -1;
    for (int i = 0; i < n; ++i) {
        if (sv[static_cast<size_t>(i)] == vA) posA = i;
        if (sv[static_cast<size_t>(i)] == vB) posB = i;
    }
    if (posA < 0 || posB < 0) return kInvalid;
    if (posA > posB) std::swap(posA, posB);
    if (posB - posA == 1 || (posA == 0 && posB == n - 1)) return kInvalid;  // adjacent

    const uint32_t vStart = sv[static_cast<size_t>(posA)];
    const uint32_t vEnd = sv[static_cast<size_t>(posB)];

    // Partition the loop's coedges into the two sub-loops (segment A: posA..posB-1;
    // segment B: posB..end, 0..posA-1).
    std::vector<uint32_t> segA, segB;
    for (int i = posA; i < posB; ++i) segA.push_back(ce[static_cast<size_t>(i)]);
    for (int i = posB; i < n; ++i) segB.push_back(ce[static_cast<size_t>(i)]);
    for (int i = 0; i < posA; ++i) segB.push_back(ce[static_cast<size_t>(i)]);

    // New cut edge (vStart -> vEnd). splitFace builds a straight Line chord;
    // imprintCurve supplies the intersection curve itself, its param range set to
    // reproduce the two endpoints so checkGeometry holds.
    const uint32_t curveId = static_cast<uint32_t>(m_curves.size());
    // DOUBLE: these are curve parameters. Narrowing them here re-rounded every arc an
    // imprint builds back to float, which is where the seam ring's float pi came from.
    double et0 = 0.0, et1 = 0.0;
    if (explicitCurve != nullptr) {
        m_curves.push_back(*explicitCurve);
        et0 = paramOnCurve(*explicitCurve, m_verts[vStart].point);
        et1 = paramOnCurve(*explicitCurve, m_verts[vEnd].point);
    } else {
        const Vec3 d = sub(m_verts[vEnd].point, m_verts[vStart].point);
        Curve cu;
        cu.kind = CurveKind::Line;
        cu.origin = m_verts[vStart].point;
        cu.dir = normalize(d);
        m_curves.push_back(cu);
        et1 = length(d);
    }
    const uint32_t edgeId = static_cast<uint32_t>(m_edges.size());
    {
        Edge e;
        e.curve = curveId;
        e.v0 = vStart;
        e.v1 = vEnd;
        e.t0 = et0;
        e.t1 = et1;
        m_edges.push_back(e);
    }

    // Face B + loop B (face A reuses faceId + its loop).
    const uint32_t faceB = static_cast<uint32_t>(m_faces.size());
    {
        Face f;
        f.surface = m_faces[faceId].surface;
        f.reversed = m_faces[faceId].reversed;
        f.shell = m_faces[faceId].shell;
        m_faces.push_back(f);
    }
    const uint32_t loopB = static_cast<uint32_t>(m_loops.size());
    {
        Loop l;
        l.face = faceB;
        l.outer = true;
        m_loops.push_back(l);
    }
    m_faces[faceB].outerLoop = loopB;

    // Diagonal coedges: dA closes loop A (vEnd->vStart, reversed on the new
    // edge), dB closes loop B (vStart->vEnd, forward). They are partners.
    const uint32_t dA = static_cast<uint32_t>(m_coedges.size());
    { Coedge x; x.edge = edgeId; x.reversed = true;  x.loop = loopId; m_coedges.push_back(x); }
    const uint32_t dB = static_cast<uint32_t>(m_coedges.size());
    { Coedge x; x.edge = edgeId; x.reversed = false; x.loop = loopB;  m_coedges.push_back(x); }
    m_coedges[dA].partner = dB;
    m_coedges[dB].partner = dA;

    for (uint32_t cb : segB) m_coedges[cb].loop = loopB;

    // Wire loop A: segA (in existing order) then dA back to the front.
    const uint32_t aFirst = segA.front(), aLast = segA.back();
    m_coedges[aLast].next = dA; m_coedges[dA].prev = aLast;
    m_coedges[dA].next = aFirst; m_coedges[aFirst].prev = dA;
    m_loops[loopId].first = aFirst;

    // Wire loop B: segB then dB back to the front.
    const uint32_t bFirst = segB.front(), bLast = segB.back();
    m_coedges[bLast].next = dB; m_coedges[dB].prev = bLast;
    m_coedges[dB].next = bFirst; m_coedges[bFirst].prev = dB;
    m_loops[loopB].first = bFirst;

    const uint32_t sh = m_faces[faceId].shell;
    if (sh < m_shells.size()) m_shells[sh].faces.push_back(faceB);
    m_edges[edgeId].coedge = dA;
    return faceB;
}

uint32_t Body::imprintCurve(uint32_t faceId, const Curve& curve, Tolerance tol,
                            const std::vector<Vec3>* ringPoints)
{
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return kInvalid;
    if (curve.kind != CurveKind::Line && curve.kind != CurveKind::Circle) return kInvalid;
    const uint32_t loopId = m_faces[faceId].outerLoop;
    if (loopId >= m_loops.size() || m_loops[loopId].first >= m_coedges.size()) return kInvalid;
    // Crossing test tolerance, proportioned to the face (unit-ish characteristic).
    const double eps = tol.at(1.f) * 10.f;

    // Walk the outer loop → ordered boundary vertices + their outgoing edges.
    std::vector<uint32_t> bVerts, bEdges;
    {
        uint32_t w = m_loops[loopId].first, guard = 0;
        do {
            const Coedge& x = m_coedges[w];
            bVerts.push_back(x.reversed ? m_edges[x.edge].v1 : m_edges[x.edge].v0);
            bEdges.push_back(x.edge);
            w = x.next;
            if (++guard > m_coedges.size() + 1) return kInvalid;
        } while (w != m_loops[loopId].first);
    }
    const size_t n = bVerts.size();

    // ── Circle imprint (arc bite crossing two distinct boundary edges) ──────────
    if (curve.kind == CurveKind::Circle) {
        // The circle must lie ON the face's surface, so the arc it cuts is a curve of
        // the face rather than something merely passing nearby. That holds for a
        // coplanar circle on a PLANE and for a latitude circle on a CYLINDER — the
        // seam a plane cuts on a cylinder, i.e. the cylinder-through-box case.
        const uint32_t sid = m_faces[faceId].surface;
        if (sid >= m_surfaces.size()) return kInvalid;
        const Surface& fsurf = m_surfaces[sid];
        const bool onPlane = fsurf.kind == SurfaceKind::Plane;
        if (!circleLiesOnSurface(fsurf, curve, eps)) return kInvalid;
        const Vec3 fn = fsurf.normal;
        // Whether a candidate point lies inside this face's boundary. A planar face
        // keeps the direct 3D test; a curved one is decided in the parameter domain,
        // where the boundary is an ordinary polygon.
        auto insideFace = [&](const Vec3& q, const std::vector<Vec3>& ring) {
            return onPlane ? pointInPlanarPolygon(q, ring, fn)
                           : pointInSurfacePatchUV(fsurf, q, ring);
        };

        // Subdivide an arc edge at every supplied ring point lying strictly inside its span.
        //
        // This is what makes an arc-bite seam SHARED. The two operands discretize the same
        // seam circle by their own topology: cutting a box face along the circle gives the
        // box ONE arc edge from entry to exit, while the cylinder's rim over that same
        // stretch is a chain of facet arcs. Measured at the z=-1 seam of box(2,2,2) against
        // an offset cylinder(r=0.5,16): 2 vertices on the box's side against 13 on the
        // cylinder's, so 1 edge facing 12. Not one of them can partner, which is the same
        // failure the latitude ring had at 8-against-16 and is fixed the same way — build on
        // the points the other operand already chose rather than on an independent guess.
        //
        // Splitting proceeds from the FAR end back, and each fraction is recomputed against
        // the edge's current range, because splitEdge keeps the near half under the same id
        // and appends the far one: taking the largest parameter first leaves every remaining
        // point inside the retained edge. Points already at an endpoint fall outside the
        // strict bounds and are skipped, which is what makes a second pass a no-op.
        // Returns how many splits it performed, so a caller can honour imprintCurve's
        // contract: kInvalid must mean the body was NOT touched.
        auto subdivideArcAt = [&](uint32_t arcEdge) -> uint32_t {
            if (ringPoints == nullptr || arcEdge >= m_edges.size()) return 0u;
            const Vec3 axis = normalize(curve.dir);
            const double spanT0 = m_edges[arcEdge].t0, spanT1 = m_edges[arcEdge].t1;
            if (std::abs(spanT1 - spanT0) <= 1e-12f) return 0u;
            constexpr double kTwoPiS = 6.283185307179586476925286766559;

            // Target PARAMETERS on the shared curve, not fractions: a fraction goes stale
            // the moment the edge is split, a parameter does not.
            std::vector<double> params;
            for (const Vec3& p : *ringPoints) {
                // Only points genuinely on this circle — a caller's list is a hint, and a
                // stray point would place a vertex off the curve.
                const Vec3 d = sub(p, curve.origin);
                const double axial = dot(d, axis);
                const double radial = length(sub(d, scale(axis, axial)));
                if (std::abs(axial) > eps || std::abs(radial - curve.radius) > eps) continue;
                double tp = paramOnCurve(curve, p);
                while (tp < std::min(spanT0, spanT1)) tp += kTwoPiS;
                while (tp > std::max(spanT0, spanT1)) tp -= kTwoPiS;
                const double f = (tp - spanT0) / (spanT1 - spanT0);
                if (f > 1e-5f && f < 1.f - 1e-5f) params.push_back(tp);
            }
            if (params.empty()) return 0u;
            // Split the FAR end first, so every remaining target stays inside the edge that
            // keeps the near end (and keeps this edge id).
            std::sort(params.begin(), params.end(), [&](double a, double b) {
                return (spanT1 > spanT0) ? (a > b) : (a < b);
            });
            uint32_t splits = 0;
            for (const double tp : params) {
                const double t0 = m_edges[arcEdge].t0, t1 = m_edges[arcEdge].t1;
                if (std::abs(t1 - t0) <= 1e-12f) break;
                const double f = (tp - t0) / (t1 - t0);  // recomputed against what is left
                if (!(f > 1e-5f && f < 1.f - 1e-5f)) continue;
                if (splitEdge(arcEdge, f, tol) != kInvalid) ++splits;
            }
            return splits;
        };

        // ── ALREADY-SEGMENTED GUARD, ahead of every circle path ─────────────────────
        // If this face's boundary already lies on this circle there is nothing left to
        // imprint: the face has been segmented along it, and it is on one side of it.
        //
        // This has to be checked BEFORE the crossing search, not after. A cylinder side
        // face spanning two latitudes has its own rim ON the latitude circle, and that
        // circle meets each of the face's uprights at the upright's ENDPOINT — which
        // Phase 4d taught the crossing solver to accept, correctly, because that is how a
        // neighbour's cut is recognised. Two endpoint crossings then look exactly like a
        // clean two-point arc bite, and the face gets "cut" from one end of its own rim to
        // the other, producing a zero-area lune bounded by the rim and a second copy of it.
        //
        // Nothing downstream catches that: the result passes checkIntegrity, checkGeometry,
        // isClosed and euler, because a degenerate face is topologically ordinary. What had
        // been preventing it was pure luck — the two rim corners were ADJACENT in the loop
        // and cutFaceBetween refuses adjacent vertices. The moment anything drops a vertex
        // between them (the generatrix imprint below does exactly that) the accident stops
        // protecting it and four flat faces appear on the cylinder. Measured that way round:
        // 4 flat cuts with the generatrix imprint enabled, 0 with it disabled — the
        // generatrix only exposed a hazard the circle path already had.
        {
            auto sameCircle = [&](const Curve& ex) {
                return ex.kind == CurveKind::Circle &&
                       std::abs(ex.radius - curve.radius) <= eps &&
                       length(sub(ex.origin, curve.origin)) <= eps &&
                       std::abs(dot(normalize(ex.dir), normalize(curve.dir))) >= 1.f - 1e-4f;
            };
            // Whether ANY edge of a loop lies on this circle. The whole ring is walked
            // rather than just its first edge, because after an arc bite the circle is one
            // edge among several and where it lands in the ring is an accident of the
            // loop's starting coedge.
            auto loopLiesOnCircle = [&](uint32_t l) {
                if (l >= m_loops.size() || !m_loops[l].alive) return false;
                const uint32_t first = m_loops[l].first;
                if (first >= m_coedges.size()) return false;
                uint32_t w = first, guard = 0;
                do {
                    const uint32_t ie = m_coedges[w].edge;
                    if (ie < m_edges.size()) {
                        const uint32_t icu = m_edges[ie].curve;
                        if (icu < m_curves.size() && sameCircle(m_curves[icu])) return true;
                    }
                    w = m_coedges[w].next;
                    if (++guard > m_coedges.size() + 1u) break;  // corrupt loop guard
                } while (w != first);
                return false;
            };
            // The outer boundary: covers the cylinder rim above, and the disk that the
            // interior-circle path splits off — which the driver re-offers the very seam
            // that created it, and which left unguarded would be handed a ring equal to its
            // own outline, every point exactly ON the polygon where containment is a coin
            // toss, segmenting inward for ever.
            if (loopLiesOnCircle(m_faces[faceId].outerLoop)) {
                // Already segmented — but not necessarily agreeing with the other operand on
                // HOW FINELY. Reconcile before leaving: the cut that produced this boundary
                // ran before the other operand had been cut at all (the mutual imprint does
                // one direction at a time), so at that moment there were no partner vertices
                // to build on. This is the same ordering trap the latitude ring hit, and the
                // same answer — the information arrives on a later round, so use it then.
                // Splitting at points that are already vertices is a no-op, which is what
                // lets this run every pass without accumulating.
                uint32_t w = m_loops[m_faces[faceId].outerLoop].first;
                const uint32_t first = w;
                uint32_t guard = 0;
                std::vector<uint32_t> onCircleEdges;
                do {
                    const uint32_t ie = m_coedges[w].edge;
                    if (ie < m_edges.size()) {
                        const uint32_t icu = m_edges[ie].curve;
                        if (icu < m_curves.size() && sameCircle(m_curves[icu]))
                            onCircleEdges.push_back(ie);
                    }
                    w = m_coedges[w].next;
                    if (++guard > m_coedges.size() + 1u) break;
                } while (w != first);
                uint32_t splits = 0;
                for (const uint32_t ie : onCircleEdges) splits += subdivideArcAt(ie);
                // Report the truth. No FACE was split, but if the seam was subdivided the
                // body changed, and imprintCurve's kInvalid means "nothing was touched" —
                // the mutual imprint's fixpoint reads it that way, and a test asserts it.
                // Returning this face when work was done also keeps the driver iterating
                // until the reconciliation settles, which it must: splitting at a point
                // that is already a vertex is a no-op, so the very next call returns 0 and
                // the fixpoint closes.
                return splits > 0 ? faceId : kInvalid;
            }
            // And the holes: the interior path leaves the circle exactly as interior as it
            // found it, so without this the driver — which imprints to a fixpoint and
            // re-offers every tool surface each pass, with an explosion guard watching the
            // FACE count that a ring barely changes — appended the same ring once per pass
            // until the iteration cap: a six-face box against a sixteen-segment cylinder
            // reached 1,599,992 vertices.
            for (uint32_t il : m_faces[faceId].innerLoops)
                if (loopLiesOnCircle(il)) return kInvalid;
        }

        // Boundary polygon (captured before any split) for the inside-arc test.
        std::vector<Vec3> poly;
        poly.reserve(n);
        for (uint32_t v : bVerts) poly.push_back(m_verts[v].point);

        // Circle × each Line boundary edge → crossings; need exactly two, on two
        // DISTINCT boundary points. (Same-edge bite and fully-interior circle →
        // inner-loop hole are follow-up increments.) A crossing within the face's
        // coincidence tolerance `eps` of an edge endpoint is resolved onto that
        // EXISTING vertex instead of split — mirroring the line-imprint path, so
        // a near-vertex crossing does not manufacture a near-duplicate vertex.
        struct CCross { bool isVertex; uint32_t vertex; uint32_t edge; double frac; };
        std::vector<CCross> cc;

        // Fractions along one boundary Line edge where the circle crosses it.
        //
        // On a PLANE the circle and the edge are coplanar, so |p(s) − centre| = r is a
        // transversal root and solving that quadratic is well conditioned.
        //
        // On a CYLINDER it is not. A latitude circle meets an axis-parallel edge at the
        // one axial level where that edge — which sits at exactly the cylinder's radius
        // along its whole length — touches the circle, so the same quadratic has a
        // DOUBLE root. A double root computed in float carries √ε error, not ε:
        // measured at 1.22e-4 on a unit-scale cylinder, a thousand times the
        // coincidence tolerance, which put the split vertex at z = −0.9999 for a circle
        // lying at z = −1 and made checkGeometry fail on the arc built through it.
        // A latitude circle IS the level set of the cylinder's axial parameter, so
        // solve for that level linearly, which is exact to the last bit float allows.
        auto edgeCrossings = [&](const Vec3& q0, const Vec3& q1, double fr[2]) -> int {
            if (onPlane) return circleSegmentFracs(q0, q1, curve.origin, curve.radius, fr, eps);
            // Stated generally, because a sphere has no axis to borrow: the circle IS the
            // section of this surface by the plane through its centre with normal
            // circle.dir, so an edge lying on the surface meets the circle exactly where it
            // crosses that plane. Solving the plane is linear and exact to the last bit,
            // where the distance equation has the double root described above. For a
            // cylinder's latitude circle circle.dir IS the axis, so this reduces to the
            // axial level set it replaces — same arithmetic, one less assumption.
            const Vec3 ax = normalize(curve.dir);
            const double a0 = dot(sub(q0, curve.origin), ax);
            const double a1 = dot(sub(q1, curve.origin), ax);
            const double d = a1 - a0;
            if (std::abs(d) <= 1e-12) return 0;  // edge lies in the circle's plane
            const double s = -a0 / d;
            // Accept a crossing AT an endpoint, not only strictly inside the edge. The
            // level routinely falls exactly on an existing boundary vertex: once one
            // face of a cylinder has been cut at this latitude, its neighbour's upright
            // edges are already split there, so every remaining edge meets the level at
            // s = 0 or s = 1. Rejecting those reported the face as UNCROSSED, it fell
            // through to the interior-hole case, and was refused — which is why a side
            // face crossed by two planes only ever got one of its two cuts. The
            // caller's snap-to-vertex step turns an endpoint fraction into a reuse of
            // that vertex, exactly as the planar path already did.
            const double len = length(sub(q1, q0));
            const double slack = (len > 0.0) ? (eps / len) : 0.0;
            if (!(s > -slack && s < 1.0 + slack)) return 0;
            const double sc = std::min(std::max(s, 0.0), 1.0);
            // Crossing the circle's PLANE only implies meeting the circle when the edge lies
            // on the surface. A cylinder's uprights and a sphere's arcs do; a straight chord
            // across a curved face would not, and would otherwise be split at a point that
            // is not on the curve at all. Cheap to verify, so verify rather than assume.
            const Vec3 hit = add(q0, scale(sub(q1, q0), sc));
            if (std::abs(length(sub(hit, curve.origin)) - curve.radius) > eps) return 0;
            fr[0] = sc;
            return 1;
        };
        // Where an ARC boundary edge meets the imprint circle, as a fraction of the arc's
        // own parameter span. Restricting the crossing hunt to Line edges was what kept a
        // circle off a SPHERE entirely: a cylinder's side face is bounded by two uprights
        // and two rim arcs, so the uprights carried it, but a sphere's lat-lon patch is
        // bounded by four ARCS and has no straight edge anywhere. Every crossing there is
        // on an arc, so a Line-only search finds none, the face is reported uncrossed, and
        // it falls through to the interior-hole case which refuses a non-planar face.
        //
        // Solved in closed form on the geometry, like its Line counterpart. The imprint
        // circle is the section of this surface by the plane through curve.origin with
        // normal curve.dir, and the boundary arc meets that plane where
        //     dot(arc(t) - curve.origin, n) = A + B·cos t + C·sin t = 0
        // with A the centre offset along n and (B, C) the arc's radius projected onto n
        // through its own frame. Writing that as R·cos(t - phi) = -A with R = hypot(B, C)
        // gives t = phi ± acos(-A/R): exact, and giving BOTH roots, since an arc can cross
        // a plane twice — which the two-crossing bite test downstream depends on.
        auto arcCrossings = [&](uint32_t e, double fr[2]) -> int {
            constexpr double kTwoPiA = 6.283185307179586476925286766559;
            const Curve& arc = m_curves[m_edges[e].curve];
            const double at0 = m_edges[e].t0, at1 = m_edges[e].t1;
            if (std::abs(at1 - at0) <= 1e-12) return 0;
            const Vec3 nrm = normalize(curve.dir);
            const Vec3 bi = cross(arc.dir, arc.ref);
            const double A = dot(sub(arc.origin, curve.origin), nrm);
            const double B = arc.radius * dot(arc.ref, nrm);
            const double C = arc.radius * dot(bi, nrm);
            const double R = std::sqrt(B * B + C * C);
            if (R <= 1e-12) {
                // The arc does not cross the cut plane transversally. Either it is parallel
                // to it and misses — nothing to do — or it lies IN it, and then the two are
                // coplanar circles whose meeting is not a plane crossing at all but an
                // ordinary circle-circle intersection solved in that shared plane.
                //
                // This is the boundary of every planar face that is bounded by arcs rather
                // than by straight edges: a cylinder's cap, a cone's base, a disk. Solving
                // it as a plane crossing reports NOTHING, so such a face was never cut.
                // MEASURED on two parallel cylinders — r1 and r0.7 with axes 1.0 apart —
                // the caps were offered their seam circle and refused it, staying two faces
                // throughout while the side walls cut correctly, and every operator returned
                // empty with 24 boundary edges left one-sided.
                if (std::abs(A) > eps) return 0;  // parallel plane, genuinely no meeting
                const Vec3 dc = sub(curve.origin, arc.origin);
                const double dd = length(sub(dc, scale(nrm, dot(dc, nrm))));
                if (dd <= 1e-12) return 0;  // concentric: coincident or never meeting
                if (dd > arc.radius + curve.radius || dd < std::abs(arc.radius - curve.radius))
                    return 0;
                const Vec3 uu = scale(dc, 1.0 / dd);
                const Vec3 ww = cross(nrm, uu);
                const double tt = (dd * dd + arc.radius * arc.radius -
                                   curve.radius * curve.radius) / (2.0 * dd);
                const double hh2 = arc.radius * arc.radius - tt * tt;
                if (hh2 < 0.0) return 0;
                const double hh = std::sqrt(hh2);
                const Vec3 mid = add(arc.origin, scale(uu, tt));
                const double arcLen0 = std::abs(at1 - at0) * arc.radius;
                const double slack0 = (arcLen0 > 0.0) ? (eps / arcLen0) : 0.0;
                int got = 0;
                for (const double sgn : {1.0, -1.0}) {
                    const Vec3 hit = add(mid, scale(ww, sgn * hh));
                    double tp = paramOnCurve(arc, hit);
                    while (tp < std::min(at0, at1)) tp += kTwoPiA;
                    while (tp > std::max(at0, at1)) tp -= kTwoPiA;
                    double sfrac = (tp - at0) / (at1 - at0);
                    if (!(sfrac > -slack0 && sfrac < 1.0 + slack0)) continue;
                    sfrac = std::min(std::max(sfrac, 0.0), 1.0);
                    if (got < 2) fr[got++] = sfrac;
                }
                if (got == 2 && std::abs(fr[0] - fr[1]) * arcLen0 <= eps) got = 1;
                return got;
            }
            const double cosArg = -A / R;
            if (cosArg < -1.0 || cosArg > 1.0) return 0;  // never reaches the plane
            const double phi = std::atan2(C, B);
            const double da = std::acos(std::min(1.0, std::max(-1.0, cosArg)));
            // Accept a crossing AT an endpoint, not only strictly inside the arc — the same
            // rule the Line branch above needs, and for the same reason, which is worth
            // stating because it is the second time it has had to be learned. On a sphere
            // EVERY boundary is an arc, so the moment one lat-lon patch is cut, its
            // neighbours' shared boundary arcs are already split at that crossing and the
            // circle meets them exactly at an endpoint. Demanding a strictly interior
            // fraction reports those neighbours UNCROSSED, so the seam stops at the first
            // face and comes out as disconnected bites instead of a ring. Measured on
            // box(2³)/sphere(r1.2,8,12): 6 isolated arcs per seam, 12 of whose 12 endpoints
            // were degree-1, versus a closed ring once the endpoint is admitted.
            const double arcLen = std::abs(at1 - at0) * arc.radius;
            const double slack = (arcLen > 0.0) ? (eps / arcLen) : 0.0;
            int found = 0;
            for (const double root : {phi + da, phi - da}) {
                double tp = root;
                while (tp < std::min(at0, at1)) tp += kTwoPiA;
                while (tp > std::max(at0, at1)) tp -= kTwoPiA;
                double s = (tp - at0) / (at1 - at0);
                if (!(s > -slack && s < 1.0 + slack)) continue;  // outside this arc's own span
                s = std::min(std::max(s, 0.0), 1.0);
                // Meeting the plane is only meeting the circle if the point is also at the
                // circle's radius. On one sphere that holds by construction; verified
                // rather than assumed, so a boundary arc lying on some OTHER surface can
                // never contribute a split at a point that is not on the curve.
                const double tc = at0 + s * (at1 - at0);
                if (std::abs(length(sub(arc.eval(tc), curve.origin)) - curve.radius) > eps)
                    continue;
                if (found < 2) fr[found++] = s;
            }
            return found;
        };

        for (size_t i = 0; i < n; ++i) {
            const uint32_t e = bEdges[i];
            if (e >= m_edges.size()) continue;
            const uint32_t cu = m_edges[e].curve;
            if (cu >= m_curves.size()) continue;
            const bool arcEdge = m_curves[cu].kind == CurveKind::Circle;
            if (!arcEdge && m_curves[cu].kind != CurveKind::Line) continue;
            const Vec3 p0 = m_verts[m_edges[e].v0].point;
            const Vec3 p1 = m_verts[m_edges[e].v1].point;
            // For an arc the fraction is measured in PARAMETER space (which is what
            // splitEdge consumes), so the endpoint-snap test below must compare against the
            // arc's own length rather than its chord.
            const double edgeLen = arcEdge
                                       ? std::abs(m_edges[e].t1 - m_edges[e].t0)
                                             * m_curves[cu].radius
                                       : length(sub(p1, p0));
            double fr[2];
            const int k = arcEdge ? arcCrossings(e, fr) : edgeCrossings(p0, p1, fr);

            for (int j = 0; j < k; ++j) {
                const double s = fr[j];
                // circleSegmentFracs measures s along the STORED edge (v0->v1);
                // map s=0 onto v0's boundary vertex regardless of coedge winding.
                const bool storedForward = (m_edges[e].v0 == bVerts[i]);
                const uint32_t nearVertex = storedForward ? bVerts[i] : bVerts[(i + 1) % n];
                const uint32_t farVertex = storedForward ? bVerts[(i + 1) % n] : bVerts[i];
                // Carry the source edge id even for a snapped-to-vertex crossing,
                // so the same-edge-bite guard below is self-contained (a vertex
                // and an interior crossing that both belong to edge `e` are a
                // degenerate same-edge bite, not a valid two-point arc).
                if (s * edgeLen <= eps)
                    cc.push_back({true, nearVertex, e, 0.0});
                else if ((1.f - s) * edgeLen <= eps)
                    cc.push_back({true, farVertex, e, 0.0});
                else
                    cc.push_back({false, kInvalid, e, s});
            }
        }
        // A crossing at a shared corner is reported once by each incident edge;
        // collapse those duplicate vertex crossings so the two-crossing test below
        // counts distinct boundary points, not edges.
        if (cc.size() > 1) {
            std::vector<CCross> uniq;
            uniq.reserve(cc.size());
            for (const CCross& x : cc) {
                bool dup = false;
                if (x.isVertex)
                    for (const CCross& y : uniq)
                        if (y.isVertex && y.vertex == x.vertex) { dup = true; break; }
                if (!dup) uniq.push_back(x);
            }
            cc = std::move(uniq);
        }

        constexpr double kTwoPi = 6.283185307179586476925286766559;

        // Selects, of the two arcs sharing the cut edge's endpoints, the one lying INSIDE
        // the face. cutFaceBetween sets the range from the raw endpoint angles, which
        // traces whichever arc the angles happen to bracket; both share the endpoints, so
        // checkGeometry holds either way and only this test tells them apart.
        auto keepArcInsideFace = [&](uint32_t cutEdge) {
            if (cutEdge >= m_edges.size() || poly.empty()) return;
            const double t0 = m_edges[cutEdge].t0, t1 = m_edges[cutEdge].t1;
            const double alt = (t1 > t0) ? (t1 - kTwoPi) : (t1 + kTwoPi);
            const Vec3 midA = curve.eval((t0 + t1) * 0.5);
            const Vec3 midB = curve.eval((t0 + alt) * 0.5);
            // Ask about BOTH candidates rather than testing one and flipping on failure.
            // The two are not independent — exactly one of them lies in the face — so a
            // single test that comes back "outside" is only evidence for the other one if
            // the test is trustworthy for this face. When it discriminates, it decides.
            // The face's own extent. Being inside it is a NECESSARY condition — an arc
            // lying in the face cannot leave the box that bounds the face's boundary — so
            // this is used twice below: to VETO a containment verdict that contradicts it,
            // and to break a tie the containment test cannot.
            Vec3 lo = poly[0], hi = poly[0];
            for (const Vec3& q : poly) {
                lo = {std::min(lo.x, q.x), std::min(lo.y, q.y), std::min(lo.z, q.z)};
                hi = {std::max(hi.x, q.x), std::max(hi.y, q.y), std::max(hi.z, q.z)};
            }
            auto inBoundaryBox = [&](const Vec3& q) {
                return q.x >= lo.x - eps && q.x <= hi.x + eps && q.y >= lo.y - eps &&
                       q.y <= hi.y + eps && q.z >= lo.z - eps && q.z <= hi.z + eps;
            };
            const bool boxA = inBoundaryBox(midA), boxB = inBoundaryBox(midB);

            // Ask about BOTH candidates rather than testing one and flipping on failure.
            // The two are not independent — exactly one of them lies in the face — so a
            // single test that comes back "outside" is only evidence for the other one if
            // the test is trustworthy for this face.
            //
            // And it is not always trustworthy. Containment on a curved face is decided in
            // the surface's (u,v) domain, and an analytic sphere's parametrisation is
            // SINGULAR at ±uAxis, where v is an atan2 with no value. Near that pole the test
            // does not merely become uncertain — it can be confidently WRONG. MEASURED on
            // box(2³) against sphere(r0.8) offset 0.3, where the seam sits at 61° of
            // latitude: of eight seam arcs, seven were decided correctly and one — on the
            // face mirroring one that was decided correctly, with identical spans — chose the
            // complement, 5.3716 rad against a true 0.9116.
            //
            // The veto is what catches that, and it is logic rather than a heuristic: the
            // box is a necessary condition, so a containment verdict the box contradicts
            // cannot be right. Vetoing it turns a confidently wrong answer into an
            // undecided one, which the tie-break below then resolves correctly.
            const bool inA = insideFace(midA, poly) && boxA;
            const bool inB = insideFace(midB, poly) && boxB;
            if (inA != inB) {
                if (!inA) m_edges[cutEdge].t1 = alt;
                return;
            }

            // The test could not tell them apart, which for a curved face is a real and
            // reachable condition rather than a paranoid branch. Containment on a curved
            // patch is decided in the surface's (u,v) domain, and an analytic sphere's
            // parametrisation is SINGULAR at ±uAxis: v is an atan2 that becomes undefined
            // there, so a patch near that pole has no well-formed (u,v) polygon and points
            // on either side of it can both read as outside. makeSphere puts uAxis on X
            // while its tessellation's grid poles are on Z, so the singularity sits in the
            // middle of ordinary patches near ±X — exactly where a box's ±X faces cut
            // their seam. Measured on box(2³)/sphere(r1.2, 8x12): of the 72 seam arcs, 71
            // discriminate cleanly and ONE — the patch straddling the parametric pole —
            // reports both candidates outside.
            //
            // What made that one silent is that both arcs share their endpoints, so the
            // wrong one still reproduces its own vertices and checkGeometry, checkIntegrity,
            // isClosed and Euler are all satisfied by the complement. It surfaces only much
            // later, as a seam that will not sew: the complement spans 96% of the circle
            // (6.0333 rad against the true 0.2499), swallowing ten ring vertices that
            // belong to its neighbours, and the reconcile pass then splits it at each of
            // them and manufactures ten duplicate vertices.
            //
            // Break the tie on the face's own extent. The arc lying in the face cannot
            // leave the bounding box of the face's boundary, while the complement — going
            // the long way round the circle — leaves it immediately. This is a coarser
            // question than containment, which is exactly why it survives the singularity:
            // it never evaluates the parametrisation. Verified against the discriminating
            // cases rather than assumed — on the same fixture it agrees with the (u,v) test
            // on all 71 of them, in both directions (65 keep, 6 flip), and resolves the 1
            // it cannot answer.
            if (boxA != boxB) {
                if (!boxA) m_edges[cutEdge].t1 = alt;
                return;
            }
            // Both still tied: keep the SHORTER arc. A cut between two boundary points of
            // one patch is the short way round far more often than not, and leaving the
            // range untouched here would mean keeping whichever arc the raw endpoint
            // angles happened to bracket — a coin toss rather than a decision.
            if (std::abs(alt - t0) < std::abs(t1 - t0)) m_edges[cutEdge].t1 = alt;
        };

        // ── SAME-EDGE ARC BITE ──────────────────────────────────────────────────────
        // The circle enters and leaves through the SAME boundary edge, cutting a lens off
        // it. This is not an exotic case: it is what an OFFSET cylinder does to the face it
        // pierces the moment its footprint reaches a side wall, so every offset
        // cylinder-through-box boolean depended on it and it was deferred.
        //
        // Why it needed its own path. The natural topology here is a two-sided face — the
        // arc plus the chord it cuts across the edge — and a loop of two coedges is
        // rejected by checkIntegrity, which requires three. That rule is stricter than a
        // B-rep strictly needs, but it is load-bearing for everything built on it, so the
        // fix is not to weaken it: split the chord at its MIDPOINT first. The bite is then
        // bounded by three edges (arc + two collinear chord halves), the two crossing
        // vertices are no longer adjacent in the loop — which is separately what
        // cutFaceBetween requires — and the extra vertex sits exactly on the original
        // straight boundary, so it adds a redundant vertex and not one micron of geometric
        // error. mergeCollinearEdges removes it later if a caller wants the minimal body.
        if (cc.size() == 2 && cc[0].edge == cc[1].edge) {
            // Both crossings must be interior to the edge. A bite that starts or ends ON an
            // existing corner is a different topology — the chord degenerates into part of
            // an adjacent edge — and is still deferred rather than half-handled.
            if (cc[0].isVertex || cc[1].isVertex) return kInvalid;
            const uint32_t e0 = cc[0].edge;
            if (e0 >= m_edges.size()) return kInvalid;
            double fA = cc[0].frac, fB = cc[1].frac;
            if (fA > fB) std::swap(fA, fB);
            // Parameter of the SECOND crossing on the shared curve, captured before the
            // first split rewrites this edge's range.
            const double et0 = m_edges[e0].t0, et1 = m_edges[e0].t1;
            const double tSecond = et0 + (et1 - et0) * fB;

            // splitEdge reuses e0 for the near half and appends the far half, so the second
            // crossing lands on that appended edge — whose id is the current edge count.
            const uint32_t eFar = static_cast<uint32_t>(m_edges.size());
            const uint32_t vA = splitEdge(e0, fA, tol);
            if (vA == kInvalid || eFar >= m_edges.size()) return kInvalid;

            // Re-express the second crossing as a fraction of the far half's ACTUAL range:
            // splitEdge floors the split away from either endpoint, so the realised first
            // split may not sit exactly where it was asked to.
            const double fT0 = m_edges[eFar].t0, fT1 = m_edges[eFar].t1;
            if (!(std::abs(fT1 - fT0) > 0.f)) return kInvalid;
            const double localB = (tSecond - fT0) / (fT1 - fT0);
            if (!(localB > 0.f && localB < 1.f)) return kInvalid;
            const uint32_t vB = splitEdge(eFar, localB, tol);
            if (vB == kInvalid || vB == vA) return kInvalid;

            // eFar now spans vA → vB: halve it so the two crossings are two edges apart.
            if (splitEdge(eFar, 0.5f, tol) == kInvalid) return kInvalid;

            const uint32_t ce = static_cast<uint32_t>(m_edges.size());  // the cut edge id
            const uint32_t newFace = cutFaceBetween(faceId, vA, vB, &curve);
            if (newFace == kInvalid) return kInvalid;
            keepArcInsideFace(ce);
            (void)subdivideArcAt(ce);  // match the other operand's discretization
            return newFace;
        }

        // Arc bite across TWO distinct boundary edges: the circle crosses the boundary at
        // two points (each a fresh edge split or an existing vertex) → split the face along
        // the arc between them.
        if (cc.size() == 2) {
            auto resolve = [&](const CCross& c) {
                return c.isVertex ? c.vertex : splitEdge(c.edge, c.frac, tol);
            };
            const uint32_t vA = resolve(cc[0]);
            const uint32_t vB = resolve(cc[1]);
            if (vA == kInvalid || vB == kInvalid || vA == vB) return kInvalid;

            const uint32_t ce = static_cast<uint32_t>(m_edges.size());  // the cut edge id
            const uint32_t newFace = cutFaceBetween(faceId, vA, vB, &curve);
            if (newFace == kInvalid) return kInvalid;
            keepArcInsideFace(ce);
            (void)subdivideArcAt(ce);  // match the other operand's discretization
            return newFace;
        }
        // The circle crosses the boundary MORE THAN TWICE. Everything above handles a
        // single bite, and anything else used to be refused — which quietly left the face
        // STRADDLING the other solid, the one state the imprint exists to eliminate. The
        // face is then classified whole, from one sample point, and every part of it that
        // belongs on the other side of the circle is lost with it.
        //
        // It is not an exotic configuration. A sphere pushed off-centre through a box cuts
        // the face it exits in a circle LARGER than the face's inradius but smaller than
        // its half-diagonal, so the circle leaves and re-enters through all four edges:
        // eight crossings, four arcs inside the face (one per corner) alternating with four
        // outside. Measured on box(2³) against sphere(r1.2) offset 0.5, the +X face came
        // out as a single 16-vertex face spanning the whole plane, classified Inside from
        // its centre and dropped entire — 24 boundary edges left with one face instead of
        // two, and all three operators empty. Concentric works only because there the
        // circle is smaller than the face and takes the fully-interior path instead.
        //
        // Every cut is made HERE rather than one per call, because a face that has been cut
        // once carries the arc on its boundary and the already-segmented guard above — which
        // is right to stop a face being bitten along its own rim — would refuse the rest.
        if (cc.size() > 2 && (cc.size() % 2) == 0) {
            // Where a crossing sits in 3D, which is what orders the crossings around the
            // circle. A fraction is along the boundary EDGE; the ordering has to be along
            // the CURVE, and the two are unrelated once more than one edge is involved.
            auto crossPoint = [&](const CCross& c) -> Vec3 {
                if (c.isVertex) return m_verts[c.vertex].point;
                const Edge& ed = m_edges[c.edge];
                const uint32_t ecu = ed.curve;
                if (ecu < m_curves.size() && m_curves[ecu].kind == CurveKind::Circle)
                    return m_curves[ecu].eval(ed.t0 + c.frac * (ed.t1 - ed.t0));
                const Vec3 a = m_verts[ed.v0].point, b = m_verts[ed.v1].point;
                return add(a, scale(sub(b, a), c.frac));
            };

            const size_t n = cc.size();
            std::vector<double> prm(n);
            for (size_t i = 0; i < n; ++i) prm[i] = paramOnCurve(curve, crossPoint(cc[i]));
            std::vector<size_t> order(n);
            for (size_t i = 0; i < n; ++i) order[i] = i;
            std::sort(order.begin(), order.end(),
                      [&](size_t a, size_t b) { return prm[a] < prm[b]; });

            // Consecutive crossings around the circle bound an arc that is wholly inside
            // the face or wholly outside it, alternating. The inside ones are the cuts.
            std::vector<std::pair<size_t, size_t>> bites;
            for (size_t i = 0; i < n; ++i) {
                const size_t ia = order[i], ib = order[(i + 1) % n];
                const double pa = prm[ia];
                const double pb = (prm[ib] > pa) ? prm[ib] : prm[ib] + kTwoPi;
                if (!insideFace(curve.eval((pa + pb) * 0.5), poly)) continue;
                // Both ends on ONE edge is the same-edge lens, which needs the midpoint
                // split the dedicated path above performs. Left to it rather than
                // half-handled here.
                if (!cc[ia].isVertex && !cc[ib].isVertex && cc[ia].edge == cc[ib].edge) continue;
                bites.emplace_back(ia, ib);
            }
            if (bites.empty()) return kInvalid;

            // Resolve every crossing to a vertex BEFORE any face is cut. Several crossings
            // routinely land on the SAME boundary edge — the eight-crossing case above puts
            // two on each side of the square — and a fraction goes stale the moment that
            // edge is split. So each is captured as an absolute parameter on the edge's own
            // curve, and the edge is split from its FAR end inward, which leaves every
            // remaining (lower) parameter inside the half that keeps the edge's id.
            std::vector<uint32_t> vid(n, kInvalid);
            std::vector<double> tAbs(n, 0.0);
            for (size_t i = 0; i < n; ++i) {
                if (cc[i].isVertex || cc[i].edge >= m_edges.size()) continue;
                const Edge& ed = m_edges[cc[i].edge];
                tAbs[i] = ed.t0 + cc[i].frac * (ed.t1 - ed.t0);
            }
            std::vector<size_t> toSplit;
            for (size_t i = 0; i < n; ++i)
                if (!cc[i].isVertex) toSplit.push_back(i);
            std::sort(toSplit.begin(), toSplit.end(), [&](size_t a, size_t b) {
                if (cc[a].edge != cc[b].edge) return cc[a].edge < cc[b].edge;
                return cc[a].frac > cc[b].frac;  // far end of a shared edge first
            });
            for (const size_t i : toSplit) {
                const uint32_t e = cc[i].edge;
                if (e >= m_edges.size()) return kInvalid;
                const double a0 = m_edges[e].t0, a1 = m_edges[e].t1;
                if (!(std::abs(a1 - a0) > 0.0)) return kInvalid;
                const double f = (tAbs[i] - a0) / (a1 - a0);
                if (!(f > 0.0 && f < 1.0)) return kInvalid;
                vid[i] = splitEdge(e, f, tol);
                if (vid[i] == kInvalid) return kInvalid;
            }
            for (size_t i = 0; i < n; ++i)
                if (cc[i].isVertex) vid[i] = cc[i].vertex;

            // Does this face's outer loop carry both endpoints? After the first cut there
            // are two faces and each later bite belongs to exactly one of them.
            auto loopCarries = [&](uint32_t f, uint32_t va, uint32_t vb) {
                if (f >= m_faces.size() || !m_faces[f].alive) return false;
                const uint32_t l = m_faces[f].outerLoop;
                if (l >= m_loops.size() || m_loops[l].first >= m_coedges.size()) return false;
                bool ga = false, gb = false;
                const uint32_t first = m_loops[l].first;
                uint32_t w = first, guard = 0;
                do {
                    const uint32_t e = m_coedges[w].edge;
                    if (e < m_edges.size()) {
                        for (const uint32_t v : {m_edges[e].v0, m_edges[e].v1}) {
                            if (v == va) ga = true;
                            if (v == vb) gb = true;
                        }
                    }
                    w = m_coedges[w].next;
                    if (++guard > m_coedges.size() + 1u) break;
                } while (w != first);
                return ga && gb;
            };

            std::vector<uint32_t> pieces{faceId};
            uint32_t produced = kInvalid;
            for (const auto& [ia, ib] : bites) {
                const uint32_t vA = vid[ia], vB = vid[ib];
                if (vA == kInvalid || vB == kInvalid || vA == vB) continue;
                uint32_t target = kInvalid;
                for (const uint32_t f : pieces)
                    if (loopCarries(f, vA, vB)) { target = f; break; }
                if (target == kInvalid) continue;
                const uint32_t ce = static_cast<uint32_t>(m_edges.size());
                const uint32_t nf = cutFaceBetween(target, vA, vB, &curve);
                if (nf == kInvalid) continue;
                // The arc is chosen against the ORIGINAL boundary, which is the polygon the
                // cut has to lie inside; the sub-faces are only its pieces.
                keepArcInsideFace(ce);
                (void)subdivideArcAt(ce);
                pieces.push_back(nf);
                produced = nf;
            }
            return produced;  // kInvalid if nothing could be cut — the caller treats that
                              // as "untouched", which it is.
        }
        if (!cc.empty()) return kInvalid;  // odd crossing count → deferred

        // Fully-interior circle → the face is SEGMENTED along it: the circle becomes an
        // inner loop of this face and the disk it encloses becomes a face of its own,
        // the two sharing the ring edge for edge. PLANE only: a latitude circle wraps
        // its whole cylinder, so it always leaves the patch and can never be interior
        // to one — and its centre lies on the axis, which is not a point of the surface
        // at all, so the interior test below is meaningless there.
        if (!onPlane) return kInvalid;

        if (!pointInPlanarPolygon(curve.origin, poly, fn)) return kInvalid;

        // The ring's DISCRETIZATION. A hole ring used to be built at a fixed eight
        // segments, which is fine in isolation and wrong the moment the ring is a
        // boolean seam: the other operand discretizes the SAME circle by its own
        // topology, and a sixteen-segment cylinder therefore met an eight-segment hole.
        // Both rings were the same circle — the eight vertices coincided with eight of
        // the sixteen to 1.7e-7 — but each hole edge spanned two of the cylinder's, so
        // no edge could ever find a partner and the sew could not close. When the
        // caller knows the other operand's vertices on this circle it passes them, and
        // the ring is built on exactly those points so the two sides agree by
        // construction rather than by luck. Absent that, the old uniform ring stands.
        std::vector<Vec3> ring;
        if (ringPoints != nullptr && ringPoints->size() >= 3) {
            // Accept only points genuinely on this circle; a caller's list is a hint,
            // not an authority, and a stray point would put a vertex off the curve and
            // break checkGeometry.
            for (const Vec3& p : *ringPoints) {
                const Vec3 d = sub(p, curve.origin);
                const double axial = dot(d, normalize(curve.dir));
                const double radial = length(sub(d, scale(normalize(curve.dir), axial)));
                if (std::abs(axial) <= eps && std::abs(radial - curve.radius) <= eps)
                    ring.push_back(p);
            }
            if (ring.size() < 3) ring.clear();
        }
        if (ring.empty()) {
            // A caller that passes a (possibly empty) ring list is coordinating this
            // circle across two operands, and committing to a resolution before the
            // partner's vertices exist is what produced the 8-against-16 mismatch. So
            // refuse: the caller runs another round once the other side is imprinted.
            // A caller that passes nothing is imprinting a lone body and a uniform ring
            // is the only sensible answer — that path is unchanged.
            if (ringPoints != nullptr) return kInvalid;
            constexpr uint32_t kUniform = 8;  // arc segments when no partner is involved
            for (uint32_t k = 0; k < kUniform; ++k)
                ring.push_back(curve.eval(kTwoPi * static_cast<float>(k) / kUniform));
        }
        // Order the ring by angle on the circle so consecutive points bound a forward
        // arc, then keep the parameters monotonically increasing across the ring so
        // every edge's [t0,t1] is a forward sweep that reproduces its own endpoints.
        std::sort(ring.begin(), ring.end(), [&](const Vec3& a, const Vec3& b) {
            return paramOnCurve(curve, a) < paramOnCurve(curve, b);
        });
        const uint32_t K = static_cast<uint32_t>(ring.size());
        std::vector<double> tAt(K);
        for (uint32_t k = 0; k < K; ++k) tAt[k] = paramOnCurve(curve, ring[k]);
        for (const Vec3& p : ring)
            if (!pointInPlanarPolygon(p, poly, fn)) return kInvalid;

        // ORIENTATION. The ring runs in increasing-parameter order, which turns either
        // way about this face's normal depending only on where the seam circle's axis
        // happens to point: a plane cutting a cylinder yields ONE circle per level, and
        // its axis agrees with the normal of the face above it and opposes the face
        // below. Assuming a direction therefore gets one of any two such rings backwards
        // — an inner loop wound WITH the outer boundary bounds a second outer region
        // instead of an opening, and the disk below would come out inside-out. So read
        // the convention off the face actually being cut: whichever way its own outer
        // loop turns about fn is outer-like, the ring bounding the opening must turn the
        // other way, and the disk's own boundary the same way.
        auto turnAboutNormal = [&](const std::vector<Vec3>& r) {
            Vec3 acc{0.f, 0.f, 0.f};  // Newell: 2·area·n, independent of the origin
            for (size_t i = 0; i < r.size(); ++i)
                acc = add(acc, cross(r[i], r[(i + 1) % r.size()]));
            return dot(acc, fn);
        };
        const bool ringForwardIsOuterLike =
            (turnAboutNormal(ring) > 0.f) == (turnAboutNormal(poly) > 0.f);

        // One shared Circle curve + K arc edges, each traversed TWICE: once by this
        // face's new inner loop and once by the disk face's outer loop. Segmenting a
        // face is not the same as opening it — the disk is still part of the solid, and
        // an imprint that removed it would leave the shell with a boundary. That is not
        // a cosmetic difference: classifyPoint counts ray crossings of the tessellated
        // shell, so a shell with an opening classifies points BEHIND that opening as
        // inside, and the boolean's own classification of the other operand's faces
        // silently went wrong (measured on a cylinder driven through a box: five of the
        // cylinder's sixteen faces a whole unit clear of the box came back Inside).
        // Beyond that, the material inside the ring is what caps the intersection — with
        // the disk discarded, box ∩ cylinder could never be closed at all.
        const uint32_t cid = static_cast<uint32_t>(m_curves.size());
        m_curves.push_back(curve);
        const uint32_t v0 = static_cast<uint32_t>(m_verts.size());
        for (uint32_t k = 0; k < K; ++k) {
            Vertex vt;
            vt.point = ring[k];
            m_verts.push_back(vt);
        }
        const uint32_t e0 = static_cast<uint32_t>(m_edges.size());
        for (uint32_t k = 0; k < K; ++k) {
            Edge e;
            e.curve = cid;
            e.v0 = v0 + k;
            e.v1 = v0 + (k + 1) % K;
            e.t0 = tAt[k];
            // The closing edge wraps, so its end parameter is the first point's angle
            // one full turn on; every other edge ends at its successor's angle.
            e.t1 = (k + 1 == K) ? tAt[0] + kTwoPi : tAt[k + 1];
            m_edges.push_back(e);
        }

        // This face's inner loop + the disk face and its outer loop.
        const uint32_t innerLoopId = static_cast<uint32_t>(m_loops.size());
        {
            Loop l;
            l.face = faceId;
            l.outer = false;
            m_loops.push_back(l);
        }
        const uint32_t diskFace = static_cast<uint32_t>(m_faces.size());
        {
            Face f;  // the disk lies in the parent's surface, oriented the same way
            f.surface = m_faces[faceId].surface;
            f.reversed = m_faces[faceId].reversed;
            f.shell = m_faces[faceId].shell;
            m_faces.push_back(f);
        }
        const uint32_t diskLoopId = static_cast<uint32_t>(m_loops.size());
        {
            Loop l;
            l.face = diskFace;
            l.outer = true;
            m_loops.push_back(l);
        }
        m_faces[diskFace].outerLoop = diskLoopId;

        // A closed coedge ring over the K arc edges, traversing each either FORWARD
        // (V_k → V_{k+1}, increasing parameter) or backward. Coedge index k is always
        // the one on arc edge e0+k, so the two rings partner edge for edge.
        auto buildArcRing = [&](uint32_t ownerLoop, bool forward) {
            const uint32_t c0 = static_cast<uint32_t>(m_coedges.size());
            for (uint32_t k = 0; k < K; ++k) {
                Coedge c;
                c.edge = e0 + k;
                c.reversed = !forward;
                c.loop = ownerLoop;
                // Forward, CE_k ends at V_{k+1} so its successor is CE_{k+1}; backward,
                // CE_k runs V_{k+1}→V_k so its successor is CE_{k−1}.
                c.next = c0 + (forward ? (k + 1) % K : (k + K - 1) % K);
                c.prev = c0 + (forward ? (k + K - 1) % K : (k + 1) % K);
                m_coedges.push_back(c);
            }
            m_loops[ownerLoop].first = c0;
            for (uint32_t k = 0; k < K; ++k)  // a vertex points at a coedge STARTING there
                m_verts[forward ? (v0 + k) : (v0 + (k + 1) % K)].coedge = c0 + k;
            return c0;
        };
        const uint32_t innerC0 = buildArcRing(innerLoopId, !ringForwardIsOuterLike);
        const uint32_t diskC0 = buildArcRing(diskLoopId, ringForwardIsOuterLike);
        for (uint32_t k = 0; k < K; ++k) {
            m_coedges[innerC0 + k].partner = diskC0 + k;
            m_coedges[diskC0 + k].partner = innerC0 + k;
            m_edges[e0 + k].coedge = innerC0 + k;
        }
        m_faces[faceId].innerLoops.push_back(innerLoopId);
        const uint32_t sh = m_faces[faceId].shell;
        if (sh < m_shells.size()) m_shells[sh].faces.push_back(diskFace);
        return diskFace;
    }

    // ── Line imprint ────────────────────────────────────────────────────────────
    // The planar face's normal — the plane in which the exact in-plane straddle
    // (orient3D) decides edge/line crossings.
    const uint32_t lineSid = m_faces[faceId].surface;
    const Vec3 faceNormal =
        (lineSid < m_surfaces.size()) ? m_surfaces[lineSid].normal : Vec3{0.f, 0.f, 1.f};
    // Perpendicular distance of a point to the (infinite) imprint line.
    auto distToLine = [&](const Vec3& p) {
        const Vec3 w = sub(p, curve.origin);
        return length(sub(w, scale(curve.dir, dot(w, curve.dir))));
    };

    // A boundary crossing is EITHER an existing boundary vertex the line passes
    // through, OR an interior point on a boundary edge. Recognising the vertex
    // case is essential for repeated/mutual imprinting: a neighbouring face's
    // imprint drops a vertex on the shared edge exactly where a later cut line
    // meets this face, so that meeting point is a vertex, not an edge interior.
    std::vector<bool> vOnLine(n, false);
    for (size_t i = 0; i < n; ++i) vOnLine[i] = distToLine(m_verts[bVerts[i]].point) <= eps;

    struct Cross { bool isVertex; uint32_t vertex; uint32_t edge; double frac; };
    std::vector<Cross> crossings;
    for (size_t i = 0; i < n; ++i) {
        if (vOnLine[i]) {
            crossings.push_back({true, bVerts[i], kInvalid, 0.0});
            continue;
        }
        // Interior crossing on edge i (skip if its far endpoint is on the line —
        // that crossing is already represented by the vertex case).
        if (vOnLine[(i + 1) % n]) continue;
        const uint32_t e = bEdges[i];
        if (e >= m_edges.size()) continue;
        const uint32_t cu = m_edges[e].curve;
        if (cu >= m_curves.size()) continue;
        double s = 0.0;
        if (m_curves[cu].kind == CurveKind::Circle) {
            // The boundary edge is an ARC. Skipping these was why a generatrix could not be
            // imprinted onto a cylindrical face at all — and that is the whole reason the
            // two operands never shared a vertical seam. Such a face is bounded by two rim
            // arcs and two uprights, and the generatrix runs PARALLEL to the uprights, so
            // the only crossings it can possibly have are on the arcs. The box's side wall
            // was cut along the two generatrices while the cylinder kept its facet
            // boundaries, and the sew had nothing to pair.
            //
            // Solve it on the geometry the arc provides rather than by a generic distance
            // equation, for the same reason a latitude crossing is solved on the cylinder's
            // axial parameter: the line meets the arc's PLANE in exactly one point, and that
            // is transversal and well conditioned. Find it, confirm it is on the circle, and
            // read off its parameter.
            const Curve& arc = m_curves[cu];
            const Vec3 aAxis = normalize(arc.dir);
            const double denom = dot(curve.dir, aAxis);
            if (std::abs(denom) <= 1e-12f) continue;  // line parallel to the arc's plane
            const double tHit = dot(sub(arc.origin, curve.origin), aAxis) / denom;
            const Vec3 hit = add(curve.origin, scale(curve.dir, tHit));
            if (std::abs(length(sub(hit, arc.origin)) - arc.radius) > eps) continue;  // misses
            const double at0 = m_edges[e].t0, at1 = m_edges[e].t1;
            if (std::abs(at1 - at0) <= 1e-12f) continue;
            constexpr double kTwoPiA = 6.283185307179586476925286766559;
            double tp = paramOnCurve(arc, hit);
            while (tp < std::min(at0, at1)) tp += kTwoPiA;
            while (tp > std::max(at0, at1)) tp -= kTwoPiA;
            s = (tp - at0) / (at1 - at0);
            // Strictly inside this arc's span, with no slack: a crossing AT an endpoint is
            // the whole-loop `vOnLine` case above, which reports it once as a vertex, and
            // allowing slack here would let both arcs meeting at that endpoint each report
            // it. The eps snap below still pulls a genuinely interior crossing that lands
            // near an endpoint onto that vertex.
            if (!(s > 0.f && s < 1.f)) continue;
        } else if (m_curves[cu].kind == CurveKind::Line) {
            if (!segmentLineCrossing(m_verts[m_edges[e].v0].point, m_verts[m_edges[e].v1].point,
                                     curve, faceNormal, eps, s))
                continue;
        } else {
            continue;  // NURBS boundary edge — not analytically crossed here
        }

        // s is the fraction along the STORED edge (v0->v1), which may run either
        // direction relative to the loop walk (bVerts[i] -> bVerts[i+1]) depending
        // on this coedge's winding. Map s back onto whichever boundary vertex it
        // is closest to in absolute terms.
        const bool storedForward = (m_edges[e].v0 == bVerts[i]);
        const uint32_t nearVertex = storedForward ? bVerts[i] : bVerts[(i + 1) % n];
        const uint32_t farVertex = storedForward ? bVerts[(i + 1) % n] : bVerts[i];
        const double edgeLen = length(sub(m_verts[m_edges[e].v1].point, m_verts[m_edges[e].v0].point));

        // Snap a crossing that lands within `eps` of one of the edge's own
        // endpoints onto that EXISTING vertex instead of splitting. Without this,
        // a crossing whose true position is a hair past a vertex (but not close
        // enough for the whole-loop `vOnLine` scan above, e.g. because that scan's
        // target — the OTHER operand's matching seam vertex — has not yet been
        // imprinted here) manufactures a fresh, near-duplicate vertex a few ulps
        // to a few tenths of a percent of the edge away from the real corner.
        // Two operands computing that same physical crossing independently then
        // disagree on where it sits, so their seam polylines cannot pair up
        // coedge-for-coedge and the sewn shell is left open. Reusing the
        // pre-existing vertex when the crossing is within the same coincidence
        // tolerance already used for the whole-loop scan (`eps`, itself
        // `tol.at(1.f) * 10`, i.e. scaled off this face's characteristic size)
        // keeps both operands landing on the identical point whenever they should.
        if (s * edgeLen <= eps) {
            crossings.push_back({true, nearVertex, kInvalid, 0.0});
        } else if ((1.f - s) * edgeLen <= eps) {
            crossings.push_back({true, farVertex, kInvalid, 0.0});
        } else {
            crossings.push_back({false, kInvalid, e, s});
        }
    }
    if (crossings.size() != 2) return kInvalid;  // need a clean entry + exit

    // Resolve each crossing to a vertex (splitting the edge where interior). The
    // two edges are distinct, so splitting one leaves the other's frac valid.
    auto resolve = [&](const Cross& c) { return c.isVertex ? c.vertex : splitEdge(c.edge, c.frac, tol); };
    const uint32_t vA = resolve(crossings[0]);
    const uint32_t vB = resolve(crossings[1]);
    // vA == vB is the within-eps corner-clip case: both crossings snapped to the
    // SAME existing boundary vertex (the line grazes a single corner). That is a
    // tolerance-correct no-op, not an error — imprintMutually treats kInvalid as
    // "this tool face did not split this face" and moves on, so nothing is lost.
    if (vA == kInvalid || vB == kInvalid || vA == vB) return kInvalid;

    // Cut the face between the two crossing vertices with the imprint curve itself.
    return cutFaceBetween(faceId, vA, vB, &curve);
}

bool Body::joinEdges(uint32_t nv) { return joinEdgesImpl(nv, /*requireSameCurve=*/true, {}); }

bool Body::joinEdgesImpl(uint32_t nv, bool requireSameCurve, Tolerance tol)
{
    const uint32_t C = static_cast<uint32_t>(m_coedges.size());
    if (nv >= m_verts.size() || !m_verts[nv].alive) return false;

    auto startV = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v1 : m_edges[e.edge].v0; };
    auto endV   = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v0 : m_edges[e.edge].v1; };

    // The two live edges incident to nv (must be exactly two).
    uint32_t e1 = kInvalid, e2 = kInvalid;
    for (uint32_t e = 0; e < m_edges.size(); ++e) {
        if (!m_edges[e].alive) continue;
        if (m_edges[e].v0 == nv || m_edges[e].v1 == nv) {
            if (e1 == kInvalid) e1 = e;
            else if (e2 == kInvalid) e2 = e;
            else return false;  // degree > 2
        }
    }
    if (e1 == kInvalid || e2 == kInvalid) return false;

    // Non-nv endpoints.
    const uint32_t a1 = (m_edges[e1].v0 == nv) ? m_edges[e1].v1 : m_edges[e1].v0;
    const uint32_t a2 = (m_edges[e2].v0 == nv) ? m_edges[e2].v1 : m_edges[e2].v0;
    if (a1 == a2) return false;  // would collapse to a duplicate/degenerate edge

    // Parameters of a1 / a2 in the SURVIVOR (e1) curve's frame. When the two
    // edges already share a curve (joinEdges, e.g. re-joining a split arc), use
    // the stored params directly. Otherwise (mergeCollinearEdges) the two edges
    // must be COLLINEAR Lines — verify and project the far endpoints onto e1's
    // line to get their params.
    double pa1, pa2;
    if (m_edges[e1].curve == m_edges[e2].curve) {
        pa1 = (m_edges[e1].v0 == nv) ? m_edges[e1].t1 : m_edges[e1].t0;
        pa2 = (m_edges[e2].v0 == nv) ? m_edges[e2].t1 : m_edges[e2].t0;
    } else {
        if (requireSameCurve) return false;
        const uint32_t cid1 = m_edges[e1].curve, cid2 = m_edges[e2].curve;
        if (cid1 >= m_curves.size() || cid2 >= m_curves.size()) return false;
        const Curve& cs = m_curves[cid1];
        const Curve& co = m_curves[cid2];
        if (cs.kind != CurveKind::Line || co.kind != CurveKind::Line) return false;
        if (std::abs(dot(cs.dir, co.dir)) < 1.f - 1e-4f) return false;  // not parallel
        const double dTol = tol.at(1.f) * 10.f;
        auto onLine = [&](uint32_t v) {
            const Vec3 w = sub(m_verts[v].point, cs.origin);
            return length(sub(w, scale(cs.dir, dot(w, cs.dir)))) <= dTol;
        };
        if (!onLine(a2)) return false;  // e2 not on e1's supporting line
        auto param = [&](uint32_t v) { return dot(sub(m_verts[v].point, cs.origin), cs.dir); };
        pa1 = param(a1);
        pa2 = param(a2);
    }

    const uint32_t vLow  = (pa1 < pa2) ? a1 : a2;
    const uint32_t vHigh = (pa1 < pa2) ? a2 : a1;
    const double    tLow  = (pa1 < pa2) ? pa1 : pa2;
    const double    tHigh = (pa1 < pa2) ? pa2 : pa1;

    // Merge pairs: each coedge that ENDS at nv (over e1 or e2), with its next
    // (which starts at nv over the other edge). One pair per incident face.
    struct Pair { uint32_t ceIn, ceOut, sIn; };
    std::vector<Pair> pairs;
    for (uint32_t c = 0; c < C; ++c) {
        if (!m_coedges[c].alive) continue;
        const uint32_t ed = m_coedges[c].edge;
        if (ed != e1 && ed != e2) continue;
        if (endV(c) != nv) continue;
        const uint32_t ceOut = m_coedges[c].next;
        if (ceOut >= C || !m_coedges[ceOut].alive || startV(ceOut) != nv) return false;
        pairs.push_back({c, ceOut, startV(c)});
    }
    if (pairs.empty() || pairs.size() > 2) return false;

    const uint32_t survivor = e1;
    std::vector<uint32_t> merged;
    for (const Pair& pr : pairs) {
        const uint32_t on = m_coedges[pr.ceOut].next;
        m_coedges[pr.ceIn].edge = survivor;
        m_coedges[pr.ceIn].reversed = (pr.sIn == vHigh);
        m_coedges[pr.ceIn].next = on;
        if (on < C) m_coedges[on].prev = pr.ceIn;
        const uint32_t lp = m_coedges[pr.ceIn].loop;
        if (lp < m_loops.size() && m_loops[lp].first == pr.ceOut) m_loops[lp].first = pr.ceIn;
        m_coedges[pr.ceOut].alive = false;
        merged.push_back(pr.ceIn);
    }

    // Survivor edge now spans vLow -> vHigh over [tLow, tHigh].
    m_edges[survivor].v0 = vLow;
    m_edges[survivor].v1 = vHigh;
    m_edges[survivor].t0 = tLow;
    m_edges[survivor].t1 = tHigh;
    m_edges[survivor].coedge = merged[0];

    if (merged.size() == 2) {
        m_coedges[merged[0]].partner = merged[1];
        m_coedges[merged[1]].partner = merged[0];
    } else {
        m_coedges[merged[0]].partner = kInvalid;
    }

    // Tombstone the removed vertex + edge.
    m_edges[e2].alive = false;
    m_verts[nv].alive = false;

    // Re-root the surviving endpoints onto a live outgoing coedge.
    auto reroot = [&](uint32_t x) {
        const uint32_t cur = m_verts[x].coedge;
        if (cur < C && m_coedges[cur].alive && startV(cur) == x) return;
        m_verts[x].coedge = kInvalid;
        for (uint32_t c = 0; c < static_cast<uint32_t>(m_coedges.size()); ++c)
            if (m_coedges[c].alive && startV(c) == x) { m_verts[x].coedge = c; break; }
    };
    reroot(vLow);
    reroot(vHigh);
    return true;
}

bool Body::mergeFaces(uint32_t edgeId)
{
    const uint32_t C = static_cast<uint32_t>(m_coedges.size());
    if (edgeId >= m_edges.size() || !m_edges[edgeId].alive) return false;

    auto startV = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v1 : m_edges[e.edge].v0; };

    const uint32_t cA = m_edges[edgeId].coedge;
    if (cA >= C || !m_coedges[cA].alive) return false;
    const uint32_t cB = m_coedges[cA].partner;
    if (cB == kInvalid || cB >= C || !m_coedges[cB].alive) return false;  // boundary edge

    const uint32_t lA = m_coedges[cA].loop, lB = m_coedges[cB].loop;
    if (lA >= m_loops.size() || lB >= m_loops.size() || lA == lB) return false;
    const uint32_t fA = m_loops[lA].face, fB = m_loops[lB].face;
    if (fA == fB) return false;  // both coedges on one face (a seam/slit) — not a merge

    const uint32_t pA = m_coedges[cA].prev, nA = m_coedges[cA].next;
    const uint32_t pB = m_coedges[cB].prev, nB = m_coedges[cB].next;
    // If either side would collapse (a face was a single-edge sliver), refuse.
    if (pA == cA || nA == cA || pB == cB || nB == cB) return false;

    // Collect loop B's coedges (excluding cB) before splicing, to re-home them.
    std::vector<uint32_t> loopBCoedges;
    {
        uint32_t w = nB, guard = 0;
        while (w != cB) {
            loopBCoedges.push_back(w);
            w = m_coedges[w].next;
            if (++guard > C) return false;
        }
    }

    // Splice the two rings into one by bypassing cA and cB:
    //   cA.prev -> cB.next   and   cB.prev -> cA.next.
    m_coedges[pA].next = nB; m_coedges[nB].prev = pA;
    m_coedges[pB].next = nA; m_coedges[nA].prev = pB;

    // Re-home loop B's coedges onto loop A (the surviving loop).
    for (uint32_t c : loopBCoedges) m_coedges[c].loop = lA;
    m_loops[lA].first = pA;  // pA is live and on loop A

    // Tombstone the removed edge, its two coedges, face B and its loop.
    m_edges[edgeId].alive = false;
    m_coedges[cA].alive = false;
    m_coedges[cB].alive = false;
    m_faces[fB].alive = false;
    m_loops[lB].alive = false;

    // Drop face B from its shell's list (defensive: no dead id lingering).
    const uint32_t sh = m_faces[fB].shell;
    if (sh < m_shells.size()) {
        auto& fs = m_shells[sh].faces;
        fs.erase(std::remove(fs.begin(), fs.end(), fB), fs.end());
    }

    // Re-root the removed edge's endpoints onto a live outgoing coedge.
    auto reroot = [&](uint32_t x) {
        const uint32_t cur = m_verts[x].coedge;
        if (cur < C && m_coedges[cur].alive && startV(cur) == x) return;
        m_verts[x].coedge = kInvalid;
        for (uint32_t c = 0; c < static_cast<uint32_t>(m_coedges.size()); ++c)
            if (m_coedges[c].alive && startV(c) == x) { m_verts[x].coedge = c; break; }
    };
    reroot(m_edges[edgeId].v0);
    reroot(m_edges[edgeId].v1);
    return true;
}

uint32_t Body::mergeCoplanarFaces(Tolerance tol)
{
    auto faceOfCoedge = [&](uint32_t c) -> uint32_t {
        if (c >= m_coedges.size() || !m_coedges[c].alive) return kInvalid;
        const uint32_t l = m_coedges[c].loop;
        return l < m_loops.size() ? m_loops[l].face : kInvalid;
    };
    // Outward normal of a planar face (surface normal, flipped when reversed).
    auto outwardN = [&](uint32_t f) -> Vec3 {
        const Face& fc = m_faces[f];
        const Vec3 n = (fc.surface < m_surfaces.size()) ? m_surfaces[fc.surface].normal
                                                        : Vec3{0.f, 0.f, 1.f};
        return fc.reversed ? Vec3{-n.x, -n.y, -n.z} : n;
    };
    // Coplanar: planar faces, equal outward normal, and fB's points on fA's plane.
    auto coplanar = [&](uint32_t fA, uint32_t fB) -> bool {
        const Face &FA = m_faces[fA], &FB = m_faces[fB];
        if (FA.surface >= m_surfaces.size() || FB.surface >= m_surfaces.size()) return false;
        const Surface &sA = m_surfaces[FA.surface], &sB = m_surfaces[FB.surface];
        if (sA.kind != SurfaceKind::Plane || sB.kind != SurfaceKind::Plane) return false;
        const Vec3 nA = outwardN(fA), nB = outwardN(fB);
        if (dot(nA, nB) < 1.f - 1e-4f) return false;  // parallel & co-oriented
        const std::vector<uint32_t> vs = faceVertices(fB);
        if (vs.empty()) return false;
        // Every vertex of fB must lie ON fA's plane. Use the EXACT orient3D-based
        // side test (band = tol.at(1)*10, matching the historic distance band) so
        // near-coplanar/borderline vertices are decided without float-cancellation
        // sign flips.
        for (uint32_t v : vs)
            if (facePlaneSide(fA, m_verts[v].point, tol) != 0) return false;
        return true;
    };
    // Number of edges the two faces share (via a coedge and its partner).
    auto sharedEdges = [&](uint32_t fA, uint32_t fB) -> int {
        int shared = 0;
        const uint32_t loopId = m_faces[fA].outerLoop;
        if (loopId >= m_loops.size()) return 0;
        uint32_t w = m_loops[loopId].first, guard = 0;
        if (w >= m_coedges.size()) return 0;
        do {
            const uint32_t p = m_coedges[w].partner;
            if (p != kInvalid && p < m_coedges.size() && m_coedges[p].alive &&
                faceOfCoedge(p) == fB)
                ++shared;
            w = m_coedges[w].next;
            if (++guard > m_coedges.size() + 1) break;
        } while (w != m_loops[loopId].first);
        return shared;
    };

    uint32_t merges = 0;
    bool changed = true;
    size_t safety = 0;
    while (changed && ++safety < 100000) {
        changed = false;
        for (uint32_t e = 0; e < m_edges.size(); ++e) {
            if (!m_edges[e].alive) continue;
            const uint32_t cA = m_edges[e].coedge;
            if (cA >= m_coedges.size() || !m_coedges[cA].alive) continue;
            const uint32_t cB = m_coedges[cA].partner;
            if (cB == kInvalid || cB >= m_coedges.size() || !m_coedges[cB].alive) continue;
            const uint32_t fA = faceOfCoedge(cA), fB = faceOfCoedge(cB);
            if (fA == kInvalid || fB == kInvalid || fA == fB) continue;
            if (!coplanar(fA, fB)) continue;
            if (sharedEdges(fA, fB) != 1) continue;  // 2+ shared → would slit
            if (mergeFaces(e)) {
                ++merges;
                changed = true;
                break;  // indices shifted; restart the scan
            }
        }
    }
    return merges;
}

uint32_t Body::mergeCollinearEdges(Tolerance tol)
{
    uint32_t removed = 0;
    bool changed = true;
    size_t safety = 0;
    while (changed && ++safety < 100000) {
        changed = false;
        for (uint32_t v = 0; v < m_verts.size(); ++v) {
            if (!m_verts[v].alive) continue;
            if (joinEdgesImpl(v, /*requireSameCurve=*/false, tol)) {
                ++removed;
                changed = true;
                break;  // liveness shifted; restart the scan
            }
        }
    }
    return removed;
}

uint32_t Body::simplify(Tolerance tol)
{
    uint32_t total = 0, pass = 0;
    do {
        // Each pass unlocks the other: merging collinear edges exposes new
        // single-shared-edge coplanar pairs, and merging faces exposes new
        // degree-2 collinear vertices.
        pass = mergeCoplanarFaces(tol) + mergeCollinearEdges(tol);
        total += pass;
    } while (pass > 0);
    return total;
}

// ──────────── Surface evaluation ─────────────────────────────────────────────

Vec3 Body::surfacePoint(uint32_t surfaceId, float u, float v) const
{
    if (surfaceId >= m_surfaces.size()) return {};
    const Surface& s = m_surfaces[surfaceId];
    if (s.kind == SurfaceKind::Nurbs && s.nurbs < m_nurbsSurfaces.size())
        return m_nurbsSurfaces[s.nurbs].evaluate(u, v);
    return s.eval(u, v);
}

Body::PointContainment Body::classifyPoint(const Vec3& p, Tolerance tol) const
{
    // Tessellate the shell to a watertight, crack-free triangle set. subdivisions
    // are placed per shared edge, so curved faces are approximated coherently.
    Mesh mesh = toMesh(6);
    (void)mesh.topology().triangulate();
    const auto& pos = mesh.attributes().positions();
    const auto& topo = mesh.topology();
    const size_t triCount = topo.faceCount();
    if (triCount == 0) return PointContainment::Outside;

    // OnBoundary: within tol of the tessellated boundary.
    const double tolAbs2 = tol.absolute * tol.absolute;
    for (size_t i = 0; i < triCount; ++i) {
        const auto& idx = topo.face(i).indices;
        if (idx.size() != 3) continue;
        if (pointTriangleDist2(p, pos[idx[0]], pos[idx[1]], pos[idx[2]]) <= tolAbs2)
            return PointContainment::OnBoundary;
    }

    // EXACT parity ray cast (Simulation-of-Simplicity). Cast from p along ONE fixed
    // generic direction to a far endpoint B beyond every mesh vertex, and count the
    // triangles the segment p→B crosses via segmentCrossesTriangleSoS. SoS
    // symbolically perturbs p so NO test is ever ambiguous — a query point coplanar
    // with a pole triangle, a ray through a shared edge/vertex — all resolve
    // consistently (a ray through a shared edge is counted for exactly one of the
    // two triangles), so the odd/even parity is exact and a single direction
    // suffices (no retry). Zero-area tessellation triangles contribute nothing.
    double R = 0.0;
    for (const auto& q : pos) {
        const double dq = length(sub(q, p));
        if (dq > R) R = dq;
    }
    const Vec3 d = normalize(Vec3{0.4680f, 0.6301f, 0.6201f});  // one fixed generic direction
    const Vec3 B = add(p, scale(d, 2.f * R + 1.f));             // beyond every vertex

    int crossings = 0;
    for (size_t i = 0; i < triCount; ++i) {
        const auto& idx = topo.face(i).indices;
        if (idx.size() != 3) continue;
        if (segmentCrossesTriangleSoS(p, B, pos[idx[0]], pos[idx[1]], pos[idx[2]])) ++crossings;
    }
    return (crossings & 1) ? PointContainment::Inside : PointContainment::Outside;
}

Vec3 Body::faceCentroid(uint32_t faceId) const
{
    const std::vector<uint32_t> vs = faceVertices(faceId);
    if (vs.empty()) return {};
    Vec3 c{0.f, 0.f, 0.f};
    for (uint32_t v : vs)
        if (v < m_verts.size()) c = add(c, m_verts[v].point);
    return scale(c, 1.f / static_cast<float>(vs.size()));
}

Body::PointContainment Body::classifyFace(uint32_t faceId, const Body& other, Tolerance tol) const
{
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return PointContainment::Outside;
    // faceSamplePoint, not faceCentroid: on a face pierced by a hole the outline's
    // average is the hole's centre — the one point the face does not occupy — which
    // classified a box face whose material lies entirely outside a cylinder as being
    // inside it. Identical to the centroid for a face without holes.
    return other.classifyPoint(faceSamplePoint(faceId), tol);
}

int Body::facePlaneSide(uint32_t faceId, const Vec3& p, Tolerance tol) const
{
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return 0;
    const std::vector<uint32_t> vs = faceVertices(faceId);
    if (vs.size() < 3) return 0;

    // Three non-collinear outer-loop vertices define the supporting plane.
    const Vec3 a = m_verts[vs[0]].point;
    Vec3 b{}, c{};
    bool haveB = false, haveC = false;
    for (size_t i = 1; i < vs.size(); ++i) {
        const Vec3 q = m_verts[vs[i]].point;
        if (!haveB) {
            if (dot(sub(q, a), sub(q, a)) > 0.f) { b = q; haveB = true; }
            continue;
        }
        // c is non-collinear iff (b-a)×(q-a) is non-degenerate.
        const Vec3 cr = cross(sub(b, a), sub(q, a));
        if (dot(cr, cr) > 0.f) { c = q; haveC = true; break; }
    }
    if (!haveB || !haveC) return 0;  // degenerate face (all points collinear)

    // Exact orientation: sign(orient3D) is the exact side w.r.t. the geometric
    // normal g = (b-a)×(c-a); |orient3D|/|g| is the perpendicular distance.
    const double o3 = RobustPredicates::orient3D(a, b, c, p);
    const Vec3 g = cross(sub(b, a), sub(c, a));
    const double glen = std::sqrt(static_cast<double>(dot(g, g)));
    if (glen <= 0.0) return 0;
    const double dist = o3 / glen;
    const double band = tol.at(1.f) * 10.f;
    if (std::abs(dist) <= static_cast<double>(band)) return 0;  // on-boundary band

    // Map the exact sign to the face's OUTWARD normal. g is parallel to the
    // surface normal, so the alignment sign is float-robust (never near zero).
    Vec3 n = (m_faces[faceId].surface < m_surfaces.size()) ? m_surfaces[m_faces[faceId].surface].normal
                                                           : Vec3{0.f, 0.f, 1.f};
    if (m_faces[faceId].reversed) n = Vec3{-n.x, -n.y, -n.z};
    // RobustPredicates::orient3D(a,b,c,p) is NEGATIVE on the +g = (b-a)×(c-a)
    // side, so sign along g is -sign(o3); then rotate onto the outward normal.
    int s = (o3 < 0.0) ? 1 : -1;
    if (dot(n, g) < 0.f) s = -s;
    return s;
}

namespace {

// 12-point Gauss-Legendre on [-1,1]. The integrands over an analytic patch are smooth
// trigonometric polynomials, so this converges spectrally and reaches float precision far
// below the order where it would matter.
constexpr int    kGaussN = 12;
constexpr double kGaussX[kGaussN] = {
    -0.9815606342467192, -0.9041172563704749, -0.7699026741943047, -0.5873179542866175,
    -0.3678314989981802, -0.1252334085114689,  0.1252334085114689,  0.3678314989981802,
     0.5873179542866175,  0.7699026741943047,  0.9041172563704749,  0.9815606342467192};
constexpr double kGaussW[kGaussN] = {
    0.0471753363865118, 0.1069393259953184, 0.1600783285433462, 0.2031674267230659,
    0.2334925365383548, 0.2491470458134028, 0.2491470458134028, 0.2334925365383548,
    0.2031674267230659, 0.1600783285433462, 0.1069393259953184, 0.0471753363865118};

struct Dvec { double x, y, z; };

// A point on an analytic surface together with its two parametric tangents, in double.
struct Patch { Dvec p, du, dv; };

bool analyticPatch(const Surface& s, double u, double v, Patch& out)
{
    const Vec3 vaF = s.vAxis();
    const Dvec U{s.uAxis.x, s.uAxis.y, s.uAxis.z};
    const Dvec Va{vaF.x, vaF.y, vaF.z};
    const Dvec N{s.normal.x, s.normal.y, s.normal.z};
    const Dvec O{s.origin.x, s.origin.y, s.origin.z};
    const double r = s.radius;
    const double c = std::cos(u), sn = std::sin(u);

    switch (s.kind) {
        case SurfaceKind::Cylinder:
            out.p  = {O.x + r * (c * U.x + sn * Va.x) + v * N.x,
                      O.y + r * (c * U.y + sn * Va.y) + v * N.y,
                      O.z + r * (c * U.z + sn * Va.z) + v * N.z};
            out.du = {r * (-sn * U.x + c * Va.x), r * (-sn * U.y + c * Va.y),
                      r * (-sn * U.z + c * Va.z)};
            out.dv = N;
            return true;
        case SurfaceKind::Sphere: {
            // This kernel: p = r*(sin(u)*U + cos(u)*cos(v)*Va + cos(u)*sin(v)*N).
            // u is latitude (poles at u = +/-pi/2, where cos(u) = 0), v is longitude.
            const double cu = c, su = sn;  // c/sn were cos(u)/sin(u)
            const double cv = std::cos(v), sv = std::sin(v);
            out.p  = {O.x + r * (su * U.x + cu * cv * Va.x + cu * sv * N.x),
                      O.y + r * (su * U.y + cu * cv * Va.y + cu * sv * N.y),
                      O.z + r * (su * U.z + cu * cv * Va.z + cu * sv * N.z)};
            out.du = {r * (cu * U.x - su * cv * Va.x - su * sv * N.x),
                      r * (cu * U.y - su * cv * Va.y - su * sv * N.y),
                      r * (cu * U.z - su * cv * Va.z - su * sv * N.z)};
            out.dv = {r * cu * (-sv * Va.x + cv * N.x),
                      r * cu * (-sv * Va.y + cv * N.y),
                      r * cu * (-sv * Va.z + cv * N.z)};
            return true;
        }
        case SurfaceKind::Cone: {
            // origin = apex, normal = axis, radius = slope; v is axial distance from apex.
            const double rr = r * v;
            out.p  = {O.x + v * N.x + rr * (c * U.x + sn * Va.x),
                      O.y + v * N.y + rr * (c * U.y + sn * Va.y),
                      O.z + v * N.z + rr * (c * U.z + sn * Va.z)};
            out.du = {rr * (-sn * U.x + c * Va.x), rr * (-sn * U.y + c * Va.y),
                      rr * (-sn * U.z + c * Va.z)};
            out.dv = {N.x + r * (c * U.x + sn * Va.x), N.y + r * (c * U.y + sn * Va.y),
                      N.z + r * (c * U.z + sn * Va.z)};
            return true;
        }
        default:
            return false;  // Plane is exact from triangles; NURBS is not handled here
    }
}

// Inverse parameterisation, mapping a vertex to its (u,v). `degenerate` reports a point
// where the parameterisation collapses — a cone's apex or a sphere's pole — so its swept
// coordinate (u for a cone apex, v for a sphere pole) is meaningless and the caller expands
// it into the two edges that meet there rather than trusting the value returned.
bool analyticInverse(const Surface& s, const Vec3& p, double& u, double& v, bool& degenerate)
{
    const Vec3 vaF = s.vAxis();
    const double dx = p.x - s.origin.x, dy = p.y - s.origin.y, dz = p.z - s.origin.z;
    const double au = dx * s.uAxis.x + dy * s.uAxis.y + dz * s.uAxis.z;
    const double av = dx * vaF.x + dy * vaF.y + dz * vaF.z;
    const double an = dx * s.normal.x + dy * s.normal.y + dz * s.normal.z;
    const double radial = std::sqrt(au * au + av * av);
    const double scale = std::max(1.0, std::abs(static_cast<double>(s.radius)));
    degenerate = false;

    switch (s.kind) {
        case SurfaceKind::Cylinder:
            u = std::atan2(av, au); v = an;
            return true;
        case SurfaceKind::Cone:
            // The ring radius is slope*v, so the apex (v = 0) has no meaningful u.
            degenerate = (radial <= 1e-6 * scale);
            u = degenerate ? 0.0 : std::atan2(av, au);
            v = an;
            return true;
        case SurfaceKind::Sphere: {
            if (s.radius <= 0.f) return false;
            // au = r*sin(u), and (av, an) = r*cos(u)*(cos(v), sin(v)). So u is latitude and
            // v is longitude. At a pole cos(u) = 0, the ring (av, an) collapses, and v is
            // meaningless there while u stays valid (u = +/-pi/2).
            const double r = static_cast<double>(s.radius);
            u = std::asin(std::clamp(au / r, -1.0, 1.0));
            const double ring = std::sqrt(av * av + an * an);
            degenerate = (ring <= 1e-6 * scale);
            v = degenerate ? 0.0 : std::atan2(an, av);
            return true;
        }
        default: return false;
    }
}

// Integrate over a curved analytic face's image in PARAMETER space, calling `sample` at
// each quadrature point with the surface point, the (reversed-as-needed) area-weighted
// normal du x dv, and the quadrature weight. Returns whether the face was integrated
// exactly; a face whose surface does not reproduce its vertices, or a NURBS/plane face,
// returns false so the caller can fall back to tessellation.
//
// The polygon is the face's vertices mapped to (u,v), with a pole/apex vertex expanded
// into the two edges meeting there (its swept parameter is meaningless but the Jacobian
// vanishes at the pole, so the value does not matter). The periodic parameter is unwrapped
// by walking the loop so a wedge crossing the +/-pi seam is contiguous. Each parameter-
// space triangle is integrated by mapping the unit square onto it (Duffy).
template <class Sample>
bool integrateFaceParametric(const Surface& surf, const std::vector<Vec3>& pts, bool reversed,
                             Sample&& sample)
{
    if (surf.kind != SurfaceKind::Cylinder && surf.kind != SurfaceKind::Cone
        && surf.kind != SurfaceKind::Sphere) {
        return false;
    }
    const int n = static_cast<int>(pts.size());
    if (n < 3) return false;

    std::vector<double> U(n), V(n);
    std::vector<char>   isPole(n, 0);
    const double kPi = 3.141592653589793, kTwoPi = 6.283185307179586;

    for (int k = 0; k < n; ++k) {
        double uu = 0, vv = 0;
        bool   deg = false;
        if (!analyticInverse(surf, pts[k], uu, vv, deg)) return false;
        if (!deg) {
            Patch probe;
            if (!analyticPatch(surf, uu, vv, probe)) return false;
            const double ex = probe.p.x - pts[k].x, ey = probe.p.y - pts[k].y, ez = probe.p.z - pts[k].z;
            const double scale = std::max(1.0, std::abs(static_cast<double>(surf.radius)));
            if (std::sqrt(ex * ex + ey * ey + ez * ez) > 1e-4 * scale) return false;
        }
        U[k] = uu;
        V[k] = vv;
        isPole[k] = deg ? 1 : 0;
    }

    // Unwrap the periodic parameter by walking the loop.
    const bool vPeriodic = surf.kind == SurfaceKind::Sphere;
    int firstNP = -1;
    for (int k = 0; k < n; ++k) if (!isPole[k]) { firstNP = k; break; }
    if (firstNP < 0) return false;
    {
        double prev = vPeriodic ? V[firstNP] : U[firstNP];
        for (int step = 1; step < n; ++step) {
            const int k = (firstNP + step) % n;
            if (isPole[k]) continue;
            double& a = vPeriodic ? V[k] : U[k];
            while (a - prev >  kPi) a -= kTwoPi;
            while (a - prev <= -kPi) a += kTwoPi;
            prev = a;
        }
    }

    // Build the parameter-space polygon, expanding a pole/apex vertex into two edge points.
    std::vector<double> PU, PV;
    PU.reserve(n + 2);
    PV.reserve(n + 2);
    const bool sweptIsV = surf.kind != SurfaceKind::Cone;  // cone apex fixes v
    for (int k = 0; k < n; ++k) {
        if (!isPole[k]) { PU.push_back(U[k]); PV.push_back(V[k]); continue; }
        const int prev = (k + n - 1) % n, next = (k + 1) % n;
        if (sweptIsV) {
            PU.push_back(U[k]); PV.push_back(V[prev]);
            PU.push_back(U[k]); PV.push_back(V[next]);
        } else {
            PU.push_back(U[prev]); PV.push_back(V[k]);
            PU.push_back(U[next]); PV.push_back(V[k]);
        }
    }

    const int m = static_cast<int>(PU.size());
    for (int k = 1; k + 1 < m; ++k) {
        const double q0u = PU[0], q0v = PV[0];
        const double q1u = PU[k], q1v = PV[k];
        const double q2u = PU[k + 1], q2v = PV[k + 1];
        const double twoA = std::abs((q1u - q0u) * (q2v - q0v) - (q2u - q0u) * (q1v - q0v));
        if (twoA < 1e-14) continue;
        for (int i = 0; i < kGaussN; ++i) {
            for (int j = 0; j < kGaussN; ++j) {
                const double a = 0.5 * (kGaussX[i] + 1.0);
                const double b = 0.5 * (kGaussX[j] + 1.0);
                const double l0 = 1.0 - a, l1 = a * (1.0 - b), l2 = a * b;
                const double u = l0 * q0u + l1 * q1u + l2 * q2u;
                const double v = l0 * q0v + l1 * q1v + l2 * q2v;
                Patch pt;
                if (!analyticPatch(surf, u, v, pt)) return false;
                Dvec nrm{pt.du.y * pt.dv.z - pt.du.z * pt.dv.y,
                         pt.du.z * pt.dv.x - pt.du.x * pt.dv.z,
                         pt.du.x * pt.dv.y - pt.du.y * pt.dv.x};
                if (reversed) nrm = {-nrm.x, -nrm.y, -nrm.z};
                // 0.25 maps [-1,1]^2 -> [0,1]^2; twoA*a is the parameter-triangle Jacobian.
                const double w = 0.25 * kGaussW[i] * kGaussW[j] * twoA * a;
                sample(pt.p, nrm, w);
            }
        }
    }
    return true;
}

// One boundary edge of a planar face, as a curve segment traversed start -> end.
struct PlanarEdge {
    Curve  curve;
    double c0, c1;  // curve parameters at the segment's start and end (in traversal order)
};

// Exact contribution of a PLANAR face (possibly bounded by circular arcs) to the ten
// divergence-theorem boundary integrals and to its area, via Green's theorem in the plane.
//
// A flat face's contribution to a surface integral of a constant-normal field is n times
// the 2D area integral over the region: intg-term = n_component * coeff * integral of a
// cubic monomial in x,y,z over the face. Each coordinate is linear in the in-plane
// coordinates (s,t) — x = O.x + s*e1.x + t*e2.x — so every such integral is a combination
// of the 2D area moments m_ij = integral of s^i t^j dA (i+j <= 3). Each moment is in turn
// a boundary line integral, m_ij = closed integral of (1/(i+1)) s^(i+1) t^j dt (Green),
// evaluated exactly per edge: a Line edge gives a polynomial, a Circle arc a trigonometric
// integrand that 12-point Gauss resolves to float precision. This is what makes a
// cylinder or cone cap a true pi*r^2 disk rather than the inscribed n-gon a triangulation
// would give.
//
// `nOut` is the outward unit normal; e1,e2 are an in-plane orthonormal frame with
// e1 x e2 = nOut, and O a point in the plane.
bool assemblePlanarFace(const std::vector<PlanarEdge>& edges, const Vec3& O, const Vec3& e1,
                        const Vec3& e2, const Vec3& nOut, std::array<double, 10>& intg,
                        double& area)
{
    if (edges.size() < 2) return false;

    // 2D area moments m[i][j] = integral of s^i t^j dA over the region, i+j <= 3.
    double m[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
    for (const PlanarEdge& pe : edges) {
        const Curve& cu = pe.curve;
        if (cu.kind == CurveKind::Nurbs) return false;  // not supported: caller tessellates
        const double cs = pe.c0, ce = pe.c1;
        const double hc = 0.5 * (ce - cs), mc = 0.5 * (ce + cs);
        for (int g = 0; g < kGaussN; ++g) {
            const double c = mc + hc * kGaussX[g];
            const Vec3 p = cu.eval(static_cast<float>(c));
            // in-plane coordinates and the boundary tangent's t-component
            const double dxp = p.x - O.x, dyp = p.y - O.y, dzp = p.z - O.z;
            const double sC = dxp * e1.x + dyp * e1.y + dzp * e1.z;
            const double tC = dxp * e2.x + dyp * e2.y + dzp * e2.z;
            // dp/dc analytically per curve kind (kept in double).
            double dpx, dpy, dpz;
            if (cu.kind == CurveKind::Line) {
                dpx = cu.dir.x; dpy = cu.dir.y; dpz = cu.dir.z;  // p = origin + c*dir
            } else {  // Circle: centre + R(cos c * ref + sin c * (dir x ref))
                const double wx = cu.dir.y * cu.ref.z - cu.dir.z * cu.ref.y;
                const double wy = cu.dir.z * cu.ref.x - cu.dir.x * cu.ref.z;
                const double wz = cu.dir.x * cu.ref.y - cu.dir.y * cu.ref.x;
                const double sc = std::sin(c), cc = std::cos(c);
                dpx = cu.radius * (-sc * cu.ref.x + cc * wx);
                dpy = cu.radius * (-sc * cu.ref.y + cc * wy);
                dpz = cu.radius * (-sc * cu.ref.z + cc * wz);
            }
            const double dtdc = dpx * e2.x + dpy * e2.y + dpz * e2.z;
            const double wG = kGaussW[g] * hc * dtdc;  // dt = (dp . e2) dc
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j + i < 4; ++j)
                    m[i][j] += wG * (1.0 / (i + 1)) * std::pow(sC, i + 1) * std::pow(tC, j);
        }
    }

    if (m[0][0] <= 1e-14) return false;  // degenerate / zero-area
    area += m[0][0];

    // Integrals over the face of products of the linear forms x=(ax,bx,cx).(1,s,t), etc.
    const double ax = O.x, bx = e1.x, cx = e2.x;
    const double ay = O.y, by = e1.y, cy = e2.y;
    const double az = O.z, bz = e1.z, cz = e2.z;
    auto I1 = [&](double a, double b, double c) {
        return a * m[0][0] + b * m[1][0] + c * m[0][1];
    };
    auto I2 = [&](double a1, double b1, double c1, double a2, double b2, double c2) {
        return a1 * a2 * m[0][0] + (a1 * b2 + a2 * b1) * m[1][0] + (a1 * c2 + a2 * c1) * m[0][1]
             + b1 * b2 * m[2][0] + (b1 * c2 + b2 * c1) * m[1][1] + c1 * c2 * m[0][2];
    };
    auto I3 = [&](double a1, double b1, double c1, double a2, double b2, double c2, double a3,
                  double b3, double c3) {
        const double bv[3][3] = {{a1, b1, c1}, {a2, b2, c2}, {a3, b3, c3}};
        const int es[3] = {0, 1, 0}, et[3] = {0, 0, 1};
        double C[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
        for (int u = 0; u < 3; ++u)
            for (int v = 0; v < 3; ++v)
                for (int w = 0; w < 3; ++w)
                    C[es[u] + es[v] + es[w]][et[u] + et[v] + et[w]] += bv[0][u] * bv[1][v] * bv[2][w];
        double r = 0;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j + i < 4; ++j) r += C[i][j] * m[i][j];
        return r;
    };

    intg[0] += nOut.x * I1(ax, bx, cx);
    intg[1] += 0.5 * nOut.x * I2(ax, bx, cx, ax, bx, cx);
    intg[2] += 0.5 * nOut.y * I2(ay, by, cy, ay, by, cy);
    intg[3] += 0.5 * nOut.z * I2(az, bz, cz, az, bz, cz);
    intg[4] += (1.0 / 3.0) * nOut.x * I3(ax, bx, cx, ax, bx, cx, ax, bx, cx);
    intg[5] += (1.0 / 3.0) * nOut.y * I3(ay, by, cy, ay, by, cy, ay, by, cy);
    intg[6] += (1.0 / 3.0) * nOut.z * I3(az, bz, cz, az, bz, cz, az, bz, cz);
    intg[7] += 0.5 * nOut.x * I3(ax, bx, cx, ax, bx, cx, ay, by, cy);
    intg[8] += 0.5 * nOut.y * I3(ay, by, cy, ay, by, cy, az, bz, cz);
    intg[9] += 0.5 * nOut.z * I3(az, bz, cz, az, bz, cz, ax, bx, cx);
    return true;
}

}  // namespace

bool Body::integratePlanarFace(uint32_t faceId, std::array<double, 10>& intg, double& area) const
{
    const Face& face = m_faces[faceId];
    if (!face.alive || face.surface == kInvalid || face.surface >= m_surfaces.size()) return false;
    const Surface& surf = m_surfaces[face.surface];
    if (surf.kind != SurfaceKind::Plane) return false;
    if (!face.innerLoops.empty()) return false;   // holes need the loop subtracted; tessellate
    if (face.outerLoop == kInvalid || face.outerLoop >= m_loops.size()) return false;

    // Outward frame: e1 in-plane, e2 = nOut x e1 so e1 x e2 = nOut.
    const Vec3 nOut = face.reversed
                          ? Vec3{-surf.normal.x, -surf.normal.y, -surf.normal.z}
                          : surf.normal;
    Vec3 e1 = surf.uAxis;
    const double e1l = length(e1);
    if (e1l < 1e-12f) return false;
    e1 = {e1.x / e1l, e1.y / e1l, e1.z / e1l};
    const Vec3 e2{nOut.y * e1.z - nOut.z * e1.y, nOut.z * e1.x - nOut.x * e1.z,
                  nOut.x * e1.y - nOut.y * e1.x};
    const Vec3 O = surf.origin;

    // Walk the outer loop's coedges into oriented curve segments (start -> end).
    std::vector<PlanarEdge> edges;
    const uint32_t first = m_loops[face.outerLoop].first;
    if (first == kInvalid || first >= m_coedges.size()) return false;
    uint32_t cur = first;
    for (int guard = 0; guard < 100000; ++guard) {
        const Coedge& ce = m_coedges[cur];
        if (ce.edge == kInvalid || ce.edge >= m_edges.size()) return false;
        const Edge& ed = m_edges[ce.edge];
        if (ed.curve == kInvalid || ed.curve >= m_curves.size()) return false;
        const Curve& cu = m_curves[ed.curve];
        if (cu.kind == CurveKind::Nurbs) return false;  // caller tessellates
        // The edge's curve runs v0 -> v1 over [t0,t1]; a reversed coedge traverses it e1->e0.
        PlanarEdge pe;
        pe.curve = cu;
        pe.c0 = ce.reversed ? ed.t1 : ed.t0;
        pe.c1 = ce.reversed ? ed.t0 : ed.t1;
        edges.push_back(pe);
        cur = ce.next;
        if (cur == first) break;
        if (cur == kInvalid || cur >= m_coedges.size()) return false;
    }

    return assemblePlanarFace(edges, O, e1, e2, nOut, intg, area);
}

float Body::surfaceArea() const
{
    // Surface area is the sum of face areas with no closure constraint, so unlike volume
    // it needs no all-or-nothing guard: each curved analytic face is integrated exactly
    // over its parameter domain (area = integral of |dp/du x dp/dv| du dv), independently,
    // and any face that cannot be — planar, NURBS, or one whose surface does not fit it —
    // contributes its exact flat-triangle area instead. Tessellating a flat polygon is
    // exact, so the only faces that gain from the analytic path are the curved ones, which
    // is exactly where the chord error was: a tessellated sphere was 6.8% high.
    double area = 0.0;

    for (uint32_t fi = 0; fi < static_cast<uint32_t>(m_faces.size()); ++fi) {
        const Face& face = m_faces[fi];
        if (!face.alive || face.surface == kInvalid || face.surface >= m_surfaces.size()) continue;
        const Surface& surf = m_surfaces[face.surface];

        bool exact = false;
        if (face.innerLoops.empty()) {
            std::vector<Vec3> pts;
            for (const uint32_t vid : faceVertices(fi))
                if (vid < m_verts.size()) pts.push_back(m_verts[vid].point);

            double acc = 0.0;
            // |du x dv| is the area element; winding does not affect its magnitude.
            const bool ok = integrateFaceParametric(
                surf, pts, false, [&](const Dvec&, const Dvec& nrm, double w) {
                    acc += w * std::sqrt(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
                });
            if (ok) { area += acc; exact = true; }
        }
        if (exact) continue;

        // Planar faces: exact via Green's theorem over the boundary (arc caps become true
        // disks, not inscribed n-gons). Only the area output is used here.
        {
            std::array<double, 10> dummy{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            double planarArea = 0.0;
            if (integratePlanarFace(fi, dummy, planarArea)) { area += planarArea; continue; }
        }

        // Fall back: this face's flat-triangle area (a planar face bounded only by chords).
        const std::vector<uint32_t> vids = faceVertices(fi);
        for (uint32_t k = 1; k + 1 < static_cast<uint32_t>(vids.size()); ++k) {
            if (vids[0] >= m_verts.size() || vids[k] >= m_verts.size()
                || vids[k + 1] >= m_verts.size()) continue;
            const Vec3 a = m_verts[vids[0]].point;
            const Vec3 b = m_verts[vids[k]].point;
            const Vec3 c = m_verts[vids[k + 1]].point;
            area += 0.5 * static_cast<double>(length(cross(sub(b, a), sub(c, a))));
        }
    }

    return static_cast<float>(area);
}


nexus::geometry::MassProperties Body::massProperties(float density) const
{
    // Volume, centroid and inertia from the boundary, by the divergence theorem.
    //
    // PLANAR faces are exact from their triangles — a flat polygon is reproduced by its
    // triangulation with no error at all — so they go through the shared mesh integrator.
    // CURVED analytic faces are not: chords cut the corner, and tessellating left a 0.65%
    // volume error on a sphere and about 1% on its moments. An analytic modeller should
    // not be approximate about its own analytic primitives, so those faces are integrated
    // over their PARAMETER domain by tensor-product Gauss-Legendre instead, which is
    // spectrally accurate for the trigonometric integrands involved.
    //
    // The two contributions simply add: a boundary integral is additive over disjoint
    // pieces of the boundary, which is why MeshMassProperties exposes its raw integrals.
    //
    // A face qualifies for exact treatment only if its surface genuinely fits it — the
    // patch must reproduce every one of the face's own vertices. Anything else (a NURBS
    // face, or a face whose surface does not contain it) falls back to triangles, which is
    // always correct and merely less precise. That guard is why this is safe to attempt at
    // all: until this kernel grew a real Cone surface, a cone's faces were tagged as
    // cylinders that did not contain their own apex.
    std::array<double, 10> intg{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    Mesh residual;                       // faces not integrated exactly
    std::vector<Vec3>    rpos;
    std::vector<nexus::geometry::Face> rfaces;  // MESH faces, not brep::Face
    bool anyExact = false;
    // A curved surface is exact only if EVERY one of its faces integrates exactly. The
    // round patches and the flat triangle fallback do not join into a closed boundary — a
    // sphere face straddling the +/-pi longitude seam (which happens at ODD longitude
    // counts) leaves a sliver gap against its exactly-integrated neighbours. If any curved
    // face fails, the whole body falls back to tessellation, which is correct and
    // convergent. (A cylinder's flat CAPS are planar, not curved, so they legitimately go
    // to the residual without tripping this — their n_x is zero in the volume term.)
    bool curvedFail = false;

    for (uint32_t fi = 0; fi < static_cast<uint32_t>(m_faces.size()); ++fi) {
        const Face& face = m_faces[fi];
        if (!face.alive || face.surface == kInvalid || face.surface >= m_surfaces.size()) continue;
        const Surface& surf = m_surfaces[face.surface];

        bool exact = false;
        // Curved analytic faces (Cylinder, Cone, Sphere) are integrated exactly over their
        // parameter domain; see integrateFaceParametric. Planar and NURBS faces return
        // false and go to the tessellated residual below.
        if (face.innerLoops.empty()) {
            std::vector<Vec3> pts;
            for (const uint32_t vid : faceVertices(fi))
                if (vid < m_verts.size()) pts.push_back(m_verts[vid].point);

            std::array<double, 10> acc{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            const bool ok = integrateFaceParametric(
                surf, pts, face.reversed, [&](const Dvec& q, const Dvec& nrm, double w) {
                    const double x = q.x, y = q.y, z = q.z;
                    acc[0] += w * x * nrm.x;
                    acc[1] += w * 0.5 * x * x * nrm.x;
                    acc[2] += w * 0.5 * y * y * nrm.y;
                    acc[3] += w * 0.5 * z * z * nrm.z;
                    acc[4] += w * (1.0 / 3.0) * x * x * x * nrm.x;
                    acc[5] += w * (1.0 / 3.0) * y * y * y * nrm.y;
                    acc[6] += w * (1.0 / 3.0) * z * z * z * nrm.z;
                    acc[7] += w * 0.5 * x * x * y * nrm.x;
                    acc[8] += w * 0.5 * y * y * z * nrm.y;
                    acc[9] += w * 0.5 * z * z * x * nrm.z;
                });
            if (ok) {
                for (int k = 0; k < 10; ++k) intg[k] += acc[k];
                exact = true;
                anyExact = true;
            } else if (surf.kind == SurfaceKind::Cylinder || surf.kind == SurfaceKind::Cone
                       || surf.kind == SurfaceKind::Sphere) {
                curvedFail = true;  // a curved face that could not be integrated
            }
        }

        if (exact) continue;

        // Planar faces: exact via Green's theorem over the boundary (arc-bounded caps are
        // true disks, closing the transverse-moment facet error a triangulated cap left).
        {
            std::array<double, 10> planar{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            double dummyArea = 0.0;
            if (integratePlanarFace(fi, planar, dummyArea)) {
                for (int k = 0; k < 10; ++k) intg[k] += planar[k];
                anyExact = true;
                continue;
            }
        }

        // Not exact: contribute this face's triangles to the residual mesh. Faces with
        // inner loops need the full tessellator, so their presence sends the whole body
        // down the tessellated path below.
        if (!face.innerLoops.empty()) { rfaces.clear(); anyExact = false; break; }
        const std::vector<uint32_t> vids = faceVertices(fi);
        if (vids.size() < 3) continue;
        const uint32_t base = static_cast<uint32_t>(rpos.size());
        for (const uint32_t vid : vids) {
            if (vid >= m_verts.size()) continue;
            rpos.push_back(m_verts[vid].point);
        }
        for (uint32_t k = 1; k + 1 < static_cast<uint32_t>(vids.size()); ++k) {
            nexus::geometry::Face t;
            t.indices = {base, base + k, base + k + 1};
            rfaces.push_back(std::move(t));
        }
    }

    if (!anyExact || curvedFail) {
        // Nothing qualified, or a curved surface could not be fully integrated exactly:
        // integrate the whole tessellated boundary, which is correct and convergent.
        return nexus::geometry::MeshMassProperties::fromIntegrals(
            nexus::geometry::MeshMassProperties::integrals(toMesh(3)), density);
    }

    if (!rfaces.empty()) {
        // Render boundary again: the residual Mesh carries single precision.
        std::vector<nexus::render::Vec3> rposF;
        rposF.reserve(rpos.size());
        for (const Vec3& rp : rpos) rposF.push_back(rp.toFloat());
        residual.attributes().setPositions(std::move(rposF));
        for (nexus::geometry::Face& f : rfaces) residual.topology().addFace(std::move(f));
        const std::array<double, 10> flat =
            nexus::geometry::MeshMassProperties::integrals(residual);
        for (int k = 0; k < 10; ++k) intg[k] += flat[k];
    }

    return nexus::geometry::MeshMassProperties::fromIntegrals(intg, density);
}

bool Body::transform(const nexus::render::Mat4& mat)
{
    using nexus::render::Vec4;
    // Reject non-finite matrices up front (before any mutation).
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (!isFinite(mat.m[i][j])) return false;
    // The bottom row must be affine [0 0 0 1].
    if (mat.m[3][0] != 0.f || mat.m[3][1] != 0.f || mat.m[3][2] != 0.f || mat.m[3][3] != 1.f)
        return false;
    // NURBS-backed faces store control points we do not transform here — a follow-up.
    if (!m_nurbsSurfaces.empty()) return false;

    // Linear part columns (image of the basis vectors). Require a proper rotation
    // times a single UNIFORM scale: equal-length, mutually orthogonal, det > 0.
    const Vec3 a0{mat.m[0][0], mat.m[1][0], mat.m[2][0]};
    const Vec3 a1{mat.m[0][1], mat.m[1][1], mat.m[2][1]};
    const Vec3 a2{mat.m[0][2], mat.m[1][2], mat.m[2][2]};
    const double s0 = length(a0), s1 = length(a1), s2 = length(a2);
    if (s0 < 1e-12f || s1 < 1e-12f || s2 < 1e-12f) return false;
    const double sTol = 1e-4f * s0;
    if (std::abs(s0 - s1) > sTol || std::abs(s0 - s2) > sTol) return false;  // non-uniform
    const double oTol = 1e-4f * s0 * s0;
    if (std::abs(dot(a0, a1)) > oTol || std::abs(dot(a0, a2)) > oTol ||
        std::abs(dot(a1, a2)) > oTol)
        return false;  // shear
    if (dot(a0, cross(a1, a2)) <= 0.f) return false;  // reflection / degenerate
    const double s = s0;  // uniform scale factor

    // Mat4's ENTRIES are single precision, but the point must not be: narrowing the point
    // to float for the multiply and widening the result back throws away exactly the
    // precision this type exists to keep, and it does so even for a transform that changes
    // nothing. Measured when it did: translating a cylinder by (0,0,0) perturbed its
    // coordinates enough that it no longer compared coincident with an untranslated copy of
    // itself, and the identity boolean of two identical cylinders returned empty. So promote
    // the matrix and do the arithmetic in double; only the matrix's own precision limits it.
    auto xformPoint = [&](const Vec3& p) {
        const double m00 = mat.m[0][0], m01 = mat.m[0][1], m02 = mat.m[0][2], m03 = mat.m[0][3];
        const double m10 = mat.m[1][0], m11 = mat.m[1][1], m12 = mat.m[1][2], m13 = mat.m[1][3];
        const double m20 = mat.m[2][0], m21 = mat.m[2][1], m22 = mat.m[2][2], m23 = mat.m[2][3];
        return Vec3{m00 * p.x + m01 * p.y + m02 * p.z + m03,
                    m10 * p.x + m11 * p.y + m12 * p.z + m13,
                    m20 * p.x + m21 * p.y + m22 * p.z + m23};
    };
    // Direction: linear part only, renormalized (proper rotation ⇒ unit preserved).
    auto xformDir = [&](const Vec3& d) {
        const double m00 = mat.m[0][0], m01 = mat.m[0][1], m02 = mat.m[0][2];
        const double m10 = mat.m[1][0], m11 = mat.m[1][1], m12 = mat.m[1][2];
        const double m20 = mat.m[2][0], m21 = mat.m[2][1], m22 = mat.m[2][2];
        return normalize(Vec3{m00 * d.x + m01 * d.y + m02 * d.z,
                              m10 * d.x + m11 * d.y + m12 * d.z,
                              m20 * d.x + m21 * d.y + m22 * d.z});
    };

    for (auto& v : m_verts) v.point = xformPoint(v.point);
    for (auto& c : m_curves) {
        c.origin = xformPoint(c.origin);
        c.dir = xformDir(c.dir);
        c.ref = xformDir(c.ref);
        c.radius *= s;  // Circle radius scales; Line ignores radius
    }
    // Line edge params are arc-length (distance) ⇒ scale by s; Circle params are
    // angles ⇒ preserved. (Curve::eval then still reproduces the endpoints.)
    for (auto& e : m_edges) {
        if (e.curve < m_curves.size() && m_curves[e.curve].kind == CurveKind::Line) {
            e.t0 *= s;
            e.t1 *= s;
        }
    }
    for (auto& sf : m_surfaces) {
        sf.origin = xformPoint(sf.origin);
        sf.normal = xformDir(sf.normal);
        sf.uAxis = xformDir(sf.uAxis);
        sf.radius *= s;  // Cylinder / Sphere radius; Plane ignores radius
    }
    return true;
}

bool Body::translate(const Vec3& t)
{
    nexus::render::Mat4 m = nexus::render::Mat4::identity();
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
    return transform(m);
}

// ──────────── Tessellation ───────────────────────────────────────────────────

Mesh Body::toMesh(uint32_t subdivisions) const
{
    Mesh mesh;

    // Output vertices: live B-rep vertices (compacted), then `subdivisions`
    // intermediate points per live edge — placed via the edge's curve so BOTH
    // incident faces reference the SAME points (crack-free / watertight).
    // `posD` is the same points before the narrowing to float. The mesh carries floats
    // because that is what the mesh is, but the TRIANGULATION DECISIONS below are read from
    // the doubles — see the convexity test for why that distinction is load-bearing.
    std::vector<nexus::render::Vec3> pos;
    std::vector<Vec3> posD;
    std::vector<int> vOut(m_verts.size(), -1);
    for (uint32_t v = 0; v < m_verts.size(); ++v) {
        if (!m_verts[v].alive) continue;
        vOut[v] = static_cast<int>(pos.size());
        pos.push_back(m_verts[v].point.toFloat());
        posD.push_back(m_verts[v].point);
    }
    std::vector<std::vector<uint32_t>> edgeMid(m_edges.size());
    if (subdivisions > 0) {
        for (uint32_t e = 0; e < m_edges.size(); ++e) {
            if (!m_edges[e].alive || m_edges[e].curve >= m_curves.size()) continue;
            const Edge& ed = m_edges[e];
            const Curve& cu = m_curves[ed.curve];
            edgeMid[e].reserve(subdivisions);
            for (uint32_t k = 1; k <= subdivisions; ++k) {
                const double f = static_cast<float>(k) / static_cast<float>(subdivisions + 1u);
                edgeMid[e].push_back(static_cast<uint32_t>(pos.size()));
                const Vec3 p = cu.eval(ed.t0 + (ed.t1 - ed.t0) * f);
                pos.push_back(p.toFloat());
                posD.push_back(p);
            }
        }
    }
    mesh.attributes().setPositions(pos);

    // Output-vertex indices around a loop, with per-edge subdivision points
    // inserted in traversal order (crack-free with the neighbouring face).
    auto buildRing = [&](uint32_t firstCoedge) {
        std::vector<uint32_t> ring;
        if (firstCoedge >= m_coedges.size()) return ring;
        uint32_t walk = firstCoedge, steps = 0;
        do {
            const Coedge& ce = m_coedges[walk];
            const uint32_t sv = ce.reversed ? m_edges[ce.edge].v1 : m_edges[ce.edge].v0;
            if (sv < vOut.size() && vOut[sv] >= 0) ring.push_back(static_cast<uint32_t>(vOut[sv]));
            const std::vector<uint32_t>& mids = edgeMid[ce.edge];
            if (!ce.reversed)
                for (uint32_t m : mids) ring.push_back(m);
            else
                for (auto it = mids.rbegin(); it != mids.rend(); ++it) ring.push_back(*it);
            walk = ce.next;
            if (++steps > m_coedges.size()) break;
        } while (walk != firstCoedge);
        return ring;
    };
    auto emitTri = [&](uint32_t a, uint32_t b, uint32_t c, const Vec3& nrm) {
        const Vec3 g = cross(sub(pos[b], pos[a]), sub(pos[c], pos[a]));
        nexus::geometry::Face f;  // the mesh Face, not brep::Face
        if (dot(g, nrm) < 0.f) f.indices = {a, c, b};
        else f.indices = {a, b, c};
        mesh.topology().addFace(std::move(f));
    };

    for (const Face& fc : m_faces) {
        if (!fc.alive || fc.outerLoop >= m_loops.size()) continue;
        const std::vector<uint32_t> ring = buildRing(m_loops[fc.outerLoop].first);
        if (ring.size() < 3) continue;

        Vec3 nrm{0.0, 0.0, 1.0};
        if (fc.surface < m_surfaces.size()) nrm = m_surfaces[fc.surface].normal;
        if (fc.reversed) nrm = {-nrm.x, -nrm.y, -nrm.z};

        // EVERY inner ring, not just the first. This used to keep one and break, which is
        // wrong for any face with two holes in it — a plate drilled twice, which is to say
        // most real parts. The consequence is not a cosmetic one: a hole that never reaches
        // the tessellation is not a hole as far as classifyPoint is concerned, because that
        // is a parity ray cast against these triangles. MEASURED on a 4x4x1 plate drilled
        // four times in sequence — the canonical chain — the second bore's own wall facets
        // were reported OUTSIDE the plate they sit inside (3 of 50 faces), were dropped from
        // the difference, left 8 boundary edges with one face instead of two, and the sew
        // refused. The chain died at the second hole.
        std::vector<std::vector<uint32_t>> inners;
        for (uint32_t il : fc.innerLoops) {
            if (il >= m_loops.size()) continue;
            std::vector<uint32_t> r = buildRing(m_loops[il].first);
            if (r.size() >= 3) inners.push_back(std::move(r));
        }

        // A CONVEX outer ring with no hole fans from its first vertex — the cheapest
        // correct triangulation, and the one every face used to get.
        //
        // It is only correct for a convex ring, though, and that used to be guaranteed: a
        // Line imprint splits a convex face into two convex faces, and an interior circle
        // takes the hole path below. An ARC BITE breaks it — the remainder of a bitten face
        // is concave by construction, its boundary dipping inward along the arc — and a fan
        // over a concave ring emits triangles that cover the notch a second time. Measured
        // on the +Z face of box(2,2,2) bitten by a cylinder offset to straddle the +X wall:
        // the remainder tessellated to area 4.65 where the whole face is 4.0, double-counting
        // the 0.65 lens that had just been cut away from it. Signed volume happened to
        // survive that (the duplicate triangles cancel), which is why the validators and the
        // volume all looked clean; only the unsigned area showed it. It matters beyond area:
        // toMesh feeds classifyPoint's parity ray, so a double-covered region flips
        // inside/outside for anything behind it.
        //
        // So convexity is now TESTED rather than assumed, and a concave ring goes to the
        // ear-clipper below — the same one the hole path already used.
        // The convexity test is a PLANAR one — it measures turns about the face normal — and
        // for a curved face `nrm` is not a face normal at all: a Cylinder surface stores its
        // AXIS there. Projecting a cylindrical side patch along its own axis collapses the
        // two upright edges to points, so the turns degenerate and the ring reads as concave;
        // it would then be ear-clipped in a frame that does not describe it. A non-planar
        // face therefore keeps the fan unconditionally, exactly as before — measured
        // consequence of getting this wrong: the centred cylinder-through-box boolean stopped
        // sewing, because classifyPoint's parity ray runs against this tessellation.
        {
            const bool planar = fc.surface < m_surfaces.size() &&
                                m_surfaces[fc.surface].kind == SurfaceKind::Plane;
            bool convex = inners.empty();
            if (convex && planar && ring.size() >= 3) {
                // Sign of the turn at each corner, in the face plane; a convex ring turns the
                // same way throughout. Collinear corners (zero) are ignored, not counted as
                // a reversal, so a redundant midpoint vertex — which the arc bite leaves on
                // the chord by design, and which every subdivided straight edge contributes
                // two of — does not misread as concavity.
                //
                // This reads `posD`, the DOUBLE positions, not the float ones the mesh
                // carries. A subdivision point on a straight edge is exactly collinear with
                // its neighbours, so its turn is algebraically zero — but narrowing the three
                // points to float perturbs each by an ulp and leaves a residue with a
                // confident SIGN. Measured on a box face bitten by a sphere (fuzz seed
                // 0xA17E51, iteration 102): the identical face read +0.0 at those corners in
                // one body and +1.47e-08 / −1.48e-08 in another whose vertices differ by a
                // single ulp. The negative one counted as a reversal, so the same face was
                // fanned in one body and ear-clipped in the other. The exact data was sitting
                // right there in the B-rep; only the copy the decision was read from had lost
                // the collinearity. The threshold is relative to match — the sine of the turn
                // angle, not the raw cross product, which scales with both edge lengths and
                // so asks a different question at every corner.
                //
                // Be precise about what this is worth, because it is easy to overclaim. It
                // moves NO volume and NO area: the flip only ever goes convex → "concave",
                // which routes a convex ring to the ear-clipper, and both routes cover the
                // ring exactly (swept over 2000 fuzz configurations, every planar face's
                // triangles sum to its own ring area). Reverting it leaves the conservation
                // identities unchanged to the digit. What it does buy is that the same patch
                // is cut into the SAME TRIANGLES wherever it appears — with this reverted, 13
                // of 1220 triangles still differ between operand and result on the fixture
                // above. That is the property BRepFanApexStability asserts, and it is
                // strictly stronger than any total, which is the whole reason this class of
                // defect survived so long behind clean validators.
                int turnSign = 0;
                for (size_t i = 0; i < ring.size() && convex; ++i) {
                    const Vec3& a = posD[ring[i]];
                    const Vec3& b = posD[ring[(i + 1) % ring.size()]];
                    const Vec3& c = posD[ring[(i + 2) % ring.size()]];
                    const Vec3 e1 = b - a, e2 = c - b;
                    const double sc2 = std::sqrt(e1.dot(e1)) * std::sqrt(e2.dot(e2));
                    if (sc2 <= 0.0) continue;  // a repeated point turns nowhere
                    const double t = e1.cross(e2).dot(nrm) / sc2;
                    const int s = (t > 1e-9) ? 1 : (t < -1e-9 ? -1 : 0);
                    if (s == 0) continue;
                    if (turnSign == 0) turnSign = s;
                    else if (s != turnSign) convex = false;
                }
            }
            if (convex) {
                // The fan's apex is chosen by GEOMETRY — the lexicographically smallest
                // vertex position — rather than by wherever the ring happens to start.
                //
                // For a PLANAR face this changes nothing that can be measured: every fan of
                // a planar polygon covers the same region and encloses the same volume. For
                // a CURVED patch it decides the answer. A four-vertex patch on a sphere is
                // not planar, so its two diagonals span different surfaces and enclose
                // different volumes, and which one a fan picks depends only on the ring's
                // first vertex.
                //
                // That is exactly the freedom a Boolean cannot afford. Difference reverses
                // the vertex ring of every face it takes from the second operand, and a
                // reversed ring starts at a different vertex, so the SAME patch was
                // triangulated one way inside the intersection and the other way inside the
                // difference. The two are supposed to cancel — the identity D + I = A holds
                // because the shared patch appears once in each with opposite orientation —
                // and they did not. Measured on box(2³) against sphere(r1.2) offset 0.5: the
                // box triangles summed to 8.000000010 as they should, while the 116 shared
                // sphere triangles came to +2.189830002 in the intersection and
                // -2.192173031 in the difference, leaving the difference short of the volume
                // it owed by 2.3e-03 — a few parts in ten thousand, watertight, and
                // invisible to every validator this kernel owns, since all of them are
                // topological and none of them weighs anything.
                //
                // A fan from a FIXED apex emits the same diagonals whichever way the ring is
                // traversed — only the winding flips, which is precisely what is wanted — so
                // pinning the apex to a property of the geometry makes the two copies
                // cancel exactly rather than approximately.
                //
                // "The same geometry" has to mean the same to WITHIN ROUNDING, though, and
                // the first cut of this took it to mean bitwise. It compared the float
                // positions with `==`, so a tie fell through to the next axis only when the
                // two coordinates agreed in every bit — and on a seam, sharing a coordinate
                // is the normal case, not a coincidence: every point imprinted onto a box
                // face carries that face's plane coordinate exactly. Which means the tie-
                // break was being decided by whatever noise sat in the last bit.
                //
                // MEASURED (fuzz seed 0xA17E51, iteration 102, sphere r1.51 against a box):
                // one ring held a B-rep vertex and an arc midpoint both at x = -0.45200936…,
                // agreeing to 15 significant figures. In the imprinted operand the midpoint's
                // x was 2.8e-16 MORE NEGATIVE and took the apex; in the union the two were
                // bitwise equal, the comparison fell through to y, and the vertex took it.
                // A different apex on a CURVED patch spans different diagonals and encloses
                // a different volume — over the body, U + I came out 1.2e-02 short of A + B,
                // on faces that are otherwise identical point for point.
                //
                // So the comparison is made on the DOUBLE positions, quantized to a grid far
                // above the rounding noise and far below any real feature. Rounding to a grid
                // (rather than comparing "within a tolerance") keeps this a total order, so
                // the minimum does not depend on where the ring starts. What remains is two
                // ring points landing on opposite sides of one grid boundary while being
                // 1e-16 apart — possible, no longer systematic, and worth 1e-12 of volume
                // rather than 1e-2.
                double sc = 1.0;
                for (const uint32_t vi : ring) {
                    const Vec3& p = posD[vi];
                    sc = std::max(sc, std::max(std::abs(p.x), std::max(std::abs(p.y),
                                                                       std::abs(p.z))));
                }
                const double grid = sc * 1e-12;
                auto snap = [grid](double v) { return std::floor(v / grid + 0.5) * grid; };
                size_t apex = 0;
                for (size_t i = 1; i < ring.size(); ++i) {
                    const Vec3& p = posD[ring[i]];
                    const Vec3& q = posD[ring[apex]];
                    const double px = snap(p.x), py = snap(p.y), pz = snap(p.z);
                    const double qx = snap(q.x), qy = snap(q.y), qz = snap(q.z);
                    if (px < qx || (px == qx && (py < qy || (py == qy && pz < qz)))) apex = i;
                }
                const size_t rn = ring.size();
                for (size_t i = 2; i < rn; ++i) {
                    nexus::geometry::Face f;
                    f.indices = {ring[apex], ring[(apex + i - 1) % rn], ring[(apex + i) % rn]};
                    mesh.topology().addFace(std::move(f));
                }
                continue;
            }
        }

        // Face with a hole (bridged into one simple polygon), or a concave face without
        // one: ear-clip it. (Multiple holes on one face are a rare follow-up.)
        // 2D frame of the face plane.
        const Vec3 u = normalize(sub(pos[ring[1]], pos[ring[0]]));
        const Vec3 vv = cross(nrm, u);
        const Vec3 org = pos[ring[0]];
        auto X = [&](uint32_t idx) { return dot(sub(pos[idx], org), u); };
        auto Y = [&](uint32_t idx) { return dot(sub(pos[idx], org), vv); };

        // Bridge every hole into the outer ring, one at a time, so what reaches the
        // ear-clipper is a single simple polygon.
        //
        // Each hole is joined by a two-way cut from its RIGHTMOST vertex M: cast a ray in
        // +X from M, take the first edge of the polygon-so-far that it meets, and bridge to
        // that edge's right-hand endpoint. The holes are merged right to left, so a hole
        // bridging leftward may land on a hole already merged — which is correct, and is why
        // the ray is cast against the growing polygon rather than against the outer ring.
        //
        // The bridge duplicates both endpoints, giving a degenerate channel of zero width
        // that the ear-clipper walks around; the duplicated indices are the SAME position
        // index, which is what lets its containment test exclude them by identity.
        std::vector<uint32_t> poly = ring;  // no holes → ear-clip the outer ring as it stands
        if (!inners.empty()) {
            auto rightmostX = [&](const std::vector<uint32_t>& h) {
                double m = X(h[0]);
                for (const uint32_t i : h) m = std::max(m, X(i));
                return m;
            };
            std::sort(inners.begin(), inners.end(),
                      [&](const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
                          return rightmostX(a) > rightmostX(b);
                      });
            for (const std::vector<uint32_t>& hole : inners) {
                size_t mIn = 0;
                for (size_t k = 1; k < hole.size(); ++k)
                    if (X(hole[k]) > X(hole[mIn])) mIn = k;
                const double mx = X(hole[mIn]), my = Y(hole[mIn]);

                size_t bestEdge = poly.size();
                double bestX = 0.0;
                for (size_t k = 0; k < poly.size(); ++k) {
                    const uint32_t a = poly[k], b = poly[(k + 1) % poly.size()];
                    const double ya = Y(a), yb = Y(b);
                    if ((ya > my) == (yb > my)) continue;  // edge does not straddle the ray
                    const double t = (my - ya) / (yb - ya);
                    const double xi = X(a) + t * (X(b) - X(a));
                    if (xi < mx) continue;  // behind M
                    if (bestEdge == poly.size() || xi < bestX) {
                        bestX = xi;
                        bestEdge = k;
                    }
                }
                // No edge to the right to bridge to — leave this hole out rather than splice
                // a cut that crosses the boundary. The face is then drawn solid there, which
                // is wrong but bounded; splicing blindly corrupts the whole polygon.
                if (bestEdge == poly.size()) continue;

                size_t pOut = bestEdge;
                {
                    const uint32_t a = poly[bestEdge], b = poly[(bestEdge + 1) % poly.size()];
                    if (X(b) > X(a)) pOut = (bestEdge + 1) % poly.size();
                }

                std::vector<uint32_t> merged;
                merged.reserve(poly.size() + hole.size() + 2);
                for (size_t k = 0; k <= pOut; ++k) merged.push_back(poly[k]);
                for (size_t k = 0; k < hole.size(); ++k)
                    merged.push_back(hole[(mIn + k) % hole.size()]);
                merged.push_back(hole[mIn]);
                for (size_t k = pOut; k < poly.size(); ++k) merged.push_back(poly[k]);
                poly = std::move(merged);
            }
        }

        // Ear-clip the simple polygon (CCW). Duplicate bridge vertices share their
        // pos index, so they are excluded from the containment test by identity.
        auto cross2 = [&](uint32_t a, uint32_t b, uint32_t c) {
            return (X(b) - X(a)) * (Y(c) - Y(a)) - (Y(b) - Y(a)) * (X(c) - X(a));
        };
        std::vector<uint32_t> idx(poly.size());
        for (uint32_t k = 0; k < poly.size(); ++k) idx[k] = k;
        double area = 0.0;
        for (size_t k = 0; k < idx.size(); ++k)
            area += cross2(poly[idx[0]], poly[idx[k]],
                           poly[idx[(k + 1) % idx.size()]]);  // sign only
        if (area < 0.0) std::reverse(idx.begin(), idx.end());
        auto inTri = [&](uint32_t p, uint32_t a, uint32_t b, uint32_t c) {
            const double d1 = cross2(a, b, p), d2 = cross2(b, c, p), d3 = cross2(c, a, p);
            const bool neg = d1 < 0.f || d2 < 0.f || d3 < 0.f;
            const bool ppos = d1 > 0.f || d2 > 0.f || d3 > 0.f;
            return !(neg && ppos);
        };
        size_t guard = 0;
        while (idx.size() > 3 && guard++ < 20000) {
            bool clipped = false;
            const size_t m2 = idx.size();
            for (size_t a = 0; a < m2; ++a) {
                const uint32_t pv = poly[idx[(a + m2 - 1) % m2]];
                const uint32_t cv = poly[idx[a]];
                const uint32_t nv = poly[idx[(a + 1) % m2]];
                if (cross2(pv, cv, nv) <= 1e-12f) continue;  // reflex / collinear
                bool ok = true;
                for (size_t k = 0; k < m2 && ok; ++k) {
                    const uint32_t q = poly[idx[k]];
                    if (q == pv || q == cv || q == nv) continue;  // skip bridge dups
                    if (inTri(q, pv, cv, nv)) ok = false;
                }
                if (!ok) continue;
                emitTri(pv, cv, nv, nrm);
                idx.erase(idx.begin() + static_cast<long>(a));
                clipped = true;
                break;
            }
            if (!clipped) break;
        }
        // Emit whatever is left. Normally that is the final triangle; but the loop above
        // gives up when it can find no ear (a self-touching ring, a bridge that grazes, a
        // numerically flat corner), and this used to emit NOTHING unless exactly three
        // vertices remained — silently dropping the remnant and leaving a HOLE in a shell
        // that is supposed to be closed. A hole is the worst outcome available here:
        // classifyPoint casts a parity ray against this tessellation, so a missing patch
        // flips inside/outside for everything behind it, and the whole watertight-or-empty
        // contract rests on that classification. Fanning the remnant can overlap itself
        // where the leftover is concave, which costs area accuracy — a real cost, but a
        // bounded one, and never a hole. Previously only holed faces reached this code;
        // routing concave faces here (so they stop being fanned wholesale) made the stall
        // path reachable for far more shapes, which is why it is no longer left silent.
        for (size_t k = 2; k < idx.size(); ++k)
            emitTri(poly[idx[0]], poly[idx[k - 1]], poly[idx[k]], nrm);
    }
    return mesh;
}

Mesh Body::tessellateTrimmedFace(uint32_t faceId, uint32_t gridRes, Tolerance tol) const
{
    Mesh out;
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return out;
    const Face& fc = m_faces[faceId];
    if (fc.surface >= m_surfaces.size()) return out;
    const Surface& surf = m_surfaces[fc.surface];
    if (surf.kind != SurfaceKind::Nurbs || surf.nurbs >= m_nurbsSurfaces.size()) return out;
    if (gridRes < 1u) return out;

    // Assemble a loop's pcurve chain into a closed (u,v) polygon. Every coedge of
    // the loop must carry a pcurve, and consecutive pcurves must join (end of one
    // == start of the next) — else the loop is not fully trimmed and we bail.
    auto buildTrimLoop = [&](uint32_t firstCoedge,
                             std::vector<std::pair<float, float>>& poly) -> bool {
        poly.clear();
        if (firstCoedge >= m_coedges.size()) return false;
        uint32_t walk = firstCoedge, steps = 0;
        do {
            const Coedge& ce = m_coedges[walk];
            if (!ce.pcurve.present) return false;
            const std::pair<float, float> start{ce.pcurve.u0, ce.pcurve.v0};
            if (!poly.empty()) {
                // The previous pcurve's end must meet this one's start.
                const auto& prevEnd = poly.back();
                if (!tol.nearlyEqual(prevEnd.first, start.first) ||
                    !tol.nearlyEqual(prevEnd.second, start.second))
                    return false;
                poly.pop_back();  // drop the shared point; re-added as this start
            }
            poly.push_back(start);
            for (const auto& ip : ce.pcurve.interior) poly.push_back(ip);  // curved trim samples
            poly.emplace_back(ce.pcurve.u1, ce.pcurve.v1);
            walk = ce.next;
            if (++steps > m_coedges.size()) return false;
        } while (walk != firstCoedge);
        // The last pcurve's end must close back to the first's start.
        if (poly.size() < 4) return false;
        if (!tol.nearlyEqual(poly.front().first, poly.back().first) ||
            !tol.nearlyEqual(poly.front().second, poly.back().second))
            return false;
        poly.pop_back();  // drop the duplicated closing point
        return poly.size() >= 3;
    };

    std::vector<std::vector<std::pair<float, float>>> loops;
    {
        std::vector<std::pair<float, float>> outer;
        if (fc.outerLoop >= m_loops.size()) return out;
        if (!buildTrimLoop(m_loops[fc.outerLoop].first, outer)) return out;
        loops.push_back(std::move(outer));
        for (uint32_t il : fc.innerLoops) {  // inner loops are trim holes
            if (il >= m_loops.size()) continue;
            std::vector<std::pair<float, float>> inner;
            if (buildTrimLoop(m_loops[il].first, inner)) loops.push_back(std::move(inner));
        }
    }

    // Even-odd point-in-region test across all trim loops (ray cast +u).
    auto inRegion = [&](float u, float v) -> bool {
        bool inside = false;
        for (const auto& lp : loops) {
            const size_t n = lp.size();
            for (size_t i = 0, j = n - 1; i < n; j = i++) {
                const double ui = lp[i].first, vi = lp[i].second;
                const double uj = lp[j].first, vj = lp[j].second;
                if (((vi > v) != (vj > v)) &&
                    (u < (uj - ui) * (v - vi) / (vj - vi) + ui))
                    inside = !inside;
            }
        }
        return inside;
    };

    const NurbsSurface& ns = m_nurbsSurfaces[surf.nurbs];
    const auto [u0, u1] = ns.domainU();
    const auto [v0, v1] = ns.domainV();
    if (!isFinite(u0) || !isFinite(u1) || !isFinite(v0) || !isFinite(v1)) return out;

    const uint32_t g = gridRes;
    const uint32_t stride = g + 1u;
    auto uAt = [&](uint32_t i) { return u0 + (u1 - u0) * (static_cast<float>(i) / static_cast<float>(g)); };
    auto vAt = [&](uint32_t j) { return v0 + (v1 - v0) * (static_cast<float>(j) / static_cast<float>(g)); };
    // Grid-sample the parameter domain; every sample is evaluated ON the surface.
    std::vector<nexus::render::Vec3> pos(static_cast<size_t>(stride) * stride);
    for (uint32_t j = 0; j <= g; ++j)
        for (uint32_t i = 0; i <= g; ++i)
            pos[static_cast<size_t>(j) * stride + i] = ns.evaluate(uAt(i), vAt(j));
    out.attributes().setPositions(pos);

    // Emit a cell's two triangles when its CENTRE lies inside the trim region.
    // Classifying by centre (never on a grid line, so robust even when the trim
    // boundary is grid-aligned) makes the tessellated area converge to the true
    // trimmed area — exact when the trim boundary lands on grid lines.
    for (uint32_t j = 0; j < g; ++j) {
        const double vc = 0.5f * (vAt(j) + vAt(j + 1));
        for (uint32_t i = 0; i < g; ++i) {
            const double uc = 0.5f * (uAt(i) + uAt(i + 1));
            if (!inRegion(uc, vc)) continue;
            const uint32_t a = j * stride + i;
            const uint32_t b = j * stride + (i + 1);
            const uint32_t c = (j + 1) * stride + (i + 1);
            const uint32_t d = (j + 1) * stride + i;
            nexus::geometry::Face f0;
            f0.indices = {a, b, c};
            out.topology().addFace(std::move(f0));
            nexus::geometry::Face f1;
            f1.indices = {a, c, d};
            out.topology().addFace(std::move(f1));
        }
    }
    return out;
}

uint32_t Body::addTrimHole(uint32_t faceId, const std::vector<Vec3>& ring)
{
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return kInvalid;
    const size_t n = ring.size();
    if (n < 3) return kInvalid;
    for (const Vec3& p : ring)
        if (!isFinite(p)) return kInvalid;

    // New ring vertices (the hole boundary).
    const uint32_t baseV = static_cast<uint32_t>(m_verts.size());
    for (const Vec3& p : ring) {
        Vertex vx;
        vx.point = p;
        m_verts.push_back(vx);
    }

    // Inner loop.
    const uint32_t loopId = static_cast<uint32_t>(m_loops.size());
    m_loops.push_back({});
    m_loops[loopId].face = faceId;
    m_loops[loopId].outer = false;

    const uint32_t firstCoedge = static_cast<uint32_t>(m_coedges.size());
    for (size_t j = 0; j < n; ++j) {
        const uint32_t a = baseV + static_cast<uint32_t>(j);
        const uint32_t c = baseV + static_cast<uint32_t>((j + 1) % n);

        const uint32_t curveId = static_cast<uint32_t>(m_curves.size());
        Curve cur;
        cur.kind = CurveKind::Line;
        cur.origin = m_verts[a].point;
        const Vec3 d = sub(m_verts[c].point, m_verts[a].point);
        cur.dir = normalize(d);
        m_curves.push_back(cur);

        const uint32_t edgeId = static_cast<uint32_t>(m_edges.size());
        Edge ed;
        ed.curve = curveId;
        ed.v0 = a;
        ed.v1 = c;
        ed.t0 = 0.f;
        ed.t1 = length(d);
        m_edges.push_back(ed);

        const uint32_t coedgeId = static_cast<uint32_t>(m_coedges.size());
        Coedge ce;
        ce.edge = edgeId;
        ce.reversed = false;  // edge stored a→c
        ce.loop = loopId;
        m_coedges.push_back(ce);
        m_edges[edgeId].coedge = coedgeId;
        if (m_verts[a].coedge == kInvalid) m_verts[a].coedge = coedgeId;
    }
    for (size_t j = 0; j < n; ++j) {
        const uint32_t cur = firstCoedge + static_cast<uint32_t>(j);
        m_coedges[cur].next = firstCoedge + static_cast<uint32_t>((j + 1) % n);
        m_coedges[cur].prev = firstCoedge + static_cast<uint32_t>((j + n - 1) % n);
    }
    m_loops[loopId].first = firstCoedge;
    m_faces[faceId].innerLoops.push_back(loopId);
    return firstCoedge;
}

bool Body::setEdgeArc(uint32_t edgeId, const Vec3& center, const Vec3& axis, double radius, Tolerance tol)
{
    if (edgeId >= m_edges.size() || !m_edges[edgeId].alive) return false;
    const uint32_t v0 = m_edges[edgeId].v0, v1 = m_edges[edgeId].v1;
    if (v0 >= m_verts.size() || v1 >= m_verts.size()) return false;
    const Vec3 p0 = m_verts[v0].point, p1 = m_verts[v1].point;
    const Vec3 ax = normalize(axis);
    const Vec3 d0 = sub(p0, center), d1 = sub(p1, center);
    // Both endpoints must lie on the circle (radius, in the plane through center
    // perpendicular to axis).
    if (!tol.nearlyEqual(length(d0), radius) || !tol.nearlyEqual(length(d1), radius)) return false;
    if (!tol.isZero(dot(d0, ax)) || !tol.isZero(dot(d1, ax))) return false;

    const Vec3 ref0 = normalize(d0);      // radius direction at v0 (t = 0)
    const Vec3 bi = cross(ax, ref0);      // sweep direction
    const double ang = std::atan2(dot(d1, bi), dot(d1, ref0));  // signed short-arc angle

    const uint32_t curveId = m_edges[edgeId].curve;
    Curve& cu = m_curves[curveId];        // per-edge curve (not shared) — safe to retag
    cu.kind = CurveKind::Circle;
    cu.origin = center;
    cu.dir = ax;
    cu.ref = ref0;
    cu.radius = radius;
    m_edges[edgeId].t0 = 0.f;
    m_edges[edgeId].t1 = ang;
    return true;
}

bool Body::setCoedgePcurve(uint32_t coedgeId, float u0, float v0, float u1, float v1, Tolerance tol)
{
    if (coedgeId >= m_coedges.size() || !m_coedges[coedgeId].alive) return false;
    if (!isFinite(u0) || !isFinite(v0) || !isFinite(u1) || !isFinite(v1)) return false;
    Coedge& ce = m_coedges[coedgeId];
    if (ce.edge >= m_edges.size() || ce.loop >= m_loops.size()) return false;
    const Edge& ed = m_edges[ce.edge];
    if (ed.v0 >= m_verts.size() || ed.v1 >= m_verts.size()) return false;
    const uint32_t faceId = m_loops[ce.loop].face;
    if (faceId >= m_faces.size()) return false;
    const uint32_t surfId = m_faces[faceId].surface;
    if (surfId >= m_surfaces.size()) return false;

    // Directed endpoints: a pcurve runs the coedge's start → end vertex.
    const uint32_t sV = ce.reversed ? ed.v1 : ed.v0;
    const uint32_t eV = ce.reversed ? ed.v0 : ed.v1;
    // Each parameter-space endpoint must map (through the face surface) back onto
    // the corresponding 3D vertex — else the pcurve does not trim this boundary.
    if (!coincident(surfacePoint(surfId, u0, v0), m_verts[sV].point, tol)) return false;
    if (!coincident(surfacePoint(surfId, u1, v1), m_verts[eV].point, tol)) return false;

    ce.pcurve = Pcurve{true, u0, v0, u1, v1, {}};
    return true;
}

bool Body::setCoedgePcurvePolyline(uint32_t coedgeId,
                                   const std::vector<std::pair<float, float>>& points,
                                   Tolerance tol)
{
    if (points.size() < 2) return false;
    if (coedgeId >= m_coedges.size() || !m_coedges[coedgeId].alive) return false;
    Coedge& ce = m_coedges[coedgeId];
    if (ce.edge >= m_edges.size() || ce.loop >= m_loops.size()) return false;
    const Edge& ed = m_edges[ce.edge];
    if (ed.v0 >= m_verts.size() || ed.v1 >= m_verts.size()) return false;
    const uint32_t faceId = m_loops[ce.loop].face;
    if (faceId >= m_faces.size()) return false;
    const uint32_t surfId = m_faces[faceId].surface;
    if (surfId >= m_surfaces.size()) return false;

    // Parameter domain (for the in-domain guard on curved trims).
    const Surface& surf = m_surfaces[surfId];
    bool haveDomain = false;
    float du0 = 0.f, du1 = 0.f, dv0 = 0.f, dv1 = 0.f;
    if (surf.kind == SurfaceKind::Nurbs && surf.nurbs < m_nurbsSurfaces.size()) {
        const auto [a, b] = m_nurbsSurfaces[surf.nurbs].domainU();
        const auto [c, d] = m_nurbsSurfaces[surf.nurbs].domainV();
        du0 = a; du1 = b; dv0 = c; dv1 = d;
        haveDomain = isFinite(a) && isFinite(b) && isFinite(c) && isFinite(d);
    }
    const double su = haveDomain ? tol.at(du1 - du0) : 0.f;
    const double sv = haveDomain ? tol.at(dv1 - dv0) : 0.f;
    for (const auto& p : points) {
        if (!isFinite(p.first) || !isFinite(p.second)) return false;
        if (haveDomain && (p.first < du0 - su || p.first > du1 + su ||
                           p.second < dv0 - sv || p.second > dv1 + sv))
            return false;  // trim point outside the surface's parameter domain
    }

    // Endpoints must map (through the surface) onto the coedge's directed 3D
    // vertices, exactly as for a straight pcurve.
    const uint32_t sV = ce.reversed ? ed.v1 : ed.v0;
    const uint32_t eV = ce.reversed ? ed.v0 : ed.v1;
    if (!coincident(surfacePoint(surfId, points.front().first, points.front().second),
                    m_verts[sV].point, tol))
        return false;
    if (!coincident(surfacePoint(surfId, points.back().first, points.back().second),
                    m_verts[eV].point, tol))
        return false;

    Pcurve pc;
    pc.present = true;
    pc.u0 = points.front().first;
    pc.v0 = points.front().second;
    pc.u1 = points.back().first;
    pc.v1 = points.back().second;
    pc.interior.assign(points.begin() + 1, points.end() - 1);
    ce.pcurve = std::move(pc);
    return true;
}

// ──────────── Serialization ──────────────────────────────────────────────────

namespace {
constexpr std::uint32_t kBRepMagic = 0x5242584Eu;  // 'NXBR'
constexpr std::uint32_t kBRepVersion = 4u;  // v2: per-coedge pcurves; v3: curved pcurves; v4: DOUBLE positions

void putU8(std::vector<std::uint8_t>& o, std::uint8_t v) { o.push_back(v); }
void putU32(std::vector<std::uint8_t>& o, std::uint32_t v)
{
    o.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}
void putU64(std::vector<std::uint8_t>& o, std::uint64_t v)
{
    for (int i = 0; i < 8; ++i) o.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
}
void putI32(std::vector<std::uint8_t>& o, std::int32_t v) { putU32(o, std::bit_cast<std::uint32_t>(v)); }
void putF32(std::vector<std::uint8_t>& o, float v) { putU32(o, std::bit_cast<std::uint32_t>(v)); }
void putF64(std::vector<std::uint8_t>& o, double v) { putU64(o, std::bit_cast<std::uint64_t>(v)); }
// v4 writes positions as DOUBLE. Earlier versions wrote float and are still read as float —
// the reader branches on the version, so every existing .nxb keeps loading.
void putVec3(std::vector<std::uint8_t>& o, const Vec3& v) { putF64(o, v.x); putF64(o, v.y); putF64(o, v.z); }
// The NURBS control-point store is still single precision; it is written as it always was.
void putVec3f(std::vector<std::uint8_t>& o, const nexus::render::Vec3& v) { putF32(o, v.x); putF32(o, v.y); putF32(o, v.z); }

struct Reader {
    const std::vector<std::uint8_t>& b;
    std::size_t off = 0;
    bool ok = true;
    bool u8(std::uint8_t& out)
    {
        if (!ok || off + 1 > b.size()) return ok = false;
        out = b[off++];
        return true;
    }
    bool u64(std::uint64_t& out)
    {
        if (!ok || off + 8 > b.size()) return ok = false;
        out = 0;
        for (int i = 0; i < 8; ++i) out |= static_cast<std::uint64_t>(b[off + i]) << (i * 8);
        off += 8;
        return true;
    }
    bool u32(std::uint32_t& out)
    {
        if (!ok || off + 4 > b.size()) return ok = false;
        out = static_cast<std::uint32_t>(b[off]) | (static_cast<std::uint32_t>(b[off + 1]) << 8) |
              (static_cast<std::uint32_t>(b[off + 2]) << 16) |
              (static_cast<std::uint32_t>(b[off + 3]) << 24);
        off += 4;
        return true;
    }
    bool i32(std::int32_t& out)
    {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        out = std::bit_cast<std::int32_t>(u);
        return true;
    }
    bool f32(float& out)
    {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        out = std::bit_cast<float>(u);
        if (!isFinite(out)) return ok = false;  // reject non-finite on read
        return true;
    }
    // v4 stores doubles; v1-3 stored floats. The reader is told which by the caller.
    bool f64(double& out)
    {
        std::uint64_t u = 0;
        if (!u64(u)) return false;
        out = std::bit_cast<double>(u);
        if (!isFinite(out)) return ok = false;  // reject non-finite on read
        return true;
    }
    // Curve/surface radii and edge parameters widened with the positions in v4.
    bool scalar(double& out, bool wide)
    {
        if (wide) return f64(out);
        float f = 0.f;
        if (!f32(f)) return false;
        out = f;
        return true;
    }
    bool vec3(Vec3& out, bool wide)
    {
        if (wide) return f64(out.x) && f64(out.y) && f64(out.z);
        float x = 0.f, y = 0.f, z = 0.f;
        if (!f32(x) || !f32(y) || !f32(z)) return false;
        out = Vec3{x, y, z};
        return true;
    }
    bool vec3f(nexus::render::Vec3& out) { return f32(out.x) && f32(out.y) && f32(out.z); }
    bool boolean(bool& out)
    {
        std::uint8_t v = 0;
        if (!u8(v)) return false;
        out = (v != 0);
        return true;
    }
    // A length prefix that cannot exceed the remaining bytes (each element ≥1B),
    // so a garbage buffer can't trigger a huge allocation.
    bool count(std::uint32_t& out)
    {
        if (!u32(out)) return false;
        if (out > b.size()) return ok = false;
        return true;
    }
    bool u32Vec(std::vector<std::uint32_t>& out)
    {
        std::uint32_t n = 0;
        if (!count(n)) return false;
        out.resize(n);
        for (auto& e : out)
            if (!u32(e)) return false;
        return true;
    }
    bool floatVec(std::vector<float>& out)
    {
        std::uint32_t n = 0;
        if (!count(n)) return false;
        out.resize(n);
        for (auto& e : out)
            if (!f32(e)) return false;
        return true;
    }
    // NURBS control points are single precision (that is what NurbsSurface holds), so this
    // reader is explicitly the float one — it is not the B-rep's own position reader.
    bool vec3Vec(std::vector<nexus::render::Vec3>& out)
    {
        std::uint32_t n = 0;
        if (!count(n)) return false;
        out.resize(n);
        for (auto& e : out)
            if (!vec3f(e)) return false;   // NURBS control points stay single precision
        return true;
    }
};
}  // namespace

std::vector<std::uint8_t> Body::serialize() const
{
    std::vector<std::uint8_t> o;
    putU32(o, kBRepMagic);
    putU32(o, kBRepVersion);

    putU32(o, static_cast<std::uint32_t>(m_verts.size()));
    for (const Vertex& v : m_verts) {
        putVec3(o, v.point);
        putU32(o, v.coedge);
        putU8(o, v.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_edges.size()));
    for (const Edge& e : m_edges) {
        putU32(o, e.curve);
        putU32(o, e.v0);
        putU32(o, e.v1);
        putF64(o, e.t0);
        putF64(o, e.t1);
        putU32(o, e.coedge);
        putU8(o, e.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_coedges.size()));
    for (const Coedge& c : m_coedges) {
        putU32(o, c.edge);
        putU8(o, c.reversed ? 1u : 0u);
        putU32(o, c.loop);
        putU32(o, c.next);
        putU32(o, c.prev);
        putU32(o, c.partner);
        putU8(o, c.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_loops.size()));
    for (const Loop& l : m_loops) {
        putU32(o, l.face);
        putU32(o, l.first);
        putU8(o, l.outer ? 1u : 0u);
        putU8(o, l.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_faces.size()));
    for (const Face& f : m_faces) {
        putU32(o, f.surface);
        putU8(o, f.reversed ? 1u : 0u);
        putU32(o, f.outerLoop);
        putU32(o, static_cast<std::uint32_t>(f.innerLoops.size()));
        for (std::uint32_t il : f.innerLoops) putU32(o, il);
        putU32(o, f.shell);
        putU8(o, f.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_shells.size()));
    for (const Shell& s : m_shells) {
        putU32(o, static_cast<std::uint32_t>(s.faces.size()));
        for (std::uint32_t fi : s.faces) putU32(o, fi);
        putU8(o, s.closed ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_solids.size()));
    for (const Solid& s : m_solids) {
        putU32(o, static_cast<std::uint32_t>(s.shells.size()));
        for (std::uint32_t sh : s.shells) putU32(o, sh);
    }
    putU32(o, static_cast<std::uint32_t>(m_curves.size()));
    for (const Curve& c : m_curves) {
        putU8(o, static_cast<std::uint8_t>(c.kind));
        putVec3(o, c.origin);
        putVec3(o, c.dir);
        putVec3(o, c.ref);
        putF64(o, c.radius);
        putU32(o, c.nurbs);
    }
    putU32(o, static_cast<std::uint32_t>(m_surfaces.size()));
    for (const Surface& s : m_surfaces) {
        putU8(o, static_cast<std::uint8_t>(s.kind));
        putVec3(o, s.origin);
        putVec3(o, s.normal);
        putVec3(o, s.uAxis);
        putF64(o, s.radius);
        putU32(o, s.nurbs);
    }
    putU32(o, static_cast<std::uint32_t>(m_nurbsSurfaces.size()));
    for (const NurbsSurface& n : m_nurbsSurfaces) {
        putI32(o, n.degreeU());
        putI32(o, n.degreeV());
        putI32(o, n.controlPointCountU());
        putI32(o, n.controlPointCountV());
        putU32(o, static_cast<std::uint32_t>(n.knotU().size()));
        for (float k : n.knotU()) putF32(o, k);
        putU32(o, static_cast<std::uint32_t>(n.knotV().size()));
        for (float k : n.knotV()) putF32(o, k);
        putU32(o, static_cast<std::uint32_t>(n.controlPoints().size()));
        for (const nexus::render::Vec3& p : n.controlPoints()) putVec3f(o, p);
        putU32(o, static_cast<std::uint32_t>(n.weights().size()));
        for (float w : n.weights()) putF32(o, w);
    }
    // v2: parameter-space trim curves (pcurves), a SPARSE trailing section keyed
    // by coedge index — so a v1 blob (which lacks it) decodes identically under
    // the v2 reader, and only coedges that carry a pcurve cost bytes.
    std::uint32_t nPc = 0;
    for (const Coedge& c : m_coedges)
        if (c.pcurve.present) ++nPc;
    putU32(o, nPc);
    for (std::uint32_t i = 0; i < m_coedges.size(); ++i) {
        const Pcurve& pc = m_coedges[i].pcurve;
        if (!pc.present) continue;
        putU32(o, i);
        putF32(o, pc.u0);
        putF32(o, pc.v0);
        putF32(o, pc.u1);
        putF32(o, pc.v1);
        // v3: curved (polyline) interior points.
        putU32(o, static_cast<std::uint32_t>(pc.interior.size()));
        for (const auto& ip : pc.interior) {
            putF32(o, ip.first);
            putF32(o, ip.second);
        }
    }
    return o;
}

std::optional<Body> Body::deserialize(const std::vector<std::uint8_t>& bytes)
{
    Reader r{bytes};
    std::uint32_t magic = 0, version = 0;
    if (!r.u32(magic) || magic != kBRepMagic) return std::nullopt;
    if (!r.u32(version) || version < 1u || version > kBRepVersion) return std::nullopt;
    // v4 onward stores positions as double; everything earlier stored float.
    const bool wide = version >= 4u;

    Body b;
    std::uint32_t n = 0;

    if (!r.count(n)) return std::nullopt;
    b.m_verts.resize(n);
    for (Vertex& v : b.m_verts)
        if (!r.vec3(v.point, wide) || !r.u32(v.coedge) || !r.boolean(v.alive)) return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_edges.resize(n);
    for (Edge& e : b.m_edges)
        if (!r.u32(e.curve) || !r.u32(e.v0) || !r.u32(e.v1) || !r.scalar(e.t0, wide) ||
            !r.scalar(e.t1, wide) || !r.u32(e.coedge) || !r.boolean(e.alive))
            return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_coedges.resize(n);
    for (Coedge& c : b.m_coedges)
        if (!r.u32(c.edge) || !r.boolean(c.reversed) || !r.u32(c.loop) || !r.u32(c.next) ||
            !r.u32(c.prev) || !r.u32(c.partner) || !r.boolean(c.alive))
            return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_loops.resize(n);
    for (Loop& l : b.m_loops)
        if (!r.u32(l.face) || !r.u32(l.first) || !r.boolean(l.outer) || !r.boolean(l.alive))
            return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_faces.resize(n);
    for (Face& f : b.m_faces) {
        if (!r.u32(f.surface) || !r.boolean(f.reversed) || !r.u32(f.outerLoop)) return std::nullopt;
        if (!r.u32Vec(f.innerLoops)) return std::nullopt;
        if (!r.u32(f.shell) || !r.boolean(f.alive)) return std::nullopt;
    }

    if (!r.count(n)) return std::nullopt;
    b.m_shells.resize(n);
    for (Shell& s : b.m_shells) {
        if (!r.u32Vec(s.faces)) return std::nullopt;
        if (!r.boolean(s.closed)) return std::nullopt;
    }

    if (!r.count(n)) return std::nullopt;
    b.m_solids.resize(n);
    for (Solid& s : b.m_solids)
        if (!r.u32Vec(s.shells)) return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_curves.resize(n);
    for (Curve& c : b.m_curves) {
        std::uint8_t kind = 0;
        if (!r.u8(kind) || !r.vec3(c.origin, wide) || !r.vec3(c.dir, wide) || !r.vec3(c.ref, wide) ||
            !r.scalar(c.radius, wide) || !r.u32(c.nurbs))
            return std::nullopt;
        c.kind = static_cast<CurveKind>(kind);
    }

    if (!r.count(n)) return std::nullopt;
    b.m_surfaces.resize(n);
    for (Surface& s : b.m_surfaces) {
        std::uint8_t kind = 0;
        if (!r.u8(kind) || !r.vec3(s.origin, wide) || !r.vec3(s.normal, wide) || !r.vec3(s.uAxis, wide) ||
            !r.scalar(s.radius, wide) || !r.u32(s.nurbs))
            return std::nullopt;
        s.kind = static_cast<SurfaceKind>(kind);
    }

    if (!r.count(n)) return std::nullopt;
    b.m_nurbsSurfaces.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        std::int32_t degU = 0, degV = 0, nU = 0, nV = 0;
        std::vector<float> knotU, knotV, weights;
        std::vector<nexus::render::Vec3> ctl;  // NURBS control points are single precision
        if (!r.i32(degU) || !r.i32(degV) || !r.i32(nU) || !r.i32(nV)) return std::nullopt;
        if (!r.floatVec(knotU) || !r.floatVec(knotV) || !r.vec3Vec(ctl) || !r.floatVec(weights))
            return std::nullopt;
        if (weights.empty())
            b.m_nurbsSurfaces.emplace_back(degU, degV, std::move(knotU), std::move(knotV),
                                           std::move(ctl), nU, nV);
        else
            b.m_nurbsSurfaces.emplace_back(degU, degV, std::move(knotU), std::move(knotV),
                                           std::move(ctl), nU, nV, std::move(weights));
    }

    // v2: sparse pcurve trailing section (absent in v1 blobs → skipped).
    if (version >= 2u) {
        if (!r.count(n)) return std::nullopt;
        for (std::uint32_t i = 0; i < n; ++i) {
            std::uint32_t idx = 0;
            Pcurve pc;
            if (!r.u32(idx) || !r.f32(pc.u0) || !r.f32(pc.v0) || !r.f32(pc.u1) || !r.f32(pc.v1))
                return std::nullopt;
            if (version >= 3u) {  // v3: curved (polyline) interior points
                std::uint32_t m = 0;
                if (!r.count(m)) return std::nullopt;
                pc.interior.resize(m);
                for (auto& ip : pc.interior)
                    if (!r.f32(ip.first) || !r.f32(ip.second)) return std::nullopt;
            }
            if (idx >= b.m_coedges.size()) return std::nullopt;
            pc.present = true;
            b.m_coedges[idx].pcurve = std::move(pc);
        }
    }

    if (!r.ok) return std::nullopt;
    return b;
}

// ──────────── Primitives ─────────────────────────────────────────────────────

Body makeBox(float width, float height, float depth)
{
    const double w = width * 0.5, h = height * 0.5, d = depth * 0.5;
    const std::vector<Vec3> pts = {
        {-w, -h, -d}, {w, -h, -d}, {w, h, -d}, {-w, h, -d},
        {-w, -h, d},  {w, -h, d},  {w, h, d},  {-w, h, d},
    };
    // Each face wound CCW as seen from outside (so shared edges are traversed in
    // opposite directions by adjacent faces → coedges partner and the shell is
    // watertight). The Plane surface takes the geometric outward normal.
    const uint32_t rings[6][4] = {
        {0, 3, 2, 1},  // -Z
        {4, 5, 6, 7},  // +Z
        {0, 1, 5, 4},  // -Y
        {2, 3, 7, 6},  // +Y
        {0, 4, 7, 3},  // -X
        {1, 2, 6, 5},  // +X
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(6);
    for (const auto& ring : rings) {
        Body::FaceDef fd;
        fd.loop = {ring[0], ring[1], ring[2], ring[3]};
        const Vec3& p0 = pts[ring[0]];
        const Vec3 e1 = sub(pts[ring[1]], p0);
        const Vec3 e2 = sub(pts[ring[3]], p0);
        Surface s;
        s.kind = SurfaceKind::Plane;
        s.origin = p0;
        s.normal = normalize(cross(e1, e2));  // outward for CCW-from-outside winding
        s.uAxis = normalize(e1);
        fd.surface = s;
        defs.push_back(std::move(fd));
    }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

Body makeOpenBox(float width, float depth, float height)
{
    if (!isFinite(width) || !isFinite(depth) || !isFinite(height) || width <= 0. ||
        depth <= 0. || height <= 0.)
        return Body{};
    const double x = width * 0.5, y = depth * 0.5, z = height * 0.5;
    // b0..b3 floor ring (z=−z), t0..t3 rim ring (z=+z).
    const std::vector<Vec3> pts = {
        {-x, -y, -z}, {x, -y, -z}, {x, y, -z}, {-x, y, -z},
        {-x, -y, z},  {x, -y, z},  {x, y, z},  {-x, y, z},
    };
    auto planeDef = [&](std::vector<uint32_t> loop) {
        Body::FaceDef fd;
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[loop[0]];
        fd.surface.normal = normalize(
            cross(sub(pts[loop[1]], pts[loop[0]]), sub(pts[loop[2]], pts[loop[0]])));
        fd.surface.uAxis = normalize(sub(pts[loop[1]], pts[loop[0]]));
        fd.loop = std::move(loop);
        return fd;
    };
    // Floor + four walls, outward-wound; the +Z top is OMITTED → an open shell
    // whose top rim (t0..t3) edges are boundary edges.
    std::vector<Body::FaceDef> defs;
    defs.reserve(5);
    defs.push_back(planeDef({0, 3, 2, 1}));  // floor (−Z outward)
    defs.push_back(planeDef({0, 1, 5, 4}));  // −Y wall
    defs.push_back(planeDef({1, 2, 6, 5}));  // +X wall
    defs.push_back(planeDef({2, 3, 7, 6}));  // +Y wall
    defs.push_back(planeDef({3, 0, 4, 7}));  // −X wall

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

// A capped cylinder along +Z: n side quads on a Cylinder surface + top/bottom
// planar n-gon caps. V=2n, E=3n, F=n+2 → euler 2.
Body makeCylinder(float radius, float height, uint32_t segments)
{
    const uint32_t n = std::max(segments, 3u);
    const double h = height * 0.5;
    const double twoPi = 6.283185307179586476925286766559;

    std::vector<Vec3> pts(static_cast<size_t>(n) * 2u);
    for (uint32_t i = 0; i < n; ++i) {
        const double ang = twoPi * static_cast<double>(i) / static_cast<double>(n);
        const double cx = std::cos(ang) * radius, cy = std::sin(ang) * radius;
        pts[i] = {cx, cy, -h};              // bottom ring
        pts[static_cast<size_t>(n) + i] = {cx, cy, h};  // top ring
    }

    auto planeSurface = [](const Vec3& o, const Vec3& nrm) {
        Surface s;
        s.kind = SurfaceKind::Plane;
        s.origin = o;
        s.normal = nrm;
        s.uAxis = {1., 0., 0.};
        return s;
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(static_cast<size_t>(n) + 2u);

    // Bottom cap: descending indices → CW in XY → CCW seen from −Z (outward −Z).
    Body::FaceDef bottom;
    bottom.loop.reserve(n);
    bottom.loop.push_back(0u);
    for (uint32_t i = n; i-- > 1;) bottom.loop.push_back(i);
    bottom.surface = planeSurface({0., 0., -h}, {0., 0., -1.});
    defs.push_back(std::move(bottom));

    // Top cap: ascending indices → CCW seen from +Z (outward +Z).
    Body::FaceDef top;
    top.loop.reserve(n);
    for (uint32_t i = 0; i < n; ++i) top.loop.push_back(n + i);
    top.surface = planeSurface({0., 0., h}, {0., 0., 1.});
    defs.push_back(std::move(top));

    // Side quads: {bottom[i], bottom[i+1], top[i+1], top[i]} — traverses each
    // shared edge opposite to its cap / neighbour side, so coedges partner.
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = (i + 1) % n;
        Body::FaceDef side;
        side.loop = {i, j, n + j, n + i};
        side.surface.kind = SurfaceKind::Cylinder;
        side.surface.origin = {0., 0., 0.};
        side.surface.normal = {0., 0., 1.};  // axis
        side.surface.uAxis = {1., 0., 0.};
        side.surface.radius = radius;
        defs.push_back(std::move(side));
    }

    auto b = Body::fromFaces(pts, defs);
    if (!b.has_value()) return Body{};
    // Upgrade the ring edges (both endpoints at the same height) to Circle arcs
    // about the axis, so toMesh(subdivisions) renders the cylinder smoothly.
    for (uint32_t e = 0; e < static_cast<uint32_t>(b->edgeCount()); ++e) {
        const Vec3& p0 = b->vertex(b->edge(e).v0).point;
        const Vec3& p1 = b->vertex(b->edge(e).v1).point;
        if (std::abs(p0.z - p1.z) < 1e-5f)
            b->setEdgeArc(e, {0., 0., p0.z}, {0., 0., 1.}, radius);
    }
    return std::move(*b);
}

// A cone along +Z: apex + bottom ring, n triangular sides + one planar cap.
// V=n+1, E=2n, F=n+1 → euler 2.
Body makeCone(float radius, float height, uint32_t segments)
{
    const uint32_t n = std::max(segments, 3u);
    const double h = height * 0.5;
    const double twoPi = 6.283185307179586476925286766559;

    std::vector<Vec3> pts(static_cast<size_t>(n) + 1u);
    for (uint32_t i = 0; i < n; ++i) {
        const double ang = twoPi * static_cast<double>(i) / static_cast<double>(n);
        pts[i] = {std::cos(ang) * radius, std::sin(ang) * radius, -h};
    }
    const uint32_t apex = n;
    pts[apex] = {0., 0., h};

    std::vector<Body::FaceDef> defs;
    defs.reserve(static_cast<size_t>(n) + 1u);

    // Bottom cap (descending → outward −Z), opposite winding to the side tris.
    Body::FaceDef bottom;
    bottom.loop.reserve(n);
    bottom.loop.push_back(0u);
    for (uint32_t i = n; i-- > 1;) bottom.loop.push_back(i);
    bottom.surface.kind = SurfaceKind::Plane;
    bottom.surface.origin = {0., 0., -h};
    bottom.surface.normal = {0., 0., -1.};
    bottom.surface.uAxis = {1., 0., 0.};
    defs.push_back(std::move(bottom));

    // Side triangles: {bottom[i], bottom[i+1], apex} — traverses bottom[i]→
    // bottom[i+1] (opposite the cap) and shares apex spokes with neighbours.
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = (i + 1) % n;
        Body::FaceDef side;
        side.loop = {i, j, apex};
        // A real conical surface. This used to be tagged Cylinder "as an approximation",
        // which was not an approximation but a false statement: the tagged cylinder of
        // radius `radius` does not contain the apex at all — measured, 16 of a cone's 48
        // lateral vertices sat a full unit off their own face's surface, and every
        // surface-based query on a cone was reading from that.
        // apex is at +h, base at -h, so the axis runs apex -> base along -Z and the slope
        // (base radius over height) is radius/height.
        side.surface.kind = SurfaceKind::Cone;
        side.surface.origin = {0., 0., h};       // apex
        side.surface.normal = {0., 0., -1.};    // axis, apex -> base
        side.surface.uAxis = {1., 0., 0.};
        side.surface.radius = (height > 0.) ? (radius / height) : 0.;  // slope
        defs.push_back(std::move(side));
    }

    auto b = Body::fromFaces(pts, defs);
    return b.has_value() ? std::move(*b) : Body{};
}

// A UV sphere: south + north pole vertices, (lat-1) latitude rings of `lon`
// verts, a triangle fan at each pole and quad bands between rings. All verts on
// the radius. V = 2 + (lat-1)*lon, F = lat*lon, E = lon*(2*lat-1) → euler 2.
Body makeSphere(float radius, uint32_t latSegments, uint32_t lonSegments)
{
    const uint32_t lat = std::max(latSegments, 2u);
    const uint32_t lon = std::max(lonSegments, 3u);
    const double pi = 3.141592653589793238462643383279;
    const double twoPi = 6.283185307179586476925286766559;

    // Vertex of latitude ring L (1..lat-1) at longitude j (wrapped).
    auto vid = [lon](uint32_t L, uint32_t j) -> uint32_t {
        return 1u + (L - 1u) * lon + (j % lon);
    };
    const uint32_t southPole = 0u;
    const uint32_t northPole = 1u + (lat - 1u) * lon;

    std::vector<Vec3> pts(2u + static_cast<size_t>(lat - 1u) * lon);
    pts[southPole] = {0., 0., -radius};
    pts[northPole] = {0., 0., radius};
    for (uint32_t L = 1u; L < lat; ++L) {
        const double v = -0.5 * pi + pi * static_cast<double>(L) / static_cast<double>(lat);
        const double cv = std::cos(v), sv = std::sin(v);
        for (uint32_t j = 0u; j < lon; ++j) {
            const double u = twoPi * static_cast<double>(j) / static_cast<double>(lon);
            pts[vid(L, j)] = {radius * cv * std::cos(u), radius * cv * std::sin(u), radius * sv};
        }
    }

    auto sphereFace = [radius](std::vector<uint32_t> loop) {
        Body::FaceDef fd;
        fd.loop = std::move(loop);
        fd.surface.kind = SurfaceKind::Sphere;
        fd.surface.origin = {0., 0., 0.};
        fd.surface.normal = {0., 0., 1.};
        fd.surface.uAxis = {1., 0., 0.};
        fd.surface.radius = radius;
        return fd;
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(static_cast<size_t>(lat) * lon);

    // South pole fan — wound opposite to band 1 along the ring-1 edges.
    for (uint32_t j = 0u; j < lon; ++j)
        defs.push_back(sphereFace({southPole, vid(1u, j + 1u), vid(1u, j)}));

    // Latitude bands between ring L and L+1.
    for (uint32_t L = 1u; L + 1u < lat; ++L)
        for (uint32_t j = 0u; j < lon; ++j)
            defs.push_back(sphereFace({vid(L, j), vid(L, j + 1u), vid(L + 1u, j + 1u), vid(L + 1u, j)}));

    // North pole fan — wound opposite to the last band along the top-ring edges.
    const uint32_t topRing = lat - 1u;
    for (uint32_t j = 0u; j < lon; ++j)
        defs.push_back(sphereFace({northPole, vid(topRing, j), vid(topRing, j + 1u)}));

    auto b = Body::fromFaces(pts, defs);
    if (!b.has_value()) return Body{};
    // Every sphere edge is a circle arc: latitude rings are small circles about
    // the axis at their height; meridians (band verticals + pole spokes) are
    // great circles through the sphere centre. Upgrading them makes toMesh
    // tessellate exactly on the sphere.
    for (uint32_t e = 0; e < static_cast<uint32_t>(b->edgeCount()); ++e) {
        const Vec3& p0 = b->vertex(b->edge(e).v0).point;
        const Vec3& p1 = b->vertex(b->edge(e).v1).point;
        const double r0 = std::sqrt(p0.x * p0.x + p0.y * p0.y);
        const double r1 = std::sqrt(p1.x * p1.x + p1.y * p1.y);
        if (std::abs(p0.z - p1.z) < 1e-5f && std::abs(r0 - r1) < 1e-5f) {
            b->setEdgeArc(e, {0., 0., p0.z}, {0., 0., 1.}, r0);  // latitude ring
        } else {
            const Vec3 ax = normalize(cross(p0, p1));                 // meridian great circle
            b->setEdgeArc(e, {0., 0., 0.}, ax, radius);
        }
    }
    return std::move(*b);
}

// ──────────── Creation ───────────────────────────────────────────────────────

Body extrudeProfile(const std::vector<Vec3>& profile, const Vec3& dir)
{
    const size_t n = profile.size();
    if (n < 3) return Body{};
    for (const Vec3& p : profile)
        if (!isFinite(p)) return Body{};
    if (!isFinite(dir)) return Body{};

    // Profile plane normal (Newell's method; length = 2·area) → consistent with a
    // CCW winding about +N.
    Vec3 N{0., 0., 0.};
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = profile[i];
        const Vec3& b = profile[(i + 1) % n];
        N.x += (a.y - b.y) * (a.z + b.z);
        N.y += (a.z - b.z) * (a.x + b.x);
        N.z += (a.x - b.x) * (a.y + b.y);
    }
    const double area2 = length(N);
    if (area2 < 1e-10f) return Body{};  // degenerate / near-zero-area profile
    N = scale(N, 1. / area2);

    double h = dot(dir, N);
    if (h > -1e-9f && h < 1e-9f) return Body{};  // dir parallel to the plane

    // Orient so the profile is CCW about the extrusion direction (+N side gets the
    // top cap, giving a positive-volume outward solid regardless of dir's sign).
    std::vector<Vec3> poly = profile;
    if (h < 0.) {
        std::reverse(poly.begin(), poly.end());
        N = {-N.x, -N.y, -N.z};
    }

    // Points: [0,n) bottom = poly, [n,2n) top = poly + dir.
    std::vector<Vec3> pts;
    pts.reserve(2 * n);
    for (const Vec3& p : poly) pts.push_back(p);
    for (const Vec3& p : poly) pts.push_back(add(p, dir));

    const Vec3 uAxis = normalize(sub(poly[1], poly[0]));
    std::vector<Body::FaceDef> defs;
    defs.reserve(n + 2);

    // Bottom cap: poly reversed → outward −N.
    {
        Body::FaceDef fd;
        for (size_t k = 0; k < n; ++k) fd.loop.push_back(static_cast<uint32_t>(n - 1 - k));
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = poly[0];
        fd.surface.normal = {-N.x, -N.y, -N.z};
        fd.surface.uAxis = uAxis;
        defs.push_back(std::move(fd));
    }
    // Top cap: poly order (shifted) → outward +N.
    {
        Body::FaceDef fd;
        for (size_t k = 0; k < n; ++k) fd.loop.push_back(static_cast<uint32_t>(n + k));
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[n];
        fd.surface.normal = N;
        fd.surface.uAxis = uAxis;
        defs.push_back(std::move(fd));
    }
    // Side quads: [b_i, b_{i+1}, t_{i+1}, t_i] — CCW from outside.
    for (size_t i = 0; i < n; ++i) {
        const uint32_t bi = static_cast<uint32_t>(i);
        const uint32_t bj = static_cast<uint32_t>((i + 1) % n);
        const uint32_t tj = static_cast<uint32_t>(n + (i + 1) % n);
        const uint32_t ti = static_cast<uint32_t>(n + i);
        Body::FaceDef fd;
        fd.loop = {bi, bj, tj, ti};
        const Vec3 e1 = sub(pts[bj], pts[bi]);
        const Vec3 e2 = sub(pts[ti], pts[bi]);
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[bi];
        fd.surface.normal = normalize(cross(e1, e2));
        fd.surface.uAxis = normalize(e1);
        defs.push_back(std::move(fd));
    }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

Body loftProfiles(const std::vector<Vec3>& bottom, const std::vector<Vec3>& top)
{
    const size_t n = bottom.size();
    if (n < 3 || top.size() != n) return Body{};
    for (const Vec3& p : bottom)
        if (!isFinite(p)) return Body{};
    for (const Vec3& p : top)
        if (!isFinite(p)) return Body{};

    auto newell = [](const std::vector<Vec3>& poly) {
        Vec3 N{0., 0., 0.};
        const size_t c = poly.size();
        for (size_t i = 0; i < c; ++i) {
            const Vec3& a = poly[i];
            const Vec3& b = poly[(i + 1) % c];
            N.x += (a.y - b.y) * (a.z + b.z);
            N.y += (a.z - b.z) * (a.x + b.x);
            N.z += (a.x - b.x) * (a.y + b.y);
        }
        return N;
    };
    auto centroid = [](const std::vector<Vec3>& poly) {
        Vec3 c{0., 0., 0.};
        for (const Vec3& p : poly) c = add(c, p);
        return scale(c, 1. / static_cast<double>(poly.size()));
    };

    Vec3 N = newell(bottom);
    const double area2 = length(N);
    if (area2 < 1e-10f) return Body{};       // degenerate bottom
    if (length(newell(top)) < 1e-10f) return Body{};  // degenerate top
    N = scale(N, 1. / area2);               // bottom plane normal (CCW about +N)

    // Orient so +N points from the bottom toward the top (positive-volume solid).
    const double h = dot(sub(centroid(top), centroid(bottom)), N);
    if (h > -1e-9f && h < 1e-9f) return Body{};  // top in the bottom's plane → undefined
    std::vector<Vec3> b = bottom, t = top;
    if (h < 0.) {
        std::reverse(b.begin(), b.end());
        std::reverse(t.begin(), t.end());  // reverse BOTH → the i↔i pairing is preserved
        N = {-N.x, -N.y, -N.z};
    }

    // Points: [0,n) bottom, [n,2n) top.
    std::vector<Vec3> pts;
    pts.reserve(2 * n);
    for (const Vec3& p : b) pts.push_back(p);
    for (const Vec3& p : t) pts.push_back(p);

    const Vec3 uAxis = normalize(sub(b[1], b[0]));
    std::vector<Body::FaceDef> defs;
    defs.reserve(n + 2);

    // Bottom cap: reversed order → outward −N.
    {
        Body::FaceDef fd;
        for (size_t k = 0; k < n; ++k) fd.loop.push_back(static_cast<uint32_t>(n - 1 - k));
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = b[0];
        fd.surface.normal = {-N.x, -N.y, -N.z};
        fd.surface.uAxis = uAxis;
        defs.push_back(std::move(fd));
    }
    // Top cap: forward (shifted) order → outward +Nt (from the top's own winding).
    {
        Body::FaceDef fd;
        for (size_t k = 0; k < n; ++k) fd.loop.push_back(static_cast<uint32_t>(n + k));
        Vec3 Nt = newell(t);
        Nt = normalize(Nt);
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[n];
        fd.surface.normal = Nt;
        fd.surface.uAxis = normalize(sub(t[1], t[0]));
        defs.push_back(std::move(fd));
    }
    // Side quads: [b_i, b_{i+1}, t_{i+1}, t_i] — CCW from outside (ruled trapezoid;
    // planar for similar scaled profiles, faceted otherwise).
    for (size_t i = 0; i < n; ++i) {
        const uint32_t bi = static_cast<uint32_t>(i);
        const uint32_t bj = static_cast<uint32_t>((i + 1) % n);
        const uint32_t tj = static_cast<uint32_t>(n + (i + 1) % n);
        const uint32_t ti = static_cast<uint32_t>(n + i);
        Body::FaceDef fd;
        fd.loop = {bi, bj, tj, ti};
        const Vec3 e1 = sub(pts[bj], pts[bi]);
        const Vec3 e2 = sub(pts[ti], pts[bi]);
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[bi];
        fd.surface.normal = normalize(cross(e1, e2));
        fd.surface.uAxis = normalize(e1);
        defs.push_back(std::move(fd));
    }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

Body twistExtrude(const std::vector<Vec3>& profile, const Vec3& dir, float twistRadians,
                  uint32_t layers)
{
    const size_t n = profile.size();
    if (n < 3 || layers < 1u) return Body{};
    if (!isFinite(twistRadians)) return Body{};
    for (const Vec3& p : profile)
        if (!isFinite(p)) return Body{};
    if (!isFinite(dir)) return Body{};

    // Profile plane normal (Newell) + centroid.
    Vec3 N{0., 0., 0.};
    Vec3 C{0., 0., 0.};
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = profile[i];
        const Vec3& b = profile[(i + 1) % n];
        N.x += (a.y - b.y) * (a.z + b.z);
        N.y += (a.z - b.z) * (a.x + b.x);
        N.z += (a.x - b.x) * (a.y + b.y);
        C = add(C, a);
    }
    if (length(N) < 1e-10f) return Body{};  // degenerate / near-zero-area profile
    N = normalize(N);
    C = scale(C, 1. / static_cast<double>(n));
    if (length(dir) < 1e-9f) return Body{};
    const double h = dot(dir, N);
    if (h > -1e-9f && h < 1e-9f) return Body{};  // dir parallel to the plane

    // Orient so the profile is CCW about the extrusion direction (positive-volume
    // outward solid regardless of dir's sign).
    std::vector<Vec3> poly = profile;
    if (h < 0.) {
        std::reverse(poly.begin(), poly.end());
        N = {-N.x, -N.y, -N.z};
    }
    const Vec3 axis = normalize(dir);  // twist axis through the centroid

    auto rot = [&](const Vec3& p, double ang) {
        const Vec3 v = sub(p, C);
        const double c = std::cos(ang), s = std::sin(ang);
        const Vec3 r = add(add(scale(v, c), scale(cross(axis, v), s)),
                           scale(axis, dot(axis, v) * (1. - c)));
        return add(C, r);
    };

    // (layers+1) rings: ring j rotated by twist·j/L and translated by dir·j/L.
    std::vector<Vec3> pts;
    pts.reserve((layers + 1u) * n);
    for (uint32_t j = 0; j <= layers; ++j) {
        const double f = static_cast<double>(j) / static_cast<double>(layers);
        for (size_t i = 0; i < n; ++i)
            pts.push_back(add(rot(poly[i], twistRadians * f), scale(dir, f)));
    }
    auto V = [&](uint32_t j, size_t i) { return static_cast<uint32_t>(j * n + i); };

    auto planeDef = [&](std::vector<uint32_t> loop) {
        Body::FaceDef fd;
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[loop[0]];
        fd.surface.normal = normalize(
            cross(sub(pts[loop[1]], pts[loop[0]]), sub(pts[loop[2]], pts[loop[0]])));
        fd.surface.uAxis = normalize(sub(pts[loop[1]], pts[loop[0]]));
        fd.loop = std::move(loop);
        return fd;
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(layers * n + 2u);
    // Bottom cap (ring 0, reversed → outward −N) and top cap (ring L, forward).
    {
        std::vector<uint32_t> lo, hi;
        for (size_t i = 0; i < n; ++i) lo.push_back(V(0, n - 1 - i));
        for (size_t i = 0; i < n; ++i) hi.push_back(V(layers, i));
        defs.push_back(planeDef(std::move(lo)));
        defs.push_back(planeDef(std::move(hi)));
    }
    // Side walls between consecutive rings, as TRIANGLES.
    //
    // A twisted quad is warped — its four corners are not coplanar — so describing it with
    // a Plane surface states something false about it. Measured on a twisted box, every
    // side quad deviated from its own tagged plane, by as much as 0.24 units at four
    // layers, and the deviation only shrinks with finer layering rather than vanishing. A
    // triangle is planar by construction, so splitting each wall makes the surface an
    // exact description instead of an approximate one, and removes the ambiguity of what a
    // warped quad even bounds.
    for (uint32_t j = 0; j < layers; ++j)
        for (size_t i = 0; i < n; ++i) {
            const size_t i1 = (i + 1) % n;
            defs.push_back(planeDef({V(j, i), V(j, i1), V(j + 1, i1)}));
            defs.push_back(planeDef({V(j, i), V(j + 1, i1), V(j + 1, i)}));
        }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

Body makePyramid(const std::vector<Vec3>& base, const Vec3& apex)
{
    const size_t n = base.size();
    if (n < 3) return Body{};
    for (const Vec3& p : base)
        if (!isFinite(p)) return Body{};
    if (!isFinite(apex)) return Body{};

    // Base plane normal (Newell; length = 2·area) → consistent with a CCW winding.
    Vec3 N{0., 0., 0.};
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = base[i];
        const Vec3& b = base[(i + 1) % n];
        N.x += (a.y - b.y) * (a.z + b.z);
        N.y += (a.z - b.z) * (a.x + b.x);
        N.z += (a.x - b.x) * (a.y + b.y);
    }
    if (length(N) < 1e-10f) return Body{};  // degenerate / near-zero-area base
    N = normalize(N);

    // Orient so the apex is on the +N side (positive-volume outward solid).
    const double hgt = dot(sub(apex, base[0]), N);
    if (hgt > -1e-9f && hgt < 1e-9f) return Body{};  // apex in the base plane
    std::vector<Vec3> poly = base;
    if (hgt < 0.) {
        std::reverse(poly.begin(), poly.end());
        N = {-N.x, -N.y, -N.z};
    }

    std::vector<Vec3> pts = poly;
    pts.push_back(apex);
    const uint32_t A = static_cast<uint32_t>(n);  // apex index

    auto planeDef = [&](std::vector<uint32_t> loop) {
        Body::FaceDef fd;
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[loop[0]];
        fd.surface.normal = normalize(
            cross(sub(pts[loop[1]], pts[loop[0]]), sub(pts[loop[2]], pts[loop[0]])));
        fd.surface.uAxis = normalize(sub(pts[loop[1]], pts[loop[0]]));
        fd.loop = std::move(loop);
        return fd;
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(n + 1);
    // Base cap: reversed order → outward −N (the apex is on +N).
    {
        std::vector<uint32_t> loop;
        for (size_t k = 0; k < n; ++k) loop.push_back(static_cast<uint32_t>(n - 1 - k));
        defs.push_back(planeDef(std::move(loop)));
    }
    // Side triangles: base[i] → base[i+1] → apex (outward-facing).
    for (size_t i = 0; i < n; ++i)
        defs.push_back(planeDef({static_cast<uint32_t>(i), static_cast<uint32_t>((i + 1) % n), A}));

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

Body makeFacetedCylinder(float radius, float height, uint32_t segments)
{
    const uint32_t n = std::max(segments, 3u);
    const double h = height * 0.5;
    const double twoPi = 6.283185307179586476925286766559;
    std::vector<Vec3> ring;  // regular n-gon at z = -h, CCW about +Z
    ring.reserve(n);
    for (uint32_t k = 0; k < n; ++k) {
        const double a = twoPi * static_cast<double>(k) / static_cast<double>(n);
        ring.push_back({radius * std::cos(a), radius * std::sin(a), -h});
    }
    return extrudeProfile(ring, {0., 0., height});  // all-planar prism
}

Body makeTube(float outerRadius, float innerRadius, float height, uint32_t segments)
{
    if (!isFinite(outerRadius) || !isFinite(innerRadius) || !isFinite(height)) return Body{};
    if (outerRadius <= innerRadius || innerRadius <= 0. || height <= 0.) return Body{};
    const uint32_t n = std::max(segments, 3u);
    const double h = height * 0.5;
    const double twoPi = 6.283185307179586476925286766559;

    // Four rings of n vertices: outer-bottom, outer-top, inner-bottom, inner-top.
    std::vector<Vec3> pts;
    pts.reserve(4u * n);
    auto ring = [&](float radius, float z) {
        for (uint32_t k = 0; k < n; ++k) {
            const double a = twoPi * static_cast<double>(k) / static_cast<double>(n);
            pts.push_back({radius * std::cos(a), radius * std::sin(a), z});
        }
    };
    ring(outerRadius, -h);  // [0,n)  outer bottom
    ring(outerRadius, +h);  // [n,2n) outer top
    ring(innerRadius, -h);  // [2n,3n) inner bottom
    ring(innerRadius, +h);  // [3n,4n) inner top
    auto oB = [&](uint32_t k) { return k % n; };
    auto oT = [&](uint32_t k) { return n + k % n; };
    auto iB = [&](uint32_t k) { return 2u * n + k % n; };
    auto iT = [&](uint32_t k) { return 3u * n + k % n; };

    auto planeDef = [&](std::vector<uint32_t> loop) {
        Body::FaceDef fd;
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[loop[0]];
        fd.surface.normal = normalize(
            cross(sub(pts[loop[1]], pts[loop[0]]), sub(pts[loop[2]], pts[loop[0]])));
        fd.surface.uAxis = normalize(sub(pts[loop[1]], pts[loop[0]]));
        fd.loop = std::move(loop);
        return fd;
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(4u * n);
    for (uint32_t k = 0; k < n; ++k) {
        // Outer wall — outward (away from the axis).
        defs.push_back(planeDef({oB(k), oB(k + 1), oT(k + 1), oT(k)}));
        // Inner wall — inward (toward the axis: the reversed winding).
        defs.push_back(planeDef({iT(k), iT(k + 1), iB(k + 1), iB(k)}));
        // Top annular cap (+Z outward).
        defs.push_back(planeDef({oT(k), oT(k + 1), iT(k + 1), iT(k)}));
        // Bottom annular cap (−Z outward).
        defs.push_back(planeDef({iB(k), iB(k + 1), oB(k + 1), oB(k)}));
    }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

Body makeFacetedSphere(float radius, uint32_t latSegments, uint32_t lonSegments)
{
    const uint32_t lat = std::max(latSegments, 2u);
    const double pi = 3.141592653589793238462643383279;
    // Polygonal semicircle from the bottom pole up to the top pole, in the xz
    // half-plane x ≥ 0, revolved about the z-axis. Endpoints are on the axis.
    std::vector<Vec3> semi;
    semi.reserve(lat + 1);
    for (uint32_t k = 0; k <= lat; ++k) {
        const double a = -pi * 0.5 + pi * static_cast<double>(k) / static_cast<double>(lat);
        semi.push_back({radius * std::cos(a), 0., radius * std::sin(a)});
    }
    return revolveProfile(semi, {0., 0., 0.}, {0., 0., 1.}, std::max(lonSegments, 3u));
}

Body revolveProfile(const std::vector<Vec3>& profile, const Vec3& axisOrigin,
                    const Vec3& axisDir, uint32_t segments)
{
    const size_t m = profile.size();
    if (m < 3 || segments < 3) return Body{};
    for (const Vec3& p : profile)
        if (!isFinite(p)) return Body{};
    if (!isFinite(axisOrigin) || !isFinite(axisDir)) return Body{};
    const double axisLen = length(axisDir);
    if (axisLen < 1e-9f) return Body{};
    const Vec3 n = scale(axisDir, 1. / axisLen);

    // Non-degenerate profile area (Newell).
    Vec3 Nprof{0., 0., 0.};
    for (size_t i = 0; i < m; ++i) {
        const Vec3& a = profile[i];
        const Vec3& b = profile[(i + 1) % m];
        Nprof.x += (a.y - b.y) * (a.z + b.z);
        Nprof.y += (a.z - b.z) * (a.x + b.x);
        Nprof.z += (a.x - b.x) * (a.y + b.y);
    }
    if (length(Nprof) < 1e-10f) return Body{};

    // The profile must lie to ONE side of the axis: it may TOUCH the axis (a
    // vertex on it → a pole), but not cross to the other side. Off-axis vertices
    // must all share the same radial side.
    std::vector<bool> onAxis(m, false);
    Vec3 rHat{0., 0., 0.};
    bool haveRHat = false;
    for (size_t i = 0; i < m; ++i) {
        const Vec3 v = sub(profile[i], axisOrigin);
        const Vec3 rad = sub(v, scale(n, dot(v, n)));
        const double rl = length(rad);
        if (rl < 1e-6f) {
            onAxis[i] = true;  // pole vertex
            continue;
        }
        if (!haveRHat) { rHat = scale(rad, 1. / rl); haveRHat = true; }
        else if (dot(rad, rHat) <= 1e-6f) return Body{};  // crossed to the other side
    }
    if (!haveRHat) return Body{};  // entirely on the axis → degenerate

    // Rotate a point about the axis by `ang` (Rodrigues).
    auto rot = [&](const Vec3& p, double ang) {
        const Vec3 v = sub(p, axisOrigin);
        const double c = std::cos(ang), s = std::sin(ang);
        const Vec3 r = add(add(scale(v, c), scale(cross(n, v), s)),
                           scale(n, dot(n, v) * (1. - c)));
        return add(axisOrigin, r);
    };

    // Vertices: an on-axis profile vertex welds to ONE pole point; an off-axis one
    // fans into `segments` rotated copies.
    const double twoPi = 6.283185307179586476925286766559;
    std::vector<Vec3> pts;
    std::vector<uint32_t> ringStart(m);  // pole index, or start of the ring of copies
    for (size_t i = 0; i < m; ++i) {
        ringStart[i] = static_cast<uint32_t>(pts.size());
        if (onAxis[i]) {
            pts.push_back(profile[i]);  // single welded pole vertex
        } else {
            for (uint32_t j = 0; j < segments; ++j)
                pts.push_back(rot(profile[i], twoPi * static_cast<double>(j) /
                                                  static_cast<double>(segments)));
        }
    }
    auto vAt = [&](size_t i, uint32_t j) {
        return onAxis[i] ? ringStart[i] : ringStart[i] + (j % segments);
    };

    // One face per (profile edge, angular step): a quad, or — where an endpoint is
    // a pole — a collapsed triangle; a profile edge lying ON the axis contributes
    // nothing. A full revolution needs no end caps.
    std::vector<Body::FaceDef> defs;
    defs.reserve(m * segments);
    for (uint32_t j = 0; j < segments; ++j) {
        for (size_t i = 0; i < m; ++i) {
            const size_t i1 = (i + 1) % m;
            // Outward winding (away from the axis).
            const uint32_t L[4] = {vAt(i, j), vAt(i, j + 1), vAt(i1, j + 1), vAt(i1, j)};
            std::vector<uint32_t> loop;
            for (int k = 0; k < 4; ++k)
                if (loop.empty() || loop.back() != L[k]) loop.push_back(L[k]);
            if (loop.size() >= 2 && loop.front() == loop.back()) loop.pop_back();
            if (loop.size() < 3) continue;  // degenerate (edge on the axis)

            Body::FaceDef fd;
            fd.loop = loop;
            fd.surface.kind = SurfaceKind::Plane;
            fd.surface.origin = pts[loop[0]];
            fd.surface.normal = normalize(
                cross(sub(pts[loop[1]], pts[loop[0]]), sub(pts[loop[2]], pts[loop[0]])));
            fd.surface.uAxis = normalize(sub(pts[loop[1]], pts[loop[0]]));
            defs.push_back(std::move(fd));
        }
    }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

Body revolveProfilePartial(const std::vector<Vec3>& profile, const Vec3& axisOrigin,
                           const Vec3& axisDir, uint32_t segments, float sweepRadians)
{
    const double twoPi = 6.283185307179586476925286766559;
    if (!isFinite(sweepRadians) || sweepRadians <= 1e-6f) return Body{};
    if (sweepRadians >= twoPi - 1e-5f)  // a full turn → the genus-1 ring path
        return revolveProfile(profile, axisOrigin, axisDir, segments);

    const size_t m = profile.size();
    if (m < 3 || segments < 3) return Body{};
    for (const Vec3& p : profile)
        if (!isFinite(p)) return Body{};
    if (!isFinite(axisOrigin) || !isFinite(axisDir)) return Body{};
    const double axisLen = length(axisDir);
    if (axisLen < 1e-9f) return Body{};
    const Vec3 n = scale(axisDir, 1. / axisLen);

    // Non-degenerate profile area (Newell).
    Vec3 Nprof{0., 0., 0.};
    for (size_t i = 0; i < m; ++i) {
        const Vec3& a = profile[i];
        const Vec3& b = profile[(i + 1) % m];
        Nprof.x += (a.y - b.y) * (a.z + b.z);
        Nprof.y += (a.z - b.z) * (a.x + b.x);
        Nprof.z += (a.x - b.x) * (a.y + b.y);
    }
    if (length(Nprof) < 1e-10f) return Body{};

    // The profile must lie to ONE side of the axis: it may TOUCH the axis (a
    // vertex on it → a welded pole shared across all angles and both caps), but
    // not cross to the other side. Off-axis vertices share one radial side.
    std::vector<bool> onAxis(m, false);
    Vec3 rHat{0., 0., 0.};
    bool haveRHat = false;
    for (size_t i = 0; i < m; ++i) {
        const Vec3 v = sub(profile[i], axisOrigin);
        const Vec3 rad = sub(v, scale(n, dot(v, n)));
        const double rl = length(rad);
        if (rl < 1e-6f) { onAxis[i] = true; continue; }  // pole vertex
        if (!haveRHat) { rHat = scale(rad, 1. / rl); haveRHat = true; }
        else if (dot(rad, rHat) <= 1e-6f) return Body{};  // crossed to the other side
    }
    if (!haveRHat) return Body{};  // entirely on the axis → degenerate

    auto rot = [&](const Vec3& p, double ang) {
        const Vec3 v = sub(p, axisOrigin);
        const double c = std::cos(ang), s = std::sin(ang);
        const Vec3 r = add(add(scale(v, c), scale(cross(n, v), s)),
                           scale(n, dot(n, v) * (1. - c)));
        return add(axisOrigin, r);
    };

    // An on-axis profile vertex welds to ONE pole point (rotation leaves it
    // fixed); an off-axis one fans into (segments+1) rotated copies (an OPEN arc).
    std::vector<Vec3> pts;
    std::vector<uint32_t> ringStart(m);
    for (size_t i = 0; i < m; ++i) {
        ringStart[i] = static_cast<uint32_t>(pts.size());
        if (onAxis[i]) {
            pts.push_back(profile[i]);  // single welded pole
        } else {
            for (uint32_t k = 0; k <= segments; ++k)
                pts.push_back(rot(profile[i], sweepRadians * static_cast<float>(k) /
                                                  static_cast<float>(segments)));
        }
    }
    auto vAt = [&](size_t i, uint32_t k) {
        return onAxis[i] ? ringStart[i] : ringStart[i] + k;
    };

    auto planeDef = [&](std::vector<uint32_t> loop) {
        Body::FaceDef fd;
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[loop[0]];
        fd.surface.normal = normalize(
            cross(sub(pts[loop[1]], pts[loop[0]]), sub(pts[loop[2]], pts[loop[0]])));
        fd.surface.uAxis = normalize(sub(pts[loop[1]], pts[loop[0]]));
        fd.loop = std::move(loop);
        return fd;
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(m * segments + 2u);
    // Swept side bands (outward winding, same convention as revolveProfile; k not
    // wrapped — this is an open arc).
    for (uint32_t k = 0; k < segments; ++k) {
        for (size_t i = 0; i < m; ++i) {
            const size_t i1 = (i + 1) % m;
            const uint32_t L[4] = {vAt(i, k), vAt(i, k + 1), vAt(i1, k + 1), vAt(i1, k)};
            std::vector<uint32_t> loop;
            for (int q = 0; q < 4; ++q)
                if (loop.empty() || loop.back() != L[q]) loop.push_back(L[q]);
            if (loop.size() >= 2 && loop.front() == loop.back()) loop.pop_back();
            if (loop.size() < 3) continue;
            defs.push_back(planeDef(std::move(loop)));
        }
    }
    // Two end caps — the flat radial faces (the profile at angle 0 and at θ). The
    // start cap walks the profile FORWARD; the end cap REVERSED — so each cap's
    // coedges oppose the side band's on their shared boundary edges, giving a
    // consistent outward-wound shell. A cap loop may contain a pole vertex (when
    // the profile touches the axis); duplicates are deduped so it stays simple.
    auto addCap = [&](std::vector<uint32_t> loop) {
        std::vector<uint32_t> simple;
        for (uint32_t v : loop)
            if (simple.empty() || simple.back() != v) simple.push_back(v);
        if (simple.size() >= 2 && simple.front() == simple.back()) simple.pop_back();
        if (simple.size() >= 3) defs.push_back(planeDef(std::move(simple)));
    };
    {
        std::vector<uint32_t> start, end;
        start.reserve(m);
        end.reserve(m);
        for (size_t i = 0; i < m; ++i) start.push_back(vAt(i, 0));
        for (size_t i = 0; i < m; ++i) end.push_back(vAt(m - 1 - i, segments));
        addCap(std::move(start));
        addCap(std::move(end));
    }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

// ──────────── Boolean building blocks ────────────────────────────────────────

namespace {
// Imprint onto `target` every intersection Line where a planar face of `tool`
// transects one of target's faces, iterating to a fixpoint: each successful
// imprintCurve splits a straddling face, and the resulting sub-faces are
// re-scanned until no tool plane cuts any target face's interior. This leaves no
// target face straddling a tool face-plane.
// Axis-aligned bounding box of a face, padded by `pad`.
struct FaceBox {
    Vec3 lo{0, 0, 0}, hi{0, 0, 0};
    bool valid = false;
};
FaceBox faceBox(const Body& b, uint32_t f, float pad)
{
    FaceBox box;
    for (uint32_t v : b.faceVertices(f)) {
        const Vec3 p = b.vertex(v).point;
        if (!box.valid) { box.lo = box.hi = p; box.valid = true; }
        box.lo = {std::min(box.lo.x, p.x), std::min(box.lo.y, p.y), std::min(box.lo.z, p.z)};
        box.hi = {std::max(box.hi.x, p.x), std::max(box.hi.y, p.y), std::max(box.hi.z, p.z)};
    }
    box.lo = {box.lo.x - pad, box.lo.y - pad, box.lo.z - pad};
    box.hi = {box.hi.x + pad, box.hi.y + pad, box.hi.z + pad};
    return box;
}
bool boxesOverlap(const FaceBox& a, const FaceBox& b)
{
    return a.valid && b.valid && a.lo.x <= b.hi.x && b.lo.x <= a.hi.x && a.lo.y <= b.hi.y &&
           b.lo.y <= a.hi.y && a.lo.z <= b.hi.z && b.lo.z <= a.hi.z;
}

bool imprintOneWay(Body& target, const Body& tool, Tolerance tol)
{
    // Snapshot each tool face's surface + AABB once (tool is not modified here).
    // The AABB is the broad-phase key: a tool face can only cut a target face if
    // their boxes overlap, which prunes the spurious cuts a far tool plane's
    // infinite intersection line would otherwise imprint (the source of the
    // O(tool-faces²) face explosion).
    const double pad = tol.at(1.f) * 10.f;
    struct ToolFace { Surface surf; FaceBox box; };
    std::vector<ToolFace> toolFaces;
    toolFaces.reserve(tool.faceCount());
    for (uint32_t ft = 0; ft < tool.faceCount(); ++ft) {
        if (!tool.face(ft).alive) continue;
        const uint32_t sIdx = tool.face(ft).surface;
        if (sIdx < tool.surfaceCount()) toolFaces.push_back({tool.surface(sIdx), faceBox(tool, ft, pad)});
    }

    // Fixpoint by FULL passes: each pass sweeps every current face once, cutting
    // it by the first tool plane that applies (inner break), and continues to the
    // next face rather than restarting the scan after every cut. Sub-faces
    // appended mid-pass are handled on the following pass. Deterministic.
    // Bound the total work. A near-tangent faceted config — e.g. a faceted sphere
    // grazing a box face by ~1e-4 — makes the tool's facet planes imprint an O(n²)
    // line arrangement on the target face, one cut per pass, growing toward a
    // thousand-plus faces (found by the near-tangent torture: the Boolean effectively
    // hung). Cap the imprinted face count relative to the inputs; when it is exceeded
    // the imprint stops incomplete, and booleanToBody's watertight-or-empty invariant
    // then returns a clean empty Body rather than hanging on a degenerate tangency.
    const size_t faceBudget =
        128 + 4 * (static_cast<size_t>(target.faceCount()) + toolFaces.size());
    // A face count alone does not bound the work: an inner-loop (hole) imprint adds
    // vertices and edges while leaving the face count untouched, so a non-idempotent
    // one ran to the iteration cap below and reached 1.6 million vertices on a
    // six-face box. imprintCurve now refuses a hole it has already made, but the
    // guard is kept honest by bounding the entity count too — whatever else may fail
    // to terminate, it cannot do so unboundedly.
    const size_t vertexBudget =
        256 + 16 * (static_cast<size_t>(target.vertexCount()) + toolFaces.size());
    bool changed = true;
    size_t safety = 0;
    const size_t cap = 100000;
    while (changed && ++safety < cap) {
        if (target.faceCount() > faceBudget) return false;    // degenerate explosion → bail
        if (target.vertexCount() > vertexBudget) return false;  // non-face-growing runaway
        changed = false;
        const uint32_t fcount = static_cast<uint32_t>(target.faceCount());
        for (uint32_t f = 0; f < fcount; ++f) {
            if (!target.face(f).alive) continue;
            const uint32_t saIdx = target.face(f).surface;
            if (saIdx >= target.surfaceCount()) continue;
            const Surface sa = target.surface(saIdx);
            const FaceBox tbox = faceBox(target, f, pad);
            for (const ToolFace& tf : toolFaces) {
                if (!boxesOverlap(tbox, tf.box)) continue;  // broad-phase prune
                const SurfaceIntersection si = intersectSurfaces(sa, tf.surf, tol);
                // Every seam branch intersectSurfaces can express analytically is
                // offered to the imprint, not just the straight one. A Circle is the
                // seam a plane cuts on a sphere or square across a cylinder, and
                // dropping it was what left curved operands straddling; TwoLines
                // simply has a second Line branch, and imprinting only the first
                // would leave the face straddling the other. imprintCurve is the
                // authority on whether a given curve actually lies on this face and
                // crosses its boundary cleanly, so an inapplicable branch costs one
                // refused call rather than needing to be pre-filtered here.
                // Collect the TOOL's vertices lying on a seam circle and pass them along.
                // A hole ring must be discretized the same way the other operand
                // discretized the same circle, or the two rings cannot partner
                // edge-for-edge and the sew cannot close.
                auto cutAlongCircle = [&](const Curve& seam) {
                    const Vec3 cax = normalize(seam.dir);
                    std::vector<Vec3> onCircle;
                    for (uint32_t tv = 0; tv < static_cast<uint32_t>(tool.vertexCount()); ++tv) {
                        if (!tool.vertex(tv).alive) continue;
                        const Vec3 d = sub(tool.vertex(tv).point, seam.origin);
                        const double axial = dot(d, cax);
                        const double radial = length(sub(d, scale(cax, axial)));
                        if (std::abs(axial) <= pad && std::abs(radial - seam.radius) <= pad)
                            onCircle.push_back(tool.vertex(tv).point);
                    }
                    return target.imprintCurve(f, seam, tol, &onCircle) != kInvalid;
                };

                bool cut = false;
                switch (si.kind) {
                    case SurfaceIntersectionKind::Line:
                        cut = target.imprintCurve(f, si.curve, tol) != kInvalid;
                        break;
                    case SurfaceIntersectionKind::Circle:
                        cut = cutAlongCircle(si.curve);
                        break;
                    case SurfaceIntersectionKind::TwoLines:
                        cut = target.imprintCurve(f, si.curve, tol) != kInvalid ||
                              target.imprintCurve(f, si.curve2, tol) != kInvalid;
                        break;
                    case SurfaceIntersectionKind::TwoCircles:
                        // Both rings are offered, and BOTH are attempted rather than
                        // short-circuiting on the first: one face can be crossed by both
                        // rings of a sphere spanning the bore, and imprinting only one
                        // would leave it straddling the other.
                        cut = cutAlongCircle(si.curve);
                        cut = cutAlongCircle(si.curve2) || cut;
                        break;
                    case SurfaceIntersectionKind::None:
                    case SurfaceIntersectionKind::Point:
                    case SurfaceIntersectionKind::Unsupported:
                        break;  // nothing to imprint (tangency is a measure-zero touch)
                }
                if (cut) {
                    changed = true;  // f was split; move on to the next face
                    break;
                }
            }
        }
    }
    return safety < cap;  // false ⇒ hit the safety cap (degenerate)
}
}  // namespace

bool imprintMutually(Body& a, Body& b, Tolerance tol)
{
    // Two rounds, not one. A hole-ring seam must be discretized to match the OTHER
    // operand's vertices on the same circle, which on the first pass may not exist yet:
    // a cylinder's latitude ring is created by imprinting the box onto it, so a box
    // imprinted first has no partner ring to match and defers. The second round makes
    // those deferred cuts. Everything already imprinted is idempotent and refuses, so
    // the extra round converges rather than accumulating.
    bool ok = true;
    for (int round = 0; round < 2; ++round) {
        ok = imprintOneWay(a, b, tol) && ok;
        ok = imprintOneWay(b, a, tol) && ok;
    }
    return ok;  // false ⇒ a degenerate/near-tangent config blew the imprint budget
}

bool segmentCrossesTriangleExact(const Vec3& A, const Vec3& B, const Vec3& v0, const Vec3& v1,
                                 const Vec3& v2, bool& degenerate)
{
    // A and B must lie strictly on opposite sides of the triangle's plane.
    const double oa = RobustPredicates::orient3D(v0, v1, v2, A);
    const double ob = RobustPredicates::orient3D(v0, v1, v2, B);
    // Both zero ⇒ the segment is coplanar with the triangle, OR the triangle is
    // degenerate (zero-area: orient3D is 0 for every 4th point). Neither is a
    // transversal crossing, so contribute nothing WITHOUT flagging a retry.
    if (oa == 0.0 && ob == 0.0) return false;
    if (oa == 0.0 || ob == 0.0) {
        degenerate = true;  // exactly one endpoint on the plane ⇒ genuine grazing
        return false;
    }
    if ((oa > 0.0) == (ob > 0.0)) return false;  // same side ⇒ no crossing

    // The line A→B must pass on the SAME rotational side of all three edges to
    // pierce the interior. Each orient3D is exactly signed.
    const double e0 = RobustPredicates::orient3D(A, B, v0, v1);
    const double e1 = RobustPredicates::orient3D(A, B, v1, v2);
    const double e2 = RobustPredicates::orient3D(A, B, v2, v0);
    if (e0 == 0.0 || e1 == 0.0 || e2 == 0.0) {
        degenerate = true;  // the line grazes an edge/vertex exactly
        return false;
    }
    const bool p0 = e0 > 0.0, p1 = e1 > 0.0, p2 = e2 > 0.0;
    return p0 == p1 && p1 == p2;
}

int pointPlaneSideSoS(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& p)
{
    const double o = RobustPredicates::orient3D(v0, v1, v2, p);
    if (o > 0.0) return 1;
    if (o < 0.0) return -1;

    // p lies exactly on the plane (or the triangle is degenerate). Resolve the
    // tie by the consistent symbolic perturbation p → p+(ε,ε²,ε³): since
    // orient3D is NEGATIVE on the +g side (g=(v1−v0)×(v2−v0)), the perturbed sign
    // is −sign(ε·gx + ε²·gy + ε³·gz), i.e. −sign of the FIRST non-zero component
    // of g in x,y,z order. Each component is an EXACT orient2D minor over the
    // triangle's projection onto a coordinate plane.
    using nexus::geometry::Vec2;
    const double gx = RobustPredicates::orient2D({v0.y, v0.z}, {v1.y, v1.z}, {v2.y, v2.z});
    if (gx != 0.0) return gx > 0.0 ? -1 : 1;
    const double gy = RobustPredicates::orient2D({v0.z, v0.x}, {v1.z, v1.x}, {v2.z, v2.x});
    if (gy != 0.0) return gy > 0.0 ? -1 : 1;
    const double gz = RobustPredicates::orient2D({v0.x, v0.y}, {v1.x, v1.y}, {v2.x, v2.y});
    if (gz != 0.0) return gz > 0.0 ? -1 : 1;
    return 0;  // g == 0 ⇒ a genuinely degenerate (zero-area / collinear) triangle
}

bool segmentCrossesTriangleSoS(const Vec3& p, const Vec3& B, const Vec3& v0, const Vec3& v1,
                               const Vec3& v2)
{
    // Plane-side of the two ray endpoints, exact even when p (or B) is coplanar.
    const int sp = pointPlaneSideSoS(v0, v1, v2, p);
    const int sb = pointPlaneSideSoS(v0, v1, v2, B);
    if (sp == 0 || sb == 0) return false;  // degenerate (zero-area) triangle
    if (sp == sb) return false;            // both endpoints on the same side → no crossing

    // The perturbed ray-line p→B must pass on the SAME rotational side of all three
    // edges. Each edge test is orient3D(p,B,vi,vj); an exact zero (the ray-line
    // through the edge/vertex) is resolved by the p→p+(ε,ε²,ε³) perturbation, whose
    // sign is −sign(first non-zero of (B−p)×(vj−vi)) (κ=−1, calibrated).
    const double dx = static_cast<double>(B.x) - p.x;
    const double dy = static_cast<double>(B.y) - p.y;
    const double dz = static_cast<double>(B.z) - p.z;
    auto edgeSide = [&](const Vec3& vi, const Vec3& vj) -> int {
        const double e = RobustPredicates::orient3D(p, B, vi, vj);
        if (e > 0.0) return 1;
        if (e < 0.0) return -1;
        const double ex = static_cast<double>(vj.x) - vi.x;
        const double ey = static_cast<double>(vj.y) - vi.y;
        const double ez = static_cast<double>(vj.z) - vi.z;
        const double wx = dy * ez - dz * ey;
        if (wx != 0.0) return wx > 0.0 ? -1 : 1;
        const double wy = dz * ex - dx * ez;
        if (wy != 0.0) return wy > 0.0 ? -1 : 1;
        const double wz = dx * ey - dy * ex;
        if (wz != 0.0) return wz > 0.0 ? -1 : 1;
        return 0;  // ray parallel to a degenerate/zero-length edge
    };
    const int s0 = edgeSide(v0, v1), s1 = edgeSide(v1, v2), s2 = edgeSide(v2, v0);
    if (s0 == 0 || s1 == 0 || s2 == 0) return false;  // degenerate edge → no contribution
    return s0 == s1 && s1 == s2;
}

}  // namespace nexus::geometry::brep
