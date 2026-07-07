#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::crossplatform {

/**
 * @brief Record system calls with their timings for cross-platform analysis.
 */
class SystemCallWrapper {
public:
    struct CallRecord {
        std::string name;
        std::int64_t returnValue = 0;
        double durationUs = 0.0;
        int errorCode = 0;
    };

    struct CallStats {
        std::string name;
        std::size_t count = 0;
        double totalUs = 0.0;
        double maxUs = 0.0;
        std::size_t errors = 0;
        double averageUs() const { return count ? totalUs / count : 0.0; }
    };

    static SystemCallWrapper& instance() {
        static SystemCallWrapper inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordCall(const std::string& name, double durationUs,
                    std::int64_t returnValue = 0, int errorCode = 0) {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.push_back({name, returnValue, durationUs, errorCode});
        auto& s = stats_[name];
        s.name = name;
        ++s.count;
        s.totalUs += durationUs;
        if (durationUs > s.maxUs) s.maxUs = durationUs;
        if (errorCode != 0) ++s.errors;
        ++total_;
    }

    CallStats stats(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = stats_.find(name);
        return it == stats_.end() ? CallStats{name, 0, 0, 0, 0} : it->second;
    }

    std::vector<CallStats> allStats() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<CallStats> out;
        for (const auto& [n, s] : stats_) out.push_back(s);
        return out;
    }

    std::size_t totalCalls() const { return total_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        history_.clear();
        stats_.clear();
        total_ = 0;
    }

private:
    SystemCallWrapper() = default;
    ~SystemCallWrapper() = default;
    SystemCallWrapper(const SystemCallWrapper&) = delete;
    SystemCallWrapper& operator=(const SystemCallWrapper&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<CallRecord> history_;
    std::unordered_map<std::string, CallStats> stats_;
    std::atomic<std::size_t> total_{0};
};

} // namespace nexus::utility::crossplatform
