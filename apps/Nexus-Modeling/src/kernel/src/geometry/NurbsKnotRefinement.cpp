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

NurbsCurve NurbsKnotRefinement::degreeElevate(const NurbsCurve& curve, int32_t targetDegree) {
    if (!curve.isValid()) return curve;
    if (targetDegree <= curve.degree()) return curve;

    NurbsCurve result = curve;
    for (int32_t d = curve.degree(); d < targetDegree; ++d) {
        const auto& knots = result.knots();
        if (knots.size() < 2) break;

        float midU = (knots.front() + knots.back()) * 0.5f;
        result = result.insertKnot(midU, targetDegree - d);
    }
    return result;
}

NurbsCurve NurbsKnotRefinement::bezierDecomposition(const NurbsCurve& curve) {
    if (!curve.isValid()) return curve;

    NurbsCurve result = curve;
    const auto& knots = result.knots();
    int32_t degree = result.degree();

    for (size_t i = static_cast<size_t>(degree); i + 1 < knots.size(); ++i) {
        if (std::abs(knots[i + 1] - knots[i]) > 1e-10f) {
            float midKnot = (knots[i] + knots[i + 1]) * 0.5f;
            auto domain = result.domain();
            if (midKnot > domain.first && midKnot < domain.second) {
                result = result.insertKnot(midKnot, degree);
            }
        }
    }

    return result;
}

uint32_t NurbsKnotRefinement::knotMultiplicity(const NurbsCurve& curve, float u) {
    const auto& knots = curve.knots();
    uint32_t count = 0;
    for (float k : knots) {
        if (std::abs(k - u) < 1e-10f) ++count;
    }
    return count;
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
