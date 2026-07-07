#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace nexus::utility::cpp26 {

class PatternMatchTracer {
public:
    struct MatchEvent {
        std::string patternDescription;
        bool matched;
        size_t branchIndex;
        std::chrono::microseconds evaluationTime{0};
    };

    struct MatchSession {
        std::string matchExpression;
        std::vector<MatchEvent> events;
        size_t totalBranches;
        size_t matchedBranches;
        std::chrono::microseconds totalTime{0};
    };

    void startSession(const std::string& expression) {
        current_ = {expression, {}, 0, 0, std::chrono::microseconds(0)};
    }

    void recordBranch(const std::string& desc, size_t branchIdx, bool matched,
                      std::chrono::microseconds evalTime) {
        current_.events.push_back({desc, matched, branchIdx, evalTime});
        current_.totalBranches++;
        if (matched) current_.matchedBranches++;
        current_.totalTime += evalTime;
    }

    MatchSession endSession() {
        auto s = current_;
        sessions_.push_back(s);
        return s;
    }

    std::vector<MatchSession> getSessions() const { return sessions_; }

    double getMatchRate() const {
        size_t t = 0, m = 0;
        for (const auto& s : sessions_) { t += s.totalBranches; m += s.matchedBranches; }
        return t > 0 ? (double)m / t * 100.0 : 0.0;
    }

    void clear() { sessions_.clear(); }

private:
    MatchSession current_;
    std::vector<MatchSession> sessions_;
};

} // namespace nexus::utility::cpp26
