#pragma once

#include <string>
#include <unordered_map>
#include <random>
#include <mutex>

namespace nexus::utility::chaos {

class DiskFaultInjector {
public:
    static DiskFaultInjector& instance() {
        static DiskFaultInjector inst;
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
        return std::to_string(fault_paths_.size()) + " fault paths configured";
    }

    void reset() {
        std::lock_guard lock(mutex_);
        fault_paths_.clear();
    }

    void addFaultPath(const std::string& path, double error_rate_percent) {
        std::lock_guard lock(mutex_);
        fault_paths_[path] = error_rate_percent;
    }

    void removeFaultPath(const std::string& path) {
        std::lock_guard lock(mutex_);
        fault_paths_.erase(path);
    }

    bool shouldFail(const std::string& path) {
        if (!enabled_) return false;
        std::lock_guard lock(mutex_);
        auto it = fault_paths_.find(path);
        if (it == fault_paths_.end()) return false;
        std::uniform_real_distribution<double> dist(0.0, 100.0);
        return dist(rng_) < it->second;
    }

private:
    DiskFaultInjector() : rng_(std::random_device{}()) {}
    ~DiskFaultInjector() = default;

    DiskFaultInjector(const DiskFaultInjector&) = delete;
    DiskFaultInjector& operator=(const DiskFaultInjector&) = delete;
    DiskFaultInjector(DiskFaultInjector&&) = delete;
    DiskFaultInjector& operator=(DiskFaultInjector&&) = delete;

    bool enabled_ = false;
    std::string config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, double> fault_paths_;
    std::mt19937 rng_;
};

} // namespace nexus::utility::chaos
