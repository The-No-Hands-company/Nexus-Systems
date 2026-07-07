#pragma once

#include <string>
#include <mutex>
#include <cstddef>

namespace nexus::utility::container {

class ContainerResourceMonitor {
public:
    static ContainerResourceMonitor& instance() {
        static ContainerResourceMonitor inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); memory_limit_ = 0; cpu_limit_ = 0; current_memory_ = 0; current_cpu_ = 0; }

    void setMemoryLimit(size_t bytes) {
        std::lock_guard<std::mutex> lk(mutex_);
        memory_limit_ = bytes;
    }

    void setCpuLimit(double cores) {
        std::lock_guard<std::mutex> lk(mutex_);
        cpu_limit_ = cores;
    }

    void recordMemoryUsage(size_t bytes) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        current_memory_ = bytes;
    }

    void recordCpuUsage(double cores) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        current_cpu_ = cores;
    }

    double getMemoryUsagePercent() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (memory_limit_ == 0) return 0.0;
        return (static_cast<double>(current_memory_) / memory_limit_) * 100.0;
    }

    double getCpuUsagePercent() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (cpu_limit_ == 0.0) return 0.0;
        return (current_cpu_ / cpu_limit_) * 100.0;
    }

    bool isMemoryNearLimit(double threshold) const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (memory_limit_ == 0) return false;
        return (static_cast<double>(current_memory_) / memory_limit_) > threshold;
    }

private:
    ContainerResourceMonitor() = default;
    ~ContainerResourceMonitor() = default;
    ContainerResourceMonitor(const ContainerResourceMonitor&) = delete;
    ContainerResourceMonitor& operator=(const ContainerResourceMonitor&) = delete;
    ContainerResourceMonitor(ContainerResourceMonitor&&) = delete;
    ContainerResourceMonitor& operator=(ContainerResourceMonitor&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    size_t memory_limit_ = 0;
    double cpu_limit_ = 0.0;
    size_t current_memory_ = 0;
    double current_cpu_ = 0.0;
};

} // namespace nexus::utility::container
