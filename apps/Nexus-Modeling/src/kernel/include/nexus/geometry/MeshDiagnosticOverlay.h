#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Mesh Diagnostic Overlay Data Contract (UI consumption)
//
//  MeshDiagnosticExporter::extract() produces per-element typed annotations
//  suitable for rendering debug overlays in a viewport:
//    • Non-manifold edges (> 2 adjacent faces)
//    • Boundary / open edges (exactly 1 adjacent face)
//    • Degenerate faces (near-zero area)
//    • Isolated vertices (not referenced by any face)
//
//  The contract is read-only with respect to the source mesh.
//  All extraction paths are noexcept and headless-safe.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

// ─────────────────────────────────────────────────────────────────────────────
//  Overlay mode bitmask — controls which categories are extracted.
// ─────────────────────────────────────────────────────────────────────────────
enum class MeshOverlayMode : uint32_t {
    None             = 0,
    NonManifoldEdges = 1u << 0,
    BoundaryEdges    = 1u << 1,
    DegenerateFaces  = 1u << 2,
    IsolatedVertices = 1u << 3,
    All              = NonManifoldEdges | BoundaryEdges | DegenerateFaces | IsolatedVertices,
};

inline MeshOverlayMode operator|(MeshOverlayMode a, MeshOverlayMode b) noexcept
{
    return static_cast<MeshOverlayMode>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline MeshOverlayMode operator&(MeshOverlayMode a, MeshOverlayMode b) noexcept
{
    return static_cast<MeshOverlayMode>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasMode(MeshOverlayMode val, MeshOverlayMode flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Per-element annotation types
// ─────────────────────────────────────────────────────────────────────────────
enum class OverlayEdgeKind : uint8_t {
    NonManifold,
    Boundary,
};

enum class OverlayFaceKind : uint8_t {
    Degenerate,
};

enum class OverlayVertexKind : uint8_t {
    Isolated,
};

struct OverlayEdgeEntry {
    uint32_t v0 = 0;
    uint32_t v1 = 0;
    OverlayEdgeKind kind = OverlayEdgeKind::Boundary;
};

struct OverlayFaceEntry {
    uint32_t faceIndex = 0;
    OverlayFaceKind kind = OverlayFaceKind::Degenerate;
};

struct OverlayVertexEntry {
    uint32_t vertexIndex = 0;
    OverlayVertexKind kind = OverlayVertexKind::Isolated;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Overlay data container — passed to extract(), populated in-place.
// ─────────────────────────────────────────────────────────────────────────────
struct MeshDiagnosticOverlayData {
    std::vector<OverlayEdgeEntry>   edges;
    std::vector<OverlayFaceEntry>   faces;
    std::vector<OverlayVertexEntry> vertices;

    uint32_t nonManifoldEdgeCount = 0;
    uint32_t boundaryEdgeCount    = 0;
    uint32_t degenerateFaceCount  = 0;
    uint32_t isolatedVertexCount  = 0;

    void clear() noexcept
    {
        edges.clear();
        faces.clear();
        vertices.clear();
        nonManifoldEdgeCount = 0;
        boundaryEdgeCount    = 0;
        degenerateFaceCount  = 0;
        isolatedVertexCount  = 0;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Extraction options
// ─────────────────────────────────────────────────────────────────────────────
struct MeshDiagnosticOverlayOptions {
    MeshOverlayMode modes = MeshOverlayMode::All;
    // Faces whose triangle area is below this threshold are flagged degenerate.
    float degenerateAreaThreshold = 1e-10f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  MeshDiagnosticExporter — stateless extractor
// ─────────────────────────────────────────────────────────────────────────────
class MeshDiagnosticExporter {
public:
    // Populates 'out' with typed overlay entries from 'mesh'.
    // Returns true on success. Returns false only if 'mesh' has no topology.
    // 'out' is cleared before population regardless of return value.
    [[nodiscard]] static bool extract(const Mesh&                          mesh,
                                       const MeshDiagnosticOverlayOptions&  options,
                                       MeshDiagnosticOverlayData&           out) noexcept;
};

} // namespace nexus::geometry
