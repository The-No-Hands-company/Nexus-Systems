// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Boolean Operations Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/geometry/MeshBooleanRobust.h>
#include <nexus/geometry/RobustPredicates.h>
#include <nexus/render/Camera.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <set>

namespace nexus::geometry {

namespace {

using Vec3 = nexus::render::Vec3;
using EdgeKey = std::pair<uint32_t, uint32_t>;

// Geometry utilities for v0 boolean operations
struct TriangleFace {
    uint32_t indices[3];
    Vec3 normal;
};

struct BoundingBox {
    Vec3 min, max;

    static BoundingBox fromPositions(const std::vector<Vec3>& positions)
    {
        BoundingBox bb;
        if (positions.empty()) {
            bb.min = bb.max = Vec3{0, 0, 0};
            return bb;
        }
        bb.min = positions[0];
        bb.max = positions[0];
        for (const auto& p : positions) {
            bb.min.x = std::min(bb.min.x, p.x);
            bb.min.y = std::min(bb.min.y, p.y);
            bb.min.z = std::min(bb.min.z, p.z);
            bb.max.x = std::max(bb.max.x, p.x);
            bb.max.y = std::max(bb.max.y, p.y);
            bb.max.z = std::max(bb.max.z, p.z);
        }
        return bb;
    }

    [[nodiscard]] bool intersects(const BoundingBox& other, float tol = 1e-5f) const
    {
        return (max.x + tol >= other.min.x) && (min.x - tol <= other.max.x) &&
               (max.y + tol >= other.min.y) && (min.y - tol <= other.max.y) &&
               (max.z + tol >= other.min.z) && (min.z - tol <= other.max.z);
    }
};

// Explicit Vec3 arithmetic helpers to avoid ambiguity
inline Vec3 vec3Add(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 vec3Sub(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec3Scale(const Vec3& v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

inline float vec3Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 vec3Cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float vec3Length(const Vec3& v)
{
    return std::sqrt(vec3Dot(v, v));
}

inline bool isFiniteFloat(float value) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

inline Vec3 vec3Normalize(const Vec3& v)
{
    float len = vec3Length(v);
    if (len < 1e-6f) {
        return {0, 0, 0};
    }
    return vec3Scale(v, 1.0f / len);
}

inline bool isFiniteVec3(const Vec3& v)
{
    return isFiniteFloat(v.x) && isFiniteFloat(v.y) && isFiniteFloat(v.z);
}

// Scale-INVARIANT degeneracy test. `|cross|² = (2·area)²` scales as length⁴, so a
// FIXED absolute floor (the old 1e-16 on |cross|²) is scale-blind: it wrongly drops
// perfectly valid sub-millimetre triangles (edge ≲ 1e-4 → |cross|² < 1e-16) — which
// makes small-scale booleans fail with an empty result — and wrongly keeps genuine
// slivers on huge models. Instead compare the area to a `relEps`-fraction of the
// triangle's OWN largest edge²: |cross|² ≤ (relEps · maxEdgeLen²)². Equivalent to
// "the triangle's thin dimension (height = |cross|/|maxEdge|) is below a scale-
// relative linear tolerance relEps·|maxEdge|", so the decision is identical in
// proportion at any model scale. `relEps` is DIMENSIONLESS; 1e-8 reproduces the old
// unit-scale behaviour (|cross|² ≤ 1e-16 at maxEdgeLen² ≈ 1).
inline bool isDegenerateTriangle(const Vec3& p0, const Vec3& p1, const Vec3& p2, float relEps)
{
    const Vec3 e1 = vec3Sub(p1, p0);
    const Vec3 e2 = vec3Sub(p2, p0);
    const Vec3 e3 = vec3Sub(p2, p1);
    const Vec3 c = vec3Cross(e1, e2);
    const float maxLen2 = std::max({vec3Dot(e1, e1), vec3Dot(e2, e2), vec3Dot(e3, e3)});
    const float floor2 = relEps * maxLen2;  // relEps·maxEdge²  (units: length²)
    return vec3Dot(c, c) <= floor2 * floor2;  // |cross|² ≤ (relEps·maxEdge²)²
}

inline EdgeKey makeEdgeKey(uint32_t a, uint32_t b)
{
    if (a < b) {
        return {a, b};
    }
    return {b, a};
}

BooleanOperationDiagnostic mapValidationCodeToB(BooleanOperationDiagnostic code)
{
    uint32_t bits = static_cast<uint32_t>(code);
    const uint32_t successWarn = static_cast<uint32_t>(BooleanOperationDiagnostic::SuccessWithWarnings);
    const uint32_t aInvalid = static_cast<uint32_t>(BooleanOperationDiagnostic::InputAInvalid);
    const uint32_t aEmpty = static_cast<uint32_t>(BooleanOperationDiagnostic::InputAEmpty);
    const uint32_t aNotManifold = static_cast<uint32_t>(BooleanOperationDiagnostic::InputANotManifold);
    const uint32_t aNonTri = static_cast<uint32_t>(BooleanOperationDiagnostic::InputAHasNonTriangles);
    const uint32_t bInvalid = static_cast<uint32_t>(BooleanOperationDiagnostic::InputBInvalid);
    const uint32_t bEmpty = static_cast<uint32_t>(BooleanOperationDiagnostic::InputBEmpty);
    const uint32_t bNotManifold = static_cast<uint32_t>(BooleanOperationDiagnostic::InputBNotManifold);
    const uint32_t bNonTri = static_cast<uint32_t>(BooleanOperationDiagnostic::InputBHasNonTriangles);

    bits &= ~aInvalid;
    bits &= ~aEmpty;
    bits &= ~aNotManifold;
    bits &= ~aNonTri;

    if (hasCode(code, BooleanOperationDiagnostic::InputAInvalid)) {
        bits |= bInvalid;
    }
    if (hasCode(code, BooleanOperationDiagnostic::InputAEmpty)) {
        bits |= bEmpty;
    }
    if (hasCode(code, BooleanOperationDiagnostic::InputANotManifold)) {
        bits |= bNotManifold;
    }
    if (hasCode(code, BooleanOperationDiagnostic::InputAHasNonTriangles)) {
        bits |= bNonTri;
    }

    if ((bits & ~successWarn) == 0u && (bits & successWarn) != 0u) {
        return BooleanOperationDiagnostic::SuccessWithWarnings;
    }

    return static_cast<BooleanOperationDiagnostic>(bits);
}

// Compute face normal from triangle vertices
Vec3 computeFaceNormal(const Vec3& p0, const Vec3& p1, const Vec3& p2)
{
    Vec3 e1 = vec3Sub(p1, p0);
    Vec3 e2 = vec3Sub(p2, p0);
    return vec3Normalize(vec3Cross(e1, e2));
}

// Point-in-mesh test using ray casting

// Triangulate faces (convert quads/n-gons to triangles)
void triangulateInputMesh(const Mesh& input, std::vector<Vec3>& posOut,
                          std::vector<TriangleFace>& trisOut,
                          size_t& degenerateTrianglesSkipped)
{
    degenerateTrianglesSkipped = 0;
    const auto& inputAttrs = input.attributes();
    const auto& inputPos = inputAttrs.positions();
    posOut = std::vector<Vec3>(inputPos.begin(), inputPos.end());

    const auto& topo = input.topology();
    for (size_t faceIdx = 0; faceIdx < topo.faceCount(); ++faceIdx) {
        const auto& face = topo.face(faceIdx);
        if (face.indices.size() < 3) {
            continue;
        }
        if (!face.indicesInBounds(inputPos.size())) {
            continue;
        }

        // Triangulate n-gon using fan triangulation
        for (size_t i = 1; i + 1 < face.indices.size(); ++i) {
            TriangleFace tri;
            tri.indices[0] = face.indices[0];
            tri.indices[1] = face.indices[i];
            tri.indices[2] = face.indices[i + 1];

            if (isDegenerateTriangle(posOut[tri.indices[0]],
                                     posOut[tri.indices[1]],
                                     posOut[tri.indices[2]],
                                     1e-8f)) {  // dimensionless relative degeneracy
                ++degenerateTrianglesSkipped;
                continue;
            }

            tri.normal = computeFaceNormal(posOut[tri.indices[0]], posOut[tri.indices[1]],
                                           posOut[tri.indices[2]]);

            trisOut.push_back(tri);
        }
    }
}

// v0 algorithm: Filter based on point-in-mesh tests
// For union: keep triangles from A or B
// For difference: keep triangles from A that are outside B
// For intersection: keep triangles from A that are inside B
// ── Post-Boolean mesh cleanup ─────────────────────────────────────────
//  Removes degenerate triangles and merges coincident vertices.






// ── Triangle-triangle intersection line (Möller) ──────────────────────
//  Returns the intersection segment between two triangles.
//  Used to report where meshes intersect for visual feedback.

[[nodiscard]] inline bool computeIntersectionLine(
    const Vec3& t0a, const Vec3& t0b, const Vec3& t0c,
    const Vec3& t1a, const Vec3& t1b, const Vec3& t1c,
    Vec3& outA, Vec3& outB, float eps = 1e-6f) noexcept
{
    Vec3 n0 = vec3Cross(vec3Sub(t0b, t0a), vec3Sub(t0c, t0a));
    float nl0 = vec3Length(n0);
    if (nl0 < eps) return false;
    n0 = vec3Scale(n0, 1.0f / nl0);

    double d1a = RobustPredicates::orient3D(t0a, t0b, t0c, t1a);
    double d1b = RobustPredicates::orient3D(t0a, t0b, t0c, t1b);
    double d1c = RobustPredicates::orient3D(t0a, t0b, t0c, t1c);

    int above = (d1a > 0) + (d1b > 0) + (d1c > 0);
    int below = (d1a < 0) + (d1b < 0) + (d1c < 0);
    if (above == 3 || below == 3) return false;

    Vec3 edgePts[2];
    int found = 0;
    auto intersect = [&](const Vec3& va, const Vec3& vb, double da, double db) {
        if (found >= 2 || da * db >= 0) return;
        if (std::abs(db - da) < static_cast<double>(eps)) return;
        double t = -da / (db - da);
        edgePts[found++] = vec3Add(va, vec3Scale(vec3Sub(vb, va), static_cast<float>(std::clamp(t, 0.0, 1.0))));
    };
    intersect(t1a, t1b, d1a, d1b);
    intersect(t1b, t1c, d1b, d1c);
    intersect(t1c, t1a, d1c, d1a);

    if (found < 2) return false;

    auto pointInTri = [&](const Vec3& p) {
        Vec3 v0 = vec3Sub(t0b, t0a), v1 = vec3Sub(t0c, t0a), v2 = vec3Sub(p, t0a);
        float d00 = vec3Dot(v0, v0), d01 = vec3Dot(v0, v1), d11 = vec3Dot(v1, v1);
        float d20 = vec3Dot(v2, v0), d21 = vec3Dot(v2, v1);
        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < eps) return false;
        float v = (d11 * d20 - d01 * d21) / denom;
        float w = (d00 * d21 - d01 * d20) / denom;
        return v >= -eps && w >= -eps && (v + w) <= 1.0f + eps;
    };

    if (!pointInTri(edgePts[0]) && !pointInTri(edgePts[1])) return false;
    outA = edgePts[0];
    outB = edgePts[1];
    return true;
}

}  // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  BooleanOperation implementation
// ─────────────────────────────────────────────────────────────────────────────

BooleanOperationReport BooleanOperation::validateMesh(const Mesh& mesh) noexcept
{
    BooleanOperationReport report;
    report.code = BooleanOperationDiagnostic::Success;
    report.valid = false;

    const auto& attrs = mesh.attributes();
    const auto& topo = mesh.topology();

    // Check positions
    if (!attrs.hasPositions()) {
        report.addMessage("Mesh has no positions");
        report.code = BooleanOperationDiagnostic::InputAInvalid;
        return report;
    }

    if (attrs.vertexCount() == 0) {
        report.addMessage("Mesh has no vertices");
        report.code = BooleanOperationDiagnostic::InputAEmpty;
        return report;
    }

    for (const auto& p : attrs.positions()) {
        if (!isFiniteVec3(p)) {
            report.addMessage("Mesh contains non-finite position values");
            report.code = BooleanOperationDiagnostic::InputAInvalid;
            return report;
        }
    }

    // Check faces
    if (topo.faceCount() == 0) {
        report.addMessage("Mesh has no faces");
        report.code = BooleanOperationDiagnostic::InputAEmpty;
        return report;
    }

    // Check for non-triangle faces
    bool hasNonTriangles = false;
    bool hasDegenerate = false;
    std::map<EdgeKey, uint32_t> edgeUseCount;
    for (size_t i = 0; i < topo.faceCount(); ++i) {
        const auto& face = topo.face(i);
        if (face.indices.size() < 3) {
            report.addMessage("Mesh contains face with fewer than 3 indices");
            report.code = BooleanOperationDiagnostic::InputAInvalid;
            return report;
        }
        if (face.indices.size() != 3) {
            hasNonTriangles = true;
        }

        for (size_t j = 0; j < face.indices.size(); ++j) {
            const uint32_t a = face.indices[j];
            const uint32_t b = face.indices[(j + 1) % face.indices.size()];
            if (a == b) {
                hasDegenerate = true;
                continue;
            }
            edgeUseCount[makeEdgeKey(a, b)] += 1u;
        }

        if (face.indices.size() == 3) {
            // Validate index bounds before accessing positions
            bool indicesValid = true;
            for (uint32_t idx : face.indices) {
                if (idx >= attrs.vertexCount()) {
                    indicesValid = false;
                    break;
                }
            }
            if (indicesValid) {
                const Vec3& p0 = attrs.positions()[face.indices[0]];
                const Vec3& p1 = attrs.positions()[face.indices[1]];
                const Vec3& p2 = attrs.positions()[face.indices[2]];
                if (isDegenerateTriangle(p0, p1, p2, 1e-8f)) {  // dimensionless relative degeneracy
                    hasDegenerate = true;
                }
            }
        }
    }

    if (hasNonTriangles) {
        report.code = BooleanOperationDiagnostic::InputAHasNonTriangles;
        report.addMessage("Mesh contains non-triangle faces (will be triangulated)");
    }

    // Validate index bounds
    if (!topo.hasValidIndices(attrs.vertexCount())) {
        report.addMessage("Face indices exceed vertex count");
        report.code = BooleanOperationDiagnostic::InputAInvalid;
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    for (const auto& [edge, uses] : edgeUseCount) {
        (void)edge;
        if (uses > 2u) {
            report.addMessage("Mesh contains non-manifold edges");
            report.code = BooleanOperationDiagnostic::InputANotManifold;
            std::sort(report.messages.begin(), report.messages.end());
            return report;
        }
    }

    if (hasDegenerate) {
        report.addMessage("Mesh contains degenerate triangles/edges");
        report.code = BooleanOperationDiagnostic::GeometricDegeneracy;
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    report.valid = true;
    report.code = hasNonTriangles
        ? (BooleanOperationDiagnostic::SuccessWithWarnings | BooleanOperationDiagnostic::InputAHasNonTriangles)
        : BooleanOperationDiagnostic::Success;

    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

BooleanOperationReport BooleanOperation::compute(
    const Mesh& meshA, const Mesh& meshB, BooleanOperationType operation,
    const BooleanOperationOptions& options, Mesh& output) noexcept
{
    BooleanOperationReport report;

    if (!isFiniteFloat(options.geometricTolerance) || options.geometricTolerance <= 0.f) {
        report.code = BooleanOperationDiagnostic::NumericalInstability;
        report.addMessage("geometricTolerance must be finite and > 0");
        report.valid = false;
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    // Validate inputs
    if (options.validateInputs) {
        auto reportA = validateMesh(meshA);
        if (!reportA.valid) {
            report.code = reportA.code;
            if (!reportA.messages.empty()) {
                report.addMessage("Input A validation failed: " + reportA.messages[0]);
            } else {
                report.addMessage("Input A validation failed");
            }
            std::sort(report.messages.begin(), report.messages.end());
            return report;
        }
        if (reportA.hasWarning(BooleanOperationDiagnostic::InputAHasNonTriangles)) {
            report.addMessage("Input A contains non-triangle faces and will be triangulated");
        }

        auto reportB = validateMesh(meshB);
        if (!reportB.valid) {
            report.code = mapValidationCodeToB(reportB.code);
            if (!reportB.messages.empty()) {
                report.addMessage("Input B validation failed: " + reportB.messages[0]);
            } else {
                report.addMessage("Input B validation failed");
            }
            std::sort(report.messages.begin(), report.messages.end());
            return report;
        }
        if (reportB.hasWarning(BooleanOperationDiagnostic::InputAHasNonTriangles)) {
            report.addMessage("Input B contains non-triangle faces and will be triangulated");
        }
    }

    // Triangulate inputs
    std::vector<Vec3> posA, posB;
    std::vector<TriangleFace> trisA, trisB;
    size_t degenerateA = 0;
    size_t degenerateB = 0;

    triangulateInputMesh(meshA, posA, trisA, degenerateA);
    triangulateInputMesh(meshB, posB, trisB, degenerateB);

    if (degenerateA > 0 || degenerateB > 0) {
        report.addMessage("Degenerate triangles were skipped during triangulation");
    }

    if (posA.empty() || trisA.empty()) {
        report.code = BooleanOperationDiagnostic::InputAEmpty;
        report.addMessage("Input A has no triangles after triangulation");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    if (posB.empty() || trisB.empty()) {
        report.code = BooleanOperationDiagnostic::InputBEmpty;
        report.addMessage("Input B has no triangles after triangulation");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    // Check bounding box intersection
    BoundingBox bbA = BoundingBox::fromPositions(posA);
    BoundingBox bbB = BoundingBox::fromPositions(posB);

    if (!bbA.intersects(bbB, options.geometricTolerance)) {
        if (operation == BooleanOperationType::Union) {
            // For union, we can still combine non-intersecting meshes
            report.addMessage("Meshes do not intersect (union will concatenate)");
        } else if (operation == BooleanOperationType::Difference) {
            // Difference against a disjoint mesh is identity: A - B == A.
            output = meshA;
            report.code = BooleanOperationDiagnostic::Success;
            report.addMessage("Meshes do not intersect; returning input A for difference");
            report.valid = true;
            std::sort(report.messages.begin(), report.messages.end());
            return report;
        } else {
            report.code = BooleanOperationDiagnostic::NoIntersection;
            report.addMessage("Meshes do not intersect for this operation");
            report.valid = true;  // Empty result is valid
            std::sort(report.messages.begin(), report.messages.end());
            return report;
        }
    }

    // Compute boolean result
    std::vector<Vec3> outPos;
    std::vector<Face> outFaces;

    // Robust CSG: cut both meshes along the intersection curve, classify each
    // sub-triangle inside/outside the other, stitch the seam — a clean watertight
    // result on coarse meshes (replaces the legacy whole-triangle classifier).
    {
        const Mesh rb = robustMeshBoolean(meshA, meshB, operation);
        outPos = rb.attributes().positions();
        outFaces.reserve(rb.topology().faceCount());
        for (size_t f = 0; f < rb.topology().faceCount(); ++f) {
            outFaces.push_back(rb.topology().face(f));
        }
    }

    if (outPos.empty() || outFaces.empty()) {
        report.code = BooleanOperationDiagnostic::OutputEmpty;
        report.addMessage("Boolean operation resulted in empty mesh");
        report.valid = true;  // Empty result can be valid (e.g., no intersection)
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    // Set output
    output.attributes().setPositions(outPos);
    for (const auto& face : outFaces) {
        output.topology().addFace(face);
    }

    // Compute normals if requested
    if (options.preserveNormals) {
        std::vector<Vec3> normals(outPos.size(), Vec3{0, 0, 0});
        std::vector<uint32_t> normalCounts(outPos.size(), 0);

        // Accumulate face normals to vertices
        for (size_t faceIdx = 0; faceIdx < output.topology().faceCount(); ++faceIdx) {
            const auto& face = output.topology().face(faceIdx);
            if (face.indices.size() == 3) {
                if (!face.indicesInBounds(outPos.size())) continue;
                Vec3 normal = computeFaceNormal(outPos[face.indices[0]], outPos[face.indices[1]],
                                               outPos[face.indices[2]]);

                for (uint32_t idx : face.indices) {
                    normals[idx] = vec3Add(normals[idx], normal);
                    normalCounts[idx]++;
                }
            }
        }

        // Average and normalize
        for (size_t i = 0; i < normals.size(); ++i) {
            if (normalCounts[i] > 0) {
                normals[i] = vec3Normalize(normals[i]);
            }
        }

        output.attributes().setNormals(normals);
    }

    report.code = (degenerateA > 0 || degenerateB > 0)
        ? (BooleanOperationDiagnostic::SuccessWithWarnings | BooleanOperationDiagnostic::GeometricDegeneracy)
        : BooleanOperationDiagnostic::Success;
    report.valid = true;
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

const char* BooleanOperation::operationName(BooleanOperationType op) noexcept
{
    switch (op) {
        case BooleanOperationType::Union:
            return "Union";
        case BooleanOperationType::Difference:
            return "Difference";
        case BooleanOperationType::Intersection:
            return "Intersection";
        default:
            return "Unknown";
    }
}

}  // namespace nexus::geometry
