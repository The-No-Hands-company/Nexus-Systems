// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Semantic Query — Descriptive Topology Queries Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/SemanticQuery.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace nexus::agent {

SemanticQueryEngine::SemanticQueryEngine(const nexus::geometry::HalfEdgeMesh& hem)
    : m_hem(hem) {}

TopologyQueryResult SemanticQueryEngine::query(const TopologyQuery& q) const
{
    switch (q.criterion) {
        case QueryCriterion::AdjacentTo:           return findFacesAdjacentTo(q.targetFaces);
        case QueryCriterion::ParallelToDirection:  return findFacesParallelTo(q.direction, q.tolerance);
        case QueryCriterion::LargerThanArea:       return findFacesLargerThan(q.threshold);
        case QueryCriterion::SmallerThanArea:      return findFacesSmallerThan(q.threshold);
        case QueryCriterion::SharperDihedralThan:  return findEdgesSharperThan(q.threshold);
        case QueryCriterion::SofterDihedralThan:   return findEdgesSofterThan(q.threshold);
        case QueryCriterion::ClosestToPoint:       return findFacesClosestTo(q.direction, 1);
        case QueryCriterion::OnPlane:              return findFacesOnPlane(q.direction, q.threshold, q.tolerance);
        case QueryCriterion::Cylindrical:          return findCylindricalFaces(q.tolerance);
        case QueryCriterion::Planar:               return findPlanarFaces(q.tolerance);
        default: {
            TopologyQueryResult result;
            result.valid = false;
            result.messages.push_back("unknown query criterion");
            return result;
        }
    }
}

TopologyQueryResult SemanticQueryEngine::findFacesAdjacentTo(const std::vector<uint32_t>& faceIds) const
{
    TopologyQueryResult result;
    std::vector<uint32_t> adjacent;
    for (auto fid : faceIds) {
        if (fid >= m_hem.faceCount()) continue;
        const auto& face = m_hem.face(fid);
        if (face.edge == nexus::geometry::HalfEdgeMesh::kInvalid) continue;
        auto ei = face.edge;
        auto start = ei;
        do {
            const auto& he = m_hem.edge(ei);
            const auto& twin = m_hem.edge(he.twin);
            if (twin.face != nexus::geometry::HalfEdgeMesh::kInvalid && twin.face != fid) {
                adjacent.push_back(twin.face);
            }
            ei = he.next;
        } while (ei != start);
    }
    std::sort(adjacent.begin(), adjacent.end());
    adjacent.erase(std::unique(adjacent.begin(), adjacent.end()), adjacent.end());
    result.faces = std::move(adjacent);
    return result;
}

TopologyQueryResult SemanticQueryEngine::findFacesParallelTo(const Vec3& direction, float tolerance) const
{
    TopologyQueryResult result;
    Vec3 dir = direction.normalize();
    for (uint32_t fi = 0; fi < m_hem.faceCount(); ++fi) {
        auto n = faceNormal(fi);
        float dot = std::abs(n.dot(dir));
        if (std::abs(dot - 1.0f) < tolerance) result.faces.push_back(fi);
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findFacesLargerThan(float area) const
{
    TopologyQueryResult result;
    for (uint32_t fi = 0; fi < m_hem.faceCount(); ++fi) {
        if (faceArea(fi) > area) result.faces.push_back(fi);
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findFacesSmallerThan(float area) const
{
    TopologyQueryResult result;
    for (uint32_t fi = 0; fi < m_hem.faceCount(); ++fi) {
        float fa = faceArea(fi);
        if (fa > 0.0f && fa < area) result.faces.push_back(fi);
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findEdgesSharperThan(float dihedralDegrees) const
{
    TopologyQueryResult result;
    for (uint32_t ei = 0; ei < m_hem.edgeCount(); ++ei) {
        float angle = dihedralAngle(ei);
        if (angle > dihedralDegrees && angle < 180.0f - dihedralDegrees) result.edges.push_back(ei);
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findEdgesSofterThan(float dihedralDegrees) const
{
    TopologyQueryResult result;
    for (uint32_t ei = 0; ei < m_hem.edgeCount(); ++ei) {
        float angle = dihedralAngle(ei);
        if (angle < dihedralDegrees || angle > 180.0f - dihedralDegrees) result.edges.push_back(ei);
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findFacesClosestTo(const Vec3& point, uint32_t count) const
{
    TopologyQueryResult result;
    struct Dist { uint32_t face; float dist; };
    std::vector<Dist> dists;
    for (uint32_t fi = 0; fi < m_hem.faceCount(); ++fi) {
        Vec3 fc = faceCenter(fi);
        dists.push_back({fi, (fc - point).lengthSq()});
    }
    std::sort(dists.begin(), dists.end(), [](const Dist& a, const Dist& b) { return a.dist < b.dist; });
    count = std::min(count, static_cast<uint32_t>(dists.size()));
    for (uint32_t i = 0; i < count; ++i) result.faces.push_back(dists[i].face);
    return result;
}

TopologyQueryResult SemanticQueryEngine::findFacesOnPlane(const Vec3& normal, float offset, float tolerance) const
{
    TopologyQueryResult result;
    Vec3 n = normal.normalize();
    for (uint32_t fi = 0; fi < m_hem.faceCount(); ++fi) {
        Vec3 fc = faceCenter(fi);
        float dist = std::abs(fc.dot(n) - offset);
        float ndot = std::abs(faceNormal(fi).dot(n));
        if (dist < tolerance && ndot > 1.0f - tolerance) result.faces.push_back(fi);
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findCylindricalFaces(float radiusTolerance) const
{
    TopologyQueryResult result;
    (void)radiusTolerance;
    for (uint32_t fi = 0; fi < m_hem.faceCount(); ++fi) {
        const auto& face = m_hem.face(fi);
        if (face.edge == nexus::geometry::HalfEdgeMesh::kInvalid) continue;
        auto ei = face.edge;
        auto start = ei;
        Vec3 axisAcc{};
        uint32_t axisCount = 0;
        Vec3 prevNormal = faceNormal(fi);
        do {
            const auto& he = m_hem.edge(ei);
            const auto& twin = m_hem.edge(he.twin);
            if (twin.face != nexus::geometry::HalfEdgeMesh::kInvalid) {
                Vec3 adjNormal = faceNormal(twin.face);
                Vec3 cross = prevNormal.cross(adjNormal);
                if (cross.lengthSq() > 1e-10f) { axisAcc = axisAcc + cross; axisCount++; }
            }
            ei = he.next;
        } while (ei != start);
        if (axisCount > 2) {
            Vec3 avgAxis = (axisAcc * (1.0f / static_cast<float>(axisCount))).normalize();
            float maxDev = 0.0f;
            auto checkEi = start;
            uint32_t checkCount = 0;
            do {
                const auto& checkHe = m_hem.edge(checkEi);
                const auto& pos = m_hem.positions()[checkHe.src];
                Vec3 p{pos.x, pos.y, pos.z};
                Vec3 proj = avgAxis * p.dot(avgAxis);
                float dev = (p - proj).length();
                if (dev > maxDev) maxDev = dev;
                checkCount++;
                checkEi = checkHe.next;
            } while (checkEi != start);
            if (checkCount > 4 && maxDev < 10.0f) result.faces.push_back(fi);
        }
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findPlanarFaces(float flatnessTolerance) const
{
    TopologyQueryResult result;
    for (uint32_t fi = 0; fi < m_hem.faceCount(); ++fi) {
        const auto& face = m_hem.face(fi);
        if (face.edge == nexus::geometry::HalfEdgeMesh::kInvalid) continue;
        Vec3 fn = faceNormal(fi);
        Vec3 fc = faceCenter(fi);
        auto ei = face.edge;
        auto start = ei;
        float maxDev = 0.0f;
        do {
            const auto& he = m_hem.edge(ei);
            const auto& pos = m_hem.positions()[he.src];
            Vec3 p{pos.x, pos.y, pos.z};
            float projDist = std::abs((p - fc).dot(fn));
            if (projDist > maxDev) maxDev = projDist;
            ei = he.next;
        } while (ei != start);
        if (maxDev < flatnessTolerance) result.faces.push_back(fi);
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findFacesByDraftAngle(const Vec3& pullDirection, float minAngle, float maxAngle) const
{
    TopologyQueryResult result;
    Vec3 pull = pullDirection.normalize();
    for (uint32_t fi = 0; fi < m_hem.faceCount(); ++fi) {
        Vec3 fn = faceNormal(fi);
        float dot = fn.dot(pull);
        float angle = std::acos(std::clamp(dot, -1.0f, 1.0f)) * (180.0f / std::numbers::pi_v<float>);
        if (angle >= minAngle && angle <= maxAngle) result.faces.push_back(fi);
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findEdgesByLength(float minLength, float maxLength) const
{
    TopologyQueryResult result;
    for (uint32_t ei = 0; ei < m_hem.edgeCount(); ++ei) {
        float len = edgeLength(ei);
        if (len >= minLength && len <= maxLength) result.edges.push_back(ei);
    }
    return result;
}

TopologyQueryResult SemanticQueryEngine::findEdgesBoundary() const
{
    TopologyQueryResult result;
    for (uint32_t ei = 0; ei < m_hem.edgeCount(); ++ei) {
        if (m_hem.isBoundaryEdge(ei)) result.edges.push_back(ei);
    }
    return result;
}

Vec3 SemanticQueryEngine::faceNormal(uint32_t faceId) const
{
    const auto& face = m_hem.face(faceId);
    if (face.edge == nexus::geometry::HalfEdgeMesh::kInvalid) return Vec3{0, 1, 0};
    const auto& he0 = m_hem.edge(face.edge);
    const auto& he1 = m_hem.edge(he0.next);
    const auto& he2 = m_hem.edge(he1.next);
    const auto& p0 = m_hem.positions()[he0.src];
    const auto& p1 = m_hem.positions()[he1.src];
    const auto& p2 = m_hem.positions()[he2.src];
    Vec3 a{p0.x, p0.y, p0.z};
    Vec3 b{p1.x, p1.y, p1.z};
    Vec3 c{p2.x, p2.y, p2.z};
    return (b - a).cross(c - a).normalize();
}

float SemanticQueryEngine::faceArea(uint32_t faceId) const
{
    const auto& face = m_hem.face(faceId);
    if (face.edge == nexus::geometry::HalfEdgeMesh::kInvalid) return 0.0f;
    std::vector<Vec3> verts;
    auto ei = face.edge;
    auto start = ei;
    do {
        const auto& he = m_hem.edge(ei);
        const auto& pos = m_hem.positions()[he.src];
        verts.push_back(Vec3{pos.x, pos.y, pos.z});
        ei = he.next;
    } while (ei != start);
    if (verts.size() < 3) return 0.0f;
    float area = 0.0f;
    for (size_t i = 1; i + 1 < verts.size(); ++i) {
        area += (verts[0] - verts[i]).cross(verts[i + 1] - verts[i]).length();
    }
    return area * 0.5f;
}

float SemanticQueryEngine::dihedralAngle(uint32_t edgeId) const
{
    const auto& he = m_hem.edge(edgeId);
    const auto& twin = m_hem.edge(he.twin);
    uint32_t f0 = he.face;
    uint32_t f1 = twin.face;
    if (f0 == nexus::geometry::HalfEdgeMesh::kInvalid || f1 == nexus::geometry::HalfEdgeMesh::kInvalid) return 0.0f;
    Vec3 n0 = faceNormal(f0);
    Vec3 n1 = faceNormal(f1);
    float dot = std::clamp(n0.dot(n1), -1.0f, 1.0f);
    return std::acos(dot) * (180.0f / std::numbers::pi_v<float>);
}

Vec3 SemanticQueryEngine::faceCenter(uint32_t faceId) const
{
    const auto& face = m_hem.face(faceId);
    if (face.edge == nexus::geometry::HalfEdgeMesh::kInvalid) return Vec3{};
    Vec3 center{};
    uint32_t count = 0;
    auto ei = face.edge;
    auto start = ei;
    do {
        const auto& he = m_hem.edge(ei);
        const auto& pos = m_hem.positions()[he.src];
        center.x += pos.x; center.y += pos.y; center.z += pos.z;
        count++;
        ei = he.next;
    } while (ei != start);
    if (count == 0) return Vec3{};
    float inv = 1.0f / static_cast<float>(count);
    return {center.x * inv, center.y * inv, center.z * inv};
}

float SemanticQueryEngine::edgeLength(uint32_t edgeId) const
{
    const auto& he = m_hem.edge(edgeId);
    const auto& twin = m_hem.edge(he.twin);
    const auto& p0 = m_hem.positions()[he.src];
    const auto& p1 = m_hem.positions()[twin.src];
    float dx = p1.x - p0.x;
    float dy = p1.y - p0.y;
    float dz = p1.z - p0.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool SemanticQueryEngine::isEdgeBoundary(uint32_t edgeId) const
{
    return m_hem.isBoundaryEdge(edgeId);
}

} // namespace nexus::agent
