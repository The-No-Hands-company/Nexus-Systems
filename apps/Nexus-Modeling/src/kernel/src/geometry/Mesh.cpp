#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/Tolerance.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <map>
#include <unordered_map>

#include <limits>

namespace nexus::geometry {

namespace {

bool isFiniteFloat(float value) noexcept
{
    constexpr std::uint32_t kExpMask = 0x7F800000u;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & kExpMask) != kExpMask;
}

nexus::render::Vec3 vec3Sub(const nexus::render::Vec3& a, const nexus::render::Vec3& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

nexus::render::Vec3 vec3Cross(const nexus::render::Vec3& a, const nexus::render::Vec3& b) noexcept
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

nexus::render::Vec3 vec3Normalize(const nexus::render::Vec3& v) noexcept
{
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq < 1e-12f) {
        return {0.f, 1.f, 0.f};
    }
    const float inv = 1.f / std::sqrt(lenSq);
    return {v.x * inv, v.y * inv, v.z * inv};
}

nexus::render::Vec3 vec3Add(const nexus::render::Vec3& a, const nexus::render::Vec3& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

float vec3Dot(const nexus::render::Vec3& a, const nexus::render::Vec3& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float vec3Length(const nexus::render::Vec3& v) noexcept
{
    return std::sqrt(vec3Dot(v, v));
}

nexus::render::Vec3 vec3Scale(const nexus::render::Vec3& v, float s) noexcept
{
    return {v.x * s, v.y * s, v.z * s};
}

float clamp01Signed(float x) noexcept
{
    if (x < -1.f) return -1.f;
    if (x > 1.f) return 1.f;
    return x;
}

float cornerAngle(const nexus::render::Vec3& edgeA,
                  const nexus::render::Vec3& edgeB) noexcept
{
    const float lenA = vec3Length(edgeA);
    const float lenB = vec3Length(edgeB);
    if (lenA < 1e-12f || lenB < 1e-12f) {
        return 0.f;
    }

    const float c = clamp01Signed(vec3Dot(edgeA, edgeB) / (lenA * lenB));
    return std::acos(c);
}

nexus::render::Vec3 vec3Orthonormalize(const nexus::render::Vec3& tangent,
                                       const nexus::render::Vec3& normal) noexcept
{
    const nexus::render::Vec3 projected = vec3Sub(tangent, vec3Scale(normal, vec3Dot(normal, tangent)));
    return vec3Normalize(projected);
}

nexus::render::Vec3 transformDirection(const nexus::render::Mat4& m,
                                       const nexus::render::Vec3& v) noexcept
{
    const nexus::render::Vec4 transformed = m * nexus::render::Vec4{v.x, v.y, v.z, 0.f};
    return nexus::render::Vec3{transformed.x, transformed.y, transformed.z}.normalize();
}

nexus::render::Vec3 transformPoint(const nexus::render::Mat4& m,
                                   const nexus::render::Vec3& v) noexcept
{
    const nexus::render::Vec4 transformed = m * nexus::render::Vec4{v.x, v.y, v.z, 1.f};
    return {transformed.x, transformed.y, transformed.z};
}

struct UndirectedEdge {
    uint32_t v0 = 0;
    uint32_t v1 = 0;
};

UndirectedEdge makeUndirectedEdge(uint32_t a, uint32_t b) noexcept
{
    if (a < b) {
        return {a, b};
    }
    return {b, a};
}

bool edgeLess(const UndirectedEdge& a, const UndirectedEdge& b) noexcept
{
    if (a.v0 != b.v0) {
        return a.v0 < b.v0;
    }
    return a.v1 < b.v1;
}

using EdgeIdMap = std::map<UndirectedEdge, MeshElementID, decltype(&edgeLess)>;

// Compute face normal for a triangle given three positions.
nexus::render::Vec3 faceNormal(const nexus::render::Vec3& p0,
                               const nexus::render::Vec3& p1,
                               const nexus::render::Vec3& p2) noexcept
{
    return vec3Normalize(vec3Cross(vec3Sub(p1, p0), vec3Sub(p2, p0)));
}

bool nearlyEqual(float a, float b, float epsilon) noexcept
{
    return std::abs(a - b) <= epsilon;
}

bool vec2NearlyEqual(const Vec2& a, const Vec2& b, float epsilon) noexcept
{
    return nearlyEqual(a.u, b.u, epsilon) && nearlyEqual(a.v, b.v, epsilon);
}

bool vec3NearlyEqual(const nexus::render::Vec3& a,
                     const nexus::render::Vec3& b,
                     float epsilon) noexcept
{
    return nearlyEqual(a.x, b.x, epsilon)
        && nearlyEqual(a.y, b.y, epsilon)
        && nearlyEqual(a.z, b.z, epsilon);
}

bool vec4NearlyEqual(const Vec4& a, const Vec4& b, float epsilon) noexcept
{
    return nearlyEqual(a.x, b.x, epsilon)
        && nearlyEqual(a.y, b.y, epsilon)
        && nearlyEqual(a.z, b.z, epsilon)
        && nearlyEqual(a.w, b.w, epsilon);
}

bool skinWeightsNearlyEqual(const JointWeight4& a,
                            const JointWeight4& b,
                            float epsilon) noexcept
{
    for (size_t i = 0; i < kMaxSkinInfluencesPerVertex; ++i) {
        if (!nearlyEqual(a[i], b[i], epsilon)) {
            return false;
        }
    }
    return true;
}

bool optionalChannelsMatch(const MeshAttributes& a, const MeshAttributes& b) noexcept
{
    return a.hasNormals() == b.hasNormals()
        && a.hasTangents() == b.hasTangents()
        && a.hasUVs() == b.hasUVs()
        && a.hasSkinning() == b.hasSkinning();
}

bool vertexRowsEquivalent(const MeshAttributes& attributes,
                          size_t a,
                          size_t b,
                          float epsilon) noexcept
{
    if (!coincident(attributes.positions()[a], attributes.positions()[b], Tolerance{epsilon, 0.f})) {
        return false;
    }
    if (attributes.hasNormals()
        && !vec3NearlyEqual(attributes.normals()[a], attributes.normals()[b], epsilon)) {
        return false;
    }
    if (attributes.hasTangents()
        && !vec4NearlyEqual(attributes.tangents()[a], attributes.tangents()[b], epsilon)) {
        return false;
    }
    if (attributes.hasUVs() && !vec2NearlyEqual(attributes.uvs()[a], attributes.uvs()[b], epsilon)) {
        return false;
    }
    if (attributes.hasSkinning()) {
        if (attributes.jointIndices()[a] != attributes.jointIndices()[b]) {
            return false;
        }
        if (!skinWeightsNearlyEqual(attributes.jointWeights()[a], attributes.jointWeights()[b], epsilon)) {
            return false;
        }
    }
    return true;
}

template <typename T>
std::vector<T> compactVertexBuffer(const std::vector<T>& source,
                                   const std::vector<uint32_t>& survivorOldIndices)
{
    std::vector<T> compacted;
    compacted.reserve(survivorOldIndices.size());
    for (const uint32_t oldIndex : survivorOldIndices) {
        compacted.push_back(source[oldIndex]);
    }
    return compacted;
}

EdgeIdMap buildStableEdgeIdMap(const MeshTopology& topology,
                               std::span<const MeshElementID> edgeIds)
{
    EdgeIdMap stableEdgeIds(&edgeLess);
    for (size_t fi = 0; fi < topology.faceCount(); ++fi) {
        const Face& face = topology.face(fi);
        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t a = face.indices[i];
            const uint32_t b = face.indices[(i + 1) % face.indices.size()];
            if (a == b) {
                continue;
            }

            stableEdgeIds.emplace(makeUndirectedEdge(a, b), MeshElementID{0});
        }
    }

    if (stableEdgeIds.size() != edgeIds.size()) {
        return EdgeIdMap(&edgeLess);
    }

    size_t edgeIndex = 0;
    for (auto& [_, id] : stableEdgeIds) {
        id = edgeIds[edgeIndex++];
    }
    return stableEdgeIds;
}

StableMeshElementIds preserveStableIdsAfterVertexWeld(const MeshTopology& oldTopology,
                                                      const StableMeshElementIds& oldIds,
                                                      const std::vector<uint32_t>& canonicalOldIndices,
                                                      const std::vector<uint32_t>& survivorOldIndices,
                                                      const std::vector<Face>& compactedFaces)
{
    StableMeshElementIds preserved{};

    if (oldIds.vertexIds.size() == canonicalOldIndices.size()) {
        preserved.vertexIds.reserve(survivorOldIndices.size());
        for (const uint32_t oldIndex : survivorOldIndices) {
            preserved.vertexIds.push_back(oldIds.vertexIds[oldIndex]);
        }
    }

    if (oldIds.faceIds.size() == oldTopology.faceCount()) {
        preserved.faceIds = oldIds.faceIds;
    }

    if (oldIds.edgeIds.empty()) {
        return preserved;
    }

    const EdgeIdMap oldEdgeIds = buildStableEdgeIdMap(oldTopology, oldIds.edgeIds);
    if (oldEdgeIds.empty() && !oldIds.edgeIds.empty()) {
        preserved.edgeIds.clear();
        return preserved;
    }

    EdgeIdMap canonicalOldEdgeIds(&edgeLess);
    for (size_t fi = 0; fi < oldTopology.faceCount(); ++fi) {
        const Face& face = oldTopology.face(fi);
        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t a = face.indices[i];
            const uint32_t b = face.indices[(i + 1) % face.indices.size()];
            if (a == b) {
                continue;
            }

            const UndirectedEdge oldEdge = makeUndirectedEdge(a, b);
            const UndirectedEdge weldedOldEdge = makeUndirectedEdge(canonicalOldIndices[a], canonicalOldIndices[b]);
            const auto oldIt = oldEdgeIds.find(oldEdge);
            if (oldIt != oldEdgeIds.end()) {
                canonicalOldEdgeIds.emplace(weldedOldEdge, oldIt->second);
            }
        }
    }

    MeshElementID nextEdgeId = 1;
    for (const auto& [_, id] : oldEdgeIds) {
        nextEdgeId = std::max(nextEdgeId, id + 1u);
    }

    EdgeIdMap compactedEdgeIds(&edgeLess);
    for (const Face& face : compactedFaces) {
        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t a = face.indices[i];
            const uint32_t b = face.indices[(i + 1) % face.indices.size()];
            const UndirectedEdge compactedEdge = makeUndirectedEdge(a, b);
            if (compactedEdgeIds.find(compactedEdge) != compactedEdgeIds.end()) {
                continue;
            }

            const UndirectedEdge oldSurvivorEdge = makeUndirectedEdge(
                survivorOldIndices[a],
                survivorOldIndices[b]);
            const auto preservedIt = canonicalOldEdgeIds.find(oldSurvivorEdge);
            if (preservedIt != canonicalOldEdgeIds.end()) {
                compactedEdgeIds.emplace(compactedEdge, preservedIt->second);
                continue;
            }

            compactedEdgeIds.emplace(compactedEdge, nextEdgeId++);
        }
    }

    preserved.edgeIds.reserve(compactedEdgeIds.size());
    for (const auto& [_, id] : compactedEdgeIds) {
        preserved.edgeIds.push_back(id);
    }

    return preserved;
}

StableMeshElementIds preserveStableIdsAfterAppend(const Mesh& original,
                                                  const Mesh& appended,
                                                  uint32_t vertexOffset,
                                                  uint32_t faceOffset)
{
    StableMeshElementIds preserved{};
    const StableMeshElementIds& originalIds = original.stableElementIds();

    if (originalIds.vertexIds.size() == original.attributes().vertexCount()) {
        preserved.vertexIds = originalIds.vertexIds;
    }
    if (originalIds.faceIds.size() == original.topology().faceCount()) {
        preserved.faceIds = originalIds.faceIds;
    }
    if (originalIds.edgeIds.size() > 0) {
        preserved.edgeIds = originalIds.edgeIds;
    }

    MeshElementID nextVertexId = 1u;
    for (const MeshElementID id : preserved.vertexIds) {
        nextVertexId = std::max(nextVertexId, id + 1u);
    }
    for (size_t i = 0; i < appended.attributes().vertexCount(); ++i) {
        preserved.vertexIds.push_back(nextVertexId++);
    }

    MeshElementID nextFaceId = 1u;
    for (const MeshElementID id : preserved.faceIds) {
        nextFaceId = std::max(nextFaceId, id + 1u);
    }
    for (size_t i = 0; i < appended.topology().faceCount(); ++i) {
        preserved.faceIds.push_back(nextFaceId++);
    }

    MeshElementID nextEdgeId = 1u;
    for (const MeshElementID id : preserved.edgeIds) {
        nextEdgeId = std::max(nextEdgeId, id + 1u);
    }

    EdgeIdMap appendedEdges(&edgeLess);
    for (size_t fi = 0; fi < appended.topology().faceCount(); ++fi) {
        const Face& face = appended.topology().face(fi);
        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t a = vertexOffset + face.indices[i];
            const uint32_t b = vertexOffset + face.indices[(i + 1) % face.indices.size()];
            if (a == b) {
                continue;
            }
            appendedEdges.emplace(makeUndirectedEdge(a, b), MeshElementID{0});
        }
    }
    for (auto& [_, id] : appendedEdges) {
        id = nextEdgeId++;
        preserved.edgeIds.push_back(id);
    }

    (void)faceOffset;
    return preserved;
}

StableMeshElementIds preserveStableIdsForExtractedRange(const Mesh& source,
                                                        const std::vector<uint32_t>& selectedFaceIndices,
                                                        const std::vector<uint32_t>& usedVertexIndices,
                                                        const std::vector<Face>& extractedFaces)
{
    StableMeshElementIds preserved{};
    if (!source.hasStableElementIds()) {
        return preserved;
    }

    const StableMeshElementIds& sourceIds = source.stableElementIds();

    if (sourceIds.vertexIds.size() == source.attributes().vertexCount()) {
        preserved.vertexIds.reserve(usedVertexIndices.size());
        for (const uint32_t oldIndex : usedVertexIndices) {
            preserved.vertexIds.push_back(sourceIds.vertexIds[oldIndex]);
        }
    }

    if (sourceIds.faceIds.size() == source.topology().faceCount()) {
        preserved.faceIds.reserve(selectedFaceIndices.size());
        for (const uint32_t oldFaceIndex : selectedFaceIndices) {
            preserved.faceIds.push_back(sourceIds.faceIds[oldFaceIndex]);
        }
    }

    if (sourceIds.edgeIds.empty()) {
        return preserved;
    }

    const EdgeIdMap sourceEdgeIds = buildStableEdgeIdMap(source.topology(), sourceIds.edgeIds);
    if (sourceEdgeIds.empty() && !sourceIds.edgeIds.empty()) {
        return preserved;
    }

    std::vector<uint32_t> newToOldVertex(usedVertexIndices.begin(), usedVertexIndices.end());
    EdgeIdMap extractedEdgeIds(&edgeLess);
    for (const Face& face : extractedFaces) {
        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t oldA = newToOldVertex[face.indices[i]];
            const uint32_t oldB = newToOldVertex[face.indices[(i + 1) % face.indices.size()]];
            const auto it = sourceEdgeIds.find(makeUndirectedEdge(oldA, oldB));
            if (it != sourceEdgeIds.end()) {
                extractedEdgeIds.emplace(makeUndirectedEdge(face.indices[i], face.indices[(i + 1) % face.indices.size()]), it->second);
            }
        }
    }

    preserved.edgeIds.reserve(extractedEdgeIds.size());
    for (const auto& [_, id] : extractedEdgeIds) {
        preserved.edgeIds.push_back(id);
    }

    return preserved;
}

bool buildMeshFromFaceSelection(const Mesh& source,
                                std::span<const uint32_t> selectedFaceIndices,
                                Mesh& out,
                                StableMeshElementIds* preservedIds = nullptr) noexcept
{
    if (!source.isValid() || selectedFaceIndices.empty()) {
        return false;
    }

    std::vector<uint8_t> faceSelected(source.topology().faceCount(), 0u);
    std::vector<uint8_t> vertexUsed(source.attributes().vertexCount(), 0u);
    std::vector<uint32_t> orderedUsedVertices;
    orderedUsedVertices.reserve(source.attributes().vertexCount());

    for (const uint32_t faceIndex : selectedFaceIndices) {
        if (static_cast<size_t>(faceIndex) >= source.topology().faceCount()) {
            return false;
        }
        if (faceSelected[faceIndex] != 0u) {
            return false;
        }
        faceSelected[faceIndex] = 1u;

        const Face& face = source.topology().face(faceIndex);
        for (const uint32_t vertexIndex : face.indices) {
            if (vertexUsed[vertexIndex] != 0u) {
                continue;
            }
            vertexUsed[vertexIndex] = 1u;
            orderedUsedVertices.push_back(vertexIndex);
        }
    }

    std::vector<uint32_t> oldToNewVertex(source.attributes().vertexCount(), std::numeric_limits<uint32_t>::max());
    for (size_t newIndex = 0; newIndex < orderedUsedVertices.size(); ++newIndex) {
        oldToNewVertex[orderedUsedVertices[newIndex]] = static_cast<uint32_t>(newIndex);
    }

    Mesh extracted;
    extracted.attributes().setPositions(compactVertexBuffer(source.attributes().positions(), orderedUsedVertices));
    if (source.attributes().hasNormals()) {
        extracted.attributes().setNormals(compactVertexBuffer(source.attributes().normals(), orderedUsedVertices));
    }
    if (source.attributes().hasTangents()) {
        extracted.attributes().setTangents(compactVertexBuffer(source.attributes().tangents(), orderedUsedVertices));
    }
    if (source.attributes().hasUVs()) {
        extracted.attributes().setUVs(compactVertexBuffer(source.attributes().uvs(), orderedUsedVertices));
    }
    if (source.attributes().hasSkinning()) {
        extracted.attributes().setSkinning(
            compactVertexBuffer(source.attributes().jointIndices(), orderedUsedVertices),
            compactVertexBuffer(source.attributes().jointWeights(), orderedUsedVertices));
    }

    std::vector<Face> extractedFaces;
    extractedFaces.reserve(selectedFaceIndices.size());
    std::vector<uint32_t> selectedFaceVector(selectedFaceIndices.begin(), selectedFaceIndices.end());
    for (const uint32_t faceIndex : selectedFaceIndices) {
        Face remapped = source.topology().face(faceIndex);
        for (uint32_t& index : remapped.indices) {
            index = oldToNewVertex[index];
        }
        extracted.topology().addFace(remapped);
        extractedFaces.push_back(std::move(remapped));
    }

    if (preservedIds != nullptr) {
        preservedIds->vertexIds.clear();
        preservedIds->faceIds.clear();
        preservedIds->edgeIds.clear();
        if (source.hasStableElementIds()) {
            *preservedIds = preserveStableIdsForExtractedRange(
                source,
                selectedFaceVector,
                orderedUsedVertices,
                extractedFaces);
        }
    }

    out = std::move(extracted);
    return out.isValid();
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  MeshAttributes
// ─────────────────────────────────────────────────────────────────────────────

void MeshAttributes::setPositions(std::vector<nexus::render::Vec3> positions)
{
    m_positions = std::move(positions);
}

void MeshAttributes::setNormals(std::vector<nexus::render::Vec3> normals)
{
    m_normals = std::move(normals);
}

void MeshAttributes::setTangents(std::vector<Vec4> tangents)
{
    m_tangents = std::move(tangents);
}

void MeshAttributes::setUVs(std::vector<Vec2> uvs)
{
    m_uvs = std::move(uvs);
}

void MeshAttributes::setSkinning(std::vector<JointIndex4> jointIndices,
                                 std::vector<JointWeight4> jointWeights)
{
    m_jointIndices = std::move(jointIndices);
    m_jointWeights = std::move(jointWeights);
}

void MeshAttributes::clearSkinning() noexcept
{
    m_jointIndices.clear();
    m_jointWeights.clear();
}

bool MeshAttributes::hasSkinning() const noexcept
{
    return !m_jointIndices.empty() || !m_jointWeights.empty();
}

void MeshAttributes::normalizeSkinningWeights() noexcept
{
    if (m_jointWeights.empty()) {
        return;
    }

    for (auto& w4 : m_jointWeights) {
        float sum = 0.f;
        for (const float w : w4) {
            sum += std::max(0.f, w);
        }

        if (sum <= std::numeric_limits<float>::epsilon()) {
            continue;
        }

        for (float& w : w4) {
            w = std::max(0.f, w) / sum;
        }
    }
}

bool MeshAttributes::skinningJointIndicesValid(size_t skeletonBoneCount) const noexcept
{
    if (!hasSkinning()) {
        return true;
    }

    for (const auto& idx4 : m_jointIndices) {
        for (const uint16_t idx : idx4) {
            if (static_cast<size_t>(idx) >= skeletonBoneCount) {
                return false;
            }
        }
    }
    return true;
}

PackedSkinningStreams MeshAttributes::packSkinningStreams() const
{
    PackedSkinningStreams packed{};
    const size_t vertexCount = m_jointIndices.size();
    if (vertexCount == 0 || m_jointWeights.size() != vertexCount) {
        return packed;
    }

    packed.jointIndices.reserve(vertexCount * kMaxSkinInfluencesPerVertex);
    packed.jointWeights.reserve(vertexCount * kMaxSkinInfluencesPerVertex);

    for (size_t v = 0; v < vertexCount; ++v) {
        for (uint32_t i = 0; i < kMaxSkinInfluencesPerVertex; ++i) {
            packed.jointIndices.push_back(m_jointIndices[v][i]);
            packed.jointWeights.push_back(m_jointWeights[v][i]);
        }
    }

    return packed;
}

bool MeshAttributes::isConsistent() const noexcept
{
    const size_t n = m_positions.size();
    if (!m_normals.empty() && m_normals.size() != n) return false;
    if (!m_tangents.empty() && m_tangents.size() != n) return false;
    if (!m_uvs.empty()     && m_uvs.size()     != n) return false;
    if (!m_jointIndices.empty() && m_jointIndices.size() != n) return false;
    if (!m_jointWeights.empty() && m_jointWeights.size() != n) return false;
    if (m_jointIndices.size() != m_jointWeights.size()) return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MeshTopology
// ─────────────────────────────────────────────────────────────────────────────

void MeshTopology::addFace(Face face)
{
    m_faces.push_back(std::move(face));
}

bool MeshTopology::hasValidIndices(size_t vertexCount) const noexcept
{
    for (const auto& f : m_faces) {
        for (const uint32_t idx : f.indices) {
            if (static_cast<size_t>(idx) >= vertexCount) {
                return false;
            }
        }
    }
    return true;
}

bool MeshTopology::allFacesArePoly3Plus() const noexcept
{
    for (const auto& f : m_faces) {
        if (f.vertexCount() < 3) {
            return false;
        }
    }
    return true;
}

size_t MeshTopology::triangulate()
{
    std::vector<Face> result;
    result.reserve(m_faces.size());

    for (const auto& f : m_faces) {
        const size_t n = f.vertexCount();
        if (n < 3) {
            // Degenerate: keep as-is; isValid() will catch it.
            result.push_back(f);
            continue;
        }
        if (n == 3) {
            result.push_back(f);
            continue;
        }
        // Fan triangulation: pivot at index 0.
        for (size_t i = 1; i + 1 < n; ++i) {
            Face tri{};
            tri.indices = {f.indices[0], f.indices[i], f.indices[i + 1]};
            result.push_back(std::move(tri));
        }
    }

    m_faces = std::move(result);
    return m_faces.size();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mesh
// ─────────────────────────────────────────────────────────────────────────────

bool Mesh::isValid() const noexcept
{
    if (!m_attributes.hasPositions()) return false;
    if (!m_attributes.isConsistent()) return false;
    if (!m_topology.allFacesArePoly3Plus()) return false;
    if (!m_topology.hasValidIndices(m_attributes.vertexCount())) return false;
    return true;
}

bool Mesh::computeVertexNormals()
{
    if (!m_attributes.hasPositions()) return false;
    if (!m_topology.hasValidIndices(m_attributes.vertexCount())) return false;

    const auto& positions = m_attributes.positions();
    const size_t numVerts = positions.size();

    std::vector<nexus::render::Vec3> accumulated(numVerts, {0.f, 0.f, 0.f});

    for (size_t fi = 0; fi < m_topology.faceCount(); ++fi) {
        const Face& face = m_topology.face(fi);
        if (face.vertexCount() < 3) continue;

        // Compute face normal from first triangle of this face.
        const nexus::render::Vec3 n = faceNormal(
            positions[face.indices[0]],
            positions[face.indices[1]],
            positions[face.indices[2]]);

        for (const uint32_t idx : face.indices) {
            accumulated[idx] = vec3Add(accumulated[idx], n);
        }
    }

    std::vector<nexus::render::Vec3> normals(numVerts);
    for (size_t i = 0; i < numVerts; ++i) {
        normals[i] = vec3Normalize(accumulated[i]);
    }

    m_attributes.setNormals(std::move(normals));
    return true;
}

bool Mesh::computeVertexTangents()
{
    if (!m_attributes.hasPositions() || !m_attributes.hasNormals() || !m_attributes.hasUVs()) return false;
    if (!m_topology.hasValidIndices(m_attributes.vertexCount())) return false;

    const auto& positions = m_attributes.positions();
    const auto& normals = m_attributes.normals();
    const auto& uvs = m_attributes.uvs();
    const size_t numVerts = positions.size();

    std::vector<nexus::render::Vec3> accumulatedTangents(numVerts, {0.f, 0.f, 0.f});
    std::vector<nexus::render::Vec3> accumulatedBitangents(numVerts, {0.f, 0.f, 0.f});
    bool accumulatedAny = false;

    for (size_t fi = 0; fi < m_topology.faceCount(); ++fi) {
        const Face& face = m_topology.face(fi);
        if (face.vertexCount() < 3) {
            continue;
        }

        for (size_t i = 1; i + 1 < face.vertexCount(); ++i) {
            const uint32_t i0 = face.indices[0];
            const uint32_t i1 = face.indices[i];
            const uint32_t i2 = face.indices[i + 1];

            const auto& p0 = positions[i0];
            const auto& p1 = positions[i1];
            const auto& p2 = positions[i2];
            const auto& uv0 = uvs[i0];
            const auto& uv1 = uvs[i1];
            const auto& uv2 = uvs[i2];

            const nexus::render::Vec3 edge1 = vec3Sub(p1, p0);
            const nexus::render::Vec3 edge2 = vec3Sub(p2, p0);
            const float du1 = uv1.u - uv0.u;
            const float dv1 = uv1.v - uv0.v;
            const float du2 = uv2.u - uv0.u;
            const float dv2 = uv2.v - uv0.v;
            const float det = du1 * dv2 - du2 * dv1;
            if (std::abs(det) < 1e-8f) {
                continue;
            }

            const nexus::render::Vec3 faceCross = vec3Cross(edge1, edge2);
            const float areaWeight = 0.5f * vec3Length(faceCross);
            if (areaWeight < 1e-12f) {
                continue;
            }

            const float angle0 = cornerAngle(vec3Sub(p1, p0), vec3Sub(p2, p0));
            const float angle1 = cornerAngle(vec3Sub(p2, p1), vec3Sub(p0, p1));
            const float angle2 = cornerAngle(vec3Sub(p0, p2), vec3Sub(p1, p2));

            const float invDet = 1.f / det;
            const nexus::render::Vec3 tangent = {
                invDet * (dv2 * edge1.x - dv1 * edge2.x),
                invDet * (dv2 * edge1.y - dv1 * edge2.y),
                invDet * (dv2 * edge1.z - dv1 * edge2.z),
            };
            const nexus::render::Vec3 bitangent = {
                invDet * (-du2 * edge1.x + du1 * edge2.x),
                invDet * (-du2 * edge1.y + du1 * edge2.y),
                invDet * (-du2 * edge1.z + du1 * edge2.z),
            };

            const float weight0 = areaWeight * angle0;
            const float weight1 = areaWeight * angle1;
            const float weight2 = areaWeight * angle2;

            accumulatedTangents[i0] = vec3Add(accumulatedTangents[i0], vec3Scale(tangent, weight0));
            accumulatedTangents[i1] = vec3Add(accumulatedTangents[i1], vec3Scale(tangent, weight1));
            accumulatedTangents[i2] = vec3Add(accumulatedTangents[i2], vec3Scale(tangent, weight2));

            accumulatedBitangents[i0] = vec3Add(accumulatedBitangents[i0], vec3Scale(bitangent, weight0));
            accumulatedBitangents[i1] = vec3Add(accumulatedBitangents[i1], vec3Scale(bitangent, weight1));
            accumulatedBitangents[i2] = vec3Add(accumulatedBitangents[i2], vec3Scale(bitangent, weight2));
            accumulatedAny = true;
        }
    }

    if (!accumulatedAny) {
        return false;
    }

    std::vector<Vec4> tangents(numVerts);
    for (size_t i = 0; i < numVerts; ++i) {
        const nexus::render::Vec3 ortho = vec3Orthonormalize(accumulatedTangents[i], normals[i]);
        const nexus::render::Vec3 referenceBitangent = accumulatedBitangents[i];
        const nexus::render::Vec3 derivedBitangent = vec3Cross(normals[i], ortho);
        const float handedness = vec3Dot(derivedBitangent, referenceBitangent) < 0.f ? -1.f : 1.f;
        tangents[i] = {ortho.x, ortho.y, ortho.z, handedness};
    }

    m_attributes.setTangents(std::move(tangents));
    return true;
}

bool Mesh::applyTransform(const nexus::render::Mat4& transform) noexcept
{
    if (!m_attributes.hasPositions()) {
        return false;
    }

    auto positions = m_attributes.positions();
    for (auto& position : positions) {
        position = transformPoint(transform, position);
    }
    m_attributes.setPositions(std::move(positions));

    if (m_attributes.hasNormals()) {
        auto normals = m_attributes.normals();
        for (auto& normal : normals) {
            normal = transformDirection(transform, normal);
        }
        m_attributes.setNormals(std::move(normals));
    }

    if (m_attributes.hasTangents()) {
        auto tangents = m_attributes.tangents();
        for (auto& tangent : tangents) {
            const nexus::render::Vec3 direction = transformDirection(transform, {tangent.x, tangent.y, tangent.z});
            tangent.x = direction.x;
            tangent.y = direction.y;
            tangent.z = direction.z;
        }
        m_attributes.setTangents(std::move(tangents));
    }

    return true;
}

nexus::render::Aabb Mesh::computeBounds() const noexcept
{
    const auto& positions = m_attributes.positions();
    if (positions.empty()) {
        return {}; // no positions -> zero box (min == max)
    }

    nexus::render::Vec3 lo = positions.front();
    nexus::render::Vec3 hi = positions.front();
    for (const auto& p : positions) {
        if (!isFiniteFloat(p.x) || !isFiniteFloat(p.y) || !isFiniteFloat(p.z)) {
            return {}; // a non-finite vertex makes the bounds meaningless
        }
        lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
    }
    return nexus::render::Aabb{ lo, hi };
}

bool Mesh::appendMesh(const Mesh& other) noexcept
{
    if (!isValid() || !other.isValid()) {
        return false;
    }

    if (!optionalChannelsMatch(m_attributes, other.attributes())) {
        return false;
    }

    const uint32_t vertexOffset = static_cast<uint32_t>(m_attributes.vertexCount());
    const uint32_t faceOffset = static_cast<uint32_t>(m_topology.faceCount());
    const bool preserveIds = hasStableElementIds();
    const Mesh original = *this;

    auto positions = m_attributes.positions();
    positions.insert(positions.end(), other.attributes().positions().begin(), other.attributes().positions().end());
    m_attributes.setPositions(std::move(positions));

    if (m_attributes.hasNormals()) {
        auto normals = m_attributes.normals();
        normals.insert(normals.end(), other.attributes().normals().begin(), other.attributes().normals().end());
        m_attributes.setNormals(std::move(normals));
    }
    if (m_attributes.hasTangents()) {
        auto tangents = m_attributes.tangents();
        tangents.insert(tangents.end(), other.attributes().tangents().begin(), other.attributes().tangents().end());
        m_attributes.setTangents(std::move(tangents));
    }
    if (m_attributes.hasUVs()) {
        auto uvs = m_attributes.uvs();
        uvs.insert(uvs.end(), other.attributes().uvs().begin(), other.attributes().uvs().end());
        m_attributes.setUVs(std::move(uvs));
    }
    if (m_attributes.hasSkinning()) {
        auto jointIndices = m_attributes.jointIndices();
        auto jointWeights = m_attributes.jointWeights();
        jointIndices.insert(jointIndices.end(), other.attributes().jointIndices().begin(), other.attributes().jointIndices().end());
        jointWeights.insert(jointWeights.end(), other.attributes().jointWeights().begin(), other.attributes().jointWeights().end());
        m_attributes.setSkinning(std::move(jointIndices), std::move(jointWeights));
    }

    for (size_t fi = 0; fi < other.topology().faceCount(); ++fi) {
        Face face = other.topology().face(fi);
        for (uint32_t& index : face.indices) {
            index += vertexOffset;
        }
        m_topology.addFace(std::move(face));
    }

    if (!preserveIds) {
        return true;
    }

    m_stableElementIds = preserveStableIdsAfterAppend(original, other, vertexOffset, faceOffset);
    if (m_stableElementIds.vertexIds.size() != m_attributes.vertexCount()
        || m_stableElementIds.faceIds.size() != m_topology.faceCount()) {
        rebuildStableElementIds();
    }

    return true;
}

bool Mesh::extractFaceRange(size_t firstFace, size_t faceCount, Mesh& out) const noexcept
{
    if (!isValid()) {
        return false;
    }
    if (faceCount == 0) {
        return false;
    }
    if (firstFace >= m_topology.faceCount() || firstFace + faceCount > m_topology.faceCount()) {
        return false;
    }

    std::vector<uint32_t> selectedFaceIndices;
    selectedFaceIndices.reserve(faceCount);
    for (size_t faceIndex = firstFace; faceIndex < firstFace + faceCount; ++faceIndex) {
        selectedFaceIndices.push_back(static_cast<uint32_t>(faceIndex));
    }

    StableMeshElementIds preservedIds;
    if (!buildMeshFromFaceSelection(*this, selectedFaceIndices, out, &preservedIds)) {
        return false;
    }
    if (hasStableElementIds()) {
        out.m_stableElementIds = std::move(preservedIds);
        if (out.m_stableElementIds.vertexIds.size() != out.attributes().vertexCount()
            || out.m_stableElementIds.faceIds.size() != out.topology().faceCount()) {
            out.rebuildStableElementIds();
        }
    }
    return true;
}

bool Mesh::splitFaceRange(size_t firstFace, size_t faceCount, Mesh& out) noexcept
{
    if (!isValid()) {
        return false;
    }
    if (faceCount == 0) {
        return false;
    }
    if (firstFace >= m_topology.faceCount() || firstFace + faceCount > m_topology.faceCount()) {
        return false;
    }
    if (faceCount >= m_topology.faceCount()) {
        return false;
    }

    std::vector<uint32_t> extractedFaceIndices;
    extractedFaceIndices.reserve(faceCount);
    std::vector<uint32_t> remainingFaceIndices;
    remainingFaceIndices.reserve(m_topology.faceCount() - faceCount);
    for (size_t faceIndex = 0; faceIndex < m_topology.faceCount(); ++faceIndex) {
        if (faceIndex >= firstFace && faceIndex < firstFace + faceCount) {
            extractedFaceIndices.push_back(static_cast<uint32_t>(faceIndex));
            continue;
        }
        remainingFaceIndices.push_back(static_cast<uint32_t>(faceIndex));
    }

    Mesh extracted;
    Mesh remaining;
    StableMeshElementIds extractedIds;
    StableMeshElementIds remainingIds;
    if (!buildMeshFromFaceSelection(*this, extractedFaceIndices, extracted, &extractedIds)) {
        return false;
    }
    if (!buildMeshFromFaceSelection(*this, remainingFaceIndices, remaining, &remainingIds)) {
        return false;
    }

    if (hasStableElementIds()) {
        extracted.m_stableElementIds = std::move(extractedIds);
        if (extracted.m_stableElementIds.vertexIds.size() != extracted.attributes().vertexCount()
            || extracted.m_stableElementIds.faceIds.size() != extracted.topology().faceCount()) {
            extracted.rebuildStableElementIds();
        }

        remaining.m_stableElementIds = std::move(remainingIds);
        if (remaining.m_stableElementIds.vertexIds.size() != remaining.attributes().vertexCount()
            || remaining.m_stableElementIds.faceIds.size() != remaining.topology().faceCount()) {
            remaining.rebuildStableElementIds();
        }
    }

    *this = std::move(remaining);
    out = std::move(extracted);
    return true;
}

bool Mesh::weldCoincidentVertices(float epsilon, WeldCollapsePolicy policy) noexcept
{
    if (!isValid() || epsilon < 0.f) {
        return false;
    }

    const size_t vertexCount = m_attributes.vertexCount();
    if (vertexCount < 2) {
        return true;
    }

    std::vector<uint32_t> canonicalOldIndices(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        canonicalOldIndices[i] = static_cast<uint32_t>(i);
    }

    // Pair vertices through a spatial hash rather than by comparing every vertex with
    // every earlier one. The double loop was O(V^2) — measured at a constant 320 ns per
    // V^2, so 40,000 vertices cost 486 ms for a SINGLE weld — and the mesh cut welds both
    // operands while the boolean welds again, which made welding, not the geometry work,
    // the dominant cost of a boolean on any detailed model.
    //
    // Semantics are preserved EXACTLY. The old loop bound vertex i to the FIRST (smallest)
    // earlier j equivalent to it; this finds the same j, by gathering the candidates that
    // could possibly be within epsilon and taking the minimum index among those the same
    // predicate accepts. Cells are epsilon-sized, so any pair within epsilon lands in the
    // same cell or an adjacent one, and all 27 are examined. The predicate itself is
    // unchanged, so attribute seams are still respected.
    bool changed = false;
    const std::vector<nexus::render::Vec3>& weldPos = m_attributes.positions();
    const bool hashable = epsilon > 0.f && weldPos.size() == vertexCount;

    if (hashable) {
        const double cell = static_cast<double>(epsilon);
        auto cellOf = [cell](float v) -> long long {
            return static_cast<long long>(std::floor(static_cast<double>(v) / cell));
        };
        auto key = [](long long cx, long long cy, long long cz) -> uint64_t {
            // Cheap 3D integer hash; collisions only add candidates, never remove them.
            const uint64_t ux = static_cast<uint64_t>(cx) * 0x9E3779B97F4A7C15ull;
            const uint64_t uy = static_cast<uint64_t>(cy) * 0xC2B2AE3D27D4EB4Full;
            const uint64_t uz = static_cast<uint64_t>(cz) * 0x165667B19E3779F9ull;
            uint64_t h = ux ^ (uy + 0x9E3779B97F4A7C15ull + (ux << 6) + (ux >> 2));
            h ^= uz + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
            return h;
        };

        std::unordered_map<uint64_t, std::vector<uint32_t>> grid;
        grid.reserve(vertexCount * 2);

        for (size_t i = 0; i < vertexCount; ++i) {
            const long long cx = cellOf(weldPos[i].x);
            const long long cy = cellOf(weldPos[i].y);
            const long long cz = cellOf(weldPos[i].z);

            size_t bestJ = vertexCount;  // sentinel: none found
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dz = -1; dz <= 1; ++dz) {
                        const auto it = grid.find(key(cx + dx, cy + dy, cz + dz));
                        if (it == grid.end()) continue;
                        for (const uint32_t cand : it->second) {
                            if (cand >= bestJ) continue;  // already have an earlier match
                            if (vertexRowsEquivalent(m_attributes, i, cand, epsilon)) {
                                bestJ = cand;
                            }
                        }
                    }
                }
            }

            if (bestJ < vertexCount) {
                canonicalOldIndices[i] = canonicalOldIndices[bestJ];
                changed = true;
            }
            grid[key(cx, cy, cz)].push_back(static_cast<uint32_t>(i));
        }
    } else {
        for (size_t i = 0; i < vertexCount; ++i) {
            for (size_t j = 0; j < i; ++j) {
                if (!vertexRowsEquivalent(m_attributes, i, j, epsilon)) {
                    continue;
                }

                canonicalOldIndices[i] = canonicalOldIndices[j];
                changed = true;
                break;
            }
        }
    }

    if (!changed) {
        return true;
    }

    std::vector<Face> canonicalFaces;
    canonicalFaces.reserve(m_topology.faceCount());
    for (size_t fi = 0; fi < m_topology.faceCount(); ++fi) {
        const Face& face = m_topology.face(fi);
        Face remappedFace{};
        remappedFace.indices.reserve(face.indices.size());
        for (const uint32_t index : face.indices) {
            remappedFace.indices.push_back(canonicalOldIndices[index]);
        }

        bool collapsed = false;
        for (size_t i = 0; i < remappedFace.indices.size() && !collapsed; ++i) {
            for (size_t j = i + 1; j < remappedFace.indices.size(); ++j) {
                if (remappedFace.indices[i] == remappedFace.indices[j]) {
                    collapsed = true;
                    break;
                }
            }
        }
        if (collapsed) {
            // RejectWhole abandons the entire weld; DropCollapsedFace removes only this
            // zero-area face, which is an edge collapse and keeps a closed surface closed.
            if (policy == WeldCollapsePolicy::RejectWhole) {
                return false;
            }
            continue;
        }

        canonicalFaces.push_back(std::move(remappedFace));
    }

    std::vector<uint32_t> survivorOldIndices;
    survivorOldIndices.reserve(vertexCount);
    std::vector<uint32_t> compactedOldToNew(vertexCount, std::numeric_limits<uint32_t>::max());
    for (size_t i = 0; i < vertexCount; ++i) {
        if (canonicalOldIndices[i] != i) {
            continue;
        }

        compactedOldToNew[i] = static_cast<uint32_t>(survivorOldIndices.size());
        survivorOldIndices.push_back(static_cast<uint32_t>(i));
    }

    for (size_t i = 0; i < vertexCount; ++i) {
        compactedOldToNew[i] = compactedOldToNew[canonicalOldIndices[i]];
    }

    std::vector<Face> compactedFaces = canonicalFaces;
    for (Face& face : compactedFaces) {
        for (uint32_t& index : face.indices) {
            index = compactedOldToNew[index];
        }
    }

    const bool hadStableIds = hasStableElementIds();
    const MeshTopology previousTopology = m_topology;
    const StableMeshElementIds previousStableIds = m_stableElementIds;

    m_attributes.setPositions(compactVertexBuffer(m_attributes.positions(), survivorOldIndices));
    if (m_attributes.hasNormals()) {
        m_attributes.setNormals(compactVertexBuffer(m_attributes.normals(), survivorOldIndices));
    }
    if (m_attributes.hasTangents()) {
        m_attributes.setTangents(compactVertexBuffer(m_attributes.tangents(), survivorOldIndices));
    }
    if (m_attributes.hasUVs()) {
        m_attributes.setUVs(compactVertexBuffer(m_attributes.uvs(), survivorOldIndices));
    }
    if (m_attributes.hasSkinning()) {
        m_attributes.setSkinning(
            compactVertexBuffer(m_attributes.jointIndices(), survivorOldIndices),
            compactVertexBuffer(m_attributes.jointWeights(), survivorOldIndices));
    }

    m_topology.clearFaces();
    for (const Face& face : compactedFaces) {
        m_topology.addFace(face);
    }

    if (!hadStableIds) {
        return true;
    }

    m_stableElementIds = preserveStableIdsAfterVertexWeld(
        previousTopology,
        previousStableIds,
        canonicalOldIndices,
        survivorOldIndices,
        compactedFaces);

    if (m_stableElementIds.vertexIds.size() != m_attributes.vertexCount()
        || m_stableElementIds.faceIds.size() != m_topology.faceCount()) {
        rebuildStableElementIds();
    }

    return true;
}

void Mesh::rebuildStableElementIds()
{
    m_stableElementIds = {};

    m_stableElementIds.vertexIds.reserve(m_attributes.vertexCount());
    for (size_t i = 0; i < m_attributes.vertexCount(); ++i) {
        m_stableElementIds.vertexIds.push_back(static_cast<MeshElementID>(i + 1u));
    }

    m_stableElementIds.faceIds.reserve(m_topology.faceCount());
    for (size_t i = 0; i < m_topology.faceCount(); ++i) {
        m_stableElementIds.faceIds.push_back(static_cast<MeshElementID>(i + 1u));
    }

    std::map<UndirectedEdge, MeshElementID, decltype(&edgeLess)> edgeIds(&edgeLess);
    MeshElementID nextEdgeId = 1;
    for (size_t fi = 0; fi < m_topology.faceCount(); ++fi) {
        const Face& face = m_topology.face(fi);
        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t a = face.indices[i];
            const uint32_t b = face.indices[(i + 1) % face.indices.size()];
            if (a == b) {
                continue;
            }

            const UndirectedEdge edge = makeUndirectedEdge(a, b);
            if (edgeIds.find(edge) == edgeIds.end()) {
                edgeIds.emplace(edge, nextEdgeId++);
            }
        }
    }

    m_stableElementIds.edgeIds.reserve(edgeIds.size());
    for (const auto& [_, id] : edgeIds) {
        m_stableElementIds.edgeIds.push_back(id);
    }
}

bool Mesh::isSkinnableForSkeleton(size_t skeletonBoneCount) const noexcept
{
    if (!isValid()) {
        return false;
    }
    return m_attributes.skinningJointIndicesValid(skeletonBoneCount);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Primitive constructors
// ─────────────────────────────────────────────────────────────────────────────

namespace primitives {

Mesh makeTriangle(float size)
{
    if (!isFiniteFloat(size)) {
        return {};
    }

    const float h = size * 0.5f;
    std::vector<nexus::render::Vec3> positions = {
        {  0.f,  0.f,  h },   // front apex
        { -h,    0.f, -h },   // back-left
        {  h,    0.f, -h },   // back-right
    };

    Face f{};
    f.indices = {0, 1, 2};

    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.topology().addFace(std::move(f));
    return mesh;
}

Mesh makeBox(float widthX, float heightY, float depthZ)
{
    if (!isFiniteFloat(widthX) || !isFiniteFloat(heightY) || !isFiniteFloat(depthZ)) {
        return {};
    }

    const float hx = widthX * 0.5f;
    const float hy = heightY * 0.5f;
    const float hz = depthZ * 0.5f;

    // 8 unique corner vertices, indexed by (x-sign, y-sign, z-sign) → 0..7.
    std::vector<nexus::render::Vec3> positions = {
        {-hx, -hy, -hz},  // 0
        { hx, -hy, -hz},  // 1
        { hx,  hy, -hz},  // 2
        {-hx,  hy, -hz},  // 3
        {-hx, -hy,  hz},  // 4
        { hx, -hy,  hz},  // 5
        { hx,  hy,  hz},  // 6
        {-hx,  hy,  hz},  // 7
    };

    // 6 quad faces, each CCW when viewed from the outside.
    const std::array<std::array<uint32_t, 4>, 6> quads = {{
        {0, 3, 2, 1},  // -Z face
        {4, 5, 6, 7},  // +Z face
        {0, 1, 5, 4},  // -Y face
        {3, 7, 6, 2},  // +Y face
        {0, 4, 7, 3},  // -X face
        {1, 2, 6, 5},  // +X face
    }};

    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    for (const auto& q : quads) {
        Face face{};
        face.indices = {q[0], q[1], q[2], q[3]};
        mesh.topology().addFace(std::move(face));
    }
    return mesh;
}

Mesh makePlane(float width, float depth, uint32_t widthSegs, uint32_t depthSegs)
{
    if (!isFiniteFloat(width) || !isFiniteFloat(depth)) {
        return {};
    }

    widthSegs = std::max(1u, widthSegs);
    depthSegs = std::max(1u, depthSegs);

    const float hw = width * 0.5f;
    const float hd = depth * 0.5f;

    const uint32_t numCols = widthSegs + 1;
    const uint32_t numRows = depthSegs + 1;

    std::vector<nexus::render::Vec3> positions;
    positions.reserve(static_cast<size_t>(numCols) * numRows);

    std::vector<Vec2> uvs;
    uvs.reserve(positions.capacity());

    for (uint32_t row = 0; row < numRows; ++row) {
        const float t = static_cast<float>(row) / static_cast<float>(depthSegs);
        const float z = -hd + t * depth;
        for (uint32_t col = 0; col < numCols; ++col) {
            const float s = static_cast<float>(col) / static_cast<float>(widthSegs);
            const float x = -hw + s * width;
            positions.push_back({x, 0.f, z});
            uvs.push_back({s, t});
        }
    }

    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.attributes().setUVs(std::move(uvs));

    for (uint32_t row = 0; row < depthSegs; ++row) {
        for (uint32_t col = 0; col < widthSegs; ++col) {
            const uint32_t tl = row * numCols + col;
            const uint32_t tr = tl + 1;
            const uint32_t bl = tl + numCols;
            const uint32_t br = bl + 1;

            Face face{};
            face.indices = {tl, bl, br, tr};  // CCW quad
            mesh.topology().addFace(std::move(face));
        }
    }

    return mesh;
}

} // namespace primitives

// ─────────────────────────────────────────────────────────────────────────────
//  Additional primitive constructors — Month 5
// ─────────────────────────────────────────────────────────────────────────────

namespace primitives {

namespace {

static constexpr float kPi = 3.14159265358979323846f;

} // anonymous namespace

Mesh makeSphere(float radius, uint32_t latSegs, uint32_t lonSegs)
{
    if (!isFiniteFloat(radius)) {
        return {};
    }

    latSegs = std::max(2u, latSegs);
    lonSegs = std::max(3u, lonSegs);

    std::vector<nexus::render::Vec3> positions;
    std::vector<Vec2> uvs;

    // Layout: top pole (1 vertex), then (latSegs-1) interior rings × lonSegs each,
    // then bottom pole (1 vertex).  No seam duplication — UV continuity is
    // approximate, which is acceptable for a default primitive.
    const uint32_t topPole = 0u;
    positions.push_back({0.f, radius, 0.f});
    uvs.push_back({0.5f, 0.f});

    for (uint32_t ring = 1; ring < latSegs; ++ring) {
        const float phi    = static_cast<float>(ring) / static_cast<float>(latSegs) * kPi;
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);
        const float vCoord = static_cast<float>(ring) / static_cast<float>(latSegs);
        for (uint32_t seg = 0; seg < lonSegs; ++seg) {
            const float theta = static_cast<float>(seg) / static_cast<float>(lonSegs) * 2.f * kPi;
            const float uCoord = static_cast<float>(seg) / static_cast<float>(lonSegs);
            positions.push_back({
                radius * sinPhi * std::cos(theta),
                radius * cosPhi,
                radius * sinPhi * std::sin(theta)
            });
            uvs.push_back({uCoord, vCoord});
        }
    }

    const uint32_t botPole = static_cast<uint32_t>(positions.size());
    positions.push_back({0.f, -radius, 0.f});
    uvs.push_back({0.5f, 1.f});

    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.attributes().setUVs(std::move(uvs));

    // Helper: ring vertex index  (ring 1..latSegs-1, seg 0..lonSegs-1)
    auto ringVert = [&](uint32_t ring, uint32_t seg) -> uint32_t {
        return 1u + (ring - 1u) * lonSegs + (seg % lonSegs);
    };

    // Top cap triangles: {topPole, ring1[next], ring1[seg]}  (CCW outward)
    for (uint32_t seg = 0; seg < lonSegs; ++seg) {
        Face f{};
        f.indices = { topPole,
                      ringVert(1, (seg + 1) % lonSegs),
                      ringVert(1, seg) };
        mesh.topology().addFace(std::move(f));
    }

    // Interior quads: {tl, tr, br, bl}  (CCW outward) for rings 1..latSegs-2
    for (uint32_t ring = 1; ring < latSegs - 1u; ++ring) {
        for (uint32_t seg = 0; seg < lonSegs; ++seg) {
            const uint32_t tl = ringVert(ring,     seg);
            const uint32_t tr = ringVert(ring,     (seg + 1) % lonSegs);
            const uint32_t br = ringVert(ring + 1, (seg + 1) % lonSegs);
            const uint32_t bl = ringVert(ring + 1, seg);
            Face f{};
            f.indices = {tl, tr, br, bl};
            mesh.topology().addFace(std::move(f));
        }
    }

    // Bottom cap triangles: {lastRing[seg], lastRing[next], botPole}  (CCW outward)
    const uint32_t lastRing = latSegs - 1u;
    for (uint32_t seg = 0; seg < lonSegs; ++seg) {
        Face f{};
        f.indices = { ringVert(lastRing, seg),
                      ringVert(lastRing, (seg + 1) % lonSegs),
                      botPole };
        mesh.topology().addFace(std::move(f));
    }

    return mesh;
}

Mesh makeCylinder(float radius, float height, uint32_t radialSegs)
{
    if (!isFiniteFloat(radius) || !isFiniteFloat(height)) {
        return {};
    }

    radialSegs = std::max(3u, radialSegs);

    std::vector<nexus::render::Vec3> positions;
    std::vector<Vec2> uvs;

    const float hy = height * 0.5f;

    // Bottom ring (indices 0..radialSegs-1), Top ring (indices radialSegs..2*radialSegs-1)
    for (uint32_t seg = 0; seg < radialSegs; ++seg) {
        const float theta = static_cast<float>(seg) / static_cast<float>(radialSegs) * 2.f * kPi;
        const float x     = radius * std::cos(theta);
        const float z     = radius * std::sin(theta);
        const float u     = static_cast<float>(seg) / static_cast<float>(radialSegs);
        positions.push_back({x, -hy, z});  uvs.push_back({u, 1.f});
        positions.push_back({x,  hy, z});  uvs.push_back({u, 0.f});
    }

    // Bottom center and top center
    const uint32_t botCenter = static_cast<uint32_t>(positions.size());
    positions.push_back({0.f, -hy, 0.f});  uvs.push_back({0.5f, 0.5f});
    const uint32_t topCenter = botCenter + 1u;
    positions.push_back({0.f,  hy, 0.f});  uvs.push_back({0.5f, 0.5f});

    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.attributes().setUVs(std::move(uvs));

    // Vertex helpers: each segment contributes 2 vertices (bottom=even, top=odd)
    auto botRing = [&](uint32_t seg) { return (seg % radialSegs) * 2u; };
    auto topRing = [&](uint32_t seg) { return (seg % radialSegs) * 2u + 1u; };

    // Barrel: {topCur, topNext, botNext, botCur}  (CCW outward)
    for (uint32_t seg = 0; seg < radialSegs; ++seg) {
        const uint32_t tl = topRing(seg);
        const uint32_t tr = topRing(seg + 1);
        const uint32_t br = botRing(seg + 1);
        const uint32_t bl = botRing(seg);
        Face f{};
        f.indices = {tl, tr, br, bl};
        mesh.topology().addFace(std::move(f));
    }

    // Top cap: {topCenter, topRing[seg], topRing[next]}  (CCW outward, +Y normal)
    for (uint32_t seg = 0; seg < radialSegs; ++seg) {
        Face f{};
        f.indices = { topCenter,
                      topRing(seg + 1),
                      topRing(seg) };
        mesh.topology().addFace(std::move(f));
    }

    // Bottom cap: {botCenter, botRing[seg], botRing[next]}  (CCW outward, -Y normal)
    for (uint32_t seg = 0; seg < radialSegs; ++seg) {
        Face f{};
        f.indices = { botCenter,
                      botRing(seg),
                      botRing(seg + 1) };
        mesh.topology().addFace(std::move(f));
    }

    return mesh;
}

Mesh makeCone(float radius, float height, uint32_t radialSegs)
{
    if (!isFiniteFloat(radius) || !isFiniteFloat(height)) {
        return {};
    }

    radialSegs = std::max(3u, radialSegs);

    std::vector<nexus::render::Vec3> positions;
    std::vector<Vec2> uvs;

    // Base ring (y = 0)
    for (uint32_t seg = 0; seg < radialSegs; ++seg) {
        const float theta = static_cast<float>(seg) / static_cast<float>(radialSegs) * 2.f * kPi;
        const float u     = static_cast<float>(seg) / static_cast<float>(radialSegs);
        positions.push_back({ radius * std::cos(theta), 0.f, radius * std::sin(theta) });
        uvs.push_back({u, 1.f});
    }

    // Apex and base center
    const uint32_t apexIdx       = static_cast<uint32_t>(positions.size());
    positions.push_back({0.f, height, 0.f});  uvs.push_back({0.5f, 0.f});
    const uint32_t baseCenterIdx = apexIdx + 1u;
    positions.push_back({0.f, 0.f,   0.f});   uvs.push_back({0.5f, 0.5f});

    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.attributes().setUVs(std::move(uvs));

    auto baseRing = [&](uint32_t seg) { return seg % radialSegs; };

    // Lateral triangles: {apex, ring[next], ring[seg]}  (CCW outward)
    for (uint32_t seg = 0; seg < radialSegs; ++seg) {
        Face f{};
        f.indices = { apexIdx,
                      baseRing(seg + 1),
                      baseRing(seg) };
        mesh.topology().addFace(std::move(f));
    }

    // Base cap: {baseCenter, ring[seg], ring[next]}  (CCW outward, -Y normal)
    for (uint32_t seg = 0; seg < radialSegs; ++seg) {
        Face f{};
        f.indices = { baseCenterIdx,
                      baseRing(seg),
                      baseRing(seg + 1) };
        mesh.topology().addFace(std::move(f));
    }

    return mesh;
}

Mesh makeCapsule(float radius, float cylinderHeight,
                 uint32_t radialSegs, uint32_t ringSegs)
{
    if (!isFiniteFloat(radius) || !isFiniteFloat(cylinderHeight)) {
        return {};
    }

    radialSegs = std::max(3u, radialSegs);
    ringSegs   = std::max(2u, ringSegs);
    if (cylinderHeight < 0.f) cylinderHeight = 0.f;

    std::vector<nexus::render::Vec3> positions;
    std::vector<Vec2> uvs;

    const float hcy = cylinderHeight * 0.5f;
    const float totalHeight = cylinderHeight + 2.f * radius;

    // -------------------------------------------------------------------
    // Vertex layout:
    //   [0]                   : top pole   (0, hcy+radius, 0)
    //   [1 .. ringSegs*lon]   : top hemisphere rings (ring 1..ringSegs)
    //   [ringSegs*lon+1 .. (ringSegs+1)*lon] : cylinder bottom equator ring
    //   [(ringSegs+1)*lon+1 .. (2*ringSegs)*lon] : bottom hemisphere rings
    //   [last]                : bottom pole (0, -(hcy+radius), 0)
    // -------------------------------------------------------------------

    // UV v-coordinate: map full capsule height to [0,1]
    auto vCoordForY = [&](float y) {
        return (totalHeight * 0.5f - y) / totalHeight;
    };

    // Top pole
    const uint32_t topPole = 0u;
    positions.push_back({0.f, hcy + radius, 0.f});
    uvs.push_back({0.5f, vCoordForY(hcy + radius)});

    // Top hemisphere rings (phi: pi/(2*ringSegs) .. pi/2)
    for (uint32_t ring = 1; ring <= ringSegs; ++ring) {
        const float phi    = static_cast<float>(ring) / static_cast<float>(ringSegs) * (kPi * 0.5f);
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);
        const float y      = hcy + radius * cosPhi;
        const float r      = radius * sinPhi;
        const float vc     = vCoordForY(y);
        for (uint32_t seg = 0; seg < radialSegs; ++seg) {
            const float theta = static_cast<float>(seg) / static_cast<float>(radialSegs) * 2.f * kPi;
            const float uc    = static_cast<float>(seg) / static_cast<float>(radialSegs);
            positions.push_back({r * std::cos(theta), y, r * std::sin(theta)});
            uvs.push_back({uc, vc});
        }
    }

    // Cylinder bottom equator ring (y = -hcy, same radius as sphere equator)
    const uint32_t cylBotRingBase = static_cast<uint32_t>(positions.size());
    {
        const float vc = vCoordForY(-hcy);
        for (uint32_t seg = 0; seg < radialSegs; ++seg) {
            const float theta = static_cast<float>(seg) / static_cast<float>(radialSegs) * 2.f * kPi;
            const float uc    = static_cast<float>(seg) / static_cast<float>(radialSegs);
            positions.push_back({radius * std::cos(theta), -hcy, radius * std::sin(theta)});
            uvs.push_back({uc, vc});
        }
    }

    // Bottom hemisphere rings (phi: pi/2 + pi/(2*ringSegs) .. pi - pi/(2*ringSegs))
    for (uint32_t ring = 1; ring < ringSegs; ++ring) {
        const float phi    = kPi * 0.5f + static_cast<float>(ring) / static_cast<float>(ringSegs) * (kPi * 0.5f);
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);
        const float y      = -hcy + radius * cosPhi;
        const float r      = radius * sinPhi;
        const float vc     = vCoordForY(y);
        for (uint32_t seg = 0; seg < radialSegs; ++seg) {
            const float theta = static_cast<float>(seg) / static_cast<float>(radialSegs) * 2.f * kPi;
            const float uc    = static_cast<float>(seg) / static_cast<float>(radialSegs);
            positions.push_back({r * std::cos(theta), y, r * std::sin(theta)});
            uvs.push_back({uc, vc});
        }
    }

    // Bottom pole
    const uint32_t botPole = static_cast<uint32_t>(positions.size());
    positions.push_back({0.f, -(hcy + radius), 0.f});
    uvs.push_back({0.5f, vCoordForY(-(hcy + radius))});

    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.attributes().setUVs(std::move(uvs));

    // Ring accessor helpers
    // Top hemisphere ring r (1..ringSegs) starts at: 1 + (r-1)*radialSegs
    auto topHemiRing = [&](uint32_t r, uint32_t seg) -> uint32_t {
        return 1u + (r - 1u) * radialSegs + (seg % radialSegs);
    };
    // Cylinder bottom equator ring: cylBotRingBase + seg
    auto cylBotRing = [&](uint32_t seg) -> uint32_t {
        return cylBotRingBase + (seg % radialSegs);
    };
    // Bottom hemisphere ring r (1..ringSegs-1):
    // starts at cylBotRingBase + radialSegs + (r-1)*radialSegs
    auto botHemiRing = [&](uint32_t r, uint32_t seg) -> uint32_t {
        return cylBotRingBase + radialSegs + (r - 1u) * radialSegs + (seg % radialSegs);
    };

    // Top cap
    for (uint32_t seg = 0; seg < radialSegs; ++seg) {
        Face f{};
        f.indices = { topPole,
                      topHemiRing(1, (seg + 1) % radialSegs),
                      topHemiRing(1, seg) };
        mesh.topology().addFace(std::move(f));
    }

    // Top hemisphere interior quads (rings 1..ringSegs-1)
    for (uint32_t ring = 1; ring < ringSegs; ++ring) {
        for (uint32_t seg = 0; seg < radialSegs; ++seg) {
            const uint32_t tl = topHemiRing(ring,     seg);
            const uint32_t tr = topHemiRing(ring,     (seg + 1) % radialSegs);
            const uint32_t br = topHemiRing(ring + 1, (seg + 1) % radialSegs);
            const uint32_t bl = topHemiRing(ring + 1, seg);
            Face f{};
            f.indices = {tl, tr, br, bl};
            mesh.topology().addFace(std::move(f));
        }
    }

    // Cylinder barrel: top hemi equator (ringSegs) → cylinder bottom equator
    for (uint32_t seg = 0; seg < radialSegs; ++seg) {
        const uint32_t tl = topHemiRing(ringSegs,  seg);
        const uint32_t tr = topHemiRing(ringSegs,  (seg + 1) % radialSegs);
        const uint32_t br = cylBotRing((seg + 1) % radialSegs);
        const uint32_t bl = cylBotRing(seg);
        Face f{};
        f.indices = {tl, tr, br, bl};
        mesh.topology().addFace(std::move(f));
    }

    if (ringSegs > 1u) {
        // Bottom hemisphere: cylinder equator → first bottom hemi ring
        for (uint32_t seg = 0; seg < radialSegs; ++seg) {
            const uint32_t tl = cylBotRing(seg);
            const uint32_t tr = cylBotRing((seg + 1) % radialSegs);
            const uint32_t br = botHemiRing(1, (seg + 1) % radialSegs);
            const uint32_t bl = botHemiRing(1, seg);
            Face f{};
            f.indices = {tl, tr, br, bl};
            mesh.topology().addFace(std::move(f));
        }

        // Bottom hemisphere interior quads (rings 1..ringSegs-2)
        for (uint32_t ring = 1; ring < ringSegs - 1u; ++ring) {
            for (uint32_t seg = 0; seg < radialSegs; ++seg) {
                const uint32_t tl = botHemiRing(ring,     seg);
                const uint32_t tr = botHemiRing(ring,     (seg + 1) % radialSegs);
                const uint32_t br = botHemiRing(ring + 1, (seg + 1) % radialSegs);
                const uint32_t bl = botHemiRing(ring + 1, seg);
                Face f{};
                f.indices = {tl, tr, br, bl};
                mesh.topology().addFace(std::move(f));
            }
        }

        // Bottom cap: last bottom hemi ring → bottom pole
        for (uint32_t seg = 0; seg < radialSegs; ++seg) {
            Face f{};
            f.indices = { botHemiRing(ringSegs - 1u, seg),
                          botHemiRing(ringSegs - 1u, (seg + 1) % radialSegs),
                          botPole };
            mesh.topology().addFace(std::move(f));
        }
    } else {
        // ringSegs == 2: no interior bottom hemi rings — cylinder equator directly to pole
        for (uint32_t seg = 0; seg < radialSegs; ++seg) {
            Face f{};
            f.indices = { cylBotRing(seg),
                          cylBotRing((seg + 1) % radialSegs),
                          botPole };
            mesh.topology().addFace(std::move(f));
        }
    }

    return mesh;
}

Mesh makeTorus(float majorRadius, float minorRadius, uint32_t majorSegs, uint32_t minorSegs)
{
    if (!isFiniteFloat(majorRadius) || !isFiniteFloat(minorRadius)) {
        return {};
    }

    majorSegs = std::max(3u, majorSegs);
    minorSegs = std::max(3u, minorSegs);

    std::vector<nexus::render::Vec3> positions;
    std::vector<Vec2> uvs;
    positions.reserve(static_cast<size_t>(majorSegs) * minorSegs);
    uvs.reserve(static_cast<size_t>(majorSegs) * minorSegs);

    // u sweeps the major circle (XZ plane); v sweeps the tube cross-section.
    for (uint32_t i = 0; i < majorSegs; ++i) {
        const float u    = static_cast<float>(i) / static_cast<float>(majorSegs) * 2.f * kPi;
        const float cosU = std::cos(u);
        const float sinU = std::sin(u);
        for (uint32_t j = 0; j < minorSegs; ++j) {
            const float v     = static_cast<float>(j) / static_cast<float>(minorSegs) * 2.f * kPi;
            const float ringR = majorRadius + minorRadius * std::cos(v);
            positions.push_back({ ringR * cosU, minorRadius * std::sin(v), ringR * sinU });
            uvs.push_back({ static_cast<float>(i) / static_cast<float>(majorSegs),
                            static_cast<float>(j) / static_cast<float>(minorSegs) });
        }
    }

    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.attributes().setUVs(std::move(uvs));

    auto vert = [&](uint32_t i, uint32_t j) -> uint32_t {
        return (i % majorSegs) * minorSegs + (j % minorSegs);
    };

    // Closed in both directions: majorSegs × minorSegs quads (no caps).
    for (uint32_t i = 0; i < majorSegs; ++i) {
        for (uint32_t j = 0; j < minorSegs; ++j) {
            Face f{};
            f.indices = { vert(i,     j),
                          vert(i + 1, j),
                          vert(i + 1, j + 1),
                          vert(i,     j + 1) };
            mesh.topology().addFace(std::move(f));
        }
    }

    return mesh;
}

} // namespace primitives (second block)
} // namespace nexus::geometry
