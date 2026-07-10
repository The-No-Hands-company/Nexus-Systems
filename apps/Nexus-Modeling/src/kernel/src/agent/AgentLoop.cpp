// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Loop — Integrated AI Modeling Orchestrator Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentLoop.h>

#include <sstream>

namespace nexus::agent {

AgentLoop::AgentLoop(const AgentLoopConfig& config)
    : m_config(config)
{
    if (m_config.defaultPlanningContext.intent.empty()) {
        m_config.defaultPlanningContext.intent = "optimize geometry quality";
    }
}

AgentLoop::~AgentLoop() = default;

AgentResponse AgentLoop::executeIntent(const std::string& intent,
                                         const std::vector<AgentCommand>& candidates)
{
    if (candidates.empty()) {
        AgentResponse resp;
        resp.success = false;
        resp.errorMessage = "no candidates to execute";
        return resp;
    }

    if (m_config.enableRecorder) {
        m_recorder.beginEpisode("episode_" + std::to_string(m_status.stepsExecuted));
        m_recorder.tagEpisode(intent);
    }

    AgentResponse bestResponse;
    bestResponse.success = false;

    if (m_config.enablePlanner && candidates.size() > 1) {
        auto planResult = plan(intent, candidates);
        m_status.candidatesEvaluated += planResult.allCandidates.size();

        if (planResult.valid) {
            bestResponse = executeCommand(planResult.bestCandidate.command);
            m_status.averagePlanScore = planResult.bestCandidate.score;
        }
    }

    if (!bestResponse.success && candidates.size() == 1) {
        bestResponse = executeCommand(candidates[0]);
        if (m_config.enableRecorder) {
            m_recorder.recordStep(candidates[0], bestResponse);
        }
    }

    if (!bestResponse.success && candidates.size() > 1 && !m_config.enablePlanner) {
        for (const auto& cmd : candidates) {
            bestResponse = executeCommand(cmd);
            if (m_config.enableRecorder) {
                m_recorder.recordStep(cmd, bestResponse);
            }
            if (bestResponse.success) break;
        }
    }

    if (m_config.enableRecorder) {
        m_recorder.endEpisode(bestResponse.success ? 1.0f : 0.0f);
    }

    return bestResponse;
}

AgentResponse AgentLoop::executeCommand(const AgentCommand& command)
{
    auto resp = m_session.execute(command);
    m_status.stepsExecuted++;
    m_status.sessionValid = resp.success;
    return resp;
}

PlanningResult AgentLoop::plan(const std::string& intent,
                                 const std::vector<AgentCommand>& candidates)
{
    PlanningContext ctx = m_config.defaultPlanningContext;
    ctx.intent = intent;

    return m_planner.evaluateCandidates(m_session, candidates, ctx);
}

void AgentLoop::setConfig(const AgentLoopConfig& config)
{
    m_config = config;
}

std::string AgentLoop::exportTrainingCorpus(const CorpusExportOptions& options) const
{
    return m_recorder.exportCorpus(options);
}

} // namespace nexus::agent
