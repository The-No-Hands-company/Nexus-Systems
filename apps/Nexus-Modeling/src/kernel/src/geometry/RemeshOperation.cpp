#include <nexus/geometry/RemeshOperation.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <vector>

namespace nexus::geometry {

namespace {

using Vec3 = nexus::render::Vec3;

struct EdgeKey {
    uint32_t v0 = 0;
    uint32_t v1 = 0;

    [[nodiscard]] bool operator<(const EdgeKey& other) const noexcept
    {
        if (v0 != other.v0) {
            return v0 < other.v0;
        }
        return v1 < other.v1;
    }
};

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
        return {0.f, 0.f, 1.f};
    }
    return vec3Scale(v, 1.f / len);
}

float edgeLength(const std::vector<Vec3>& positions, uint32_t a, uint32_t b) noexcept
{
    return vec3Length(vec3Sub(positions[a], positions[b]));
}

Vec2 lerpVec2(const Vec2& a, const Vec2& b) noexcept
{
    return {(a.u + b.u) * 0.5f, (a.v + b.v) * 0.5f};
}

Vec4 lerpVec4(const Vec4& a, const Vec4& b) noexcept
{
    return {
        (a.x + b.x) * 0.5f,
        (a.y + b.y) * 0.5f,
        (a.z + b.z) * 0.5f,
        (a.w + b.w) * 0.5f,
    };
}

bool isFiniteFloat(float value) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

JointWeight4 averageWeights(const JointWeight4& a, const JointWeight4& b) noexcept
{
    JointWeight4 out{};
    for (size_t i = 0; i < kMaxSkinInfluencesPerVertex; ++i) {
        out[i] = (a[i] + b[i]) * 0.5f;
    }
    return out;
}

} // namespace

RemeshReport RemeshOperation::apply(const Mesh& input,
                                    const RemeshDesc& desc,
                                    Mesh& output) noexcept
{
    RemeshReport report{};
    report.inputFaceCount = static_cast<uint32_t>(input.topology().faceCount());

    if (!input.isValid()) {
        report.diagnostic = RemeshDiagnostic::InvalidInputMesh;
        report.messages.push_back("Input mesh is invalid");
        return report;
    }
    for (const auto& p : input.attributes().positions()) {
        if (!isFiniteFloat(p.x) || !isFiniteFloat(p.y) || !isFiniteFloat(p.z)) {
            // Reject non-finite input rather than report valid with NaN/Inf output.
            report.diagnostic = RemeshDiagnostic::InvalidInputMesh;
            report.messages.push_back("Input mesh has non-finite positions");
            return report;
        }
    }

    if (desc.targetEdgeLength <= 0.f || !isFiniteFloat(desc.targetEdgeLength)
        || desc.splitThresholdMultiplier < 1.0f || !isFiniteFloat(desc.splitThresholdMultiplier)) {
        report.diagnostic = RemeshDiagnostic::InvalidTargetEdgeLength;
        report.messages.push_back("targetEdgeLength must be finite and > 0, splitThresholdMultiplier must be >= 1");
        return report;
    }

    if (desc.maxIterations == 0u) {
        report.diagnostic = RemeshDiagnostic::InvalidIterationCount;
        report.messages.push_back("maxIterations must be > 0");
        return report;
    }

    output = input;

    if (output.topology().triangulate() != output.topology().faceCount()) {
        // This branch is unreachable by construction, kept for defensive consistency.
    }

    if (output.topology().faceCount() != input.topology().faceCount()) {
        report.diagnostic = report.diagnostic | RemeshDiagnostic::InputTriangulated;
        report.messages.push_back("Input contained non-triangle faces and was triangulated before remeshing");
    }

    std::vector<Vec3> positions = output.attributes().positions();
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    std::vector<Vec4> tangents;
    std::vector<JointIndex4> jointIndices;
    std::vector<JointWeight4> jointWeights;

    const MeshAttributes& attrs = output.attributes();
    if (attrs.hasNormals()) {
        normals = attrs.normals();
    }
    if (attrs.hasUVs()) {
        uvs = attrs.uvs();
    }
    if (attrs.hasTangents()) {
        tangents = attrs.tangents();
    }
    if (attrs.hasSkinning()) {
        jointIndices = attrs.jointIndices();
        jointWeights = attrs.jointWeights();
    }

    std::vector<Face> faces;
    faces.reserve(output.topology().faceCount());
    for (size_t fi = 0; fi < output.topology().faceCount(); ++fi) {
        faces.push_back(output.topology().face(fi));
    }

    const float splitThreshold = desc.targetEdgeLength * desc.splitThresholdMultiplier;

    for (uint32_t iteration = 0; iteration < desc.maxIterations; ++iteration) {
        report.iterationsRan = iteration + 1;
        bool anySplit = false;

        std::map<EdgeKey, uint32_t> midpointCache;
        std::vector<Face> nextFaces;
        nextFaces.reserve(faces.size() * 2u);

        auto midpointForEdge = [&](uint32_t a, uint32_t b) {
            const EdgeKey key{std::min(a, b), std::max(a, b)};
            const auto it = midpointCache.find(key);
            if (it != midpointCache.end()) {
                return it->second;
            }

            const uint32_t m = static_cast<uint32_t>(positions.size());
            positions.push_back(vec3Scale(vec3Add(positions[a], positions[b]), 0.5f));

            if (!normals.empty()) {
                normals.push_back(vec3Normalize(vec3Add(normals[a], normals[b])));
            }
            if (!uvs.empty()) {
                uvs.push_back(lerpVec2(uvs[a], uvs[b]));
            }
            if (!tangents.empty()) {
                tangents.push_back(lerpVec4(tangents[a], tangents[b]));
            }
            if (!jointIndices.empty()) {
                jointIndices.push_back(jointIndices[a]);
                jointWeights.push_back(averageWeights(jointWeights[a], jointWeights[b]));
            }

            midpointCache.emplace(key, m);
            return m;
        };

        for (const Face& face : faces) {
            if (face.indices.size() != 3u) {
                nextFaces.push_back(face);
                continue;
            }

            const uint32_t i0 = face.indices[0];
            const uint32_t i1 = face.indices[1];
            const uint32_t i2 = face.indices[2];

            const float l01 = edgeLength(positions, i0, i1);
            const float l12 = edgeLength(positions, i1, i2);
            const float l20 = edgeLength(positions, i2, i0);

            if (std::max({l01, l12, l20}) <= splitThreshold) {
                nextFaces.push_back(face);
                continue;
            }

            anySplit = true;
            ++report.splitCount;

            if (l01 >= l12 && l01 >= l20) {
                const uint32_t m = midpointForEdge(i0, i1);
                nextFaces.push_back(Face{{i0, m, i2}});
                nextFaces.push_back(Face{{m, i1, i2}});
                continue;
            }

            if (l12 >= l01 && l12 >= l20) {
                const uint32_t m = midpointForEdge(i1, i2);
                nextFaces.push_back(Face{{i1, m, i0}});
                nextFaces.push_back(Face{{m, i2, i0}});
                continue;
            }

            const uint32_t m = midpointForEdge(i2, i0);
            nextFaces.push_back(Face{{i2, m, i1}});
            nextFaces.push_back(Face{{m, i0, i1}});
        }

        faces = std::move(nextFaces);
        if (!anySplit) {
            break;
        }
    }

    // ── Edge-collapse pass ────────────────────────────────────────────────────
    // Collapses edges shorter than (collapseEdgesBelow * targetEdgeLength).
    // Disabled when collapseEdgesBelow == 0 (default).
    if (desc.collapseEdgesBelow > 0.f && !faces.empty()) {
        const float collapseThreshold = desc.collapseEdgesBelow * desc.targetEdgeLength;

        for (uint32_t colIter = 0; colIter < desc.maxCollapseIterations; ++colIter) {
            // Build union-find remap over current vertex count
            const uint32_t nVerts = static_cast<uint32_t>(positions.size());
            std::vector<uint32_t> remap(nVerts);
            std::iota(remap.begin(), remap.end(), 0u);

            // Path-halving find
            auto findRoot = [&](uint32_t v) -> uint32_t {
                while (remap[v] != v) {
                    remap[v] = remap[remap[v]];
                    v = remap[v];
                }
                return v;
            };

            // Collect candidate edges below threshold (unique, sorted ascending)
            std::vector<std::pair<float, EdgeKey>> candidates;
            {
                std::set<EdgeKey> seen;
                for (const Face& face : faces) {
                    if (face.indices.size() != 3u) {
                        continue;
                    }
                    for (size_t i = 0; i < 3u; ++i) {
                        const uint32_t a = face.indices[i];
                        const uint32_t b = face.indices[(i + 1u) % 3u];
                        EdgeKey key{std::min(a, b), std::max(a, b)};
                        if (seen.insert(key).second) {
                            const float len = edgeLength(positions, key.v0, key.v1);
                            if (len < collapseThreshold) {
                                candidates.push_back({len, key});
                            }
                        }
                    }
                }
            }
            if (candidates.empty()) {
                break;
            }

            // Sort shortest-first for greedy quality
            std::sort(candidates.begin(), candidates.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });

            bool anyCollapse = false;
            for (const auto& [len, key] : candidates) {
                (void)len;
                const uint32_t a = findRoot(key.v0);
                const uint32_t b = findRoot(key.v1);
                if (a == b) {
                    continue;  // already merged by a prior collapse this iteration
                }

                const uint32_t keep = std::min(a, b);
                const uint32_t drop = std::max(a, b);

                // Merge drop -> keep; set keep's attributes to midpoint
                positions[keep] = vec3Scale(vec3Add(positions[keep], positions[drop]), 0.5f);
                if (!normals.empty()) {
                    normals[keep] = vec3Normalize(vec3Add(normals[keep], normals[drop]));
                }
                if (!uvs.empty()) {
                    uvs[keep] = lerpVec2(uvs[keep], uvs[drop]);
                }
                if (!tangents.empty()) {
                    tangents[keep] = lerpVec4(tangents[keep], tangents[drop]);
                }
                if (!jointIndices.empty()) {
                    jointWeights[keep] = averageWeights(jointWeights[keep], jointWeights[drop]);
                    // Keep joint index set from 'keep' vertex (no blend for topology)
                }

                remap[drop] = keep;
                anyCollapse = true;
                ++report.collapseCount;
            }

            if (!anyCollapse) {
                break;
            }

            // Apply remap, remove degenerate faces (two or more identical indices)
            std::vector<Face> collapsedFaces;
            collapsedFaces.reserve(faces.size());
            for (const Face& face : faces) {
                if (face.indices.size() != 3u) {
                    collapsedFaces.push_back(face);
                    continue;
                }
                const uint32_t r0 = findRoot(face.indices[0]);
                const uint32_t r1 = findRoot(face.indices[1]);
                const uint32_t r2 = findRoot(face.indices[2]);
                if (r0 == r1 || r1 == r2 || r0 == r2) {
                    continue;  // degenerate after collapse — discard
                }
                collapsedFaces.push_back(Face{{r0, r1, r2}});
            }
            faces = std::move(collapsedFaces);
        }
    }

    output.topology().clearFaces();
    for (const Face& face : faces) {
        output.topology().addFace(face);
    }
    output.attributes().setPositions(std::move(positions));

    if (!normals.empty()) {
        output.attributes().setNormals(std::move(normals));
    }
    if (!uvs.empty()) {
        output.attributes().setUVs(std::move(uvs));
    }
    if (!tangents.empty()) {
        output.attributes().setTangents(std::move(tangents));
    }
    if (!jointIndices.empty()) {
        output.attributes().setSkinning(std::move(jointIndices), std::move(jointWeights));
    }

    if (desc.recomputeNormals) {
        if (!output.computeVertexNormals()) {
            report.diagnostic = report.diagnostic | RemeshDiagnostic::NormalRebuildFailed;
            report.messages.push_back("Failed to rebuild normals after remesh");
        }
        output.attributes().clearTangents();
    }

    output.rebuildStableElementIds();
    report.outputFaceCount = static_cast<uint32_t>(output.topology().faceCount());

    if (report.splitCount == 0u && report.collapseCount == 0u) {
        report.diagnostic = report.diagnostic | RemeshDiagnostic::NoChangesApplied;
        report.messages.push_back("No edges exceeded remesh threshold; output remains unchanged");
    }

    if (!output.isValid()) {
        report.diagnostic = report.diagnostic | RemeshDiagnostic::OutputTopologyInvalid;
        report.messages.push_back("Remesh produced an invalid output mesh");
        report.valid = false;
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    if (report.diagnostic == RemeshDiagnostic::Success) {
        report.valid = true;
        return report;
    }

    report.diagnostic = report.diagnostic | RemeshDiagnostic::SuccessWithWarnings;
    report.valid = true;
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

} // namespace nexus::geometry
