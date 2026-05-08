#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Remesh Operation v0
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

enum class RemeshDiagnostic : uint32_t {
    Success                   = 0,
    SuccessWithWarnings       = 1u << 31,

    InvalidInputMesh          = 1u << 0,
    InvalidTargetEdgeLength   = 1u << 1,
    InvalidIterationCount     = 1u << 2,
    InputTriangulated         = 1u << 3,
    NoChangesApplied          = 1u << 4,
    OutputTopologyInvalid     = 1u << 5,
    NormalRebuildFailed       = 1u << 6,
};

inline RemeshDiagnostic operator|(RemeshDiagnostic a, RemeshDiagnostic b) noexcept
{
    return static_cast<RemeshDiagnostic>(static_cast<uint32_t>(a)
                                       | static_cast<uint32_t>(b));
}

inline bool hasDiagnostic(RemeshDiagnostic val, RemeshDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

struct RemeshDesc {
    float targetEdgeLength = 0.25f;
    uint32_t maxIterations = 2;
    float splitThresholdMultiplier = 1.25f;
    bool recomputeNormals = true;
    // Edge-collapse pass: edges shorter than (collapseEdgesBelow * targetEdgeLength)
    // are collapsed. Set to 0 (default) to disable.
    float collapseEdgesBelow = 0.f;
    uint32_t maxCollapseIterations = 2;
};

struct RemeshReport {
    RemeshDiagnostic diagnostic = RemeshDiagnostic::Success;
    bool valid = false;
    uint32_t iterationsRan = 0;
    uint32_t splitCount = 0;
    uint32_t collapseCount = 0;
    uint32_t inputFaceCount = 0;
    uint32_t outputFaceCount = 0;
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == RemeshDiagnostic::Success
            || diagnostic == RemeshDiagnostic::SuccessWithWarnings;
    }
};

class RemeshOperation {
public:
    [[nodiscard]] static RemeshReport apply(const Mesh& input,
                                            const RemeshDesc& desc,
                                            Mesh& output) noexcept;
};

} // namespace nexus::geometry
