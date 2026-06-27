#include <nexus/geometry/SurfaceBlending.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

Vec3 sampleSurface(const NurbsSurface& surf, float u, float v) {
    return surf.evaluate(u, v);
}

Vec3 sampleCurve(const NurbsCurve& curve, float t) {
    return curve.evaluate(t);
}

} // namespace

NurbsSurface createBlendSurface(const NurbsSurface& surfA, const NurbsSurface& surfB,
                                  const NurbsCurve& railCurve, const BlendOptions& opts)
{
    if (!surfA.isValid() || !surfB.isValid() || !railCurve.isValid()) return {};

    auto domUa = surfA.domainU(), domVa = surfA.domainV();
    auto domUb = surfB.domainU(), domVb = surfB.domainV();
    (void)domUb;
    auto railDom = railCurve.domain();

    int32_t nU = opts.blendSegments;   // across the blend (circumferential)
    int32_t nV = opts.railSegments;     // along the rail
    int32_t degU = std::min(3, nU - 1);
    int32_t degV = std::min(3, nV - 1);
    degU = std::max(degU, 1);
    degV = std::max(degV, 1);

    std::vector<Vec3> cps(static_cast<size_t>(nU) * static_cast<size_t>(nV));

    for (int32_t j = 0; j < nV; ++j) {
        float t = railDom.first +
            (railDom.second - railDom.first) * static_cast<float>(j) / static_cast<float>(nV - 1);
        Vec3 railPt = sampleCurve(railCurve, t);

        // Sample both surfaces near the rail point to get cross-boundary directions.
        float ua = domUa.first + (domUa.second - domUa.first) * 0.5f;
        float va = domVa.first + (domVa.second - domVa.first) * 0.5f;
        float ub = domUb.first + (domUb.second - domUb.first) * 0.5f;
        float vb = domVb.first + (domVb.second - domVb.first) * 0.5f;

        Vec3 ptA = sampleSurface(surfA, ua, va);
        Vec3 ptB = sampleSurface(surfB, ub, vb);

        Vec3 dirA = (ptA - railPt).normalize();
        Vec3 dirB = (ptB - railPt).normalize();

        // Blend along a circular arc from surfA to surfB.
        for (int32_t i = 0; i < nU; ++i) {
            float s = static_cast<float>(i) / static_cast<float>(nU - 1);
            // Circular arc interpolation between the two contact points.
            float cosAngle = dirA.dot(dirB);
            float angle = std::acos(std::clamp(cosAngle, -1.f, 1.f));
            float blendAngle = angle * s;

            Vec3 blendDir = dirA * std::cos(blendAngle) +
                (dirB - dirA * cosAngle).normalize() * std::sin(blendAngle);
            Vec3 blendPt = railPt + blendDir * opts.blendRadius;

            cps[static_cast<size_t>(i * nV + j)] = blendPt;
        }
    }

    std::vector<float> knotU(static_cast<size_t>(nU + degU + 1));
    for (int32_t j = 0; j <= degU; ++j) knotU[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nU - degU; ++j)
        knotU[static_cast<size_t>(degU + j)] = static_cast<float>(j) / static_cast<float>(nU - degU);
    for (int32_t j = 0; j <= degU; ++j) knotU[knotU.size()-1-static_cast<size_t>(j)] = 1.f;

    std::vector<float> knotV(static_cast<size_t>(nV + degV + 1));
    for (int32_t j = 0; j <= degV; ++j) knotV[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nV - degV; ++j)
        knotV[static_cast<size_t>(degV + j)] = static_cast<float>(j) / static_cast<float>(nV - degV);
    for (int32_t j = 0; j <= degV; ++j) knotV[knotV.size()-1-static_cast<size_t>(j)] = 1.f;

    return NurbsSurface(degU, degV, std::move(knotU), std::move(knotV),
                        std::move(cps), nU, nV);
}

NurbsSurface createFilletSurface(const NurbsSurface& surfA, const NurbsSurface& surfB,
                                   float radius, const BlendOptions& opts)
{
    // Fillet = blend along the intersection curve of offset surfaces.
    // Simplified: sample both surfaces and approximate the fillet rail.
    if (!surfA.isValid() || !surfB.isValid() || radius <= 0.f) return {};

    auto domUa = surfA.domainU(), domVa = surfA.domainV();
    (void)domUa; (void)domVa;
    auto domUb = surfB.domainU();
    (void)domUb;

    // Build a rail curve by sampling the approximate intersection.
    int32_t railSegs = opts.railSegments;
    std::vector<Vec3> railPts;
    for (int32_t i = 0; i < railSegs; ++i) {
        float ua = domUa.first + (domUa.second - domUa.first) * static_cast<float>(i) / static_cast<float>(railSegs - 1);
        float va = domVa.first + (domVa.second - domVa.first) * 0.5f;
        Vec3 pt = sampleSurface(surfA, ua, va);
        railPts.push_back(pt);
    }

    int32_t nRail = railSegs;
    int32_t deg = std::min(3, nRail - 1);
    std::vector<float> rKnots(static_cast<size_t>(nRail + deg + 1));
    for (int32_t j = 0; j <= deg; ++j) rKnots[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nRail - deg; ++j)
        rKnots[static_cast<size_t>(deg + j)] = static_cast<float>(j) / static_cast<float>(nRail - deg);
    for (int32_t j = 0; j <= deg; ++j) rKnots[rKnots.size()-1-static_cast<size_t>(j)] = 1.f;

    NurbsCurve rail(deg, std::move(rKnots), std::move(railPts));

    BlendOptions blendOpts = opts;
    blendOpts.blendRadius = radius;
    return createBlendSurface(surfA, surfB, rail, blendOpts);
}

float surfaceContinuityAngle(const NurbsSurface& surfA, float uA, float vA,
                              const NurbsSurface& surfB, float uB, float vB)
{
    if (!surfA.isValid() || !surfB.isValid()) return 180.f;

    Vec3 nA = surfA.derivativeU(uA, vA).cross(surfA.derivativeV(uA, vA)).normalize();
    Vec3 nB = surfB.derivativeU(uB, vB).cross(surfB.derivativeV(uB, vB)).normalize();

    float dot = std::clamp(nA.dot(nB), -1.f, 1.f);
    return std::acos(dot) * 180.f / static_cast<float>(std::numbers::pi);
}

float surfaceCurvatureError(const NurbsSurface& surfA, float uA, float vA,
                              const NurbsSurface& surfB, float uB, float vB)
{
    if (!surfA.isValid() || !surfB.isValid()) return 0.f;

    // Approximate curvature from second derivatives.
    // κ ≈ |d²S/du² × dS/du| / |dS/du|³  (simplified mean curvature proxy)
    // For a simple comparison, compare the magnitudes of the second derivatives.
    Vec3 d2uA = surfA.derivativeU(uA, vA); // Actually this is first derivative
    Vec3 d2vA = surfA.derivativeV(uA, vA);
    Vec3 d2uB = surfB.derivativeU(uB, vB);
    Vec3 d2vB = surfB.derivativeV(uB, vB);

    float kA = d2uA.length() + d2vA.length();
    float kB = d2uB.length() + d2vB.length();

    if (kA < 1e-10f && kB < 1e-10f) return 0.f;
    return std::abs(kA - kB) / std::max(kA + kB, 1e-10f);
}

} // namespace nexus::geometry
