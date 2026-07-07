#pragma once

#include <string>
#include <map>
#include <mutex>

namespace nexus::utility::cloud {

class CloudQuotaMonitor {
public:
    struct QuotaInfo {
        double limit = 0.0;
        double usage = 0.0;
    };

    static CloudQuotaMonitor& instance() {
        static CloudQuotaMonitor inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); quotas_.clear(); }

    void setQuota(const std::string& resource, double limit) {
        std::lock_guard<std::mutex> lk(mutex_);
        quotas_[resource].limit = limit;
    }

    void recordUsage(const std::string& resource, double usage) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        quotas_[resource].usage = usage;
    }

    double getUsagePercent(const std::string& resource) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = quotas_.find(resource);
        if (it == quotas_.end() || it->second.limit == 0.0) return 0.0;
        return (it->second.usage / it->second.limit) * 100.0;
    }

    bool isNearLimit(const std::string& resource, double threshold) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = quotas_.find(resource);
        if (it == quotas_.end() || it->second.limit == 0.0) return false;
        return (it->second.usage / it->second.limit) > threshold;
    }

private:
    CloudQuotaMonitor() = default;
    ~CloudQuotaMonitor() = default;
    CloudQuotaMonitor(const CloudQuotaMonitor&) = delete;
    CloudQuotaMonitor& operator=(const CloudQuotaMonitor&) = delete;
    CloudQuotaMonitor(CloudQuotaMonitor&&) = delete;
    CloudQuotaMonitor& operator=(CloudQuotaMonitor&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, QuotaInfo> quotas_;
};

} // namespace nexus::utility::cloud
