#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::parser {

/// @brief Traces compiler optimization passes and the functions they modify.
class OptimizationTracer {
public:
    struct PassStats {
        size_t invocations = 0;
        size_t changes = 0;
    };

    static OptimizationTracer& instance() {
        static OptimizationTracer inst;
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
        passes_.clear();
        optimized_functions_.clear();
        total_changes_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordOptimization(const std::string& pass, const std::string& function, bool changed) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& p = passes_[pass];
        ++p.invocations;
        if (changed) {
            ++p.changes;
            ++total_changes_;
            optimized_functions_.insert(function);
        }
    }

    PassStats getPassStats(const std::string& pass) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = passes_.find(pass);
        return it != passes_.end() ? it->second : PassStats{};
    }

    std::vector<std::string> getOptimizedFunctions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {optimized_functions_.begin(), optimized_functions_.end()};
    }

    size_t getTotalChanges() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_changes_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        passes_.clear();
        optimized_functions_.clear();
        total_changes_ = 0;
    }

private:
    OptimizationTracer() = default;
    ~OptimizationTracer() = default;
    OptimizationTracer(const OptimizationTracer&) = delete;
    OptimizationTracer& operator=(const OptimizationTracer&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, PassStats> passes_;
    std::set<std::string> optimized_functions_;
    size_t total_changes_ = 0;
};

} // namespace nexus::utility::parser
