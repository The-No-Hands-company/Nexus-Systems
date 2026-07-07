#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>

namespace nexus::utility::eventsystem {

/// @brief Monitors per-queue backpressure by tracking depth statistics.
class BackpressureMonitor {
public:
    struct DepthStats {
        size_t samples = 0;
        size_t current = 0;
        size_t max = 0;
        double avg = 0.0;
    };

    static BackpressureMonitor& instance() {
        static BackpressureMonitor inst;
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
        queues_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordQueueDepth(const std::string& queue, size_t depth) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& q = queues_[queue];
        q.current = depth;
        q.sum += depth;
        ++q.samples;
        if (depth > q.max) q.max = depth;
    }

    bool isBackpressured(const std::string& queue, size_t threshold) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queues_.find(queue);
        return it != queues_.end() && it->second.current > threshold;
    }

    size_t getMaxDepth(const std::string& queue) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queues_.find(queue);
        return it != queues_.end() ? it->second.max : 0;
    }

    double getAvgDepth(const std::string& queue) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queues_.find(queue);
        if (it == queues_.end() || it->second.samples == 0) return 0.0;
        return static_cast<double>(it->second.sum) / static_cast<double>(it->second.samples);
    }

    DepthStats getStats(const std::string& queue) const {
        std::lock_guard<std::mutex> lock(mutex_);
        DepthStats out;
        auto it = queues_.find(queue);
        if (it != queues_.end()) {
            out.samples = it->second.samples;
            out.current = it->second.current;
            out.max = it->second.max;
            out.avg = it->second.samples > 0
                          ? static_cast<double>(it->second.sum) / static_cast<double>(it->second.samples)
                          : 0.0;
        }
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        queues_.clear();
    }

private:
    struct QueueData {
        size_t samples = 0;
        size_t current = 0;
        size_t max = 0;
        size_t sum = 0;
    };

    BackpressureMonitor() = default;
    ~BackpressureMonitor() = default;
    BackpressureMonitor(const BackpressureMonitor&) = delete;
    BackpressureMonitor& operator=(const BackpressureMonitor&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, QueueData> queues_;
};

} // namespace nexus::utility::eventsystem
