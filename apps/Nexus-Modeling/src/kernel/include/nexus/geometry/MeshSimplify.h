#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

struct SimplifyOptions {
    uint32_t targetFaceCount = 500;
};

class MeshSimplify {
public:
    struct SimplifyStats {
        uint32_t originalFaces = 0, originalVertices = 0;
        uint32_t simplifiedFaces = 0, simplifiedVertices = 0;
        float faceReductionPercent = 0, vertexReductionPercent = 0;
    };
    [[nodiscard]] static Mesh decimate(const Mesh& mesh, const SimplifyOptions& opts = {});
    [[nodiscard]] static Mesh decimateToTarget(const Mesh& mesh, uint32_t targetFaces);
    [[nodiscard]] static Mesh decimateByRatio(const Mesh& mesh, float ratio);
    [[nodiscard]] static SimplifyStats getStats(const Mesh& original, const Mesh& simplified) noexcept;
};

} // namespace nexus::geometry
