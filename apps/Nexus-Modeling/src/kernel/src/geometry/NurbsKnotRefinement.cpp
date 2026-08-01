#include <nexus/geometry/NurbsKnotRefinement.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

NurbsCurve NurbsKnotRefinement::insertKnot(const NurbsCurve& curve, float u) {
    if (!curve.isValid()) return curve;
    auto [uMin, uMax] = curve.domain();
    if (u < uMin || u > uMax) return curve;
    return curve.insertKnot(u, 1);
}

NurbsCurve NurbsKnotRefinement::refineCurve(const NurbsCurve& curve,
                                             const std::vector<float>& knots) {
    if (!curve.isValid()) return curve;
    NurbsCurve result = curve;
    for (float u : knots) {
        auto [uMin, uMax] = result.domain();
        if (u >= uMin && u <= uMax)
            result = result.insertKnot(u, 1);
    }
    return result;
}

namespace {

// Knot values are compared against each other, never accumulated, so equality is what is
// wanted; the tolerance only absorbs a knot that has been through an insertion. It is
// relative to the knot range because an absolute 1e-10f is far below float resolution once
// a knot is near 1 (float eps is ~1.2e-7), which made the old comparison exact-equality by
// accident rather than by decision.
[[nodiscard]] float knotTolerance(const NurbsCurve& c) noexcept {
    const auto& k = c.knots();
    if (k.size() < 2) return 1e-6f;
    const float span = std::abs(k.back() - k.front());
    return std::max(1e-7f * std::max(1.f, span), span * 1e-6f);
}

// The DISTINCT interior knot values, collected up front. Taking a copy is load-bearing:
// every insertion rebuilds the curve, so a reference into its knot vector dangles the
// moment the curve is reassigned.
[[nodiscard]] std::vector<float> distinctInteriorKnots(const NurbsCurve& c) {
    const std::vector<float> knots = c.knots();  // by value, deliberately
    const auto [uMin, uMax] = c.domain();
    const float tol = knotTolerance(c);
    std::vector<float> out;
    for (const float k : knots) {
        if (k <= uMin + tol || k >= uMax - tol) continue;
        if (!out.empty() && std::abs(k - out.back()) <= tol) continue;
        out.push_back(k);
    }
    return out;
}

[[nodiscard]] uint32_t multiplicityOf(const NurbsCurve& c, float u, float tol) noexcept {
    uint32_t n = 0;
    for (const float k : c.knots())
        if (std::abs(k - u) <= tol) ++n;
    return n;
}

}  // namespace

// Raise the degree to `targetDegree` while leaving the curve geometrically UNCHANGED.
//
// The previous implementation inserted a knot at the domain midpoint and returned. Knot
// insertion cannot change a degree, so it returned a curve of the ORIGINAL degree carrying
// spurious knots — a silent no-op with respect to the one thing the function is named for.
//
// This is the standard decompose / elevate / recompose: split into Bezier segments, raise
// each segment with the Bezier degree-elevation identity
//     Q_0 = P_0,  Q_i = (i/(p+1)) P_{i-1} + (1 - i/(p+1)) P_i,  Q_{p+1} = P_p
// and reassemble. Rational curves are elevated in HOMOGENEOUS coordinates (w*x, w*y, w*z, w)
// and projected back, which is what makes a rational curve's shape survive.
//
// NOTE, stated rather than implied: the result keeps every interior knot at the elevated
// multiplicity, so the knot vector is VALID AND EXACT but not minimal. Restoring minimal
// multiplicity is knot REMOVAL (Piegl & Tiller A5.8), a separate algorithm with its own
// tolerance decisions; it changes the representation, never the curve.
NurbsCurve NurbsKnotRefinement::degreeElevate(const NurbsCurve& curve, int32_t targetDegree) {
    if (!curve.isValid()) return curve;
    if (targetDegree <= curve.degree()) return curve;

    NurbsCurve result = curve;
    for (int32_t step = result.degree(); step < targetDegree; ++step) {
        const NurbsCurve bez = bezierDecomposition(result);
        if (!bez.isValid()) return result;

        const int32_t p = bez.degree();
        const std::vector<float> interior = distinctInteriorKnots(bez);
        const size_t segments = interior.size() + 1;
        const std::vector<Vec3>& P = bez.controlPoints();
        const bool rational = bez.isRational();
        const std::vector<float>& W = bez.weights();

        if (P.size() != segments * static_cast<size_t>(p) + 1) return result;  // not decomposed

        std::vector<Vec3> outPts;
        std::vector<float> outW;
        outPts.reserve(segments * static_cast<size_t>(p + 1) + 1);
        if (rational) outW.reserve(segments * static_cast<size_t>(p + 1) + 1);

        for (size_t s = 0; s < segments; ++s) {
            const size_t base = s * static_cast<size_t>(p);
            // homogeneous control points of this Bezier segment
            std::vector<float> hx(static_cast<size_t>(p) + 1), hy(hx.size()), hz(hx.size()),
                hw(hx.size());
            for (int32_t i = 0; i <= p; ++i) {
                const size_t k = base + static_cast<size_t>(i);
                const float w = rational ? W[k] : 1.f;
                hx[static_cast<size_t>(i)] = P[k].x * w;
                hy[static_cast<size_t>(i)] = P[k].y * w;
                hz[static_cast<size_t>(i)] = P[k].z * w;
                hw[static_cast<size_t>(i)] = w;
            }
            // elevate p -> p+1
            std::vector<float> ex(static_cast<size_t>(p) + 2), ey(ex.size()), ez(ex.size()),
                ew(ex.size());
            for (int32_t i = 0; i <= p + 1; ++i) {
                const float a = static_cast<float>(i) / static_cast<float>(p + 1);
                const size_t I = static_cast<size_t>(i);
                const float lx = (i > 0) ? hx[I - 1] : 0.f, ly = (i > 0) ? hy[I - 1] : 0.f;
                const float lz = (i > 0) ? hz[I - 1] : 0.f, lw = (i > 0) ? hw[I - 1] : 0.f;
                const float rx = (i <= p) ? hx[I] : 0.f, ry = (i <= p) ? hy[I] : 0.f;
                const float rz = (i <= p) ? hz[I] : 0.f, rw = (i <= p) ? hw[I] : 0.f;
                ex[I] = a * lx + (1.f - a) * rx;
                ey[I] = a * ly + (1.f - a) * ry;
                ez[I] = a * lz + (1.f - a) * rz;
                ew[I] = a * lw + (1.f - a) * rw;
            }
            // append, sharing the junction point with the previous segment
            for (int32_t i = (s == 0 ? 0 : 1); i <= p + 1; ++i) {
                const size_t I = static_cast<size_t>(i);
                const float w = ew[I];
                const float inv = (std::abs(w) > 1e-20f) ? 1.f / w : 0.f;
                outPts.push_back(Vec3{ex[I] * inv, ey[I] * inv, ez[I] * inv});
                if (rational) outW.push_back(w);
            }
        }

        const auto [uMin, uMax] = bez.domain();
        std::vector<float> outKnots;
        outKnots.reserve(outPts.size() + static_cast<size_t>(p) + 3);
        for (int32_t i = 0; i < p + 2; ++i) outKnots.push_back(uMin);
        for (const float k : interior)
            for (int32_t i = 0; i < p + 1; ++i) outKnots.push_back(k);
        for (int32_t i = 0; i < p + 2; ++i) outKnots.push_back(uMax);

        NurbsCurve elevated = rational
            ? NurbsCurve(p + 1, std::move(outKnots), std::move(outPts), std::move(outW))
            : NurbsCurve(p + 1, std::move(outKnots), std::move(outPts));
        if (!elevated.isValid()) return result;  // never return something worse than the input
        result = std::move(elevated);
    }
    return result;
}

// Split the curve into Bezier segments by raising every distinct INTERIOR knot to
// multiplicity `degree`. The curve is unchanged; only its representation is.
//
// The previous implementation crashed. It held `const auto& knots = result.knots()` and then
// reassigned `result` inside the loop, so every subsequent read of `knots.size()` and
// `knots[i]` was a USE-AFTER-FREE — a segfault on an ordinary degree-2 curve with one
// interior knot. It also inserted at the MIDPOINT between adjacent knots, which subdivides
// the curve at new parameters rather than raising the multiplicity of the knots already
// there, so even without the crash it did not decompose anything.
NurbsCurve NurbsKnotRefinement::bezierDecomposition(const NurbsCurve& curve) {
    if (!curve.isValid()) return curve;

    const int32_t p = curve.degree();
    const std::vector<float> interior = distinctInteriorKnots(curve);

    NurbsCurve result = curve;
    for (const float u : interior) {
        const float tol = knotTolerance(result);
        const uint32_t mult = multiplicityOf(result, u, tol);
        if (mult >= static_cast<uint32_t>(p)) continue;
        // insertKnot's own working buffers assume r <= p - s, so ask for exactly the deficit
        NurbsCurve next = result.insertKnot(u, static_cast<int32_t>(static_cast<uint32_t>(p) - mult));
        if (!next.isValid()) return result;
        result = std::move(next);
    }
    return result;
}

uint32_t NurbsKnotRefinement::knotMultiplicity(const NurbsCurve& curve, float u) {
    return multiplicityOf(curve, u, knotTolerance(curve));
}

NurbsSurface NurbsKnotRefinement::insertKnotU(const NurbsSurface& surface, float u) {
    if (!surface.isValid()) return surface;
    auto [uMin, uMax] = surface.domainU();
    if (u < uMin || u > uMax) return surface;
    return surface.insertKnotU(u, 1);
}

NurbsSurface NurbsKnotRefinement::insertKnotV(const NurbsSurface& surface, float v) {
    if (!surface.isValid()) return surface;
    auto [vMin, vMax] = surface.domainV();
    if (v < vMin || v > vMax) return surface;
    return surface.insertKnotV(v, 1);
}

NurbsSurface NurbsKnotRefinement::refineSurface(const NurbsSurface& surface,
                                                  const std::vector<float>& knotsU,
                                                  const std::vector<float>& knotsV) {
    if (!surface.isValid()) return surface;
    NurbsSurface result = surface;

    for (float u : knotsU) {
        auto [uMin, uMax] = result.domainU();
        if (u >= uMin && u <= uMax)
            result = result.insertKnotU(u, 1);
    }
    for (float v : knotsV) {
        auto [vMin, vMax] = result.domainV();
        if (v >= vMin && v <= vMax)
            result = result.insertKnotV(v, 1);
    }

    return result;
}

} // namespace nexus::geometry
