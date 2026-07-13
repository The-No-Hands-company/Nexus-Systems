#include <nexus/geometry/ExtrudeOperation.h>
#include <nexus/geometry/HalfEdgeMesh.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <map>
#include <vector>

namespace nexus::geometry {

namespace {

using Vec3 = nexus::render::Vec3;

struct EdgeKey {
    uint32_t a = 0u;
    uint32_t b = 0u;

    bool operator<(const EdgeKey& rhs) const noexcept
    {
        if (a != rhs.a) { return a < rhs.a; }
        return b < rhs.b;
    }
};

EdgeKey makeEdgeKey(uint32_t i0, uint32_t i1) noexcept
{
    if (i0 < i1) { return {i0, i1}; }
    return {i1, i0};
}

bool isFiniteF32(float v) noexcept
{
    // Bitwise finite check to avoid compiler assumptions under finite-math modes.
    uint32_t bits = 0u;
    std::memcpy(&bits, &v, sizeof(v));
    return (bits & 0x7F800000u) != 0x7F800000u;
}

inline Vec3 vec3Sub(const Vec3& a, const Vec3& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec3Add(const Vec3& a, const Vec3& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 vec3Scale(const Vec3& v, float s) noexcept
{
    return {v.x * s, v.y * s, v.z * s};
}

inline Vec3 vec3Cross(const Vec3& a, const Vec3& b) noexcept
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float vec3Dot(const Vec3& a, const Vec3& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float vec3Length(const Vec3& v) noexcept
{
    return std::sqrt(vec3Dot(v, v));
}

inline Vec3 vec3Normalize(const Vec3& v) noexcept
{
    const float len = vec3Length(v);
    if (len <= 1e-12f) {
        return {0.f, 1.f, 0.f};
    }
    return vec3Scale(v, 1.f / len);
}

// Compute face normal using the first triangle fan of the face.
// Falls back to +Y for degenerate faces.
Vec3 faceNormal(const std::vector<Vec3>& positions, const Face& face) noexcept
{
    if (!face.indicesInBounds(positions.size())) {
        return {0.f, 1.f, 0.f};
    }
    if (face.indices.size() < 3u) {
        return {0.f, 1.f, 0.f};
    }

    // Newell's method — robust for convex and non-convex polygons
    Vec3 n{0.f, 0.f, 0.f};
    const size_t count = face.indices.size();
    for (size_t i = 0; i < count; ++i) {
        const Vec3& cur  = positions[face.indices[i]];
        const Vec3& next = positions[face.indices[(i + 1) % count]];
        n.x += (cur.y - next.y) * (cur.z + next.z);
        n.y += (cur.z - next.z) * (cur.x + next.x);
        n.z += (cur.x - next.x) * (cur.y + next.y);
    }

    return vec3Normalize(n);
}

} // namespace

ExtrudeReport ExtrudeOperation::applyToAllFaces(const Mesh&        input,
                                                const ExtrudeDesc& desc,
                                                Mesh&              output) noexcept
{
    ExtrudeReport report{};

    if (!input.isValid()) {
        report.diagnostic = ExtrudeDiagnostic::InvalidInputMesh;
        report.messages.push_back("Input mesh is invalid");
        return report;
    }

    if (!isFiniteF32(desc.distance)) {
        report.diagnostic = ExtrudeDiagnostic::InvalidDistance;
        report.messages.push_back("Extrude distance must be finite");
        return report;
    }

    const size_t originalFaceCount   = input.topology().faceCount();
    const size_t originalVertexCount = input.attributes().vertexCount();

    if (originalFaceCount == 0u || originalVertexCount == 0u) {
        report.diagnostic = ExtrudeDiagnostic::InvalidInputMesh;
        report.messages.push_back("Input mesh has no geometry");
        return report;
    }

    // Fast path: keepOriginalFaces=false is a manifold per-face prism extrude —
    // route it through the hardened half-edge core (extrudeFacePrism, integrity-
    // proven, full attribute copy). keepOriginalFaces=true retains the base as an
    // overlapping (non-manifold) prism the half-edge core can't represent, so it
    // stays on the raw path below; likewise any input that isn't
    // half-edge-constructible falls back.
    if (!desc.keepOriginalFaces) {
        std::vector<uint32_t> faceSides(originalFaceCount);
        for (size_t fi = 0; fi < originalFaceCount; ++fi) {
            faceSides[fi] = static_cast<uint32_t>(input.topology().face(fi).indices.size());
        }
        if (auto hem = HalfEdgeMesh::fromMesh(input)) {
            uint32_t extruded = 0;
            for (uint32_t fi = 0; fi < static_cast<uint32_t>(originalFaceCount); ++fi) {
                if (faceSides[fi] >= 3u && hem->extrudeFacePrism(fi, desc.distance)) ++extruded;
            }
            if (extruded > 0u) {
                output = hem->toMesh(/*triangulate=*/false);
                report.extrudedFaceCount = extruded;
                report.addedVertexCount  = static_cast<uint32_t>(
                    output.attributes().vertexCount() - originalVertexCount);
                report.addedFaceCount    = static_cast<uint32_t>(output.topology().faceCount());

                if (desc.recomputeNormals) {
                    if (!output.computeVertexNormals()) {
                        report.diagnostic = report.diagnostic | ExtrudeDiagnostic::NormalRebuildFailed;
                        report.messages.push_back("Normal rebuild failed after extrusion");
                    }
                    output.attributes().clearTangents();
                }
                output.rebuildStableElementIds();
                if (!output.isValid()) {
                    report.diagnostic = report.diagnostic | ExtrudeDiagnostic::OutputTopologyInvalid;
                    report.messages.push_back("Output mesh topology is invalid");
                    report.valid = false;
                    return report;
                }
                report.diagnostic = (report.diagnostic == ExtrudeDiagnostic::Success)
                    ? ExtrudeDiagnostic::Success
                    : (report.diagnostic | ExtrudeDiagnostic::SuccessWithWarnings);
                report.valid = true;
                return report;
            }
        }
        // Not half-edge-constructible or nothing extruded — fall through to raw.
    }

    // Snapshot all input data before writing to output (handles input == output)
    const std::vector<Vec3>        srcPositions = input.attributes().positions();
    const bool                     hasNormals   = input.attributes().hasNormals();
    const bool                     hasUVs       = input.attributes().hasUVs();
    const bool                     hasTangents  = input.attributes().hasTangents();
    const bool                     hasSkinning  = input.attributes().hasSkinning();
    const std::vector<Vec3>        srcNormals   = hasNormals  ? input.attributes().normals()      : std::vector<Vec3>{};
    const std::vector<Vec2>        srcUVs       = hasUVs      ? input.attributes().uvs()          : std::vector<Vec2>{};
    const std::vector<Vec4>        srcTangents  = hasTangents ? input.attributes().tangents()     : std::vector<Vec4>{};
    const std::vector<JointIndex4> srcJIndices  = hasSkinning ? input.attributes().jointIndices() : std::vector<JointIndex4>{};
    const std::vector<JointWeight4>srcJWeights  = hasSkinning ? input.attributes().jointWeights() : std::vector<JointWeight4>{};

    std::vector<Face> srcFaces;
    srcFaces.reserve(originalFaceCount);
    for (size_t fi = 0; fi < originalFaceCount; ++fi) {
        srcFaces.push_back(input.topology().face(fi));
    }

    std::map<EdgeKey, uint32_t> srcEdgeUseCount;
    for (const Face& face : srcFaces) {
        if (face.indices.size() < 2u) {
            continue;
        }
        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t a = face.indices[i];
            const uint32_t b = face.indices[(i + 1u) % face.indices.size()];
            srcEdgeUseCount[makeEdgeKey(a, b)] += 1u;
        }
    }

    // ── Build output attribute buffers (start from copy of input) ─────────────
    std::vector<Vec3>         outPositions = srcPositions;
    std::vector<Vec3>         outNormals   = srcNormals;
    std::vector<Vec2>         outUVs       = srcUVs;
    std::vector<Vec4>         outTangents  = srcTangents;
    std::vector<JointIndex4>  outJIndices  = srcJIndices;
    std::vector<JointWeight4> outJWeights  = srcJWeights;

    // ── Build new face list ───────────────────────────────────────────────────
    // If keepOriginalFaces, start with originals; otherwise start empty.
    std::vector<Face> outFaces;
    outFaces.reserve(originalFaceCount * 6u);
    if (desc.keepOriginalFaces) {
        outFaces = srcFaces;
    }

    for (const Face& face : srcFaces) {
        const size_t n = face.indices.size();
        if (n < 3u) {
            // Degenerate — keep as-is if we're keeping originals, skip otherwise
            if (desc.keepOriginalFaces) {
                // already in outFaces
            }
            continue;
        }

        const Vec3 normal = faceNormal(srcPositions, face);

        bool duplicateWallBase = false;
        if (desc.keepOriginalFaces) {
            for (size_t j = 0; j < n; ++j) {
                const uint32_t a = face.indices[j];
                const uint32_t b = face.indices[(j + 1u) % n];
                const auto it = srcEdgeUseCount.find(makeEdgeKey(a, b));
                if (it != srcEdgeUseCount.end() && it->second > 1u) {
                    duplicateWallBase = true;
                    break;
                }
            }
        }

        std::vector<uint32_t> wallBaseIndices(n);
        if (duplicateWallBase) {
            for (size_t j = 0; j < n; ++j) {
                const uint32_t srcVI = face.indices[j];
                const uint32_t newVI = static_cast<uint32_t>(outPositions.size());
                wallBaseIndices[j] = newVI;

                outPositions.push_back(srcPositions[srcVI]);
                if (hasNormals && srcVI < srcNormals.size()) {
                    outNormals.push_back(srcNormals[srcVI]);
                }
                if (hasUVs && srcVI < srcUVs.size()) {
                    outUVs.push_back(srcUVs[srcVI]);
                }
                if (hasTangents && srcVI < srcTangents.size()) {
                    outTangents.push_back(srcTangents[srcVI]);
                }
                if (hasSkinning && srcVI < srcJIndices.size()) {
                    outJIndices.push_back(srcJIndices[srcVI]);
                    outJWeights.push_back(srcJWeights[srcVI]);
                }
                ++report.addedVertexCount;
            }
        } else {
            wallBaseIndices.assign(face.indices.begin(), face.indices.end());
        }

        // Create one new vertex per face vertex (the extruded ring)
        std::vector<uint32_t> capIndices(n);
        for (size_t j = 0; j < n; ++j) {
            const uint32_t srcVI  = face.indices[j];
            const uint32_t newVI  = static_cast<uint32_t>(outPositions.size());
            capIndices[j] = newVI;

            const Vec3& srcPos = srcPositions[srcVI];
            outPositions.push_back(vec3Add(srcPos, vec3Scale(normal, desc.distance)));

            if (hasNormals && srcVI < srcNormals.size()) {
                outNormals.push_back(srcNormals[srcVI]);
            }
            if (hasUVs && srcVI < srcUVs.size()) {
                outUVs.push_back(srcUVs[srcVI]);
            }
            if (hasTangents && srcVI < srcTangents.size()) {
                outTangents.push_back(srcTangents[srcVI]);
            }
            if (hasSkinning && srcVI < srcJIndices.size()) {
                outJIndices.push_back(srcJIndices[srcVI]);
                outJWeights.push_back(srcJWeights[srcVI]);
            }
            ++report.addedVertexCount;
        }

        // Add extruded cap face (same winding as original → correct normal direction)
        outFaces.push_back(Face{std::vector<uint32_t>(capIndices.begin(), capIndices.end())});
        ++report.addedFaceCount;

        // Add wall quads for each edge: [src[j], src[k], cap[k], cap[j]]
        for (size_t j = 0; j < n; ++j) {
            const size_t   k    = (j + 1u) % n;
            const uint32_t sv0  = wallBaseIndices[j];
            const uint32_t sv1  = wallBaseIndices[k];
            const uint32_t cv0  = capIndices[j];
            const uint32_t cv1  = capIndices[k];
            // Wall winding: outward-facing for positive extrusion distance
            outFaces.push_back(Face{{sv1, sv0, cv0, cv1}});
            ++report.addedFaceCount;
        }

        ++report.extrudedFaceCount;
    }

    if (report.extrudedFaceCount == 0u) {
        report.diagnostic = ExtrudeDiagnostic::NoFacesExtruded;
        report.messages.push_back("No valid faces were extruded");
        return report;
    }

    // ── Write output mesh ─────────────────────────────────────────────────────
    output.topology().clearFaces();
    for (const Face& f : outFaces) {
        output.topology().addFace(f);
    }

    output.attributes().setPositions(std::move(outPositions));

    if (hasNormals && !outNormals.empty()) {
        output.attributes().setNormals(std::move(outNormals));
    }
    if (hasUVs && !outUVs.empty()) {
        output.attributes().setUVs(std::move(outUVs));
    }
    if (hasTangents && !outTangents.empty()) {
        output.attributes().setTangents(std::move(outTangents));
    }
    if (hasSkinning && !outJIndices.empty()) {
        output.attributes().setSkinning(std::move(outJIndices), std::move(outJWeights));
    }

    if (desc.recomputeNormals) {
        if (!output.computeVertexNormals()) {
            report.diagnostic = report.diagnostic | ExtrudeDiagnostic::NormalRebuildFailed;
            report.messages.push_back("Normal rebuild failed after extrusion");
        }
        output.attributes().clearTangents();
    }

    output.rebuildStableElementIds();

    if (!output.isValid()) {
        report.diagnostic = report.diagnostic | ExtrudeDiagnostic::OutputTopologyInvalid;
        report.messages.push_back("Output mesh topology is invalid");
        report.valid = false;
        return report;
    }

    if (report.diagnostic == ExtrudeDiagnostic::Success) {
        report.valid = true;
    } else {
        report.diagnostic = report.diagnostic | ExtrudeDiagnostic::SuccessWithWarnings;
        report.valid = true;
    }
    return report;
}

} // namespace nexus::geometry
