#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Bevel/Chamfer Operations
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

enum class BevelChamferMode : uint8_t {
    Bevel,
    Chamfer,
};

enum class BevelChamferDiagnostic : uint32_t {
    Success                 = 0,
    SuccessWithWarnings     = 1u << 31,

    InvalidInputMesh        = 1u << 0,
    InvalidDistance         = 1u << 1,
    InvalidSharpAngle       = 1u << 2,
    NoSharpEdgesDetected    = 1u << 3,
    NonManifoldTopology     = 1u << 4,
    NormalRebuildFailed     = 1u << 5,
    OutputNonManifold       = 1u << 6,
    GeneratedDegenerateFace = 1u << 7,
};

inline BevelChamferDiagnostic operator|(BevelChamferDiagnostic a,
                                        BevelChamferDiagnostic b) noexcept
{
    return static_cast<BevelChamferDiagnostic>(static_cast<uint32_t>(a)
                                             | static_cast<uint32_t>(b));
}

inline BevelChamferDiagnostic operator&(BevelChamferDiagnostic a,
                                        BevelChamferDiagnostic b) noexcept
{
    return static_cast<BevelChamferDiagnostic>(static_cast<uint32_t>(a)
                                             & static_cast<uint32_t>(b));
}

inline bool hasDiagnostic(BevelChamferDiagnostic val,
                          BevelChamferDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

struct BevelChamferDesc {
    BevelChamferMode mode = BevelChamferMode::Bevel;
    float distance = 0.025f;
    float sharpAngleDegrees = 30.0f;
    bool includeBoundaryEdges = true;
    bool recomputeNormals = true;
};

struct BevelChamferReport {
    BevelChamferDiagnostic diagnostic = BevelChamferDiagnostic::Success;
    bool valid = false;
    uint32_t sharpEdgeCount = 0;
    uint32_t splitEdgeCount = 0;
    uint32_t supportFaceCount = 0;
    uint32_t movedVertexCount = 0;
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == BevelChamferDiagnostic::Success
            || diagnostic == BevelChamferDiagnostic::SuccessWithWarnings;
    }
};

class BevelChamferOperation {
public:
    [[nodiscard]] static BevelChamferReport apply(const Mesh& input,
                                                  const BevelChamferDesc& desc,
                                                  Mesh& output) noexcept;
};

} // namespace nexus::geometry
