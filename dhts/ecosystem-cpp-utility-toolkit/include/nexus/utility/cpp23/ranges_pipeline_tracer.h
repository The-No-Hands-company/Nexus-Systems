#pragma once

#include <string>
#include <cstddef>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::cpp23 {

/// @brief Trace std::ranges pipeline throughput and reduction ratios.
class RangesPipelineTracer {
public:
    struct PipelineStats {
        size_t invocations = 0;
        size_t total_input = 0;
        size_t total_output = 0;
    };

    static RangesPipelineTracer& instance() {
        static RangesPipelineTracer inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        pipelines_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordPipeline(const std::string& name, size_t input_size, size_t output_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& s = pipelines_[name];
        ++s.invocations;
        s.total_input += input_size;
        s.total_output += output_size;
    }

    PipelineStats getPipelineStats(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pipelines_.find(name);
        return it != pipelines_.end() ? it->second : PipelineStats{};
    }

    /// Ratio output/input across all invocations (1.0 = no reduction, 0 if no input).
    double getReductionRatio(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pipelines_.find(name);
        if (it == pipelines_.end() || it->second.total_input == 0) return 0.0;
        return static_cast<double>(it->second.total_output) /
               static_cast<double>(it->second.total_input);
    }

private:
    RangesPipelineTracer() = default;
    ~RangesPipelineTracer() = default;
    RangesPipelineTracer(const RangesPipelineTracer&) = delete;
    RangesPipelineTracer& operator=(const RangesPipelineTracer&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, PipelineStats> pipelines_;
};

} // namespace nexus::utility::cpp23
