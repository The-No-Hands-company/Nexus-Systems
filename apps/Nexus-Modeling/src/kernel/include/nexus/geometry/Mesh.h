#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — indexed mesh primitives
//
//  Provides minimal runtime contracts for:
//  - indexed triangle/polygon mesh topology
//  - per-vertex attribute buffers (positions, normals, UVs)
//  - primitive constructors (triangle, box, plane)
//  - per-vertex normal computation from face geometry
//  - topology validation (winding, boundary check, index bounds)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Camera.h>  // Vec3, Vec2

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nexus::geometry {

// ─────────────────────────────────────────────────────────────────────────────
//  Vec2 — 2D float for UVs (not yet in render/)
// ─────────────────────────────────────────────────────────────────────────────
struct Vec2 {
    float u = 0.f, v = 0.f;
};

struct Vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
};

inline constexpr uint32_t kMaxSkinInfluencesPerVertex = 4;

using JointIndex4 = std::array<uint16_t, kMaxSkinInfluencesPerVertex>;
using JointWeight4 = std::array<float, kMaxSkinInfluencesPerVertex>;
using MeshElementID = uint64_t;

struct StableMeshElementIds {
    std::vector<MeshElementID> vertexIds;
    std::vector<MeshElementID> faceIds;
    std::vector<MeshElementID> edgeIds;

    [[nodiscard]] bool empty() const noexcept
    {
        return vertexIds.empty() && faceIds.empty() && edgeIds.empty();
    }
};

struct PackedSkinningStreams {
    uint32_t influencesPerVertex = kMaxSkinInfluencesPerVertex;
    std::vector<uint16_t> jointIndices;  // flattened [vertex * influencesPerVertex + influence]
    std::vector<float> jointWeights;     // flattened [vertex * influencesPerVertex + influence]
};

// ─────────────────────────────────────────────────────────────────────────────
//  Face — polygon defined by a flat list of vertex indices.
//  Triangles (3 indices) are the primary supported primitive.
//  Quads and n-gons can be stored for authoring and are triangulated on demand.
// ─────────────────────────────────────────────────────────────────────────────
struct Face {
    std::vector<uint32_t> indices;  // CCW winding, index into MeshAttributes vertex arrays

    [[nodiscard]] bool isTriangle() const noexcept { return indices.size() == 3; }
    [[nodiscard]] bool isQuad()     const noexcept { return indices.size() == 4; }
    [[nodiscard]] size_t vertexCount() const noexcept { return indices.size(); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  MeshAttributes — per-vertex attribute buffers
// ─────────────────────────────────────────────────────────────────────────────
class MeshAttributes {
public:
    // Positions — required for a valid mesh.
    void setPositions(std::vector<nexus::render::Vec3> positions);
    [[nodiscard]] bool hasPositions() const noexcept { return !m_positions.empty(); }
    [[nodiscard]] const std::vector<nexus::render::Vec3>& positions() const noexcept {
        return m_positions;
    }
    [[nodiscard]] size_t vertexCount() const noexcept { return m_positions.size(); }

    // Per-vertex normals.
    void setNormals(std::vector<nexus::render::Vec3> normals);
    void clearNormals() noexcept { m_normals.clear(); }
    [[nodiscard]] bool hasNormals() const noexcept { return !m_normals.empty(); }
    [[nodiscard]] const std::vector<nexus::render::Vec3>& normals() const noexcept {
        return m_normals;
    }

    // Per-vertex tangents. Stored as xyz tangent plus handedness in w.
    void setTangents(std::vector<Vec4> tangents);
    void clearTangents() noexcept { m_tangents.clear(); }
    [[nodiscard]] bool hasTangents() const noexcept { return !m_tangents.empty(); }
    [[nodiscard]] const std::vector<Vec4>& tangents() const noexcept { return m_tangents; }

    // Per-vertex UV coordinates.
    void setUVs(std::vector<Vec2> uvs);
    void clearUVs() noexcept { m_uvs.clear(); }
    [[nodiscard]] bool hasUVs() const noexcept { return !m_uvs.empty(); }
    [[nodiscard]] const std::vector<Vec2>& uvs() const noexcept { return m_uvs; }

    // Per-vertex skinning influences (fixed-width for upload determinism).
    void setSkinning(std::vector<JointIndex4> jointIndices,
                     std::vector<JointWeight4> jointWeights);
    void clearSkinning() noexcept;
    [[nodiscard]] bool hasSkinning() const noexcept;
    [[nodiscard]] const std::vector<JointIndex4>& jointIndices() const noexcept {
        return m_jointIndices;
    }
    [[nodiscard]] const std::vector<JointWeight4>& jointWeights() const noexcept {
        return m_jointWeights;
    }

    // Normalizes each vertex influence set so the weight sum is 1 (if sum > epsilon).
    // Vertices with near-zero total influence are left unchanged.
    void normalizeSkinningWeights() noexcept;

    // Validates that all referenced joint indices are < skeletonBoneCount.
    // Returns true when no skinning data is present.
    [[nodiscard]] bool skinningJointIndicesValid(size_t skeletonBoneCount) const noexcept;

    // Flattens fixed-width skinning streams into deterministic upload buffers.
    [[nodiscard]] PackedSkinningStreams packSkinningStreams() const;

    // Returns true when every non-empty optional buffer has the same size as positions.
    [[nodiscard]] bool isConsistent() const noexcept;

private:
    std::vector<nexus::render::Vec3> m_positions;
    std::vector<nexus::render::Vec3> m_normals;
    std::vector<Vec4>                m_tangents;
    std::vector<Vec2>                m_uvs;
    std::vector<JointIndex4>         m_jointIndices;
    std::vector<JointWeight4>        m_jointWeights;
};

// ─────────────────────────────────────────────────────────────────────────────
//  MeshTopology — face list with vertex index references
// ─────────────────────────────────────────────────────────────────────────────
class MeshTopology {
public:
    void addFace(Face face);
    void clearFaces() noexcept { m_faces.clear(); }

    [[nodiscard]] size_t faceCount() const noexcept { return m_faces.size(); }
    [[nodiscard]] const Face& face(size_t i) const { return m_faces.at(i); }

    // Returns true when all face indices are strictly less than vertexCount.
    [[nodiscard]] bool hasValidIndices(size_t vertexCount) const noexcept;

    // Returns true when every face has at least 3 vertices.
    [[nodiscard]] bool allFacesArePoly3Plus() const noexcept;

    // Triangulate all faces in-place. Quads are split into two triangles,
    // n-gons are fan-triangulated (vertex 0 as pivot).
    // Returns the number of faces after triangulation.
    [[nodiscard]] size_t triangulate();

private:
    std::vector<Face> m_faces;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Mesh — paired topology + attributes with validation and utility operations
// ─────────────────────────────────────────────────────────────────────────────
class Mesh {
public:
    [[nodiscard]] MeshTopology&       topology()       noexcept { return m_topology; }
    [[nodiscard]] const MeshTopology& topology() const noexcept { return m_topology; }

    [[nodiscard]] MeshAttributes&       attributes()       noexcept { return m_attributes; }
    [[nodiscard]] const MeshAttributes& attributes() const noexcept { return m_attributes; }

    // Returns true when topology indices are in bounds, all faces have ≥3 verts,
    // and attribute buffers are internally consistent.
    [[nodiscard]] bool isValid() const noexcept;

    // Computes per-vertex normals by averaging the face normals of incident faces.
    // Requires positions to be set. Overwrites any existing normals.
    // Returns false if positions are not set or topology indices are out of bounds.
    [[nodiscard]] bool computeVertexNormals();

    // Computes per-vertex tangents from positions, normals, and UVs.
    // Requires positions, normals, and UVs to be present and topology indices to be valid.
    // Tangent handedness in w is derived from the accumulated bitangent direction.
    [[nodiscard]] bool computeVertexTangents();

    // Applies a topology-preserving transform to positions and directional channels.
    // Positions are transformed with w=1, normals/tangents with w=0 and renormalized.
    // Stable element IDs are preserved because topology does not change.
    [[nodiscard]] bool applyTransform(const nexus::render::Mat4& transform) noexcept;

    // Appends another mesh into this mesh and remaps appended indices by the current
    // vertex count. This is the first explicit merge path for Month 2 and requires
    // matching optional attribute-channel presence across both meshes.
    // Existing stable element IDs are preserved; appended elements receive fresh IDs.
    [[nodiscard]] bool appendMesh(const Mesh& other) noexcept;

    // Extracts a deterministic submesh from a contiguous face range.
    // Used as the first split primitive for Month 2: selected faces are copied into out,
    // vertex buffers are compacted, and face indices are remapped to the compacted range.
    // When stable element IDs are present, retained vertex/face/edge IDs are preserved.
    [[nodiscard]] bool extractFaceRange(size_t firstFace, size_t faceCount, Mesh& out) const noexcept;

    // Destructive split variant for Month 2 composition. Selected faces are extracted into out,
    // while this mesh keeps only the remaining faces with compacted vertex streams and remapped
    // indices. Rejects whole-mesh extraction so both results remain valid meshes.
    [[nodiscard]] bool splitFaceRange(size_t firstFace, size_t faceCount, Mesh& out) noexcept;

    // Welds duplicate vertices whose positions and optional attribute channels match
    // within the provided epsilon. The operation is constrained to preserve face arity;
    // if any weld would collapse a face or merge distinct attribute seams, it fails.
    // When stable element IDs are present, retained vertex/face IDs are preserved.
    [[nodiscard]] bool weldCoincidentVertices(float epsilon = 1e-5f) noexcept;

    // Rebuilds deterministic stable element IDs for the current mesh shape.
    // IDs are stable for unchanged topology and attribute-only edits such as transforms.
    void rebuildStableElementIds();
    [[nodiscard]] bool hasStableElementIds() const noexcept { return !m_stableElementIds.empty(); }
    [[nodiscard]] const StableMeshElementIds& stableElementIds() const noexcept { return m_stableElementIds; }

    // Returns true when mesh is valid and any skinning streams reference only indices
    // in range [0, skeletonBoneCount).
    [[nodiscard]] bool isSkinnableForSkeleton(size_t skeletonBoneCount) const noexcept;

private:
    MeshTopology   m_topology;
    MeshAttributes m_attributes;
    StableMeshElementIds m_stableElementIds;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Primitive constructors
// ─────────────────────────────────────────────────────────────────────────────
namespace primitives {

// A single CCW triangle in the XZ plane.
[[nodiscard]] Mesh makeTriangle(float size = 1.f);

// An axis-aligned box centred at the origin; 8 unique vertices, 6 quad faces.
// widthX/heightY/depthZ are total extents.
[[nodiscard]] Mesh makeBox(float widthX, float heightY, float depthZ);

// A flat XZ plane centred at the origin subdivided into widthSegs × depthSegs quads.
// widthSegs and depthSegs must be ≥ 1; clamped to 1 if smaller.
[[nodiscard]] Mesh makePlane(float width, float depth,
                             uint32_t widthSegs = 1, uint32_t depthSegs = 1);

[[nodiscard]] Mesh makeSphere(float radius,
                              uint32_t latSegs = 16,
                              uint32_t lonSegs = 16);

[[nodiscard]] Mesh makeCylinder(float radius,
                                float height,
                                uint32_t radialSegs = 16);

[[nodiscard]] Mesh makeCone(float radius,
                            float height,
                            uint32_t radialSegs = 16);

[[nodiscard]] Mesh makeCapsule(float radius,
                              float cylinderHeight,
                              uint32_t radialSegs = 16,
                              uint32_t ringSegs = 8);

// Torus in the XZ plane (Y up). majorRadius is the distance from the center to the
// tube center; minorRadius is the tube radius. Closed in both directions (no caps).
[[nodiscard]] Mesh makeTorus(float majorRadius,
                             float minorRadius,
                             uint32_t majorSegs = 24,
                             uint32_t minorSegs = 12);

} // namespace primitives

} // namespace nexus::geometry
