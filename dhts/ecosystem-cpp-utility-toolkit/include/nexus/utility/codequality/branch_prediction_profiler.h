#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <cstddef>

namespace nexus::utility::codequality {

/**
 * @brief Profile branch prediction outcomes per source location.
 */
class BranchPredictionProfiler {
public:
    struct BranchStats {
        std::size_t total = 0;
        std::size_t taken = 0;
        std::size_t not_taken = 0;
        double taken_rate = 0.0;
    };

    static BranchPredictionProfiler& instance() {
        static BranchPredictionProfiler inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Record a branch outcome at @p location.
    void recordBranch(const std::string& location, bool taken) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& e = entries_[location];
        ++e.total;
        if (taken) ++e.taken; else ++e.not_taken;
        ++globalTotal_;
        if (taken) ++globalTaken_;
    }

    /// Aggregate statistics for @p location.
    BranchStats getBranchStats(const std::string& location) const {
        std::lock_guard<std::mutex> lk(mutex_);
        BranchStats s;
        auto it = entries_.find(location);
        if (it != entries_.end()) {
            s.total = it->second.total;
            s.taken = it->second.taken;
            s.not_taken = it->second.not_taken;
            s.taken_rate = s.total ? static_cast<double>(s.taken) / s.total : 0.0;
        }
        return s;
    }

    /// Taken rate across all recorded branches (0.0 - 1.0).
    double getGlobalTakenRate() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return globalTotal_ ? static_cast<double>(globalTaken_) / globalTotal_ : 0.0;
    }

    /// Number of distinct branch locations tracked.
    std::size_t getBranchCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return entries_.size();
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return "BranchPredictionProfiler{locations=" + std::to_string(entries_.size()) +
               ", records=" + std::to_string(globalTotal_) + "}";
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        entries_.clear();
        globalTotal_ = 0;
        globalTaken_ = 0;
    }

private:
    BranchPredictionProfiler() = default;
    ~BranchPredictionProfiler() = default;
    BranchPredictionProfiler(const BranchPredictionProfiler&) = delete;
    BranchPredictionProfiler& operator=(const BranchPredictionProfiler&) = delete;

    struct Entry {
        std::size_t total = 0;
        std::size_t taken = 0;
        std::size_t not_taken = 0;
    };

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Entry> entries_;
    std::size_t globalTotal_ = 0;
    std::size_t globalTaken_ = 0;
};

} // namespace nexus::utility::codequality
