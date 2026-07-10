#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Planner — Candidate Evaluation & Heuristic Scoring
//
//  Evaluates candidate AgentCommands by simulating execution, scoring
//  geometric outcomes, and ranking by expected quality.  Uses snapshot-based
//  branching so evaluations are side-effect-free.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/agent/AgentProtocol.h>
#include <nexus/agent/AgentSession.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nexus::agent {

struct PlanningCandidate {
    AgentCommand command;
    float score = 0.0f;
    float topologyScore = 0.0f;
    float validityScore = 0.0f;
    float efficiencyScore = 0.0f;
    std::string rationale;
};

struct PlanningContext {
    std::string intent;
    std::string styleHint;
    float topologyWeight = 0.4f;
    float validityWeight = 0.35f;
    float efficiencyWeight = 0.25f;
    uint32_t maxSimulationDepth = 3;
};

struct PlanningResult {
    bool valid = false;
    PlanningCandidate bestCandidate;
    std::vector<PlanningCandidate> allCandidates;
    std::string planExplanation;
};

class AgentPlanner {
public:
    AgentPlanner();

    [[nodiscard]] PlanningResult evaluateCandidates(
        AgentSession& session,
        const std::vector<AgentCommand>& candidates,
        const PlanningContext& context = {});

    [[nodiscard]] float scoreGeometry(const nexus::cad::CadDocument& doc) const;
    [[nodiscard]] float scoreTopologyCleanliness(const nexus::cad::CadDocument& doc) const;
    [[nodiscard]] float scoreConstraintHealth(const nexus::cad::CadDocument& doc) const;

    [[nodiscard]] const PlanningContext& defaultContext() const noexcept { return m_defaultContext; }
    void setDefaultContext(const PlanningContext& ctx) { m_defaultContext = ctx; }

private:
    [[nodiscard]] PlanningCandidate evaluateSingle(
        AgentSession& session,
        const AgentCommand& candidate,
        const PlanningContext& context);

    [[nodiscard]] float computeCompositeScore(const PlanningCandidate& c,
                                               const PlanningContext& ctx) const;

    [[nodiscard]] uint32_t countDegenerateFaces(const nexus::cad::CadDocument& doc) const;
    [[nodiscard]] uint32_t countNonManifoldEdges(const nexus::cad::CadDocument& doc) const;
    [[nodiscard]] float averageFaceArea(const nexus::cad::CadDocument& doc) const;
    [[nodiscard]] uint32_t totalFaceCount(const nexus::cad::CadDocument& doc) const;

    PlanningContext m_defaultContext;
};

} // namespace nexus::agent
