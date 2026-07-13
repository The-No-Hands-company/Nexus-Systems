#include <nexus/geometry/InsetFacesOperation.h>
#include <nexus/geometry/HalfEdgeMesh.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

namespace {

using Vec3 = nexus::render::Vec3;

bool isFiniteF32(float v) noexcept
{
    // Bitwise finite check to avoid compiler assumptions under finite-math modes.
    uint32_t bits = 0u;
    std::memcpy(&bits, &v, sizeof(v));
    return (bits & 0x7F800000u) != 0x7F800000u;
}

inline Vec3 vec3Add(const Vec3& a, const Vec3& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 vec3Sub(const Vec3& a, const Vec3& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec3Scale(const Vec3& v, float s) noexcept
{
    return {v.x * s, v.y * s, v.z * s};
}

inline float vec3Length(const Vec3& v) noexcept
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// Compute centroid of a polygon face.
Vec3 faceCentroid(const std::vector<Vec3>& positions, const Face& face) noexcept
{
    Vec3 c{0.f, 0.f, 0.f};
    for (const uint32_t idx : face.indices) {
        c = vec3Add(c, positions[idx]);
    }
    const float inv = 1.f / static_cast<float>(face.indices.size());
    return vec3Scale(c, inv);
}

// Lerp Vec3 a → b by t
Vec3 lerpVec3(const Vec3& a, const Vec3& b, float t) noexcept
{
    return vec3Add(a, vec3Scale(vec3Sub(b, a), t));
}

Vec2 lerpVec2(const Vec2& a, const Vec2& b, float t) noexcept
{
    return {a.u + (b.u - a.u) * t, a.v + (b.v - a.v) * t};
}

// Compute inset t for a single vertex given the inset mode/amount.
// Returns a factor in [0, 1] representing how far to move from vertex toward centroid.
float insetFactor(const Vec3& vertex, const Vec3& centroid,
                  const InsetDesc& desc) noexcept
{
    if (desc.mode == InsetMode::Factor) {
        return std::clamp(desc.amount, 0.f, 1.f);
    }
    // Distance mode: compute t such that |vertex - centroid| * t == distance
    const float distToCentroid = vec3Length(vec3Sub(centroid, vertex));
    if (distToCentroid <= 1e-12f) {
        return 0.f;
    }
    return std::clamp(desc.amount / distToCentroid, 0.f, 1.f);
}

} // namespace

InsetReport InsetFacesOperation::applyToAllFaces(const Mesh&      input,
                                                  const InsetDesc& desc,
                                                  Mesh&            output) noexcept
{
    InsetReport report{};

    if (!input.isValid()) {
        report.diagnostic = InsetDiagnostic::InvalidInputMesh;
        report.messages.push_back("Input mesh is invalid");
        return report;
    }

    if (!isFiniteF32(desc.amount)) {
        report.diagnostic = InsetDiagnostic::InvalidAmount;
        report.messages.push_back("Inset amount must be finite");
        return report;
    }

    if (desc.mode == InsetMode::Distance && desc.amount < 0.f) {
        report.diagnostic = InsetDiagnostic::InvalidAmount;
        report.messages.push_back("Inset distance must be >= 0");
        return report;
    }

    const size_t originalFaceCount   = input.topology().faceCount();
    const size_t originalVertexCount = input.attributes().vertexCount();

    if (originalFaceCount == 0u || originalVertexCount == 0u) {
        report.diagnostic = InsetDiagnostic::InvalidInputMesh;
        report.messages.push_back("Input mesh has no geometry");
        return report;
    }

    // Fast path: route the common Factor + replaceOriginal case through the
    // hardened half-edge core. HalfEdgeMesh::insetFace preserves the full
    // attribute set and is integrity-proven (inner ring + border quads per
    // face). Falls back to the raw index path below when the mesh isn't
    // half-edge-constructible (non-manifold / duplicate-index faces).
    if (desc.mode == InsetMode::Factor && desc.replaceOriginal) {
        std::vector<uint32_t> faceSides(originalFaceCount);
        for (size_t fi = 0; fi < originalFaceCount; ++fi) {
            faceSides[fi] = static_cast<uint32_t>(input.topology().face(fi).indices.size());
        }
        if (auto hem = HalfEdgeMesh::fromMesh(input)) {
            uint32_t inset = 0;
            for (uint32_t fi = 0; fi < static_cast<uint32_t>(originalFaceCount); ++fi) {
                if (faceSides[fi] >= 3u && hem->insetFace(fi, desc.amount)) ++inset;
            }
            if (inset > 0u) {
                output = hem->toMesh(/*triangulate=*/false);  // preserve inner ring + quad borders
                report.insetFaceCount   = inset;
                report.addedVertexCount = static_cast<uint32_t>(
                    output.attributes().vertexCount() - originalVertexCount);
                report.addedFaceCount   = static_cast<uint32_t>(output.topology().faceCount());

                if (desc.recomputeNormals) {
                    if (!output.computeVertexNormals()) {
                        report.diagnostic = report.diagnostic | InsetDiagnostic::NormalRebuildFailed;
                        report.messages.push_back("Normal rebuild failed after inset");
                    }
                    output.attributes().clearTangents();
                }
                output.rebuildStableElementIds();
                if (!output.isValid()) {
                    report.diagnostic = report.diagnostic | InsetDiagnostic::OutputTopologyInvalid;
                    report.messages.push_back("Output mesh topology is invalid");
                    report.valid = false;
                    return report;
                }
                report.diagnostic = (report.diagnostic == InsetDiagnostic::Success)
                    ? InsetDiagnostic::Success
                    : (report.diagnostic | InsetDiagnostic::SuccessWithWarnings);
                report.valid = true;
                return report;
            }
        }
        // Not half-edge-constructible or nothing inset — fall through to raw path.
    }

    // Snapshot input (handles input == output aliasing)
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

    // ── Build output attribute buffers ────────────────────────────────────────
    std::vector<Vec3>         outPositions = srcPositions;
    std::vector<Vec3>         outNormals   = srcNormals;
    std::vector<Vec2>         outUVs       = srcUVs;
    std::vector<Vec4>         outTangents  = srcTangents;
    std::vector<JointIndex4>  outJIndices  = srcJIndices;
    std::vector<JointWeight4> outJWeights  = srcJWeights;

    // ── Build face list ───────────────────────────────────────────────────────
    std::vector<Face> outFaces;
    outFaces.reserve(originalFaceCount * 6u);

    if (!desc.replaceOriginal) {
        outFaces = srcFaces;  // keep originals alongside inset geometry
    }

    for (const Face& face : srcFaces) {
        const size_t n = face.indices.size();
        if (n < 3u) {
            if (!desc.replaceOriginal) { /* already kept */ }
            continue;
        }

        const Vec3 centroid = faceCentroid(srcPositions, face);

        // Create inner ring vertices
        std::vector<uint32_t> innerIndices(n);
        for (size_t j = 0; j < n; ++j) {
            const uint32_t srcVI = face.indices[j];
            const uint32_t newVI = static_cast<uint32_t>(outPositions.size());
            innerIndices[j]      = newVI;

            const Vec3& srcPos = srcPositions[srcVI];
            const float t      = insetFactor(srcPos, centroid, desc);
            const Vec3  insetPos = lerpVec3(srcPos, centroid, t);
            outPositions.push_back(insetPos);

            if (hasNormals && srcVI < srcNormals.size()) {
                outNormals.push_back(srcNormals[srcVI]);
            }
            if (hasUVs && srcVI < srcUVs.size()) {
                // Centroid UV = average of face UV coordinates
                Vec2 uvCentroid{0.f, 0.f};
                for (const uint32_t idx : face.indices) {
                    uvCentroid.u += srcUVs[idx].u;
                    uvCentroid.v += srcUVs[idx].v;
                }
                uvCentroid.u /= static_cast<float>(n);
                uvCentroid.v /= static_cast<float>(n);
                outUVs.push_back(lerpVec2(srcUVs[srcVI], uvCentroid, t));
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

        // Inner face (inset cap, same winding as original)
        outFaces.push_back(Face{std::vector<uint32_t>(innerIndices.begin(), innerIndices.end())});
        ++report.addedFaceCount;

        // Border ring quads: [outer[j], outer[k], inner[k], inner[j]]
        for (size_t j = 0; j < n; ++j) {
            const size_t   k    = (j + 1u) % n;
            const uint32_t ov0  = face.indices[j];
            const uint32_t ov1  = face.indices[k];
            const uint32_t iv0  = innerIndices[j];
            const uint32_t iv1  = innerIndices[k];
            outFaces.push_back(Face{{ov0, ov1, iv1, iv0}});
            ++report.addedFaceCount;
        }

        ++report.insetFaceCount;
    }

    if (report.insetFaceCount == 0u) {
        report.diagnostic = InsetDiagnostic::NoFacesInset;
        report.messages.push_back("No valid faces were inset");
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
            report.diagnostic = report.diagnostic | InsetDiagnostic::NormalRebuildFailed;
            report.messages.push_back("Normal rebuild failed after inset");
        }
        output.attributes().clearTangents();
    }

    output.rebuildStableElementIds();

    if (!output.isValid()) {
        report.diagnostic = report.diagnostic | InsetDiagnostic::OutputTopologyInvalid;
        report.messages.push_back("Output mesh topology is invalid");
        report.valid = false;
        return report;
    }

    if (report.diagnostic == InsetDiagnostic::Success) {
        report.valid = true;
    } else {
        report.diagnostic = report.diagnostic | InsetDiagnostic::SuccessWithWarnings;
        report.valid = true;
    }
    return report;
}

} // namespace nexus::geometry
