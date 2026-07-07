#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::safety {

/// @brief Aggregates per-scenario measurements to analyze worst/average case and margins.
class WorstCaseAnalyzer {
public:
    struct ScenarioStats {
        double min = 0.0;
        double max = 0.0;
        double sum = 0.0;
        size_t count = 0;
    };

    static WorstCaseAnalyzer& instance() {
        static WorstCaseAnalyzer inst;
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
        scenarios_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordMeasurement(const std::string& scenario, double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& s = scenarios_[scenario];
        if (s.count == 0) {
            s.min = value;
            s.max = value;
        } else {
            if (value < s.min) s.min = value;
            if (value > s.max) s.max = value;
        }
        s.sum += value;
        ++s.count;
    }

    /// @brief Worst (maximum) recorded value for a scenario.
    double getWorstCase(const std::string& scenario) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = scenarios_.find(scenario);
        return it != scenarios_.end() && it->second.count > 0 ? it->second.max : 0.0;
    }

    /// @brief Mean recorded value for a scenario.
    double getAverageCase(const std::string& scenario) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = scenarios_.find(scenario);
        if (it == scenarios_.end() || it->second.count == 0) return 0.0;
        return it->second.sum / static_cast<double>(it->second.count);
    }

    /// @brief Margin between a limit and the worst case (limit - worst).
    double getMargin(const std::string& scenario, double limit) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = scenarios_.find(scenario);
        double worst = it != scenarios_.end() && it->second.count > 0 ? it->second.max : 0.0;
        return limit - worst;
    }

    ScenarioStats getStats(const std::string& scenario) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = scenarios_.find(scenario);
        return it != scenarios_.end() ? it->second : ScenarioStats{};
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        scenarios_.clear();
    }

private:
    WorstCaseAnalyzer() = default;
    ~WorstCaseAnalyzer() = default;
    WorstCaseAnalyzer(const WorstCaseAnalyzer&) = delete;
    WorstCaseAnalyzer& operator=(const WorstCaseAnalyzer&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, ScenarioStats> scenarios_;
};

} // namespace nexus::utility::safety
