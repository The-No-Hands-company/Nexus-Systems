#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Inset Faces Operation v0
//
//  Insets every face of an input mesh inward toward the face centroid by a
//  given factor or absolute distance.  The operation produces:
//    • An inner polygon (the inset face) at (lerp(centroid, vertex, 1 - factor)).
//    • A ring of n quad "border" faces connecting the original edge to the inner ring.
//
//  InsetMode controls whether 'amount' is interpreted as an absolute world-space
//  distance or as a fractional factor of the half-edge length.
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
enum class InsetDiagnostic : uint32_t {
    Success               = 0,
    SuccessWithWarnings   = 1u << 31,

    InvalidInputMesh      = 1u << 0,
    InvalidAmount         = 1u << 1,
    NoFacesInset          = 1u << 2,
    NormalRebuildFailed   = 1u << 3,
    OutputTopologyInvalid = 1u << 4,
};

inline InsetDiagnostic operator|(InsetDiagnostic a, InsetDiagnostic b) noexcept
{
    return static_cast<InsetDiagnostic>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool hasDiagnostic(InsetDiagnostic val, InsetDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  InsetMode
// ─────────────────────────────────────────────────────────────────────────────
enum class InsetMode : uint8_t {
    // 'amount' is a fraction [0, 1] of the distance from vertex to centroid.
    // 0 = no inset (inner ring at original vertices).
    // 1 = full collapse to centroid.
    Factor,
    // 'amount' is a world-space distance to offset each vertex toward the centroid.
    Distance,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Options
// ─────────────────────────────────────────────────────────────────────────────
struct InsetDesc {
    float     amount          = 0.1f;
    InsetMode mode            = InsetMode::Factor;
    // When true, original faces are replaced by the inset face + border ring.
    // When false, original faces are kept alongside the inset geometry.
    bool      replaceOriginal = true;
    bool      recomputeNormals = true;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Report
// ─────────────────────────────────────────────────────────────────────────────
struct InsetReport {
    InsetDiagnostic diagnostic       = InsetDiagnostic::Success;
    bool            valid            = false;
    uint32_t        insetFaceCount   = 0;  // original faces processed
    uint32_t        addedFaceCount   = 0;  // inner faces + border quads added
    uint32_t        addedVertexCount = 0;  // new inner ring vertices
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == InsetDiagnostic::Success
            || diagnostic == InsetDiagnostic::SuccessWithWarnings;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  InsetFacesOperation — stateless inset operator
// ─────────────────────────────────────────────────────────────────────────────
class InsetFacesOperation {
public:
    // Insets all faces of 'input'.
    // 'output' receives the result; input and output may safely alias.
    [[nodiscard]] static InsetReport applyToAllFaces(const Mesh&      input,
                                                     const InsetDesc& desc,
                                                     Mesh&            output) noexcept;
};

} // namespace nexus::geometry
