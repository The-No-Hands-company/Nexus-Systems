#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Mesh Import/Export (hard-surface blockout workflow)
//
//  MeshIO::exportMesh() serialises a Mesh to disk in OBJ / PLY (ASCII) or STL
//  (binary) or glTF 2.0 (.glb) format. MeshIO::importMesh() reads a Mesh back
//  from OBJ, STL, PLY, or glTF (.gltf/.glb), enabling import -> edit -> export
//  round-trips. Formats and binary/ASCII variants are auto-detected.
//
//  Design constraints:
//  - All paths are noexcept and headless-safe.
//  - Output is deterministic for identical input.
//  - Import rejects non-finite data (mirrors the export hardening).
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
    OBJ,   // Wavefront OBJ (ASCII)
    PLY,   // Stanford PLY  (ASCII)
    STL,   // Stereolithography (binary)
    GLTF,  // glTF 2.0 (binary .glb container, self-contained)
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
//  Import format
// ─────────────────────────────────────────────────────────────────────────────
enum class MeshImportFormat : uint8_t {
    Auto,  // detect from file extension
    OBJ,   // Wavefront OBJ (ASCII)
    STL,   // Stereolithography (binary or ASCII, auto-detected)
    PLY,   // Stanford PLY (ASCII or binary little-endian, auto-detected)
    GLTF,  // glTF 2.0 (.gltf JSON w/ data-URI or external buffers, or .glb binary)
};

enum class MeshImportDiagnostic : uint32_t {
    Success           = 0,
    FileOpenFailed    = 1u << 0,
    ParseError        = 1u << 1,
    EmptyMesh         = 1u << 2,
    UnsupportedFormat = 1u << 3,
    NonFiniteData     = 1u << 4,
};

inline MeshImportDiagnostic operator|(MeshImportDiagnostic a,
                                      MeshImportDiagnostic b) noexcept
{
    return static_cast<MeshImportDiagnostic>(static_cast<uint32_t>(a)
                                           | static_cast<uint32_t>(b));
}

inline bool hasDiagnostic(MeshImportDiagnostic val,
                          MeshImportDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

struct MeshImportOptions {
    MeshImportFormat format                 = MeshImportFormat::Auto;
    bool             computeNormalsIfMissing = true;   // derive vertex normals when the file has none
    bool             weldVertices            = false;  // merge coincident verts (useful for STL soup)
    float            weldEpsilon             = 1e-5f;
};

struct MeshImportReport {
    MeshImportDiagnostic diagnostic  = MeshImportDiagnostic::Success;
    bool                 valid       = false;
    uint32_t             verticesRead = 0;
    uint32_t             facesRead    = 0;
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == MeshImportDiagnostic::Success;
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

    // Imports a mesh from the file at 'path' into 'outMesh'.
    // Format is taken from options.format, or detected from the extension when Auto.
    // Returns a report; report.valid == true on success. 'outMesh' is left cleared on failure.
    [[nodiscard]] static MeshImportReport importMesh(const std::string&       path,
                                                     const MeshImportOptions& options,
                                                     Mesh&                    outMesh) noexcept;
};

} // namespace nexus::geometry
