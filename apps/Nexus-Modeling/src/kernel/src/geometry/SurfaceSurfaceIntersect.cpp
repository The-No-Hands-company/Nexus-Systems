#include <nexus/geometry/SurfaceSurfaceIntersect.h>
#include <nexus/geometry/NurbsSurfaceClosestPoint.h>
#include <nexus/geometry/Tolerance.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEps = 1e-10f;

// A length scale for the pair, sampled from both surfaces' domains. Used to proportion
// the marching step so one default behaves the same on a millimetre part and a kilometre
// one, instead of being right only near unit size.
float modelExtent(const NurbsSurface& a, const NurbsSurface& b) {
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    auto add = [&](const NurbsSurface& s) {
        const auto du = s.domainU();
        const auto dv = s.domainV();
        for (int i = 0; i <= 4; ++i) {
            for (int j = 0; j <= 4; ++j) {
                const float u = du.first + (du.second - du.first) * static_cast<float>(i) / 4.f;
                const float v = dv.first + (dv.second - dv.first) * static_cast<float>(j) / 4.f;
                const Vec3 p = s.evaluate(u, v);
                const float c[3] = {p.x, p.y, p.z};
                for (int k = 0; k < 3; ++k) {
                    lo[k] = std::min(lo[k], c[k]);
                    hi[k] = std::max(hi[k], c[k]);
                }
            }
        }
    };
    add(a);
    add(b);
    const float dx = hi[0] - lo[0], dy = hi[1] - lo[1], dz = hi[2] - lo[2];
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    // geometry::isFinite, not std::isfinite: the project detects non-finite values by
    // inspecting the exponent bits, because std::isfinite returns TRUE for NaN and Inf
    // under fast-math and the guard would be dead code if that flag ever came back.
    return isFinite(d) && d > 0.f ? d : 1.f;
}

struct SeedPoint {
    float ua, va, ub, vb;
};

void newtonRefine(const NurbsSurface& a, const NurbsSurface& b,
                  float& ua, float& va, float& ub, float& vb,
                  int32_t maxIter = 20) {
    auto domAU = a.domainU();
    auto domAV = a.domainV();
    auto domBU = b.domainU();
    auto domBV = b.domainV();

    for (int32_t iter = 0; iter < maxIter; ++iter) {
        Vec3 pa = a.evaluate(ua, va);
        Vec3 pb = b.evaluate(ub, vb);

        // The residual is the part of (pa - pb) ALONG B'S NORMAL, not the whole vector.
        //
        // (ub, vb) is a closest-point projection, accurate to its own tolerance, so
        // b.evaluate(ub, vb) sits a little way from pa's true foot — and that error lies
        // IN B's surface. Feeding the raw difference to Newton makes it chase a tangential
        // residual that says nothing about being off the surface, and it will happily walk
        // a point that is already exactly on the intersection curve straight off it.
        // Measured before this: a sample sitting at x-1 = 0.000e+00 reported a residual of
        // 2.25e-05, and one refinement pass moved it to x-1 = -4.46e-05 — the refinement
        // was the thing introducing the error, which is why more iterations made the curve
        // slightly worse rather than better.
        Vec3 nB = b.derivativeU(ub, vb).cross(b.derivativeV(ub, vb));
        const float nBLen = nB.length();
        Vec3 d;
        if (nBLen > kEps) {
            nB = {nB.x / nBLen, nB.y / nBLen, nB.z / nBLen};
            const float sd = (pa.x - pb.x) * nB.x + (pa.y - pb.y) * nB.y + (pa.z - pb.z) * nB.z;
            d = {nB.x * sd, nB.y * sd, nB.z * sd};
        } else {
            d = {pa.x - pb.x, pa.y - pb.y, pa.z - pb.z};
        }

        Vec3 duA = a.derivativeU(ua, va);
        Vec3 dvA = a.derivativeV(ua, va);
        float J00 = duA.dot(duA); float J01 = duA.dot(dvA);
        float J10 = dvA.dot(duA); float J11 = dvA.dot(dvA);
        float rhs0 = -d.dot(duA);
        float rhs1 = -d.dot(dvA);

        float delta_ua, delta_va, delta_ub, delta_vb;
        float det = J00 * J11 - J01 * J10;
        if (std::abs(det) > 1e-12f) {
            float s0 = (J11 * rhs0 - J01 * rhs1) / det;
            float s1 = (J00 * rhs1 - J10 * rhs0) / det;

            delta_ua = s0;
            delta_va = s1;
        } else {
            delta_ua = rhs0 * 0.5f;
            delta_va = rhs1 * 0.5f;
        }
        delta_ub = 0.f;
        delta_vb = 0.f;

        float maxDelta = std::max({std::abs(delta_ua), std::abs(delta_va),
                                    std::abs(delta_ub), std::abs(delta_vb)});
        if (maxDelta > 0.1f) {
            float scale = 0.1f / maxDelta;
            delta_ua *= scale; delta_va *= scale;
            delta_ub *= scale; delta_vb *= scale;
        }

        ua = std::clamp(ua + delta_ua, domAU.first, domAU.second);
        va = std::clamp(va + delta_va, domAV.first, domAV.second);

        // B's parameters follow by PROJECTION rather than by the ad-hoc coupling that
        // used to live here (delta_ub = -duA.duB*s0 - dvA.duB*s1, which is not a
        // derivative of anything and is not even dimensionally a parameter step). The
        // closest-point projection is the same operation the march already relies on.
        const auto proj = NurbsSurfaceClosestPoint::project(b, a.evaluate(ua, va));
        if (proj.converged) {
            ub = std::clamp(proj.u, domBU.first, domBU.second);
            vb = std::clamp(proj.v, domBV.first, domBV.second);
        }

        if (maxDelta < 1e-8f) break;
    }
}

std::vector<SeedPoint> findSeeds(const NurbsSurface& a,
                                  const NurbsSurface& b,
                                  const IntersectSeedGrid& seedGrid,
                                  float extent) {
    std::vector<SeedPoint> seeds;
    // One grid cell in model units — the reach a seed candidate is allowed, and the only
    // scale in this function that is not a bare constant.
    const float cell = extent / static_cast<float>(
        std::max(1, std::min(seedGrid.resU, seedGrid.resV) - 1));
    const float onCurve = std::max(extent * 1e-5f, 1e-7f);
    auto domAU = a.domainU();
    auto domAV = a.domainV();

    NurbsSurfaceClosestPointOptions projOpts;
    projOpts.maxIter = 15;
    projOpts.tolerance = 1e-6f;

    for (int32_t i = 0; i < seedGrid.resU; ++i) {
        for (int32_t j = 0; j < seedGrid.resV; ++j) {
            float ua = domAU.first + (domAU.second - domAU.first) * static_cast<float>(i) / static_cast<float>(std::max(1, seedGrid.resU - 1));
            float va = domAV.first + (domAV.second - domAV.first) * static_cast<float>(j) / static_cast<float>(std::max(1, seedGrid.resV - 1));

            Vec3 pt = a.evaluate(ua, va);
            auto proj = NurbsSurfaceClosestPoint::project(b, pt, projOpts);
            if (!proj.converged) continue;

            float d2 = (pt.x - proj.point.x) * (pt.x - proj.point.x) +
                       (pt.y - proj.point.y) * (pt.y - proj.point.y) +
                       (pt.z - proj.point.z) * (pt.z - proj.point.z);

            // ACCEPT GENEROUSLY, THEN VERIFY. A grid sample is a candidate if it is within
            // about one grid CELL of the other surface — the distance at which Newton can
            // reasonably be expected to reach the curve — and it is kept only if the
            // refined point really does lie on both surfaces.
            //
            // What this replaces was a fixed absolute cut, `d2 < 0.01*(1-|cos|)+0.001`,
            // i.e. a distance of 0.105 model units regardless of the grid, the surfaces or
            // their size. On the flat test fixtures the grid happens to land exactly on
            // the intersection so everything passed. On a curved one it does not: for
            // z = x² cut by z = 1 the distance at a sample is |x²−1|, and of the 16 grid
            // columns the nearest to the crossing sits at |x²−1| = 0.129 — so NO seed was
            // produced, and a real two-branch intersection came back as no intersection at
            // all. A threshold that is neither relative to the model nor to the sampling
            // is a guess that happens to fit the fixture it was written against.
            if (d2 <= cell * cell) {
                SeedPoint s{ua, va, proj.u, proj.v};
                newtonRefine(a, b, s.ua, s.va, s.ub, s.vb);
                const Vec3 pa = a.evaluate(s.ua, s.va);
                const Vec3 pb = b.evaluate(s.ub, s.vb);
                const float rx = pa.x - pb.x, ry = pa.y - pb.y, rz = pa.z - pb.z;
                if (rx * rx + ry * ry + rz * rz > onCurve * onCurve) continue;

                bool duplicate = false;
                for (const auto& existing : seeds) {
                    float du = existing.ua - s.ua;
                    float dv = existing.va - s.va;
                    if (du*du + dv*dv < 1e-8f) { duplicate = true; break; }
                }
                if (!duplicate) seeds.push_back(s);
            }
        }
    }

    return seeds;
}


// Unit tangent of the intersection curve at the current point, or false where the two
// surfaces are too nearly tangent for a direction to be decidable. |nA x nB| with both
// normals unit IS the sine of the angle between the surfaces, so the threshold is a
// scale-free measure of transversality.
bool curveTangent(const NurbsSurface& a, const NurbsSurface& b,
                  float ua, float va, float ub, float vb, Vec3& dir) {
    const Vec3 nA0 = a.derivativeU(ua, va).cross(a.derivativeV(ua, va));
    const Vec3 nB0 = b.derivativeU(ub, vb).cross(b.derivativeV(ub, vb));
    const float la = nA0.length(), lb = nB0.length();
    if (la < kEps || lb < kEps) return false;
    const Vec3 nA{nA0.x / la, nA0.y / la, nA0.z / la};
    const Vec3 nB{nB0.x / lb, nB0.y / lb, nB0.z / lb};
    const Vec3 c = nA.cross(nB);
    const float cl = c.length();
    if (cl < 1e-3f) return false;  // tangency: no decidable direction
    dir = {c.x / cl, c.y / cl, c.z / cl};
    return true;
}

// Advance one arc-length step of `stepSize` along the intersection curve.
//
// The parameter step is the least-squares solution of  du*dA/du + dv*dA/dv = stepSize*dir
// over A's tangent basis — a 2x2 Gram solve. It replaces a per-axis formula that divided
// BOTH components by |dA/du|^2 * |dA/dv|^2 (the PRODUCT of two squared lengths), which is
// dimensionally wrong and correct only when the parameterisation happens to be unit-speed.
// Measured on the test's own 3x3 plane, where |dA/du| = 3: every step came out 9x too
// short, so 100 steps advanced 0.119 of a 3-unit line instead of completing it. The Gram
// form is also right for a non-ORTHOGONAL parameterisation, which a per-axis projection
// never is.
enum class StepResult { Advanced, LeftDomain, Undecidable };

StepResult marchStep(const NurbsSurface& a, const NurbsSurface& b,
                     float& ua, float& va, float& ub, float& vb,
                     const Vec3& dir, float stepSize) {
    const auto domAU = a.domainU();
    const auto domAV = a.domainV();

    const Vec3 duA = a.derivativeU(ua, va);
    const Vec3 dvA = a.derivativeV(ua, va);

    const float g00 = duA.dot(duA), g01 = duA.dot(dvA), g11 = dvA.dot(dvA);
    const float det = g00 * g11 - g01 * g01;
    if (std::abs(det) < kEps) return StepResult::Undecidable;

    const Vec3 w{dir.x * stepSize, dir.y * stepSize, dir.z * stepSize};
    const float b0 = w.dot(duA), b1 = w.dot(dvA);
    const float deltaUa = (g11 * b0 - g01 * b1) / det;
    const float deltaVa = (g00 * b1 - g01 * b0) / det;

    float nu = ua + deltaUa;
    float nv = va + deltaVa;

    // Leaving the domain is a real end of the curve, not a failure. Clamp onto the
    // boundary and report it, so the caller can record that last point instead of
    // dropping it — the previous code returned false here and the caller broke out
    // BEFORE appending, losing the endpoint and making its own hitBoundary flag dead.
    const bool outside = nu < domAU.first || nu > domAU.second ||
                         nv < domAV.first || nv > domAV.second;
    nu = std::clamp(nu, domAU.first, domAU.second);
    nv = std::clamp(nv, domAV.first, domAV.second);
    ua = nu;
    va = nv;

    const auto proj = NurbsSurfaceClosestPoint::project(b, a.evaluate(ua, va));
    if (!proj.converged) return StepResult::Undecidable;
    ub = proj.u;
    vb = proj.v;

    return outside ? StepResult::LeftDomain : StepResult::Advanced;
}

// March from a seed in ONE direction, appending as it goes. Returns true if the curve
// closed back onto `origin`.
bool marchFrom(const NurbsSurface& a, const NurbsSurface& b,
               float ua, float va, float ub, float vb,
               const Vec3& origin, float sign, int32_t maxSteps, float stepSize,
               std::vector<Vec3>& out) {
    Vec3 prevDir{0.f, 0.f, 0.f};
    bool havePrev = false;

    for (int32_t step = 0; step < maxSteps; ++step) {
        Vec3 dir;
        if (!curveTangent(a, b, ua, va, ub, vb, dir)) return false;
        dir = {dir.x * sign, dir.y * sign, dir.z * sign};

        // nA x nB has an arbitrary sign, and it can flip from one evaluation to the next.
        // Without this the trace reverses mid-curve and walks back over itself — two of
        // the measured curves ran backwards (x 0.586 -> 0.550, 0.996 -> 0.811).
        if (havePrev && dir.dot(prevDir) < 0.f) dir = {-dir.x, -dir.y, -dir.z};
        prevDir = dir;
        havePrev = true;

        const StepResult r = marchStep(a, b, ua, va, ub, vb, dir, stepSize);
        if (r == StepResult::Undecidable) return false;
        newtonRefine(a, b, ua, va, ub, vb, 10);
        const Vec3 p = a.evaluate(ua, va);
        out.push_back(p);
        if (r == StepResult::LeftDomain) return false;

        // Closure, so a closed intersection stops instead of circling until the step
        // budget runs out. Every measured curve used its full budget of 101 points.
        if (step >= 3) {
            const Vec3 d{p.x - origin.x, p.y - origin.y, p.z - origin.z};
            if (d.length() <= stepSize * 1.5f) return true;
        }
    }
    return false;
}

std::vector<Vec3> traceCurve(const NurbsSurface& a, const NurbsSurface& b,
                              float ua0, float va0, float ub0, float vb0,
                              int32_t maxSteps, float stepSize) {
    float ua = ua0, va = va0, ub = ub0, vb = vb0;
    newtonRefine(a, b, ua, va, ub, vb);
    const Vec3 origin = a.evaluate(ua, va);

    std::vector<Vec3> forward;
    const bool closed =
        marchFrom(a, b, ua, va, ub, vb, origin, +1.f, maxSteps, stepSize, forward);

    std::vector<Vec3> curve;
    if (!closed) {
        // An OPEN curve seeded in its middle keeps only the half that was walked, unless
        // the other half is walked too. The previous tracer marched one direction only,
        // so a mid-curve seed silently returned half an answer.
        std::vector<Vec3> backward;
        (void)marchFrom(a, b, ua, va, ub, vb, origin, -1.f, maxSteps, stepSize, backward);
        std::reverse(backward.begin(), backward.end());
        curve = std::move(backward);
    }
    curve.push_back(origin);
    curve.insert(curve.end(), forward.begin(), forward.end());
    return curve;
}

} // namespace

std::vector<std::vector<Vec3>> SurfaceSurfaceIntersect::intersect(
    const NurbsSurface& a,
    const NurbsSurface& b,
    const IntersectSeedGrid& seedGrid,
    int32_t marchSteps,
    float stepSize) {

    std::vector<std::vector<Vec3>> curves;

    if (!a.isValid() || !b.isValid()) return curves;

    // A step and a budget in MODEL units. The old defaults were a fixed 0.01 with 100
    // steps, which can only complete a curve on a surface that happens to be about a unit
    // across; a non-positive value now means "proportion it to the model", which is the
    // only way one default can be right at 0.5 mm and at 5 km.
    const float extent = modelExtent(a, b);
    const float h = stepSize > 0.f ? stepSize : std::max(extent * 0.01f, 1e-6f);
    const int32_t budget = marchSteps > 0 ? marchSteps : 4000;

    // Two seeds on the same curve produce the same curve. Nothing removed those
    // duplicates, so a single straight line came back THIRTY-TWO times, 3186 points for
    // an answer that needs two. Skip a seed already covered by something traced.
    const float dedupe = h * 1.5f;
    auto alreadyTraced = [&](const Vec3& p) {
        for (const auto& c : curves) {
            for (const Vec3& q : c) {
                const float dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
                if (dx * dx + dy * dy + dz * dz < dedupe * dedupe) return true;
            }
        }
        return false;
    };

    for (const auto& seed : findSeeds(a, b, seedGrid, extent)) {
        float ua = seed.ua, va = seed.va, ub = seed.ub, vb = seed.vb;
        newtonRefine(a, b, ua, va, ub, vb);
        if (alreadyTraced(a.evaluate(ua, va))) continue;

        auto curve = traceCurve(a, b, seed.ua, seed.va, seed.ub, seed.vb, budget, h);
        if (curve.size() >= 3) curves.push_back(std::move(curve));
    }

    return curves;
}

} // namespace nexus::geometry
