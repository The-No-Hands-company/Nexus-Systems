#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  DEPRECATED — use BooleanOperation instead
//
//  MeshBoolean is a legacy wrapper over BooleanOperation that provides
//  a simpler call signature.  New code should prefer BooleanOperation
//  directly for richer diagnostics and configuration control.
//
//  This header will be removed in a future release.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

enum class BooleanOp : uint8_t {
    Union,
    Difference,
    Intersection,
};

struct MeshBooleanResult {
    bool ok = false;
    std::string error;
    Mesh result;
};

class MeshBoolean {
public:
    [[nodiscard]] static MeshBooleanResult compute(const Mesh& a, const Mesh& b, BooleanOp op);
    [[nodiscard]] static MeshBooleanResult computeMultiple(const std::vector<Mesh>& meshes, BooleanOp op);
    [[nodiscard]] static MeshBooleanResult computeWithOptions(const Mesh& a, const Mesh& b, BooleanOp op, bool preserveAttributes);
    [[nodiscard]] static bool canCompute(const Mesh& a, const Mesh& b) noexcept;
};

} // namespace nexus::geometry
