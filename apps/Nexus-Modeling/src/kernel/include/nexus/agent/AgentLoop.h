#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Loop — Integrated AI Modeling Orchestrator
//
//  Wires together the Planner, Visual Bridge, and Training Recorder into a
//  single high-level agent interface.  Receives fuzzy intent, plans via MCTS,
//  executes commands, captures visual feedback, and records training data.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/agent/AgentPlanner.h>
#include <nexus/agent/AgentProtocol.h>
#include <nexus/agent/AgentSession.h>
#include <nexus/agent/AgentVisualBridge.h>
#include <nexus/agent/AgentTrainingRecorder.h>

#include <memory>
#include <string>
#include <vector>

namespace nexus::agent {

struct AgentLoopConfig {
    bool enablePlanner = true;
    bool enableVisualBridge = false;
    bool enableRecorder = true;
    PlanningContext defaultPlanningContext;
};

struct AgentLoopStatus {
    size_t stepsExecuted = 0;
    size_t candidatesEvaluated = 0;
    float averagePlanScore = 0.0f;
    bool sessionValid = true;
    std::vector<std::string> diagnosticMessages;
};

class AgentLoop {
public:
    explicit AgentLoop(const AgentLoopConfig& config = {});
    ~AgentLoop();

    AgentLoop(const AgentLoop&) = delete;
    AgentLoop& operator=(const AgentLoop&) = delete;

    [[nodiscard]] AgentResponse executeIntent(const std::string& intent,
                                               const std::vector<AgentCommand>& candidates);

    [[nodiscard]] AgentResponse executeCommand(const AgentCommand& command);

    [[nodiscard]] AgentSession& session() noexcept { return m_session; }
    [[nodiscard]] const AgentSession& session() const noexcept { return m_session; }
    [[nodiscard]] AgentPlanner& planner() noexcept { return m_planner; }
    [[nodiscard]] AgentVisualBridge& visual() noexcept { return m_visualBridge; }
    [[nodiscard]] AgentTrainingRecorder& recorder() noexcept { return m_recorder; }
    [[nodiscard]] const AgentLoopStatus& status() const noexcept { return m_status; }

    void setConfig(const AgentLoopConfig& config);

    [[nodiscard]] std::string exportTrainingCorpus(const CorpusExportOptions& options = {}) const;

private:
    [[nodiscard]] PlanningResult plan(const std::string& intent,
                                       const std::vector<AgentCommand>& candidates);

    AgentSession m_session;
    AgentPlanner m_planner;
    AgentVisualBridge m_visualBridge;
    AgentTrainingRecorder m_recorder;
    AgentLoopConfig m_config;
    AgentLoopStatus m_status;
};

} // namespace nexus::agent
