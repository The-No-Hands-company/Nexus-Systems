#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Extrude Operation v0
//
//  Extrudes every face of an input mesh outward along its face normal by a
//  given distance.  The operation is per-face independent (each face extrudes
//  in isolation), producing:
//    • An extruded cap face at (original position + normal * distance).
//    • Four (or n) wall faces connecting the original edge ring to the cap ring.
//
//  keepOriginalFaces controls whether the original faces are retained as base
//  caps (useful for solid-shell results) or removed (open-base extrusion).
//
//  All paths are noexcept, deterministic, and headless-safe.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostic flags
// ─────────────────────────────────────────────────────────────────────────────
enum class ExtrudeDiagnostic : uint32_t {
    Success                 = 0,
    SuccessWithWarnings     = 1u << 31,

    InvalidInputMesh        = 1u << 0,
    InvalidDistance         = 1u << 1,
    NoFacesExtruded         = 1u << 2,
    NormalRebuildFailed     = 1u << 3,
    OutputTopologyInvalid   = 1u << 4,
};

inline ExtrudeDiagnostic operator|(ExtrudeDiagnostic a, ExtrudeDiagnostic b) noexcept
{
    return static_cast<ExtrudeDiagnostic>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool hasDiagnostic(ExtrudeDiagnostic val, ExtrudeDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Options
// ─────────────────────────────────────────────────────────────────────────────
struct ExtrudeDesc {
    // Distance to extrude each face along its face normal.
    float distance = 0.1f;
    // When true, original faces are retained as base caps (solid shell result).
    // When false, original faces are removed (open extrusion).
    bool keepOriginalFaces = false;
    // Recompute per-vertex normals on the output after extrusion.
    bool recomputeNormals = true;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Report
// ─────────────────────────────────────────────────────────────────────────────
struct ExtrudeReport {
    ExtrudeDiagnostic diagnostic        = ExtrudeDiagnostic::Success;
    bool              valid             = false;
    uint32_t          extrudedFaceCount = 0;  // number of original faces processed
    uint32_t          addedFaceCount    = 0;  // total new faces added (caps + walls)
    uint32_t          addedVertexCount  = 0;  // total new vertices added
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == ExtrudeDiagnostic::Success
            || diagnostic == ExtrudeDiagnostic::SuccessWithWarnings;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ExtrudeOperation — stateless extrusion operator
// ─────────────────────────────────────────────────────────────────────────────
class ExtrudeOperation {
public:
    // Extrudes all faces of 'input'.
    // 'output' receives the result; it is safe to pass the same mesh as input
    // and output (they are not aliased; input is read before output is written).
    [[nodiscard]] static ExtrudeReport applyToAllFaces(const Mesh&        input,
                                                       const ExtrudeDesc& desc,
                                                       Mesh&              output) noexcept;
};

} // namespace nexus::geometry
