#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace nexus::utility::cpp26 {

class ExecutionValidator {
public:
    struct SenderStats {
        std::string senderName;
        size_t connectCount = 0;
        size_t startCount = 0;
        size_t completeCount = 0;
        size_t errorCount = 0;
        std::chrono::microseconds totalExecutionTime{0};
    };

    void recordConnect(const std::string& sender) {
        stats_[sender].senderName = sender;
        stats_[sender].connectCount++;
    }

    void recordStart(const std::string& sender) {
        stats_[sender].startCount++;
    }

    void recordComplete(const std::string& sender, std::chrono::microseconds execTime) {
        auto& s = stats_[sender];
        s.completeCount++;
        s.totalExecutionTime += execTime;
    }

    void recordError(const std::string& sender) {
        stats_[sender].errorCount++;
    }

    std::vector<SenderStats> getAllStats() const {
        std::vector<SenderStats> r;
        for (const auto& [name, s] : stats_) r.push_back(s);
        return r;
    }

    SenderStats getStats(const std::string& sender) const {
        auto it = stats_.find(sender);
        return it != stats_.end() ? it->second : SenderStats{};
    }

    bool hasErrors() const {
        for (const auto& [name, s] : stats_) if (s.errorCount > 0) return true;
        return false;
    }

    void clear() { stats_.clear(); }

private:
    std::unordered_map<std::string, SenderStats> stats_;
};

} // namespace nexus::utility::cpp26
