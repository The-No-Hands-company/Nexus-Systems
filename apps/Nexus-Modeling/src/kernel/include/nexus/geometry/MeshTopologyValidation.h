#pragma once
// ─── Nexus Geometry ── MeshTopologyValidation ─────────────────────────────
//  Non-manifold prevention: Euler-Poincaré formula, manifold gates,
//  genus computation, boundary verification.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/HalfEdgeMesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

struct TopologyViolation {
    enum Kind {
        NonManifoldEdge,
        NonManifoldVertex,
        NonClosedVolume,
        InconsistentOrientation,
        DegenerateFace,
        ZeroLengthEdge,
        DuplicateVertices,
        SelfIntersection,
    } kind;
    std::string detail;
    uint32_t elementIndex = 0xFFFFFFFF;
};

struct TopologyValidityResult {
    bool valid = true;
    int euler = 0;
    int genus = 0;
    uint32_t boundaryLoops = 0;
    std::vector<TopologyViolation> violations;
};

class MeshTopologyValidation {
public:
    using Vec3 = nexus::render::Vec3;

    [[nodiscard]] static TopologyValidityResult validate(const Mesh& mesh);

    [[nodiscard]] static TopologyValidityResult validateHEM(const HalfEdgeMesh& hem);

    [[nodiscard]] static bool wouldCreateNonManifoldEdge(
        const HalfEdgeMesh& hem, uint32_t src, uint32_t dst);

    [[nodiscard]] static bool wouldCreateNonManifoldVertex(
        const HalfEdgeMesh& hem, uint32_t vertex);

    [[nodiscard]] static int computeEulerCharacteristic(
        uint32_t V, uint32_t E, uint32_t F) noexcept;

    [[nodiscard]] static int computeGenus(
        int eulerCharacteristic, uint32_t boundaryLoops) noexcept;
};

} // namespace nexus::geometry
