#pragma once

#include <string>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::embedded {

class StackUsageAnalyzer {
public:
    struct StackInfo {
        size_t used = 0;
        size_t total = 0;
        size_t peak = 0;
    };

    static StackUsageAnalyzer& instance() {
        static StackUsageAnalyzer inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void recordStackUsage(const std::string& task, size_t used, size_t total) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        auto& info = tasks_[task];
        info.used = used;
        info.total = total;
        if (used > info.peak) info.peak = used;
    }

    size_t getPeakUsage(const std::string& task) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = tasks_.find(task);
        return (it != tasks_.end()) ? it->second.peak : 0;
    }

    double getUsagePercent(const std::string& task) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = tasks_.find(task);
        if (it == tasks_.end() || it->second.total == 0) return 0.0;
        return (static_cast<double>(it->second.used) / it->second.total) * 100.0;
    }

    bool isOverflowRisk(const std::string& task, double threshold) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = tasks_.find(task);
        if (it == tasks_.end() || it->second.total == 0) return false;
        return (static_cast<double>(it->second.used) / it->second.total) > threshold;
    }

private:
    StackUsageAnalyzer() = default;
    ~StackUsageAnalyzer() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, StackInfo> tasks_;
};

} // namespace nexus::utility::embedded
