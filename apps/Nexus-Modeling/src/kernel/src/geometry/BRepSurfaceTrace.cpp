// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — numerical surface/surface intersection (the quartic cases)
//
//  intersectSurfaces() answers only where a section is exactly a Line or a Circle,
//  which is where the configuration is axially symmetric. Everything else — a
//  sphere met by an off-axis cylinder, two cylinders with crossing axes, a cone
//  against anything curved and off its axis — is a quartic space curve, and was
//  declined. Measured across 3592 chained boolean steps those declines were 35.2%
//  of all outcomes and about five times the next-largest gap.
//
//  This file traces such a curve as a polyline. The method is the standard one:
//  seed by Newton-projecting lattice points onto BOTH surfaces, then march along
//  the tangent gradA × gradB, re-projecting after every step and halving the step
//  wherever the chord departs too far from the curve.
//
//  Two properties are load-bearing and are what the tests check:
//
//  1. WHERE A CLOSED FORM EXISTS, THE TRACE MUST REPRODUCE IT. A plane cutting a
//     sphere is a circle; two equal-radius cylinders crossing at a right angle meet
//     in two plane ellipses. Those are oracles that share no code with this file,
//     and they are the only reason to believe the genuinely new cases.
//  2. DETERMINISM. Lattice order is fixed, there is no randomness and no hash
//     iteration, so the same inputs give bitwise identical output.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <nexus/geometry/Tolerance.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus::geometry::brep {

namespace {

Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scale(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double length(const Vec3& a) { return std::sqrt(dot(a, a)); }

Vec3 unitOr(const Vec3& v, const Vec3& fallback)
{
    const double L = length(v);
    return L > 1e-300 ? Vec3{v.x / L, v.y / L, v.z / L} : fallback;
}

// Signed PERPENDICULAR distance, i.e. an implicit whose gradient is the unit normal.
// This differs from the public surfaceDistance for a CONE, which returns the raw implicit
// (radial − slope·axial): that is zero on the same set but its gradient has magnitude
// √(1+slope²), and feeding it to Newton would scale every cone correction by that factor.
// Newton converges either way; it converges in fewer, better-behaved steps this way, and
// the residual then means the same thing on all four surface kinds.
double perpDistance(const Surface& s, const Vec3& p)
{
    switch (s.kind) {
        case SurfaceKind::Plane:
            return dot(sub(p, s.origin), unitOr(s.normal, {0., 0., 1.}));
        case SurfaceKind::Sphere:
            return length(sub(p, s.origin)) - s.radius;
        case SurfaceKind::Cylinder: {
            const Vec3 ax = unitOr(s.normal, {0., 0., 1.});
            const Vec3 w = sub(p, s.origin);
            return length(sub(w, scale(ax, dot(w, ax)))) - s.radius;
        }
        case SurfaceKind::Cone: {
            const Vec3 ax = unitOr(s.normal, {0., 0., 1.});
            const Vec3 w = sub(p, s.origin);
            const double axial = dot(w, ax);
            const double radial = length(sub(w, scale(ax, axial)));
            const double sl = s.radius;
            // Behind the apex the nearest point IS the apex, and the cone's implicit stops
            // describing the geometry; report the true distance so a seed that wanders
            // behind the tip is pushed back rather than pulled through it.
            if (axial <= 0.0 && radial <= -axial * sl) return length(w);
            return (radial - sl * axial) / std::sqrt(1.0 + sl * sl);
        }
        default:
            return 1e30;  // NURBS: not traced here
    }
}

// ∇(perpDistance) — the unit outward normal at p, for every kind above.
Vec3 gradient(const Surface& s, const Vec3& p)
{
    switch (s.kind) {
        case SurfaceKind::Plane:
            return unitOr(s.normal, {0., 0., 1.});
        case SurfaceKind::Sphere:
            return unitOr(sub(p, s.origin), {0., 0., 1.});
        case SurfaceKind::Cylinder: {
            const Vec3 ax = unitOr(s.normal, {0., 0., 1.});
            const Vec3 w = sub(p, s.origin);
            return unitOr(sub(w, scale(ax, dot(w, ax))), ax);
        }
        case SurfaceKind::Cone: {
            const Vec3 ax = unitOr(s.normal, {0., 0., 1.});
            const Vec3 w = sub(p, s.origin);
            const Vec3 rhat = unitOr(sub(w, scale(ax, dot(w, ax))), ax);
            const double sl = s.radius, k = 1.0 / std::sqrt(1.0 + sl * sl);
            return scale(sub(rhat, scale(ax, sl)), k);
        }
        default:
            return unitOr(s.normal, {0., 0., 1.});
    }
}

// Diagonal of a polyline's bounding box — its size as a geometric object, independent of
// how many samples happen to describe it.
double branchExtent(const std::vector<Vec3>& pts)
{
    if (pts.empty()) return 0.0;
    Vec3 lo = pts.front(), hi = pts.front();
    for (const Vec3& p : pts) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    return length(sub(hi, lo));
}

bool traceable(const Surface& s)
{
    return s.kind == SurfaceKind::Plane || s.kind == SurfaceKind::Sphere ||
           s.kind == SurfaceKind::Cylinder || s.kind == SurfaceKind::Cone;
}

// How a branch stopped. "Exhausted" is a failure and has to be reported as one: a trace
// that ran out of budget has produced a polyline that is not the curve.
enum class Stop { Closed, LeftBox, Tangency, Exhausted };

struct Tracer {
    const Surface& a;
    const Surface& b;
    double eps;         // on-surface residual we accept
    double minDet;      // Newton: below this the 2x2 system is too ill-conditioned to solve
    double sinTangent;  // marching: below this |grad x grad| the DIRECTION is not decidable

    // Newton onto BOTH surfaces at once. The system is two equations in three unknowns —
    // the undetermined direction is the curve's own tangent — so the step is taken in the
    // plane the two gradients span, which is the minimum-norm solution.
    bool project(Vec3& p) const
    {
        for (int iter = 0; iter < 24; ++iter) {
            const double da = perpDistance(a, p), db = perpDistance(b, p);
            if (!(std::abs(da) < 1e30) || !(std::abs(db) < 1e30)) return false;
            if (std::abs(da) <= eps && std::abs(db) <= eps) return true;
            const Vec3 ga = gradient(a, p), gb = gradient(b, p);
            const double ab = dot(ga, gb);
            const double det = 1.0 - ab * ab;  // both gradients are unit
            if (std::abs(det) < minDet) return false;
            const double alpha = (-da + db * ab) / det;
            const double beta = (-db + da * ab) / det;
            const Vec3 step = add(scale(ga, alpha), scale(gb, beta));
            const double sl = length(step);
            if (!(sl < 1e30)) return false;
            p = add(p, step);
        }
        // One last look: 24 iterations is generous, and a point that is on both surfaces
        // is on both surfaces however long it took to say so.
        return std::abs(perpDistance(a, p)) <= eps && std::abs(perpDistance(b, p)) <= eps;
    }

    // Unit tangent, or false where the two gradients are too nearly parallel to decide a
    // direction. Both gradients are unit, so |grad x grad| IS the sine of the angle between
    // the surfaces — a scale-free measure of how transversally they meet.
    //
    // This threshold is why the tracer stops at a SINGULAR POINT instead of guessing
    // through it. Two equal-radius cylinders crossing at a right angle meet in two ellipses
    // that intersect each other at (0, ±1, 0), and there the intersection curve genuinely
    // has no tangent: two branches pass through one point. Marched naively the trace
    // wandered from one ellipse onto the other and back, never closing — measured, it ran
    // to 20000 points per branch and reported success. The curve there is not one loop, and
    // no step size makes it one; the honest output is four arcs meeting at two singular
    // points, which is also exactly what an imprint would need (arcs are edges, singular
    // points are vertices).
    bool tangent(const Vec3& p, Vec3& t) const
    {
        const Vec3 c = cross(gradient(a, p), gradient(b, p));
        const double L = length(c);
        if (L < sinTangent) return false;
        t = scale(c, 1.0 / L);
        return true;
    }
};

bool inBox(const Vec3& p, const Vec3& lo, const Vec3& hi, double pad)
{
    return p.x >= lo.x - pad && p.x <= hi.x + pad && p.y >= lo.y - pad && p.y <= hi.y + pad &&
           p.z >= lo.z - pad && p.z <= hi.z + pad;
}

}  // namespace

SurfaceTrace traceSurfaceIntersection(const Surface& a, const Surface& b, const Vec3& lo,
                                      const Vec3& hi, Tolerance tol)
{
    SurfaceTrace out;
    if (!traceable(a) || !traceable(b)) return out;

    const Vec3 span = sub(hi, lo);
    const double diag = length(span);
    if (!(diag > 0.0) || !(diag < 1e30)) return out;

    const double eps = std::max(static_cast<double>(tol.at(static_cast<float>(diag))), 1e-12);
    // Chord sag budget: how far the polyline may bow away from the true curve. Tied to the
    // box, not absolute, so the same shape traced at 1mm and at 1km gets the same fidelity.
    const double sagMax = std::max(diag * 1e-4, eps);
    // 1e-3 in the sine is an angle of ~0.06 degrees. Tight enough that a branch is
    // traced almost all the way into a singular point, loose enough that it stops
    // BEFORE the direction becomes noise.
    const Tracer tr{a, b, eps, 1e-7, 1e-3};

    const double hMax = diag * 0.05;
    const double hMin = diag * 1e-6;
    // A well-behaved branch is 60-200 points at this sag budget. A cap of 20000 was
    // not a safety net, it was a place for a confused trace to hide.
    constexpr size_t kMaxPointsPerBranch = 4000;
    constexpr size_t kMaxBranches = 16;

    // A point is "already traced" if it sits within this of a branch we have. It has to be
    // comfortably larger than the step, or a seed one step along an existing branch starts
    // the same curve over again.
    const double dedupe = std::max(hMax * 0.75, eps * 10.0);
    bool exhausted = false;
    // Every polyline this call produced, INCLUDING the degenerate ones that get rejected
    // below. Dedupe has to consult the rejects too: with only the published branches
    // considered, a tangency artifact is re-traced from every lattice seed near it, because
    // nothing records that the ground was already covered. Measured on a cylinder tangent
    // to a sphere, that cost 886 ms of re-deriving the same three points; remembering them
    // puts it back to 2 ms.
    std::vector<std::vector<Vec3>> traced;
    auto alreadyTraced = [&](const Vec3& p) {
        for (const std::vector<Vec3>& pts : traced) {
            for (const Vec3& q : pts) {
                if (length(sub(p, q)) < dedupe) return true;
            }
        }
        return false;
    };

    // March from `from` in one direction, appending as we go. Returns true if it closed
    // back onto the branch's first point.
    auto march = [&](std::vector<Vec3>& into, const Vec3& start, double dirSign,
                     bool allowClose) -> Stop {
        Vec3 p = start;
        double h = hMax;
        Vec3 prevT{0., 0., 0.};
        bool havePrevT = false;
        for (size_t n = 0; n < kMaxPointsPerBranch; ++n) {
            Vec3 t;
            if (!tr.tangent(p, t)) return Stop::Tangency;  // singular: stop rather than guess
            t = scale(t, dirSign);
            // TANGENT CONTINUITY — the guard that actually finds a singular point.
            //
            // A threshold on |grad x grad| alone does not: approaching where two branches
            // cross, Newton re-projects the chord's midpoint onto the OTHER branch, the
            // measured sag explodes, and the step halves over and over. Measured on two
            // equal perpendicular cylinders, whose intersection is two smooth ellipses with
            // no high curvature anywhere: 968 to 3676 points on arcs that need about fifty.
            // The trace was not resolving curvature, it was disintegrating.
            //
            // Sag control keeps the turn per step small, so consecutive tangents on one
            // branch are nearly parallel. A jump of more than 60 degrees is not a bend, it
            // is a different curve.
            if (havePrevT && dot(t, prevT) < 0.5) return Stop::Tangency;
            prevT = t;
            havePrevT = true;
            Vec3 next;
            bool stepped = false;
            for (int attempt = 0; attempt < 30; ++attempt) {
                next = add(p, scale(t, h));  // t already carries dirSign
                if (tr.project(next)) {
                    // Sag control: the true curve's midpoint, versus the chord's. This is
                    // what makes the polyline's fidelity a stated property rather than a
                    // consequence of whatever step happened to be in use.
                    const Vec3 chordMid = scale(add(p, next), 0.5);
                    Vec3 curveMid = chordMid;
                    if (tr.project(curveMid)) {
                        const double sag = length(sub(curveMid, chordMid));
                        if (sag <= sagMax) { stepped = true; break; }
                    } else {
                        stepped = true;  // cannot measure the sag; accept and let h shrink
                        break;
                    }
                }
                h *= 0.5;
                if (h < hMin) return Stop::Tangency;
            }
            if (!stepped) return Stop::Tangency;

            // Closure: back near where this branch began, having actually gone somewhere.
            if (allowClose && !into.empty() && n >= 3 &&
                length(sub(next, into.front())) <= h * 1.5) {
                return Stop::Closed;
            }
            if (!inBox(next, lo, hi, diag * 1e-3)) return Stop::LeftBox;
            into.push_back(next);
            p = next;
            // Creep back toward the coarse step so a locally tight bend does not force the
            // rest of the curve to be traced at its step size.
            h = std::min(hMax, h * 1.3);
        }
        return Stop::Exhausted;
    };

    // Deterministic lattice of seeds. Odd counts so the box centre is sampled, which is
    // where a centred configuration's curve actually is.
    constexpr int kGrid = 11;
    for (int ix = 0; ix < kGrid && out.branches.size() < kMaxBranches; ++ix) {
        for (int iy = 0; iy < kGrid && out.branches.size() < kMaxBranches; ++iy) {
            for (int iz = 0; iz < kGrid && out.branches.size() < kMaxBranches; ++iz) {
                const double fx = static_cast<double>(ix) / (kGrid - 1);
                const double fy = static_cast<double>(iy) / (kGrid - 1);
                const double fz = static_cast<double>(iz) / (kGrid - 1);
                Vec3 seed{lo.x + span.x * fx, lo.y + span.y * fy, lo.z + span.z * fz};
                if (!tr.project(seed)) continue;
                if (!inBox(seed, lo, hi, 0.0)) continue;
                if (alreadyTraced(seed)) continue;

                TracedBranch br;
                br.points.push_back(seed);
                const Stop fwd = march(br.points, seed, +1.0, true);
                if (fwd == Stop::Exhausted) {
                    exhausted = true;
                    traced.push_back(br.points);
                    continue;
                }
                br.closed = (fwd == Stop::Closed);
                if (!br.closed) {
                    // Open branch: it left the box (or hit a tangency) going forward, so
                    // walk the other way from the seed and prepend, or half the curve is
                    // silently missing.
                    std::vector<Vec3> back;
                    if (march(back, seed, -1.0, false) == Stop::Exhausted) {
                        exhausted = true;
                        traced.push_back(back);
                        traced.push_back(br.points);
                        continue;
                    }
                    std::reverse(back.begin(), back.end());
                    back.insert(back.end(), br.points.begin(), br.points.end());
                    br.points = std::move(back);
                }
                // A TANGENCY IS A POINT, NOT A CURVE. Where two surfaces touch instead of
                // crossing, Newton still finds points satisfying both equations and the
                // march still emits a few of them before the tangent gives out — two
                // tangent spheres produced a 3-point "branch" and the trace reported
                // success. That is the same shape of silent nonsense this whole arc exists
                // to remove, one layer down.
                //
                // Measured, the two populations are far apart: tangency artifacts span
                // 1.9e-04 and 7.2e-04 of the box diagonal, while the smallest LEGITIMATE
                // curve tried — the circle of two spheres overlapping by 0.01 — spans
                // 2.7e-02, a factor of 37. The cut is placed between them with room either
                // side, and on the extent rather than the point count, so it stays a
                // statement about geometry rather than about step control.
                const bool real = br.points.size() >= 2 && branchExtent(br.points) > diag * 5e-3;
                traced.push_back(br.points);
                if (real) out.branches.push_back(std::move(br));
            }
        }
    }

    // A trace that ran out of budget produced a polyline that is not the curve, and
    // saying otherwise is exactly the silent-success failure this whole arc is about.
    out.ok = !out.branches.empty() && !exhausted;
    return out;
}

}  // namespace nexus::geometry::brep
