// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Planner — Candidate Evaluation Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentPlanner.h>

#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/parametric/FeatureHistory.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace nexus::agent {

using Vec3 = nexus::render::Vec3;

AgentPlanner::AgentPlanner() = default;

PlanningResult AgentPlanner::evaluateCandidates(
    AgentSession& session,
    const std::vector<AgentCommand>& candidates,
    const PlanningContext& context)
{
    PlanningResult result;
    if (candidates.empty()) {
        result.valid = true;
        result.planExplanation = "no candidates to evaluate";
        return result;
    }

    for (const auto& candidate : candidates) {
        auto scored = evaluateSingle(session, candidate, context);
        scored.score = computeCompositeScore(scored, context);
        result.allCandidates.push_back(scored);

        if (scored.score >= 0.0f) (void)session.undo();
    }

    if (!result.allCandidates.empty()) {
        std::sort(result.allCandidates.begin(), result.allCandidates.end(),
            [](const PlanningCandidate& a, const PlanningCandidate& b) {
                return a.score > b.score;
            });

        result.bestCandidate = result.allCandidates.front();
        result.valid = true;

        std::ostringstream oss;
        oss << "evaluated " << result.allCandidates.size() << " candidates; "
            << "best=" << operationName(result.bestCandidate.command.operation)
            << " score=" << result.bestCandidate.score;
        result.planExplanation = oss.str();
    }

    return result;
}

float AgentPlanner::scoreGeometry(const nexus::cad::CadDocument& doc) const
{
    auto faces = totalFaceCount(doc);
    auto degenerate = countDegenerateFaces(doc);
    auto nonManifold = countNonManifoldEdges(doc);

    if (faces == 0) return 0.0f;

    float degenerateRatio = static_cast<float>(degenerate) / static_cast<float>(std::max(faces, 1u));
    float manifoldPenalty = static_cast<float>(nonManifold) * 0.5f;

    float score = 1.0f;
    score -= degenerateRatio * 2.0f;
    score -= manifoldPenalty * 0.1f;
    return std::max(0.0f, score);
}

float AgentPlanner::scoreTopologyCleanliness(const nexus::cad::CadDocument& doc) const
{
    auto faces = totalFaceCount(doc);
    auto avgArea = averageFaceArea(doc);

    if (faces == 0) return 0.0f;

    float areaVariationPenalty = 0.0f;
    if (avgArea < 1e-6f) areaVariationPenalty = 1.0f;
    else if (avgArea < 1e-3f) areaVariationPenalty = 0.5f;

    auto nonManifold = countNonManifoldEdges(doc);
    float manifoldPenalty = static_cast<float>(nonManifold) * 0.2f;

    float score = 1.0f - areaVariationPenalty - manifoldPenalty;
    return std::max(0.0f, score);
}

float AgentPlanner::scoreConstraintHealth(const nexus::cad::CadDocument& doc) const
{
    (void)doc;
    return 1.0f;
}

PlanningCandidate AgentPlanner::evaluateSingle(
    AgentSession& session,
    const AgentCommand& candidate,
    const PlanningContext& context)
{
    PlanningCandidate pc;
    pc.command = candidate;

    auto resp = session.execute(candidate);

    if (!resp.success) {
        pc.validityScore = 0.0f;
        pc.topologyScore = 0.0f;
        pc.efficiencyScore = 0.0f;
        pc.score = -1.0f;
        pc.rationale = "command failed: " + resp.errorMessage;
        return pc;
    }

    pc.validityScore = resp.success ? 1.0f : 0.0f;
    pc.topologyScore = scoreTopologyCleanliness(session.document());
    pc.efficiencyScore = scoreGeometry(session.document());

    (void)context;

    std::ostringstream oss;
    oss << operationName(candidate.operation) << ": topology=" << pc.topologyScore
        << " validity=" << pc.validityScore << " efficiency=" << pc.efficiencyScore;
    pc.rationale = oss.str();

    return pc;
}

float AgentPlanner::computeCompositeScore(const PlanningCandidate& c,
                                           const PlanningContext& ctx) const
{
    if (c.score < 0.0f) return -1.0f;
    return c.topologyScore * ctx.topologyWeight +
           c.validityScore * ctx.validityWeight +
           c.efficiencyScore * ctx.efficiencyWeight;
}

uint32_t AgentPlanner::countDegenerateFaces(const nexus::cad::CadDocument& doc) const
{
    uint32_t count = 0;
    for (nexus::parametric::FeatureId i = 1;
         i <= static_cast<nexus::parametric::FeatureId>(doc.history().featureCount()); ++i) {
        auto* node = doc.history().node(i);
        if (!node || !node->mesh || node->deleted) continue;
        const auto& topo = node->mesh->topology();
        for (uint32_t fi = 0; fi < topo.faceCount(); ++fi) {
            if (topo.face(fi).vertexCount() < 3) count++;
        }
    }
    return count;
}

uint32_t AgentPlanner::countNonManifoldEdges(const nexus::cad::CadDocument& doc) const
{
    // Approximate: count boundary edges as potential issues
    uint32_t count = 0;
    for (nexus::parametric::FeatureId i = 1;
         i <= static_cast<nexus::parametric::FeatureId>(doc.history().featureCount()); ++i) {
        auto* node = doc.history().node(i);
        if (!node || !node->mesh || node->deleted) continue;
        auto hem = nexus::geometry::HalfEdgeMesh::fromMesh(*node->mesh);
        if (!hem) continue;
        for (uint32_t ei = 0; ei < hem->edgeCount(); ++ei) {
            if (hem->isBoundaryEdge(ei)) count++;
        }
    }
    return count;
}

float AgentPlanner::averageFaceArea(const nexus::cad::CadDocument& doc) const
{
    float totalArea = 0.0f;
    uint32_t totalFaces = 0;
    for (nexus::parametric::FeatureId i = 1;
         i <= static_cast<nexus::parametric::FeatureId>(doc.history().featureCount()); ++i) {
        auto* node = doc.history().node(i);
        if (!node || !node->mesh || node->deleted) continue;
        auto hem = nexus::geometry::HalfEdgeMesh::fromMesh(*node->mesh);
        if (!hem) continue;
        for (uint32_t fi = 0; fi < hem->faceCount(); ++fi) {
            const auto& face = hem->face(fi);
            if (face.edge == nexus::geometry::HalfEdgeMesh::kInvalid) continue;
            std::vector<Vec3> verts;
            auto ei = face.edge;
            auto start = ei;
            do {
                const auto& he = hem->edge(ei);
                const auto& pos = hem->positions()[he.src];
                verts.push_back(Vec3{pos.x, pos.y, pos.z});
                ei = he.next;
            } while (ei != start);
            if (verts.size() < 3) continue;
            float area = 0.0f;
            for (size_t j = 1; j + 1 < verts.size(); ++j) {
                area += (verts[0] - verts[j]).cross(verts[j + 1] - verts[j]).length();
            }
            totalArea += area * 0.5f;
            totalFaces++;
        }
    }
    if (totalFaces == 0) return 0.0f;
    return totalArea / static_cast<float>(totalFaces);
}

uint32_t AgentPlanner::totalFaceCount(const nexus::cad::CadDocument& doc) const
{
    uint32_t count = 0;
    for (nexus::parametric::FeatureId i = 1;
         i <= static_cast<nexus::parametric::FeatureId>(doc.history().featureCount()); ++i) {
        auto* node = doc.history().node(i);
        if (!node || !node->mesh || node->deleted) continue;
        count += node->mesh->topology().faceCount();
    }
    return count;
}

} // namespace nexus::agent
