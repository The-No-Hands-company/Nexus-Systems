#include <nexus/geometry/CurveExtrude.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

float degToRad(float deg) noexcept
{
    return deg * static_cast<float>(std::numbers::pi) / 180.f;
}

Vec3 computeCentroid(const std::vector<Vec3>& pts) noexcept
{
    if (pts.empty()) return {0.f, 0.f, 0.f};
    Vec3 sum{0.f, 0.f, 0.f};
    for (const auto& p : pts) sum = sum + p;
    return sum * (1.f / static_cast<float>(pts.size()));
}

int32_t profileSampleCount(const NurbsCurve& profile) noexcept
{
    int32_t n = profile.controlPointCount();
    if (n < 2) n = 20;
    if (n > 200) n = 200;
    return n;
}

} // namespace

CurveExtrudeReport CurveExtrudeOperation::extrude(const NurbsCurve&        profile,
                                                   const CurveExtrudeDesc& desc,
                                                   NurbsSurface&            outSurface,
                                                   Mesh*                    outMesh) noexcept
{
    outSurface = {};
    if (outMesh) *outMesh = {};

    CurveExtrudeReport report{};

    if (!profile.isValid()) {
        report.diagnostic = CurveExtrudeDiagnostic::InvalidProfileCurve;
        report.converged  = false;
        return report;
    }

    float dirLen = desc.direction.length();
    // Scale-adaptive zero check for direction vector
    float maxCoord = std::max({std::abs(desc.direction.x), std::abs(desc.direction.y), std::abs(desc.direction.z)});
    float dirLenTolerance = std::max(maxCoord * 1e-8f, 1e-12f); // Relative tolerance with floor
    if (dirLen < dirLenTolerance) {
        report.diagnostic = CurveExtrudeDiagnostic::InvalidDirection;
        report.converged  = false;
        return report;
    }

    if (desc.height <= 0.f) {
        report.diagnostic = CurveExtrudeDiagnostic::InvalidHeight;
        report.converged  = false;
        return report;
    }

    const uint32_t hSteps  = std::max(desc.heightSamples, 2u);
    const int32_t  pSteps  = profileSampleCount(profile);
    const auto     profDom = profile.domain();

    const Vec3  unitDir     = desc.direction * (1.f / dirLen);
    const float totalHeight = desc.height;
    const float draftAngleRad = degToRad(std::clamp(desc.draftAngleDeg, -89.f, 89.f));
    const float draftFactor = std::tan(draftAngleRad);

    // Compute maximum safe fraction where scale would cross zero.
    // scale = 1 - fraction * draftFactor * height, so scale >= 0 when
    // fraction <= 1 / (draftFactor * height) for positive draftFactor,
    // and fraction >= 1 / (draftFactor * height) for negative draftFactor.
    // Scale-adaptive check for near-zero denominator to avoid numerical instability
    float heightScale = std::max(std::abs(totalHeight), 1.0f); // Use height as scale reference with floor
    float denomThreshold = heightScale * 1e-8f; // Relative tolerance
    const float maxFraction = (std::abs(draftFactor * totalHeight) > denomThreshold)
        ? 1.f / (draftFactor * totalHeight)
        : std::numeric_limits<float>::max();

    // Sample the base profile
    std::vector<Vec3> basePts(static_cast<size_t>(pSteps));
    for (int32_t j = 0; j < pSteps; ++j) {
        float t = (pSteps == 1) ? 0.5f
            : profDom.first + (profDom.second - profDom.first) * static_cast<float>(j) / static_cast<float>(pSteps - 1);
        basePts[static_cast<size_t>(j)] = profile.evaluate(t);
    }

    const Vec3 centroid = computeCentroid(basePts);

    // Generate extruded vertex grid: [hSteps × pSteps]
    const uint32_t hs = hSteps;
    const uint32_t ps = static_cast<uint32_t>(pSteps);
    std::vector<Vec3> allVerts;
    allVerts.reserve(static_cast<size_t>(hs) * static_cast<size_t>(ps));

    for (uint32_t i = 0; i < hs; ++i) {
        float fraction = (hs == 1) ? 0.5f
            : static_cast<float>(i) / static_cast<float>(hs - 1);
        float h = fraction * totalHeight;

        float safeFrac = std::min(fraction, maxFraction);
        float scale = 1.f - safeFrac * draftFactor * totalHeight;
        if (scale < 0.f) scale = 0.f;

        for (uint32_t j = 0; j < ps; ++j) {
            Vec3 pt = basePts[j];
            Vec3 scaled = centroid + (pt - centroid) * scale;
            allVerts.push_back(scaled + unitDir * h);
        }
    }

    report.vertexCount = static_cast<uint32_t>(allVerts.size());

    // Output NURBS surface if requested
    if (desc.outputAsNurbsSurface && static_cast<int32_t>(ps) > 1 && static_cast<int32_t>(hs) > 1) {
        int32_t degU = std::min(3, static_cast<int32_t>(ps) - 1);
        int32_t degV = std::min(3, static_cast<int32_t>(hs) - 1);
        degU = std::max(degU, 1);
        degV = std::max(degV, 1);
        int32_t nU = static_cast<int32_t>(ps);
        int32_t nV = static_cast<int32_t>(hs);

        std::vector<float> knotU(static_cast<size_t>(nU + degU + 1));
        for (int32_t j = 0; j <= degU; ++j) knotU[static_cast<size_t>(j)] = 0.f;
        for (int32_t j = 1; j < nU - degU; ++j) {
            knotU[static_cast<size_t>(degU + j)] = static_cast<float>(j) / static_cast<float>(nU - degU);
        }
        for (int32_t j = 0; j <= degU; ++j) knotU[knotU.size() - 1 - static_cast<size_t>(j)] = 1.f;

        std::vector<float> knotV(static_cast<size_t>(nV + degV + 1));
        for (int32_t j = 0; j <= degV; ++j) knotV[static_cast<size_t>(j)] = 0.f;
        for (int32_t j = 1; j < nV - degV; ++j) {
            knotV[static_cast<size_t>(degV + j)] = static_cast<float>(j) / static_cast<float>(nV - degV);
        }
        for (int32_t j = 0; j <= degV; ++j) knotV[knotV.size() - 1 - static_cast<size_t>(j)] = 1.f;

        outSurface = NurbsSurface(degU, degV,
                                   std::move(knotU), std::move(knotV),
                                   allVerts, nU, nV);
    }

    // Output mesh if requested
    if (outMesh) {
        Mesh result;
        result.attributes().setPositions(allVerts);
        auto& topo = result.topology();

        for (uint32_t i = 0; i + 1 < hs; ++i) {
            for (uint32_t j = 0; j + 1 < ps; ++j) {
                uint32_t v00 = i * ps + j;
                uint32_t v01 = (i + 1) * ps + j;
                uint32_t v11 = (i + 1) * ps + (j + 1);
                uint32_t v10 = i * ps + (j + 1);

                topo.addFace(Face{{v00, v01, v11}});
                topo.addFace(Face{{v00, v11, v10}});
            }
        }

        report.faceCount = static_cast<uint32_t>(topo.faceCount());

        // Add end caps (bottom and top faces).
        if (desc.capEnds && ps >= 3) {
            // Bottom cap: fan from first vertex of bottom row (i=0).
            // Bottom row vertices: 0..ps-1
            uint32_t base = 0;
            uint32_t pivot = base + 0;  // fan pivot: vertex 0 of bottom row
            for (uint32_t j = 1; j + 1 < ps; ++j) {
                topo.addFace(Face{{pivot, base + j + 1, base + j}});
            }

            // Top cap: fan from first vertex of top row (i=hs-1), reverse winding.
            // Top row vertices: (hs-1)*ps .. hs*ps-1
            base = (hs - 1) * ps;
            pivot = base + 0;  // fan pivot: vertex 0 of top row
            for (uint32_t j = 1; j + 1 < ps; ++j) {
                topo.addFace(Face{{pivot, base + j, base + j + 1}});
            }

            report.capFaceCount = 2 * (ps - 2);
            report.faceCount = static_cast<uint32_t>(topo.faceCount());
        }

        *outMesh = std::move(result);
    }

    return report;
}

} // namespace nexus::geometry
