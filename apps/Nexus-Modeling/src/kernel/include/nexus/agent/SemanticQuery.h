#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Semantic Query — Descriptive Topology Queries
//
//  Provides intent-based topology queries ("find the face closest to point X",
//  "get all edges sharper than 30 degrees") so agents never work with raw IDs.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nexus::agent {

using Vec3 = nexus::render::Vec3;

enum class QueryCriterion : uint8_t {
    AdjacentTo,
    ParallelToDirection,
    LargerThanArea,
    SmallerThanArea,
    SharperDihedralThan,
    SofterDihedralThan,
    ClosestToPoint,
    OnPlane,
    Cylindrical,
    Planar,
    Count,
};

struct TopologyQuery {
    QueryCriterion criterion = QueryCriterion::AdjacentTo;
    std::vector<uint32_t> targetFaces;
    std::vector<uint32_t> targetEdges;
    Vec3 direction{0, 0, 0};
    float threshold = 0.0f;
    float tolerance = 1e-4f;
};

struct TopologyQueryResult {
    bool valid = true;
    std::vector<uint32_t> faces;
    std::vector<uint32_t> edges;
    std::vector<uint32_t> vertices;
    std::vector<std::string> messages;
};

class SemanticQueryEngine {
public:
    explicit SemanticQueryEngine(const nexus::geometry::HalfEdgeMesh& hem);

    [[nodiscard]] TopologyQueryResult query(const TopologyQuery& q) const;

    [[nodiscard]] TopologyQueryResult findFacesAdjacentTo(const std::vector<uint32_t>& faceIds) const;
    [[nodiscard]] TopologyQueryResult findFacesParallelTo(const Vec3& direction, float tolerance = 0.01f) const;
    [[nodiscard]] TopologyQueryResult findFacesLargerThan(float area) const;
    [[nodiscard]] TopologyQueryResult findFacesSmallerThan(float area) const;
    [[nodiscard]] TopologyQueryResult findEdgesSharperThan(float dihedralDegrees) const;
    [[nodiscard]] TopologyQueryResult findEdgesSofterThan(float dihedralDegrees) const;
    [[nodiscard]] TopologyQueryResult findFacesClosestTo(const Vec3& point, uint32_t count = 1) const;
    [[nodiscard]] TopologyQueryResult findFacesOnPlane(const Vec3& normal, float offset, float tolerance = 0.01f) const;
    [[nodiscard]] TopologyQueryResult findCylindricalFaces(float radiusTolerance = 0.05f) const;
    [[nodiscard]] TopologyQueryResult findPlanarFaces(float flatnessTolerance = 0.001f) const;
    [[nodiscard]] TopologyQueryResult findFacesByDraftAngle(const Vec3& pullDirection, float minAngle, float maxAngle) const;

    [[nodiscard]] TopologyQueryResult findEdgesByLength(float minLength, float maxLength) const;
    [[nodiscard]] TopologyQueryResult findEdgesBoundary() const;

    [[nodiscard]] const nexus::geometry::HalfEdgeMesh& mesh() const noexcept { return m_hem; }

private:
    [[nodiscard]] Vec3 faceNormal(uint32_t faceId) const;
    [[nodiscard]] float faceArea(uint32_t faceId) const;
    [[nodiscard]] float dihedralAngle(uint32_t edgeId) const;
    [[nodiscard]] Vec3 faceCenter(uint32_t faceId) const;
    [[nodiscard]] float edgeLength(uint32_t edgeId) const;
    [[nodiscard]] bool isEdgeBoundary(uint32_t edgeId) const;

    const nexus::geometry::HalfEdgeMesh& m_hem;
};

} // namespace nexus::agent
