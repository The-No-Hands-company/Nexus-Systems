#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Direct Modeling (push/pull faces)
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HalfEdgeMesh.h>

namespace nexus::geometry {

struct DirectModelingReport {
    bool     valid = false;
    uint32_t facesMoved = 0;
    uint32_t verticesAdded = 0;
};

class DirectModeling {
public:
    // Push selected faces outward along their normals by a given distance.
    // Retriangulates the surrounding faces to accommodate the offset.
    [[nodiscard]] static DirectModelingReport pushFaces(
        HalfEdgeMesh& mesh,
        const std::vector<uint32_t>& faceIndices,
        float distance) noexcept;

    [[nodiscard]] static DirectModelingReport pushFacesPerFace(
        HalfEdgeMesh& mesh,
        const std::vector<uint32_t>& faceIndices,
        float distance) noexcept;

    [[nodiscard]] static DirectModelingReport pushFacesWithDraft(
        HalfEdgeMesh& mesh,
        const std::vector<uint32_t>& faceIndices,
        float distance,
        float draftAngleDeg) noexcept;

    [[nodiscard]] static DirectModelingReport pullFaces(
        HalfEdgeMesh& mesh,
        const std::vector<uint32_t>& faceIndices,
        float distance) noexcept;
};

} // namespace nexus::geometry
