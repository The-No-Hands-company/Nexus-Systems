#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Boolean Operations
//
//  Provides deterministic boolean operations (union, difference, intersection)
//  on triangle meshes for hard-surface modeling workflows.
//
//  v0 design principles:
//  - Prioritize correctness and determinism over performance
//  - Robustly handle edge cases and invalid inputs
//  - Provide comprehensive failure diagnostics
//  - Preserve mesh attributes (normals, UVs) where semantically meaningful
//  - Support simple convex geometry in initial release
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/Mesh.h>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

// ─────────────────────────────────────────────────────────────────────────────
//  BooleanOperationType
// ─────────────────────────────────────────────────────────────────────────────
enum class BooleanOperationType : uint8_t {
    Union,       // A ∪ B: material inside A or B
    Difference,  // A - B: material inside A but outside B
    Intersection // A ∩ B: material inside both A and B
};

// ─────────────────────────────────────────────────────────────────────────────
//  BooleanOperationDiagnostic
//
//  Explains why a boolean operation failed or succeeded with warnings.
// ─────────────────────────────────────────────────────────────────────────────
enum class BooleanOperationDiagnostic : uint32_t {
    // Success codes
    Success                    = 0,
    SuccessWithWarnings        = 1u << 31,

    // Validation codes
    InputAInvalid              = 1 << 0,  // Input A topology/geometry invalid
    InputBInvalid              = 1 << 1,  // Input B topology/geometry invalid
    InputAEmpty                = 1 << 2,  // Input A has no faces
    InputBEmpty                = 1 << 3,  // Input B has no faces
    InputANotManifold          = 1 << 4,  // Input A is non-manifold
    InputBNotManifold          = 1 << 5,  // Input B is non-manifold
    InputAHasNonTriangles      = 1 << 6,  // Input A contains non-triangle faces
    InputBHasNonTriangles      = 1 << 7,  // Input B contains non-triangle faces

    // Computational codes
    GeometricDegeneracy        = 1 << 8,  // Degenerate geometry (coplanar, zero-volume)
    NoIntersection             = 1 << 9,  // Meshes don't intersect for difference/intersection
    NumericalInstability       = 1 << 10, // Floating-point precision issues detected
    AlgorithmFailed            = 1 << 11, // Algorithm could not complete

    // Output codes
    OutputEmpty                = 1 << 12, // Result has no faces (expected for some ops)
    OutputNonManifold          = 1 << 13, // Result topology is non-manifold
    OutputAttributeLoss        = 1 << 14  // Some mesh attributes were not preserved
};

inline BooleanOperationDiagnostic operator|(BooleanOperationDiagnostic a,
                                            BooleanOperationDiagnostic b) noexcept {
    return static_cast<BooleanOperationDiagnostic>(static_cast<uint32_t>(a) |
                                                    static_cast<uint32_t>(b));
}

inline BooleanOperationDiagnostic operator&(BooleanOperationDiagnostic a,
                                            BooleanOperationDiagnostic b) noexcept {
    return static_cast<BooleanOperationDiagnostic>(static_cast<uint32_t>(a) &
                                                    static_cast<uint32_t>(b));
}

inline bool hasCode(BooleanOperationDiagnostic diag, BooleanOperationDiagnostic code) noexcept {
    return (static_cast<uint32_t>(diag) & static_cast<uint32_t>(code)) != 0;
}

struct BooleanOperationReport {
    BooleanOperationDiagnostic code = BooleanOperationDiagnostic::Success;
    bool valid = false;  // true if operation succeeded and output is usable
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept {
        return code == BooleanOperationDiagnostic::Success ||
               code == BooleanOperationDiagnostic::SuccessWithWarnings;
    }

    [[nodiscard]] bool hasWarning(BooleanOperationDiagnostic warning) const noexcept {
        return hasCode(code, warning);
    }

    void addMessage(const std::string& msg) { messages.push_back(msg); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  BooleanOperationOptions
// ─────────────────────────────────────────────────────────────────────────────
struct BooleanOperationOptions {
    // Tolerance for geometric operations (in absolute units).
    // Used for point-in-volume and coplanarity tests.
    float geometricTolerance = 1e-5f;

    // When true, attempt to repair invalid output topology.
    bool attemptRepair = true;

    // When true, preserve normal vectors via interpolation/reconstruction.
    // If false, normals are cleared and must be rebuilt.
    bool preserveNormals = true;

    // When true, preserve UV coordinates via interpolation.
    // If false, UVs are cleared.
    bool preserveUVs = false;

    // Maximum number of repair iterations before giving up.
    uint32_t maxRepairIterations = 3;

    // When true, validate input meshes before operation.
    bool validateInputs = true;
};

// ─────────────────────────────────────────────────────────────────────────────
//  BooleanOperation — deterministic boolean operations on triangle meshes
// ─────────────────────────────────────────────────────────────────────────────
class BooleanOperation {
public:
    // Performs a boolean operation on two meshes.
    // Inputs are assumed to be closed, oriented triangle meshes.
    // Output topology is always triangle-only.
    //
    // Preconditions:
    //   - meshA and meshB are valid Mesh objects with positions and faces
    //   - operation is a valid BooleanOperationType
    //   - options are reasonable (geometricTolerance > 0, etc.)
    //
    // Postconditions:
    //   - report.valid == true means output can be used in rendering/export
    //   - report.valid == false means output should be discarded or repaired externally
    //   - output.positions() is guaranteed non-empty if report.valid == true
    [[nodiscard]] static BooleanOperationReport compute(
        const Mesh& meshA,
        const Mesh& meshB,
        BooleanOperationType operation,
        const BooleanOperationOptions& options,
        Mesh& output) noexcept;

    // Convenience overload with default options.
    [[nodiscard]] static BooleanOperationReport compute(
        const Mesh& meshA,
        const Mesh& meshB,
        BooleanOperationType operation,
        Mesh& output) noexcept {
        return compute(meshA, meshB, operation, BooleanOperationOptions{}, output);
    }

    // Validates that a mesh is a candidate for boolean operations.
    // Returns detailed diagnostic information.
    [[nodiscard]] static BooleanOperationReport validateMesh(const Mesh& mesh) noexcept;

    // Helper: converts BooleanOperationType to readable string for logging.
    [[nodiscard]] static const char* operationName(BooleanOperationType op) noexcept;
};

}  // namespace nexus::geometry
