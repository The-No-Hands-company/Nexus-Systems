#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace nexus::utility::chaos {

class LatencyInjector {
public:
    static LatencyInjector& instance() {
        static LatencyInjector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        config_ = config;
        enabled_ = true;
    }

    void shutdown() {
        reset();
        enabled_ = false;
    }

    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard lock(mutex_);
        return std::to_string(latencies_.size()) + " latency operations";
    }

    void reset() {
        std::lock_guard lock(mutex_);
        latencies_.clear();
    }

    void injectLatency(const std::string& operation_name, int64_t delay_ms) {
        std::lock_guard lock(mutex_);
        latencies_[operation_name] = delay_ms;
    }

    void removeLatency(const std::string& operation_name) {
        std::lock_guard lock(mutex_);
        latencies_.erase(operation_name);
    }

    int64_t shouldDelay(const std::string& operation_name) const {
        if (!enabled_) return 0;
        std::lock_guard lock(mutex_);
        auto it = latencies_.find(operation_name);
        if (it == latencies_.end()) return 0;
        return it->second;
    }

private:
    LatencyInjector() = default;
    ~LatencyInjector() = default;

    LatencyInjector(const LatencyInjector&) = delete;
    LatencyInjector& operator=(const LatencyInjector&) = delete;
    LatencyInjector(LatencyInjector&&) = delete;
    LatencyInjector& operator=(LatencyInjector&&) = delete;

    bool enabled_ = false;
    std::string config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, int64_t> latencies_;
};

} // namespace nexus::utility::chaos
