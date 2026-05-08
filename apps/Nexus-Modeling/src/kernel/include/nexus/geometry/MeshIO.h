#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Mesh Import/Export (hard-surface blockout workflow)
//
//  MeshIO::exportMesh() serialises a Mesh to disk in OBJ or PLY (ASCII) format.
//  These two formats cover the primary DCC interchange requirements for the
//  hard-surface blockout pipeline slice.
//
//  Design constraints:
//  - All paths are noexcept and headless-safe.
//  - Output is deterministic for identical input.
//  - Import (read) is intentionally out of scope for this v0 endpoint.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

// ─────────────────────────────────────────────────────────────────────────────
//  Export format
// ─────────────────────────────────────────────────────────────────────────────
enum class MeshExportFormat : uint8_t {
    OBJ,  // Wavefront OBJ (ASCII)
    PLY,  // Stanford PLY  (ASCII)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostic flags
// ─────────────────────────────────────────────────────────────────────────────
enum class MeshExportDiagnostic : uint32_t {
    Success           = 0,
    InvalidMesh       = 1u << 0,
    FileOpenFailed    = 1u << 1,
    WriteError        = 1u << 2,
    UnsupportedFormat = 1u << 3,
};

inline MeshExportDiagnostic operator|(MeshExportDiagnostic a,
                                      MeshExportDiagnostic b) noexcept
{
    return static_cast<MeshExportDiagnostic>(static_cast<uint32_t>(a)
                                           | static_cast<uint32_t>(b));
}

inline bool hasDiagnostic(MeshExportDiagnostic val,
                          MeshExportDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Options
// ─────────────────────────────────────────────────────────────────────────────
struct MeshExportOptions {
    MeshExportFormat format        = MeshExportFormat::OBJ;
    bool             includeNormals = true;
    bool             includeUVs     = true;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Export report
// ─────────────────────────────────────────────────────────────────────────────
struct MeshExportReport {
    MeshExportDiagnostic diagnostic     = MeshExportDiagnostic::Success;
    bool                 valid          = false;
    uint32_t             verticesWritten = 0;
    uint32_t             facesWritten    = 0;
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == MeshExportDiagnostic::Success;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  MeshIO — stateless serialiser
// ─────────────────────────────────────────────────────────────────────────────
class MeshIO {
public:
    // Exports 'mesh' to the file at 'path'.
    // Returns a report; report.valid == true on success.
    [[nodiscard]] static MeshExportReport exportMesh(const Mesh&              mesh,
                                                     const std::string&       path,
                                                     const MeshExportOptions& options) noexcept;
};

} // namespace nexus::geometry
