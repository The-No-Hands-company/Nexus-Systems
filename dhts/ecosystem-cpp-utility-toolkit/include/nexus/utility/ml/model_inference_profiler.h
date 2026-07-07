#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace nexus::utility::ml {

/**
 * @brief Profile model inference times per model.
 */
class ModelInferenceProfiler {
public:
    struct ModelStats {
        std::string model;
        std::size_t inferences = 0;
        double totalMs = 0.0;
        double minMs = 0.0;
        double maxMs = 0.0;
        double averageMs() const { return inferences ? totalMs / inferences : 0.0; }
    };

    static ModelInferenceProfiler& instance() {
        static ModelInferenceProfiler inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordInference(const std::string& model, double latencyMs) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& s = stats_[model];
        if (s.inferences == 0) { s.model = model; s.minMs = s.maxMs = latencyMs; }
        else { s.minMs = std::min(s.minMs, latencyMs); s.maxMs = std::max(s.maxMs, latencyMs); }
        ++s.inferences;
        s.totalMs += latencyMs;
    }

    ModelStats stats(const std::string& model) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stats_.find(model);
        return it == stats_.end() ? ModelStats{model, 0, 0, 0, 0} : it->second;
    }

    /// Throughput in inferences per second for a model.
    double throughput(const std::string& model) const {
        auto s = stats(model);
        return s.averageMs() > 0.0 ? 1000.0 / s.averageMs() : 0.0;
    }

    std::vector<ModelStats> allStats() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<ModelStats> out;
        for (const auto& [m, s] : stats_) out.push_back(s);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        stats_.clear();
    }

private:
    ModelInferenceProfiler() = default;
    ~ModelInferenceProfiler() = default;
    ModelInferenceProfiler(const ModelInferenceProfiler&) = delete;
    ModelInferenceProfiler& operator=(const ModelInferenceProfiler&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, ModelStats> stats_;
};

} // namespace nexus::utility::ml
