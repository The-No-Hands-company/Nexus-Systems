#include <nexus/geometry/MeshBoolean.h>
#include <nexus/geometry/BooleanOperation.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

static BooleanOperationType toBooleanOperationType(BooleanOp op) noexcept {
    switch (op) {
        case BooleanOp::Union:        return BooleanOperationType::Union;
        case BooleanOp::Difference:    return BooleanOperationType::Difference;
        case BooleanOp::Intersection:  return BooleanOperationType::Intersection;
    }
    return BooleanOperationType::Union;
}

MeshBooleanResult MeshBoolean::compute(const Mesh& a, const Mesh& b, BooleanOp op) {
    BooleanOperationOptions opts;
    Mesh result;
    auto report = BooleanOperation::compute(a, b, toBooleanOperationType(op), opts, result);
    if (report.valid) return {true, "", std::move(result)};
    return {false, "Boolean operation failed", {}};
}

MeshBooleanResult MeshBoolean::computeMultiple(const std::vector<Mesh>& meshes, BooleanOp op) {
    if (meshes.empty()) return {false, "No meshes provided", {}};
    if (meshes.size() == 1) return {true, "", meshes[0]};

    MeshBooleanResult current = compute(meshes[0], meshes[1], op);
    if (!current.ok) return current;

    for (size_t i = 2; i < meshes.size(); ++i) {
        current = compute(current.result, meshes[i], op);
        if (!current.ok) return current;
    }

    return current;
}

MeshBooleanResult MeshBoolean::computeWithOptions(const Mesh& a, const Mesh& b, BooleanOp op,
                                                    bool preserveAttributes) {
    BooleanOperationOptions opts;
    opts.preserveUVs = preserveAttributes;
    opts.preserveNormals = preserveAttributes;

    Mesh result;
    auto report = BooleanOperation::compute(a, b, toBooleanOperationType(op), opts, result);
    if (report.valid) return {true, "", std::move(result)};
    return {false, "Boolean operation failed", {}};
}

bool MeshBoolean::canCompute(const Mesh& a, const Mesh& b) noexcept {
    if (!a.isValid() || !b.isValid()) return false;

    auto boundsA = a.computeBounds();
    auto boundsB = b.computeBounds();

    float dx = std::max(boundsA.min.x, boundsB.min.x) - std::min(boundsA.max.x, boundsB.max.x);
    float dy = std::max(boundsA.min.y, boundsB.min.y) - std::min(boundsA.max.y, boundsB.max.y);
    float dz = std::max(boundsA.min.z, boundsB.min.z) - std::min(boundsA.max.z, boundsB.max.z);

    bool overlaps = (dx <= 0) && (dy <= 0) && (dz <= 0);
    return a.topology().faceCount() > 0 && b.topology().faceCount() > 0 && overlaps;
}

} // namespace nexus::geometry
