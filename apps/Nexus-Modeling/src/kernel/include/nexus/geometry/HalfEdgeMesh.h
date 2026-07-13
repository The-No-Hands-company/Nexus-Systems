#pragma once
// --- Nexus Geometry — HalfEdgeMesh

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

class HalfEdgeMesh {
public:
    static constexpr uint32_t kInvalid = 0xFFFFFFFFu;

    struct HEEdge { uint32_t next = kInvalid, prev = kInvalid, twin = kInvalid, src = kInvalid, face = kInvalid; };
    struct HEVertex { uint32_t edge = kInvalid; };
    struct HEFace { uint32_t edge = kInvalid; };

    [[nodiscard]] static std::optional<HalfEdgeMesh> fromMesh(const Mesh& mesh);

    // Converts back to an indexed Mesh. By default faces are fan-triangulated
    // (most consumers expect triangles); pass triangulate=false to preserve the
    // original n-gon faces (e.g. quad-preserving edit ops).
    Mesh toMesh(bool triangulate = true) const;

    bool isManifold() const;
    bool isClosed() const;
    bool isTriangulated() const;

    // Connectivity-integrity validator. Unlike isManifold()/toMesh().isValid(),
    // this inspects the raw half-edge invariants directly, walking only *live*
    // elements (tombstoned edges have face==kInvalid; tombstoned faces have
    // edge==kInvalid). It is the authoritative post-condition every Euler
    // operator (flip/split/collapse) is expected to preserve. Counts refer to
    // live elements only; `boundaryEdges` are live half-edges with no twin.
    struct IntegrityReport {
        bool ok = true;
        std::string reason;      // first violation encountered; empty when ok
        uint32_t liveEdges = 0;  // live directed half-edges
        uint32_t liveFaces = 0;
        uint32_t liveVerts = 0;  // vertices that are src of some live half-edge
        uint32_t boundaryEdges = 0;
    };
    [[nodiscard]] IntegrityReport checkIntegrity() const;

    uint32_t edgeCount() const { return static_cast<uint32_t>(m_edges.size()); }
    uint32_t vertexCount() const { return static_cast<uint32_t>(m_verts.size()); }
    uint32_t faceCount() const { return static_cast<uint32_t>(m_faces.size()); }

    std::vector<std::vector<uint32_t>> boundaryLoops() const;

    bool flipEdge(uint32_t he);
    bool splitEdge(uint32_t he);
    bool insertEdgeVertex(uint32_t he, float t = 0.5f);
    bool collapseEdge(uint32_t he, const nexus::render::Vec3& target);

    std::vector<uint32_t> vertexFaces(uint32_t v) const;
    std::vector<uint32_t> vertexNeighbors(uint32_t v) const;

    uint32_t faceValence(uint32_t fi) const;
    uint32_t edgeValence(uint32_t ei) const;
    bool isBoundaryEdge(uint32_t ei) const;
    bool isBoundaryVertex(uint32_t v) const;

    std::vector<uint32_t> edgeLoop(uint32_t seedEdge) const;
    std::vector<uint32_t> edgeRing(uint32_t seedEdge) const;

    bool insertEdgeLoop(uint32_t seedEdge, float slide = 0.5f);
    bool slideEdgeLoop(uint32_t seedEdge, float slide);
    bool splitFacesAlongLoop(const std::vector<uint32_t>& newVerts);

    bool bevelVertex(uint32_t vertex, float distance);
    // Insets one face toward its centroid by factor t in [0,1] (0 = no inset):
    // the face becomes an inner ring, ringed by n border quads. Interpolates the
    // full attribute set (position/uv/normal/tangent/skin) per new vertex.
    bool insetFace(uint32_t faceIndex, float t);
    // Extrudes one face into a prism: the face slot becomes a new cap ring
    // offset along the face normal by `distance`, ringed by n wall quads (the
    // original base is not retained). Same topology as insetFace; cap vertices
    // copy the full attribute set. Integrity-clean on a manifold input.
    bool extrudeFacePrism(uint32_t faceIndex, float distance);
    bool pokeFace(uint32_t faceIndex);
    bool connectVertices(uint32_t v0, uint32_t v1);
    bool gridFill(const std::vector<uint32_t>& boundaryLoop);
    bool bevelEdgesHEM(const std::vector<uint32_t>& edges, float distance, uint32_t segments = 1);
    bool extrudeFaces(const std::vector<uint32_t>& faceIndices, float distance, bool keepOriginal = true);

    const HEEdge& edge(uint32_t i) const { return m_edges[i]; }
    const HEVertex& vertex(uint32_t i) const { return m_verts[i]; }
    const HEFace& face(uint32_t i) const { return m_faces[i]; }

    [[nodiscard]] const HEEdge* edgeSafe(uint32_t i) const noexcept {
        return (i < m_edges.size()) ? &m_edges[i] : nullptr;
    }
    [[nodiscard]] const HEVertex* vertexSafe(uint32_t i) const noexcept {
        return (i < m_verts.size()) ? &m_verts[i] : nullptr;
    }
    [[nodiscard]] const HEFace* faceSafe(uint32_t i) const noexcept {
        return (i < m_faces.size()) ? &m_faces[i] : nullptr;
    }

    const std::vector<nexus::render::Vec3>& positions() const { return m_positions; }
    std::vector<nexus::render::Vec3>& positions() { return m_positions; }
    const std::vector<Vec2>& uvs() const { return m_uvs; }
    std::vector<Vec2>& uvs() { return m_uvs; }
    const std::vector<nexus::render::Vec3>& normals() const { return m_normals; }
    std::vector<nexus::render::Vec3>& normals() { return m_normals; }
    const std::vector<Vec4>& tangents() const { return m_tangents; }
    std::vector<Vec4>& tangents() { return m_tangents; }
    const std::vector<JointIndex4>& jointIndices() const { return m_jointIndices; }
    std::vector<JointIndex4>& jointIndices() { return m_jointIndices; }
    const std::vector<JointWeight4>& jointWeights() const { return m_jointWeights; }
    std::vector<JointWeight4>& jointWeights() { return m_jointWeights; }
    [[nodiscard]] bool hasUVs() const noexcept { return !m_uvs.empty(); }
    [[nodiscard]] bool hasNormals() const noexcept { return !m_normals.empty(); }
    [[nodiscard]] bool hasTangents() const noexcept { return !m_tangents.empty(); }
    [[nodiscard]] bool hasSkinning() const noexcept
    {
        return !m_jointIndices.empty() && !m_jointWeights.empty();
    }

    uint32_t findEdge(uint32_t src, uint32_t dst) const;

private:
    std::vector<HEEdge> m_edges;
    std::vector<HEVertex> m_verts;
    std::vector<HEFace> m_faces;
    std::vector<nexus::render::Vec3> m_positions;
    std::vector<Vec2> m_uvs;
    std::vector<nexus::render::Vec3> m_normals;
    std::vector<Vec4> m_tangents;
    std::vector<JointIndex4> m_jointIndices;
    std::vector<JointWeight4> m_jointWeights;
    std::unordered_map<uint64_t, uint32_t> m_edgeMap;

    uint32_t addEdgePair(uint32_t src, uint32_t dst, uint32_t face);
    void updateEdgeMap(uint32_t ei);
};

} // namespace nexus::geometry
