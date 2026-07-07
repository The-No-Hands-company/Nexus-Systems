#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::pipeline {

/// @brief Monitors pipeline stages, tracking throughput and timing per stage.
class PipelineMonitor {
public:
    struct StageStats {
        size_t count = 0;
        double total_ms = 0.0;
        double avg_ms = 0.0;
    };

    static PipelineMonitor& instance() {
        static PipelineMonitor inst;
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
        stages_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordStageEntry(const std::string& stage, const std::string& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        (void)item;
        ++stages_[stage].entries;
    }

    void recordStageExit(const std::string& stage, const std::string& item, double ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        (void)item;
        auto& s = stages_[stage];
        ++s.exits;
        s.total_ms += ms;
    }

    StageStats getStageStats(const std::string& stage) const {
        std::lock_guard<std::mutex> lock(mutex_);
        StageStats out;
        auto it = stages_.find(stage);
        if (it != stages_.end()) {
            out.count = it->second.exits;
            out.total_ms = it->second.total_ms;
            out.avg_ms = it->second.exits > 0
                             ? it->second.total_ms / static_cast<double>(it->second.exits)
                             : 0.0;
        }
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        stages_.clear();
    }

private:
    struct StageData {
        size_t entries = 0;
        size_t exits = 0;
        double total_ms = 0.0;
    };

    PipelineMonitor() = default;
    ~PipelineMonitor() = default;
    PipelineMonitor(const PipelineMonitor&) = delete;
    PipelineMonitor& operator=(const PipelineMonitor&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, StageData> stages_;
};

} // namespace nexus::utility::pipeline
