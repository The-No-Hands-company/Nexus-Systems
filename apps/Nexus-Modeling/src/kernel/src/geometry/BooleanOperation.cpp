// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Boolean Operations Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/MeshBVH.h>
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

inline bool isDegenerateTriangle(const Vec3& p0, const Vec3& p1, const Vec3& p2, float eps)
{
    const Vec3 e1 = vec3Sub(p1, p0);
    const Vec3 e2 = vec3Sub(p2, p0);
    const Vec3 c = vec3Cross(e1, e2);
    return vec3Dot(c, c) <= eps;
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
bool pointInMesh(const Vec3& point, const std::vector<Vec3>& positions,
                 const std::vector<TriangleFace>& triangles, float tolerance = 1e-5f)
{
    // Ray direction (fixed for determinism)
    Vec3 rayDir = vec3Normalize(Vec3{1.0f, 1.0f, 1.0f});

    // Count ray-triangle intersections
    int crossings = 0;
    for (const auto& tri : triangles) {
        if (tri.indices[0] >= positions.size() ||
            tri.indices[1] >= positions.size() ||
            tri.indices[2] >= positions.size()) continue;
        const Vec3& p0 = positions[tri.indices[0]];
        const Vec3& p1 = positions[tri.indices[1]];
        const Vec3& p2 = positions[tri.indices[2]];

        // Möller-Trumbore intersection test
        Vec3 e1 = vec3Sub(p1, p0);
        Vec3 e2 = vec3Sub(p2, p0);
        Vec3 h = vec3Cross(rayDir, e2);
        float a = vec3Dot(e1, h);

        if (std::abs(a) < tolerance) {
            continue;  // Parallel or degenerate
        }

        float f = 1.0f / a;
        Vec3 s = vec3Sub(point, p0);
        float u = f * vec3Dot(s, h);

        if (u < 0.0f || u > 1.0f) {
            continue;
        }

        Vec3 q = vec3Cross(s, e1);
        float v = f * vec3Dot(rayDir, q);

        if (v < 0.0f || u + v > 1.0f) {
            continue;
        }

        float t = f * vec3Dot(e2, q);
        if (t > tolerance) {
            crossings++;
        }
    }

    return (crossings & 1) == 1;
}

// BVH-accelerated point-in-mesh: uses raycast for fast outside rejection.
// If the ray doesn't hit any BVH node, the point is definitely outside.
// If it hits, falls back to exact triangle-by-triangle counting.
bool pointInMeshBVH(const Vec3& point, const MeshBVH& bvh,
                    const std::vector<Vec3>& positions,
                    const std::vector<TriangleFace>& triangles,
                    float tolerance = 1e-5f) noexcept
{
    Vec3 rayDir = vec3Normalize(Vec3{1.0f, 1.0f, 1.0f});
    Ray ray{point, rayDir};

    auto hit = bvh.raycast(ray);
    if (hit.t == std::numeric_limits<float>::max()) return false; // fast reject: outside

    return pointInMesh(point, positions, triangles, tolerance);
}

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
                                     1e-16f)) {
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

void cleanupBooleanOutput(std::vector<Vec3>& positions, std::vector<Face>& faces, float mergeEps = 1e-5f) {
    if (faces.empty()) return;

    // Phase 1: Merge coincident vertices
    std::vector<uint32_t> vertexRemap(positions.size());
    std::vector<Vec3> mergedPos;
    for (size_t i = 0; i < positions.size(); ++i) {
        bool found = false;
        for (size_t j = 0; j < mergedPos.size(); ++j) {
            float dx = mergedPos[j].x - positions[i].x;
            float dy = mergedPos[j].y - positions[i].y;
            float dz = mergedPos[j].z - positions[i].z;
            if (dx*dx + dy*dy + dz*dz < mergeEps * mergeEps) {
                vertexRemap[i] = static_cast<uint32_t>(j);
                found = true;
                break;
            }
        }
        if (!found) {
            vertexRemap[i] = static_cast<uint32_t>(mergedPos.size());
            mergedPos.push_back(positions[i]);
        }
    }
    positions = std::move(mergedPos);

    // Phase 2: Remap face indices and remove degenerate faces
    std::vector<Face> cleanFaces;
    for (auto& f : faces) {
        for (auto& idx : f.indices) {
            if (idx < vertexRemap.size()) idx = vertexRemap[idx];
        }
        // Remove degenerate faces (duplicate indices)
        if (f.indices.size() >= 3) {
            bool degenerate = false;
            for (size_t i = 0; i < f.indices.size(); ++i) {
                if (f.indices[i] == f.indices[(i + 1) % f.indices.size()]) {
                    degenerate = true;
                    break;
                }
            }
            if (!degenerate) {
                // Also check zero area
                Vec3 a = positions[f.indices[0]];
                Vec3 b = positions[f.indices[1]];
                Vec3 c = positions[f.indices[2]];
                Vec3 cross = vec3Cross(vec3Sub(b, a), vec3Sub(c, a));
                if (vec3Dot(cross, cross) > 1e-12f) {
                    cleanFaces.push_back(f);
                }
            }
        }
    }
    faces = std::move(cleanFaces);
}


void computeBooleanResult(const std::vector<Vec3>& posA, const std::vector<TriangleFace>& trisA,
                          const std::vector<Vec3>& posB, const std::vector<TriangleFace>& trisB,
                          BooleanOperationType operation, float tolerance,
                          std::vector<Vec3>& outPos, std::vector<Face>& outFaces)
{
    outPos.clear();
    outFaces.clear();

    // Build BVHs for fast outside rejection
    Mesh bvhMeshB;
    bvhMeshB.attributes().setPositions(posB);
    for (const auto& tri : trisB) {
        Face f; f.indices = {tri.indices[0], tri.indices[1], tri.indices[2]};
        bvhMeshB.topology().addFace(f);
    }
    MeshBVH bvhB;
    bvhB.build(bvhMeshB);

    Mesh bvhMeshA;
    bvhMeshA.attributes().setPositions(posA);
    for (const auto& tri : trisA) {
        Face f; f.indices = {tri.indices[0], tri.indices[1], tri.indices[2]};
        bvhMeshA.topology().addFace(f);
    }
    MeshBVH bvhA;
    bvhA.build(bvhMeshA);

    std::map<uint32_t, uint32_t> posIndexRemap;  // old pos index -> new pos index

    auto addTriangle = [&](const Vec3& p0, const Vec3& p1, const Vec3& p2) {
        // Find or create indices
        auto findOrAddPos = [&](const Vec3& p) {
            // Simple pointer comparison for determinism
            uint32_t oldIdx = 0;
            float minDist = std::numeric_limits<float>::max();
            for (size_t i = 0; i < posA.size(); ++i) {
                float d = vec3Length(vec3Sub(posA[i], p));
                if (d < minDist) {
                    minDist = d;
                    oldIdx = i;
                }
            }

            auto it = posIndexRemap.find(oldIdx);
            if (it != posIndexRemap.end()) {
                return it->second;
            }

            uint32_t newIdx = static_cast<uint32_t>(outPos.size());
            outPos.push_back(p);
            posIndexRemap[oldIdx] = newIdx;
            return newIdx;
        };

        Face f;
        f.indices.push_back(findOrAddPos(p0));
        f.indices.push_back(findOrAddPos(p1));
        f.indices.push_back(findOrAddPos(p2));
        outFaces.push_back(f);
    };

    // Process triangles from A
    for (const auto& tri : trisA) {
        if (tri.indices[0] >= posA.size() || tri.indices[1] >= posA.size() || tri.indices[2] >= posA.size()) continue;
        const Vec3& p0 = posA[tri.indices[0]];
        const Vec3& p1 = posA[tri.indices[1]];
        const Vec3& p2 = posA[tri.indices[2]];

        // Test face center against B mesh
        Vec3 center = vec3Scale(vec3Add(p0, vec3Add(vec3Scale(p1, 1.0f), vec3Scale(p2, 1.0f))),
                                1.0f / 3.0f);
        bool inB = pointInMeshBVH(center, bvhB, posB, trisB, tolerance);

        bool keep = false;
        switch (operation) {
            case BooleanOperationType::Union:
                keep = true;  // Always keep A in union
                break;
            case BooleanOperationType::Difference:
                keep = !inB;  // Keep A triangles outside B
                break;
            case BooleanOperationType::Intersection:
                keep = inB;  // Keep A triangles inside B
                break;
        }

        if (keep) {
            addTriangle(p0, p1, p2);
        }
    }

    // Process triangles from B (only for union)
    if (operation == BooleanOperationType::Union) {
        for (const auto& tri : trisB) {
            const Vec3& p0 = posB[tri.indices[0]];
            const Vec3& p1 = posB[tri.indices[1]];
            const Vec3& p2 = posB[tri.indices[2]];

            // Test face center against A mesh
            Vec3 center = vec3Scale(vec3Add(p0, vec3Add(vec3Scale(p1, 1.0f), vec3Scale(p2, 1.0f))),
                                    1.0f / 3.0f);
            bool inA = pointInMeshBVH(center, bvhA, posA, trisA, tolerance);

            // Only add B triangles that are outside A (to avoid duplicates)
            if (!inA) {
                addTriangle(p0, p1, p2);
            }
        }
    }

    // Post-process: remove degenerate triangles and merge coincident vertices
    cleanupBooleanOutput(outPos, outFaces);
}



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
                if (isDegenerateTriangle(p0, p1, p2, 1e-16f)) {
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

    computeBooleanResult(posA, trisA, posB, trisB, operation, options.geometricTolerance, outPos,
                         outFaces);

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
