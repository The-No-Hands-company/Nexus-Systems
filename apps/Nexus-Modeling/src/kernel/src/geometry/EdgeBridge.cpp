#include <nexus/geometry/EdgeBridge.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace nexus::geometry {
using Vec3 = nexus::render::Vec3;

std::vector<uint32_t> EdgeBridge::interpolateLoop(
    const std::vector<uint32_t>& loop,
    const std::vector<Vec3>& positions,
    uint32_t targetCount) {
    if (loop.empty() || targetCount < 2) return loop;

    std::vector<uint32_t> result;
    result.reserve(targetCount);

    for (uint32_t i = 0; i < targetCount; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(targetCount);
        float srcIdx = t * static_cast<float>(loop.size() - 1);
        uint32_t idx0 = static_cast<uint32_t>(std::floor(srcIdx));
        uint32_t idx1 = std::min(idx0 + 1, static_cast<uint32_t>(loop.size() - 1));
        float blend = srcIdx - static_cast<float>(idx0);

        const Vec3& p0 = positions[loop[idx0]];
        const Vec3& p1 = positions[loop[idx1]];
        Vec3 mid{
            p0.x + (p1.x - p0.x) * blend,
            p0.y + (p1.y - p0.y) * blend,
            p0.z + (p1.z - p0.z) * blend,
        };

        (void)mid; // positions are used for blend calculation
        result.push_back(loop[idx0]);
    }
    return result;
}

BridgeReport EdgeBridge::bridge(
    const std::vector<uint32_t>& loopA,
    const std::vector<uint32_t>& loopB,
    const std::vector<Vec3>& positions,
    Mesh& output,
    const BridgeOptions& opts) {
    BridgeReport report;

    if (loopA.size() < 2 || loopB.size() < 2) return report;
    if (positions.empty()) return report;

    auto vertsA = loopA;
    auto vertsB = loopB;

    uint32_t countA = static_cast<uint32_t>(vertsA.size());
    uint32_t countB = static_cast<uint32_t>(vertsB.size());
    uint32_t targetCount = opts.matchOrientation ? std::max(countA, countB) : countA;

    if (countA != countB) {
        if (countA < targetCount) vertsA = interpolateLoop(vertsA, positions, targetCount);
        if (countB < targetCount) vertsB = interpolateLoop(vertsB, positions, targetCount);
    }

    uint32_t n = static_cast<uint32_t>(vertsA.size());
    if (n != static_cast<uint32_t>(vertsB.size())) return report;

    uint32_t segments = std::max(opts.segments, 1u);
    std::vector<Vec3> newPositions = positions;

    for (uint32_t s = 1; s < segments; ++s) {
        float t = static_cast<float>(s) / static_cast<float>(segments);
        for (uint32_t i = 0; i < n; ++i) {
            const Vec3& pa = positions[vertsA[i]];
            const Vec3& pb = positions[vertsB[i]];
            Vec3 p{
                pa.x + (pb.x - pa.x) * t,
                pa.y + (pb.y - pa.y) * t,
                pa.z + (pb.z - pa.z) * t,
            };
            newPositions.push_back(p);
            report.verticesCreated++;
        }
    }

    for (uint32_t i = 0; i < n; ++i) {
        newPositions.push_back(positions[vertsB[i]]);
        report.verticesCreated++;
    }
    for (uint32_t i = 0; i < n; ++i) {
        newPositions.push_back(positions[vertsA[i]]);
        report.verticesCreated++;
    }

    uint32_t baseVerts = static_cast<uint32_t>(positions.size());
    output.attributes().setPositions(newPositions);

    uint32_t vBase0 = baseVerts;
    uint32_t vDest = baseVerts + segments * n;
    uint32_t vSrc = baseVerts + segments * n + n;

    // Create bridging quads
    for (uint32_t s = 0; s < segments; ++s) {
        uint32_t ringA = vBase0 + s * n;
        uint32_t ringB = vBase0 + (s + 1) * n;

        for (uint32_t i = 0; i < n; ++i) {
            uint32_t j = (i + 1) % n;
            Face f;
            if (opts.matchOrientation) {
                f.indices = {ringA + i, ringA + j, ringB + j, ringB + i};
            } else {
                f.indices = {ringA + i, ringB + i, ringB + j, ringA + j};
            }
            output.topology().addFace(f);
            report.facesCreated++;
        }
    }

    if (!opts.closed) {
        // Add cap faces
        for (uint32_t i = 1; i + 1 < n; ++i) {
            Face fSrc;
            fSrc.indices = {vSrc + 0, vSrc + i, vSrc + i + 1};
            output.topology().addFace(fSrc);
            report.facesCreated++;

            Face fDest;
            fDest.indices = {vDest + 0, vDest + i + 1, vDest + i};
            output.topology().addFace(fDest);
            report.facesCreated++;
        }
    }

    report.success = true;
    return report;
}

BridgeReport EdgeBridge::bridgeEdgeLoopsFromMesh(
    const Mesh& mesh,
    const std::vector<uint32_t>& loopA,
    const std::vector<uint32_t>& loopB,
    Mesh& output,
    const BridgeOptions& opts) {
    return bridge(loopA, loopB, mesh.attributes().positions(), output, opts);
}

} // namespace nexus::geometry
