#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Training Recorder — Synthetic Corpus Generator
//
//  Records every AgentCommand + AgentStateReport + visual context from an
//  agent session into structured training episodes.  Exports corpora suitable
//  for fine-tuning language models on B-Rep modeling reasoning.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/agent/AgentProtocol.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::agent {

struct TrainingStep {
    AgentCommand command;
    AgentResponse response;
    std::string visualContext;
    bool serialized = false;
};

struct TrainingEpisode {
    std::string episodeId;
    std::vector<TrainingStep> steps;
    std::string finalState;
    float successScore = 0.0f;
    std::vector<std::string> tags;
};

struct CorpusExportOptions {
    bool includeCommandText = true;
    bool includeStateReports = true;
    bool includeVisualContext = false;
    bool includeFinalState = true;
    bool includeScores = false;
    bool prettyPrint = false;
};

class AgentTrainingRecorder {
public:
    AgentTrainingRecorder();

    void beginEpisode(const std::string& episodeId);
    void recordStep(const AgentCommand& command,
                    const AgentResponse& response,
                    const std::string& visualContext = "");
    void endEpisode(float successScore = 0.0f);
    void tagEpisode(const std::string& tag);

    [[nodiscard]] const std::vector<TrainingEpisode>& episodes() const noexcept { return m_episodes; }
    [[nodiscard]] size_t episodeCount() const noexcept { return m_episodes.size(); }
    [[nodiscard]] size_t totalSteps() const noexcept;

    [[nodiscard]] std::string exportCorpus(const CorpusExportOptions& options = {}) const;
    [[nodiscard]] std::string exportEpisode(size_t episodeIndex,
                                             const CorpusExportOptions& options = {}) const;

    void clear();

private:
    [[nodiscard]] std::string serializeStep(const TrainingStep& step,
                                             const CorpusExportOptions& options) const;
    [[nodiscard]] std::string sanitizeForCorpus(const std::string& text) const;

    std::vector<TrainingEpisode> m_episodes;
    bool m_inEpisode = false;
};

} // namespace nexus::agent
