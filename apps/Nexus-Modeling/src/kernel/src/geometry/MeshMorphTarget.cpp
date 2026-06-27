#include <nexus/geometry/MeshMorphTarget.h>

#include <cmath>
#include <vector>
#include <algorithm>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh MeshMorphTarget::blend(const Mesh& base,
                              const std::vector<MorphTarget>& targets,
                              const std::vector<float>& weights,
                              const BlendOptions& opts) {
    Mesh result = base;
    if (!result.isValid()) return result;

    const auto& pos = base.attributes().positions();
    size_t V = pos.size();
    if (V == 0 || targets.empty() || weights.empty()) return result;

    if (targets.size() != weights.size()) return result;

    for (size_t t = 0; t < targets.size(); ++t) {
        if (targets[t].deltas.size() != V) return result;
    }

    std::vector<Vec3> blended = pos;
    for (size_t v = 0; v < V; ++v) {
        Vec3 delta{};
        for (size_t t = 0; t < targets.size(); ++t) {
            float w = weights[t];
            if (std::fabs(w) < 1e-10f) continue;
            delta = Vec3{delta.x + targets[t].deltas[v].x * w,
                         delta.y + targets[t].deltas[v].y * w,
                         delta.z + targets[t].deltas[v].z * w};
        }
        blended[v] = Vec3{blended[v].x + delta.x, blended[v].y + delta.y, blended[v].z + delta.z};
    }

    result.attributes().setPositions(std::move(blended));

    if (opts.recomputeNormals) {
        (void)result.computeVertexNormals();
    }

    return result;
}

Mesh MeshMorphTarget::blendMasked(const Mesh& base,
                                   const std::vector<MorphTarget>& targets,
                                   const std::vector<float>& weights,
                                   const std::vector<float>& vertexMask,
                                   const BlendOptions& opts) {
    Mesh result = base;
    if (!result.isValid()) return result;

    const auto& pos = base.attributes().positions();
    size_t V = pos.size();
    if (V == 0 || targets.empty() || weights.empty()) return result;
    if (targets.size() != weights.size()) return result;
    if (vertexMask.size() != V && !vertexMask.empty()) return result;

    for (size_t t = 0; t < targets.size(); ++t) {
        if (targets[t].deltas.size() != V) return result;
    }

    std::vector<Vec3> blended = pos;
    for (size_t v = 0; v < V; ++v) {
        float maskVal = vertexMask.empty() ? 1.0f : std::clamp(vertexMask[v], 0.0f, 1.0f);
        if (maskVal < 1e-10f) continue;

        Vec3 delta{};
        for (size_t t = 0; t < targets.size(); ++t) {
            float w = weights[t] * maskVal;
            if (std::fabs(w) < 1e-10f) continue;
            delta = Vec3{delta.x + targets[t].deltas[v].x * w,
                         delta.y + targets[t].deltas[v].y * w,
                         delta.z + targets[t].deltas[v].z * w};
        }
        blended[v] = Vec3{blended[v].x + delta.x, blended[v].y + delta.y, blended[v].z + delta.z};
    }

    result.attributes().setPositions(std::move(blended));

    if (opts.recomputeNormals) {
        (void)result.computeVertexNormals();
    }

    return result;
}

Mesh MeshMorphTarget::blendInBetween(const Mesh& base,
                                       const std::vector<MorphTarget>& targets,
                                       float t) {
    if (targets.size() < 2) return base;
    if (!base.isValid()) return base;

    size_t V = base.attributes().positions().size();
    if (V == 0) return base;

    std::vector<float> weights(targets.size(), 0.0f);
    float totalSegments = static_cast<float>(targets.size() - 1);
    float rawIdx = t * totalSegments;
    size_t idxA = static_cast<size_t>(std::floor(rawIdx));
    size_t idxB = std::min(idxA + 1, targets.size() - 1);
    float localT = rawIdx - static_cast<float>(idxA);

    weights[idxA] = 1.0f - localT;
    weights[idxB] = localT;

    return blend(base, targets, weights, BlendOptions{});
}

Mesh MeshMorphTarget::createMorphTarget(const Mesh& base, const Mesh& target, const std::string& name) {
    const auto& bp = base.attributes().positions();
    const auto& tp = target.attributes().positions();
    size_t V = std::min(bp.size(), tp.size());

    std::vector<MorphTarget> targets(1);
    targets[0].name = name;
    targets[0].deltas.resize(V);

    for (size_t i = 0; i < V; ++i) {
        targets[0].deltas[i] = Vec3{tp[i].x - bp[i].x, tp[i].y - bp[i].y, tp[i].z - bp[i].z};
    }

    return base;
}

} // namespace nexus::geometry
