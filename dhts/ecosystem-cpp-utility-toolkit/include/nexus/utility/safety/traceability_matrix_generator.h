#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::safety {

/// @brief Generates a requirements/test traceability matrix and coverage report.
class TraceabilityMatrixGenerator {
public:
    static TraceabilityMatrixGenerator& instance() {
        static TraceabilityMatrixGenerator inst;
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
        requirements_.clear();
        tests_.clear();
        coverage_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addRequirement(const std::string& req_id, const std::string& description) {
        std::lock_guard<std::mutex> lock(mutex_);
        requirements_[req_id] = description;
        coverage_.try_emplace(req_id);
    }

    void addTest(const std::string& test_id, const std::string& description) {
        std::lock_guard<std::mutex> lock(mutex_);
        tests_[test_id] = description;
    }

    void linkToTest(const std::string& req_id, const std::string& test_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& tests = coverage_[req_id];
        for (const auto& t : tests) {
            if (t == test_id) return;
        }
        tests.push_back(test_id);
    }

    /// @brief Map of requirement id -> linked test ids.
    std::map<std::string, std::vector<std::string>> getCoverage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return coverage_;
    }

    /// @brief Requirements that have no linked tests.
    std::vector<std::string> getUncoveredRequirements() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> out;
        for (const auto& [req, tests] : coverage_) {
            if (tests.empty()) out.push_back(req);
        }
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        requirements_.clear();
        tests_.clear();
        coverage_.clear();
    }

private:
    TraceabilityMatrixGenerator() = default;
    ~TraceabilityMatrixGenerator() = default;
    TraceabilityMatrixGenerator(const TraceabilityMatrixGenerator&) = delete;
    TraceabilityMatrixGenerator& operator=(const TraceabilityMatrixGenerator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::string> requirements_;
    std::map<std::string, std::string> tests_;
    std::map<std::string, std::vector<std::string>> coverage_;
};

} // namespace nexus::utility::safety
