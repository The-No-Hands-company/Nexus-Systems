#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::specialized {

/**
 * @brief Track timing across the stages of a radar signal processing chain.
 *
 * Typical stages: ADC -> pulse compression -> Doppler FFT -> CFAR -> tracking.
 */
class RadarSignalProcessorDebug {
public:
    struct StageTiming {
        std::string stage;
        double lastUs = 0.0;
        double totalUs = 0.0;
        std::size_t invocations = 0;
        double averageUs() const {
            return invocations == 0 ? 0.0 : totalUs / invocations;
        }
        double peakUs = 0.0;
    };

    static RadarSignalProcessorDebug& instance() {
        static RadarSignalProcessorDebug inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordStage(const std::string& stage, double microseconds) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& t = stages_[stage];
        if (t.invocations == 0) {
            t.stage = stage;
            order_.push_back(stage);
        }
        t.lastUs = microseconds;
        t.totalUs += microseconds;
        ++t.invocations;
        if (microseconds > t.peakUs) t.peakUs = microseconds;
    }

    StageTiming timing(const std::string& stage) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stages_.find(stage);
        return it == stages_.end() ? StageTiming{stage, 0, 0, 0, 0} : it->second;
    }

    /// Total pipeline latency (sum of last recorded stage timings).
    double pipelineLatencyUs() const {
        std::lock_guard<std::mutex> lk(mutex_);
        double sum = 0.0;
        for (const auto& [name, t] : stages_) sum += t.lastUs;
        return sum;
    }

    /// Slowest stage by average time (bottleneck).
    std::string bottleneck() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::string worst;
        double worstAvg = -1.0;
        for (const auto& [name, t] : stages_) {
            double avg = t.averageUs();
            if (avg > worstAvg) { worstAvg = avg; worst = name; }
        }
        return worst;
    }

    std::vector<StageTiming> stagesInOrder() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<StageTiming> out;
        for (const auto& name : order_) out.push_back(stages_.at(name));
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        stages_.clear();
        order_.clear();
    }

private:
    RadarSignalProcessorDebug() = default;
    ~RadarSignalProcessorDebug() = default;
    RadarSignalProcessorDebug(const RadarSignalProcessorDebug&) = delete;
    RadarSignalProcessorDebug& operator=(const RadarSignalProcessorDebug&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, StageTiming> stages_;
    std::vector<std::string> order_;
};

} // namespace nexus::utility::specialized
