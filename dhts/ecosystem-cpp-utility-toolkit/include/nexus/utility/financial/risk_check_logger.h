#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::financial {

struct RiskCheckEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string decision;
    double risk_score;
    std::string details;

    RiskCheckEntry(std::string d, double s, std::string det)
        : timestamp(std::chrono::system_clock::now())
        , decision(std::move(d))
        , risk_score(s)
        , details(std::move(det)) {}
};

class RiskCheckLogger {
public:
    static RiskCheckLogger& instance() {
        static RiskCheckLogger inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return "RiskCheckLogger disabled";
        return "RiskCheckLogger active, entries=" + std::to_string(history_.size());
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.clear();
    }

    void logRiskCheck(const std::string& decision, double risk_score, const std::string& details) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        history_.emplace_back(decision, risk_score, details);
    }

    std::vector<RiskCheckEntry> getHistory() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return history_;
    }

    uint64_t getDecisionCount(const std::string& decision) const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t count = 0;
        for (const auto& e : history_) {
            if (e.decision == decision) {
                ++count;
            }
        }
        return count;
    }

    double getAverageRiskScore() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (history_.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& e : history_) {
            sum += e.risk_score;
        }
        return sum / static_cast<double>(history_.size());
    }

private:
    RiskCheckLogger() = default;
    ~RiskCheckLogger() = default;

    RiskCheckLogger(const RiskCheckLogger&) = delete;
    RiskCheckLogger& operator=(const RiskCheckLogger&) = delete;
    RiskCheckLogger(RiskCheckLogger&&) = delete;
    RiskCheckLogger& operator=(RiskCheckLogger&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<RiskCheckEntry> history_;
};

} // namespace nexus::utility::financial
