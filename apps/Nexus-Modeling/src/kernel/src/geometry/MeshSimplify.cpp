#include <nexus/geometry/MeshSimplify.h>
#include <nexus/geometry/MeshDecimator.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Tolerance.h>  // geometry::isFinite (non-finite rejection convention)

#include <algorithm>
#include <cmath>

namespace nexus::geometry {

Mesh MeshSimplify::decimate(const Mesh& mesh, const SimplifyOptions& opts) {
    if (!mesh.isValid()) return mesh;
    for (const auto& p : mesh.attributes().positions())
        if (!isFinite(p)) return Mesh{};  // reject non-finite input (convention)

    const size_t currentFaces = mesh.topology().faceCount();
    if (currentFaces == 0) return mesh;
    if (currentFaces <= opts.targetFaceCount) return mesh;

    auto hemOpt = HalfEdgeMesh::fromMesh(mesh);
    if (!hemOpt) return mesh;

    DecimationOptions decOpts;
    decOpts.targetFaceCount = opts.targetFaceCount;

    auto result = MeshDecimator::decimate(*hemOpt, decOpts);
    if (!result) return mesh;

    return result->first.toMesh();
}

Mesh MeshSimplify::decimateToTarget(const Mesh& mesh, uint32_t targetFaces) {
    if (!mesh.isValid()) return mesh;
    for (const auto& p : mesh.attributes().positions())
        if (!isFinite(p)) return Mesh{};  // reject non-finite input (convention)

    size_t currentFaces = mesh.topology().faceCount();
    if (currentFaces <= targetFaces) return mesh;

    Mesh current = mesh;

    while (current.topology().faceCount() > targetFaces) {
        size_t remaining = current.topology().faceCount();
        size_t target = std::max(static_cast<size_t>(targetFaces), remaining / 2);

        auto hemOpt = HalfEdgeMesh::fromMesh(current);
        if (!hemOpt) break;

        DecimationOptions decOpts;
        decOpts.targetFaceCount = target;

        auto result = MeshDecimator::decimate(*hemOpt, decOpts);
        if (!result) break;

        current = result->first.toMesh();

        if (current.topology().faceCount() >= remaining) break;
    }

    return current;
}

Mesh MeshSimplify::decimateByRatio(const Mesh& mesh, float ratio) {
    if (!mesh.isValid() || ratio <= 0.0f || ratio >= 1.0f) return mesh;

    uint32_t targetFaces = static_cast<uint32_t>(
        static_cast<float>(mesh.topology().faceCount()) * ratio);
    if (targetFaces < 1) targetFaces = 1;

    return decimateToTarget(mesh, targetFaces);
}

MeshSimplify::SimplifyStats MeshSimplify::getStats(const Mesh& original, const Mesh& simplified) noexcept {
    SimplifyStats stats;
    stats.originalFaces = static_cast<uint32_t>(original.topology().faceCount());
    stats.originalVertices = static_cast<uint32_t>(original.attributes().positions().size());
    stats.simplifiedFaces = static_cast<uint32_t>(simplified.topology().faceCount());
    stats.simplifiedVertices = static_cast<uint32_t>(simplified.attributes().positions().size());

    if (stats.originalFaces > 0) {
        stats.faceReductionPercent = (1.0f - static_cast<float>(stats.simplifiedFaces) /
                                             static_cast<float>(stats.originalFaces)) * 100.0f;
    }
    if (stats.originalVertices > 0) {
        stats.vertexReductionPercent = (1.0f - static_cast<float>(stats.simplifiedVertices) /
                                                static_cast<float>(stats.originalVertices)) * 100.0f;
    }

    return stats;
}

} // namespace nexus::geometry
