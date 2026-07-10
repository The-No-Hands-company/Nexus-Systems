// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Training Recorder — Corpus Generator Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentTrainingRecorder.h>

#include <algorithm>
#include <sstream>

namespace nexus::agent {

AgentTrainingRecorder::AgentTrainingRecorder() = default;

void AgentTrainingRecorder::beginEpisode(const std::string& episodeId)
{
    TrainingEpisode ep;
    ep.episodeId = episodeId;
    m_episodes.push_back(std::move(ep));
    m_inEpisode = true;
}

void AgentTrainingRecorder::recordStep(const AgentCommand& command,
                                        const AgentResponse& response,
                                        const std::string& visualContext)
{
    if (!m_inEpisode || m_episodes.empty()) return;

    TrainingStep step;
    step.command = command;
    step.response = response;
    step.visualContext = visualContext;
    m_episodes.back().steps.push_back(std::move(step));
}

void AgentTrainingRecorder::endEpisode(float successScore)
{
    if (!m_inEpisode || m_episodes.empty()) return;

    auto& ep = m_episodes.back();
    ep.successScore = successScore;

    std::ostringstream oss;
    for (const auto& step : ep.steps) {
        oss << serializeCommand(step.command);
    }
    ep.finalState = oss.str();

    m_inEpisode = false;
}

void AgentTrainingRecorder::tagEpisode(const std::string& tag)
{
    if (!m_episodes.empty()) {
        m_episodes.back().tags.push_back(tag);
    }
}

size_t AgentTrainingRecorder::totalSteps() const noexcept
{
    size_t total = 0;
    for (const auto& ep : m_episodes) total += ep.steps.size();
    return total;
}

std::string AgentTrainingRecorder::exportCorpus(const CorpusExportOptions& options) const
{
    std::ostringstream oss;

    for (size_t i = 0; i < m_episodes.size(); ++i) {
        oss << exportEpisode(i, options);
        if (i + 1 < m_episodes.size() && options.prettyPrint) oss << "\n";
    }

    return oss.str();
}

std::string AgentTrainingRecorder::exportEpisode(size_t episodeIndex,
                                                   const CorpusExportOptions& options) const
{
    if (episodeIndex >= m_episodes.size()) return "";

    const auto& ep = m_episodes[episodeIndex];
    std::ostringstream oss;

    if (options.prettyPrint) {
        oss << "{\n  \"episode_id\": \"" << sanitizeForCorpus(ep.episodeId) << "\"";
        oss << ",\n  \"step_count\": " << ep.steps.size();
        if (options.includeScores)
            oss << ",\n  \"success_score\": " << ep.successScore;
        if (!ep.tags.empty()) {
            oss << ",\n  \"tags\": [";
            for (size_t i = 0; i < ep.tags.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << '"' << sanitizeForCorpus(ep.tags[i]) << '"';
            }
            oss << "]";
        }
        oss << "\n}";
    } else {
        for (const auto& step : ep.steps) {
            oss << serializeStep(step, options);
        }
    }

    return oss.str();
}

std::string AgentTrainingRecorder::serializeStep(const TrainingStep& step,
                                                    const CorpusExportOptions& options) const
{
    std::ostringstream oss;

    if (options.includeCommandText) {
        oss << "COMMAND: " << serializeCommand(step.command);
    }

    if (options.includeStateReports && step.response.success) {
        for (const auto& report : step.response.stateReports) {
            for (const auto& msg : report.messages) {
                oss << "RESULT: " << msg << "\n";
            }
        }
    }

    if (options.includeVisualContext && !step.visualContext.empty()) {
        oss << "VISUAL: " << step.visualContext << "\n";
    }

    return oss.str();
}

std::string AgentTrainingRecorder::sanitizeForCorpus(const std::string& text) const
{
    std::string out;
    for (char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

void AgentTrainingRecorder::clear()
{
    m_episodes.clear();
    m_inEpisode = false;
}

} // namespace nexus::agent
