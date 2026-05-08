#include <nexus/geometry/BevelChamfer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <span>
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

struct EdgeAdjacency {
    uint32_t vertexA = 0;
    uint32_t vertexB = 0;
    std::vector<uint32_t> faces;
};

struct SharpEdgeData {
    EdgeAdjacency adjacency;
    std::map<uint32_t, uint32_t> midpointByFace;
    uint32_t boundaryMidpoint = std::numeric_limits<uint32_t>::max();
};

struct FaceVertexKey {
    uint32_t face = 0;
    uint32_t vertex = 0;

    [[nodiscard]] bool operator<(const FaceVertexKey& other) const noexcept
    {
        if (face != other.face) {
            return face < other.face;
        }
        return vertex < other.vertex;
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

inline Vec3 vec3Cross(const Vec3& a, const Vec3& b) noexcept
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float vec3LengthSq(const Vec3& v) noexcept
{
    return vec3Dot(v, v);
}

inline Vec3 vec3Normalize(const Vec3& v) noexcept
{
    const float lenSq = vec3LengthSq(v);
    if (lenSq <= 1e-16f) {
        return {0.f, 0.f, 1.f};
    }
    const float invLen = 1.f / std::sqrt(lenSq);
    return vec3Scale(v, invLen);
}

Vec3 computeFaceNormal(const Face& face, const std::vector<Vec3>& positions) noexcept
{
    if (face.indices.size() < 3) {
        return {0.f, 0.f, 1.f};
    }

    const Vec3& p0 = positions[face.indices[0]];
    Vec3 sum{0.f, 0.f, 0.f};

    for (size_t i = 1; i + 1 < face.indices.size(); ++i) {
        const Vec3& p1 = positions[face.indices[i]];
        const Vec3& p2 = positions[face.indices[i + 1]];
        sum = vec3Add(sum, vec3Cross(vec3Sub(p1, p0), vec3Sub(p2, p0)));
    }

    return vec3Normalize(sum);
}

float degreesToRadians(float deg) noexcept
{
    return deg * 0.01745329251994329576923690768489f;
}

bool isFaceIndexReferenced(uint32_t faceIndex,
                           const std::vector<uint32_t>& refs) noexcept
{
    return std::find(refs.begin(), refs.end(), faceIndex) != refs.end();
}

uint32_t countUniqueIndices(const Face& face)
{
    std::vector<uint32_t> uniq = face.indices;
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    return static_cast<uint32_t>(uniq.size());
}

void appendSupportFaceIfValid(std::vector<Face>& outFaces,
                              std::span<const uint32_t> indices,
                              BevelChamferReport& report)
{
    Face f{};
    f.indices.assign(indices.begin(), indices.end());
    if (f.indices.size() < 3 || countUniqueIndices(f) < 3) {
        report.diagnostic = report.diagnostic | BevelChamferDiagnostic::GeneratedDegenerateFace;
        return;
    }
    outFaces.push_back(std::move(f));
    ++report.supportFaceCount;
}

void flagOutputNonManifoldIfNeeded(const std::vector<Face>& faces,
                                   BevelChamferReport& report)
{
    std::map<EdgeKey, uint32_t> edgeUseCount;
    for (const Face& face : faces) {
        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t a = face.indices[i];
            const uint32_t b = face.indices[(i + 1) % face.indices.size()];
            if (a == b) {
                report.diagnostic = report.diagnostic | BevelChamferDiagnostic::GeneratedDegenerateFace;
                continue;
            }
            const EdgeKey key{std::min(a, b), std::max(a, b)};
            ++edgeUseCount[key];
        }
    }

    for (const auto& [_, count] : edgeUseCount) {
        if (count > 2) {
            report.diagnostic = report.diagnostic | BevelChamferDiagnostic::OutputNonManifold;
            break;
        }
    }
}

} // namespace

BevelChamferReport BevelChamferOperation::apply(const Mesh& input,
                                                const BevelChamferDesc& desc,
                                                Mesh& output) noexcept
{
    BevelChamferReport report{};

    if (!input.isValid()) {
        report.diagnostic = BevelChamferDiagnostic::InvalidInputMesh;
        report.messages.push_back("Input mesh is invalid");
        return report;
    }

    if (desc.distance <= 0.f || !std::isfinite(desc.distance)) {
        report.diagnostic = BevelChamferDiagnostic::InvalidDistance;
        report.messages.push_back("distance must be finite and > 0");
        return report;
    }

    if (desc.sharpAngleDegrees <= 0.f || desc.sharpAngleDegrees >= 180.f
        || !std::isfinite(desc.sharpAngleDegrees)) {
        report.diagnostic = BevelChamferDiagnostic::InvalidSharpAngle;
        report.messages.push_back("sharpAngleDegrees must be finite and in (0, 180)");
        return report;
    }

    const auto& topo = input.topology();
    const auto& attrs = input.attributes();
    const auto& positions = attrs.positions();

    std::vector<Vec3> faceNormals;
    faceNormals.reserve(topo.faceCount());
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        faceNormals.push_back(computeFaceNormal(topo.face(fi), positions));
    }

    std::map<EdgeKey, EdgeAdjacency> edgeMap;

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& face = topo.face(fi);
        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t a = face.indices[i];
            const uint32_t b = face.indices[(i + 1) % face.indices.size()];
            if (a == b) {
                continue;
            }
            const uint32_t lo = std::min(a, b);
            const uint32_t hi = std::max(a, b);
            EdgeKey key{lo, hi};
            auto& entry = edgeMap[key];
            entry.vertexA = lo;
            entry.vertexB = hi;
            entry.faces.push_back(static_cast<uint32_t>(fi));
        }
    }

    const float cosThreshold = std::cos(degreesToRadians(desc.sharpAngleDegrees));

    std::map<EdgeKey, SharpEdgeData> sharpEdges;

    bool sawNonManifold = false;
    for (const auto& [_, edge] : edgeMap) {
        bool sharp = false;
        if (edge.faces.size() == 1) {
            sharp = desc.includeBoundaryEdges;
        } else if (edge.faces.size() == 2) {
            const Vec3& n0 = faceNormals[edge.faces[0]];
            const Vec3& n1 = faceNormals[edge.faces[1]];
            const float d = vec3Dot(n0, n1);
            sharp = d <= cosThreshold;
        } else if (!edge.faces.empty()) {
            sharp = true;
            sawNonManifold = true;
        }

        if (!sharp) {
            continue;
        }

        ++report.sharpEdgeCount;
        SharpEdgeData data{};
        data.adjacency = edge;
        sharpEdges.emplace(EdgeKey{edge.vertexA, edge.vertexB}, std::move(data));
    }

    if (sawNonManifold) {
        report.diagnostic = report.diagnostic | BevelChamferDiagnostic::NonManifoldTopology;
        report.messages.push_back("Non-manifold edges detected; continuing with conservative displacement");
    }

    if (report.sharpEdgeCount == 0) {
        output = input;
        report.diagnostic = report.diagnostic | BevelChamferDiagnostic::NoSharpEdgesDetected;
        report.messages.push_back("No sharp edges matched the threshold; output equals input");
        report.valid = true;
        return report;
    }

    const float modeScale = (desc.mode == BevelChamferMode::Bevel) ? 1.0f : 0.5f;

    std::vector<Vec3> newPositions = positions;
    std::vector<uint32_t> parentVertexByIndex(attrs.vertexCount());
    for (uint32_t i = 0; i < static_cast<uint32_t>(attrs.vertexCount()); ++i) {
        parentVertexByIndex[i] = i;
    }

    std::map<FaceVertexKey, uint32_t> duplicatedByFaceVertex;

    auto getOrCreateDuplicatedVertex = [&](uint32_t faceIndex, uint32_t vertexIndex) {
        const FaceVertexKey key{faceIndex, vertexIndex};
        const auto it = duplicatedByFaceVertex.find(key);
        if (it != duplicatedByFaceVertex.end()) {
            return it->second;
        }

        const Vec3 displaced = vec3Add(positions[vertexIndex],
                                       vec3Scale(faceNormals[faceIndex], desc.distance * modeScale));
        const uint32_t newIndex = static_cast<uint32_t>(newPositions.size());
        newPositions.push_back(displaced);
        parentVertexByIndex.push_back(vertexIndex);
        duplicatedByFaceVertex.emplace(key, newIndex);
        ++report.movedVertexCount;
        return newIndex;
    };

    for (auto& [edgeKey, sharp] : sharpEdges) {
        for (uint32_t faceIndex : sharp.adjacency.faces) {
            const uint32_t aDup = getOrCreateDuplicatedVertex(faceIndex, edgeKey.v0);
            const uint32_t bDup = getOrCreateDuplicatedVertex(faceIndex, edgeKey.v1);

            const uint32_t midIndex = static_cast<uint32_t>(newPositions.size());
            newPositions.push_back(vec3Scale(vec3Add(newPositions[aDup], newPositions[bDup]), 0.5f));
            parentVertexByIndex.push_back(edgeKey.v0);
            sharp.midpointByFace.emplace(faceIndex, midIndex);
            ++report.splitEdgeCount;
        }

        if (sharp.adjacency.faces.size() == 1) {
            sharp.boundaryMidpoint = static_cast<uint32_t>(newPositions.size());
            newPositions.push_back(vec3Scale(vec3Add(positions[edgeKey.v0], positions[edgeKey.v1]), 0.5f));
            parentVertexByIndex.push_back(edgeKey.v0);
        }
    }

    std::vector<Face> rewrittenFaces;
    rewrittenFaces.reserve(topo.faceCount() + sharpEdges.size());

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& srcFace = topo.face(fi);
        Face dst{};
        dst.indices.reserve(srcFace.indices.size() * 2u);

        for (size_t i = 0; i < srcFace.indices.size(); ++i) {
            const uint32_t oldU = srcFace.indices[i];
            const uint32_t oldV = srcFace.indices[(i + 1) % srcFace.indices.size()];
            const EdgeKey edgeKey{std::min(oldU, oldV), std::max(oldU, oldV)};

            uint32_t mappedU = oldU;
            const FaceVertexKey fvKey{static_cast<uint32_t>(fi), oldU};
            const auto dupIt = duplicatedByFaceVertex.find(fvKey);
            if (dupIt != duplicatedByFaceVertex.end()) {
                mappedU = dupIt->second;
            }

            dst.indices.push_back(mappedU);

            const auto sharpIt = sharpEdges.find(edgeKey);
            if (sharpIt != sharpEdges.end()
                && isFaceIndexReferenced(static_cast<uint32_t>(fi), sharpIt->second.adjacency.faces)) {
                const auto midIt = sharpIt->second.midpointByFace.find(static_cast<uint32_t>(fi));
                if (midIt != sharpIt->second.midpointByFace.end()) {
                    dst.indices.push_back(midIt->second);
                }
            }
        }

        rewrittenFaces.push_back(std::move(dst));
    }

    for (const auto& [edgeKey, sharp] : sharpEdges) {
        if (sharp.adjacency.faces.size() == 2) {
            const uint32_t f0 = sharp.adjacency.faces[0];
            const uint32_t f1 = sharp.adjacency.faces[1];

            const uint32_t a0 = duplicatedByFaceVertex.at(FaceVertexKey{f0, edgeKey.v0});
            const uint32_t b0 = duplicatedByFaceVertex.at(FaceVertexKey{f0, edgeKey.v1});
            const uint32_t a1 = duplicatedByFaceVertex.at(FaceVertexKey{f1, edgeKey.v0});
            const uint32_t b1 = duplicatedByFaceVertex.at(FaceVertexKey{f1, edgeKey.v1});
            const uint32_t m0 = sharp.midpointByFace.at(f0);
            const uint32_t m1 = sharp.midpointByFace.at(f1);

            const std::array<uint32_t, 6> support{{a0, m0, b0, b1, m1, a1}};
            appendSupportFaceIfValid(rewrittenFaces, support, report);
            continue;
        }

        if (sharp.adjacency.faces.size() == 1 && desc.includeBoundaryEdges) {
            const uint32_t f0 = sharp.adjacency.faces[0];
            const uint32_t a0 = duplicatedByFaceVertex.at(FaceVertexKey{f0, edgeKey.v0});
            const uint32_t b0 = duplicatedByFaceVertex.at(FaceVertexKey{f0, edgeKey.v1});
            const uint32_t m0 = sharp.midpointByFace.at(f0);
            const uint32_t mo = sharp.boundaryMidpoint;

            const std::array<uint32_t, 6> support{{a0, m0, b0, edgeKey.v1, mo, edgeKey.v0}};
            appendSupportFaceIfValid(rewrittenFaces, support, report);
        }
    }

    output = input;
    output.topology().clearFaces();
    for (const Face& face : rewrittenFaces) {
        output.topology().addFace(face);
    }
    output.attributes().setPositions(newPositions);

    if (attrs.hasUVs()) {
        std::vector<Vec2> uvs = attrs.uvs();
        uvs.reserve(newPositions.size());
        for (size_t i = attrs.vertexCount(); i < newPositions.size(); ++i) {
            uvs.push_back(attrs.uvs()[parentVertexByIndex[i]]);
        }
        output.attributes().setUVs(std::move(uvs));
    }

    if (attrs.hasSkinning()) {
        std::vector<JointIndex4> ji = attrs.jointIndices();
        std::vector<JointWeight4> jw = attrs.jointWeights();
        ji.reserve(newPositions.size());
        jw.reserve(newPositions.size());
        for (size_t i = attrs.vertexCount(); i < newPositions.size(); ++i) {
            const uint32_t parent = parentVertexByIndex[i];
            ji.push_back(attrs.jointIndices()[parent]);
            jw.push_back(attrs.jointWeights()[parent]);
        }
        output.attributes().setSkinning(std::move(ji), std::move(jw));
    }

    if (attrs.hasTangents()) {
        std::vector<Vec4> tangents = attrs.tangents();
        tangents.reserve(newPositions.size());
        for (size_t i = attrs.vertexCount(); i < newPositions.size(); ++i) {
            tangents.push_back(attrs.tangents()[parentVertexByIndex[i]]);
        }
        output.attributes().setTangents(std::move(tangents));
    }

    if (!desc.recomputeNormals && attrs.hasNormals()) {
        std::vector<Vec3> normals = attrs.normals();
        normals.reserve(newPositions.size());
        for (size_t i = attrs.vertexCount(); i < newPositions.size(); ++i) {
            normals.push_back(attrs.normals()[parentVertexByIndex[i]]);
        }
        output.attributes().setNormals(std::move(normals));
    }

    flagOutputNonManifoldIfNeeded(rewrittenFaces, report);

    if (!output.isValid()) {
        report.diagnostic = report.diagnostic | BevelChamferDiagnostic::GeneratedDegenerateFace;
        report.messages.push_back("Generated topology failed mesh validity checks");
    }

    if (desc.recomputeNormals) {
        if (!output.computeVertexNormals()) {
            report.diagnostic = report.diagnostic | BevelChamferDiagnostic::NormalRebuildFailed;
            report.messages.push_back("Failed to recompute normals");
        }
        // Tangents depend on normals; clear stale tangent channel after topology changes.
        output.attributes().clearTangents();
    }

    output.rebuildStableElementIds();

    if (report.diagnostic == BevelChamferDiagnostic::Success) {
        report.diagnostic = BevelChamferDiagnostic::Success;
    } else {
        report.diagnostic = report.diagnostic | BevelChamferDiagnostic::SuccessWithWarnings;
    }

    report.valid = true;
    return report;
}

} // namespace nexus::geometry
